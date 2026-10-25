import unittest

from tools.evaluation.render_benchmark_graph import collect_series, percentile, render_svg


def make_report():
    benchmarks = []
    for algorithm in ("BM_QuadtreeRangeQuery", "BM_BruteForceRangeQuery"):
        for distribution in (0, 1):
            for width in (11, 101):
                for repetition in range(20):
                    benchmarks.append({
                        "name": (
                            f"{algorithm}/entities:10000/distribution:{distribution}"
                            f"/query_width:{width}"
                        ),
                        "run_type": "iteration",
                        "repetition_index": repetition,
                        "real_time": (repetition + 1) * 1000,
                        "time_unit": "ns",
                    })
                benchmarks.append({
                    "name": (
                        f"{algorithm}/entities:10000/distribution:{distribution}"
                        f"/query_width:{width}_median"
                    ),
                    "run_type": "aggregate",
                    "real_time": 10500,
                    "time_unit": "ns",
                })
    return {"benchmarks": benchmarks}


class BenchmarkGraphTest(unittest.TestCase):
    def test_percentile_uses_linear_interpolation(self):
        self.assertEqual(percentile([1.0, 2.0, 3.0, 4.0], 50), 2.5)
        self.assertAlmostEqual(percentile([1.0, 2.0, 3.0, 4.0], 95), 3.85)

    def test_collects_raw_rows_and_ignores_aggregates(self):
        series = collect_series(make_report())
        self.assertEqual(len(series), 8)
        self.assertEqual(len(series[0]["values"]), 20)
        self.assertEqual(series[0]["percentiles"]["p50"], 10.5)
        self.assertEqual(series[-1]["distribution"], "Clustered")
        self.assertEqual(series[-1]["width"], 101)

    def test_svg_shows_repetitions_and_percentile_markers(self):
        svg = render_svg(collect_series(make_report()))
        self.assertIn("<svg", svg)
        self.assertIn("p50", svg)
        self.assertIn("p95", svg)
        self.assertIn("p99", svg)
        self.assertIn("Each gray dot is one measured repetition", svg)
        self.assertIn("Clustered · width 101", svg)
        self.assertEqual(svg.count('class="raw"/>'), 160)
        self.assertEqual(svg.count('class="p50"/>'), 8)
        self.assertEqual(svg.count('class="p95"/>'), 8)
        self.assertEqual(svg.count('class="p99"/>'), 8)

    def test_incomplete_raw_data_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "expected all eight"):
            collect_series({"benchmarks": []})


if __name__ == "__main__":
    unittest.main()
