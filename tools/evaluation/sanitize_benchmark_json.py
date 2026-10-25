import argparse
import json
from pathlib import Path
from typing import Any


def sanitize_report(report: Any) -> dict[str, Any]:
    if not isinstance(report, dict) or not isinstance(report.get("context"), dict):
        raise ValueError("input is not a Google Benchmark JSON report")
    if not isinstance(report.get("benchmarks"), list):
        raise ValueError("input does not contain benchmark results")

    sanitized = dict(report)
    context = dict(report["context"])
    context.pop("host_name", None)
    context["executable"] = "aegisguard_spatial_benchmarks"
    sanitized["context"] = context
    return sanitized


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Remove local host identifiers from Google Benchmark JSON."
    )
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    report: Any = json.loads(args.input.read_text(encoding="utf-8"))
    try:
        sanitized = sanitize_report(report)
    except ValueError as error:
        parser.error(str(error))
    args.output.write_text(
        json.dumps(sanitized, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
