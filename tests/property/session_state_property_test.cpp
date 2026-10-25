#include "aegisguard/session_store.hpp"

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <vector>

namespace aegisguard {
namespace {

struct ModelSession {
    SessionCredential credential;
    std::uint64_t player_id;
    SessionStore::Clock::time_point expires_at;
};

RC_GTEST_PROP(SessionStateProperty, IssueValidateRevokeMatchesReferenceModel, ()) {
    using Clock = SessionStore::Clock;
    using Duration = SessionStore::Duration;
    SessionStore store(32);
    std::vector<ModelSession> model;
    auto now = Clock::time_point{};
    const auto operations = *rc::gen::container<std::vector<int>>(
        rc::gen::inRange(0, 1000));

    for (std::size_t index = 0; index + 2 < operations.size(); index += 3) {
        now += std::chrono::milliseconds(operations[index] % 25);
        const int action = operations[index + 1] % 3;
        const std::uint64_t player_id =
            static_cast<std::uint64_t>(operations[index + 2] % 8 + 1);
        if (action == 0 && model.size() < 32) {
            model.erase(
                std::remove_if(model.begin(), model.end(),
                    [now](const ModelSession& record) {
                        return now >= record.expires_at;
                    }),
                model.end());
            const auto issued = store.issue(player_id, Duration{1000}, now);
            RC_ASSERT(issued.error == SessionError::none);
            model.push_back(ModelSession{
                issued.credential, player_id, now + Duration{1000}});
        } else if (action == 1) {
            const auto found = std::find_if(
                model.begin(), model.end(), [player_id](const ModelSession& entry) {
                    return entry.player_id == player_id;
                });
            if (found == model.end()) {
                const SessionCredential unknown{};
                RC_ASSERT(store.validate(unknown, now).error ==
                          SessionError::invalid_credential);
            } else if (now >= found->expires_at) {
                RC_ASSERT(store.validate(found->credential, now).error ==
                          SessionError::expired_credential);
                model.erase(found);
            } else {
                const auto validated = store.validate(found->credential, now);
                RC_ASSERT(validated.error == SessionError::none);
                RC_ASSERT(validated.player_id == player_id);
            }
        } else if (action == 2) {
            const auto found = std::find_if(
                model.begin(), model.end(), [player_id](const ModelSession& entry) {
                    return entry.player_id == player_id;
                });
            if (found == model.end()) {
                const SessionCredential unknown{};
                RC_ASSERT(store.revoke(unknown) == SessionError::invalid_credential);
            } else {
                RC_ASSERT(store.revoke(found->credential) == SessionError::none);
                model.erase(found);
            }
        }
        RC_ASSERT(store.size() == model.size());
    }
}

}  // namespace
}  // namespace aegisguard
