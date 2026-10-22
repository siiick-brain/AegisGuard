#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace aegisguard {

using EntityId = std::uint64_t;

struct Point {
    double x{};
    double y{};
};

struct Bounds {
    double min_x{};
    double min_y{};
    double max_x{};
    double max_y{};
};

enum class SpatialError {
    none,
    invalid_entity_id,
    duplicate_entity,
    invalid_position,
    invalid_bounds,
    unknown_entity,
};

struct RangeQueryResult {
    SpatialError error{SpatialError::none};
    std::vector<EntityId> entity_ids;
};

class Quadtree {
public:
    explicit Quadtree(Bounds bounds, std::size_t capacity = 8, std::size_t max_depth = 12);
    ~Quadtree();

    Quadtree(Quadtree&&) noexcept;
    Quadtree& operator=(Quadtree&&) noexcept;
    Quadtree(const Quadtree&) = delete;
    Quadtree& operator=(const Quadtree&) = delete;

    [[nodiscard]] SpatialError insert(EntityId id, Point position);
    [[nodiscard]] SpatialError remove(EntityId id);
    [[nodiscard]] SpatialError update(EntityId id, Point position);
    [[nodiscard]] RangeQueryResult query(Bounds area) const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    struct Entry {
        EntityId id;
        Point position;
    };

    struct Node {
        Bounds bounds;
        std::size_t depth;
        std::size_t subtree_count{};
        std::vector<Entry> entries;
        std::unique_ptr<Node> children[4];
    };

    Bounds bounds_;
    std::size_t capacity_;
    std::size_t max_depth_;
    std::size_t size_{};
    std::unordered_map<EntityId, Point> locations_;
    std::unique_ptr<Node> root_;

    [[nodiscard]] bool valid_position(Point position) const noexcept;
    [[nodiscard]] static bool valid_bounds(Bounds bounds) noexcept;
    [[nodiscard]] static std::size_t child_index(const Node& node, Point position) noexcept;
    void insert_into(Node& node, Entry entry);
    [[nodiscard]] bool erase_from(Node& node, EntityId id, Point position);
    void merge_if_small(Node& node);
    static void collect_entries(const Node& node, std::vector<Entry>& entries);
    static void query_node(const Node& node, Bounds area, std::vector<EntityId>& matches);
};

}  // namespace aegisguard
