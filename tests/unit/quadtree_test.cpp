#include "aegisguard/quadtree.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>

namespace aegisguard {
namespace {

const Bounds kWorld{0.0, 0.0, 100.0, 100.0};

std::vector<EntityId> brute_force_query(const std::map<EntityId, Point>& entities,
                                        Bounds area) {
    std::vector<EntityId> matches;
    for (const auto& [id, point] : entities) {
        if (point.x >= area.min_x && point.x <= area.max_x &&
            point.y >= area.min_y && point.y <= area.max_y) {
            matches.push_back(id);
        }
    }
    return matches;
}

TEST(QuadtreeTest, RejectsInvalidConfiguration) {
    EXPECT_THROW(Quadtree tree(kWorld, 0), std::invalid_argument);
    EXPECT_THROW(Quadtree tree(kWorld, 8, 65), std::invalid_argument);
    EXPECT_THROW(Quadtree tree({1.0, 0.0, 0.0, 1.0}), std::invalid_argument);
    EXPECT_THROW(Quadtree tree({0.0, 0.0, std::numeric_limits<double>::infinity(), 1.0}),
                 std::invalid_argument);
}

TEST(QuadtreeTest, HandlesEmptyAndSingleEntityQueries) {
    Quadtree tree(kWorld);
    EXPECT_TRUE(tree.query(kWorld).entity_ids.empty());
    ASSERT_EQ(tree.insert(1, {25.0, 75.0}), SpatialError::none);

    const auto result = tree.query({20.0, 70.0, 30.0, 80.0});
    ASSERT_EQ(result.error, SpatialError::none);
    EXPECT_EQ(result.entity_ids, std::vector<EntityId>{1});
    EXPECT_EQ(tree.size(), 1U);
}

TEST(QuadtreeTest, ValidatesIdsPositionsAndQueryBounds) {
    Quadtree tree(kWorld);
    EXPECT_EQ(tree.insert(0, {1.0, 1.0}), SpatialError::invalid_entity_id);
    EXPECT_EQ(tree.remove(0), SpatialError::invalid_entity_id);
    EXPECT_EQ(tree.update(0, {1.0, 1.0}), SpatialError::invalid_entity_id);
    EXPECT_EQ(tree.insert(1, {-1.0, 1.0}), SpatialError::invalid_position);
    EXPECT_EQ(tree.insert(1, {std::numeric_limits<double>::quiet_NaN(), 1.0}),
              SpatialError::invalid_position);
    ASSERT_EQ(tree.insert(1, {1.0, 1.0}), SpatialError::none);
    EXPECT_EQ(tree.insert(1, {2.0, 2.0}), SpatialError::duplicate_entity);
    EXPECT_EQ(tree.query({10.0, 10.0, 0.0, 20.0}).error, SpatialError::invalid_bounds);
    EXPECT_EQ(tree.remove(2), SpatialError::unknown_entity);
    EXPECT_EQ(tree.update(2, {2.0, 2.0}), SpatialError::unknown_entity);
    EXPECT_EQ(tree.update(1, {-1.0, 1.0}), SpatialError::invalid_position);
}

TEST(QuadtreeTest, IncludesWorldEdgesAndQuadrantBoundaries) {
    Quadtree tree(kWorld, 1, 8);
    ASSERT_EQ(tree.insert(1, {0.0, 0.0}), SpatialError::none);
    ASSERT_EQ(tree.insert(2, {50.0, 50.0}), SpatialError::none);
    ASSERT_EQ(tree.insert(3, {100.0, 100.0}), SpatialError::none);

    const auto result = tree.query({50.0, 50.0, 100.0, 100.0});
    EXPECT_EQ(result.entity_ids, (std::vector<EntityId>{2, 3}));
}

TEST(QuadtreeTest, RetainsEntitiesAtDuplicatePositionsAndDegenerateBounds) {
    Quadtree tree(kWorld, 1, 6);
    ASSERT_EQ(tree.insert(1, {50.0, 50.0}), SpatialError::none);
    ASSERT_EQ(tree.insert(2, {50.0, 50.0}), SpatialError::none);
    ASSERT_EQ(tree.insert(3, {50.0, 50.0}), SpatialError::none);
    EXPECT_EQ(tree.query({50.0, 50.0, 50.0, 50.0}).entity_ids,
              (std::vector<EntityId>{1, 2, 3}));

    Quadtree line({5.0, 0.0, 5.0, 10.0}, 1, 10);
    ASSERT_EQ(line.insert(4, {5.0, 2.0}), SpatialError::none);
    ASSERT_EQ(line.insert(5, {5.0, 8.0}), SpatialError::none);
    EXPECT_EQ(line.query({5.0, 0.0, 5.0, 10.0}).entity_ids,
              (std::vector<EntityId>{4, 5}));
}

TEST(QuadtreeTest, UpdatesAndRemovesEntitiesAfterSubdivision) {
    Quadtree tree(kWorld, 1, 8);
    ASSERT_EQ(tree.insert(1, {10.0, 10.0}), SpatialError::none);
    ASSERT_EQ(tree.insert(2, {90.0, 90.0}), SpatialError::none);

    ASSERT_EQ(tree.update(1, {80.0, 20.0}), SpatialError::none);
    EXPECT_TRUE(tree.query({0.0, 0.0, 20.0, 20.0}).entity_ids.empty());
    EXPECT_EQ(tree.query({75.0, 15.0, 85.0, 25.0}).entity_ids,
              std::vector<EntityId>{1});
    ASSERT_EQ(tree.remove(2), SpatialError::none);
    ASSERT_EQ(tree.remove(1), SpatialError::none);
    EXPECT_EQ(tree.size(), 0U);
    EXPECT_TRUE(tree.query(kWorld).entity_ids.empty());
}

TEST(QuadtreeTest, StopsSubdivisionAtConfiguredDepth) {
    Quadtree tree(kWorld, 1, 0);
    ASSERT_EQ(tree.insert(1, {50.0, 50.0}), SpatialError::none);
    ASSERT_EQ(tree.insert(2, {50.0, 50.0}), SpatialError::none);
    EXPECT_EQ(tree.query(kWorld).entity_ids, (std::vector<EntityId>{1, 2}));
}

TEST(QuadtreeTest, ReturnsEmptyForNonIntersectingAndFullWorldQueries) {
    Quadtree tree(kWorld);
    ASSERT_EQ(tree.insert(1, {50.0, 50.0}), SpatialError::none);
    EXPECT_TRUE(tree.query({200.0, 200.0, 300.0, 300.0}).entity_ids.empty());
    EXPECT_EQ(tree.query({0.0, 0.0, 100.0, 100.0}).entity_ids,
              std::vector<EntityId>{1});
}

TEST(QuadtreeTest, MatchesBruteForceAcrossSeededOperationSequence) {
    Quadtree tree(kWorld, 4, 10);
    std::map<EntityId, Point> oracle;
    std::mt19937 random(0xA3615U);
    std::uniform_int_distribution<int> operation(0, 3);
    std::uniform_int_distribution<int> id_distribution(1, 80);
    std::uniform_real_distribution<double> coordinate(0.0, 100.0);

    for (int iteration = 0; iteration < 600; ++iteration) {
        const EntityId id = static_cast<EntityId>(id_distribution(random));
        const int action = operation(random);
        if (action == 0 && !oracle.contains(id)) {
            const Point point{coordinate(random), coordinate(random)};
            ASSERT_EQ(tree.insert(id, point), SpatialError::none);
            oracle.emplace(id, point);
        } else if (action == 1 && oracle.contains(id)) {
            const Point point{coordinate(random), coordinate(random)};
            ASSERT_EQ(tree.update(id, point), SpatialError::none);
            oracle[id] = point;
        } else if (action == 2 && oracle.contains(id)) {
            ASSERT_EQ(tree.remove(id), SpatialError::none);
            oracle.erase(id);
        }

        const double x1 = coordinate(random);
        const double y1 = coordinate(random);
        const double x2 = coordinate(random);
        const double y2 = coordinate(random);
        const Bounds area{std::min(x1, x2), std::min(y1, y2),
                          std::max(x1, x2), std::max(y1, y2)};
        const auto result = tree.query(area);
        ASSERT_EQ(result.error, SpatialError::none);
        EXPECT_EQ(result.entity_ids, brute_force_query(oracle, area));
        EXPECT_EQ(tree.size(), oracle.size());
    }
}

}  // namespace
}  // namespace aegisguard
