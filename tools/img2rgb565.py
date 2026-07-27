#!/usr/bin/env python3
"""Converts an image to an RGB565 header that CleanGUI can blit straight from flash.

Usage (from anywhere; the paths may be relative or absolute):

    python3 tools/img2rgb565.py level1/characters/speler.png

The image must live somewhere under a folder named "images". That folder is the
root: the path below it is flattened into the output name, so that two pictures
with the same file name in different subfolders never collide.

    <root>/level1/characters/speler.png
        -> <root>/_rgb565/level1_characters_speler.h

Parameters are appended to the file name, each starting with a '-':

    -trcolor_RRGGBB   the colour that is to act as transparent.
                      If the source has an alpha channel, every pixel with
                      alpha < 128 is written as this colour. If it has no alpha
                      channel, the conversion is the same as without the
                      parameter and the colour is merely passed on to the engine
                      that blits the image.
    -ssh_RxC          the image is a spritesheet of R rows by C columns.
                      Only recorded for now; nothing acts on it yet.

    speler-trcolor_FF00FF-ssh_10x5.png
        -> _rgb565/level1_characters_speler-trcolor_FF00FF-ssh_10x5.h

The parameters stay in the header file name (which keeps it unique and
self-documenting) but are left out of the C++ identifiers inside it; there they
end up as constructor arguments of the Image:

    arLevel1CharactersSpelerPixels[]   in level1_characters_speler-....h
    imageLevel1CharactersSpeler        in _rgb565/_allImages.h

Because '-' separates parameters, it cannot be part of an image name.

by Marius Versteegen, 2026
"""

import sys

from venv_bootstrap import ensure_venv
ensure_venv()			# must happen before PIL is imported

import argparse
import os
import re

from PIL import Image as PilImage

IMAGES_ROOT_NAME = "images"
OUTPUT_DIR_NAME = "_rgb565"
PAL8_DIR_NAME = "_pal8"
INDEX_HEADER_NAME = "_allImages.h"
PALETTE_HEADER_NAME = "_palette.h"
PALETTE_ARRAY_NAME = "arPalette"

# Index 0 is de transparante, altijd. Dat is een afspraak tussen deze tool en
# crt_Image.h, en het scheelt een kleurvergelijking per pixel: doorzichtig zijn
# is nu "index is nul".
TRANSPARENT_INDEX = 0
NOF_PALETTE_COLOURS = 255		# 255 echte kleuren, plus index 0

SKIPPED_DIR_NAMES = (OUTPUT_DIR_NAME, PAL8_DIR_NAME, "venv", "__pycache__", "build")
IMAGE_EXTENSIONS = (".png", ".jpg", ".jpeg", ".bmp", ".gif")

# The line that carries everything _allImages.h needs to know about a header.
# It is written into every generated header and parsed back out of it, so that
# the index can be rebuilt from the generated tree alone - no separate bookkeeping
# file that can go stale or get lost.
META_PREFIX = "// crt-img-meta:"
META_RE = re.compile(
    r"^// crt-img-meta:"
    r" image=(?P<image>\w+)"
    r" array=(?P<array>\w+)"
    r" width=(?P<width>\d+)"
    r" height=(?P<height>\d+)"
    r" nofRows=(?P<nofRows>\d+)"
    r" nofCols=(?P<nofCols>\d+)"
    r" trcolor=(?P<trcolor>none|0x[0-9A-Fa-f]{8})"
    r"(?: format=(?P<format>RGB565|PAL8))?$"		# ontbreekt in oudere headers: dan RGB565
)

VALUES_PER_LINE = 12


class ConversionError(Exception):
    """Anything the user can fix by renaming or replacing a file."""


# --- naming ----------------------------------------------------------------

def find_images_root(path):
    """Returns the nearest ancestor folder of path that is named "images"."""

    current = os.path.dirname(os.path.abspath(path))
    while True:
        if os.path.basename(current) == IMAGES_ROOT_NAME:
            return current

        parent = os.path.dirname(current)
        if parent == current:		# hit the filesystem root
            raise ConversionError(
                "'%s' does not live under a folder named '%s'. The images of an "
                "app or example belong in such a folder; it is what the output "
                "names are relative to." % (path, IMAGES_ROOT_NAME))
        current = parent


def flatten(images_root, src_path):
    """<root>/level1/characters/speler.png -> "level1_characters_speler" """

    rel = os.path.relpath(os.path.abspath(src_path), images_root)
    stem = os.path.splitext(rel)[0]
    return stem.replace(os.sep, "_").replace("/", "_")


def parse_params(flat_stem):
    """Splits "a_b-trcolor_FF00FF-ssh_10x5" into its name and its parameters.

    Returns (base, colTransparentARGB or None, nofRows, nofCols).
    """

    parts = flat_stem.split("-")
    base = parts[0]
    col_transparent = None
    nof_rows = 1
    nof_cols = 1

    if not base:
        raise ConversionError("'%s': the name before the first '-' is empty." % flat_stem)

    for param in parts[1:]:
        match = re.fullmatch(r"trcolor_([0-9A-Fa-f]{6})", param)
        if match:
            if col_transparent is not None:
                raise ConversionError("'%s': -trcolor given more than once." % flat_stem)
            col_transparent = int(match.group(1), 16)	# 0x00RRGGBB: alpha byte stays 0
            continue

        match = re.fullmatch(r"ssh_(\d+)x(\d+)", param)
        if match:
            nof_rows = int(match.group(1))
            nof_cols = int(match.group(2))
            if (nof_rows < 1) or (nof_cols < 1):
                raise ConversionError("'%s': -ssh needs at least 1 row and 1 column." % flat_stem)
            continue

        raise ConversionError(
            "'%s': '-%s' is not a known parameter. Expected -trcolor_RRGGBB or "
            "-ssh_RxC. Mind that '-' separates parameters, so it cannot be part "
            "of an image name." % (flat_stem, param))

    return base, col_transparent, nof_rows, nof_cols


def to_identifier(base):
    """ "level1_characters_speler" -> "Level1CharactersSpeler" """

    words = [w for w in re.split(r"[^0-9A-Za-z]+", base) if w]
    if not words:
        raise ConversionError("'%s' yields no usable C++ identifier." % base)

    ident = "".join(w[0].upper() + w[1:] for w in words)
    if ident[0].isdigit():
        ident = "_" + ident		# an identifier may not start with a digit
    return ident


def find_sources(images_root):
    """Alle bronplaatjes onder images_root, in een vaste volgorde."""

    sources = []
    for (folder, dir_names, file_names) in os.walk(images_root):
        dir_names[:] = [d for d in dir_names
                        if (d not in SKIPPED_DIR_NAMES) and not d.startswith(".")]
        for name in sorted(file_names):
            if name.lower().endswith(IMAGE_EXTENSIONS):
                sources.append(os.path.join(folder, name))
    return sorted(sources)


# --- conversion ------------------------------------------------------------

def to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def read_pixels(src_path, col_transparent):
    """Returns (width, height, [rgb565, ...], nof_pixels_made_transparent)."""

    with PilImage.open(src_path) as img:
        img.load()
        has_alpha = (img.mode in ("RGBA", "LA")) or ((img.mode == "P") and ("transparency" in img.info))

        # tobytes() rather than getdata(): one flat buffer instead of a tuple per
        # pixel, which for a full screen saves both time and a lot of garbage.
        if has_alpha and (col_transparent is not None):
            converted = img.convert("RGBA")
            nof_channels = 4
        else:
            if has_alpha:
                print("  warning: '%s' has an alpha channel but no -trcolor parameter; "
                      "the transparency is dropped." % os.path.basename(src_path))
            converted = img.convert("RGB")
            nof_channels = 3

        width, height = converted.size
        raw = converted.tobytes()

    if col_transparent is None:
        col565_transparent = None
    else:
        col565_transparent = to_rgb565((col_transparent >> 16) & 0xFF,
                                       (col_transparent >> 8) & 0xFF,
                                       col_transparent & 0xFF)

    pixels = []
    nof_transparent = 0
    nof_collisions = 0

    for i in range(0, len(raw), nof_channels):
        r = raw[i]
        g = raw[i + 1]
        b = raw[i + 2]
        a = raw[i + 3] if (nof_channels == 4) else 255

        if (col565_transparent is not None) and (a < 128):
            pixels.append(col565_transparent)
            nof_transparent += 1
        else:
            col565 = to_rgb565(r, g, b)
            if col565 == col565_transparent:
                nof_collisions += 1
            pixels.append(col565)

    if nof_collisions > 0:
        print("  warning: %d opaque pixels happen to have exactly the transparent "
              "colour, and will be skipped when blitted." % nof_collisions)

    return width, height, pixels, nof_transparent


def format_array(name, width, height, pixels):
    lines = []
    for y in range(height):
        row = pixels[y * width:(y + 1) * width]
        for i in range(0, len(row), VALUES_PER_LINE):
            chunk = row[i:i + VALUES_PER_LINE]
            lines.append("\t\t" + ", ".join("0x%04X" % p for p in chunk) + ",")

    return ("\tstatic constexpr uint16_t %s[%d] =\n\t{\n%s\n\t};\n"
            % (name, width * height, "\n".join(lines)))


def convert(src_path, verbose=True):
    """Converts one image and rewrites nothing but its own header.

    Returns the path of the header that was written. Call regenerate_index()
    afterwards - convert_and_index() does both.
    """

    images_root = find_images_root(src_path)
    flat_stem = flatten(images_root, src_path)
    base, col_transparent, nof_rows, nof_cols = parse_params(flat_stem)

    ident = to_identifier(base)
    array_name = "ar%sPixels" % ident
    image_name = "image%s" % ident

    width, height, pixels, nof_transparent = read_pixels(src_path, col_transparent)

    if (nof_rows > 1) and (height % nof_rows != 0):
        print("  warning: height %d is not a multiple of the %d spritesheet rows."
              % (height, nof_rows))
    if (nof_cols > 1) and (width % nof_cols != 0):
        print("  warning: width %d is not a multiple of the %d spritesheet columns."
              % (width, nof_cols))

    out_dir = os.path.join(images_root, OUTPUT_DIR_NAME)
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, flat_stem + ".h")

    meta = "%s image=%s array=%s width=%d height=%d nofRows=%d nofCols=%d trcolor=%s" % (
        META_PREFIX, image_name, array_name, width, height, nof_rows, nof_cols,
        "none" if col_transparent is None else "0x%08X" % col_transparent)

    header = (
        "// Generated by tools/img2rgb565.py - do not edit.\n"
        "// Source: %s\n"
        "//\n"
        "// %d x %d pixels, RGB565 = %d bytes. The array is \"const\", so it lives in\n"
        "// .rodata (flash): it costs no DRAM and is never copied - the DisplayAdapter\n"
        "// blits straight out of it.\n"
        "//\n"
        "// The matching Image object is declared in %s.\n"
        "%s\n"
        "\n"
        "#pragma once\n"
        "#include <stdint.h>\n"
        "\n"
        "namespace crt\n"
        "{\n"
        "%s"
        "};\n"
        % (os.path.relpath(os.path.abspath(src_path), images_root).replace(os.sep, "/"),
           width, height, width * height * 2, INDEX_HEADER_NAME, meta,
           format_array(array_name, width, height, pixels)))

    with open(out_path, "w", newline="\n") as f:
        f.write(header)

    if verbose:
        note = ""
        if col_transparent is not None:
            note += ", %d transparent pixels" % nof_transparent
        if (nof_rows > 1) or (nof_cols > 1):
            note += ", spritesheet %dx%d" % (nof_rows, nof_cols)
        print("  %s -> %s (%d x %d, %d bytes%s)"
              % (os.path.relpath(os.path.abspath(src_path), images_root).replace(os.sep, "/"),
                 os.path.basename(out_path), width, height, width * height * 2, note))

    return out_path


# --- the index header ------------------------------------------------------

def read_meta(header_path):
    """Digs the crt-img-meta line out of a generated header."""

    with open(header_path, "r") as f:
        for line in f:
            if line.startswith(META_PREFIX):
                match = META_RE.match(line.rstrip("\n"))
                if not match:
                    raise ConversionError(
                        "'%s' has a damaged crt-img-meta line. Regenerate it with "
                        "img2rgb565.py, or delete it." % header_path)
                return match.groupdict()

    raise ConversionError(
        "'%s' has no crt-img-meta line, so it was not generated by img2rgb565.py. "
        "Only generated headers belong in %s/." % (header_path, OUTPUT_DIR_NAME))


def regenerate_index(images_root):
    """Rewrites <root>/_rgb565/_allImages.h from the headers next to it."""

    out_dir = os.path.join(images_root, OUTPUT_DIR_NAME)
    if not os.path.isdir(out_dir):
        return None

    headers = sorted(name for name in os.listdir(out_dir)
                     if name.endswith(".h") and (name != INDEX_HEADER_NAME))

    entries = [(name, read_meta(os.path.join(out_dir, name))) for name in headers]

    includes = "".join('#include "%s"\n' % name for (name, _) in entries)

    declarations = []
    for (name, meta) in entries:
        args = [meta["array"], meta["width"], meta["height"]]
        if meta["trcolor"] != "none":
            args.append(meta["trcolor"])
        if (meta["nofRows"] != "1") or (meta["nofCols"] != "1"):
            args.append(meta["nofRows"])
            args.append(meta["nofCols"])
        declarations.append("\tconstexpr Image %s(%s);\n" % (meta["image"], ", ".join(args)))

    pointers = "".join("\t\t&%s,\n" % meta["image"] for (_, meta) in entries)
    names = "".join('\t\t"%s",\n' % meta["image"] for (_, meta) in entries)

    body = (
        "// Generated by tools/img2rgb565.py - do not edit.\n"
        "//\n"
        "// One Image per generated pixel header. Everything that is not pixels -\n"
        "// the size, the transparent colour, the spritesheet layout - is passed to\n"
        "// the constructor here, which keeps the pixel headers free of it.\n"
        "//\n"
        "// Include this file; not the individual pixel headers.\n"
        "\n"
        "#pragma once\n"
        "#include <crt_Image.h>\n"
        "\n"
        "%s"
        "\n"
        "namespace crt\n"
        "{\n"
        "%s"
        "\n"
        "\t// All of the above, in the order of their file names, so that a viewer or\n"
        "\t// a test can walk them without naming each one.\n"
        "\tstatic constexpr int nofAllImages = %d;\n"
        "\n"
        "\tstatic constexpr const Image* const arAllImages[] =\n"
        "\t{\n"
        "%s"
        "\t};\n"
        "\n"
        "\tstatic constexpr const char* const arAllImageNames[] =\n"
        "\t{\n"
        "%s"
        "\t};\n"
        "};\n"
        % (includes, "".join(declarations), len(entries),
           pointers if entries else "\t\tnullptr,\n",
           names if entries else '\t\t"",\n'))

    index_path = os.path.join(out_dir, INDEX_HEADER_NAME)
    with open(index_path, "w", newline="\n") as f:
        f.write(body)

    return index_path


def convert_and_index(src_path, verbose=True):
    header = convert(src_path, verbose)
    regenerate_index(find_images_root(src_path))
    return header



# --- PAL8 ------------------------------------------------------------------
#
# Een paletted plaatje is pas snel als het plaatje en het scherm hetzelfde palet
# gebruiken: dan zijn de bytes van het plaatje al schermindices en is blitten
# niet meer dan geheugen kopieren. Daarom is dit een operatie over de hele
# images-boom en niet over een los bestand: het palet volgt uit alle plaatjes
# samen, en verandert het, dan moeten ze allemaal opnieuw.


def build_palette(sources, verbose=True):
    """Leidt een gedeeld palet af uit alle plaatjes samen.

    Geeft (pal_img, palette565) terug. palette565 heeft 256 ingangen, waarvan
    index 0 de transparante is.
    """

    pixels = bytearray()
    for path in sources:
        # Alleen bij een plaatje dat om transparantie vraagt tellen de doorzichtige
        # pixels niet mee. Vraagt het er niet om, dan worden ze straks gewoon
        # getekend (meestal als zwart) en horen ze dus in het palet thuis.
        _, col_transparent, _, _ = parse_params(flatten(find_images_root(path), path))

        with PilImage.open(path) as img:
            img.load()
            raw = img.convert("RGBA").tobytes()

        for i in range(0, len(raw), 4):
            if (col_transparent is not None) and (raw[i + 3] < 128):
                continue
            pixels += raw[i:i + 3]

    if not pixels:
        raise ConversionError("geen enkele zichtbare pixel gevonden; valt er wel iets om te kwantiseren?")

    nof_pixels = len(pixels) // 3
    montage = PilImage.frombytes("RGB", (nof_pixels, 1), bytes(pixels))

    # Geen dithering: op een sprite van 64x64 maakt dat er ruis van, en het
    # breekt juist de vlakke kleurvlakken waar deze aanpak zijn snelheid aan
    # ontleent.
    pal_img = montage.quantize(colors=NOF_PALETTE_COLOURS, method=PilImage.Quantize.MEDIANCUT)

    raw_palette = pal_img.getpalette()
    nof_colours = min(NOF_PALETTE_COLOURS, len(raw_palette) // 3)

    # Index 0 is transparant en wordt nooit getekend. Toch een opvallende kleur,
    # zodat een fout meteen als magenta in beeld staat en niet als iets dat op
    # echte beeldinhoud lijkt.
    palette565 = [to_rgb565(0xFF, 0x00, 0xFF)]
    for i in range(nof_colours):
        palette565.append(to_rgb565(raw_palette[3 * i], raw_palette[3 * i + 1], raw_palette[3 * i + 2]))
    palette565 += [0] * (256 - len(palette565))

    if verbose:
        print("  palet: %d kleuren uit %d zichtbare pixels van %d plaatjes"
              % (nof_colours, nof_pixels, len(sources)))

    return pal_img, palette565[:256]


def read_indices(src_path, pal_img, col_transparent):
    """Geeft (width, height, indices, b_transparent, nof_transparent)."""

    with PilImage.open(src_path) as img:
        img.load()
        has_alpha = (img.mode in ("RGBA", "LA")) or ((img.mode == "P") and ("transparency" in img.info))
        rgba = img.convert("RGBA")
        width, height = rgba.size
        raw = rgba.tobytes()
        quantised = rgba.convert("RGB").quantize(palette=pal_img, dither=PilImage.Dither.NONE)
        mapped = quantised.tobytes()

    # Dezelfde afspraak als bij RGB565: alleen een -trcolor-parameter maakt een
    # plaatje transparant. Een alpha-kanaal zonder die parameter wordt weggegooid,
    # zodat dezelfde bestandsnamen in beide formaten hetzelfde betekenen.
    b_transparent = (col_transparent is not None)

    if has_alpha and not b_transparent:
        print("  warning: '%s' heeft een alpha-kanaal maar geen -trcolor-parameter; "
              "de transparantie vervalt." % os.path.basename(src_path))

    col_r = (col_transparent >> 16) & 0xFF if col_transparent is not None else 0
    col_g = (col_transparent >> 8) & 0xFF if col_transparent is not None else 0
    col_b = col_transparent & 0xFF if col_transparent is not None else 0

    indices = bytearray(len(mapped))
    nof_transparent = 0

    for i in range(len(mapped)):
        b_pixel_transparent = False
        if has_alpha and b_transparent:
            b_pixel_transparent = (raw[4 * i + 3] < 128)
        elif col_transparent is not None:
            # Geen alpha-kanaal: dan geldt de opgegeven kleur zelf als
            # transparant, zoals bij de RGB565-weg.
            b_pixel_transparent = ((raw[4 * i] == col_r) and (raw[4 * i + 1] == col_g)
                                   and (raw[4 * i + 2] == col_b))

        if b_pixel_transparent:
            indices[i] = TRANSPARENT_INDEX
            nof_transparent += 1
        else:
            indices[i] = mapped[i] + 1		# palet-index 0 is voor transparant

    return width, height, indices, b_transparent, nof_transparent


def format_indices(name, width, height, indices):
    lines = []
    for y in range(height):
        row = indices[y * width:(y + 1) * width]
        for i in range(0, len(row), 16):
            chunk = row[i:i + 16]
            lines.append("\t\t" + ", ".join("0x%02X" % v for v in chunk) + ",")

    return ("\tstatic constexpr uint8_t %s[%d] =\n\t{\n%s\n\t};\n"
            % (name, width * height, "\n".join(lines)))


def convert_pal8(src_path, pal_img, verbose=True):
    """Zet een plaatje om naar indices in het gedeelde palet."""

    images_root = find_images_root(src_path)
    flat_stem = flatten(images_root, src_path)
    base, col_transparent, nof_rows, nof_cols = parse_params(flat_stem)

    ident = to_identifier(base)
    array_name = "ar%sIndices" % ident
    image_name = "image%s" % ident

    width, height, indices, b_transparent, nof_transparent = read_indices(src_path, pal_img, col_transparent)

    out_dir = os.path.join(images_root, PAL8_DIR_NAME)
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, flat_stem + ".h")

    meta = "%s image=%s array=%s width=%d height=%d nofRows=%d nofCols=%d trcolor=%s format=PAL8" % (
        META_PREFIX, image_name, array_name, width, height, nof_rows, nof_cols,
        "0x01000000" if b_transparent else "none")		# 0x01000000 = paletteIndex(0)

    header = (
        "// Generated by tools/img2rgb565.py --pal8 - do not edit.\n"
        "// Source: %s\n"
        "//\n"
        "// %d x %d pixels, een byte per pixel: een index in het gedeelde palet van\n"
        "// %s. De array is \"const\", dus hij staat in .rodata (flash) en kost geen\n"
        "// DRAM. Omdat het scherm hetzelfde palet gebruikt, is blitten niet meer dan\n"
        "// deze bytes kopieren - geen omzetting per pixel.\n"
        "//\n"
        "// Het bijbehorende Image-object staat in %s.\n"
        "%s\n"
        "\n"
        "#pragma once\n"
        "#include <stdint.h>\n"
        "\n"
        "namespace crt\n"
        "{\n"
        "%s"
        "};\n"
        % (os.path.relpath(os.path.abspath(src_path), images_root).replace(os.sep, "/"),
           width, height, PALETTE_HEADER_NAME, INDEX_HEADER_NAME, meta,
           format_indices(array_name, width, height, indices)))

    with open(out_path, "w", newline="\n") as f:
        f.write(header)

    if verbose:
        note = ""
        if b_transparent:
            note += ", %d transparant" % nof_transparent
        if (nof_rows > 1) or (nof_cols > 1):
            note += ", spritesheet %dx%d" % (nof_rows, nof_cols)
        print("  %s -> %s (%d x %d, %d bytes%s)"
              % (os.path.relpath(os.path.abspath(src_path), images_root).replace(os.sep, "/"),
                 os.path.basename(out_path), width, height, width * height, note))

    return out_path


def write_palette_header(images_root, palette565):
    out_dir = os.path.join(images_root, PAL8_DIR_NAME)
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, PALETTE_HEADER_NAME)

    lines = []
    for i in range(0, 256, 8):
        lines.append("\t\t" + ", ".join("0x%04X" % c for c in palette565[i:i + 8]) + ",")

    body = (
        "// Generated by tools/img2rgb565.py --pal8 - do not edit.\n"
        "//\n"
        "// Het palet dat alle plaatjes in deze map delen, en dat ook het scherm\n"
        "// gebruikt zolang de PAL8-adapter actief is. Juist omdat het gedeeld is,\n"
        "// zijn de bytes van een plaatje meteen schermindices en hoeft er bij het\n"
        "// blitten niets omgerekend te worden.\n"
        "//\n"
        "// Index 0 is de transparante. Hij staat hier op magenta, zodat een fout\n"
        "// opvalt in plaats van op beeldinhoud te lijken.\n"
        "\n"
        "#pragma once\n"
        "#include <stdint.h>\n"
        "\n"
        "namespace crt\n"
        "{\n"
        "\tstatic constexpr uint16_t %s[256] =\n"
        "\t{\n"
        "%s\n"
        "\t};\n"
        "};\n"
        % (PALETTE_ARRAY_NAME, "\n".join(lines)))

    with open(path, "w", newline="\n") as f:
        f.write(body)

    return path


def regenerate_index_pal8(images_root):
    """Herschrijft <root>/_pal8/_allImages.h uit de headers die ernaast staan."""

    out_dir = os.path.join(images_root, PAL8_DIR_NAME)
    if not os.path.isdir(out_dir):
        return None

    headers = sorted(name for name in os.listdir(out_dir)
                     if name.endswith(".h") and (name not in (INDEX_HEADER_NAME, PALETTE_HEADER_NAME)))

    entries = [(name, read_meta(os.path.join(out_dir, name))) for name in headers]

    includes = "".join('#include "%s"\n' % name for (name, _) in entries)

    declarations = []
    for (name, meta) in entries:
        args = [meta["array"], meta["width"], meta["height"], PALETTE_ARRAY_NAME]
        b_sheet = (meta["nofRows"] != "1") or (meta["nofCols"] != "1")
        b_transparent = (meta["trcolor"] != "none")

        if b_transparent or b_sheet:
            args.append("true" if b_transparent else "false")
        if b_sheet:
            args.append(meta["nofRows"])
            args.append(meta["nofCols"])

        declarations.append("\tconstexpr Image %s(%s);\n" % (meta["image"], ", ".join(args)))

    pointers = "".join("\t\t&%s,\n" % meta["image"] for (_, meta) in entries)
    names = "".join('\t\t"%s",\n' % meta["image"] for (_, meta) in entries)

    body = (
        "// Generated by tools/img2rgb565.py --pal8 - do not edit.\n"
        "//\n"
        "// Een Image per gegenereerde header, allemaal wijzend naar hetzelfde palet.\n"
        "// Includeer dit bestand; niet de losse headers.\n"
        "\n"
        "#pragma once\n"
        "#include <crt_Image.h>\n"
        "\n"
        '#include "%s"\n'
        "%s"
        "\n"
        "namespace crt\n"
        "{\n"
        "%s"
        "\n"
        "\t// Alles hierboven, op volgorde van bestandsnaam, zodat een viewer of een\n"
        "\t// test ze kan aflopen zonder ze bij naam te noemen.\n"
        "\tstatic constexpr int nofAllImages = %d;\n"
        "\n"
        "\tstatic constexpr const Image* const arAllImages[] =\n"
        "\t{\n"
        "%s"
        "\t};\n"
        "\n"
        "\tstatic constexpr const char* const arAllImageNames[] =\n"
        "\t{\n"
        "%s"
        "\t};\n"
        "};\n"
        % (PALETTE_HEADER_NAME, includes, "".join(declarations), len(entries),
           pointers if entries else "\t\tnullptr,\n",
           names if entries else '\t\t"",\n'))

    index_path = os.path.join(out_dir, INDEX_HEADER_NAME)
    with open(index_path, "w", newline="\n") as f:
        f.write(body)

    return index_path


def convert_tree_pal8(images_root, verbose=True):
    """Palet afleiden en de hele boom omzetten. Alles of niets, met opzet."""

    sources = find_sources(images_root)
    if not sources:
        raise ConversionError("geen plaatjes gevonden onder '%s'." % images_root)

    pal_img, palette565 = build_palette(sources, verbose)

    for path in sources:
        convert_pal8(path, pal_img, verbose)

    # Weesheaders van plaatjes die er niet meer zijn, zouden met een verkeerd
    # palet blijven staan. Weg ermee: het palet is toch net opnieuw bepaald.
    out_dir = os.path.join(images_root, PAL8_DIR_NAME)
    expected = {flatten(images_root, path) + ".h" for path in sources}
    for name in os.listdir(out_dir):
        if name.endswith(".h") and (name not in (INDEX_HEADER_NAME, PALETTE_HEADER_NAME)) and (name not in expected):
            os.remove(os.path.join(out_dir, name))
            if verbose:
                print("  verwijderd (bron bestaat niet meer): %s" % name)

    write_palette_header(images_root, palette565)
    regenerate_index_pal8(images_root)

    return len(sources)


# --- main ------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Converts images to RGB565 headers for CleanGUI.")
    parser.add_argument("images", nargs="+", help="the image files to convert")
    parser.add_argument("--pal8", action="store_true",
                        help="zet om naar palet-indices in plaats van RGB565. Dit geldt "
                             "altijd de hele images-boom van het genoemde bestand: het "
                             "palet volgt uit alle plaatjes samen.")
    args = parser.parse_args()

    roots = set()
    try:
        if args.pal8:
            for path in args.images:
                if not os.path.isfile(path):
                    raise ConversionError("'%s' does not exist." % path)
                roots.add(find_images_root(path))

            for root in sorted(roots):
                print("PAL8, hele boom: %s" % root)
                nof = convert_tree_pal8(root)
                print("%d plaatjes omgezet, palet en index herschreven." % nof)
            return 0

        for path in args.images:
            if not os.path.isfile(path):
                raise ConversionError("'%s' does not exist." % path)
            if not path.lower().endswith(IMAGE_EXTENSIONS):
                raise ConversionError(
                    "'%s' has no image extension %s." % (path, ", ".join(IMAGE_EXTENSIONS)))

            print("converting %s" % path)
            convert(path)
            roots.add(find_images_root(path))

        for root in sorted(roots):
            index = regenerate_index(root)
            print("index: %s" % index)

    except ConversionError as error:
        sys.exit("error: %s" % error)

    return 0


if __name__ == "__main__":
    sys.exit(main())
