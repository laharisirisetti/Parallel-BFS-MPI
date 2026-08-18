#!/usr/bin/env python3
import csv
import os
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Dict, Tuple


REPO_ROOT = Path(__file__).resolve().parents[1]
BENCHMARKS_DIR = REPO_ROOT / "benchmarks"
TEST_CASES_DIR = BENCHMARKS_DIR / "graphs"
EXPECTED_DIR = BENCHMARKS_DIR / "expected"
RESULTS_DIR = BENCHMARKS_DIR / "results"
BUILD_DIR = REPO_ROOT / "build"
SEQUENTIAL_BIN = BUILD_DIR / "bin" / "sequential_bfs"
PARALLEL_BIN = BUILD_DIR / "bin" / "parallel_bfs"
PROCESS_COUNTS = [1, 2, 4, 8, 12]
COMMAND_TIMEOUT_SECONDS = 120
WARMUP_RUNS = 1
MEASURED_RUNS = 5


def normalize_output(text: str) -> str:
    return " ".join(text.strip().split())


def run_command(command: List[str], input_text: str) -> Tuple[str, str]:
    try:
        result = subprocess.run(
            command,
            input=input_text,
            text=True,
            capture_output=True,
            check=False,
            timeout=COMMAND_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(
            f"Command timed out after {COMMAND_TIMEOUT_SECONDS}s: {' '.join(command)}"
        ) from exc

    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {' '.join(command)}\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
    return result.stdout, result.stderr


def ensure_binaries() -> None:
    bin_dir = BUILD_DIR / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["g++", "-std=c++17", "-O2", str(REPO_ROOT / "src" / "sequential_bfs.cpp"), "-o", str(SEQUENTIAL_BIN)],
        cwd=REPO_ROOT,
        check=True,
        timeout=COMMAND_TIMEOUT_SECONDS,
    )
    subprocess.run(
        ["mpic++", "-std=c++17", "-O2", str(REPO_ROOT / "src" / "parallel_bfs.cpp"), "-o", str(PARALLEL_BIN)],
        cwd=REPO_ROOT,
        check=True,
        timeout=COMMAND_TIMEOUT_SECONDS,
    )


def load_expected(case_name: str) -> str:
    expected_path = EXPECTED_DIR / case_name
    if not expected_path.exists():
        raise FileNotFoundError(f"Missing expected output: {expected_path}")
    return expected_path.read_text(encoding="utf-8")


def parse_time(stderr_text: str) -> float:
    # binaries print "TIME <seconds>" for the algorithm region only
    for line in stderr_text.splitlines():
        if line.startswith("TIME"):
            return float(line.split()[1])
    raise RuntimeError("no TIME line found in stderr")


def run_and_measure(command: List[str], input_text: str) -> Tuple[str, float]:
    output, stderr_text = run_command(command, input_text)
    elapsed = parse_time(stderr_text)
    return output, elapsed


def parse_graph_metadata(input_text: str) -> Tuple[int, int]:
    lines = [line.strip() for line in input_text.splitlines() if line.strip()]
    if not lines:
        raise ValueError("Input graph is empty")

    parts = lines[0].split()
    if len(parts) < 3:
        raise ValueError("Input graph header is malformed")

    vertex_count = int(parts[0])
    edge_count = int(parts[1])
    return vertex_count, edge_count


def summarize_times(times: List[float]) -> Dict[str, float]:
    if not times:
        return {"min": float("nan"), "median": float("nan"), "mean": float("nan"), "max": float("nan")}

    return {
        "min": min(times),
        "median": statistics.median(times),
        "mean": statistics.mean(times),
        "max": max(times),
    }


def write_results_csv(rows: List[Dict[str, object]]) -> None:
    fieldnames = [
        "case",
        "implementation",
        "process_count",
        "vertex_count",
        "edge_count",
        "min_time_seconds",
        "median_time_seconds",
        "mean_time_seconds",
        "max_time_seconds",
        "correct",
    ]

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    results_path = RESULTS_DIR / "final.csv"

    with results_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote benchmark results to {results_path}")


def main() -> int:
    ensure_binaries()

    case_files = sorted(TEST_CASES_DIR.glob("*.txt"))
    if not case_files:
        print("No benchmark test cases found.")
        return 1

    rows: List[Dict[str, object]] = []

    for case_path in case_files:
        case_name = case_path.name
        input_text = case_path.read_text(encoding="utf-8")
        expected_text = load_expected(case_name)
        expected_normalized = normalize_output(expected_text)

        vertex_count, edge_count = parse_graph_metadata(input_text)
        print(f"Benchmarking {case_name}... (V={vertex_count}, E={edge_count})")

        try:
            sequential_times: List[float] = []
            sequential_output = ""
            sequential_correct = False

            for _ in range(WARMUP_RUNS):
                run_command([str(SEQUENTIAL_BIN)], input_text)

            for _ in range(MEASURED_RUNS):
                sequential_output, sequential_time = run_and_measure([str(SEQUENTIAL_BIN)], input_text)
                sequential_times.append(sequential_time)

            sequential_correct = normalize_output(sequential_output) == expected_normalized
            summary = summarize_times(sequential_times)
            rows.append(
                {
                    "case": case_name,
                    "implementation": "sequential",
                    "process_count": 1,
                    "vertex_count": vertex_count,
                    "edge_count": edge_count,
                    "min_time_seconds": round(summary["min"], 6),
                    "median_time_seconds": round(summary["median"], 6),
                    "mean_time_seconds": round(summary["mean"], 6),
                    "max_time_seconds": round(summary["max"], 6),
                    "correct": sequential_correct,
                }
            )
            print(f"  sequential: min={summary['min']:.6f}s median={summary['median']:.6f}s mean={summary['mean']:.6f}s max={summary['max']:.6f}s {'PASS' if sequential_correct else 'FAIL'}")
        except RuntimeError as exc:
            print(f"  sequential: FAIL - {exc}")
            rows.append(
                {
                    "case": case_name,
                    "implementation": "sequential",
                    "process_count": 1,
                    "vertex_count": vertex_count,
                    "edge_count": edge_count,
                    "min_time_seconds": None,
                    "median_time_seconds": None,
                    "mean_time_seconds": None,
                    "max_time_seconds": None,
                    "correct": False,
                }
            )

        for process_count in PROCESS_COUNTS:
            mpi_command = [
                "mpirun",
                "--oversubscribe",
                "-np",
                str(process_count),
                str(PARALLEL_BIN),
            ]

            if getattr(os, "geteuid", lambda: -1)() == 0:
                mpi_command.insert(1, "--allow-run-as-root")

            try:
                parallel_times: List[float] = []
                parallel_output = ""
                parallel_correct = False

                for _ in range(WARMUP_RUNS):
                    run_command(mpi_command, input_text)

                for _ in range(MEASURED_RUNS):
                    parallel_output, parallel_time = run_and_measure(mpi_command, input_text)
                    parallel_times.append(parallel_time)

                parallel_correct = normalize_output(parallel_output) == expected_normalized
                summary = summarize_times(parallel_times)
                rows.append(
                    {
                        "case": case_name,
                        "implementation": "parallel",
                        "process_count": process_count,
                        "vertex_count": vertex_count,
                        "edge_count": edge_count,
                        "min_time_seconds": round(summary["min"], 6),
                        "median_time_seconds": round(summary["median"], 6),
                        "mean_time_seconds": round(summary["mean"], 6),
                        "max_time_seconds": round(summary["max"], 6),
                        "correct": parallel_correct,
                    }
                )
                print(f"  parallel np={process_count}: min={summary['min']:.6f}s median={summary['median']:.6f}s mean={summary['mean']:.6f}s max={summary['max']:.6f}s {'PASS' if parallel_correct else 'FAIL'}")
            except RuntimeError as exc:
                print(f"  parallel np={process_count}: FAIL - {exc}")
                rows.append(
                    {
                        "case": case_name,
                        "implementation": "parallel",
                        "process_count": process_count,
                        "vertex_count": vertex_count,
                        "edge_count": edge_count,
                        "min_time_seconds": None,
                        "median_time_seconds": None,
                        "mean_time_seconds": None,
                        "max_time_seconds": None,
                        "correct": False,
                    }
                )

    write_results_csv(rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
