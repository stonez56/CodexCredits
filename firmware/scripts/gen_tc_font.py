"""產生 20x20 粗體繁體中文點陣字型標頭檔 (PROGMEM)，使用微軟正黑體粗體 (msjhbd.ttc)。"""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = r"C:\Windows\Fonts\msjhbd.ttc"
OUT_PATH = Path(__file__).resolve().parent.parent / "src" / "tc_font.h"
GLYPH_W = 20
GLYPH_H = 20
BYTES_PER_ROW = (GLYPH_W + 7) // 8  # 3 bytes/row -> 60 bytes/glyph

CHARS = "".join(dict.fromkeys(
    "新竹台北中高雄"
    "日一二三四五六"
    "用量上限重設可用次數"
    "小時限制每週剩餘購買點數倒數已購限額排程紀錄尚無後"
    "體感濕度風速等待氣象同步預報天分夜間休眠額度"
))


def render_glyph(font: ImageFont.FreeTypeFont, ch: str) -> bytes:
    img = Image.new("1", (GLYPH_W, GLYPH_H), 0)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), ch, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    ox = (GLYPH_W - w) // 2 - bbox[0]
    oy = (GLYPH_H - h) // 2 - bbox[1]
    draw.text((ox, oy), ch, font=font, fill=1)

    rows = []
    for y in range(GLYPH_H):
        for b in range(BYTES_PER_ROW):
            byte_val = 0
            for bit in range(8):
                x = b * 8 + bit
                if x < GLYPH_W and img.getpixel((x, y)):
                    byte_val |= (1 << (7 - bit))
            rows.append(byte_val)
    return bytes(rows)


def main() -> None:
    font = ImageFont.truetype(FONT_PATH, 19)
    lines = [
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f"static constexpr int TC_GLYPH_W = {GLYPH_W};",
        f"static constexpr int TC_GLYPH_H = {GLYPH_H};",
        "",
        "struct TcGlyph {",
        "  uint32_t codepoint;",
        f"  uint8_t bitmap[{BYTES_PER_ROW * GLYPH_H}]; // 20x20 MSB-first",
        "};",
        "",
        f"static const size_t TC_FONT_COUNT = {len(CHARS)};",
        "static const TcGlyph TC_FONT_GLYPHS[] PROGMEM = {",
    ]
    for ch in CHARS:
        cp = ord(ch)
        bm = render_glyph(font, ch)
        hex_bytes = ", ".join(f"0x{b:02X}" for b in bm)
        lines.append(f"  {{ 0x{cp:04X}, {{ {hex_bytes} }} }}, // {ch}")
    lines.append("};")
    OUT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Generated {len(CHARS)} 20x20 glyphs -> {OUT_PATH}")


if __name__ == "__main__":
    main()
