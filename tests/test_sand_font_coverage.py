#!/usr/bin/env python3
"""Check the sand-timer fonts cover every on-screen character, and no extra."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COPY = ROOT / "main" / "sand_copy.h"
UI_SOURCES = (ROOT / "main" / "sand_ui.c", ROOT / "main" / "main.c")
FONT_16 = ROOT / "assets" / "fonts" / "sand_font_16.c"
FONT_32 = ROOT / "assets" / "fonts" / "sand_font_32.c"
FONT_64 = ROOT / "assets" / "fonts" / "sand_font_64.c"
FONT_64D = ROOT / "assets" / "fonts" / "sand_font_64d.c"
MISSING = 0x9F98  # 龜, intentionally outside the UI inventory


def string_literals(text: str) -> list[str]:
    return re.findall(r'"((?:\\.|[^"\\])*)"', text)


def copy_codepoints() -> set[int]:
    chars: set[int] = set()
    for literal in string_literals(COPY.read_text(encoding="utf-8")):
        chars.update(ord(ch) for ch in literal)
    return chars


def cmap_codepoints(text: str) -> set[int]:
    codes: set[int] = set()
    blocks = re.findall(
        r"\{[^{}]*?\.range_start = (\d+), \.range_length = (\d+),"
        r".*?\.unicode_list = (NULL|unicode_list_\d+),"
        r".*?\.list_length = (\d+), \.type = (LV_FONT_FMT_TXT_CMAP_\w+)",
        text,
        flags=re.S,
    )
    if not blocks:
        raise AssertionError("font cmap not found")
    for start_s, length_s, unicode_list, list_len_s, kind in blocks:
        start = int(start_s)
        length = int(length_s)
        list_len = int(list_len_s)
        if kind == "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY":
            if unicode_list != "NULL" or list_len != 0:
                raise AssertionError(f"unexpected tiny cmap {start:#x}")
            codes.update(range(start, start + length))
            continue
        if "SPARSE" not in kind:
            raise AssertionError(f"unsupported cmap {kind}")
        array = re.search(
            rf"static const uint16_t {unicode_list}\[\] = \{{([^}}]+)\}}",
            text,
        )
        if array is None:
            raise AssertionError(f"missing {unicode_list}")
        offsets = [int(item, 16) for item in re.findall(r"0x[0-9a-fA-F]+", array.group(1))]
        if len(offsets) != list_len:
            raise AssertionError(f"{unicode_list} length {len(offsets)} != {list_len}")
        codes.update(start + offset for offset in offsets)
    return codes


class SandFontCoverageTest(unittest.TestCase):
    def test_ui_sources_keep_non_ascii_in_the_copy_header(self) -> None:
        for path in UI_SOURCES:
            for literal in string_literals(path.read_text(encoding="utf-8")):
                extra = [ch for ch in literal if ord(ch) > 127]
                self.assertEqual(extra, [], f"{path.name} has an untracked string {literal!r}")

    def test_copy_is_english(self) -> None:
        text = COPY.read_text(encoding="utf-8")
        codes = copy_codepoints()
        self.assertTrue(all(cp < 128 for cp in codes))
        for word in ("SAND", "Pomodoro", "Stopwatch", "Form", "MIN", "Shape", "Color",
                     "Start", "Circle", "Square", "Triangle", "Diamond", "Hexagon", "Rectangle",
                     "Material", "Fire", "Water", "Stone", "Yuan", "Wood",
                     "Letter", "ABC",
                     "UP/DN PICK", "OK OPEN", "OK START", "UP/DN MOVE", "OK SET",
                     "HOLD OK BACK", "UP LIGHT", "DN SOUND", "Time", "Over"):
            self.assertIn(word, text)

    def test_font_16_covers_copy_ascii_and_rejects_a_missing_glyph(self) -> None:
        # Orbitron Medium has no caret. UI copy must not rely on it.
        required = copy_codepoints()
        required.update(range(0x20, 0x7F))
        required.discard(0x5E)
        covered = cmap_codepoints(FONT_16.read_text(encoding="utf-8"))
        missing = sorted(cp for cp in required if cp not in covered)
        self.assertEqual(missing, [], " ".join(f"U+{cp:04X}" for cp in missing))
        self.assertNotIn(0x5E, covered)
        self.assertNotIn(MISSING, covered)

    def test_font_32_covers_countdown_glyphs(self) -> None:
        covered = cmap_codepoints(FONT_32.read_text(encoding="utf-8"))
        needed = set(range(ord("0"), ord("9") + 1))
        needed.add(ord(":"))
        self.assertTrue(needed.issubset(covered))
        self.assertNotIn(ord("A"), covered)
        self.assertNotIn(MISSING, covered)

    def test_font_64d_covers_countdown_glyphs(self) -> None:
        covered = cmap_codepoints(FONT_64D.read_text(encoding="utf-8"))
        needed = set(range(ord("0"), ord("9") + 1))
        needed.add(ord(":"))
        self.assertTrue(needed.issubset(covered))
        self.assertNotIn(ord("A"), covered)
        self.assertNotIn(MISSING, covered)

    def test_font_64_covers_time_over(self) -> None:
        covered = cmap_codepoints(FONT_64.read_text(encoding="utf-8"))
        needed = {ord(ch) for ch in "TimeOver"}
        self.assertTrue(needed.issubset(covered))
        self.assertNotIn(MISSING, covered)


if __name__ == "__main__":
    unittest.main()
