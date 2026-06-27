#!/usr/bin/env python3
"""Generate deterministic fixtures for the Roger overlay tests."""
import zlib, struct, json, os
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

def dump_json(obj, name):
    # Emit pretty JSON with a trailing newline (POSIX text-file convention).
    with open(os.path.join(OUT, name), "w") as f:
        f.write(json.dumps(obj, indent=2) + "\n")

# 2x2 RGBA: TL red opaque, TR green opaque, BL blue opaque, BR transparent
write_png(os.path.join(OUT, "rgba_2x2.png"), 2, 2, [
    [(255, 0, 0, 255), (0, 255, 0, 255)],
    [(0, 0, 255, 255), (0, 0, 0, 0)],
])

# 4x2 "spritesheet": two 2x2 cels side by side (cel0 left red, cel1 right green)
write_png(os.path.join(OUT, "view.900.loop.0.png"), 4, 2, [
    [(255, 0, 0, 255), (255, 0, 0, 255), (0, 255, 0, 255), (0, 255, 0, 255)],
    [(255, 0, 0, 255), (255, 0, 0, 255), (0, 255, 0, 255), (0, 255, 0, 255)],
])
dump_json({
    "frames": {
        "0": {"frame": {"x": 0, "y": 0, "w": 2, "h": 2}},
        "1": {"frame": {"x": 2, "y": 0, "w": 2, "h": 2}},
    },
    "animations": {"loop": ["0", "1"]},
    "meta": {"image": "view.900.loop.0.png"},
}, "view.900.loop.0.json")

# 8x8 clean plate: solid gray; one 4x4 "slice" piece (white) at (4,0)
write_png(os.path.join(OUT, "plate_8x8.png"), 8, 8,
          [[(64, 64, 64, 255)] * 8 for _ in range(8)])
write_png(os.path.join(OUT, "color_ffffff.png"), 4, 4,
          [[(255, 255, 255, 255)] * 4 for _ in range(4)])
dump_json({
    "source": "plate_8x8.png", "width": 8, "height": 8,
    "pieces": [{"filename": "color_ffffff.png", "color": "#ffffff",
                "x": 4, "y": 0, "width": 4, "height": 4}],
}, "slice_manifest.json")

# View cache expects <base>/<id>/view.<id>.loop.<n>.{png,json}
import shutil
vdir = os.path.join(OUT, "900")
os.makedirs(vdir, exist_ok=True)
shutil.copy(os.path.join(OUT, "view.900.loop.0.png"), vdir)
shutil.copy(os.path.join(OUT, "view.900.loop.0.json"), vdir)

# Malformed view 901: valid JSON but missing "frames" key — tests null-safety guard.
write_png(os.path.join(OUT, "view.901.loop.0.png"), 2, 2, [
    [(255, 0, 0, 255), (0, 255, 0, 255)],
    [(0, 0, 255, 255), (0, 0, 0, 0)],
])
dump_json({
    "animations": {"loop": ["0"]},
    "meta": {"image": "view.901.loop.0.png"},
}, "view.901.loop.0.json")
vdir901 = os.path.join(OUT, "901")
os.makedirs(vdir901, exist_ok=True)
shutil.copy(os.path.join(OUT, "view.901.loop.0.png"), vdir901)
shutil.copy(os.path.join(OUT, "view.901.loop.0.json"), vdir901)

print("fixtures written to", OUT)
