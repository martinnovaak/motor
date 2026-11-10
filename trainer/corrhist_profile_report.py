#!/usr/bin/env python3
"""Merge per-history corrhist similarity profiles."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


FIELDS = (
    "lookup_cold", "lookup_exact", "lookup_collision", "lookup_unavailable",
    "update_cold", "update_exact", "update_collision", "update_unavailable",
)


def percentage(value: int, total: int) -> float:
    return 100.0 * value / total if total else 0.0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profiles", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    merged: dict[str, dict[str, int]] = {}
    for path in args.profiles:
        report = json.loads(path.read_text())
        for name, values in report["tables"].items():
            destination = merged.setdefault(name, {field: 0 for field in FIELDS})
            for field in FIELDS:
                destination[field] += int(values[field])

    result: dict[str, object] = {"profile_files": len(args.profiles), "tables": {}}
    print(
        f"{'corrhist':22s} {'lookup exact':>13s} {'collision':>11s} "
        f"{'cold':>9s} {'update exact':>13s}"
    )
    for name, values in merged.items():
        lookup_total = values["lookup_cold"] + values["lookup_exact"] + values["lookup_collision"]
        update_total = values["update_cold"] + values["update_exact"] + values["update_collision"]
        row = {
            **values,
            "lookup_total": lookup_total,
            "lookup_exact_percent": percentage(values["lookup_exact"], lookup_total),
            "lookup_collision_percent": percentage(values["lookup_collision"], lookup_total),
            "lookup_cold_percent": percentage(values["lookup_cold"], lookup_total),
            "update_total": update_total,
            "update_exact_percent": percentage(values["update_exact"], update_total),
            "update_collision_percent": percentage(values["update_collision"], update_total),
            "update_cold_percent": percentage(values["update_cold"], update_total),
        }
        result["tables"][name] = row
        print(
            f"{name:22s} {row['lookup_exact_percent']:12.3f}% "
            f"{row['lookup_collision_percent']:10.3f}% "
            f"{row['lookup_cold_percent']:8.3f}% "
            f"{row['update_exact_percent']:12.3f}%"
        )

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n")
        print(f"report={args.output}")


if __name__ == "__main__":
    main()
