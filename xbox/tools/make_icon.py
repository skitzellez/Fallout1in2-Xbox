# Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
# PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
# Draws the Fallout 1in2 for Xbox emblem (green terminal screen, "1in2") and writes the setup icon
# (xbox/setup/icon.png + icon.ico) and the Xbox app's tile pictures (xbox/Assets).
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
F = os.path.join(HERE, "..")
GREEN = (124, 255, 107)
BACK = (11, 18, 11)
FONT = "C:/Windows/Fonts/bahnschrift.ttf"


def font(size, weight="Bold"):
    f = ImageFont.truetype(FONT, size)
    try:
        f.set_variation_by_name(weight)
    except Exception:
        pass
    return f


def emblem(size):
    s = size * 4  # drawn large, scaled down smooth
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = s // 7
    d.rounded_rectangle([0, 0, s - 1, s - 1], r, fill=BACK)
    # screen glow
    glow = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([s * 0.1, s * 0.1, s * 0.9, s * 0.9], fill=(60, 150, 50, 90))
    glow = glow.filter(ImageFilter.GaussianBlur(s * 0.12))
    img.alpha_composite(glow)
    d = ImageDraw.Draw(img)
    # scan lines
    for y in range(0, s, max(4, s // 64)):
        d.line([(s * 0.06, y), (s * 0.94, y)], fill=(0, 0, 0, 28), width=max(1, s // 256))
    # frame
    w = max(2, s // 40)
    d.rounded_rectangle([w, w, s - 1 - w, s - 1 - w], r - w, outline=GREEN, width=w)
    # text with a soft glow
    text = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    td = ImageDraw.Draw(text)
    big = font(int(s * 0.36))
    small = font(int(s * 0.12), "SemiBold")
    td.text((s / 2, s * 0.50), "1in2", font=big, fill=GREEN, anchor="mm")
    td.text((s / 2, s * 0.78), "FALLOUT", font=small, fill=GREEN, anchor="mm")
    halo = text.filter(ImageFilter.GaussianBlur(s * 0.02))
    img.alpha_composite(halo)
    img.alpha_composite(text)
    return img.resize((size, size), Image.LANCZOS)


def tile(width, height, wide_text=False):
    img = Image.new("RGBA", (width, height), BACK + (255,))
    side = int(min(width, height) * (0.8 if not wide_text else 0.7))
    e = emblem(side)
    if wide_text:
        img.alpha_composite(e, (int(height * 0.15), (height - side) // 2))
        d = ImageDraw.Draw(img)
        x = int(height * 0.15) + side + int(height * 0.12)
        d.text((x, height * 0.42), "FALLOUT 1in2", font=font(int(height * 0.17)), fill=GREEN, anchor="lm")
        d.text((x, height * 0.62), "for Xbox", font=font(int(height * 0.11), "SemiBold"), fill=(95, 168, 87), anchor="lm")
    else:
        img.alpha_composite(e, ((width - side) // 2, (height - side) // 2))
    return img


os.makedirs(os.path.join(F, "setup"), exist_ok=True)
big = emblem(256)
big.save(os.path.join(F, "setup", "icon.png"))
big.save(os.path.join(F, "setup", "icon.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
assets = os.path.join(F, "Assets")
os.makedirs(assets, exist_ok=True)
emblem(150).save(os.path.join(assets, "Square150x150Logo.png"))
emblem(44).save(os.path.join(assets, "Square44x44Logo.png"))
emblem(50).save(os.path.join(assets, "StoreLogo.png"))
tile(310, 150, True).save(os.path.join(assets, "Wide310x150Logo.png"))
tile(620, 300, True).save(os.path.join(assets, "SplashScreen.png"))
print("icons written")
