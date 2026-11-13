#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Czy testy z test_panel_hist.cpp zauwazylyby blad w kodzie v3.58.5?

Skrypt po kolei psuje jedna rzecz w kodzie wycietym z main_centrala.cpp
(zly prog, odwrocony warunek, pominieta regula - lista ponizej), kompiluje
testy z tak zepsutym kodem i sprawdza, czy testy to wykryly. Pliku centrali
nie zmienia: pracuje na kopii wycinka w katalogu tymczasowym.

    python3 sprawdz_czulosc.py                       # ../src/main_centrala.cpp
    python3 sprawdz_czulosc.py sciezka/main_centrala.cpp

Wynik 0 = kazdy z bledow zostal wykryty. Trwa ok. 2 minut (kazdy blad to
osobna kompilacja). Wymaga: g++ (C++17), python3.

To sprawdzenie TESTOW, nie kodu: mowi, ze reguly dodane w v3.58.5 sa
pilnowane przez co najmniej jedno sprawdzenie. Nie dowodzi, ze testy
wykryja kazdy mozliwy blad.
"""
import os
import shutil
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
CPP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(TU, '..', 'src', 'main_centrala.cpp')

# (opis bledu, tekst w kodzie centrali, tekst po zepsuciu). Kazdy tekst musi
# wystapic w wycinku dokladnie raz - inaczej skrypt przerywa (kod sie zmienil
# i liste trzeba dopasowac).
BLEDY = [
    ('okno zawsze od poczatku zakresu (jak gdyby nie bylo zmiany [1])',
     'const uint32_t poczatek = jestZPomiarem ? pierwszaZPomiarem : poczatekZakresu;',
     'const uint32_t poczatek = poczatekZakresu;'),
    ('odcinek konczacy sie dokladnie na poczatku zakresu liczony jako pomiar w zakresie',
     'if (b <= poczatekZakresu || a >= koniecZakresu) return;',
     'if (b < poczatekZakresu || a >= koniecZakresu) return;'),
    ('koniec okna: odstep rowny progowi juz przestawia okno na biezacy czas',
     'teraz > tMax && (teraz - tMax) > prog) koniecCzas = teraz;',
     'teraz > tMax && (teraz - tMax) >= prog) koniecCzas = teraz;'),
    ('koniec okna: progi 6 min i 90 min zamienione miejscami',
     'const uint32_t prog = najnowszyDlugi ? PANEL_HIST_PRZERWA_DLUGI_S : PANEL_HIST_PRZERWA_KROTKI_S;',
     'const uint32_t prog = najnowszyDlugi ? PANEL_HIST_PRZERWA_KROTKI_S : PANEL_HIST_PRZERWA_DLUGI_S;'),
    ('lekki odczyt czasu czyta sasiednia probke bufora krotkiego',
     'czas = panelHistCzasKrotkiej(z, k, historiaBufor[(z.startK + k) % HISTORIA_MAX_PROBEK].czas_unix, poprzedniCzasK);',
     'czas = panelHistCzasKrotkiej(z, k, historiaBufor[(z.startK + k + 1) % HISTORIA_MAX_PROBEK].czas_unix, poprzedniCzasK);'),
    ('odstep miedzy probkami rowny progowi liczony jako przerwa',
     'if (czasB > czasA && (czasB - czasA) <= prog) return czasB;',
     'if (czasB > czasA && (czasB - czasA) < prog) return czasB;'),
    ('zegar: czas letni liczony jak zimowy',
     'lok.tm_isdst = -1;   // CET/CEST dobiera mktime() wedlug strefy TZ centrali',
     'lok.tm_isdst = 0;'),
    ('zegar stosowany takze do probek z umownym czasem (rok 2000)',
     'if (jestCzas && tMax >= PANEL_HIST_CZAS_PEWNY) {',
     'if (jestCzas) {'),
    ('zegar wczesniejszy niz najnowsza probka uznany za pozniejszy',
     'teraz <= PANEL_HIST_CZAS_MAX && teraz > tMax && (teraz - tMax) > prog) koniecCzas = teraz;',
     'teraz <= PANEL_HIST_CZAS_MAX && (teraz - tMax) > prog) koniecCzas = teraz;'),
    ('koniec okna zawsze na najnowszej probce (jak gdyby nie bylo zmiany [2])',
     'teraz > tMax && (teraz - tMax) > prog) koniecCzas = teraz;',
     'teraz > tMax && (teraz - tMax) > prog) koniecCzas = tMax;'),
    ('naglowek: do= podaje czas probki zamiast konca okna',
     'jestCzas ? (unsigned long)koniecCzas : 0UL,',
     'jestCzas ? (unsigned long)tMax : 0UL,'),
    ('lekki odczyt czasu pomija regule granicy miedzy buforem dlugim i krotkim',
     '    if (!panelHistDlugiLiczySie(z, c)) return false;\n    czas = c;',
     '    if (c == 0 || c > PANEL_HIST_CZAS_MAX) return false;\n    czas = c;'),
    ('odczyt zegara bez sprawdzenia roku (zegar nieustawiony brany za prawdziwy czas)',
     'if (a[0] < 2024 || a[0] > 2099 ||',
     'if (a[0] < 1971 || a[0] > 2099 ||'),
    ('ostatni wiersz historii nie liczy sie przy szukaniu poczatku okna',
     '  if (jestP) panelHistNajwczesniejsza(czasP, czasP + 1U, poczatekZakresu, koniecOkna, jestZPomiarem, pierwszaZPomiarem);\n',
     ''),
]


def main():
    rob = tempfile.mkdtemp(prefix='centrala_czulosc_')
    try:
        wycinek = os.path.join(rob, 'panel_hist_wycinek.inc')
        subprocess.run([sys.executable, os.path.join(TU, 'wytnij_blok.py'), CPP, wycinek], check=True, stdout=subprocess.DEVNULL)
        baza = open(wycinek, encoding='utf-8').read()
        test = os.path.join(TU, 'test_panel_hist.cpp')

        def zbuduj_i_uruchom(tresc, nazwa):
            kat = os.path.join(rob, nazwa)
            os.makedirs(kat)
            open(os.path.join(kat, 'panel_hist_wycinek.inc'), 'w', encoding='utf-8').write(tresc)
            k = subprocess.run(['g++', '-std=gnu++17', '-O1', '-w', '-I', kat, '-o', os.path.join(kat, 't'), test],
                               capture_output=True, text=True)
            if k.returncode != 0:
                return None, k.stderr.strip().splitlines()[:1], ''
            u = subprocess.run([os.path.join(kat, 't')], capture_output=True, text=True)
            nieudane = [l.strip() for l in u.stdout.splitlines() if '[BLAD]' in l]
            ostatnia = (u.stdout.strip().splitlines() or [''])[-1]
            return u.returncode, nieudane, ostatnia

        print('Czulosc testow kodu [v3.58.4 PANEL-WYKRESY] (stan z v3.58.5): %s' % os.path.relpath(CPP, TU))
        kod, nieudane, ostatnia = zbuduj_i_uruchom(baza, 'bez_zmian')
        print('  kod bez zmian: %s' % ostatnia)
        if kod != 0:
            print('  Testy nie przechodza na niezmienionym kodzie - najpierw ./uruchom_testy.sh')
            return 2
        wykryte = 0
        for nr, (opis, stary, nowy) in enumerate(BLEDY, 1):
            if baza.count(stary) != 1:
                print('  %2d. %s\n        PRZERWANE: szukany tekst wystepuje w kodzie %d razy (ma byc 1)' % (nr, opis, baza.count(stary)))
                return 2
            kod, nieudane, ostatnia = zbuduj_i_uruchom(baza.replace(stary, nowy), 'blad_%02d' % nr)
            if kod is None:
                print('  %2d. %s\n        NIE KOMPILUJE SIE: %s' % (nr, opis, ' '.join(nieudane)[:160]))
            elif kod != 0 and nieudane:
                wykryte += 1
                liczba = ostatnia.split(',')[-1].strip() if ',' in ostatnia else ostatnia
                print('  %2d. %s\n        WYKRYTY (%s), np.: %s' % (nr, opis, liczba, nieudane[0].replace('[BLAD] ', '')[:110]))
            else:
                print('  %2d. %s\n        NIEWYKRYTY - testy przeszly mimo bledu (%s)' % (nr, opis, ostatnia))
        print('Wykryte: %d z %d celowo wprowadzonych bledow' % (wykryte, len(BLEDY)))
        return 0 if wykryte == len(BLEDY) else 1
    finally:
        shutil.rmtree(rob, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
