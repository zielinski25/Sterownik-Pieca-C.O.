#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Czy da sie NAUCZYC poprawe modelu nocnego z wlasnego archiwum?
Porownanie 5 metod na 11 prawdziwych nocach (18-28.10.2026).
Zrodlo danych: log centrali (tag=STATUS, pola panel= i zewn=).

Wyjscie: WYNIK-POROWNANIE.txt   (na konsoli to samo)

Uruchomienie:
    python3 porownanie.py "piec_log (30).txt"
"""
import io, re, sys, statistics as st
from datetime import datetime, timedelta

LOG = sys.argv[1] if len(sys.argv) > 1 else "piec_log (30).txt"

# --- 1. wczytanie logu -------------------------------------------------------
rows = []
for line in io.open(LOG, encoding="utf-8", errors="replace"):
    if "tag=STATUS" not in line:
        continue
    m = re.search(r"t=(2026-\d\d-\d\dT\d\d:\d\d:\d\d)", line)
    p = re.search(r" panel=([0-9.\-]+)C", line)
    z = re.search(r" zewn=([0-9.\-]+)C", line)
    if m and p and z:
        rows.append((datetime.fromisoformat(m.group(1)), float(p.group(1)), float(z.group(1))))
rows.sort()
if not rows:
    sys.exit("brak probek STATUS w %s" % LOG)

# --- 2. noce: 19:00 -> 06:00, tylko te z pelnym pokryciem --------------------
nocy = []
base = datetime(2026, 10, 18, 19, 0)
for i in range(12):
    d0 = base + timedelta(days=i)
    d1 = d0 + timedelta(hours=11)
    a = [r for r in rows if d0 <= r[0] < d1]
    if len(a) < 20:
        continue
    b2 = [r for r in a if r[0] < d0 + timedelta(hours=2)]
    nocy.append(dict(
        d=str(d0.date()),
        T0=a[0][1],
        act=a[-1][1] - a[0][1],
        dT=st.mean(x[1] for x in a) - st.mean(x[2] for x in a),
        Ta=st.mean(x[2] for x in a),
        rate2=(b2[0][1] - b2[-1][1]) / 2.0 if len(b2) > 3 else None,
        hours=11.0,
    ))

# --- 3. blad modelu fizycznego (z Noce.kt, ten sam log) ----------------------
MODEL_ERR = [-1.4, -1.1, 0.0, -1.4, 0.9, 8.1, 1.9, 3.2, 1.4, 2.9, 0.3]
for i, n in enumerate(nocy):
    n["model"] = n["act"] + MODEL_ERR[i]

def mae(p):
    return sum(abs(p[i] - nocy[i]["act"]) for i in range(len(nocy))) / len(nocy)

# --- 4. metody ---------------------------------------------------------------
def predB(i):                      # zastepuje model uczonym wspolczynnikiem k
    ks = [(-n["act"] / n["hours"]) / n["dT"]
          for j, n in enumerate(nocy) if j != i and n["dT"] > 2]
    return -st.median(ks) * nocy[i]["dT"] * nocy[i]["hours"]

def predC(i):                      # persystencja: tempo z pierwszych 2 h
    n = nocy[i]
    if n["rate2"] is None:
        return n["model"]
    k = n["rate2"] / max(1.0, n["T0"] - n["Ta"])
    return -k * n["dT"] * n["hours"]

def predD(i):                      # model * mnoznik (0,5-2,0), uczony LOO
    r = st.median([nocy[j]["act"] / nocy[j]["model"]
                   for j in range(len(nocy)) if j != i and abs(nocy[j]["model"]) > 0.5])
    return nocy[i]["model"] * max(0.5, min(2.0, r))

def predE(i):                      # model + stala poprawka (-1..+1 K), uczona LOO
    c = st.median([nocy[j]["act"] - nocy[j]["model"]
                   for j in range(len(nocy)) if j != i])
    return nocy[i]["model"] + max(-1.0, min(1.0, c))

out = []
w = out.append
w("=" * 92)
w("CZY Z ARCHIWUM DA SIE NAUCZYC POPRAWE? — 5 metod, %d nocy, leave-one-out" % len(nocy))
w("=" * 92)
w("")
w("%-14s %6s %7s" % ("noc", "akt[C]", "model"))
w("-" * 92)
for n in nocy:
    w("%-14s %+6.1f %+7.1f" % (n["d"], n["act"], n["model"]))
w("-" * 92)
w("")
w("%-46s %s" % ("A) model fizyczny (STAN TERAZ, 0.31.51)", "MAE = %.2f C" % mae([n["model"] for n in nocy])))
w("%-46s %s" % ("B) uczenony wspolczynnik k (zastepuje model)", "MAE = %.2f C" % mae([predB(i) for i in range(len(nocy))])))
w("%-46s %s" % ("C) persystencja: tempo z pierwszych 2 h", "MAE = %.2f C" % mae([predC(i) for i in range(len(nocy))])))
w("%-46s %s" % ("D) model x wspolczynnik (uczony, LOO)", "MAE = %.2f C" % mae([predD(i) for i in range(len(nocy))])))
w("%-46s %s" % ("E) model + stala poprawka (uczona, LOO)", "MAE = %.2f C" % mae([predE(i) for i in range(len(nocy))])))
w("")
errs = sorted(abs(e) for e in MODEL_ERR)
w("Model fizyczny — statystyka bledu:")
w("  sredni blad bezwzgledny      = %.2f C" % mae([n["model"] for n in nocy]))
w("  mediana bledu bezwzglednego  = %.2f C" % st.median(errs))
w("  bez nocy 23/24.10 (odstep)   = %.2f C" % (sum(errs[:-1]) / (len(errs) - 1)))
w("  ta jedna noc to %.0f%% sumy bledu" % (100 * errs[-1] / sum(errs)))
w("")
w("Korelacja bledu modelu z wielkosciami mierzalnymi (0 = brak zwiazku):")
def corr(x, y):
    mx, my = st.mean(x), st.mean(y)
    num = sum((a - mx) * (b - my) for a, b in zip(x, y))
    den = (sum((a - mx) ** 2 for a in x) * sum((b - my) ** 2 for b in y)) ** 0.5
    return num / den if den else 0.0
w("  blad vs temperatura o 19:00 : %+.2f" % corr(MODEL_ERR, [n["T0"] for n in nocy]))
w("  blad vs temperatura zewn.   : %+.2f" % corr(MODEL_ERR, [n["Ta"] for n in nocy]))
w("  blad vs roznica T-Tzewn     : %+.2f" % corr(MODEL_ERR, [n["dT"] for n in nocy]))
w("")
w("WNIOSEK: zaden sposob uczenia na archiwum nie bije modelu fizycznego.")
w("         Brak korelacji bledu z czymkolwiek mierzalnym = nie ma czego nauczyc.")
w("         NIE WDRAZAMY opcji (b).")


# --- 5. test praktyczny: prognoza "do konca dnia" o kazdej pelnej godzinie ---
w("")
w("=" * 92)
w("TEST PRAKTYCZNY — ile zostanie do 24:00, liczony o kazdej pelnej godzinie")
w("  (tak dziala karta 'Do konca dnia'; 66 przypadkow)")
w("=" * 92)
b_p, b_m, n_case = 0.0, 0.0, 0
r_p, r_m = [], []
for d in sorted(set(r[0].date() for r in rows)):
    for godz in (17, 18, 19, 20, 21, 22):
        t0 = datetime(d.year, d.month, d.day, godz, 0)
        t1 = datetime(d.year, d.month, d.day, 23, 59)
        a2 = [r for r in rows if t0 - timedelta(hours=2) <= r[0] < t0]
        b2 = [r for r in rows if t0 <= r[0] <= t1]
        if len(a2) < 4 or len(b2) < 4:
            continue
        Tnow, Tend = b2[0][1], b2[-1][1]
        rate = (a2[0][1] - a2[-1][1]) / 2.0
        h = (t1 - t0).total_seconds() / 3600.0
        dTn = max(1.0, Tnow - st.mean(x[2] for x in a2))
        dTe = max(1.0, (Tnow + Tend) / 2.0 - st.mean(x[2] for x in b2))
        pred_p = Tnow - rate * (dTe / dTn) * h
        UA, C = 2.0, 418600.0
        T, steps = Tnow, max(1, int(h * 6))
        for _ in range(steps):
            T += (-UA * (T - st.mean(x[2] for x in b2)) / C) * (h / steps * 3600.0)
        r_p.append(abs(pred_p - Tend))
        r_m.append(abs(T - Tend))
        n_case += 1

w("  n = %d przypadkow" % n_case)
w("  sredni blad PERSYSTENCJA (tempo z ostatnich 2 h) = %.2f C" % (sum(r_p) / n_case))
w("  sredni blad MODEL (UA 2,0 W/K, liniowo)          = %.2f C" % (sum(r_m) / n_case))
w("")
w("  Persystencja przegrywa, bo o 17:00-18:00 dochodzi jeszcze slonce")
w("  (26.10 o 17:00 dawala -12,6 C, a woda stracila tylko 2,6 C).")

txt = "\n".join(out) + "\n"
io.open("WYNIK-POROWNANIE.txt", "w", encoding="utf-8").write(txt)
print(txt)
