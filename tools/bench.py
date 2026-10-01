#!/usr/bin/env python3
"""Бенчмарк-стенд NanoWeb (ARCHITECTURE.md, раздел 11).

Запускает `nanoweb-con.exe --bench ...`, разбирает строки вида
    bench <имя> iterations=<N> ns=<всего наносекунд>
и дописывает результаты в tests/bench/results.csv вместе с коммитом и датой.
M0: только «noop»; этапы parse/vis/layout/find появятся вместе с модулями.
"""
import argparse
import csv
import datetime
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "build" / "nanoweb-con.exe"
RESULTS = ROOT / "tests" / "bench" / "results.csv"
LINE = re.compile(r"bench (\S+) iterations=(\d+) ns=(\d+)")

CASES = [
    ["--bench", "noop", "100000000"],
]


def launcher():
    if sys.platform == "win32":
        return []
    wine = shutil.which("wine")
    if not wine:
        sys.exit("bench: нужен Windows или Wine")
    return [wine]


def git_commit() -> str:
    try:
        return subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                              capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-record", action="store_true", help="только напечатать, не писать в CSV")
    args = parser.parse_args()

    rows = []
    for case in CASES:
        proc = subprocess.run(launcher() + [str(EXE), *case], capture_output=True, text=True)
        if proc.returncode != 0:
            print(proc.stderr, file=sys.stderr)
            return 1
        for match in LINE.finditer(proc.stdout):
            name, iterations, ns = match.group(1), int(match.group(2)), int(match.group(3))
            per_iter = ns / iterations if iterations else 0.0
            print(f"{name:12} {iterations:>12} iterations  {ns / 1e6:10.3f} ms  {per_iter:8.3f} ns/iter")
            rows.append([datetime.date.today().isoformat(), git_commit(), name, iterations, ns])

    if not args.no_record and rows:
        RESULTS.parent.mkdir(parents=True, exist_ok=True)
        new = not RESULTS.exists()
        with RESULTS.open("a", newline="") as f:
            writer = csv.writer(f)
            if new:
                writer.writerow(["date", "commit", "bench", "iterations", "ns"])
            writer.writerows(rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
