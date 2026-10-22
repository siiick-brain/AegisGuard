import argparse
import json
from pathlib import Path
from typing import Any


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Remove local host identifiers from Google Benchmark JSON."
    )
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    report: Any = json.loads(args.input.read_text(encoding="utf-8"))
    if not isinstance(report, dict) or not isinstance(report.get("context"), dict):
        parser.error("input is not a Google Benchmark JSON report")
    if not isinstance(report.get("benchmarks"), list):
        parser.error("input does not contain benchmark results")

    context = report["context"]
    context.pop("host_name", None)
    context["executable"] = "aegisguard_spatial_benchmarks"
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
