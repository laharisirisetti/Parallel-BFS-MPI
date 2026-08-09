#!/usr/bin/env python3
import csv
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Dict, Tuple


REPO_ROOT = Path(__file__).resolve().parents[1]
TEST_CASES_DIR = REPO_ROOT / "tests" / "test_cases" / "benchmark"
EXPECTED_DIR = REPO_ROOT / "tests" / "expected" / "benchmark"
SEQUENTIAL_BIN = REPO_ROOT / "tests" / "sequential"
PARALLEL_BIN = REPO_ROOT / "tests" / "parallel"
RESULTS_DIR = REPO_ROOT / "tests" / "results"
PROCESS_COUNTS = [1, 2, 4, 8, 12]
COMMAND_TIMEOUT_SECONDS = 120


def normalize_output(text: str) -> str:
    return " ".join(text.strip().split())


def run_command(command: List[str], input_text: str) -> str:
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
    return result.stdout


def ensure_binaries() -> None:
    subprocess.run(
        ["g++", "-std=c++17", "src/sequential_bfs.cpp", "-O2", "-o", str(SEQUENTIAL_BIN)],
        cwd=REPO_ROOT,
        check=True,
        timeout=COMMAND_TIMEOUT_SECONDS,
    )

    if shutil.which("mpic++") is None:
        raise RuntimeError("mpic++ is not available. Install OpenMPI or provide the MPI compiler wrapper.")

    subprocess.run(
        ["mpic++", "-std=c++17", "src/parallel_bfs.cpp", "-O2", "-o", str(PARALLEL_BIN)],
        cwd=REPO_ROOT,
        check=True,
        timeout=COMMAND_TIMEOUT_SECONDS,
    )


def load_expected(case_name: str) -> str:
    expected_path = EXPECTED_DIR / case_name
    if not expected_path.exists():
        raise FileNotFoundError(f"Missing expected output: {expected_path}")
    return expected_path.read_text(encoding="utf-8")


def run_and_measure(command: List[str], input_text: str) -> Tuple[str, float]:
    start = time.perf_counter()
    output = run_command(command, input_text)
    elapsed = time.perf_counter() - start
    return output, elapsed


def write_results_csv(rows: List[Dict[str, object]]) -> None:
    fieldnames = [
        "case",
        "implementation",
        "process_count",
        "time_seconds",
        "correct",
    ]

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    results_path = RESULTS_DIR / f"benchmark_results_{timestamp}.csv"

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

        print(f"Benchmarking {case_name}...")

        try:
            sequential_output, sequential_time = run_and_measure([str(SEQUENTIAL_BIN)], input_text)
            sequential_correct = normalize_output(sequential_output) == expected_normalized
            rows.append(
                {
                    "case": case_name,
                    "implementation": "sequential",
                    "process_count": 1,
                    "time_seconds": round(sequential_time, 6),
                    "correct": sequential_correct,
                }
            )
            print(f"  sequential: {sequential_time:.6f}s {'PASS' if sequential_correct else 'FAIL'}")
        except RuntimeError as exc:
            print(f"  sequential: FAIL - {exc}")
            rows.append(
                {
                    "case": case_name,
                    "implementation": "sequential",
                    "process_count": 1,
                    "time_seconds": None,
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

            if os.geteuid() == 0:
                mpi_command.insert(1, "--allow-run-as-root")

            try:
                parallel_output, parallel_time = run_and_measure(mpi_command, input_text)
                parallel_correct = normalize_output(parallel_output) == expected_normalized
                rows.append(
                    {
                        "case": case_name,
                        "implementation": "parallel",
                        "process_count": process_count,
                        "time_seconds": round(parallel_time, 6),
                        "correct": parallel_correct,
                    }
                )
                print(f"  parallel np={process_count}: {parallel_time:.6f}s {'PASS' if parallel_correct else 'FAIL'}")
            except RuntimeError as exc:
                print(f"  parallel np={process_count}: FAIL - {exc}")
                rows.append(
                    {
                        "case": case_name,
                        "implementation": "parallel",
                        "process_count": process_count,
                        "time_seconds": None,
                        "correct": False,
                    }
                )

    write_results_csv(rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
