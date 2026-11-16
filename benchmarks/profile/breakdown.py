#!/usr/bin/env python3
"""Profile each operation of the book and print where its time goes.

Usage:
    python3 breakdown.py [--driver PATH] [--seconds N] [--keep DIRECTORY]
                         [operation ...]

Runs the orderbook_profile driver once per operation, attaches the macOS
`sample` profiler to it, and adds up the samples taken inside the measured
loop by the structure they were spent in and by whether they were spent
allocating or freeing memory.

macOS only: it depends on `sample` and on the format of its report.

What the numbers can and cannot say:
- A sample is one look at the call stack, one per millisecond. A few thousand
  of them give shares that are good to a couple of points, not to the digit.
- In an optimised build the compiler inlines the containers into the book's
  own functions, so most of the std::map, std::list and shared_ptr work shows
  up as "book logic". The allocation total is reliable either way, because
  malloc and free are never inlined.
- To see which structure asked for each allocation, build the driver with
  inlining disabled (-DCMAKE_CXX_FLAGS=-fno-inline in a separate build
  directory) and pass it with --driver. That build distorts every timing, so
  read only its allocation column.
"""

import re
import subprocess
import sys
import tempfile
import time
from argparse import ArgumentParser
from collections import defaultdict
from pathlib import Path

OPERATIONS = ["add", "cancel", "sweep-levels", "sweep-orders"]

# The driver puts every measured loop in a function whose name starts with this
TIMED_PREFIX = "timed"

# Seconds the driver gets to build its workload before sampling starts
WARMUP_SECONDS = 1.5

MALLOC_LIBRARY = "libsystem_malloc.dylib"

# One line of the call graph: tree drawing characters, sample count, symbol,
# then optionally the library, the offsets and the addresses.
CALL_GRAPH_LINE = re.compile(
    r"^([ +!:|]*)(\d+) (.+?)(?:  \(in ([^)]+)\))?(?:\s+\+ [\d,.]+)?(?:\s+\[[^\]]*\])?\s*$"
)

ORDERS = "orders_ (hash map by ID)"
LEVEL_TOTALS = "level totals (hash map by price)"
PRICE_LEVELS = "price levels (std::map)"
LEVEL_QUEUE = "level queue (std::list)"
ORDER_OBJECT = "Order object (shared_ptr)"
TRADE_LIST = "trade list (std::vector)"
BOOK_LOGIC = "book logic"
OTHER = "other"

HASH_PREFIXES = ("std::__hash", "std::unordered_map", "std::__unordered", "std::unique_ptr<std::__hash")
MAP_PREFIXES = ("std::__tree", "std::map", "std::__map", "void std::__tree")
LIST_PREFIXES = ("std::list", "std::__list")
SHARED_PREFIXES = ("std::shared_ptr", "std::__shared", "std::make_shared", "std::allocate_shared")


class Frame:
    """One node of the call graph: a function reached through a given path."""

    def __init__(self, depth, count, symbol, library, parent):
        self.depth = depth
        self.count = count
        self.symbol = symbol
        self.library = library
        self.parent = parent
        self.children = []

    def own_samples(self):
        """Return the samples taken in this function itself, not in what it called."""
        return self.count - sum(child.count for child in self.children)


def parse_call_graph(report_path):
    """Return every frame of the call graph in a `sample` report."""
    frames, open_frames, in_call_graph = [], [], False

    with open(report_path) as report:
        for line in report:
            if line.startswith("Call graph:"):
                in_call_graph = True
                continue
            if not in_call_graph:
                continue
            if line.startswith("Total number in stack"):
                break

            match = CALL_GRAPH_LINE.match(line.rstrip("\n"))
            if match is None:
                continue

            # The width of the tree drawing is the depth of the frame
            depth = len(match.group(1))
            while open_frames and open_frames[-1].depth >= depth:
                open_frames.pop()

            parent = open_frames[-1] if open_frames else None
            frame = Frame(depth, int(match.group(2)), match.group(3), match.group(4) or "", parent)
            if parent is not None:
                parent.children.append(frame)

            open_frames.append(frame)
            frames.append(frame)

    return frames


def structure_of(symbol):
    """Return the structure a function belongs to, or None if its name does not say."""
    if symbol.startswith(HASH_PREFIXES):
        if "OrderEntry" in symbol:
            return ORDERS
        if "LevelData" in symbol:
            return LEVEL_TOTALS
    if symbol.startswith(MAP_PREFIXES) or "__tree_" in symbol:
        return PRICE_LEVELS
    if symbol.startswith(LIST_PREFIXES):
        return LEVEL_QUEUE
    if symbol.startswith(SHARED_PREFIXES):
        return ORDER_OBJECT
    if symbol.startswith("std::") and "orderbook::Trade" in symbol:
        return TRADE_LIST
    if symbol.startswith("orderbook::OrderResult"):
        return TRADE_LIST
    if symbol.startswith("orderbook::"):
        return BOOK_LOGIC
    return None


def is_allocation(frame):
    return (
        frame.library == MALLOC_LIBRARY
        or "operator new" in frame.symbol
        or "operator delete" in frame.symbol
    )


def classify(frame, root):
    """Return (structure, is_allocating) for a frame, read from the path that leads to it."""
    structure, allocating = None, False

    # Walk up to the measured loop: the nearest caller that names a structure
    # owns the samples, and any allocator frame on the way marks them
    current = frame
    while current is not root.parent:
        if is_allocation(current):
            allocating = True
        if structure is None:
            structure = structure_of(current.symbol)
        current = current.parent

    return structure or OTHER, allocating


def find_timed_roots(frames):
    """Return the outermost frames of the measured loops."""
    roots = []
    for frame in frames:
        if not frame.symbol.startswith(TIMED_PREFIX):
            continue
        if frame.parent is not None and frame.parent.symbol.startswith(TIMED_PREFIX):
            continue
        roots.append(frame)
    return roots


def break_down(report_path):
    """Return ({structure: [working, allocating]}, total) for the samples inside the measured loops."""
    shares = defaultdict(lambda: [0, 0])
    total = 0

    for root in find_timed_roots(parse_call_graph(report_path)):
        pending = [root]
        while pending:
            frame = pending.pop()
            pending.extend(frame.children)

            samples = frame.own_samples()
            if samples <= 0:
                continue

            structure, allocating = classify(frame, root)
            shares[structure][1 if allocating else 0] += samples
            total += samples

    return shares, total


def record(driver, operation, seconds, report_path):
    """Run the driver for one operation and sample it into report_path."""
    run_seconds = WARMUP_SECONDS + seconds + 1
    process = subprocess.Popen([str(driver), operation, str(run_seconds)], stdout=subprocess.DEVNULL)

    time.sleep(WARMUP_SECONDS)
    subprocess.run(
        ["sample", str(process.pid), str(seconds), "-file", str(report_path)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=True,
    )
    process.wait()


def print_breakdown(operation, shares, total):
    allocating_total = sum(allocating for _, allocating in shares.values())

    print(f"\n{operation}: {total} samples, {100 * allocating_total / total:.1f}% allocating or freeing")
    print(f"  {'structure':<34}{'total':>8}{'working':>10}{'alloc/free':>12}")

    by_size = sorted(shares.items(), key=lambda item: -sum(item[1]))
    for structure, (working, allocating) in by_size:
        print(
            f"  {structure:<34}"
            f"{100 * (working + allocating) / total:>7.1f}%"
            f"{100 * working / total:>9.1f}%"
            f"{100 * allocating / total:>11.1f}%"
        )


def main():
    here = Path(__file__).parent
    default_driver = here.parent.parent / "build" / "benchmarks" / "orderbook_profile"

    parser = ArgumentParser(description="Profile each operation of the book and print where its time goes.")
    parser.add_argument("operations", nargs="*", choices=OPERATIONS, help="operations to profile (default: all)")
    parser.add_argument("--driver", type=Path, default=default_driver, help="path to the orderbook_profile executable")
    parser.add_argument("--seconds", type=int, default=5, help="how long to sample each operation")
    parser.add_argument("--keep", type=Path, help="directory to keep the raw sample reports in")
    args = parser.parse_args()

    if sys.platform != "darwin":
        sys.exit("error: this script needs the macOS `sample` profiler")
    if not args.driver.exists():
        sys.exit(f"error: {args.driver} not found.\nBuild it with:\n  cmake --build build --target orderbook_profile")

    with tempfile.TemporaryDirectory() as temporary_directory:
        report_directory = args.keep or Path(temporary_directory)
        report_directory.mkdir(parents=True, exist_ok=True)

        # An empty list means none were named, so profile them all
        for operation in args.operations or OPERATIONS:
            report_path = report_directory / f"{operation}.txt"
            record(args.driver, operation, args.seconds, report_path)

            shares, total = break_down(report_path)
            if total == 0:
                print(f"warning: no samples inside the measured loop of {operation}", file=sys.stderr)
                continue

            print_breakdown(operation, shares, total)


if __name__ == "__main__":
    main()
