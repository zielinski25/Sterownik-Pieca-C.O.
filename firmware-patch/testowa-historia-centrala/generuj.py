#!/usr/bin/env python3
"""Przykladowe CSV do osobnej PUSTEJ centrali testowej; niczego nie wysyla."""
import argparse
import csv
from datetime import datetime
import math
from pathlib import Path
import time


def wartosci(epoch):
    """Ta sama funkcja dla obu buforow, aby pomiary w tym samym czasie byly zgodne."""
    minuta = epoch / 60.0
    powoli = math.sin(minuta * 2 * math.pi / 1440)
    piec = 57 + 12 * math.sin(minuta * 2 * math.pi / 180)
    return [
        5 + 7 * powoli,                      # zewn
        47 + 5 * math.sin(minuta * 2 * math.pi / 360),
        piec,                                # piec
        57 + 10 * math.sin((minuta - 15) * 2 * math.pi / 180),
        piec - 8,                            # powrot
        30 + 8 * powoli,                     # panel solarny
        21 + 1.5 * powoli,                   # pokoj
        1.5 + .2 * powoli,                   # cisnienie
        48 + 12 * powoli,                    # wilgotnosc
        round(60 + 35 * powoli),             # klapa (stopnie)
        round(35 + 20 * powoli),             # syberka (stopnie)
    ]


def stworz(katalog, teraz=None):
    katalog = Path(katalog)
    # Nigdy nie zastap czyjegos projektu ani plikow bez jawnej decyzji.
    katalog.mkdir(parents=True, exist_ok=False)
    teraz = int(time.time() if teraz is None else teraz)
    koniec = teraz - teraz % 60
    def lokalny(t):
        return datetime.fromtimestamp(t).astimezone()
    with (katalog / 'hist_short.csv').open('x', encoding='ascii', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        for i in range(1440):
            t = koniec - (1439 - i) * 60
            v = wartosci(t)
            pump = 1 if v[2] >= 57 else 0
            out.writerow([lokalny(t).strftime('%H:%M')] + [f'{x:.1f}' for x in v[:7]] +
                         [pump, f'{v[7]:.1f}', f'{v[8]:.0f}', v[9], v[10], t])
    with (katalog / 'hist_long.csv').open('x', encoding='ascii', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        for i in range(169):
            t = koniec - (168 - i) * 3600
            v = wartosci(t)
            triplets = []
            for j, x in enumerate(v):
                x = f'{x:.0f}' if j >= 8 else f'{x:.1f}'
                triplets.extend([x, x, x])
            out.writerow([lokalny(t).strftime('%d.%m %H:%M')] + triplets + [t, int(v[2] >= 57)])
    return koniec


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('katalog', nargs='?', default='historia_testowa', help='nowy, NIEISTNIEJACY folder wyjsciowy')
    args = parser.parse_args()
    try:
        end = stworz(args.katalog)
    except FileExistsError:
        parser.exit(1, 'BLAD: folder docelowy juz istnieje. Wybierz inna nazwe; niczego nie nadpisano.\n')
    print(f'Zapisano dwa pliki CSV w {args.katalog}; ostatnia probka: {datetime.fromtimestamp(end).astimezone():%Y-%m-%d %H:%M %z}.')
    print('DANE TESTOWE! Wysylaj tylko na osobna, odlaczona od instalacji plytke.')


if __name__ == '__main__':
    main()
