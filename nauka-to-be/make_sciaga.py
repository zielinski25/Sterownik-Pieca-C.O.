#!/usr/bin/env python3
"""Generuje ściągę jako PNG (idealną do wysłania na Telegramie jako zdjęcie).

Wymaga: pip install pillow qrcode
Efekt:  sciaga-to-be.png oraz ../docs/sciaga-to-be.png (podgląd linku / GitHub Pages)
"""
from pathlib import Path
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
    import qrcode
except ImportError:
    sys.exit("Brak bibliotek: zainstaluj `python3 -m pip install pillow qrcode`")

HERE = Path(__file__).parent
URL = ("https://cdn.jsdelivr.net/gh/zielinski25/Sterownik-Pieca-C.O."
       "@arena%2Ff423a839-sterownik-pieca-c-o/docs/index.html")

FONT_DIR = Path("/usr/share/fonts/truetype/dejavu")
def font(name, size):
    return ImageFont.truetype(str(FONT_DIR / name), size)
F_BOLD = lambda s: font("DejaVuSans-Bold.ttf", s)
F_REG = lambda s: font("DejaVuSans.ttf", s)
F_MONO = lambda s: font("DejaVuSansMono.ttf", s)

TEAL = (18, 179, 166); TEAL_D = (11, 143, 134); TEAL_L = (226, 247, 244)
NAVY = (18, 48, 73); CORAL = (255, 122, 89); SUN = (255, 201, 60); MUTED = (92, 115, 144)
WHITE = (255, 255, 255); BG = (243, 248, 250); LINE = (219, 232, 238)

W, H = 1080, 1560
img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

# ── nagłówek ──────────────────────────────────────────────────────────────
d.rectangle((0, 0, W, 205), fill=TEAL)
d.ellipse((820, -120, 1180, 240), fill=(35, 195, 183))
d.text((50, 38), "TO BE — ściąga przed kartkówką", font=F_BOLD(46), fill=WHITE)
d.text((52, 112), "Język angielski · klasa 4 · kartkówka 10 listopada 2026", font=F_REG(25), fill=WHITE)
d.text((52, 155), "odmiana · twierdzenia · pytania · przeczenia", font=F_REG(22), fill=(220, 255, 250))

# ── tabela odmiany ────────────────────────────────────────────────────────
d.text((50, 228), "ODMIANA PRZEZ OSOBY", font=F_BOLD(27), fill=TEAL_D)
T_X, T_Y, T_W, ROW = 35, 268, 1010, 58
d.rounded_rectangle((T_X, T_Y, T_X + T_W, T_Y + 46 + 8 * ROW), radius=18, fill=WHITE, outline=LINE, width=2)
cols = [(50, "OSOBA"), (235, "BE"), (420, "KRÓTKO"), (640, "PRZECZENIE"), (865, "PYTANIE")]
for x, label in cols:
    d.text((x, T_Y + 12), label, font=F_BOLD(20), fill=TEAL_D)
d.line((T_X + 12, T_Y + 46, T_X + T_W - 12, T_Y + 46), fill=LINE, width=2)

rows = [
    ("I", "am", "I'm", "I'm not", "Am I?"),
    ("You", "are", "You're", "You aren't", "Are you?"),
    ("He", "is", "He's", "He isn't", "Is he?"),
    ("She", "is", "She's", "She isn't", "Is she?"),
    ("It", "is", "It's", "It isn't", "Is it?"),
    ("We", "are", "We're", "We aren't", "Are we?"),
    ("You (wy)", "are", "You're", "You aren't", "Are you?"),
    ("They", "are", "They're", "They aren't", "Are they?"),
]
for i, r in enumerate(rows):
    y = T_Y + 54 + i * ROW
    if i % 2 == 1:
        d.rounded_rectangle((T_X + 8, y - 6, T_X + T_W - 8, y + 44), radius=8, fill=(248, 252, 253))
    d.text((50, y), r[0], font=F_BOLD(25), fill=NAVY)
    d.text((235, y), r[1], font=F_BOLD(25), fill=TEAL_D)
    for j in (2, 3, 4):
        d.text((cols[j][0], y), r[j], font=F_REG(25), fill=NAVY if j != 3 else CORAL)

# ── zasady ────────────────────────────────────────────────────────────────
R_Y = 800
d.text((50, R_Y), "3 ZASADY + 1 PUŁAPKA", font=F_BOLD(27), fill=CORAL)
d.rounded_rectangle((35, R_Y + 38, 1045, R_Y + 372), radius=18, fill=WHITE, outline=LINE, width=2)
rules = [
    ("+", "TWIERDZENIE:", "osoba + be  →  She is eleven.", TEAL_D),
    ("−", "PRZECZENIE:", "be + not  →  isn't / aren't / I'm not", CORAL),
    ("?", "PYTANIE:", "be NA POCZĄTKU  →  Is she Polish?", TEAL_D),
    ("!", "PUŁAPKA:", "Yes, I am. (nie: Yes, I'm. / amn't)", CORAL),
]
for i, (mark, label, rest, color) in enumerate(rules):
    y = R_Y + 58 + i * 78
    d.rounded_rectangle((55, y, 99, y + 44), radius=10, fill=color)
    d.text((66, y + 4), mark, font=F_BOLD(30), fill=WHITE)
    d.text((118, y + 6), label, font=F_BOLD(27), fill=color)
    lw = d.textlength(label, font=F_BOLD(27))
    d.text((128 + lw, y + 8), rest, font=F_REG(25), fill=NAVY)

# ── kod QR ────────────────────────────────────────────────────────────────
Q_Y = 1200
d.rounded_rectangle((35, Q_Y, 1045, Q_Y + 285), radius=18, fill=TEAL_L, outline=(183, 233, 227), width=2)
qr = qrcode.QRCode(version=None, error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=10, border=2)
qr.add_data(URL); qr.make(fit=True)
qr_img = qr.make_image(fill_color=NAVY, back_color=WHITE).convert("RGB").resize((248, 248), Image.LANCZOS)
img.paste(qr_img, (58, Q_Y + 18))
d.text((335, Q_Y + 34), "Pełny trening na telefon:", font=F_BOLD(32), fill=NAVY)
d.text((335, Q_Y + 82), "15 ćwiczeń, fiszki i próbna", font=F_REG(27), fill=NAVY)
d.text((335, Q_Y + 118), "kartkówka — bez logowania.", font=F_REG(27), fill=NAVY)
d.text((335, Q_Y + 175), "Zeskanuj kod lub kliknij link", font=F_BOLD(24), fill=TEAL_D)
d.text((335, Q_Y + 208), "w wiadomości na Telegramie.", font=F_REG(23), fill=MUTED)
d.text((60, Q_Y + 258), "Działa też offline, gdy wyślesz sobie plik HTML.", font=F_REG(19), fill=MUTED)

d.text((50, 1515), "Materiał: Together 2026, kl. 4 (Unit 1–2) · przygotowanie do kartkówki 10.11.2026", font=F_REG(18), fill=MUTED)

for dest in (HERE / "sciaga-to-be.png", HERE.parent / "docs" / "sciaga-to-be.png"):
    dest.parent.mkdir(parents=True, exist_ok=True)
    img.save(dest, "PNG", optimize=True)
    print("OK →", dest)
