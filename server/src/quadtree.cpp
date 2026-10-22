#include "aegisguard/quadtree.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace aegisguard {
namespace {

constexpr std::size_t kMaximumDepth = 64;

bool contains(Bounds bounds, Point point) {
    return point.x >= bounds.min_x && point.x <= bounds.max_x &&
           point.y >= bounds.min_y && point.y <= bounds.max_y;
}

bool intersects(Bounds first, Bounds second) {
    return first.min_x <= second.max_x && first.max_x >= second.min_x &&
           first.min_y <= second.max_y && first.max_y >= second.min_y;
}

bool finite_bounds(Bounds bounds) {
    return std::isfinite(bounds.min_x) && std::isfinite(bounds.min_y) &&
           std::isfinite(bounds.max_x) && std::isfinite(bounds.max_y) &&
           bounds.min_x <= bounds.max_x && bounds.min_y <= bounds.max_y;
}

}  // namespace

Quadtree::Quadtree(Bounds bounds, std::size_t capacity, std::size_t max_depth)
    : bounds_(bounds),
      capacity_(capacity),
      max_depth_(max_depth),
      root_(std::make_unique<Node>(Node{bounds, 0, 0, {}, {}})) {
    if (!valid_bounds(bounds_) || capacity_ == 0 || max_depth_ > kMaximumDepth) {
        throw std::invalid_argument(
            "quadtree bounds, capacity, or maximum depth are outside supported limits");
    }
}

Quadtree::~Quadtree() = default;
Quadtree::Quadtree(Quadtree&&) noexcept = default;
Quadtree& Quadtree::operator=(Quadtree&&) noexcept = default;

SpatialError Quadtree::insert(EntityId id, Point position) {
    if (id == 0) {
        return SpatialError::invalid_entity_id;
    }
    if (locations_.contains(id)) {
        return SpatialError::duplicate_entity;
    }
    if (!valid_position(position)) {
        return SpatialError::invalid_position;
    }

    const Entry entry{id, position};
    insert_into(*root_, entry);
    locations_.emplace(id, position);
    ++size_;
    return SpatialError::none;
}

SpatialError Quadtree::remove(EntityId id) {
    if (id == 0) {
        return SpatialError::invalid_entity_id;
    }
    const auto found = locations_.find(id);
    if (found == locations_.end()) {
        return SpatialError::unknown_entity;
    }

    const Point position = found->second;
    if (!erase_from(*root_, id, position)) {
        return SpatialError::unknown_entity;
    }
    locations_.erase(found);
    --size_;
    return SpatialError::none;
}

SpatialError Quadtree::update(EntityId id, Point position) {
    if (id == 0) {
        return SpatialError::invalid_entity_id;
    }
    if (!valid_position(position)) {
        return SpatialError::invalid_position;
    }

    const auto found = locations_.find(id);
    if (found == locations_.end()) {
        return SpatialError::unknown_entity;
    }
    if (found->second.x == position.x && found->second.y == position.y) {
        return SpatialError::none;
    }

    const Point previous_position = found->second;
    if (!erase_from(*root_, id, previous_position)) {
        return SpatialError::unknown_entity;
    }
    const Entry updated{id, position};
    insert_into(*root_, updated);
    found->second = position;
    return SpatialError::none;
}

RangeQueryResult Quadtree::query(Bounds area) const {
    if (!valid_bounds(area)) {
        return {SpatialError::invalid_bounds, {}};
    }

    RangeQueryResult result;
    if (!intersects(bounds_, area)) {
        return result;
    }
    query_node(*root_, area, result.entity_ids);
    std::sort(result.entity_ids.begin(), result.entity_ids.end());
    return result;
}

std::size_t Quadtree::size() const noexcept {
    return size_;
}

bool Quadtree::valid_position(Point position) const noexcept {
    return std::isfinite(position.x) && std::isfinite(position.y) &&
           contains(bounds_, position);
}

bool Quadtree::valid_bounds(Bounds bounds) noexcept {
    return finite_bounds(bounds);
}

std::size_t Quadtree::child_index(const Node& node, Point position) noexcept {
    const double middle_x = std::midpoint(node.bounds.min_x, node.bounds.max_x);
    const double middle_y = std::midpoint(node.bounds.min_y, node.bounds.max_y);
    const bool east = position.x >= middle_x;
    const bool north = position.y >= middle_y;
    return static_cast<std::size_t>(north) * 2 + static_cast<std::size_t>(east);
}

void Quadtree::insert_into(Node& node, Entry entry) {
    if (node.children[0]) {
        insert_into(*node.children[child_index(node, entry.position)], entry);
        ++node.subtree_count;
        return;
    }

    node.entries.push_back(entry);
    ++node.subtree_count;
    const double middle_x = std::midpoint(node.bounds.min_x, node.bounds.max_x);
    const double middle_y = std::midpoint(node.bounds.min_y, node.bounds.max_y);
    if (node.entries.size() <= capacity_ || node.depth >= max_depth_ ||
        (middle_x == node.bounds.min_x && middle_x == node.bounds.max_x &&
         middle_y == node.bounds.min_y && middle_y == node.bounds.max_y)) {
        return;
    }

    const Bounds quadrants[4]{
        {node.bounds.min_x, node.bounds.min_y, middle_x, middle_y},
        {middle_x, node.bounds.min_y, node.bounds.max_x, middle_y},
        {node.bounds.min_x, middle_y, middle_x, node.bounds.max_y},
        {middle_x, middle_y, node.bounds.max_x, node.bounds.max_y},
    };
    for (std::size_t index = 0; index < 4; ++index) {
        node.children[index] = std::make_unique<Node>(
            Node{quadrants[index], node.depth + 1, 0, {}, {}});
    }

    std::vector<Entry> existing;
    existing.swap(node.entries);
    for (const Entry& existing_entry : existing) {
        insert_into(*node.children[child_index(node, existing_entry.position)], existing_entry);
    }
}

bool Quadtree::erase_from(Node& node, EntityId id, Point position) {
    if (node.children[0]) {
        Node& child = *node.children[child_index(node, position)];
        if (!erase_from(child, id, position)) {
            return false;
        }
        --node.subtree_count;
        merge_if_small(node);
        return true;
    }

    const auto found = std::find_if(node.entries.begin(), node.entries.end(),
                                    [id](const Entry& entry) { return entry.id == id; });
    if (found == node.entries.end()) {
        return false;
    }
    node.entries.erase(found);
    --node.subtree_count;
    return true;
}

void Quadtree::merge_if_small(Node& node) {
    if (node.subtree_count > capacity_) {
        return;
    }

    std::vector<Entry> entries;
    collect_entries(node, entries);
    for (auto& child : node.children) {
        child.reset();
    }
    node.entries = std::move(entries);
}

void Quadtree::collect_entries(const Node& node, std::vector<Entry>& entries) {
    if (!node.children[0]) {
        entries.insert(entries.end(), node.entries.begin(), node.entries.end());
        return;
    }
    for (const auto& child : node.children) {
        collect_entries(*child, entries);
    }
}

void Quadtree::query_node(const Node& node, Bounds area, std::vector<EntityId>& matches) {
    if (!intersects(node.bounds, area)) {
        return;
    }
    if (!node.children[0]) {
        for (const Entry& entry : node.entries) {
            if (contains(area, entry.position)) {
                matches.push_back(entry.id);
            }
        }
        return;
    }
    for (const auto& child : node.children) {
        query_node(*child, area, matches);
    }
}

}  // namespace aegisguard
