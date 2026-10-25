#include "aegisguard/quadtree.hpp"

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <vector>

namespace aegisguard {
namespace {

RC_GTEST_PROP(QuadtreeProperty, MatchesReferenceModelAfterEveryOperation, ()) {
    const Bounds world{0.0, 0.0, 100.0, 100.0};
    Quadtree tree(world, 4, 10);
    std::map<EntityId, Point> model;
    const auto operations = *rc::gen::container<std::vector<int>>(
        rc::gen::inRange(-100, 101));

    for (std::size_t index = 0; index + 4 < operations.size(); index += 5) {
        const auto id = static_cast<EntityId>(
            std::abs(operations[index]) % 24 + 1);
        const int action = std::abs(operations[index + 1]) % 3;
        const Point point{
            static_cast<double>(std::abs(operations[index + 2]) % 101),
            static_cast<double>(std::abs(operations[index + 3]) % 101)};
        if (action == 0) {
            const auto result = tree.insert(id, point);
            const auto existing = model.find(id);
            if (existing == model.end()) {
                RC_ASSERT(result == SpatialError::none);
                model.emplace(id, point);
            } else {
                RC_ASSERT(result == SpatialError::duplicate_entity);
            }
        } else if (action == 1) {
            const auto result = tree.update(id, point);
            const auto existing = model.find(id);
            if (existing == model.end()) {
                RC_ASSERT(result == SpatialError::unknown_entity);
            } else {
                RC_ASSERT(result == SpatialError::none);
                existing->second = point;
            }
        } else {
            const auto result = tree.remove(id);
            const auto existing = model.find(id);
            if (existing == model.end()) {
                RC_ASSERT(result == SpatialError::unknown_entity);
            } else {
                RC_ASSERT(result == SpatialError::none);
                model.erase(existing);
            }
        }

        const double x = static_cast<double>(
            std::abs(operations[index + 4]) % 101);
        const Bounds area{x, 0.0, 100.0, 100.0};
        auto expected = std::vector<EntityId>{};
        for (const auto& [entity_id, position] : model) {
            if (position.x >= area.min_x && position.x <= area.max_x) {
                expected.push_back(entity_id);
            }
        }
        auto actual = tree.query(area);
        RC_ASSERT(actual.error == SpatialError::none);
        std::sort(actual.entity_ids.begin(), actual.entity_ids.end());
        RC_ASSERT(actual.entity_ids == expected);
        RC_ASSERT(tree.size() == model.size());
    }
}

}  // namespace
}  // namespace aegisguard
