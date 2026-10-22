#include "aegisguard/session_store.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

namespace aegisguard {
namespace {

using Clock = SessionStore::Clock;
using Duration = SessionStore::Duration;

TEST(SessionStoreTest, IssuesRandomCredentialsBoundToOnePlayer) {
    SessionStore sessions;
    const auto now = Clock::time_point{};

    const auto first = sessions.issue(17, Duration{60000}, now);
    const auto second = sessions.issue(17, Duration{60000}, now);

    ASSERT_EQ(first.error, SessionError::none);
    ASSERT_EQ(second.error, SessionError::none);
    EXPECT_NE(first.credential, second.credential);
    EXPECT_FALSE(std::all_of(first.credential.begin(), first.credential.end(),
                             [](std::uint8_t byte) { return byte == 0; }));
    const auto result = sessions.validate(first.credential, now);
    EXPECT_EQ(result.error, SessionError::none);
    EXPECT_EQ(result.player_id, 17U);
}

TEST(SessionStoreTest, RejectsInvalidIdentityAndCredentialLifetime) {
    SessionStore sessions;
    const auto now = Clock::time_point{};

    EXPECT_EQ(sessions.issue(0, Duration{60000}, now).error,
              SessionError::invalid_player_id);
    EXPECT_EQ(sessions.issue(1, Duration{999}, now).error,
              SessionError::invalid_lifetime);
    EXPECT_EQ(sessions.issue(1, Duration{3600001}, now).error,
              SessionError::invalid_lifetime);

    const SessionCredential unknown{};
    EXPECT_EQ(sessions.validate(unknown, now).error, SessionError::invalid_credential);
    EXPECT_EQ(sessions.revoke(unknown), SessionError::invalid_credential);
}

TEST(SessionStoreTest, ExpiresAndRevokesCredentials) {
    SessionStore sessions;
    const auto now = Clock::time_point{};
    const auto issued = sessions.issue(9, Duration{1000}, now);
    ASSERT_EQ(issued.error, SessionError::none);

    EXPECT_EQ(sessions.validate(issued.credential, now + Duration{999}).error,
              SessionError::none);
    EXPECT_EQ(sessions.validate(issued.credential, now + Duration{1000}).error,
              SessionError::expired_credential);
    EXPECT_EQ(sessions.size(), 0U);

    const auto second = sessions.issue(9, Duration{1000}, now);
    ASSERT_EQ(second.error, SessionError::none);
    EXPECT_EQ(sessions.revoke(second.credential), SessionError::none);
    EXPECT_EQ(sessions.validate(second.credential, now).error,
              SessionError::invalid_credential);
}

TEST(SessionStoreTest, BoundsActiveSessionCountAndRemovesExpiredEntries) {
    SessionStore sessions(1);
    const auto now = Clock::time_point{};
    const auto first = sessions.issue(1, Duration{1000}, now);
    ASSERT_EQ(first.error, SessionError::none);
    EXPECT_EQ(sessions.issue(2, Duration{1000}, now).error,
              SessionError::session_limit);

    const auto replacement = sessions.issue(2, Duration{1000}, now + Duration{1000});
    EXPECT_EQ(replacement.error, SessionError::none);
    EXPECT_EQ(sessions.size(), 1U);
}

TEST(SessionStoreTest, RejectsZeroCapacityAndExpiryOverflow) {
    EXPECT_THROW(SessionStore sessions(0), std::invalid_argument);

    SessionStore sessions;
    const auto near_max = Clock::time_point::max() - Duration{500};
    EXPECT_EQ(sessions.issue(1, Duration{1000}, near_max).error,
              SessionError::invalid_lifetime);
}

TEST(SessionStoreTest, SupportsConcurrentValidationAndRevocation) {
    SessionStore sessions(64);
    const auto now = Clock::now();
    std::vector<SessionCredential> credentials;
    credentials.reserve(64);
    for (std::uint64_t player_id = 1; player_id <= 64; ++player_id) {
        const auto issued = sessions.issue(player_id, Duration{60000}, now);
        ASSERT_EQ(issued.error, SessionError::none);
        credentials.push_back(issued.credential);
    }

    std::atomic_size_t unexpected_results{};
    std::vector<std::thread> readers;
    readers.reserve(4);
    for (std::size_t thread_index = 0; thread_index < 4; ++thread_index) {
        readers.emplace_back([&] {
            for (std::size_t attempt = 0; attempt < 1000; ++attempt) {
                for (const auto& credential : credentials) {
                    const auto result = sessions.validate(credential, now);
                    if (result.error != SessionError::none &&
                        result.error != SessionError::invalid_credential) {
                        unexpected_results.fetch_add(
                            1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    std::thread revoker([&] {
        for (const auto& credential : credentials) {
            const auto result = sessions.revoke(credential);
            if (result != SessionError::none) {
                unexpected_results.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    for (auto& reader : readers) {
        reader.join();
    }
    revoker.join();

    EXPECT_EQ(unexpected_results.load(std::memory_order_relaxed), 0U);
    EXPECT_EQ(sessions.size(), 0U);
}

}  // namespace
}  // namespace aegisguard
