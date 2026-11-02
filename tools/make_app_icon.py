"""Generates the provisional app icon (concentric rings around a listener point).
Usage: python3 tools/make_app_icon.py <output.png> [size]"""
import math
import sys

from PIL import Image, ImageDraw, ImageFilter


def make_icon(size: int) -> Image.Image:
    img = Image.new("RGB", (size, size), (12, 13, 20))
    draw = ImageDraw.Draw(img)
    c = size / 2
    colours = [(115, 158, 255), (89, 209, 189), (189, 140, 255)]
    for i, r in enumerate([0.40, 0.29, 0.18]):
        rad = r * size
        w = max(2, int(size * 0.018))
        draw.ellipse([c - rad, c - rad, c + rad, c + rad], outline=colours[i % 3], width=w)
    # A few "sound objects" placed on the rings.
    for ang, r, col in [(-0.6, 0.40, colours[0]), (2.2, 0.29, colours[1]), (4.0, 0.18, colours[2])]:
        x, y = c + math.cos(ang) * r * size, c + math.sin(ang) * r * size
        d = size * 0.035
        draw.ellipse([x - d, y - d, x + d, y + d], fill=col)
    d = size * 0.03
    draw.ellipse([c - d, c - d, c + d, c + d], fill=(235, 236, 245))
    return img.filter(ImageFilter.SMOOTH)


if __name__ == "__main__":
    out = sys.argv[1]
    size = int(sys.argv[2]) if len(sys.argv) > 2 else 1024
    make_icon(size).save(out)
