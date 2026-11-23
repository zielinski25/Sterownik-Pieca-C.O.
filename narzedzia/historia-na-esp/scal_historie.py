#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
scal_historie.py - scala pobrane z centrali pliki historii (hist_short*.csv,
hist_long*.csv - dowolna liczba kopii, np. "hist_long (5).csv") w DWA pliki
gotowe do wgrania na LittleFS nowej plytki ESP32-S3:

    data/hist_short.csv   bufor krotki  - ostatnie 1440 wierszy (24 h co 60 s)
    data/hist_long.csv    bufor dlugi   - ostatnie 8000 wierszy (archiwum)

Format i limity sa DOKLADNIE takie, jakich oczekuje firmware centrali
(main_centrala.cpp, historiaWczytajZFS() / historiaDlugaWczytajZFS()):
  * krotki: 14 kolumn  etykieta(HH:MM),t_zewn,t_bojler,t_ogrz,t_ogrz_sr,t_powrot,
            t_panel,t_pokoj,pompa,cisnienie,wilgotnosc,klapa,syberko,czas_unix
  * dlugi : 35 kolumn (stary format) albo 36 (nowy, ostatnia = pompa)
            etykieta(DD.MM HH:MM), 9 x (sr,min,max), klapa(sr,min,max),
            syberko(sr,min,max), czas_unix[, pompa]
  * brak naglowka, LF na koncu kazdego wiersza, bez pustych linii
  * firmware trzyma 1440 / 8000 ostatnich wierszy (pierscien), czyta najwyzej
    512 kB / 2 MB od KONCA pliku - dlatego plik jest przycinany do tych limitow
Wiersze sa rozpoznawane po liczbie kolumn (nazwa pliku nie ma znaczenia),
laczone po czas_unix (duplikaty z roznych kopii odrzucane), sortowane rosnaco.
Tresc wierszy NIE jest zmieniana (firmware ma wlasna normalizacje starych
znacznikow czasu - historiaNormalizujLegacyUnix()).

Uzycie:
    python scal_historie.py <katalog_lub_pliki...> [-o data] [--krotki N] [--dlugi N]
    python scal_historie.py "C:\\Users\\Rafcio\\Downloads"
Wynik: katalog data/ z dwoma plikami + raport na ekranie. Kod wyjscia 0 = OK.
"""
import argparse
import glob
import os
import sys
from datetime import datetime, timezone, timedelta

KROTKI_KOL = 14
DLUGI_KOL = (35, 36)
KROTKI_MAX = 1440          # HISTORIA_MAX_PROBEK
DLUGI_MAX = 8000           # HISTORIA_DLUGA_MAX_PROBEK
KROTKI_LIMIT_B = 512 * 1024    # HISTORIA_WCZYTAJ_LIMIT_BAJTOW
DLUGI_LIMIT_B = 2048 * 1024    # HISTORIA_DLUGA_WCZYTAJ_LIMIT_BAJTOW
LINIA_MAX_B = 512              # bezpieczny margines (HISTORIA_STRUMIEN_LINIA_MAX_BAJTOW w firmware jest wiekszy)
CZAS_MIN = 1600000000          # 2020-09 - wczesniejsze czasy = brak RTC / smieci
CZAS_MAX = 1900000000          # 2030-03


def czas_pl(unix):
    # etykiety w plikach sa lokalne (CET/CEST); do raportu wystarczy przyblizenie UTC+1/+2
    t = datetime.fromtimestamp(unix, tz=timezone.utc)
    # prosty DST: ostatnia niedziela marca .. ostatnia niedziela pazdziernika
    rok = t.year
    def ost_niedziela(m):
        d = datetime(rok, m, 31, 1, 0, tzinfo=timezone.utc)
        while d.weekday() != 6:
            d -= timedelta(days=1)
        return d
    lato = ost_niedziela(3) <= t < ost_niedziela(10)
    return (t + timedelta(hours=2 if lato else 1)).strftime('%d.%m.%Y %H:%M')


def wczytaj_plik(sciezka, raport):
    """Zwraca liste (rodzaj, czas_unix, linia) z jednego pliku."""
    wiersze = []
    try:
        with open(sciezka, 'rb') as f:
            dane = f.read()
    except OSError as e:
        raport['bledy'].append('%s: nie mozna odczytac (%s)' % (sciezka, e))
        return wiersze
    if b'\x00' in dane:
        raport['uwagi'].append('%s: zawiera bajty zerowe (uszkodzony fragment) - takie wiersze pominieto' % os.path.basename(sciezka))
    tekst = dane.decode('utf-8', errors='replace')
    st = {'krotki': 0, 'dlugi': 0, 'odrzucone': 0, 'bez_czasu': 0}
    for surowa in tekst.split('\n'):
        linia = surowa.rstrip('\r')
        if not linia.strip():
            continue
        if '\x00' in linia or '\ufffd' in linia or len(linia.encode('utf-8')) > LINIA_MAX_B:
            st['odrzucone'] += 1
            continue
        kol = linia.split(',')
        n = len(kol)
        if n == KROTKI_KOL:
            rodzaj, idx_czasu = 'krotki', 13
        elif n in DLUGI_KOL:
            rodzaj, idx_czasu = 'dlugi', 34
        else:
            st['odrzucone'] += 1
            continue
        try:
            czas = int(kol[idx_czasu].strip())
        except ValueError:
            st['odrzucone'] += 1       # np. linia naglowka
            continue
        # pola liczbowe musza byc liczbami (toFloat w firmware zrobi z tekstu 0 - lepiej odrzucic tu)
        ok = True
        for v in kol[1:idx_czasu]:
            v = v.strip()
            if v == '':
                ok = False; break
            try:
                float(v)
            except ValueError:
                ok = False; break
        if not ok:
            st['odrzucone'] += 1
            continue
        if not (CZAS_MIN <= czas <= CZAS_MAX):
            st['bez_czasu'] += 1
            continue
        st[rodzaj] += 1
        wiersze.append((rodzaj, czas, linia))
    raport['pliki'].append((os.path.basename(sciezka), len(dane), st))
    return wiersze


def scal(wszystkie, rodzaj, maks, limit_b):
    wybrane = [(c, l) for r, c, l in wszystkie if r == rodzaj]
    po_czasie = {}
    dubli = 0
    konflikty = 0
    for czas, linia in wybrane:
        if czas in po_czasie:
            dubli += 1
            if po_czasie[czas] != linia:
                konflikty += 1
                # ten sam czas, inna tresc: zostaw wiersz z wieksza liczba kolumn (nowszy format), potem dluzszy
                if (len(linia.split(',')), len(linia)) > (len(po_czasie[czas].split(',')), len(po_czasie[czas])):
                    po_czasie[czas] = linia
            continue
        po_czasie[czas] = linia
    posortowane = [po_czasie[c] for c in sorted(po_czasie)]
    uciete = 0
    if len(posortowane) > maks:
        uciete = len(posortowane) - maks
        posortowane = posortowane[-maks:]
    # limit bajtow czytany przez firmware od konca - przytnij od poczatku, gdyby mimo wszystko przekroczony
    while posortowane and sum(len(l.encode('utf-8')) + 1 for l in posortowane) > limit_b:
        posortowane.pop(0)
        uciete += 1
    return posortowane, len(wybrane), dubli, konflikty, uciete


def main():
    ap = argparse.ArgumentParser(description='Scal pliki historii centrali w data/hist_short.csv i data/hist_long.csv')
    ap.add_argument('zrodla', nargs='+', help='katalogi (szukane *.csv) i/lub pliki')
    ap.add_argument('-o', '--wyjscie', default='data', help='katalog wyjsciowy (domyslnie data)')
    ap.add_argument('--krotki', type=int, default=KROTKI_MAX, help='ile ostatnich wierszy bufora krotkiego (domyslnie 1440)')
    ap.add_argument('--dlugi', type=int, default=DLUGI_MAX, help='ile ostatnich wierszy bufora dlugiego (domyslnie 8000)')
    a = ap.parse_args()

    pliki = []
    for z in a.zrodla:
        if os.path.isdir(z):
            pliki += sorted(glob.glob(os.path.join(z, '*.csv')) + glob.glob(os.path.join(z, '*.CSV')))
        elif os.path.isfile(z):
            pliki.append(z)
        else:
            print('BLAD: nie ma takiego pliku/katalogu: %s' % z)
            return 2
    pliki = sorted(set(os.path.abspath(p) for p in pliki))
    if not pliki:
        print('BLAD: nie znaleziono zadnego pliku .csv')
        return 2

    raport = {'pliki': [], 'bledy': [], 'uwagi': []}
    wszystkie = []
    for p in pliki:
        wszystkie += wczytaj_plik(p, raport)

    print('PLIKI WEJSCIOWE')
    for nazwa, bajty, st in raport['pliki']:
        print('  %-28s %8d B  krotkich=%-5d dlugich=%-5d odrzuconych=%-4d bez_czasu=%d' % (
            nazwa, bajty, st['krotki'], st['dlugi'], st['odrzucone'], st['bez_czasu']))
    for u in raport['uwagi']:
        print('  UWAGA: ' + u)
    for b in raport['bledy']:
        print('  BLAD: ' + b)

    os.makedirs(a.wyjscie, exist_ok=True)
    kod = 0
    for rodzaj, nazwa, maks, limit_b in (('krotki', 'hist_short.csv', a.krotki, KROTKI_LIMIT_B),
                                        ('dlugi', 'hist_long.csv', a.dlugi, DLUGI_LIMIT_B)):
        wiersze, zebrane, dubli, konflikty, uciete = scal(wszystkie, rodzaj, maks, limit_b)
        sciezka = os.path.join(a.wyjscie, nazwa)
        with open(sciezka, 'wb') as f:
            for l in wiersze:
                f.write(l.encode('utf-8') + b'\n')
        rozmiar = os.path.getsize(sciezka)
        print('\n%s  (%s)' % (nazwa, 'bufor krotki, 24 h' if rodzaj == 'krotki' else 'bufor dlugi, archiwum'))
        print('  zebrano %d wierszy ze wszystkich kopii, duplikatow (ten sam czas_unix) %d%s, przycieto najstarszych %d'
              % (zebrane, dubli, (' - w tym %d o roznej tresci' % konflikty) if konflikty else '', uciete))
        print('  ZAPISANO %d wierszy, %d B' % (len(wiersze), rozmiar))
        if wiersze:
            c0 = int(wiersze[0].split(',')[-1 if rodzaj == 'krotki' else 34])
            c1 = int(wiersze[-1].split(',')[-1 if rodzaj == 'krotki' else 34])
            print('  zakres: %s  ->  %s' % (czas_pl(c0), czas_pl(c1)))
            # przerwy > 6 h w buforze dlugim warto znac (wykres tydzien/miesiac pokaze dziure)
            if rodzaj == 'dlugi':
                czasy = [int(l.split(',')[34]) for l in wiersze]
                przerwy = [(czasy[i - 1], czasy[i]) for i in range(1, len(czasy)) if czasy[i] - czasy[i - 1] > 6 * 3600]
                if przerwy:
                    print('  przerwy dluzsze niz 6 h (brak danych w zrodlach): %d' % len(przerwy))
                    for p0, p1 in przerwy[:12]:
                        print('    %s -> %s' % (czas_pl(p0), czas_pl(p1)))
                    if len(przerwy) > 12:
                        print('    ... i %d kolejnych' % (len(przerwy) - 12))
        else:
            print('  UWAGA: brak wierszy - plik pusty')
            kod = 1
        if rozmiar > limit_b:
            print('  BLAD: plik wiekszy niz limit firmware (%d B)' % limit_b)
            kod = 1
    print('\nGOTOWE - skopiuj katalog "%s" do katalogu projektu centrali i wgraj: pio run -e esp32-s3-n16r8 -t uploadfs'
          % a.wyjscie if kod == 0 else '\nZAKONCZONE Z BLEDAMI - patrz wyzej')
    return kod


if __name__ == '__main__':
    sys.exit(main())
