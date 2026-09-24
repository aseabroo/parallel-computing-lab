#!/usr/bin/env python3
"""Capture every CPU repeat and a matching machine/build manifest."""
import argparse
import csv
import datetime
import json
import os
import platform
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=Path("build/cpu_lab"))
    parser.add_argument("--trials", type=int, default=1_000_000)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4])
    parser.add_argument("--output", type=Path, required=True, help="New CSV path; will not overwrite")
    args = parser.parse_args()
    if args.trials < 1 or args.seed < 0 or args.repeats < 1 or any(n < 1 for n in args.threads):
        parser.error("trials, repeats and thread counts must be positive; seed must be nonnegative")
    if len(set(args.threads)) != len(args.threads):
        parser.error("thread counts must be unique")
    manifest_path = args.output.with_suffix(".manifest.json")
    if args.output.exists() or manifest_path.exists():
        parser.error("output or manifest already exists; choose a new path")
    binary = args.binary.resolve(strict=True)
    rows = []
    for threads in args.threads:
        result = subprocess.run(
            [str(binary), str(args.trials), str(args.seed), str(threads), str(args.repeats)],
            check=True, text=True, capture_output=True,
        )
        parsed = list(csv.DictReader(result.stdout.splitlines()))
        if len(parsed) != args.repeats or any(int(row["threads"]) != threads for row in parsed):
            raise ValueError("unexpected row count or thread count")
        rows.extend(parsed)
    if len({row["hits"] for row in rows}) != 1:
        raise ValueError("hit counts differed across runs")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    manifest = {
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "command": {"trials": args.trials, "seed": args.seed, "repeats": args.repeats, "threads": args.threads},
        "binary": str(binary),
        "platform": platform.platform(),
        "processor": platform.processor(),
        "logical_cpu_count": os.cpu_count(),
        "python": platform.python_version(),
        "source_revision": subprocess.run(["git", "rev-parse", "HEAD"], text=True, capture_output=True).stdout.strip() or None,
        "note": "Timing excludes generation of this manifest and serial validation; record compiler/build flags and host load separately.",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Wrote {len(rows)} rows to {args.output} and {manifest_path}")


if __name__ == "__main__":
    main()
