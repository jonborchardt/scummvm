#!/usr/bin/env python3
"""Generate deterministic fixtures for the Roger overlay tests."""
import zlib, struct, os
OUT = os.path.dirname(__file__)

def write_png(path, w, h, rgba_rows):
    # rgba_rows: list of h rows, each a list of (r,g,b,a) tuples of length w
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    raw = bytearray()
    for row in rgba_rows:
        raw.append(0)  # filter: none
        for (r, g, b, a) in row:
            raw += bytes((r, g, b, a))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)  # 8-bit RGBA
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    open(path, "wb").write(png)

# 2x2 RGBA: TL red opaque, TR green opaque, BL blue opaque, BR transparent
write_png(os.path.join(OUT, "rgba_2x2.png"), 2, 2, [
    [(255, 0, 0, 255), (0, 255, 0, 255)],
    [(0, 0, 255, 255), (0, 0, 0, 0)],
])

# 8x8 clean plate: solid gray
write_png(os.path.join(OUT, "plate_8x8.png"), 8, 8,
          [[(64, 64, 64, 255)] * 8 for _ in range(8)])

print("fixtures written to", OUT)
