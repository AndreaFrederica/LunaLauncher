#!/usr/bin/env python3
"""Convert an authorized MCMod JSON export to Luna's compact index format.

The export is intentionally supplied by the caller. Luna does not scrape
mcmod.cn during a normal build or at runtime.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from urllib.parse import urlparse


MCMOD_RE = re.compile(r"https://www\.mcmod\.cn/class/(\d+)\.html")
CF_RE = re.compile(r"^/(?:minecraft/)?mc-mods/([^/?#]+)")
MR_RE = re.compile(r"^/(?:mod|project)/([^/?#]+)")


def clean(value: object) -> str:
    if value is None:
        return ""
    return str(value).replace("\u200b", "").strip()


def links(record: dict, key: str) -> list[str]:
    grouped = record.get("links") or {}
    if isinstance(grouped, dict):
        grouped = grouped.get("list", grouped)
    values = grouped.get(key, []) if isinstance(grouped, dict) else []
    result = []
    for item in values if isinstance(values, list) else []:
        url = item.get("url", "") if isinstance(item, dict) else item
        if url:
            result.append(str(url))
    return result


def first_id(record: dict) -> int | None:
    for url in links(record, "mcmod"):
        match = MCMOD_RE.fullmatch(url)
        if match:
            return int(match.group(1))
    return None


def first_slug(record: dict, key: str, pattern: re.Pattern[str]) -> str:
    for url in links(record, key):
        path = urlparse(url).path if "://" in url else url
        match = pattern.match(path)
        if match:
            return match.group(1)
    return ""


def csv_field(value: str) -> str:
    # Match HMCL's CSVWriter convention (`\\c` denotes a comma).
    return value.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace(",", "\\c")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="Authorized MCMod JSON export")
    parser.add_argument("output", type=Path, help="Generated mcmod.csv")
    args = parser.parse_args()

    records = json.loads(args.input.read_text(encoding="utf-8"))
    if not isinstance(records, list):
        raise SystemExit("MCMod export must be a JSON array")

    rows: list[list[str]] = []
    skipped = 0
    for record in records:
        if not isinstance(record, dict):
            skipped += 1
            continue
        project_id = first_id(record)
        names = record.get("name") or {}
        if project_id is None or not isinstance(names, dict) or not clean(names.get("main")):
            skipped += 1
            continue
        mod_ids = record.get("modid", [])
        if isinstance(mod_ids, dict):
            mod_ids = mod_ids.get("list", [])
        if not isinstance(mod_ids, list):
            mod_ids = []
        rows.append([
            str(project_id),
            first_slug(record, "curseforge", CF_RE),
            first_slug(record, "modrinth", MR_RE),
            clean(names.get("main")),
            clean(names.get("sub")),
            clean(names.get("abbr")),
            ";".join(clean(item) for item in mod_ids if clean(item)),
        ])

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as output:
        output.write("# Luna Launcher MCMod index; generated from an authorized export.\n")
        output.write("# mcmod.cn; Copyright (C) MCMod.cn. All Rights Reserved.\n")
        output.write("# Data attribution and terms: see MC-MOD-LICENSE.txt.\n")
        output.write("# Fields: mcmod_id, curseforge_id, modrinth_id, main_name, secondary_name, abbreviation, mod_ids\n")
        for row in rows:
            output.write(",".join(csv_field(item) for item in row) + "\n")
    print(f"generated {len(rows)} records; skipped {skipped}")


if __name__ == "__main__":
    main()
