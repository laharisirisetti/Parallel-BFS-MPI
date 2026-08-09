#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List


REPO_ROOT = Path(__file__).resolve().parents[1]
TEST_CASES_DIR = REPO_ROOT / "tests" / "test_cases" / "deterministic"
EXPECTED_DIR = REPO_ROOT / "tests" / "expected" / "deterministic"
SEQUENTIAL_BIN = REPO_ROOT / "tests" / "sequential"
PARALLEL_BIN = REPO_ROOT / "tests" / "parallel"

PROCESS_COUNTS = [1, 2, 3, 4, 8, 12]
COMMAND_TIMEOUT_SECONDS = 5


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


def run_single_test(case_path: Path) -> bool:
    case_name = case_path.name
    expected_path = EXPECTED_DIR / case_name

    if not expected_path.exists():
        print(f"[FAIL] {case_name}: missing expected file {expected_path.name}")
        return False

    input_text = case_path.read_text()
    expected_text = expected_path.read_text()

    try:
        sequential_output = run_command([str(SEQUENTIAL_BIN)], input_text)
        sequential_normalized = normalize_output(sequential_output)
        expected_normalized = normalize_output(expected_text)

        if sequential_normalized != expected_normalized:
            print(f"[FAIL] {case_name}: sequential output does not match expected")
            print(f"  expected: {expected_normalized!r}")
            print(f"  actual  : {sequential_normalized!r}")
            return False

        print(f"[PASS] {case_name}: sequential matches expected")

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

            parallel_output = run_command(
                mpi_command,
                input_text,
            )
            parallel_normalized = normalize_output(parallel_output)

            if parallel_normalized != expected_normalized:
                print(f"[FAIL] {case_name}: parallel np={process_count} output does not match expected")
                print(f"  expected: {expected_normalized!r}")
                print(f"  actual  : {parallel_normalized!r}")
                return False

            print(f"[PASS] {case_name}: parallel np={process_count} matches expected")

        return True

    except RuntimeError as exc:
        print(f"[FAIL] {case_name}: {exc}")
        return False


def main() -> int:
    ensure_binaries()

    case_files = sorted(TEST_CASES_DIR.glob("*.txt"))
    if not case_files:
        print("No test cases found.")
        return 1

    passed = 0
    failed = 0

    for case_path in case_files:
        if run_single_test(case_path):
            passed += 1
        else:
            failed += 1

    print(f"\nSummary: {passed} passed, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
