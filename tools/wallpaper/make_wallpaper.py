"""Generates assets/wallpapers/wallpaper-4/image.png: an original abstract
blue wallpaper (radial gradient + soft light ribbons) for the Moderno theme."""
import math, sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

W, H = 1920, 1080
out = sys.argv[1] if len(sys.argv) > 1 else "image.png"

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

# radial gradient background
bg = Image.new("RGB", (W, H))
px = bg.load()
cx, cy = W * 0.62, H * 0.42
maxd = math.hypot(W, H) * 0.62
inner, mid, outer = (43, 132, 230), (16, 74, 160), (5, 20, 52)
for y in range(H):
    for x in range(0, W):
        d = min(1.0, math.hypot(x - cx, (y - cy) * 1.25) / maxd)
        px[x, y] = lerp(inner, mid, d / 0.45) if d < 0.45 else lerp(mid, outer, (d - 0.45) / 0.55)

# light ribbons: bands along sine curves, blurred and screened on top
ribbons = Image.new("RGB", (W, H), (0, 0, 0))
draw = ImageDraw.Draw(ribbons)
specs = [  # (amplitude, phase, y offset, thickness, color)
    (90, 0.0, 0.58, 46, (60, 160, 255)),
    (120, 1.1, 0.64, 30, (120, 200, 255)),
    (70, 2.3, 0.70, 22, (80, 180, 255)),
    (150, 0.6, 0.52, 14, (180, 225, 255)),
    (60, 3.0, 0.76, 60, (30, 110, 220)),
]
for amp, phase, yoff, thick, color in specs:
    pts = []
    for i in range(0, W + 40, 8):
        t = i / W
        y = H * yoff + amp * math.sin(t * math.pi * 1.6 + phase) - 260 * (t - 0.5) ** 2 * 2
        pts.append((i, y))
    for k in range(thick):
        shade = 1 - abs(k - thick / 2) / (thick / 2)
        c = tuple(int(v * (0.25 + 0.75 * shade) * 0.55) for v in color)
        draw.line([(x, y + k) for x, y in pts], fill=c, width=2)
ribbons = ribbons.filter(ImageFilter.GaussianBlur(5))
glow = ribbons.filter(ImageFilter.GaussianBlur(40))
img = ImageChops.screen(bg, glow)
img = ImageChops.screen(img, ribbons)

# soft vignette
vig = Image.new("L", (W, H), 0)
ImageDraw.Draw(vig).ellipse((-W * 0.25, -H * 0.35, W * 1.25, H * 1.35), fill=255)
vig = vig.filter(ImageFilter.GaussianBlur(160))
img = Image.composite(img, Image.new("RGB", (W, H), (3, 12, 32)), vig)

img = img.resize((1280, 720), Image.LANCZOS)
img.save(out, optimize=True)
print("saved", out, img.size)
