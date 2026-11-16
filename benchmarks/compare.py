#!/usr/bin/env python3
"""Compare two Google Benchmark JSON results side by side.

Usage:
    python3 compare.py before.json after.json [output.png]
                       [--before-label NAME] [--after-label NAME]

Draws the median time of each operation in both files as a pair of bars,
annotated with the two values and the change from the first to the second.
The two runs only mean something together if they were made back to back on
the same machine.
"""

from argparse import ArgumentParser
from pathlib import Path

import matplotlib.pyplot as plt

from plot import LABELS, LABEL_COLOR, SUBTITLE_COLOR, build_subtitle, load_aggregates

BEFORE_COLOR = "#DD8452"
AFTER_COLOR = "#4C72B0"
BAR_WIDTH = 0.38
BAR_GAP = 0.02


def build_pairs(before, after):
    """Return (label, before_median, after_median) for operations present in both files."""
    pairs = []
    for prefix, label in LABELS:
        if "median" not in before.get(prefix, {}) or "median" not in after.get(prefix, {}):
            print(f"warning: {prefix} is not in both files, skipping")
            continue
        pairs.append((label, before[prefix]["median"], after[prefix]["median"]))
    return pairs


def plot_comparison(pairs, before_label, after_label, context, output):
    fig, ax = plt.subplots(figsize=(10, 5.5))

    positions = range(len(pairs))
    offset = (BAR_WIDTH + BAR_GAP) / 2
    before_medians = [before for _, before, _ in pairs]
    after_medians = [after for _, _, after in pairs]

    ax.bar([x - offset for x in positions], before_medians, BAR_WIDTH, color=BEFORE_COLOR, label=before_label)
    ax.bar([x + offset for x in positions], after_medians, BAR_WIDTH, color=AFTER_COLOR, label=after_label)

    tallest = max(before_medians + after_medians)
    ax.set_ylim(0, tallest * 1.22)
    ax.set_ylabel("Median time (ns)")
    ax.set_xticks(list(positions))
    ax.set_xticklabels([label for label, _, _ in pairs])

    # Annotate each pair with both values and the change. The bars alone cannot
    # show a difference of a few percent, or anything about the sub-20ns pairs.
    for x, (_, before, after) in zip(positions, pairs):
        change = (after / before - 1) * 100
        change_text = f"{change:+.1f}%".replace("-", "−")  # a real minus sign, not a hyphen
        ax.text(
            x,
            max(before, after) + tallest * 0.02,
            f"{change_text}\n{before:.1f} → {after:.1f} ns",
            ha="center",
            va="bottom",
            fontsize=9,
            color=LABEL_COLOR,
        )

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.grid(axis="y", alpha=0.25, linewidth=0.6)
    ax.set_axisbelow(True)
    ax.legend(frameon=False, loc="upper right")

    fig.suptitle(f"{before_label} against {after_label}", fontsize=13, y=0.97)
    ax.set_title(build_subtitle(context), fontsize=9, color=SUBTITLE_COLOR, pad=12)

    fig.tight_layout()
    fig.savefig(output, dpi=150)
    print(f"wrote {output}")


def main():
    here = Path(__file__).parent

    parser = ArgumentParser(description="Compare two Google Benchmark JSON results side by side.")
    parser.add_argument("before", type=Path, help="results file of the first run")
    parser.add_argument("after", type=Path, help="results file of the second run")
    parser.add_argument("output", type=Path, nargs="?", default=here / "comparison_plot.png", help="where to write the chart")
    parser.add_argument("--before-label", default="before", help="legend name of the first run")
    parser.add_argument("--after-label", default="after", help="legend name of the second run")
    args = parser.parse_args()

    before, _ = load_aggregates(args.before)
    after, context = load_aggregates(args.after)

    pairs = build_pairs(before, after)
    if not pairs:
        parser.error("the two files have no operation in common")

    plot_comparison(pairs, args.before_label, args.after_label, context, args.output)


if __name__ == "__main__":
    main()
