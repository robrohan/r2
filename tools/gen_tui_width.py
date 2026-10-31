#!/usr/bin/env python3
"""Generate the rune width tables in r2_tui.h.

Rewrites the block between the BEGIN/END GENERATED WIDTH TABLE markers in
r2_tui.h (path given as the only argument) from Python's unicodedata.

Width rules (the usual wcwidth conventions):
    0  combining and enclosing marks (Mn, Me), format characters (Cf)
       except U+00AD soft hyphen, Hangul medial/final jamo U+1160..U+11FF,
       U+200B zero width space
    2  East Asian Wide or Fullwidth (W, F); this includes the emoji that
       have emoji presentation by default
    1  everything else (including controls, which r2_tui never sends:
       they are drawn as '?')

Each range is packed into one uint32: first codepoint << 11 | (length - 1),
so a range covers at most 2048 codepoints (longer ones are split). The
checksum lets the C tests confirm that the lookup agrees with this script
for every codepoint.

Usage: python3 tools/gen_tui_width.py r2_tui.h
"""
import sys
import unicodedata

BEGIN = "/* BEGIN GENERATED WIDTH TABLE"
END = "/* END GENERATED WIDTH TABLE */"


def width(cp):
    if cp == 0x00AD:
        return 1
    if 0x1160 <= cp <= 0x11FF or cp == 0x200B:
        return 0
    ch = chr(cp)
    if unicodedata.category(ch) in ("Mn", "Me", "Cf"):
        return 0
    if unicodedata.east_asian_width(ch) in ("W", "F"):
        return 2
    return 1


def ranges(widths, want):
    out = []
    start = None
    for cp, w in enumerate(widths + [None]):
        if w == want and start is None:
            start = cp
        elif w != want and start is not None:
            lo, hi = start, cp - 1
            while lo <= hi:
                end = min(hi, lo + 2047)
                out.append((lo, end))
                lo = end + 1
            start = None
    return out


def table(name, rs):
    lines = ["    static const uint32_t %s[%d] = {" % (name, len(rs))]
    packed = ["0x%08Xu" % ((lo << 11) | (hi - lo)) for lo, hi in rs]
    for i in range(0, len(packed), 6):
        lines.append("        " + ", ".join(packed[i:i + 6]) + ",")
    lines.append("    };")
    return lines


def main():
    path = sys.argv[1]
    widths = [width(cp) for cp in range(0x110000)]
    zero = ranges(widths, 0)
    wide = ranges(widths, 2)
    # FNV-1a over (width) for every codepoint
    h = 0x811C9DC5
    for w in widths:
        h = ((h ^ w) * 0x01000193) & 0xFFFFFFFF

    block = [BEGIN + ": tools/gen_tui_width.py, Unicode %s */" % unicodedata.unidata_version]
    block += ["#define R2_TUI_WIDTH_UNICODE \"%s\"" % unicodedata.unidata_version]
    block += ["#define R2_TUI_WIDTH_CHECKSUM 0x%08Xu" % h]
    block += table("r2__tui_zero", zero)
    block += table("r2__tui_wide", wide)
    block += [END]

    src = open(path, encoding="utf-8").read()
    a = src.index(BEGIN)
    b = src.index(END) + len(END)
    src = src[:a] + "\n".join(block) + src[b:]
    open(path, "w", encoding="utf-8").write(src)
    print("%s: %d zero-width ranges, %d wide ranges, %d bytes, checksum 0x%08X"
          % (path, len(zero), len(wide), 4 * (len(zero) + len(wide)), h))


if __name__ == "__main__":
    main()
