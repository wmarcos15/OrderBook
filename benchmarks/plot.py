#!/usr/bin/env python3
"""Render benchmark results from Google Benchmark JSON output.

Usage:
    python3 plot.py [results.json] [output.png] [curves_output.png]

Reads the aggregate rows produced by --benchmark_format=json and draws a
bar chart of median times per operation, annotated with the coefficient of
variation for each measurement. If the results include the matching sweep
benchmark, it also draws the cost of a crossing order against the levels it
sweeps and against the orders it fills.
"""

import json
import re
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

# The sweep benchmark has one run per point, named like
# "BM_MatchOrder_Sweep/levels:8/orders:1/iterations:16000/repeats:10".
SWEEP_PREFIX = "BM_MatchOrder_Sweep"
SWEEP_POINT = re.compile(r"levels:(\d+)/orders:(\d+)")
SWEEP_TICKS = [1, 8, 16, 32, 64]

BAR_COLOR = "#4C72B0"
LABEL_COLOR = "#333333"
SUBTITLE_COLOR = "#666666"


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


def load_sweep_medians(path):
    """Return {(levels, orders_per_level): median} for the sweep benchmark."""
    with open(path) as f:
        data = json.load(f)

    medians = {}
    for entry in data["benchmarks"]:
        if entry.get("aggregate_name") != "median":
            continue
        if not entry["run_name"].startswith(SWEEP_PREFIX):
            continue
        point = SWEEP_POINT.search(entry["run_name"])
        if point is None:
            continue
        levels, orders_per_level = int(point.group(1)), int(point.group(2))
        medians[(levels, orders_per_level)] = entry["real_time"]

    return medians


def fit_line(xs, ys):
    """Return the least-squares (slope, intercept) of ys against xs."""
    count = len(xs)
    mean_x = sum(xs) / count
    mean_y = sum(ys) / count
    spread = sum((x - mean_x) ** 2 for x in xs)
    slope = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys)) / spread
    return slope, mean_y - slope * mean_x


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

    fig.suptitle("Order book operation latency", fontsize=13, y=0.97)
    ax.set_title(build_subtitle(context), fontsize=9, color=SUBTITLE_COLOR, pad=12)

    fig.tight_layout()
    fig.savefig(output, dpi=150)
    print(f"wrote {output}")


def build_subtitle(context):
    """Return the line under a figure title: build type, date and method."""
    subtitle_parts = []
    if context.get("library_build_type"):
        subtitle_parts.append(context["library_build_type"].capitalize() + " build")
    if context.get("date"):
        subtitle_parts.append(context["date"][:10])
    subtitle_parts.append("median of 10 repetitions")
    return " · ".join(subtitle_parts)


def plot_curve(ax, xs, ys, x_label, title, unit):
    """Draw one cost curve with its fitted line and the slope it implies."""
    slope, intercept = fit_line(xs, ys)

    ax.plot(xs, [slope * x + intercept for x in xs], color=BAR_COLOR, linewidth=1.2, alpha=0.5)
    ax.plot(xs, ys, "o", color=BAR_COLOR, markersize=5)

    ax.text(
        0.05,
        0.92,
        f"{slope:.0f} ns per extra {unit}",
        transform=ax.transAxes,
        ha="left",
        va="top",
        fontsize=10,
        color=LABEL_COLOR,
    )

    ax.set_title(title, fontsize=10, color=LABEL_COLOR)
    ax.set_xlabel(x_label)
    ax.set_xticks(SWEEP_TICKS)

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(alpha=0.25, linewidth=0.6)
    ax.set_axisbelow(True)


def plot_curves(sweep_medians, context, output):
    # One order at each level: what an extra level costs
    by_levels = sorted((levels, median) for (levels, orders), median in sweep_medians.items() if orders == 1)
    # A single level: what an extra order costs
    by_orders = sorted((orders, median) for (levels, orders), median in sweep_medians.items() if levels == 1)

    fig, (levels_ax, orders_ax) = plt.subplots(1, 2, figsize=(11, 5), sharey=True)

    plot_curve(
        levels_ax,
        [levels for levels, _ in by_levels],
        [median for _, median in by_levels],
        "Levels swept",
        "One order at each level",
        "level",
    )
    plot_curve(
        orders_ax,
        [orders for orders, _ in by_orders],
        [median for _, median in by_orders],
        "Orders filled",
        "All orders at one level",
        "order",
    )
    # The two panels share the y axis, so the taller curve sets the limit for both
    levels_ax.set_ylim(0, max(sweep_medians.values()) * 1.1)
    levels_ax.set_ylabel("Median time (ns)")

    fig.suptitle("Cost of one crossing order", fontsize=13, y=0.98)
    fig.text(0.5, 0.915, build_subtitle(context), ha="center", fontsize=9, color=SUBTITLE_COLOR)

    fig.tight_layout(rect=(0, 0, 1, 0.92))
    fig.savefig(output, dpi=150)
    print(f"wrote {output}")


def main():
    here = Path(__file__).parent
    results = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "results.json"
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else here / "benchmark_plot.png"
    curves_output = Path(sys.argv[3]) if len(sys.argv) > 3 else here / "matching_curves_plot.png"

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

    sweep_medians = load_sweep_medians(results)
    if not sweep_medians:
        print(f"warning: no {SWEEP_PREFIX} results found, skipping the curves", file=sys.stderr)
        return

    plot_curves(sweep_medians, context, curves_output)


if __name__ == "__main__":
    main()
