#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>

namespace aegisguard {

using SessionCredential = std::array<std::uint8_t, 32>;

enum class SessionError {
    none,
    invalid_player_id,
    invalid_lifetime,
    session_limit,
    invalid_credential,
    expired_credential,
    crypto_failure,
};

struct SessionIssueResult {
    SessionError error{SessionError::none};
    SessionCredential credential{};
};

struct SessionValidationResult {
    SessionError error{SessionError::none};
    std::uint64_t player_id{};
};

class SessionStore {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::milliseconds;

    explicit SessionStore(std::size_t maximum_sessions = 4096);

    [[nodiscard]] SessionIssueResult issue(
        std::uint64_t player_id, Duration lifetime, Clock::time_point now);
    [[nodiscard]] SessionValidationResult validate(
        const SessionCredential& credential, Clock::time_point now);
    [[nodiscard]] SessionError revoke(const SessionCredential& credential);
    [[nodiscard]] std::size_t size() const;

private:
    using Digest = std::array<std::uint8_t, 32>;

    struct Record {
        std::uint64_t player_id;
        Clock::time_point expires_at;
    };

    std::size_t maximum_sessions_;
    std::map<Digest, Record> sessions_;
    mutable std::mutex mutex_;

    [[nodiscard]] static bool digest(
        const SessionCredential& credential, Digest& output) noexcept;
    void remove_expired(Clock::time_point now);
};

}  // namespace aegisguard
