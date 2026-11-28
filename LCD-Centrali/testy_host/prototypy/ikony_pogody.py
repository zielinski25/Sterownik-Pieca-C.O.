#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generator warstw 1-bit (40x40) ikon POGODY kafelka ZEWN (v1.5.14).
Wzor: zestaw "kolorowe plaskie" z LCD-Centrali/docs/symulator_kafelka_zewn.html
(wybor uzytkownika 23.11). Format LV_IMG_CF_ALPHA_1BIT ma jeden kolor na
obrazek, wiec ikona wielokolorowa = 1-2 warstwy nakladane w runtime (ten sam
wzorzec co BOJLER/PANEL/POKOJ). Rasteryzacja: PIL, 8x nadprobkowanie,
zmniejszenie LANCZOS, prog 50 % pokrycia (jak reszta ikon w ikony.h).
Wyjscie: src/ikony_pogoda.h (tablice C + deskryptory LVGL + tabela ikon).
Uzycie:  python3 ikony_pogody.py            (nadpisuje ../../src/ikony_pogoda.h)
         python3 ikony_pogody.py --podglad  (ASCII kazdej warstwy na stdout)
"""
import math, os, sys
from PIL import Image, ImageDraw

R = 40
SS = 8
W = R * SS

def plotno():
    im = Image.new('L', (W, W), 0)
    return im, ImageDraw.Draw(im)

def s(v):  # skala viewBox 40 -> nadprobkowany
    return v * SS

def linia(d, x1, y1, x2, y2, szer, fill=255):
    w = int(round(s(szer)))
    d.line([(s(x1), s(y1)), (s(x2), s(y2))], fill=fill, width=w)
    r = (w - 1) / 2.0   # zaokraglone konce dokladnie o szerokosci linii (bez wystajacych pikseli)
    for (x, y) in ((x1, y1), (x2, y2)):
        d.ellipse([s(x) - r, s(y) - r, s(x) + r, s(y) + r], fill=fill)

def kolo(d, cx, cy, r, fill=255):
    d.ellipse([s(cx - r), s(cy - r), s(cx + r), s(cy + r)], fill=fill)

def chmura(d, dy=0, skala=1.0, dx=0, fill=255):
    """Chmura jak w SVG: dwa kola + podstawa z zaokraglonymi rogami."""
    k = skala
    cx = 20 + dx
    # podstawa
    x0, x1, yb = cx - 10 * k, cx + 11 * k, 28 * k + dy + 20 * (1 - k)
    h = 8 * k
    d.rounded_rectangle([s(x0), s(yb - h), s(x1), s(yb)], radius=s(h / 2), fill=fill)
    kolo(d, cx - 4 * k, yb - 7 * k, 7.5 * k, fill)
    kolo(d, cx + 4.5 * k, yb - 6 * k, 6.5 * k, fill)

def slonce(d, cx=20, cy=20, r=8, dl=4.5, odst=3.5, szer=2.6):
    kolo(d, cx, cy, r)
    for a in range(0, 360, 45):
        ra = math.radians(a)
        linia(d, cx + math.cos(ra) * (r + odst), cy + math.sin(ra) * (r + odst),
              cx + math.cos(ra) * (r + odst + dl), cy + math.sin(ra) * (r + odst + dl), szer)

def warstwy():
    out = {}
    # SLONCE - pelne
    im, d = plotno(); slonce(d); out['POG_SLONCE'] = im
    # SLONCE_MALE - w lewym gornym rogu (pod chmura "czesciowo")
    im, d = plotno(); slonce(d, 14, 14, 6, 3.5, 2.5, 2.2); out['POG_SLONCE_MALE'] = im
    # KSIEZYC - kolo minus przesuniete kolo
    im, d = plotno(); kolo(d, 20, 20, 12); kolo(d, 25.5, 16.5, 11, 0); out['POG_KSIEZYC'] = im
    # CHMURA - srodek
    # (sama chmura na calym kafelku - wieksza, skala 1.35: ok. 30x21 px, jak stara IKONA_ZEWN_NEUTRALNY)
    im, d = plotno(); chmura(d, skala=1.35, dy=1); out['POG_CHMURA'] = im
    # CHMURA_NISKA - nizej i w prawo (nad malym sloncem)
    im, d = plotno(); chmura(d, dy=5, skala=0.95, dx=2); out['POG_CHMURA_NISKA'] = im
    # CHMURA_GORA - wyzej, zeby pod nia zmiescily sie krople/platki
    im, d = plotno(); chmura(d, dy=-4); out['POG_CHMURA_GORA'] = im
    # CHMURA_GORA2 - jeszcze wyzej (ulewa, burza)
    im, d = plotno(); chmura(d, dy=-6); out['POG_CHMURA_GORA2'] = im
    # KROPLE - 3 ukosne kreski
    im, d = plotno()
    for x in (14, 20, 26): linia(d, x + 1, 30, x - 1, 36, 2.6)
    out['POG_KROPLE'] = im
    # KROPLE2 - ulewa: 3 + 2 kreski
    im, d = plotno()
    for x in (14, 20, 26): linia(d, x + 1, 28, x - 1, 33.5, 2.6)
    for x in (17, 23): linia(d, x + 1, 34.5, x - 1, 39, 2.6)
    out['POG_KROPLE2'] = im
    # KROPKI - mzawka
    im, d = plotno()
    for x in (14, 20, 26): kolo(d, x, 32.5, 1.9)
    out['POG_KROPKI'] = im
    # PLATKI - 3 male gwiazdki
    im, d = plotno()
    # (gwiazdka 7x7 piksel po pikselu - linie 1,5 px pod katem po progowaniu
    # zlewaly sie w kreski; wzor jak w ikonach bitmapowych)
    WZOR = ("#..#..#", ".#.#.#.", "..###..", "#######", "..###..", ".#.#.#.", "#..#..#")
    for x0 in (9, 17, 25):
        for yy, wiersz in enumerate(WZOR):
            for xx, c in enumerate(wiersz):
                if c == '#': d.rectangle([s(x0 + xx), s(30 + yy), s(x0 + xx + 1) - 1, s(30 + yy + 1) - 1], fill=255)
    out['POG_PLATKI'] = im
    # PLATEK_DUZY - sypie: 3 osie przez srodek, na kazdym z 6 koncow dwie
    # krotkie galazki pod katem 40 stopni skierowane do srodka
    im, d = plotno()
    cx, cy = 20, 20
    for a in (90, 30, 150):
        ra = math.radians(a); ux, uy = math.cos(ra), math.sin(ra)
        linia(d, cx - ux * 15, cy - uy * 15, cx + ux * 15, cy + uy * 15, 2.6)
        for zn in (1, -1):
            ex, ey = cx + zn * ux * 15, cy + zn * uy * 15
            bx, by = -zn * ux, -zn * uy            # kierunek do srodka
            for b in (-40, 40):
                rb = math.radians(b)
                gx = bx * math.cos(rb) - by * math.sin(rb); gy = bx * math.sin(rb) + by * math.cos(rb)
                linia(d, ex - bx * 4, ey - by * 4, ex - bx * 4 + gx * 5.5, ey - by * 4 + gy * 5.5, 2.2)
    out['POG_PLATEK_DUZY'] = im
    # PIORUN - blyskawica pod chmura
    im, d = plotno()
    pts = [(22, 22), (15, 31), (20, 31), (17, 39), (26, 28), (21, 28), (24, 22)]
    d.polygon([(s(x), s(y)) for x, y in pts], fill=255)
    out['POG_PIORUN'] = im
    # MGLA - 4 poziome kreski
    im, d = plotno()
    for (x1, x2, y) in ((7, 33, 13), (5, 35, 20), (9, 31, 27), (13, 27, 34)): linia(d, x1, y, x2, y, 2.8)
    out['POG_MGLA'] = im
    # WIATR - 3 linie z zawijasem (luk: PIL liczy katy zgodnie z zegarem od godz. 3)
    im, d = plotno()
    gr = int(round(s(2.8)))
    linia(d, 4, 15, 23, 15, 2.8); d.arc([s(19), s(7), s(27), s(15)], 180, 450, fill=255, width=gr)    # zawijas w gore
    linia(d, 4, 22, 29, 22, 2.8); d.arc([s(25), s(22), s(33), s(30)], 270, 540, fill=255, width=gr)   # zawijas w dol
    linia(d, 4, 29, 17, 29, 2.8); d.arc([s(14), s(29), s(20), s(35)], 270, 540, fill=255, width=gr)
    out['POG_WIATR'] = im
    # GRAD - 3 kulki
    im, d = plotno()
    for i, x in enumerate((14, 20, 26)): kolo(d, x, 33 + (i % 2) * 2.5, 2.3)
    out['POG_GRAD'] = im
    return out

def do_1bit(im):
    m = im.resize((R, R), Image.LANCZOS)
    w = [[1 if m.getpixel((x, y)) >= 128 else 0 for x in range(R)] for y in range(R)]
    # pojedyncze piksele bez zadnego sasiada (8 kierunkow) (resztki po zaokragleniach
    # koncow linii) - usun, zeby ikona nie miala "pylkow"
    for y in range(R):
        for x in range(R):
            if w[y][x] and not any(w[yy][xx] for yy in range(max(0, y - 1), min(R, y + 2)) for xx in range(max(0, x - 1), min(R, x + 2)) if (yy, xx) != (y, x)):
                w[y][x] = 0
    return w

def bajty(w):
    out = []
    for row in w:
        for b in range(0, R, 8):
            v = 0
            for i in range(8):
                x = b + i
                if x < R and row[x]: v |= 1 << (7 - i)
            out.append(v)
    return out

def ascii(w):
    return '\n'.join(''.join('#' if p else '.' for p in row) for row in w)

# kolory RGB -> RGB565 (jak KOL_* w dashboard_lvgl_poc.cpp)
def rgb565(hexs):
    r, g, b = int(hexs[0:2], 16), int(hexs[2:4], 16), int(hexs[4:6], 16)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

KOLORY = {  # nazwa: (hex z symulatora, opis)
    'KOL_POG_SLONCE':  ('f6b73c', 'slonce - zolty (jak KOL_SLONCE ikony PANEL)'),
    'KOL_POG_KSIEZYC': ('ffe9a8', 'ksiezyc - jasny krem'),
    'KOL_POG_CHMURA':  ('dfe9f5', 'chmura - jasna, prawie biala'),
    'KOL_POG_KROPLA':  ('4c9fff', 'deszcz, mzawka - niebieski (= KOL_KROPLA 0x4C9F)'),
    'KOL_POG_SNIEG':   ('dff3ff', 'snieg - biel z odcieniem blekitu'),
    'KOL_POG_PIORUN':  ('ffd34d', 'piorun - zolty'),
    'KOL_POG_MGLA':    ('9aa3b2', 'mgla - szary (jak KOL_TEKST_DIM)'),
    'KOL_POG_WIATR':   ('8fc7ff', 'wiatr - blekit (jak KOL_ZEWN_NEUTRALNY)'),
    'KOL_POG_GRAD':    ('cfe8ff', 'grad - bardzo jasny blekit'),
}
# ikona: (warstwa1, kolor1, warstwa2, kolor2) - warstwa2 rysowana NA warstwie1
IKONY = [
    ('POG_IK_SLONCE',     'slonecznie / bezchmurnie (dzien)',      'POG_SLONCE',       'KOL_POG_SLONCE',  None,              None),
    ('POG_IK_NOC',        'pogodnie (noc)',                         'POG_KSIEZYC',      'KOL_POG_KSIEZYC', None,              None),
    ('POG_IK_CZESCIOWO',  'czesciowe zachmurzenie / przejasnienia', 'POG_SLONCE_MALE',  'KOL_POG_SLONCE',  'POG_CHMURA_NISKA', 'KOL_POG_CHMURA'),
    ('POG_IK_POCHMURNO',  'pochmurno',                              'POG_CHMURA',       'KOL_POG_CHMURA',  None,              None),
    ('POG_IK_DESZCZ',     'deszcz nadciaga / przelotny',            'POG_CHMURA_GORA',  'KOL_POG_CHMURA',  'POG_KROPLE',      'KOL_POG_KROPLA'),
    ('POG_IK_ULEWA',      'pada (teraz) / ulewa',                   'POG_CHMURA_GORA2', 'KOL_POG_CHMURA',  'POG_KROPLE2',     'KOL_POG_KROPLA'),
    ('POG_IK_MZAWKA',     'mzawka',                                 'POG_CHMURA_GORA',  'KOL_POG_CHMURA',  'POG_KROPKI',      'KOL_POG_KROPLA'),
    ('POG_IK_SNIEG',      'snieg nadciaga / przelotny',             'POG_CHMURA_GORA',  'KOL_POG_CHMURA',  'POG_PLATKI',      'KOL_POG_SNIEG'),
    ('POG_IK_SYPIE',      'sypie snieg (teraz)',                    'POG_PLATEK_DUZY',  'KOL_POG_SNIEG',   None,              None),
    ('POG_IK_BURZA',      'burza',                                  'POG_CHMURA_GORA2', 'KOL_POG_CHMURA',  'POG_PIORUN',      'KOL_POG_PIORUN'),
    ('POG_IK_MGLA',       'mgla',                                   'POG_MGLA',         'KOL_POG_MGLA',    None,              None),
    ('POG_IK_WIATR',      'silny wiatr',                            'POG_WIATR',        'KOL_POG_WIATR',   None,              None),
    ('POG_IK_GRAD',       'grad',                                   'POG_CHMURA_GORA',  'KOL_POG_CHMURA',  'POG_GRAD',        'KOL_POG_GRAD'),
]

def main():
    w = {n: do_1bit(im) for n, im in warstwy().items()}
    if '--podglad' in sys.argv:
        for n, ww in w.items():
            print('== ' + n); print(ascii(ww))
        return
    L = []
    L.append('#pragma once')
    L.append('// WYGENEROWANE przez testy_host/prototypy/ikony_pogody.py - NIE EDYTOWAC RECZNIE.')
    L.append('// [v1.5.14 POGODA-ZEWN] Ikony pogody kafelka ZEWN: %d warstw 1-bit 40x40 (po 200 B),' % len(w))
    L.append('// %d ikon zlozonych z 1-2 warstw (druga warstwa rysowana na pierwszej, kazda w swoim' % len(IKONY))
    L.append('// kolorze - LV_IMG_CF_ALPHA_1BIT ma jeden kolor na obrazek). Wzor: zestaw "kolorowe')
    L.append('// plaskie" z docs/symulator_kafelka_zewn.html (wybor uzytkownika 23.11). Ktora ikona')
    L.append('// dla ktorego kodu WMO - patrz pogoda_ikona() w dashboard_lvgl_poc.cpp.')
    L.append('#include "ikony_lvgl.h"')
    L.append('')
    for n, (hexs, opis) in KOLORY.items():
        L.append('#define %-16s 0x%04X   // #%s %s' % (n, rgb565(hexs), hexs, opis))
    L.append('')
    for n, ww in w.items():
        b = bajty(ww)
        L.append('const uint8_t %s[] PROGMEM = {' % n)
        for r in range(R):
            L.append('  ' + ', '.join('0x%02X' % v for v in b[r * 5:(r + 1) * 5]) + ',')
        L.append('};')
    L.append('')
    # ikony_lvgl.h robi #undef LVGL_IKONA_DSC na koncu - tu wlasna, identyczna kopia makra.
    L.append('#define POG_IKONA_DSC(nazwa, dane) \\')
    L.append('  const lv_img_dsc_t nazwa = { .header = { .cf = LV_IMG_CF_ALPHA_1BIT, .always_zero = 0, .reserved = 0, \\')
    L.append('    .w = IKONA_ZEWN_ROZMIAR, .h = IKONA_ZEWN_ROZMIAR }, .data_size = sizeof(dane), .data = dane };')
    for n in w:
        L.append('POG_IKONA_DSC(lv_%s, %s)' % (n.lower(), n))
    L.append('#undef POG_IKONA_DSC')
    L.append('')
    L.append('// Ikona = 1-2 warstwy z kolorami. w2 == nullptr -> jedna warstwa.')
    L.append('struct IkonaPogody { const lv_img_dsc_t *w1; uint16_t k1; const lv_img_dsc_t *w2; uint16_t k2; };')
    L.append('enum PogodaIkona : uint8_t {')
    for n, opis, *_ in IKONY:
        L.append('  %s,   // %s' % (n, opis))
    L.append('  POG_IK_ILE')
    L.append('};')
    L.append('static const IkonaPogody IKONY_POGODY[POG_IK_ILE] = {')
    for n, opis, w1, k1, w2, k2 in IKONY:
        L.append('  { &lv_%s, %s, %s, %s },   // %s' % (w1.lower(), k1, ('&lv_' + w2.lower()) if w2 else 'nullptr', k2 if k2 else '0', n))
    L.append('};')
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src', 'ikony_pogoda.h')
    open(out, 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    print('zapisano %s: %d warstw, %d ikon' % (os.path.normpath(out), len(w), len(IKONY)))

if __name__ == '__main__':
    main()
