#!/usr/bin/env python3
"""Plot BFS benchmark scaling as standalone SVG files (no third-party deps).

Reads benchmarks/results/final.csv and writes:
  benchmarks/plots/runtime.svg   median BFS-region runtime vs np (log scale), per graph
  benchmarks/plots/speedup.svg   speedup vs np, per graph, with ideal y=x

Timing is the algorithm-region-only figure the binaries report (MPI_Wtime),
not end-to-end wall clock.
"""
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

BENCHMARKS_DIR = Path(__file__).resolve().parent
RESULTS_DIR = BENCHMARKS_DIR / "results"
PLOTS_DIR = BENCHMARKS_DIR / "plots"
DEFAULT_INPUT = RESULTS_DIR / "final.csv"

W, H = 860, 460
ML, MR, MT, MB = 70, 300, 46, 54
PX0, PX1 = ML, W - MR
PY0, PY1 = MT, H - MB
COLORS = ["#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e", "#17becf"]


def load(path):
    cases = defaultdict(lambda: {"np": [], "runtime": [], "seq": None,
                                 "V": 0, "E": 0})
    with open(path) as f:
        for r in csv.DictReader(f):
            c = cases[r["case"].replace(".txt", "")]
            c["V"] = int(r["vertex_count"])
            c["E"] = int(r["edge_count"])
            t = float(r["median_time_seconds"])
            if r["implementation"] == "sequential":
                c["seq"] = t
            else:
                c["np"].append(int(r["process_count"]))
                c["runtime"].append(t)
    for c in cases.values():
        order = sorted(range(len(c["np"])), key=lambda i: c["np"][i])
        c["np"] = [c["np"][i] for i in order]
        c["runtime"] = [c["runtime"][i] for i in order]
    return cases


def _sx(x, xmin, xmax):
    return PX0 + (x - xmin) / (xmax - xmin) * (PX1 - PX0)


def _sy(y, ymin, ymax):
    return PY1 - (y - ymin) / (ymax - ymin) * (PY1 - PY0)


def _nice_ticks(vmax, n=5):
    step = vmax / n
    mag = 10 ** math.floor(math.log10(step)) if step > 0 else 1
    for m in (1, 2, 2.5, 5, 10):
        if m * mag >= step:
            step = m * mag
            break
    ticks, v = [], 0.0
    while v <= vmax + 1e-9:
        ticks.append(round(v, 6))
        v += step
    return ticks


def svg_chart(path, title, series, xvals, ylabel, ideal=None, logy=False):
    xmin, xmax = min(xvals), max(xvals)
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
        f'font-family="sans-serif" font-size="13">',
        f'<rect width="{W}" height="{H}" fill="white"/>',
        f'<text x="{W/2}" y="26" text-anchor="middle" font-size="16" '
        f'font-weight="bold">{title}</text>',
    ]

    if logy:
        allv = [v for _, ys in series for v in ys if v > 0]
        lo = math.floor(math.log10(min(allv)))
        hi = math.ceil(math.log10(max(allv)))
        ymin, ymax = lo, hi

        def ymap(v):
            return _sy(math.log10(v), ymin, ymax)
        for e in range(lo, hi + 1):
            y = _sy(e, ymin, ymax)
            parts.append(f'<line x1="{PX0}" y1="{y:.1f}" x2="{PX1}" y2="{y:.1f}" stroke="#e6e6e6"/>')
            parts.append(f'<text x="{PX0-8}" y="{y+4:.1f}" text-anchor="end" fill="#444">1e{e}</text>')
    else:
        ymax = max(v for _, ys in series for v in ys)
        if ideal is not None:
            ymax = max(ymax, max(ideal(x) for x in xvals))
        ymax *= 1.1
        ymin = 0

        def ymap(v):
            return _sy(v, ymin, ymax)
        for t in _nice_ticks(ymax):
            y = _sy(t, ymin, ymax)
            parts.append(f'<line x1="{PX0}" y1="{y:.1f}" x2="{PX1}" y2="{y:.1f}" stroke="#e6e6e6"/>')
            parts.append(f'<text x="{PX0-8}" y="{y+4:.1f}" text-anchor="end" fill="#444">{t:g}</text>')

    for xv in xvals:
        x = _sx(xv, xmin, xmax)
        parts.append(f'<line x1="{x:.1f}" y1="{PY1}" x2="{x:.1f}" y2="{PY1+5}" stroke="#444"/>')
        parts.append(f'<text x="{x:.1f}" y="{PY1+20}" text-anchor="middle" fill="#444">{xv}</text>')
    parts.append(f'<line x1="{PX0}" y1="{PY0}" x2="{PX0}" y2="{PY1}" stroke="#444"/>')
    parts.append(f'<line x1="{PX0}" y1="{PY1}" x2="{PX1}" y2="{PY1}" stroke="#444"/>')
    parts.append(f'<text x="{(PX0+PX1)/2}" y="{H-12}" text-anchor="middle" fill="#444">MPI process count</text>')
    parts.append(f'<text x="18" y="{(PY0+PY1)/2}" text-anchor="middle" fill="#444" '
                 f'transform="rotate(-90 18 {(PY0+PY1)/2})">{ylabel}</text>')

    if ideal is not None and not logy:
        pts = " ".join(f"{_sx(x,xmin,xmax):.1f},{ymap(ideal(x)):.1f}" for x in xvals)
        parts.append(f'<polyline points="{pts}" fill="none" stroke="#888" stroke-dasharray="6 4"/>')

    for i, (label, ys) in enumerate(series):
        color = COLORS[i % len(COLORS)]
        pts = " ".join(f"{_sx(x,xmin,xmax):.1f},{ymap(y):.1f}" for x, y in zip(xvals, ys))
        parts.append(f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="2"/>')
        for x, y in zip(xvals, ys):
            parts.append(f'<circle cx="{_sx(x,xmin,xmax):.1f}" cy="{ymap(y):.1f}" r="3" fill="{color}"/>')

    lx, ly = PX1 + 18, PY0 + 8
    legend = list(series) + ([("ideal", None)] if ideal is not None else [])
    for i, (label, _) in enumerate(legend):
        y = ly + i * 22
        color = "#888" if label == "ideal" else COLORS[i % len(COLORS)]
        dash = ' stroke-dasharray="6 4"' if label == "ideal" else ""
        parts.append(f'<line x1="{lx}" y1="{y}" x2="{lx+22}" y2="{y}" stroke="{color}" stroke-width="2"{dash}/>')
        parts.append(f'<text x="{lx+28}" y="{y+4}" fill="#333" font-size="12">{label}</text>')
    parts.append("</svg>")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(parts))
    print("wrote", path)


def main():
    parser = argparse.ArgumentParser(description="Plot BFS benchmark scaling as SVG.")
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output-dir", type=Path, default=PLOTS_DIR)
    args = parser.parse_args()

    cases = load(args.input)
    names = sorted(cases)
    xvals = cases[names[0]]["np"]

    def label(n):
        c = cases[n]
        return f"{n} (V={c['V']}, E={c['E']})"

    svg_chart(args.output_dir / "runtime.svg", "BFS runtime vs processes",
              [(label(n), cases[n]["runtime"]) for n in names], xvals,
              "median BFS time (s)", logy=True)

    speedups = {n: [cases[n]["seq"] / t for t in cases[n]["runtime"]] for n in names}
    svg_chart(args.output_dir / "speedup.svg", "BFS speedup vs processes",
              [(label(n), speedups[n]) for n in names], xvals,
              "speedup (T_seq / T_par)", ideal=lambda x: x)


if __name__ == "__main__":
    main()
