#!/usr/bin/env python3
"""
Generate synthetic 320x200 replacement art for a single SQ3 room.
Usage: python3 gen_sq3_room.py <pic_id> <output_dir>
Example: python3 gen_sq3_room.py 2 "J:/SteamLibrary/steamapps/common/Space Quest Collection/sq3-roger/pics/2/source"
"""
import sys, os
try:
    from PIL import Image
except ImportError:
    print("Pillow not found. Install with: pip install Pillow")
    sys.exit(1)

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <pic_id> <output_dir>")
    sys.exit(1)

pic_id = int(sys.argv[1])
out_dir = sys.argv[2]
os.makedirs(out_dir, exist_ok=True)

# Visual: solid magenta — obviously wrong, proves the hook fired
visual = Image.new("RGB", (320, 200), (255, 0, 255))
visual.save(os.path.join(out_dir, f"pic.{pic_id}.png"))

# Priority: uniform band 8 (mid-range) — sprites render on top of everything
priority = Image.new("L", (320, 200), 8)
priority.save(os.path.join(out_dir, f"pic.{pic_id}_p.png"))

# Control: uniform band 0 — entire floor walkable
control = Image.new("L", (320, 200), 0)
control.save(os.path.join(out_dir, f"pic.{pic_id}_c.png"))

print(f"Written to {out_dir}")
print(f"  pic.{pic_id}.png     (visual: magenta 320x200)")
print(f"  pic.{pic_id}_p.png   (priority: uniform 8)")
print(f"  pic.{pic_id}_c.png   (control: uniform 0)")
