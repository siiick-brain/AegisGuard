#include "aegisguard/session_store.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <stdexcept>

namespace aegisguard {
namespace {

constexpr SessionStore::Duration kMinimumLifetime{1000};
constexpr SessionStore::Duration kMaximumLifetime{3600000};

}  // namespace

SessionStore::SessionStore(std::size_t maximum_sessions)
    : maximum_sessions_(maximum_sessions) {
    if (maximum_sessions_ == 0) {
        throw std::invalid_argument("session capacity must be positive");
    }
}

SessionIssueResult SessionStore::issue(
    std::uint64_t player_id, Duration lifetime, Clock::time_point now) {
    const std::lock_guard lock(mutex_);
    if (player_id == 0) {
        return {SessionError::invalid_player_id, {}};
    }
    if (lifetime < kMinimumLifetime || lifetime > kMaximumLifetime ||
        now > Clock::time_point::max() - lifetime) {
        return {SessionError::invalid_lifetime, {}};
    }

    remove_expired(now);
    if (sessions_.size() >= maximum_sessions_) {
        return {SessionError::session_limit, {}};
    }

    SessionCredential credential{};
    if (RAND_bytes(credential.data(), static_cast<int>(credential.size())) != 1) {
        return {SessionError::crypto_failure, {}};
    }

    Digest credential_digest{};
    if (!digest(credential, credential_digest)) {
        return {SessionError::crypto_failure, {}};
    }

    const auto [position, inserted] = sessions_.emplace(
        credential_digest, Record{player_id, now + lifetime});
    static_cast<void>(position);
    if (!inserted) {
        return {SessionError::crypto_failure, {}};
    }
    return {SessionError::none, credential};
}

SessionValidationResult SessionStore::validate(
    const SessionCredential& credential, Clock::time_point now) {
    const std::lock_guard lock(mutex_);
    Digest credential_digest{};
    if (!digest(credential, credential_digest)) {
        return {SessionError::crypto_failure, 0};
    }

    const auto found = sessions_.find(credential_digest);
    if (found == sessions_.end()) {
        return {SessionError::invalid_credential, 0};
    }
    if (now >= found->second.expires_at) {
        sessions_.erase(found);
        return {SessionError::expired_credential, 0};
    }
    return {SessionError::none, found->second.player_id};
}

SessionError SessionStore::revoke(const SessionCredential& credential) {
    const std::lock_guard lock(mutex_);
    Digest credential_digest{};
    if (!digest(credential, credential_digest)) {
        return SessionError::crypto_failure;
    }

    const auto found = sessions_.find(credential_digest);
    if (found == sessions_.end()) {
        return SessionError::invalid_credential;
    }
    sessions_.erase(found);
    return SessionError::none;
}

std::size_t SessionStore::size() const {
    const std::lock_guard lock(mutex_);
    return sessions_.size();
}

bool SessionStore::digest(
    const SessionCredential& credential, Digest& output) noexcept {
    unsigned int digest_size = 0;
    return EVP_Digest(credential.data(), credential.size(), output.data(),
                      &digest_size, EVP_sha256(), nullptr) == 1 &&
           digest_size == output.size();
}

void SessionStore::remove_expired(Clock::time_point now) {
    std::erase_if(sessions_, [now](const auto& entry) {
        return now >= entry.second.expires_at;
    });
}

}  // namespace aegisguard
