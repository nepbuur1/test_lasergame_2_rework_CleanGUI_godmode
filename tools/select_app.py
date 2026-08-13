#!/usr/bin/env python3
"""Kies welke .ino main/main.cpp bouwt.

Precies één `#include <....ino>` mag actief zijn; de rest staat uitgecommentarieerd.
Met de hand omzetten gaat een keer of tien goed en dan laat je er twee aan staan,
waarna de linker over dubbele symbolen valt en je gaat zoeken in de verkeerde hoek.

    python3 tools/select_app.py                       # welke staat er nu aan?
    python3 tools/select_app.py --list                # alle keuzes
    python3 tools/select_app.py ShowEnableSemantics   # zet die aan, de rest uit

De naam mag met of zonder `.ino`, en hoofdletterongevoelig.
"""

import argparse
import os
import re
import sys

MAIN_CPP = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "..", "main", "main.cpp")

# Alleen de regels met een .ino in punthaken of aanhalingstekens. De andere
# includes in dit bestand (headers, benchmarks) blijven met rust.
LINE = re.compile(r'^(\s*)(//)?\s*(#include\s*[<"]([^>"]+\.ino)[>"])(.*)$')


def parse(path):
    entries = []
    with open(path) as f:
        for number, line in enumerate(f):
            m = LINE.match(line.rstrip("\n"))
            if m:
                entries.append({
                    "number": number,
                    "indent": m.group(1),
                    "active": m.group(2) is None,
                    "include": m.group(3),
                    "name": os.path.basename(m.group(4))[:-len(".ino")],
                    "trailer": m.group(5),
                })
    return entries


def render(entry, active):
    prefix = "" if active else "//"
    return f'{entry["indent"]}{prefix}{entry["include"]}{entry["trailer"]}\n'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("name", nargs="?")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    path = os.path.normpath(MAIN_CPP)
    entries = parse(path)
    active = [e for e in entries if e["active"]]

    if args.list:
        for e in entries:
            print(f'{"*" if e["active"] else " "} {e["name"]}')
        return 0

    if args.name is None:
        if len(active) == 1:
            print(active[0]["name"])
            return 0
        print(f"{len(active)} actieve includes: "
              f"{', '.join(e['name'] for e in active)}", file=sys.stderr)
        return 1

    wanted = args.name
    if wanted.lower().endswith(".ino"):
        wanted = wanted[:-len(".ino")]

    matches = [e for e in entries if e["name"].lower() == wanted.lower()]
    if not matches:
        print(f"'{args.name}' staat niet in main.cpp. Bekend zijn:",
              file=sys.stderr)
        for e in entries:
            print(f"  {e['name']}", file=sys.stderr)
        return 1
    if len(matches) > 1:
        print(f"'{args.name}' komt {len(matches)} keer voor in main.cpp",
              file=sys.stderr)
        return 1

    target = matches[0]
    with open(path) as f:
        lines = f.readlines()
    for e in entries:
        lines[e["number"]] = render(e, e is target)
    with open(path, "w") as f:
        f.writelines(lines)

    print(f'{target["name"]} staat aan'
          f'{"" if not active else ", was " + ", ".join(e["name"] for e in active)}')
    return 0


if __name__ == "__main__":
    sys.exit(main())
