"""Erzeugt Icons, Menü-Icon und Banner für Stamina.

    python tools/make_store_assets.py

Ausgabe:
  store/icon_large.png         144 x 144 (Store)
  store/icon_small.png          48 x 48  (Store)
  store/icon_80.png             80 x 80  (Pflichtfeld im Portal, RGB ohne Alpha, weißer Grund)
  store/icon_80_transparent.png 80 x 80  (Reserve mit Transparenz)
  store/banner.png             720 x 320 (Store)
  resources/images/menu_icon.png      25 x 25 (Launcher auf der Uhr, Farbe)
  resources/images/menu_icon~bw.png   25 x 25 (Launcher, Schwarz-Weiß-Uhren)

Schrift: Montserrat (SIL Open Font License), liegt in store/fonts/.
Motiv bewusst neutral (Herzschlag-Linie), siehe Pebble Program Policies.
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, 'store', 'fonts', 'Montserrat.ttf')
LOGO = os.path.join(ROOT, 'store', 'assets', 'side_effects_logo.png')
SCREENSHOT = os.path.join(ROOT, 'store', 'screenshots', 'emery', '01_session.png')

PINK = (255, 0, 85)          # GColorFolly, Akzentfarbe der App
PINK_LIGHT = (255, 122, 160)
BG_TOP = (18, 12, 24)
BG_BOTTOM = (52, 10, 34)
WHITE = (255, 255, 255)
GREY = (196, 186, 200)

# Herzschlag: x von 0..1, y von -1 (oben) .. 1 (unten)
ECG = [(0.00, 0.0), (0.24, 0.0), (0.30, -0.14), (0.36, 0.0), (0.42, 0.0), (0.46, 0.22),
       (0.52, -1.0), (0.58, 0.62), (0.63, 0.0), (0.71, -0.22), (0.79, 0.0), (1.00, 0.0)]


def gradient(w, h, top, bottom):
    img = Image.new('RGB', (w, h), top)
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)))
    return img


def ecg_points(x0, y0, w, amp):
    return [(x0 + px * w, y0 + py * amp) for px, py in ECG]


def draw_line(img, pts, color, width, glow=0):
    """Linie mit runden Enden, optional mit weichem Leuchten darunter."""
    if glow:
        layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
        ImageDraw.Draw(layer).line(pts, fill=color + (170,), width=int(width * 2.2), joint='curve')
        img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(glow)))
    d = ImageDraw.Draw(img)
    d.line(pts, fill=color, width=width, joint='curve')
    r = width / 2
    for x, y in (pts[0], pts[-1]):
        d.ellipse([x - r, y - r, x + r, y + r], fill=color)


def app_icon(size):
    s = 8  # Supersampling
    big = size * s
    img = gradient(big, big, BG_TOP, BG_BOTTOM).convert('RGBA')
    mask = Image.new('L', (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, big - 1, big - 1], radius=int(big * 0.22), fill=255)

    # Herz, leicht gekippt und etwas aus der Mitte: frech statt brav
    heart = heart_layer(int(big * 0.86), PINK, 255, -14, outline=max(1, big // 90), ss=1)
    glow = heart.filter(ImageFilter.GaussianBlur(big * 0.035))
    pos = ((big - heart.width) // 2 + int(big * 0.02), (big - heart.height) // 2 + int(big * 0.03))
    img.alpha_composite(glow, pos)
    img.alpha_composite(heart, pos)

    # Herzschlag quer durchs Herz; klein dicker, damit er lesbar bleibt
    small = size < 100
    pad = big * 0.10
    draw_line(img, ecg_points(pad, big * 0.54, big - 2 * pad, big * 0.26), WHITE,
              max(1, int(big * (0.085 if small else 0.06))))
    img.putalpha(mask)
    return img.resize((size, size), Image.LANCZOS)


def menu_icon(color):
    """Launcher: gekipptes Herz als deckende Silhouette (Pebble kennt nur deckend/transparent)."""
    heart = heart_layer(25, color, 255, -14, ss=8)
    img = Image.new('RGBA', (25, 25), (0, 0, 0, 0))
    img.alpha_composite(heart, ((25 - heart.width) // 2, (25 - heart.height) // 2 + 1))
    px = img.load()
    for y in range(25):
        for x in range(25):
            r, g, b, a = px[x, y]
            px[x, y] = color + (255,) if a >= 110 else (0, 0, 0, 0)
    return img


def heart_layer(size, color, alpha, angle, outline=0, ss=4):
    """Herz (klassische Herzkurve) als RGBA-Bild, gedreht, weich gezeichnet."""
    s = ss
    big = size * s
    pts = []
    for i in range(240):
        t = 2 * math.pi * i / 240
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((big / 2 + x * big / 36, big / 2 - y * big / 36 - big * 0.04))
    img = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon(pts, fill=color + (alpha,))
    if outline:
        d.line(pts + [pts[0]], fill=color + (min(255, alpha * 3),), width=outline * s, joint='curve')
    img = img.rotate(angle, resample=Image.BICUBIC, expand=True)
    return img.resize((img.width // s, img.height // s), Image.LANCZOS)


def hearts_background(img):
    """Großes, verspieltes Herz hinter dem Schriftzug, dazu kleine verstreute Herzen."""
    big = heart_layer(330, PINK, 52, 14, outline=3)
    glow = big.filter(ImageFilter.GaussianBlur(18))
    img.alpha_composite(glow, (150, -34))
    img.alpha_composite(big, (150, -34))
    # kleine Herzen: (x, y, Größe, Deckkraft, Drehung)
    for x, y, size, alpha, angle in ((34, 20, 26, 120, -18), (318, 248, 34, 110, 20),
                                     (408, 22, 22, 140, -8), (262, 284, 18, 150, 12),
                                     (262, 128, 16, 120, -24), (452, 272, 20, 100, 30)):
        img.alpha_composite(heart_layer(size, PINK_LIGHT, alpha, angle), (x, y))


def sideffects_logo(img):
    """SIDE effect's Logo unten links (Vorgabe: 185 px breit, volle Deckkraft).

    Weißer Hintergrund wird transparent; auf dunklem Grund werden Pulslinie,
    Schriftzug und Pfeil (rechter Teil ab 42 % der Breite) hellgrau, der
    Pac-Man samt schwarzem X bleibt unverändert.
    """
    logo = Image.open(LOGO).convert('RGBA')
    px = logo.load()
    for y in range(logo.height):
        for x in range(logo.width):
            r, g, b, a = px[x, y]
            if r > 235 and g > 235 and b > 235:
                px[x, y] = (0, 0, 0, 0)
    logo = logo.crop(logo.getbbox())
    px = logo.load()
    split = int(logo.width * 0.42)
    for y in range(logo.height):
        for x in range(split, logo.width):
            r, g, b, a = px[x, y]
            if a and r < 110 and g < 110 and b < 110:
                px[x, y] = (225, 232, 240, a)
    width = 185
    logo = logo.resize((width, round(logo.height * width / logo.width)), Image.LANCZOS)
    img.alpha_composite(logo, (30, img.height - logo.height - 8))


def font(size, weight):
    f = ImageFont.truetype(FONT, size)
    f.set_variation_by_name(weight)
    return f


def watch(screen):
    """Stilisierte Pebble Time 2 mit Screenshot."""
    sw, sh = screen.size
    bez = 14
    body_w, body_h = sw + 2 * bez + 12, sh + 2 * bez + 34
    img = Image.new('RGBA', (body_w + 10, body_h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Tasten rechts und links
    for y in (70, 150, 210):
        d.rounded_rectangle([body_w - 4, y, body_w + 6, y + 34], radius=4, fill=(70, 70, 78))
    d.rounded_rectangle([0, 110, 10, 144], radius=4, fill=(70, 70, 78))
    d.rounded_rectangle([4, 0, body_w, body_h], radius=38, fill=(44, 44, 50))
    d.rounded_rectangle([10, 6, body_w - 6, body_h - 6], radius=32, fill=(12, 12, 14))
    img.paste(screen.convert('RGBA'), (4 + bez + 6, 17 + bez))
    return img


def banner():
    w, h = 720, 320
    img = gradient(w, h, BG_TOP, BG_BOTTOM).convert('RGBA')
    hearts_background(img)

    # Herzschlag quer durchs Bild, dezent
    faint = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    draw_line(faint, ecg_points(-40, 230, w + 80, 70), PINK, 5, glow=8)
    faint.putalpha(faint.getchannel('A').point(lambda a: a * 55 // 100))
    img.alpha_composite(faint)

    d = ImageDraw.Draw(img)
    title_font = font(66, 'ExtraBold')
    d.text((40, 42), 'Stamina', font=title_font, fill=WHITE)
    d.text((42, 130), 'Your private rhythm tracker', font=font(23, 'SemiBold'), fill=PINK_LIGHT)
    d.text((42, 170), 'Heart rate · Sleep · Partner mode', font=font(17, 'Medium'), fill=GREY)
    d.text((42, 194), '100 % on your wrist · no cloud', font=font(17, 'Medium'), fill=GREY)

    # Beta-Kennzeichnung rechts neben dem Namen
    right = d.textbbox((40, 42), 'Stamina', font=title_font)[2]
    d.rounded_rectangle([right + 10, 56, right + 66, 80], radius=12, fill=PINK)
    d.text((right + 38, 68), 'BETA', font=font(15, 'Bold'), fill=WHITE, anchor='mm')

    sideffects_logo(img)

    shot = Image.open(SCREENSHOT)
    wt = watch(shot)
    img.alpha_composite(wt, (w - wt.width - 24, (h - wt.height) // 2))
    return img.convert('RGB')


def main():
    os.makedirs(os.path.join(ROOT, 'resources', 'images'), exist_ok=True)
    app_icon(144).save(os.path.join(ROOT, 'store', 'icon_large.png'))
    app_icon(48).save(os.path.join(ROOT, 'store', 'icon_small.png'))
    icon80 = app_icon(80)
    icon80.save(os.path.join(ROOT, 'store', 'icon_80_transparent.png'))
    white = Image.new('RGBA', icon80.size, WHITE + (255,))
    white.alpha_composite(icon80)
    white.convert('RGB').save(os.path.join(ROOT, 'store', 'icon_80.png'))
    menu_icon(PINK).save(os.path.join(ROOT, 'resources', 'images', 'menu_icon.png'))
    menu_icon((0, 0, 0)).save(os.path.join(ROOT, 'resources', 'images', 'menu_icon~bw.png'))
    banner().save(os.path.join(ROOT, 'store', 'banner.png'))
    print('ok')


if __name__ == '__main__':
    main()
