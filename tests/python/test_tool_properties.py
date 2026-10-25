from __future__ import annotations

from typing import Any

from hypothesis import given, settings
from hypothesis import strategies as st
import pytest

from tools.evaluation.render_benchmark_graph import (
    _parse_case,
    collect_series,
    percentile,
)
from tools.evaluation.sanitize_benchmark_json import sanitize_report


@settings(max_examples=200, derandomize=True)
@given(
    st.lists(
        st.floats(
            min_value=0.0001,
            max_value=1_000_000,
            allow_nan=False,
            allow_infinity=False,
        ),
        min_size=1,
        max_size=100,
    )
)
def test_percentiles_are_monotonic_and_within_sample_bounds(
    values: list[float],
) -> None:
    percentiles = [percentile(values, rank) for rank in (0, 50, 95, 99, 100)]
    assert percentiles == sorted(percentiles)
    assert min(values) <= percentiles[0]
    assert percentiles[-1] <= max(values)


@settings(max_examples=100, derandomize=True)
@given(
    st.text(max_size=80),
    st.text(max_size=80),
    st.lists(st.integers(min_value=0, max_value=1_000_000), max_size=10),
)
def test_sanitizer_removes_host_name_and_preserves_measurements(
    host_name: str,
    executable: str,
    measurements: list[int],
) -> None:
    report: dict[str, Any] = {
        "context": {
            "host_name": host_name,
            "executable": executable,
            "num_cpus": len(measurements),
        },
        "benchmarks": [{"real_time": value} for value in measurements],
    }

    sanitized = sanitize_report(report)

    assert "host_name" not in sanitized["context"]
    assert sanitized["context"]["executable"] == "aegisguard_spatial_benchmarks"
    assert sanitized["context"]["num_cpus"] == len(measurements)
    assert sanitized["benchmarks"] == report["benchmarks"]
    assert report["context"]["host_name"] == host_name


@settings(max_examples=100, derandomize=True)
@given(
    st.sampled_from(
        ["BM_QuadtreeRangeQuery", "BM_BruteForceRangeQuery"]
    ),
    st.integers(min_value=0, max_value=1),
    st.sampled_from([11, 101]),
)
def test_supported_benchmark_case_names_parse_consistently(
    algorithm: str,
    distribution: int,
    width: int,
) -> None:
    parsed = _parse_case(
        f"{algorithm}/entities:10000/distribution:{distribution}/query_width:{width}"
    )

    assert parsed is not None
    assert parsed[1] == distribution
    assert parsed[2] == width


def test_collect_series_rejects_duplicate_repetition_numbers() -> None:
    rows: list[dict[str, object]] = []
    for algorithm in ("BM_QuadtreeRangeQuery", "BM_BruteForceRangeQuery"):
        for distribution in (0, 1):
            for width in (11, 101):
                for repetition in range(20):
                    rows.append({
                        "name": (
                            f"{algorithm}/entities:10000/distribution:{distribution}"
                            f"/query_width:{width}"
                        ),
                        "run_type": "iteration",
                        "repetition_index": repetition,
                        "real_time": repetition + 1,
                        "time_unit": "us",
                    })
                rows.append({
                    "name": (
                        f"{algorithm}/entities:10000/distribution:{distribution}"
                        f"/query_width:{width}"
                    ),
                    "run_type": "iteration",
                    "repetition_index": 0,
                    "real_time": 99,
                    "time_unit": "us",
                })

    with pytest.raises(ValueError, match="duplicate repetition index"):
        collect_series({"benchmarks": rows})
