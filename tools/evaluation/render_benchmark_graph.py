import argparse
import html
import json
import math
from collections import defaultdict
from pathlib import Path
from typing import Any

_ALGORITHMS = {
    "BM_QuadtreeRangeQuery": "Quadtree",
    "BM_BruteForceRangeQuery": "Brute force",
}
_UNIT_TO_MICROSECONDS = {
    "ns": 0.001,
    "us": 1.0,
    "ms": 1000.0,
    "s": 1_000_000.0,
}
_PERCENTILES = (50, 95, 99)


def percentile(values: list[float], percentage: int) -> float:
    if not values or percentage < 0 or percentage > 100:
        raise ValueError("percentile requires values and a percentage from 0 to 100")
    ordered = sorted(values)
    rank = (len(ordered) - 1) * percentage / 100
    lower = math.floor(rank)
    upper = math.ceil(rank)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (rank - lower)


def _parse_case(name: str) -> tuple[str, int, int, str] | None:
    parts = name.split("/")
    algorithm = parts[0]
    if algorithm not in _ALGORITHMS:
        return None
    arguments: dict[str, int] = {}
    for part in parts[1:]:
        key, separator, value = part.partition(":")
        if not separator or key not in {"entities", "distribution", "query_width"}:
            continue
        try:
            arguments[key] = int(value)
        except ValueError:
            return None
    if arguments.get("entities") != 10000:
        return None
    if arguments.get("distribution") not in {0, 1}:
        return None
    if arguments.get("query_width") not in {11, 101}:
        return None
    return (
        _ALGORITHMS[algorithm],
        arguments["distribution"],
        arguments["query_width"],
        algorithm,
    )


def collect_series(report: Any) -> list[dict[str, Any]]:
    if not isinstance(report, dict) or not isinstance(report.get("benchmarks"), list):
        raise ValueError("input must be a Google Benchmark JSON report")

    samples: dict[tuple[str, int, int, str], dict[int, float]] = defaultdict(dict)
    units: dict[tuple[str, int, int, str], str] = {}
    for row in report["benchmarks"]:
        if not isinstance(row, dict) or row.get("run_type") != "iteration":
            continue
        case = _parse_case(row.get("name", ""))
        if case is None:
            continue
        repetition = row.get("repetition_index")
        value = row.get("real_time")
        unit = row.get("time_unit")
        if not isinstance(repetition, int) or repetition < 0:
            raise ValueError("raw benchmark rows must include a non-negative repetition_index")
        if not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
            raise ValueError("raw benchmark rows must include a positive finite real_time")
        if unit not in _UNIT_TO_MICROSECONDS:
            raise ValueError(f"unsupported benchmark time unit: {unit}")
        if repetition in samples[case]:
            raise ValueError(f"duplicate repetition index {repetition} for {case}")
        samples[case][repetition] = value * _UNIT_TO_MICROSECONDS[unit]
        prior_unit = units.setdefault(case, unit)
        if prior_unit != unit:
            raise ValueError(f"inconsistent time units for {case}")

    expected = {
        (algorithm, distribution, width)
        for algorithm in _ALGORITHMS.values()
        for distribution in (0, 1)
        for width in (11, 101)
    }
    found = {(case[0], case[1], case[2]) for case in samples}
    if found != expected:
        missing = sorted(expected - found)
        raise ValueError(f"expected all eight 10,000-entity query cases; missing: {missing}")

    result = []
    for case, indexed_values in samples.items():
        indices = sorted(indexed_values)
        if indices != list(range(len(indices))):
            raise ValueError(f"repetition indices must be contiguous from zero for {case}")
        if len(indices) < 20:
            raise ValueError(f"expected at least 20 raw repetitions for {case}")
        values = [indexed_values[index] for index in indices]
        result.append({
            "algorithm": case[0],
            "distribution": "Clustered" if case[1] else "Uniform",
            "width": case[2],
            "values": values,
            "percentiles": {
                f"p{percentage}": percentile(values, percentage)
                for percentage in _PERCENTILES
            },
        })
    return sorted(
        result,
        key=lambda row: (
            row["width"],
            row["distribution"] != "Uniform",
            row["algorithm"] != "Quadtree",
        ),
    )


def _x_position(value: float, left: float, right: float) -> float:
    minimum = 0.5
    maximum = 2000.0
    bounded = min(max(value, minimum), maximum)
    return left + (math.log10(bounded) - math.log10(minimum)) / (
        math.log10(maximum) - math.log10(minimum)
    ) * (right - left)


def render_svg(series: list[dict[str, Any]]) -> str:
    width, height = 1600, 680
    left, right = 390.0, 1150.0
    top, row_step = 125.0, 52.0
    bottom = top + row_step * (len(series) - 1)
    ticks = (0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000)
    colors = {"p50": "#007f86", "p95": "#d97706", "p99": "#c62828"}
    shapes = {"p50": "circle", "p95": "triangle", "p99": "square"}
    output = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" role="img" aria-labelledby="title description">',
        "<title id=\"title\">Raw spatial query repetitions and latency percentiles</title>",
        "<desc id=\"description\">Twenty raw repetitions for each 10,000-entity query case, "
        "with p50, p95, and p99 markers. The clustered Quadtree query at width 101 is the "
        "slowest case by a wide margin.</desc>",
        "<style>text{font-family:Arial,sans-serif;fill:#202124}.muted{fill:#5f6368}"
        ".grid{stroke:#dadce0;stroke-width:1}.raw{fill:#9aa0a6;opacity:.68}"
        ".p50{fill:#007f86}.p95{fill:#d97706}.p99{fill:#c62828}"
        ".legend-p50{fill:#007f86}.legend-p95{fill:#d97706}.legend-p99{fill:#c62828}"
        ".raw-legend{fill:#9aa0a6}"
        ".separator{stroke:#e8eaed;stroke-width:1}</style>",
        "<rect width=\"100%\" height=\"100%\" fill=\"#fff\"/>",
        '<text x="42" y="42" font-size="24" font-weight="700">Spatial query latency: raw repetitions and percentiles</text>',
        '<text x="42" y="70" class="muted" font-size="14">10,000 entities · 20 repetitions per case · values in microseconds · logarithmic x-axis</text>',
    ]

    for tick in ticks:
        x = _x_position(tick, left, right)
        output.append(f'<line x1="{x:.2f}" y1="96" x2="{x:.2f}" y2="{bottom + 18:.2f}" class="grid"/>')
        output.append(f'<text x="{x:.2f}" y="{bottom + 42:.2f}" text-anchor="middle" class="muted" font-size="12">{tick:g}</text>')

    for index, row in enumerate(series):
        y = top + index * row_step
        if row["width"] == 101 and row["distribution"] == "Clustered":
            output.append(f'<rect x="28" y="{y - 21:.2f}" width="1538" height="42" rx="5" fill="#fff3e0"/>')
        output.append(f'<text x="42" y="{y + 5:.2f}" font-size="14">{html.escape(row["algorithm"])} · {row["distribution"]} · width {row["width"]}</text>')
        for repetition, value in enumerate(row["values"]):
            jitter = (repetition % 5 - 2) * 2.0
            x = _x_position(value, left, right)
            output.append(f'<circle cx="{x:.2f}" cy="{y + jitter:.2f}" r="2.6" class="raw"/>')
        for percentile_name, offset in (("p50", -6), ("p95", 0), ("p99", 6)):
            value = row["percentiles"][percentile_name]
            x = _x_position(value, left, right)
            marker_y = y + offset
            if shapes[percentile_name] == "circle":
                output.append(f'<circle cx="{x:.2f}" cy="{marker_y:.2f}" r="5" class="{percentile_name}"/>')
            elif shapes[percentile_name] == "triangle":
                output.append(f'<path d="M{x:.2f} {marker_y - 6:.2f} l6 11 h-12 z" class="{percentile_name}"/>')
            else:
                output.append(f'<rect x="{x - 4:.2f}" y="{marker_y - 4:.2f}" width="8" height="8" class="{percentile_name}"/>')
        stats = row["percentiles"]
        label = "  ".join(f"{name} {stats[name]:.2f}" for name in ("p50", "p95", "p99"))
        output.append(f'<text x="1180" y="{y + 5:.2f}" font-size="12">{label} µs</text>')
        if index < len(series) - 1:
            output.append(f'<line x1="42" y1="{y + 26:.2f}" x2="1560" y2="{y + 26:.2f}" class="separator"/>')

    for index, percentile_name in enumerate(("p50", "p95", "p99")):
        x = 990 + index * 100
        if shapes[percentile_name] == "circle":
            output.append(f'<circle cx="{x}" cy="82" r="5" class="legend-{percentile_name}"/>')
        elif shapes[percentile_name] == "triangle":
            output.append(f'<path d="M{x} 76 l6 11 h-12 z" class="legend-{percentile_name}"/>')
        else:
            output.append(f'<rect x="{x - 4}" y="78" width="8" height="8" class="legend-{percentile_name}"/>')
        output.append(f'<text x="{x + 12}" y="87" font-size="12">{percentile_name}</text>')
    output.extend([
        f'<text x="{(left + right) / 2:.2f}" y="{bottom + 68:.2f}" text-anchor="middle" font-size="13">Time per query (µs, log scale)</text>',
        '<circle cx="42" cy="635" r="3" class="raw-legend"/><text x="53" y="639" class="muted" font-size="12">Each gray dot is one measured repetition. Percentiles use linear interpolation across the 20 values; they are not production tail-latency guarantees.</text>',
        "</svg>",
    ])
    return "\n".join(output) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Render raw Google Benchmark query repetitions as an SVG graph."
    )
    parser.add_argument("input", type=Path, help="sanitized Google Benchmark JSON with raw repetitions")
    parser.add_argument("output", type=Path, help="output SVG image")
    args = parser.parse_args()
    try:
        report = json.loads(args.input.read_text(encoding="utf-8"))
        series = collect_series(report)
        svg = render_svg(series)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(svg, encoding="utf-8")
    except (OSError, json.JSONDecodeError, ValueError) as error:
        parser.error(str(error))
    print(f"Rendered {len(series)} cases with {len(series[0]['values'])} repetitions each to {args.output}")
    for row in series:
        values = " ".join(
            f"{name}={row['percentiles'][name]:.2f}us"
            for name in ("p50", "p95", "p99")
        )
        print(f"{row['algorithm']} {row['distribution']} width={row['width']}: {values}")


if __name__ == "__main__":
    main()
