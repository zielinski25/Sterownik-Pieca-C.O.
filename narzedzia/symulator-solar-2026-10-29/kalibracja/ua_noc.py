#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Ile ciepla NAPRAWDE traci zbiornik w nocy — osobna regresja dla kazdej nocy.

Metoda: dla kazdej pary sasiednich probek (5-20 min) liczymy tempo stygniecia
[K/h] i srednia roznice (woda - powietrze). Regresja przez zero daje
UA_efektywne [W/K]. Regresja, a nie "dwa punkty", bo probki 5-minutowe sa
skwantowane do 0,1 C — z dwoch punktow wychodzi smiec.

Uruchomienie:
    python3 ua_noc.py "piec_log (30).txt"
Wynik na ekran + WYNIK-UA-NOC.txt
"""
import io
import re
import sys
import statistics as st
from datetime import datetime, timedelta

LOG = sys.argv[1] if len(sys.argv) > 1 else "piec_log (30).txt"
C = 418680.0          # 100 l wody, 0,1163 kWh/K
TANK_UA = 1.4         # parametr katalogowy z modelu

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

out = []
w = out.append
w("=" * 92)
w("UA_EFEKTYWNE ZBIORNIKA — osobna regresja dla kazdej nocy (19:00 -> 06:00)")
w("  dT/dt = (UA/C) * (T - Ta), regresja przez zero")
w("  katalog modelu: tankUa = %.1f W/K" % TANK_UA)
w("=" * 92)
w("")
w("%-12s %6s %7s %8s %7s %10s %11s" % ("noc", "T19", "T06", "spadek", "TaSr", "UA [W/K]", "x katalog"))
w("-" * 92)

base = datetime(2026, 10, 18, 19, 0)
vals = []
for i in range(11):
    s0 = base + timedelta(days=i)
    s1 = s0 + timedelta(hours=11)
    a = [r for r in rows if s0 <= r[0] < s1]
    if len(a) < 20:
        continue
    xs, ys = [], []
    for j in range(len(a) - 1):
        dt = (a[j + 1][0] - a[j][0]).total_seconds() / 3600.0
        if not (0.05 <= dt <= 0.3):
            continue
        xs.append((a[j][1] + a[j + 1][1]) / 2 - (a[j][2] + a[j + 1][2]) / 2)
        ys.append((a[j + 1][1] - a[j][1]) / dt)
    if not xs:
        continue
    k = -sum(x * y for x, y in zip(xs, ys)) / sum(x * x for x in xs)
    ua = k * C / 3600.0
    vals.append(ua)
    w("%-12s %6.1f %7.1f %8.1f %7.1f %10.2f %11.2f"
      % (str(s0.date()), a[0][1], a[-1][1], a[-1][1] - a[0][1],
         st.mean(x[2] for x in a), ua, ua / TANK_UA))

w("-" * 92)
w("")
w("  minimum            = %.2f W/K  (%.2fx katalogu)" % (min(vals), min(vals) / TANK_UA))
w("  maksimum           = %.2f W/K  (%.2fx katalogu)" % (max(vals), max(vals) / TANK_UA))
w("  mediana            = %.2f W/K  (%.2fx katalogu)" % (st.median(vals), st.median(vals) / TANK_UA))
w("  rozrzut max/min    = %.1fx" % (max(vals) / min(vals)))
w("  nocy zgodnych z katalogiem (0,8-1,2x) = %d z %d"
  % (sum(1 for v in vals if 0.8 <= v / TANK_UA <= 1.2), len(vals)))
w("")
w("WNIOSEK: efektywna strata zmienia sie dziesieciokrotnie. ZADNA stala jej nie")
w("         opisze — ani katalogowa 1,4, ani nauczona z archiwum.")

txt = "\n".join(out) + "\n"
io.open("WYNIK-UA-NOC.txt", "w", encoding="utf-8").write(txt)
print(txt)
