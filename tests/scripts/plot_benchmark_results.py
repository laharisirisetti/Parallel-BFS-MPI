#!/usr/bin/env python3
import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


RESULTS_DIR = Path(__file__).resolve().parents[2] / "tests" / "results"
DEFAULT_INPUT = RESULTS_DIR / "final.csv"
DEFAULT_OUTPUT = RESULTS_DIR / "benchmark_scaling.png"


def load_results(path: Path):
    rows = []
    with path.open("r", encoding="utf-8", newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            rows.append(
                {
                    "case": row["case"],
                    "implementation": row["implementation"],
                    "process_count": int(row["process_count"]),
                    "median_time_seconds": float(row["median_time_seconds"]),
                    "vertex_count": int(row["vertex_count"]),
                    "edge_count": int(row["edge_count"]),
                }
            )
    return rows


def group_results(rows):
    grouped = defaultdict(lambda: {"sequential": None, "parallel": []})
    for row in rows:
        case = row["case"]
        impl = row["implementation"]
        if impl == "sequential":
            grouped[case]["sequential"] = row
        elif impl == "parallel":
            grouped[case]["parallel"].append(row)
    return grouped


def pick_cases(grouped, requested):
    if requested:
        return [case for case in requested if case in grouped]
    return sorted(grouped.keys())


def plot_scaling(grouped, cases, output_path: Path):
    fig, (ax_runtime, ax_speedup) = plt.subplots(
        2, 1, figsize=(10, 10), sharex=True
    )

    for case in cases:
        data = grouped[case]
        parallel_rows = sorted(data["parallel"], key=lambda row: row["process_count"])
        if not parallel_rows or data["sequential"] is None:
            continue

        process_counts = [row["process_count"] for row in parallel_rows]
        median_times = [row["median_time_seconds"] for row in parallel_rows]
        speedups = [data["sequential"]["median_time_seconds"] / t for t in median_times]

        metadata = data["sequential"]
        label = f"{case.replace('.txt', '')} (V={metadata['vertex_count']}, E={metadata['edge_count']})"

        ax_runtime.plot(
            process_counts,
            median_times,
            marker="o",
            label=label,
        )
        ax_speedup.plot(
            process_counts,
            speedups,
            marker="o",
            label=label,
        )

    ax_runtime.set_title("MPI BFS Median Runtime vs Process Count")
    ax_runtime.set_ylabel("Median runtime (seconds)")
    ax_runtime.grid(True, linestyle="--", alpha=0.5)
    ax_runtime.legend()

    ax_speedup.set_title("MPI BFS Speedup vs Process Count")
    ax_speedup.set_xlabel("MPI process count")
    ax_speedup.set_ylabel("Speedup over sequential")
    ax_speedup.grid(True, linestyle="--", alpha=0.5)
    ax_speedup.legend()

    fig.tight_layout()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=200)
    print(f"Saved scaling plot to {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="Plot BFS benchmark scaling from benchmark result CSV data."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=DEFAULT_INPUT,
        help=f"Input CSV file (default: {DEFAULT_INPUT})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Output image path (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument(
        "--case",
        action="append",
        help="Specific graph case(s) to include. Use multiple times or comma-separated values.",
    )

    args = parser.parse_args()

    cases = []
    if args.case:
        for item in args.case:
            cases.extend([c.strip() for c in item.split(",") if c.strip()])

    rows = load_results(args.input)
    grouped = group_results(rows)
    selected_cases = pick_cases(grouped, cases)

    if not selected_cases:
        raise SystemExit("No matching cases found in benchmark results.")

    plot_scaling(grouped, selected_cases, args.output)


if __name__ == "__main__":
    main()
