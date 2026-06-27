#!/usr/bin/env python3
"""Generate deterministic test fixture PNGs for Roger unit tests."""
from PIL import Image
import os

out = os.path.dirname(os.path.abspath(__file__))

# 4x4 grayscale, every pixel = 5
img = Image.new("L", (4, 4), 5)
img.save(os.path.join(out, "4x4_p5.png"))

# 4x4 grayscale, known pixel values:
# (0,0)=3  (1,0)=7  (2,0)=0  (3,0)=15
# (0,1)=1  (1,1)=14 (2,1)=9  (3,1)=2
# (0,2)=0  (1,2)=8  (2,2)=6  (3,2)=4
# (0,3)=11 (1,3)=5  (2,3)=13 (3,3)=12
img2 = Image.new("L", (4, 4))
pixels = [3,7,0,15, 1,14,9,2, 0,8,6,4, 11,5,13,12]
img2.putdata(pixels)
img2.save(os.path.join(out, "4x4_gradient.png"))

print("Fixtures written to", out)
