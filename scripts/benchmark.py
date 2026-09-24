#!/usr/bin/env python3
"""Capture every CPU repeat and a matching machine/build manifest."""
import argparse
import csv
import datetime
import hashlib
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
    # Rotate the order so drift in host load is less confounded with thread count.
    for round_index in range(args.repeats):
        schedule = args.threads[round_index % len(args.threads):] + args.threads[:round_index % len(args.threads)]
        for threads in schedule:
            result = subprocess.run(
                [str(binary), str(args.trials), str(args.seed), str(threads), "1"],
                check=True, text=True, capture_output=True,
            )
            parsed = list(csv.DictReader(result.stdout.splitlines()))
            if len(parsed) != 1 or int(parsed[0]["threads"]) != threads:
                raise ValueError("unexpected row count or thread count")
            parsed[0]["repeat"] = str(round_index)
            parsed[0]["launch_index"] = str(len(rows))
            rows.extend(parsed)
    if len({row["hits"] for row in rows}) != 1:
        raise ValueError("hit counts differed across runs")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    repo = Path(__file__).resolve().parents[1]
    git = lambda *args: subprocess.run(["git", "-C", str(repo), *args], text=True, capture_output=True, check=True).stdout.strip()
    manifest = {
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "command": {"trials": args.trials, "seed": args.seed, "repeats": args.repeats, "threads": args.threads},
        "binary": str(binary),
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "platform": platform.platform(),
        "processor": platform.processor(),
        "logical_cpu_count": os.cpu_count(),
        "python": platform.python_version(),
        "source_revision": git("rev-parse", "HEAD"),
        "source_dirty": bool(git("status", "--porcelain")),
        "schedule": "cyclic rotation of thread counts across rounds; CSV launch_index is chronological",
        "note": "Timing excludes generation of this manifest and serial validation; record compiler/build flags, CPU quota and host load separately.",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Wrote {len(rows)} rows to {args.output} and {manifest_path}")


if __name__ == "__main__":
    main()
