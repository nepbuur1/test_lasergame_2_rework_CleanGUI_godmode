#!/usr/bin/env python3
"""Converts every image in an images tree that changed since the previous run.

Usage:

    python3 tools/allImg2rgb565.py [<images folder>]

Without an argument it takes the current folder, which must be an "images"
folder or live inside one.

What "changed" means: the file is newer than the timestamp in
allImg2rgb565_last_called.txt (which sits in the images root), OR its header in
_rgb565/ does not exist. That second condition matters more than it looks -
without it, throwing away _rgb565/ or doing a fresh checkout would leave every
source "older than the last run", and nothing would be regenerated.

The timestamp that gets written is the moment the run *started*, not the moment
it finished. An image that is edited while the run is busy would otherwise fall
exactly in the gap between the two and never be picked up again. Now it is
converted once too often at worst, and never missed.

If anything fails, the timestamp is not written at all, so the next run tries
again.

Headers whose source has disappeared are reported but not deleted; pass --prune
to remove them.

by Marius Versteegen, 2026
"""

import sys

from venv_bootstrap import ensure_venv
ensure_venv()			# must happen before PIL is imported

import argparse
import os
import time

from img2rgb565 import (
    ConversionError, IMAGES_ROOT_NAME, IMAGE_EXTENSIONS, INDEX_HEADER_NAME,
    OUTPUT_DIR_NAME, convert, find_images_root, flatten, regenerate_index)

TIMESTAMP_FILE_NAME = "allImg2rgb565_last_called.txt"

SKIPPED_DIR_NAMES = (OUTPUT_DIR_NAME, "venv", "__pycache__", "build")


def resolve_images_root(folder):
    """Accepts the images folder itself, or anything inside it."""

    folder = os.path.abspath(folder)
    if not os.path.isdir(folder):
        raise ConversionError("'%s' is not a folder." % folder)

    if os.path.basename(folder) == IMAGES_ROOT_NAME:
        return folder

    # find_images_root() works on a file path, so hand it a pretend file.
    return find_images_root(os.path.join(folder, "x"))


def read_timestamp(images_root):
    """Returns the epoch seconds of the previous run, or None."""

    path = os.path.join(images_root, TIMESTAMP_FILE_NAME)
    if not os.path.isfile(path):
        return None

    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                try:
                    # Epoch seconds rather than a readable date: mtimes are epoch
                    # seconds too, so no timezone or daylight saving can creep in.
                    return float(line.split()[0])
                except ValueError:
                    break

    print("warning: '%s' is unreadable; converting everything." % path)
    return None


def write_timestamp(images_root, when):
    path = os.path.join(images_root, TIMESTAMP_FILE_NAME)
    readable = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(when))
    with open(path, "w", newline="\n") as f:
        f.write("# Written by tools/allImg2rgb565.py: the start of its last successful run.\n")
        f.write("# Epoch seconds, followed by the same moment for human eyes.\n")
        f.write("%.6f  # %s\n" % (when, readable))


def find_sources(images_root):
    sources = []
    for (folder, dir_names, file_names) in os.walk(images_root):
        dir_names[:] = [d for d in dir_names
                        if (d not in SKIPPED_DIR_NAMES) and not d.startswith(".")]
        for name in sorted(file_names):
            if name.lower().endswith(IMAGE_EXTENSIONS):
                sources.append(os.path.join(folder, name))
    return sorted(sources)


def find_orphans(images_root, sources):
    """Generated headers whose source image is gone."""

    out_dir = os.path.join(images_root, OUTPUT_DIR_NAME)
    if not os.path.isdir(out_dir):
        return []

    expected = {flatten(images_root, path) + ".h" for path in sources}
    return sorted(os.path.join(out_dir, name) for name in os.listdir(out_dir)
                  if name.endswith(".h") and (name != INDEX_HEADER_NAME) and (name not in expected))


def main():
    parser = argparse.ArgumentParser(
        description="Converts every changed image of an images tree to RGB565.")
    parser.add_argument("folder", nargs="?", default=".",
                        help="the images folder (default: the current folder)")
    parser.add_argument("--all", action="store_true",
                        help="convert everything, regardless of the timestamp")
    parser.add_argument("--prune", action="store_true",
                        help="delete generated headers whose source image is gone")
    args = parser.parse_args()

    started = time.time()

    try:
        images_root = resolve_images_root(args.folder)
        print("images root: %s" % images_root)

        last_called = None if args.all else read_timestamp(images_root)
        if last_called is None:
            print("no usable timestamp: converting everything.")

        sources = find_sources(images_root)
        out_dir = os.path.join(images_root, OUTPUT_DIR_NAME)

        nof_converted = 0
        for path in sources:
            header = os.path.join(out_dir, flatten(images_root, path) + ".h")

            if os.path.isfile(header) and (last_called is not None) and (os.path.getmtime(path) <= last_called):
                continue

            convert(path)
            nof_converted += 1

        orphans = find_orphans(images_root, sources)
        for path in orphans:
            if args.prune:
                os.remove(path)
                print("pruned %s" % os.path.basename(path))
            else:
                print("warning: '%s' has no source image any more. Pass --prune to delete it."
                      % os.path.basename(path))

        # Always, even when nothing was converted: a header that was deleted or
        # dropped in by hand would otherwise leave _allImages.h out of step with
        # the folder, and that only shows up as a compile error much later.
        regenerate_index(images_root)

        print("%d images found, %d converted, %d unchanged."
              % (len(sources), nof_converted, len(sources) - nof_converted))

    except ConversionError as error:
        sys.exit("error: %s (the timestamp was left alone, so the next run retries)" % error)

    write_timestamp(images_root, started)
    return 0


if __name__ == "__main__":
    sys.exit(main())
