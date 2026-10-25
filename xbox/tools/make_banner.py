# Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
# PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
# Draws the GitHub page banner (docs/xbox/banner.png): the emblem, the name and a green terminal glow.
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
X = os.path.join(HERE, "..")
OUT = os.path.join(X, "..", "docs", "xbox")
GREEN = (124, 255, 107)
DIM = (95, 168, 87)
BACK = (11, 18, 11)
FONT = "C:/Windows/Fonts/bahnschrift.ttf"


def font(size, weight="Bold"):
    f = ImageFont.truetype(FONT, size)
    try:
        f.set_variation_by_name(weight)
    except Exception:
        pass
    return f


W, H = 1600, 520
img = Image.new("RGBA", (W, H), BACK + (255,))
glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(glow).ellipse([W * 0.12, -H * 0.3, W * 0.88, H * 1.3], fill=(50, 130, 42, 110))
img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(140)))
d = ImageDraw.Draw(img)
for y in range(0, H, 4):
    d.line([(0, y), (W, y)], fill=(0, 0, 0, 34))
d.rounded_rectangle([14, 14, W - 15, H - 15], 26, outline=(46, 90, 46, 255), width=3)

emblem = Image.open(os.path.join(X, "setup", "icon.png")).convert("RGBA").resize((300, 300), Image.LANCZOS)
img.alpha_composite(emblem, (120, (H - 300) // 2))

text = Image.new("RGBA", (W, H), (0, 0, 0, 0))
t = ImageDraw.Draw(text)
t.text((480, 150), "FALLOUT 1in2", font=font(124), fill=GREEN)
t.text((486, 292), "for Xbox", font=font(64, "SemiBold"), fill=GREEN)
t.text((488, 384), "Fallout 1 on the Fallout 2 engine  ·  native  ·  made for the controller", font=font(30, "SemiBold"), fill=DIM)
img.alpha_composite(text.filter(ImageFilter.GaussianBlur(6)))
img.alpha_composite(text)

os.makedirs(OUT, exist_ok=True)
img.convert("RGB").save(os.path.join(OUT, "banner.png"), optimize=True)
print("banner written")
