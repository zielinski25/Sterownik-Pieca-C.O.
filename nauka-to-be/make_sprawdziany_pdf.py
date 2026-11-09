#!/usr/bin/env python3
"""Generuje sprawdziany do druku jako OSOBNE pliki PDF (A4).

Wymaga: pip install fpdf2   (w tej sesji: PYTHONPATH=/tmp/pylibs)
Dane pobiera prosto z data.js przez node, żeby PDF-y zawsze zgadzały się
z tym, co widać na stronie i w plikach HTML.

Wyniki (zapisuje też kopie w ../docs/):
  sprawdzian-1.pdf ... sprawdzian-6.pdf   2 strony: arkusz + klucz
  klucz-1.pdf     ... klucz-6.pdf        1 strona: sam klucz dla rodzica
  sprawdziany-wszystkie.pdf               13 stron: okładka + 6 arkuszy + 6 kluczy
  klucze-wszystkie.pdf                    6 stron: wszystkie klucze
"""

import json
import subprocess
from pathlib import Path

from fpdf import FPDF
from fpdf.enums import XPos, YPos

HERE = Path(__file__).resolve().parent
DOCS = HERE.parent / "docs"
FONT_DIR = Path("/usr/share/fonts/truetype/dejavu")
FONT_R = FONT_DIR / "DejaVuSans.ttf"
FONT_B = FONT_DIR / "DejaVuSans-Bold.ttf"

NAVY = (18, 48, 73)
INK = (24, 45, 63)
TEAL = (11, 143, 134)
MUTED = (92, 115, 144)
LINE = (178, 196, 208)
LIGHT = (226, 247, 244)

W = 178.0          # 210 mm - 2 x 16 mm marginesu
DATE = "10 listopada 2026"


def load_sheets():
    code = "require(%r); process.stdout.write(JSON.stringify(global.TOBE_DATA.printTests));" % str(HERE / "data.js")
    raw = subprocess.check_output(["node", "-e", code], cwd=str(HERE))
    sheets = json.loads(raw.decode("utf-8"))
    if len(sheets) != 6 or any(len(s["questions"]) != 15 for s in sheets):
        raise SystemExit("BŁĄD: oczekiwano 6 sprawdzianów po 15 pytań")
    prompts = [q["p"] for s in sheets for q in s["questions"]]
    if len(set(prompts)) != 90:
        raise SystemExit("BŁĄD: sprawdziany zawierają powtórzone pytania")
    return sheets


def answer(q):
    a = q["a"]
    return a[0] if isinstance(a, list) else str(a)


class SheetPDF(FPDF):
    def __init__(self, title="Sprawdziany TO BE"):
        super().__init__(orientation="P", unit="mm", format="A4")
        self.set_margins(16, 14, 16)
        self.set_auto_page_break(True, margin=14)
        self.add_font("DejaVu", "", str(FONT_R))
        self.add_font("DejaVu", "B", str(FONT_B))
        self.set_title(title)
        self.set_author("TO BE — trening przed kartkówką")
        self.set_subject("czasownik to be: odmiana, twierdzenia, pytania, przeczenia")
        self.set_creator("make_sprawdziany_pdf.py")

    def rule(self, color=NAVY, width=0.7):
        self.set_draw_color(*color)
        self.set_line_width(width)
        self.line(self.l_margin, self.get_y(), self.w - self.r_margin, self.get_y())
        self.set_line_width(0.2)

    def dashed(self, x1, y, x2):
        self.set_draw_color(*LINE)
        self.set_line_width(0.22)
        self.set_dash_pattern(dash=0.9, gap=0.9)
        self.line(x1, y, x2, y)
        self.set_dash_pattern()                     # powrót do linii ciągłej
        self.set_line_width(0.2)


def sheet_page(pdf, sh, n):
    """Jedna strona A4: arkusz dla dziecka, 15 zadań."""
    pdf.add_page()
    pdf.set_font("DejaVu", "B", 17)
    pdf.set_text_color(*NAVY)
    pdf.cell(W, 9, "SPRAWDZIAN %d" % n, new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_font("DejaVu", "", 10.5)
    pdf.set_text_color(*TEAL)
    pdf.cell(W, 6, "Język angielski · czasownik „to be” · klasa 4 · kartkówka " + DATE,
             new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_y(pdf.get_y() + 1.5)
    pdf.rule(NAVY, 0.8)
    pdf.set_y(pdf.get_y() + 3)

    pdf.set_font("DejaVu", "", 10)
    pdf.set_text_color(*INK)
    pdf.cell(W, 6.5,
             "Imię i nazwisko: __________________________   Data: ____________   Wynik: _____ / 15   Ocena: _____",
             new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_font("DejaVu", "", 8.6)
    pdf.set_text_color(*MUTED)
    pdf.cell(W, 5.5,
             "Zakres: odmiana, twierdzenia, pytania, przeczenia, krótkie odpowiedzi · Together 2026",
             new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_y(pdf.get_y() + 2.5)
    pdf.rule(LINE, 0.35)
    pdf.set_y(pdf.get_y() + 3)

    for i, q in enumerate(sh["questions"], 1):
        if pdf.get_y() > 266:                     # awaryjnie: nie ucinaj zadań
            pdf.add_page()
            pdf.set_y(20)
        x0 = pdf.l_margin
        pdf.set_xy(x0, pdf.get_y())
        pdf.set_font("DejaVu", "B", 10.2)
        pdf.set_text_color(*NAVY)
        pdf.cell(8.5, 5.4, "%d." % i, new_x=XPos.RIGHT, new_y=YPos.TOP)
        xt = pdf.get_x()

        pdf.set_font("DejaVu", "", 10.2)
        pdf.set_text_color(*INK)
        pdf.multi_cell(W - 8.5, 5.4, q["p"], new_x=XPos.LMARGIN, new_y=YPos.NEXT)

        if q.get("o"):
            pdf.set_x(xt)
            pdf.set_font("DejaVu", "", 9.2)
            pdf.set_text_color(*MUTED)
            pdf.multi_cell(W - 8.5, 4.8,
                           "A) %s      B) %s      C) %s" % tuple(q["o"][:3]),
                           new_x=XPos.LMARGIN, new_y=YPos.NEXT)

        y = pdf.get_y() + 1.4
        pdf.dashed(xt, y, xt + W - 8.5 - 4)
        pdf.set_y(y + 5.0)                       # miejsce na wpisanie odpowiedzi

    pdf.set_y(max(pdf.get_y() + 1, 272))
    pdf.set_font("DejaVu", "", 8.2)
    pdf.set_text_color(*MUTED)
    pdf.set_draw_color(*LINE)
    pdf.set_line_width(0.3)
    pdf.line(pdf.l_margin, pdf.get_y() - 2, pdf.w - pdf.r_margin, pdf.get_y() - 2)
    pdf.cell(W, 5, "Wypełnij luki lub zaznacz A / B / C. Za każde zadanie 1 punkt. Powodzenia!",
             new_x=XPos.LMARGIN, new_y=YPos.NEXT)


def key_page(pdf, sh, n):
    """Jedna strona A4: klucz odpowiedzi dla rodzica / nauczyciela."""
    pdf.add_page()
    pdf.set_fill_color(*LIGHT)
    pdf.rect(pdf.l_margin, 12, W, 15, style="F")
    pdf.set_xy(pdf.l_margin + 4, 14)
    pdf.set_font("DejaVu", "B", 14)
    pdf.set_text_color(*TEAL)
    pdf.cell(W - 8, 7, "KLUCZ ODPOWIEDZI  ·  sprawdzian %d" % n,
             new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_x(pdf.l_margin + 4)
    pdf.set_font("DejaVu", "", 8.8)
    pdf.set_text_color(*MUTED)
    pdf.cell(W - 8, 5, "Dla rodzica / nauczyciela — nie drukuj razem z arkuszem dla dziecka.",
             new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_y(pdf.get_y() + 4)
    pdf.rule(NAVY, 0.8)
    pdf.set_y(pdf.get_y() + 4)

    for i, q in enumerate(sh["questions"], 1):
        x0 = pdf.l_margin
        pdf.set_xy(x0, pdf.get_y())
        pdf.set_font("DejaVu", "B", 10)
        pdf.set_text_color(*NAVY)
        pdf.cell(8.5, 5.6, "%d." % i, new_x=XPos.RIGHT, new_y=YPos.TOP)
        xt = pdf.get_x()
        pdf.set_font("DejaVu", "B", 10)
        pdf.set_text_color(*TEAL)
        ans = answer(q)
        aw = pdf.get_string_width(ans) + 2
        pdf.cell(aw, 5.6, ans, new_x=XPos.RIGHT, new_y=YPos.TOP)
        pdf.set_font("DejaVu", "", 9.3)
        pdf.set_text_color(*MUTED)
        rest = "  —  " + (q.get("w") or "")
        pdf.multi_cell(W - 8.5 - aw, 5.6, rest, new_x=XPos.LMARGIN, new_y=YPos.NEXT)
        pdf.set_y(pdf.get_y() + 1.1)

    pdf.set_y(max(pdf.get_y() + 2, 268))
    pdf.set_draw_color(*LINE)
    pdf.set_line_width(0.3)
    pdf.line(pdf.l_margin, pdf.get_y() - 2, pdf.w - pdf.r_margin, pdf.get_y() - 2)
    pdf.set_font("DejaVu", "", 8.6)
    pdf.set_text_color(*MUTED)
    pdf.multi_cell(W, 5,
                   "Skala (zgodna z aplikacją): 15 pkt → 6 · 14 → 5 · 11–13 → 4 · 9–10 → 3 · 6–8 → 2 · 0–5 → 1.",
                   new_x=XPos.LMARGIN, new_y=YPos.NEXT)


def cover_page(pdf, sheets):
    pdf.add_page()
    pdf.set_fill_color(*LIGHT)
    pdf.rect(0, 0, 210, 62, style="F")
    pdf.set_xy(16, 16)
    pdf.set_font("DejaVu", "B", 21)
    pdf.set_text_color(*NAVY)
    pdf.cell(W, 11, "6 sprawdzianów z „to be”", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_x(16)
    pdf.set_font("DejaVu", "", 11)
    pdf.set_text_color(*TEAL)
    pdf.cell(W, 7, "Język angielski · klasa 4 · kartkówka " + DATE,
             new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_x(16)
    pdf.set_font("DejaVu", "", 9.5)
    pdf.set_text_color(*MUTED)
    pdf.cell(W, 6, "Odmiana przez osoby · twierdzenia · pytania · przeczenia · krótkie odpowiedzi",
             new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_y(72)

    pdf.set_font("DejaVu", "B", 12)
    pdf.set_text_color(*NAVY)
    pdf.cell(W, 8, "Co jest w środku", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_y(pdf.get_y() + 2)
    pdf.set_font("DejaVu", "", 10)
    pdf.set_text_color(*INK)
    for line in [
        "Strony 2–7     6 arkuszy dla dziecka po 15 zadań (każdy na jednej stronie A4).",
        "Strony 8–13    6 kluczy odpowiedzi z wyjaśnieniami, po jednym na stronę.",
        "",
        "Sprawdziany są ROZŁĄCZNE: żadne pytanie nie powtarza się w innym arkuszu.",
        "W każdym teście są 4 typy zadań: dobierz formę be, utwórz pytanie,",
        "napisz przeczenie, znajdź i napraw błąd.",
        "",
        "Osobne pliki PDF: sprawdzian-1.pdf … sprawdzian-6.pdf (arkusz + klucz)",
        "oraz klucz-1.pdf … klucz-6.pdf (sam klucz do wydruku dla rodzica).",
    ]:
        pdf.cell(W, 6.4, line, new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_y(pdf.get_y() + 8)

    pdf.set_font("DejaVu", "B", 12)
    pdf.set_text_color(*NAVY)
    pdf.cell(W, 8, "Spis sprawdzianów", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_y(pdf.get_y() + 2)
    for i, sh in enumerate(sheets, 1):
        areas = {}
        for q in sh["questions"]:
            areas[q.get("area", "odmiana")] = areas.get(q.get("area", "odmiana"), 0) + 1
        pdf.set_font("DejaVu", "B", 10)
        pdf.set_text_color(*TEAL)
        pdf.cell(34, 6.6, "Sprawdzian %d" % i, new_x=XPos.RIGHT, new_y=YPos.TOP)
        pdf.set_font("DejaVu", "", 9.5)
        pdf.set_text_color(*MUTED)
        pdf.cell(W - 34, 6.6, "15 zadań · " + " / ".join(sorted(areas)),
                 new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_y(pdf.get_y() + 10)
    pdf.rule(LINE, 0.35)
    pdf.set_y(pdf.get_y() + 4)
    pdf.set_font("DejaVu", "", 9)
    pdf.set_text_color(*MUTED)
    pdf.multi_cell(W, 5.4,
                   "Druk: A4, domyślne marginesy, wyłącz nagłówki i stopki przeglądarki. "
                   "Pliki działają bez internetu. Więcej ćwiczeń bez powtórek: https://is.gd/gAbph1",
                   new_x=XPos.LMARGIN, new_y=YPos.NEXT)


def save(pdf, name, both=True):
    for d in ((HERE, DOCS) if both else (HERE,)):
        d.mkdir(parents=True, exist_ok=True)
        pdf.output(str(d / name))
    print("OK → %s (%d stron)" % (name, pdf.page_no()))


def main():
    sheets = load_sheets()
    if not FONT_R.exists() or not FONT_B.exists():
        raise SystemExit("BŁĄD: brak fontów DejaVu Sans w " + str(FONT_DIR))

    # 6 osobnych testów: arkusz + klucz
    for i, sh in enumerate(sheets, 1):
        p = SheetPDF("Sprawdzian %d · TO BE" % i)
        sheet_page(p, sh, i)
        key_page(p, sh, i)
        save(p, "sprawdzian-%d.pdf" % i)

    # 6 osobnych kluczy
    for i, sh in enumerate(sheets, 1):
        p = SheetPDF("Klucz %d · TO BE" % i)
        key_page(p, sh, i)
        save(p, "klucz-%d.pdf" % i)

    # wszystkie arkusze + wszystkie klucze w jednym pliku
    p = SheetPDF("Sprawdziany 1–6 · TO BE")
    cover_page(p, sheets)
    for i, sh in enumerate(sheets, 1):
        sheet_page(p, sh, i)
    for i, sh in enumerate(sheets, 1):
        key_page(p, sh, i)
    save(p, "sprawdziany-wszystkie.pdf")

    # same klucze w jednym pliku
    p = SheetPDF("Klucze 1–6 · TO BE")
    for i, sh in enumerate(sheets, 1):
        key_page(p, sh, i)
    save(p, "klucze-wszystkie.pdf")

    print("Razem: 6 oddzielnych testów PDF + 6 kluczy + 2 wydania zbiorcze ✓")


if __name__ == "__main__":
    main()
