#!/usr/bin/env python3
"""Generate the PWA home-screen icons for the web view from the 80x80 logo.

    python3 tools/make_pwa_icons.py

Writes homeserver/public/icon-{192,512}.png. The source is pixel art, so it is
upscaled NEAREST — any smoothing would turn crisp pixels into mush. The crab is
inset to ~60% so Android's maskable-icon crop (which can round or squircle the
outer ~20%) never clips a leg.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "logo_80.png"
OUT = ROOT / "homeserver" / "public"
BG = (0, 0, 0, 255)          # THEME_BG — matches the page and the AMOLED panel
INSET = 0.60                 # logo box as a fraction of the icon, for maskable safe zone


MASTER = 512


def main():
    logo = Image.open(SRC).convert("RGBA")
    OUT.mkdir(parents=True, exist_ok=True)

    # Build the master at a whole multiple of 80 so every source pixel gets the
    # same number of output pixels — a non-integer NEAREST upscale gives some
    # rows 1px and some 2px, which reads as wonky pixel art.
    scale = max(1, round(MASTER * INSET / logo.width))
    box = logo.width * scale
    big = logo.resize((box, box), Image.NEAREST)

    master = Image.new("RGBA", (MASTER, MASTER), BG)
    master.paste(big, ((MASTER - box) // 2, (MASTER - box) // 2), big)

    for size in (192, 512):
        # Smaller sizes come from downscaling the master, not from a fresh
        # upscale: at 192 no integer multiple of 80 lands near the safe zone
        # (80px = 42%, 160px = 83%), so the ratio has to come from the master.
        img = master if size == MASTER else master.resize((size, size), Image.LANCZOS)
        path = OUT / f"icon-{size}.png"
        img.save(path)
        print(f"{path.relative_to(ROOT)}  {size}x{size}  "
              f"logo {round(box * size / MASTER)}px ({box / MASTER:.0%})")


if __name__ == "__main__":
    main()
