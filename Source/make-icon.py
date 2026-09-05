"""Preserve source RGB; mask only the exterior of the supplied circular badge."""
from pathlib import Path
import argparse
import json
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path, help="Original 640 x 640 circular badge image")
source = parser.parse_args().source
output = Path(__file__).parent / "assets"
output.mkdir(exist_ok=True)
original = Image.open(source).convert("RGB")
assert original.size == (640, 640), "Recheck the circle for another source size"
# Circle fitted to outer cream border: extremes about x=2..637, y=0..639.
# Radius 320 keeps the full border while removing the square black corners.
pixels = original.load()
alpha = Image.new("L", original.size)
mask = alpha.load()
for y in range(640):
    for x in range(640):
        distance = ((x + 0.5 - 320)**2 + (y + 0.5 - 320)**2)**0.5
        mask[x, y] = round(255 * max(0.0, min(1.0, 320.5 - distance)))
icon = original.convert("RGBA")
icon.putalpha(alpha)
icon.save(output / "icon.png")
icon.save(output / "app.ico", sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
assert icon.convert("RGB").tobytes() == original.tobytes(), "RGB changed"
assert all(icon.getpixel(p)[3] == 0 for p in [(0,0),(639,0),(0,639),(639,639)])
assert icon.getpixel((320,320))[3] == 255
with Image.open(output / "app.ico") as saved:
    sizes = sorted(saved.ico.sizes())
    assert len(sizes) == 7
report = {"source":source.name,"rgb_preserved":True,"transparent_corners":True,"opaque_center":True,"ico_sizes":sizes}
(output / "icon-verification.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(report,ensure_ascii=False))
