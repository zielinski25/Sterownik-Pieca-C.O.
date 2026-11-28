#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generator warstw 1-bit (40x40) ikony PANEL "jak w aplikacji" (v1.5.11).

Zrodlo ksztaltu: android/.../ui/art/Ilu.kt, fun panel() - kolektor rurowy
(4 ukosne rury), poziomy zbiornik u gory, stojak, slonce w prawym gornym rogu.
viewBox 24x24 -> 40x40 (skala 5/3). Kazda warstwa osobno, bo format
LV_IMG_CF_ALPHA_1BIT ma jeden kolor na obrazek (recolor w runtime):

  IKONA_PANEL_SLONCE    - kolo + 8 promieni            (KOL_SLONCE, zolty)
  IKONA_PANEL_RURY      - 4 rury                       (KOL_KROPLA / KOL_ERR)
  IKONA_PANEL_ZBIORNIK  - zbiornik (obrys) + zaworek   (KOL_PANEL_ZBIORNIK)
  IKONA_PANEL_STOJAK    - nogi + podloze + szyna       (KOL_TEKST_DIM)

Bez PIL: wlasny rasteryzator - 8x8 podprobek na piksel, piksel zapalony gdy
pokrycie >= PROG. Wynik: tablice C w formacie ikony.h (MSB pierwszy, wiersz
padowany do pelnego bajtu, 5 bajtow na wiersz) + podglad ASCII na stderr.

Uzycie:  python3 ikony_jak_w_aplikacji.py > /tmp/warstwy.h
"""
import math
import sys

ROZMIAR = 40
S = ROZMIAR / 24.0          # skala viewBox aplikacji -> piksele
PODPROBKI = 8


# ── prymitywy (odleglosci w pikselach) ──────────────────────────────────────
def odl_odcinek(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    l2 = dx * dx + dy * dy
    if l2 == 0:
        return math.hypot(px - x1, py - y1)
    t = max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / l2))
    return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))


def linia(x1, y1, x2, y2, szer):
    """Odcinek o zaokraglonych koncach (stroke-linecap=round)."""
    return lambda px, py: odl_odcinek(px, py, x1, y1, x2, y2) <= szer / 2.0


def kolo(cx, cy, r):
    return lambda px, py: math.hypot(px - cx, py - cy) <= r


def pierscien(cx, cy, r, szer):
    return lambda px, py: abs(math.hypot(px - cx, py - cy) - r) <= szer / 2.0


def prostokat_zaokr_obrys(x, y, w, h, rx, szer):
    """Obrys zaokraglonego prostokata (SDF)."""
    def sdf(px, py):
        cx, cy = x + w / 2.0, y + h / 2.0
        qx = abs(px - cx) - (w / 2.0 - rx)
        qy = abs(py - cy) - (h / 2.0 - rx)
        zew = math.hypot(max(qx, 0.0), max(qy, 0.0))
        wew = min(max(qx, qy), 0.0)
        return zew + wew - rx
    return lambda px, py: abs(sdf(px, py)) <= szer / 2.0


def rasteryzuj(ksztalty, prog=0.5):
    """ksztalty: lista funkcji (px,py)->bool. Zwraca liste wierszy bitow."""
    wiersze = []
    for y in range(ROZMIAR):
        wiersz = []
        for x in range(ROZMIAR):
            trafione = 0
            for sy in range(PODPROBKI):
                py = y + (sy + 0.5) / PODPROBKI
                for sx in range(PODPROBKI):
                    px = x + (sx + 0.5) / PODPROBKI
                    if any(k(px, py) for k in ksztalty):
                        trafione += 1
            wiersz.append(1 if trafione >= prog * PODPROBKI * PODPROBKI else 0)
        wiersze.append(wiersz)
    return wiersze


def do_bajtow(wiersze):
    stride = (ROZMIAR + 7) // 8
    out = []
    for w in wiersze:
        for b in range(stride):
            v = 0
            for bit in range(8):
                x = b * 8 + bit
                if x < ROZMIAR and w[x]:
                    v |= 0x80 >> bit
            out.append(v)
    return out


def ascii(wiersze):
    return "\n".join("".join("#" if v else "." for v in w) for w in wiersze)


def tablica_c(nazwa, bajty, komentarz):
    stride = (ROZMIAR + 7) // 8
    linie = [f"// {komentarz}", f"const uint8_t {nazwa}[] PROGMEM = {{"]
    for r in range(ROZMIAR):
        czesc = bajty[r * stride:(r + 1) * stride]
        linie.append("  " + ", ".join(f"0x{b:02X}" for b in czesc) + ",")
    linie.append("};")
    return "\n".join(linie)


# ── warstwy PANEL (wspolrzedne z Ilu.kt * S) ────────────────────────────────
def warstwa_slonce():
    cx, cy = 33.0, 6.5              # app: (19.5, 4.5)*S = (32.5, 7.5); lekko w prawo/gore, zeby promienie
    k = [kolo(cx, cy, 3.4)]         # nie dotykaly zbiornika (w aplikacji tez sie prawie stykaja)
    for i in range(8):
        a = i * math.pi / 4.0
        k.append(linia(cx + 4.6 * math.cos(a), cy + 4.6 * math.sin(a),
                       cx + 6.8 * math.cos(a), cy + 6.8 * math.sin(a), 1.6))
    return k


def warstwa_rury():
    # app: line (5+3i,10) -> (11+3i,19), stroke 1.8
    k = []
    for i in range(4):
        k.append(linia((5 + 3 * i) * S, 10 * S, (11 + 3 * i) * S, 19 * S, 1.8 * S))
    return k


def warstwa_zbiornik():
    # app: rect x3 y5.5 w14 h4.5 rx2.25 stroke .9 ; kropka (4.5,7.75) r.7
    # (zaworek 1.5x2.5 przy prawej sciance pominiety - w 40 px zlewal sie z obrysem w kleks)
    # szerokosc 14 -> 12.5 (prawy koniec 1,5 px od dolnego promienia slonca)
    k = [prostokat_zaokr_obrys(3 * S, 5.5 * S, 12.5 * S, 4.5 * S, 2.25 * S, 1.7),
         kolo(4.9 * S, 7.75 * S, 1.3)]
    return k


def warstwa_stojak():
    # app: M4 20.5 l3 -6.5 ; M16 20.5 l3 -6.5 ; M3 20.5 h16 (stroke .9) ; szyna M10 19.5 l11 -.5 (1.4)
    k = [linia(4 * S, 20.5 * S, 7 * S, 14 * S, 1.6),
         linia(16 * S, 20.5 * S, 19 * S, 14 * S, 1.6),
         linia(3 * S, 20.5 * S, 19 * S, 20.5 * S, 1.6),
         linia(10 * S, 19.5 * S, 21 * S, 19 * S, 1.8)]
    return k


WARSTWY = [
    ("IKONA_PANEL_SLONCE", warstwa_slonce, 0.5, "slonce: kolo + 8 promieni (prawy gorny rog), kolor KOL_SLONCE"),
    ("IKONA_PANEL_RURY", warstwa_rury, 0.5, "4 ukosne rury kolektora prozniowego, kolor KOL_KROPLA (alarm: KOL_ERR)"),
    ("IKONA_PANEL_ZBIORNIK", warstwa_zbiornik, 0.4, "poziomy zbiornik u gory (obrys) + kropka wskaznika, kolor KOL_PANEL_ZBIORNIK"),
    ("IKONA_PANEL_STOJAK", warstwa_stojak, 0.4, "stojak: dwie nogi, podloze, szyna, kolor KOL_TEKST_DIM"),
]


def main():
    razem = [[0] * ROZMIAR for _ in range(ROZMIAR)]
    for nazwa, fn, prog, opis in WARSTWY:
        w = rasteryzuj(fn(), prog)
        for y in range(ROZMIAR):
            for x in range(ROZMIAR):
                razem[y][x] |= w[y][x]
        print(f"--- {nazwa} ---\n{ascii(w)}\n", file=sys.stderr)
        print(tablica_c(nazwa, do_bajtow(w), opis))
        print()
    print("--- ZLOZENIE (OR wszystkich warstw) ---\n" + ascii(razem), file=sys.stderr)


if __name__ == "__main__":
    main()
