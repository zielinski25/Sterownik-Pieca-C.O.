#!/usr/bin/env python3
"""Generuje rozbudowaną, dwustronicową ściągę jako PNG (do wysłania na Telegramie
jako zdjęcie / do druku A4).

Strona 1: odmiana, 5 zasad + pułapka, duży kod QR do treningu.
Strona 2: błędy, pytania o dane osobowe, mini-kartkówka z kluczem, plan + samoocena.

Wymaga: pip install pillow qrcode
Efekt:  sciaga-to-be.png, sciaga-to-be-2.png (+ kopie w ../docs/)
"""
from pathlib import Path
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
    import qrcode
except ImportError:
    sys.exit("Brak bibliotek: zainstaluj `python3 -m pip install pillow qrcode`")

HERE = Path(__file__).parent
URL = "https://is.gd/gAbph1"

FONT_DIR = Path("/usr/share/fonts/truetype/dejavu")


def font(name, size):
    return ImageFont.truetype(str(FONT_DIR / name), size)


F_BOLD = lambda s: font("DejaVuSans-Bold.ttf", s)
F_REG = lambda s: font("DejaVuSans.ttf", s)

TEAL = (18, 179, 166); TEAL_D = (11, 143, 134); TEAL_L = (226, 247, 244)
NAVY = (18, 48, 73); CORAL = (255, 122, 89); SUN = (255, 201, 60); MUTED = (92, 115, 144)
WHITE = (255, 255, 255); BG = (243, 248, 250); LINE = (219, 232, 238)
W, H = 1080, 1560


def new_page():
    img = Image.new("RGB", (W, H), BG)
    return img, ImageDraw.Draw(img)


def header(d, title, sub1, sub2, page):
    d.rectangle((0, 0, W, 190), fill=TEAL)
    d.ellipse((830, -130, 1180, 230), fill=(35, 195, 183))
    d.text((50, 32), title, font=F_BOLD(45), fill=WHITE)
    d.text((52, 104), sub1, font=F_REG(24), fill=WHITE)
    d.text((52, 145), sub2, font=F_REG(21), fill=(220, 255, 250))
    d.rounded_rectangle((855, 120, 1025, 160), radius=14, fill=(11, 143, 134))
    d.text((875, 127), "STRONA " + page, font=F_BOLD(21), fill=WHITE)


def sec_title(d, y, text, color):
    d.text((50, y), text, font=F_BOLD(27), fill=color)


def box(d, y1, y2, fill=WHITE, outline=LINE, width=2):
    d.rounded_rectangle((35, y1, 1045, y2), radius=18, fill=fill, outline=outline, width=width)


def make_qr(size):
    qr = qrcode.QRCode(version=None, error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=10, border=2)
    qr.add_data(URL)
    qr.make(fit=True)
    return qr.make_image(fill_color=NAVY, back_color=WHITE).convert("RGB").resize((size, size), Image.LANCZOS)


# ══════════════════════════════════════════════════════════════════════════
# STRONA 1 — odmiana, zasady, kod QR
# ══════════════════════════════════════════════════════════════════════════
p1, d = new_page()
header(d, "TO BE — ściąga przed kartkówką",
       "Język angielski · klasa 4 · kartkówka 10 listopada 2026",
       "odmiana · twierdzenia · pytania · przeczenia · krótkie odpowiedzi", "1 / 2")

sec_title(d, 212, "ODMIANA PRZEZ OSOBY", TEAL_D)
T_X, T_Y, T_W, ROW = 35, 252, 1010, 58
box(d, T_Y, T_Y + 46 + 8 * ROW)
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

R_Y = 784
sec_title(d, R_Y, "4 ZASADY + 1 PUŁAPKA", CORAL)
box(d, R_Y + 38, R_Y + 398)
rules = [
    ("+", "TWIERDZENIE:", "osoba + be  →  She is eleven.", TEAL_D),
    ("−", "PRZECZENIE:", "be + not  →  isn't / aren't / I'm not", CORAL),
    ("?", "PYTANIE:", "be NA POCZĄTKU  →  Is she Polish?", TEAL_D),
    ("!", "ODPOWIEDŹ:", "Yes, I am. / No, I'm not.", CORAL),
    ("⚠", "PUŁAPKA:", "wiek → I'm eleven (nie I have 11 years)", TEAL_D),
]
for i, (mark, label, rest, color) in enumerate(rules):
    y = R_Y + 54 + i * 68
    d.rounded_rectangle((55, y, 99, y + 44), radius=10, fill=color)
    d.text((66, y + 4), mark, font=F_BOLD(28), fill=WHITE)
    d.text((118, y + 6), label, font=F_BOLD(24), fill=color)
    lw = d.textlength(label, font=F_BOLD(24))
    d.text((128 + lw, y + 8), rest, font=F_REG(22), fill=NAVY)

Q_Y = 1215
box(d, Q_Y, Q_Y + 285, fill=TEAL_L, outline=(183, 233, 227))
p1.paste(make_qr(248), (58, Q_Y + 18))
d.text((335, Q_Y + 24), "Pełny trening na telefon:", font=F_BOLD(30), fill=NAVY)
d.text((335, Q_Y + 68), "4 949 zadań · 16 ćwiczeń", font=F_REG(24), fill=NAVY)
d.text((335, Q_Y + 100), "oceny 1–6 · powtórki błędów", font=F_REG(24), fill=NAVY)
d.text((335, Q_Y + 132), "Laboratorium · Miks · światła", font=F_REG(24), fill=NAVY)
d.text((335, Q_Y + 176), "Zeskanuj kod lub kliknij link", font=F_BOLD(23), fill=TEAL_D)
d.text((335, Q_Y + 210), "w wiadomości na Telegramie.", font=F_REG(22), fill=MUTED)
d.text((60, Q_Y + 258), "Strona 2: błędy, pytania, mini-kartkówka i plan. Działa też offline.", font=F_REG(18), fill=MUTED)
d.text((50, 1518), "Materiał: Together 2026, kl. 4 (Unit 1–2) · przygotowanie do kartkówki 10.11.2026", font=F_REG(18), fill=MUTED)

# ══════════════════════════════════════════════════════════════════════════
# STRONA 2 — pułapki, pytania, mini-kartkówka, plan
# ══════════════════════════════════════════════════════════════════════════
p2, d = new_page()
header(d, "TO BE — ściąga · strona 2",
       "Pułapki · pytania o dane · mini-kartkówka · plan",
       'Wszystko, co lubi „zaskoczyć" na kartkówce', "2 / 2")

# A. Błędy
sec_title(d, 212, "6 BŁĘDÓW, KTÓRE LUBI NAUCZYCIELKA", CORAL)
box(d, 252, 588)
mistakes = [
    ("He are my friend.", "He is my friend."),
    ("I is eleven.", "I am eleven."),
    ("I amn't ten.", "I'm not ten."),
    ("Yes, I'm.", "Yes, I am."),
    ("Where you are from?", "Where are you from?"),
    ("I have eleven years.", "I'm eleven."),
]
for i, (bad, good) in enumerate(mistakes):
    y = 266 + i * 52
    if i % 2 == 1:
        d.rounded_rectangle((45, y - 6, 1035, y + 40), radius=8, fill=(248, 252, 253))
    d.text((60, y), bad, font=F_REG(23), fill=CORAL)
    d.text((420, y), "→", font=F_BOLD(23), fill=MUTED)
    d.text((475, y), good, font=F_BOLD(23), fill=TEAL_D)

# B. Pytania o dane osobowe
sec_title(d, 610, "PYTANIA O DANE OSOBOWE", TEAL_D)
box(d, 650, 950)
book_q = [
    ("What's your name?", "I'm Lena. / My name's Lena."),
    ("Where are you from?", "I'm from Poland."),
    ("How old are you?", "I'm eleven."),
    ("What's your nationality?", "I'm Polish."),
    ("How do you spell it?", "L-E-N-A."),
    ("Who is he?", "He's my dad."),
]
for i, (q, a) in enumerate(book_q):
    y = 664 + i * 46
    if i % 2 == 1:
        d.rounded_rectangle((45, y - 6, 1035, y + 38), radius=8, fill=(248, 252, 253))
    d.text((60, y), q, font=F_BOLD(22), fill=NAVY)
    d.text((410, y), "→", font=F_BOLD(22), fill=MUTED)
    d.text((455, y), a, font=F_REG(22), fill=TEAL_D)

# C. Mini-kartkówka z kluczem
sec_title(d, 972, "MINI KARTKÓWKA · sprawdź się (10 zdań)", CORAL)
box(d, 1012, 1262)
mini = [
    "1. I ___ eleven.                       6. Robbie ___ a robot.",
    "2. Mickey and Ana ___ friends.        7. ___ she at school today?",
    "3. We ___ from China.  (przeczenie)  8. Are you ten? — Yes, ___.",
    "4. My shoes ___ new.  (przeczenie)   9. Where ___ you from?",
    "5. Popraw: He are my friend. → ___   10. Popraw: I amn't ten. → ___",
]
for i, line in enumerate(mini):
    d.text((60, 1028 + i * 40), line, font=F_REG(21), fill=NAVY)
d.text((60, 1230), "Klucz dla rodzica (zasłoń!): am · are · aren't · aren't · He is · is · Is · I am · are · I'm not",
       font=F_REG(15), fill=MUTED)

# D. Plan + samoocena + mały QR
sec_title(d, 1284, "3 KROKI PRZED KARTKÓWKĄ + SAMOOCENA", TEAL_D)
box(d, 1324, 1492, fill=TEAL_L, outline=(183, 233, 227))
d.text((58, 1342), "1. Tabela be (strona 1)   →   2. Miks 5 minut   →   3. Próbna kartkówka",
       font=F_BOLD(20), fill=NAVY)
d.text((58, 1430), "Wiek: I'm eleven · Przeczenie: isn't / aren't / I'm not · Pytanie: Is she...?",
       font=F_REG(20), fill=NAVY)
# kolorowe kółka zamiast emoji (DejaVu nie ma glyphów emoji)
lights = [((47, 158, 111), 58, "Umiem", 90), ((217, 155, 28), 205, "Prawie", 237),
          ((209, 86, 75), 345, "Jeszcze ćwiczę", 377)]
for color, cx, label, tx in lights:
    d.ellipse((cx, 1388, cx + 24, 1412), fill=color)
    d.text((tx, 1386), label, font=F_BOLD(19), fill=NAVY)
d.text((600, 1386), "— zaznacz w Powtórkach", font=F_REG(18), fill=MUTED)
p2.paste(make_qr(112), (890, 1350))
d.text((880, 1468), "kod QR → trening", font=F_REG(15), fill=MUTED)
d.text((50, 1518), URL + " · Materiał: Together 2026, kl. 4 · kartkówka 10.11.2026", font=F_REG(18), fill=MUTED)

# ── zapis ─────────────────────────────────────────────────────────────────
for name, img in (("sciaga-to-be.png", p1), ("sciaga-to-be-2.png", p2)):
    for dest in (HERE / name, HERE.parent / "docs" / name):
        dest.parent.mkdir(parents=True, exist_ok=True)
        img.save(dest, "PNG", optimize=True)
        print("OK →", dest)
