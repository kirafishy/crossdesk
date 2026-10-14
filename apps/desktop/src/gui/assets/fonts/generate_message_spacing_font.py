#!/usr/bin/env python3
"""Regenerate message_spacing.ttf with fontTools (only needed for regeneration).

Slint 1.17 pins line height to the requested font's ascent, descent and leading,
but StyledText does not expose line-height. This metrics-only font supplies 1.5em
line boxes without replacing any visible glyphs: Slint's normal system fallback
fonts still render the text and links. Use this family for message and release-note
bodies without changing the typography of other controls.
"""

from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen


def main():
    font = FontBuilder(1000, isTTF=True)
    glyphs = [".notdef", "zeroWidthSpace"]
    font.setupGlyphOrder(glyphs)
    font.setupCharacterMap({0x200B: "zeroWidthSpace"})
    font.setupGlyf({name: TTGlyphPen(None).glyph() for name in glyphs})
    font.setupHorizontalMetrics({name: (0, 0) for name in glyphs})
    font.setupHorizontalHeader(ascent=1000, descent=-250, lineGap=250)
    font.setupOS2(
        version=4,
        sTypoAscender=1000,
        sTypoDescender=-250,
        sTypoLineGap=250,
        usWinAscent=1000,
        usWinDescent=250,
        fsSelection=0xC0,  # Regular, use typographic metrics.
    )
    font.setupNameTable({
        "familyName": "CrossDesk Message Spacing",
        "styleName": "Regular",
        "uniqueFontIdentifier": "CrossDesk-Message-Spacing-1.0",
        "fullName": "CrossDesk Message Spacing",
        "psName": "CrossDeskMessageSpacing-Regular",
        "version": "Version 1.0",
        "copyright": "Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.",
    })
    font.setupPost()
    # Fixed timestamps keep the generated asset reproducible.
    font.font["head"].created = font.font["head"].modified = 3874867200
    font.font.recalcTimestamp = False
    font.save(Path(__file__).with_name("message_spacing.ttf"))


if __name__ == "__main__":
    main()
