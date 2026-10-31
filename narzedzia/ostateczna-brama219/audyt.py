#!/usr/bin/env python3
"""219: ostateczna kontrola pliku, ktory uzytkownik wgra.

Ten etap nie dodaje nowej funkcji - sprawdza CALY plik po patchu v3.56.4:
  * czy plik jest poprawny strukturalnie (nawiasy, raw string, kodowanie),
  * czy strona WWW jest kompletna i zbalansowana (panel w srodku strony),
  * czy kazdy blok <script> to poprawny JavaScript (skladnia, node --check),
  * czy sa WSZYSTKIE poprawki z etapow 211-218 i czy ich nie ma w bazie
    v3.55.8 (matryca obecnosci: bez tego test bylby pusty),
  * czy zadna trasa nie jest martwa.

Wyjscie: JSON z lista kontroli (kod, nazwa, ok, szczegoly) + kod wyjscia 0/1.

Uruchomienie:
    python3 -B narzedzia/ostateczna-brama219/audyt.py main_centrala.cpp \\
        --baza <v3.55.8.cpp> --wersja v3.56.4 --wynik audyt.json [--js-katalog DIR]
"""
import argparse
import io
import json
import os
import re
import subprocess
import sys
import tempfile
from html.parser import HTMLParser

TU = os.path.dirname(os.path.abspath(__file__))

VOID = {'area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta',
        'param', 'source', 'track', 'wbr'}


# ─── lekser C++: kod bez komentarzy, stringow, znakow i raw stringow ──────
def lekser(tekst):
    """Zwraca (kod, widok, raw_stringi).

    'kod'  - komentarze, stringi, znaki i raw stringi zamienione na spacje
             (do liczenia nawiasow - zeby nawias w tekscie nie liczyl sie),
    'widok'- tylko komentarze zamienione na spacje (do szukania include'ow,
             nazw i wersji - tam stringi sa czescia skladni).
    Oba maja te sama dlugosc co 'tekst'.
    """
    wyjscie = io.StringIO()
    widok = io.StringIO()
    raw = []
    i = 0
    n = len(tekst)
    while i < n:
        c = tekst[i]
        # raw string: R"delim(...)delim"
        if c == 'R' and i + 1 < n and tekst[i + 1] == '"' and (i == 0 or not
                                                              (tekst[i - 1].isalnum() or tekst[i - 1] == '_')):
            k = tekst.find('(', i + 2)
            if k >= 0 and k - (i + 2) <= 16 and '\n' not in tekst[i + 2:k]:
                delim = tekst[i + 2:k]
                koniec = ')' + delim + '"'
                j = tekst.find(koniec, k + 1)
                if j >= 0:
                    j += len(koniec)
                    raw.append({'start': i, 'koniec': j, 'delim': delim})
                    wyjscie.write(' ' * (j - i))
                    widok.write(tekst[i:j])
                    i = j
                    continue
        # komentarz liniowy
        if c == '/' and i + 1 < n and tekst[i + 1] == '/':
            j = tekst.find('\n', i)
            if j < 0:
                j = n
            wyjscie.write(' ' * (j - i))
            widok.write(' ' * (j - i))
            i = j
            continue
        # komentarz blokowy
        if c == '/' and i + 1 < n and tekst[i + 1] == '*':
            j = tekst.find('*/', i + 2)
            j = n if j < 0 else j + 2
            wyjscie.write(' ' * (j - i))
            widok.write(' ' * (j - i))
            i = j
            continue
        # string
        if c == '"':
            j = i + 1
            while j < n:
                if tekst[j] == '\\':
                    j += 2
                    continue
                if tekst[j] == '"':
                    j += 1
                    break
                j += 1
            wyjscie.write(' ' * (j - i))
            widok.write(tekst[i:j])
            i = j
            continue
        # znak
        if c == "'":
            j = i + 1
            while j < n:
                if tekst[j] == '\\':
                    j += 2
                    continue
                if tekst[j] == "'":
                    j += 1
                    break
                if tekst[j] == '\n':
                    break
                j += 1
            wyjscie.write(' ' * (j - i))
            widok.write(tekst[i:j])
            i = j
            continue
        wyjscie.write(c)
        widok.write(c)
        i += 1
    return wyjscie.getvalue(), widok.getvalue(), raw


def balans(kod):
    """Sprawdza nawiasy w kodzie (bez stringow i komentarzy)."""
    pary = {')': '(', ']': '[', '}': '{'}
    stos = []
    for i, c in enumerate(kod):
        if c in '([{':
            stos.append((c, i))
        elif c in ')]}':
            if not stos:
                return {'ok': False, 'powod': 'zamkniecie %s bez otwarcia (offset %d)' % (c, i)}
            otwarty, pozycja = stos.pop()
            if otwarty != pary[c]:
                return {'ok': False, 'powod': 'niedopasowany %s (offset %d) do %s (offset %d)'
                                               % (c, i, otwarty, pozycja)}
    if stos:
        return {'ok': False, 'powod': '%d niezamknietych nawiasow, pierwszy %s (offset %d)'
                                      % (len(stos), stos[0][0], stos[0][1])}
    return {'ok': True, 'powod': ''}


# ─── HTML ────────────────────────────────────────────────────────────────
class ZbieraczHTML(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stos = []
        self.bledy = []
        self.licznik = {}
        self.skrypty = []
        self._w_skrypcie = 0
        self._bufor = None

    def handle_starttag(self, tag, attrs):
        atrybuty = dict(attrs)
        if tag in VOID:
            return
        self.stos.append((tag, self.getpos()))
        self.licznik[tag] = self.licznik.get(tag, 0) + 1
        if tag == 'script' and 'src' not in atrybuty:
            self._w_skrypcie += 1
            self._bufor = []

    def handle_endtag(self, tag):
        if tag in VOID:
            return
        if tag == 'script' and self._w_skrypcie:
            self._w_skrypcie -= 1
            self.skrypty.append(''.join(self._bufor or []))
            self._bufor = None
        if not self.stos:
            self.bledy.append(('zamkniecie bez otwarcia', tag, self.getpos()))
            return
        if self.stos[-1][0] == tag:
            self.stos.pop()
            return
        nazwy = [x[0] for x in self.stos]
        if tag in nazwy:
            while self.stos and self.stos[-1][0] != tag:
                self.bledy.append(('niedomkniety', self.stos[-1][0], self.stos[-1][1]))
                self.stos.pop()
            self.stos.pop()
        else:
            self.bledy.append(('zamkniecie bez otwarcia', tag, self.getpos()))

    def handle_data(self, dane):
        if self._bufor is not None:
            self._bufor.append(dane)


# ─── kontrole ────────────────────────────────────────────────────────────
class Audyt:
    def __init__(self):
        self.checks = []

    def dodaj(self, kod, nazwa, ok, szczegoly=''):
        self.checks.append({'kod': kod, 'nazwa': nazwa, 'ok': bool(ok), 'szczegoly': szczegoly})
        return ok

    @property
    def ok(self):
        return all(c['ok'] for c in self.checks)


def policz_w_tekscie(tekst, wzor):
    return len(re.findall(wzor, tekst, re.M))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('plik')
    ap.add_argument('--baza', default=None, help='plik bazowy v3.55.8 do matrycy obecnosci')
    ap.add_argument('--wersja', default='v3.56.4')
    ap.add_argument('--wynik', default=None)
    ap.add_argument('--js-katalog', default=None, help='gdzie zapisac bloki <script> do node --check')
    ap.add_argument('--bez-node', action='store_true')
    a = ap.parse_args()

    with open(a.plik, 'rb') as f:
        bajty = f.read()
    audyt = Audyt()

    # B01 kodowanie i konce linii
    try:
        tekst = bajty.decode('utf-8')
        utf8_ok = True
    except UnicodeDecodeError as e:
        tekst = bajty.decode('utf-8', 'replace')
        utf8_ok = False
        audyt.dodaj('B01', 'plik jest poprawnym UTF-8', False, str(e))
    if utf8_ok:
        problemy = []
        if b'\x00' in bajty:
            problemy.append('znak NUL')
        if b'\r\n' in bajty:
            problemy.append('konce linii CRLF')
        koncowka = 'nowa linia na koncu' if bajty.endswith(b'\n') else 'bez nowej linii na koncu'
        audyt.dodaj('B01', 'kodowanie i konce linii', not problemy,
                    (', '.join(problemy) + '; ' if problemy else '')
                    + 'UTF-8, LF, %d bajtow, %d linii, %s'
                    % (len(bajty), tekst.count('\n'), koncowka))

    kod, widok, raw = lekser(tekst)

    # B02 nawiasy
    b = balans(kod)
    audyt.dodaj('B02', 'nawiasy {} () [] zbalansowane w calym pliku', b['ok'], b['powod'])

    # B03 raw string strony - ten z delimitem PIECHTML
    raw_strony = [x for x in raw if x['delim'] == 'PIECHTML']
    if len(raw_strony) != 1:
        audyt.dodaj('B03', 'dokladnie jeden raw string strony (delimiter PIECHTML)', False,
                    'raw stringow %d, PIECHTML %d' % (len(raw), len(raw_strony)))
        tresc_strony = ''
    else:
        r = raw_strony[0]
        poprawny = tekst[r['koniec'] - 1] == '"' and r['delim'] == 'PIECHTML'
        tresc_strony = tekst[r['start']:r['koniec']]
        wewnetrzna = tresc_strony[len('R"PIECHTML('):-len(')PIECHTML"')]
        ma_html = wewnetrzna.lstrip().startswith('<!DOCTYPE html>') and \
            wewnetrzna.rstrip().endswith('</html>')
        audyt.dodaj('B03', 'dokladnie jeden raw string strony + DOCTYPE i </html>',
                    poprawny and ma_html,
                    'wszystkich raw stringow=%d, delim=%r, %d bajtow, DOCTYPE/</html> %s'
                    % (len(raw), r['delim'], len(wewnetrzna), 'sa' if ma_html else 'BRAK'))

    # B04 panel wewnatrz strony
    if raw_strony:
        r = raw_strony[0]
        pozycja_panelu = tekst.find('id="histBtn"')
        audyt.dodaj('B04', 'panel (id="histBtn") jest WEWNATRZ raw stringa strony',
                    r['start'] < pozycja_panelu < r['koniec'],
                    'panel offset %d, raw string %d..%d' % (pozycja_panelu, r['start'], r['koniec']))
    else:
        audyt.dodaj('B04', 'panel wewnatrz strony', False, 'brak raw stringa')

    # B05..B08 strona: budowa, tagi, skrypty
    if tresc_strony:
        wewn = tresc_strony[len('R"PIECHTML('):-len(')PIECHTML"')]
        p = ZbieraczHTML()
        p.feed(wewn)
        p.close()
        audyt.dodaj('B05', 'strona konczy sie </html> i ma DOCTYPE',
                    wewn.lstrip().startswith('<!DOCTYPE html>') and wewn.rstrip().endswith('</html>'),
                    repr(wewn.strip()[-20:]))
        problemy = list(p.bledy)
        problemy += [('niedomkniety na koncu', t, pos) for t, pos in p.stos]
        audyt.dodaj('B06', 'wszystkie tagi HTML zbalansowane', not problemy,
                    'bledow %d: %r' % (len(problemy), problemy[:5]) if problemy
                    else 'tagi: ' + ', '.join('%s=%d' % kv for kv in
                                              sorted(p.licznik.items(), key=lambda x: -x[1])[:6]))
        liczba_otwarć = len(re.findall(r'<script\b', wewn))
        liczba_zamkniec = len(re.findall(r'</script>', wewn))
        audyt.dodaj('B07', 'bloki <script> policzone i domkniete',
                    liczba_otwarć == liczba_zamkniec and len(p.skrypty) == liczba_otwarć
                    and all(s.strip() for s in p.skrypty),
                    'otwarciem=%d zamkniec=%d zebranych=%d'
                    % (liczba_otwarć, liczba_zamkniec, len(p.skrypty)))

        # B08 skladnia JS kazdego bloku
        if a.bez_node:
            audyt.dodaj('B08', 'skladnia JS (node --check)', True, 'pominiete (--bez-node)')
        else:
            katalog_js = a.js_katalog or tempfile.mkdtemp(prefix='js219-')
            os.makedirs(katalog_js, exist_ok=True)
            zle = []
            for i, blok in enumerate(p.skrypty, 1):
                sciezka = os.path.join(katalog_js, 'skrypt%02d.js' % i)
                with open(sciezka, 'w', encoding='utf-8') as f:
                    f.write(blok)
                t = subprocess.run(['node', '--check', sciezka], capture_output=True, text=True)
                if t.returncode != 0:
                    zle.append((i, t.stderr.strip().split('\n')[0][:160]))
            audyt.dodaj('B08', 'skladnia JS kazdego bloku <script> (node --check)', not zle,
                        'blokow %d, blednych %d %r' % (len(p.skrypty), len(zle), zle[:3])
                        if zle else 'blokow %d, wszystkie poprawne' % len(p.skrypty))
    else:
        for kod_b, nazwa in (('B05', 'strona kompletna'), ('B06', 'tagi HTML'),
                             ('B07', 'bloki <script>'), ('B08', 'skladnia JS')):
            audyt.dodaj(kod_b, nazwa, False, 'brak strony')

    # B09/B10 include'y
    poz_serwera = widok.find('#include <ESPAsyncWebServer.h>')
    poz_pobrania = widok.find('#include "HistoriaPobranie.h"')
    poz_biletow = widok.find('#include "HistoriaBilety.h"')
    poz_pina = widok.find('#include "HistoriaPin.h"')
    # Tylko te dwa naglowki potrzebuja typow z ESPAsyncWebServer.h (blad D1 z 214).
    # HistoriaPin.h jest samodzielny i celowo stoi wyzej.
    kolejnosc_ok = (0 <= poz_serwera < poz_pobrania and 0 <= poz_serwera < poz_biletow)
    audyt.dodaj('B09', 'ESPAsyncWebServer.h PRZED HistoriaPobranie/HistoriaBilety (D1 z 214)',
                kolejnosc_ok, 'serwer=%d pobranie=%d bilety=%d (pin=%d, samodzielny)'
                % (poz_serwera, poz_pobrania, poz_biletow, poz_pina))
    tylko_raz = {n: policz_w_tekscie(widok, r'^\s*#include\s+%s\s*$' % re.escape(n))
                 for n in ('<ESPAsyncWebServer.h>', '"HistoriaPobranie.h"',
                           '"HistoriaBilety.h"', '"HistoriaPin.h"')}
    audyt.dodaj('B10', 'kazdy z czterech include\'ow dokladnie raz',
                all(v == 1 for v in tylko_raz.values()), str(tylko_raz))

    # B11 wersja
    wersje = re.findall(r'^static const char\* FIRMWARE_VERSION = "(v[0-9.]+)";', widok, re.M)
    audyt.dodaj('B11', 'FIRMWARE_VERSION: dokladnie jedna definicja = %s' % a.wersja,
                len(wersje) == 1 and wersje[0] == a.wersja, str(wersje))

    # B12 duplikaty nowych globalnych
    duplikaty = []
    for wzor, opis in ((r'^static HistoriaBramka212 historiaBramka;', 'historiaBramka'),
                       (r'^static historia212::Pobranie historiaPobranie\(', 'historiaPobranie'),
                       (r'^static historia213::Bilety historiaBilety\(', 'historiaBilety'),
                       (r'^static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;',
                        'historiaBiletyPodpiecie')):
        liczba = policz_w_tekscie(widok, wzor)
        if liczba != 1:
            duplikaty.append('%s=%d' % (opis, liczba))
    audyt.dodaj('B12', 'kazda nowa zmienna globalna zdefiniowana dokladnie raz',
                not duplikaty, ', '.join(duplikaty) if duplikaty else '4 x 1')

    # B13/B14 trasy
    sys.path.insert(0, os.path.join(TU, '..', 'panel-trasa218'))
    import trasy as T  # noqa: E402  (narzedzia/panel-trasa218/trasy.py)
    tabela = T.wczytaj_trasy(tekst)
    martwe = T.przechwycone(tabela)
    his = [t for t in tabela if (t['sciezka'] or '').startswith('/history')]
    gole = [t for t in his if t['typ'] != 'exact']
    audyt.dodaj('B13', 'wszystkie trasy /history* przez exact()', not gole and len(his) == 3,
                'trasy /history*: %d, gole: %d' % (len(his), len(gole)))
    audyt.dodaj('B14', 'zadna trasa w pliku nie jest martwa (przechwycona)', not martwe,
                'tras %d, martwych %d %r' % (len(tabela), len(martwe),
                                             [(m['martwa'], m['przechwytuje']) for m in martwe]))

    # B15 brak znacznikow tymczasowych w kodzie
    znaczniki = 0
    for wzor in (r'\bTODO\b', r'\bFIXME\b', r'\bXXX\b', r'\bPLACEHOLDER\b', r'<<<<<<<'):
        znaczniki += len(re.findall(wzor, kod))
    audyt.dodaj('B15', 'brak znacznikow tymczasowych w kodzie (TODO/FIXME/XXX/konflikt)',
                znaczniki == 0, 'znaleziono %d' % znaczniki)

    # B16 matryca poprawek 211-218
    if a.baza:
        with open(a.baza, 'rb') as f:
            baza_tekst = f.read().decode('utf-8', 'replace')
        kod_bazy, widok_bazy, raw_bazy = lekser(baza_tekst)
        MATRYCA = [
            ('M01', '211 zapinka pliku', '#include "HistoriaPin.h"', 0, 1),
            ('M01b', '211 zapinka pliku (uzycie)', 'historia211::Pin', 0, 2),
            ('M02', '212 bramka trasy', 'struct HistoriaBramka212', 0, 1),
            ('M02b', '212 obsluga pobrania', 'historiaPobranie.obsluz', 0, 2),
            ('M03', '213 bilety', 'historia213::Bilety', 0, 1),
            ('M03b', '213 trasa biletu', 'historiaPobranie.link', 0, 1),
            ('M06', '218 exact na /history', 'AsyncURIMatcher::exact("/history")', 0, 1),
        ]
        braki = []
        for kod_m, opis, wzor, min_baza, min_nowy in MATRYCA:
            w_nowym = policz_w_tekscie(widok, re.escape(wzor))
            w_bazie = policz_w_tekscie(widok_bazy, re.escape(wzor))
            if w_nowym < min_nowy:
                braki.append('%s: brak w v3.56.4 (%d)' % (kod_m, w_nowym))
            elif w_bazie != min_baza and min_baza == 0:
                braki.append('%s: nieoczekiwanie jest juz w bazie (%d)' % (kod_m, w_bazie))
        bol = policz_w_tekscie(widok, r'wifiWebServer\.on\("/history')
        if bol != 0:
            braki.append('M06b: goly napis /history zostal (%d)' % bol)
        # M05: panel - znacznik jest w stronie (raw string), nie w kodzie
        if tresc_strony:
            for wzor_panelu, opis_panelu in (
                    ('var trasa = (strumien === \'long\') ? \'/history/long\' : \'/history\';',
                     'panel wybiera trase wedlug strumienia'),
                    ('id="histShort"', 'przycisk krotkiej historii'),
                    ('id="histLong"', 'przycisk dlugiej historii')):
                if wzor_panelu not in tresc_strony:
                    braki.append('M05: brak w panelu: %s' % opis_panelu)
        else:
            braki.append('M05: brak strony - nie ma czego sprawdzic')
        audyt.dodaj('B16', 'matryca poprawek 211-218: kazda jest i kazdej nie ma w bazie',
                    not braki, '; '.join(braki) if braki else '%d znacznikow, wszystkie na miejscu'
                    % len(MATRYCA))
    else:
        audyt.dodaj('B16', 'matryca poprawek 211-218', True, 'pominiete (brak --baza)')

    raport = {'plik': os.path.basename(a.plik), 'wersja': a.wersja,
              'ok': audyt.ok, 'kontrol': len(audyt.checks), 'checks': audyt.checks}
    tekst_wyniku = json.dumps(raport, indent=2, ensure_ascii=False) + '\n'
    if a.wynik:
        with open(a.wynik, 'w', encoding='utf-8') as f:
            f.write(tekst_wyniku)
    for c in audyt.checks:
        print('%s %s %s%s' % ('PASS' if c['ok'] else 'FAIL', c['kod'], c['nazwa'],
                              ('  [' + c['szczegoly'] + ']') if c['szczegoly'] else ''))
    print('%s audyt219 kontrol=%d bledow=%d'
          % ('PASS' if audyt.ok else 'FAIL', len(audyt.checks),
             sum(1 for c in audyt.checks if not c['ok'])))
    return 0 if audyt.ok else 1


if __name__ == '__main__':
    sys.exit(main())
