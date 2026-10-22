#include "aegisguard/quadtree.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <vector>

namespace {

using aegisguard::Bounds;
using aegisguard::EntityId;
using aegisguard::Point;
using aegisguard::Quadtree;

constexpr Bounds kWorld{0.0, 0.0, 10000.0, 10000.0};

std::vector<Point> make_points(std::size_t count, bool clustered) {
    std::vector<Point> points;
    points.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (clustered) {
            points.push_back({
                4950.0 + static_cast<double>(index % 101),
                4950.0 + static_cast<double>((index / 101) % 101),
            });
        } else {
            points.push_back({
                static_cast<double>((index * 7919) % 10000),
                static_cast<double>((index * 104729) % 10000),
            });
        }
    }
    return points;
}

bool clustered_workload(const benchmark::State& state) {
    return state.range(1) == 1;
}

bool populate_tree(Quadtree& tree, const std::vector<Point>& points) {
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (tree.insert(static_cast<EntityId>(index + 1), points[index]) !=
            aegisguard::SpatialError::none) {
            return false;
        }
    }
    return true;
}

void BM_QuadtreeInsert(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto points = make_points(count, clustered_workload(state));
    for (auto _ : state) {
        Quadtree tree(kWorld);
        for (std::size_t index = 0; index < count; ++index) {
            const auto result = tree.insert(static_cast<EntityId>(index + 1), points[index]);
            if (result != aegisguard::SpatialError::none) {
                state.SkipWithError("Quadtree rejected a valid insertion fixture");
                break;
            }
        }
        benchmark::DoNotOptimize(tree.size());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

void BM_QuadtreeRemove(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto points = make_points(count, clustered_workload(state));
    for (auto _ : state) {
        state.PauseTiming();
        Quadtree tree(kWorld);
        if (!populate_tree(tree, points)) {
            state.ResumeTiming();
            state.SkipWithError("Quadtree rejected a valid removal fixture");
            return;
        }
        state.ResumeTiming();

        for (std::size_t index = 0; index < count; ++index) {
            const auto result = tree.remove(static_cast<EntityId>(index + 1));
            if (result != aegisguard::SpatialError::none) {
                state.SkipWithError("Quadtree rejected a valid removal request");
                break;
            }
        }
        benchmark::DoNotOptimize(tree.size());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

void BM_QuadtreeUpdate(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto points = make_points(count, clustered_workload(state));
    for (auto _ : state) {
        state.PauseTiming();
        Quadtree tree(kWorld);
        if (!populate_tree(tree, points)) {
            state.ResumeTiming();
            state.SkipWithError("Quadtree rejected a valid update fixture");
            return;
        }
        state.ResumeTiming();

        for (std::size_t index = 0; index < count; ++index) {
            const Point updated{
                static_cast<double>((index * 2377 + 37) % 10000),
                static_cast<double>((index * 8191 + 53) % 10000),
            };
            const auto result = tree.update(static_cast<EntityId>(index + 1), updated);
            if (result != aegisguard::SpatialError::none) {
                state.SkipWithError("Quadtree rejected a valid update request");
                break;
            }
        }
        benchmark::DoNotOptimize(tree.size());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

void BM_QuadtreeRangeQuery(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto points = make_points(count, clustered_workload(state));
    Quadtree tree(kWorld);
    if (!populate_tree(tree, points)) {
        state.SkipWithError("Quadtree rejected a valid query fixture");
        return;
    }
    const double half_width = static_cast<double>(state.range(2)) / 2.0;
    const Bounds area{
        5000.0 - half_width,
        5000.0 - half_width,
        5000.0 + half_width,
        5000.0 + half_width,
    };

    for (auto _ : state) {
        const auto result = tree.query(area);
        if (result.error != aegisguard::SpatialError::none) {
            state.SkipWithError("Quadtree rejected a valid query area");
            break;
        }
        benchmark::DoNotOptimize(result.entity_ids.data());
        benchmark::DoNotOptimize(result.entity_ids.size());
    }
}

void BM_BruteForceRangeQuery(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto points = make_points(count, clustered_workload(state));
    const double half_width = static_cast<double>(state.range(2)) / 2.0;
    const Bounds area{
        5000.0 - half_width,
        5000.0 - half_width,
        5000.0 + half_width,
        5000.0 + half_width,
    };

    for (auto _ : state) {
        std::vector<EntityId> matches;
        for (std::size_t index = 0; index < points.size(); ++index) {
            const Point point = points[index];
            if (point.x >= area.min_x && point.x <= area.max_x &&
                point.y >= area.min_y && point.y <= area.max_y) {
                matches.push_back(static_cast<EntityId>(index + 1));
            }
        }
        benchmark::DoNotOptimize(matches.data());
        benchmark::DoNotOptimize(matches.size());
    }
}

BENCHMARK(BM_QuadtreeInsert)
    ->ArgNames({"entities", "distribution"})
    ->Args({100, 0})->Args({1000, 0})->Args({10000, 0})
    ->Args({100, 1})->Args({1000, 1})->Args({10000, 1});
BENCHMARK(BM_QuadtreeRemove)
    ->ArgNames({"entities", "distribution"})
    ->Args({100, 0})->Args({1000, 0})->Args({10000, 0})
    ->Args({100, 1})->Args({1000, 1})->Args({10000, 1});
BENCHMARK(BM_QuadtreeUpdate)
    ->ArgNames({"entities", "distribution"})
    ->Args({100, 0})->Args({1000, 0})->Args({10000, 0})
    ->Args({100, 1})->Args({1000, 1})->Args({10000, 1});
BENCHMARK(BM_QuadtreeRangeQuery)
    ->ArgNames({"entities", "distribution", "query_width"})
    ->Args({100, 0, 11})->Args({1000, 0, 11})->Args({10000, 0, 11})
    ->Args({100, 1, 11})->Args({1000, 1, 11})->Args({10000, 1, 11})
    ->Args({100, 0, 101})->Args({1000, 0, 101})->Args({10000, 0, 101})
    ->Args({100, 1, 101})->Args({1000, 1, 101})->Args({10000, 1, 101});
BENCHMARK(BM_BruteForceRangeQuery)
    ->ArgNames({"entities", "distribution", "query_width"})
    ->Args({100, 0, 11})->Args({1000, 0, 11})->Args({10000, 0, 11})
    ->Args({100, 1, 11})->Args({1000, 1, 11})->Args({10000, 1, 11})
    ->Args({100, 0, 101})->Args({1000, 0, 101})->Args({10000, 0, 101})
    ->Args({100, 1, 101})->Args({1000, 1, 101})->Args({10000, 1, 101});

}  // namespace

BENCHMARK_MAIN();
