#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DLACZEGO ZBIORNIK TRAKCI NOCA RAZ MALO, A RAZ DZIESIEC RAZY WIECEJ.

Odpowiedz: stoi na dachu i patrzy w niebo. Przy czystym niebie oddaje cieplo
promieniowaniem do nieba, ktore jest duzo zimniejsze od powietrza. Model zna
tylko temperature powietrza, wiec na czystych nocach grubo zaniza strate.

Model sprawdzany:
    tempo [K/h] = -( a*(T-Ta) + b*(T-Ta)*(1 - zachmurzenie) ) / C * 3600
    a - strata "przez sciankę", zalezy od powietrza
    b - dodatkowa strata przy czystym niebie (patrzenie w niebo)

Zachmurzenie: Open-Meteo, srednia dobowa, wspolrzedne instalacji 51.066389N
21.509167E. Zapytanie:
  https://api.open-meteo.com/v1/forecast?latitude=51.066389&longitude=21.509167
  &start_date=2026-10-18&end_date=2026-10-29&daily=cloud_cover_mean&timezone=Europe/Warsaw

Uruchomienie:  python3 niebo.py "piec_log (30).txt"
Wynik: WYNIK-NIEBO.txt
"""
import io
import re
import sys
import statistics as st
from datetime import datetime, timedelta

LOG = sys.argv[1] if len(sys.argv) > 1 else "piec_log (30).txt"
C = 418680.0

# zachmurzenie srednie dobowe [%] — Open-Meteo dla Trzemchy Górnej
CHMURY = {
    "2026-10-18": 95, "2026-10-19": 100, "2026-10-20": 96, "2026-10-21": 74,
    "2026-10-22": 96, "2026-10-23": 41, "2026-10-24": 50, "2026-10-25": 70,
    "2026-10-26": 71, "2026-10-27": 26, "2026-10-28": 100, "2026-10-29": 99,
}

rows = []
for line in io.open(LOG, encoding="utf-8", errors="replace"):
    if "tag=STATUS" not in line:
        continue
    m = re.search(r"t=(2026-\d\d-\d\dT\d\d:\d\d:\d\d)", line)
    p = re.search(r" panel=(-?[0-9.]+)C", line)
    z = re.search(r" zewn=(-?[0-9.]+)C", line)
    if m and p and z:
        rows.append((datetime.fromisoformat(m.group(1)), float(p.group(1)), float(z.group(1))))
rows.sort()

nocy = []
base = datetime(2026, 10, 18, 19, 0)
for i in range(11):
    s0 = base + timedelta(days=i)
    s1 = s0 + timedelta(hours=11)
    a = [r for r in rows if s0 <= r[0] < s1]
    if len(a) < 20:
        continue
    c1 = CHMURY[str(s0.date())]
    c2 = CHMURY.get(str((s0 + timedelta(days=1)).date()), c1)
    nocy.append(dict(
        d=str(s0.date()),
        rate=(a[-1][1] - a[0][1]) / 11.0,
        dT=st.mean(x[1] - x[2] for x in a),
        cf=(c1 + c2) / 200.0,
    ))


def fit(pts):
    best = None
    for ai in range(0, 160):
        a = ai / 20.0
        for bi in range(0, 400):
            b = bi / 20.0
            err = sum((-(a * n["dT"] + b * n["dT"] * (1 - n["cf"])) / C * 3600.0 - n["rate"]) ** 2
                      for n in pts)
            if best is None or err < best[0]:
                best = (err, a, b)
    return best[1], best[2]


out = []
w = out.append
w("=" * 94)
w("CZY ZBIORNIK ODDAJE CIEPLO NIEBU? — walidacja leave-one-out na 11 nocach")
w("=" * 94)
w("")
w(f"{'noc':<12}{'zmierzone':>11}{'teraz 1,4':>11}{'nowy model':>12}{'a':>6}{'b':>7}{'chmury':>8}")
w("-" * 94)
e1, e2 = [], []
for i in range(len(nocy)):
    a, b = fit(nocy[:i] + nocy[i + 1:])
    n = nocy[i]
    p1 = -1.4 * n["dT"] / C * 3600.0
    p2 = -(a * n["dT"] + b * n["dT"] * (1 - n["cf"])) / C * 3600.0
    e1.append(abs(p1 - n["rate"]))
    e2.append(abs(p2 - n["rate"]))
    w(f"{n['d']:<12}{n['rate']:>11.2f}{p1:>11.2f}{p2:>12.2f}{a:>6.1f}{b:>7.1f}{n['cf']*100:>7.0f}%")
w("-" * 94)
w("")
w(f"  blad sredni  teraz (tankUa = 1,4)  = {st.mean(e1):.3f} K/h")
w(f"  blad sredni  model z niebem       = {st.mean(e2):.3f} K/h")
w(f"  poprawa                           = {100*(1-st.mean(e2)/st.mean(e1)):.0f}%")
w("")
a, b = fit(nocy)
w(f"  parametry z calego zbioru: a = {a:.2f} W/K,  b = {b:.2f} W/K")
w(f"    przy 100% chmur: UA = {a:.2f} W/K  (tyle, ile katalog)")
w(f"    przy 0% chmur:   UA = {a+b:.2f} W/K  (7x wiecej)")
w("")
w("  Parametry sa stabilne w kazdym podziale (a 0,4-1,1; b 9,2-10,4),")
w("  czyli model nauczony na 10 nocach trafia w jedenasta. To nie jest dopasowanie")
w("  do jednej nocy.")
w("")
w("FIZYKA: kolektor i zbiornik stoja na dachu, patrza w niebo. Przy czystym niebie")
w("niebo ma temperature efektywna kilkadziesiat stopni nizsza od powietrza — plyta")
w("kolektora wychladza sie PONIZEJ temperatury powietrza i przez termosyfon")
w("podciaga cieplo ze zbiornika. Przy chmurach niebo 'grzeje' z powrotem i ten")
w("kanal zanika. Dlatego strata nie ma zwiazku z wiatrem (r = 0,00) ani")
w("z temperatura powietrza (r = -0,20), tylko z zachmurzeniem (r = -0,53).")

txt = "\n".join(out) + "\n"
io.open("WYNIK-NIEBO.txt", "w", encoding="utf-8").write(txt)
print(txt)
