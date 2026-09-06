#!/usr/bin/env python3
"""Render benchmark results from Google Benchmark JSON output.

Usage:
    python3 plot.py [results.json] [output.png]

Reads the aggregate rows produced by --benchmark_format=json and draws a
bar chart of median times per operation, annotated with the coefficient of
variation for each measurement.
"""

import json
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")  # no display needed; write straight to file
import matplotlib.pyplot as plt

# Map raw benchmark names to readable labels, in the order they should appear.
# Google Benchmark decorates names with its configuration (e.g.
# "BM_CancelOrderHit/iterations:100000/repeats:10_median"), so match on the
# run_name prefix rather than the full string.
LABELS = [
    ("BM_AddOrder", "addOrder"),
    ("BM_CancelOrderHit", "cancelOrder\n(hit)"),
    ("BM_CancelOrderMiss", "cancelOrder\n(miss)"),
    ("BM_ModifyOrder_Friendly", "modifyOrder\n(friendly)"),
    ("BM_ModifyOrder_Unfriendly", "modifyOrder\n(unfriendly)"),
]

BAR_COLOR = "#4C72B0"
LABEL_COLOR = "#333333"


def load_aggregates(path):
    """Return {run_name_prefix: {aggregate_name: value}} from a results file."""
    with open(path) as f:
        data = json.load(f)

    aggregates = {}
    for entry in data["benchmarks"]:
        if entry.get("run_type") != "aggregate":
            continue
        run_name = entry["run_name"]
        prefix = run_name.split("/")[0]
        aggregates.setdefault(prefix, {})[entry["aggregate_name"]] = entry["real_time"]

    return aggregates, data.get("context", {})


def build_series(aggregates):
    """Pull medians and CVs in LABELS order, skipping anything not present."""
    labels, medians, cvs = [], [], []
    for prefix, label in LABELS:
        stats = aggregates.get(prefix)
        if stats is None or "median" not in stats:
            print(f"warning: no median found for {prefix}, skipping", file=sys.stderr)
            continue
        labels.append(label)
        medians.append(stats["median"])
        # Google Benchmark reports cv as a fraction, not a percentage.
        cvs.append(stats.get("cv", 0.0) * 100.0)
    return labels, medians, cvs


def plot(labels, medians, cvs, context, output):
    fig, ax = plt.subplots(figsize=(9, 5.5))

    bars = ax.bar(labels, medians, color=BAR_COLOR, width=0.6)

    headroom = max(medians) * 1.18
    ax.set_ylim(0, headroom)
    ax.set_ylabel("Median time (ns)")

    # Annotate each bar with its value and measurement spread. Without this the
    # sub-20ns bars are visually indistinguishable from zero on a linear axis.
    offset = max(medians) * 0.02
    for bar, median, cv in zip(bars, medians, cvs):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + offset,
            f"{median:.1f} ns\nCV {cv:.2f}%",
            ha="center",
            va="bottom",
            fontsize=9,
            color=LABEL_COLOR,
        )

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(axis="y", alpha=0.25, linewidth=0.6)
    ax.set_axisbelow(True)

    subtitle_parts = []
    if context.get("library_build_type"):
        subtitle_parts.append(context["library_build_type"].capitalize() + " build")
    if context.get("date"):
        subtitle_parts.append(context["date"][:10])
    subtitle_parts.append("median of 10 repetitions")

    fig.suptitle("Order book operation latency", fontsize=13, y=0.97)
    ax.set_title(" · ".join(subtitle_parts), fontsize=9, color="#666666", pad=12)

    fig.tight_layout()
    fig.savefig(output, dpi=150)
    print(f"wrote {output}")


def main():
    here = Path(__file__).parent
    results = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "results.json"
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else here / "benchmark_plot.png"

    if not results.exists():
        sys.exit(
            f"error: {results} not found.\n"
            "Generate it with:\n"
            "  ./build/benchmarks/orderbook_benchmark "
            "--benchmark_format=json > benchmarks/results.json"
        )

    aggregates, context = load_aggregates(results)
    labels, medians, cvs = build_series(aggregates)

    if not labels:
        sys.exit("error: no aggregate results found in the input file")

    plot(labels, medians, cvs, context, output)


if __name__ == "__main__":
    main()
