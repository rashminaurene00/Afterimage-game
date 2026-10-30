"""
AFTERIMAGE - turn Blender renders into game-ready pixel-art sprites.

    python tools/pixelate_sprites.py assets_src/renders assets/sprites

For every PNG in the input folder:
  1. crop to the visible pixels and anchor the object to the bottom-centre,
  2. downscale with area averaging to 16 px wide (24 px tall for tall sprites),
  3. hard-threshold alpha (no soft edges - pixel art has none),
  4. snap every colour to the game palette (nearest colour),
  5. add a 1 px dark outline so the silhouette reads in the dark.

Requires Pillow:  pip install pillow
"""
import os
import sys

from PIL import Image

# Game palette (src/Config.h) plus the wood/fabric shades the props need.
PALETTE = [
    (8, 6, 12), (14, 10, 20), (34, 22, 48), (62, 40, 84), (96, 74, 118), (150, 126, 170),
    (88, 86, 92), (128, 122, 118), (190, 184, 176), (38, 56, 44), (92, 120, 96),
    (150, 180, 140), (150, 24, 36), (210, 40, 52), (214, 204, 180),
    # props
    (30, 20, 24), (46, 32, 36), (64, 44, 48), (80, 58, 62), (50, 36, 32), (70, 52, 46),
    (92, 76, 58), (116, 98, 74), (62, 50, 38), (66, 50, 58), (92, 72, 80), (170, 150, 100),
    (58, 62, 68), (84, 40, 52), (60, 70, 62), (80, 70, 96), (110, 100, 86), (70, 52, 90),
    (24, 16, 30),
]
TALL = {"wardrobe", "tenant"}
OUTLINE = (12, 8, 14, 230)


def nearest(c):
    r, g, b = c
    best, bd = PALETTE[0], 1 << 30
    for p in PALETTE:
        d = (r - p[0]) ** 2 * 3 + (g - p[1]) ** 2 * 4 + (b - p[2]) ** 2 * 2
        if d < bd:
            best, bd = p, d
    return best


def convert(src_path, dst_path, key):
    im = Image.open(src_path).convert("RGBA")
    bbox = im.getchannel("A").point(lambda a: 255 if a > 40 else 0).getbbox()
    if bbox:
        im = im.crop(bbox)
    tw, th = (16, 24) if key in TALL else (16, 16)
    # Fit inside the target (keep aspect), anchored bottom-centre, 1 px margin for outline.
    scale = min((tw - 2) / im.width, (th - 1) / im.height)
    nw, nh = max(1, round(im.width * scale)), max(1, round(im.height * scale))
    small = im.resize((nw, nh), Image.BOX)
    canvas = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
    canvas.paste(small, ((tw - nw) // 2, th - nh - 0))
    px = canvas.load()
    for y in range(th):
        for x in range(tw):
            r, g, b, a = px[x, y]
            px[x, y] = nearest((r, g, b)) + (255,) if a >= 110 else (0, 0, 0, 0)
    out = canvas.copy()
    opx = out.load()
    for y in range(th):
        for x in range(tw):
            if px[x, y][3]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < tw and 0 <= ny < th and px[nx, ny][3]:
                    opx[x, y] = OUTLINE
                    break
    out.save(dst_path)
    print("[pixelate] %s -> %s (%dx%d)" % (os.path.basename(src_path), dst_path, tw, th))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    n = 0
    for name in sorted(os.listdir(src)):
        if name.lower().endswith(".png"):
            key = os.path.splitext(name)[0]
            convert(os.path.join(src, name), os.path.join(dst, key + ".png"), key)
            n += 1
    print("[pixelate] converted %d sprite(s)" % n)


if __name__ == "__main__":
    main()
