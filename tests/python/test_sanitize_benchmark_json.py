import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SANITIZER = REPOSITORY_ROOT / "tools" / "evaluation" / "sanitize_benchmark_json.py"


class SanitizeBenchmarkJsonTests(unittest.TestCase):
    def test_removes_host_name_and_preserves_benchmark_data(self) -> None:
        report = {
            "context": {
                "host_name": "local-machine",
                "executable": "C:/private/build/benchmark.exe",
                "num_cpus": 8,
            },
            "benchmarks": [{"name": "quadtree/query", "real_time": 1.25}],
        }
        with tempfile.TemporaryDirectory() as temporary_directory:
            input_path = Path(temporary_directory) / "input.json"
            output_path = Path(temporary_directory) / "output.json"
            input_path.write_text(json.dumps(report), encoding="utf-8")

            result = subprocess.run(
                [sys.executable, str(SANITIZER), str(input_path), str(output_path)],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            sanitized = json.loads(output_path.read_text(encoding="utf-8"))

        self.assertNotIn("host_name", sanitized["context"])
        self.assertEqual(
            sanitized["context"]["executable"], "aegisguard_spatial_benchmarks"
        )
        self.assertEqual(sanitized["context"]["num_cpus"], 8)
        self.assertEqual(sanitized["benchmarks"], report["benchmarks"])

    def test_rejects_input_without_benchmark_results(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            input_path = Path(temporary_directory) / "input.json"
            output_path = Path(temporary_directory) / "output.json"
            input_path.write_text(
                json.dumps({"context": {}, "benchmarks": None}),
                encoding="utf-8",
            )

            result = subprocess.run(
                [sys.executable, str(SANITIZER), str(input_path), str(output_path)],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 2)
            self.assertIn("does not contain benchmark results", result.stderr)
            self.assertFalse(output_path.exists())


if __name__ == "__main__":
    unittest.main()
