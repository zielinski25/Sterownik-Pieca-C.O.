#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Analiza eksportu RTDB (telemetria V1, 11 kanalow) - wlasciwosci cieplne pieca C.O.

Wejscie : eksport JSON z Firebase RTDB. Skrypt czyta rekurencyjnie kazdy wezel,
          ktory ma pola "ts" i "a" - struktura katalogow nie ma znaczenia.
Wyjscie : raport tekstowy na stdout.

Kanally (kolejnosc V1, firmware main_centrala.cpp):
  0 zewn, 1 bojler, 2 ogrz (zasilanie), 3 ogrz_sr, 4 powrot,
  5 panel (solar), 6 pokoj, 7 trociny, 8 RH, 9 cisnienie, 10 dym (ADC)

Jednostki: temperatury x0.1 C, RH x0.1 %, cisnienie x0.1 hPa, dym surowy ADC 0-4095.
stateBits: bit0=1 pompa C.O. ON, bit1=2 alarm dymu, bit2=4 czujnik dymu aktywny.

Uruchomienie:
  python3 narzedzia/analiza_pieca.py [sciezka.json]
"""
import json
import sys
import math
import datetime
import statistics as st

# --------------------------------------------------------------- mapa kanalow
# Kolejnosc kanalow w rekordzie RTDB (pole "a") - patrz MAPA_CZUJNIKOW.md.
# Wszystkie temperatury to DS18B20 na trzech magistralach 1-Wire:
#   BUS_1 (GPIO4): pokoj, zewn
#   BUS_2 (GPIO5): ogrz, powrot, bojler, trociny   <- wszystko przy piecu
#   BUS_3 (GPIO7): panel                            <- osobno, kabel na dach
#
# Gdzie fizycznie siedzi kazdy czujnik:
#   0 zewn    - na zewnatrz, na scianie
#   1 bojler  - zbiornik CWU pieca (korelacja z zasilaniem r=0.84)
#   2 ogrz    - RURA WYJSCIOWA z pieca C.O., ZA miejscem wpiecia rury z
#               plaszcza przystawki na trociny. Mierzy wode po zmieszaniu
#               dwoch zrodel (kociol + przystawka), nie temperature paleniska.
#               Gdy przystawka nie pracuje (tak bylo w calym okresie danych),
#               to po prostu woda z kotla.
#   3 ogrz_sr - NIE JEST CZUJNIKIEM. Srednia z "ogrz" liczona tylko gdy pompa
#               pracuje (firmware v2.4.0). Do analizy uzywaj "ogrz".
#   4 powrot  - rura powrotna, przed kotlem
#   5 panel   - zbiornik solarny 100 l na dachu (termosyfon, bez pompy)
#   6 pokoj   - pokoj
#   7 trociny - W PRZYSTAWCE NA TROCINY (przystawka stoi przy piecu C.O.,
#               ma plaszcz wodny, a jej plomien korytarzem kilkucentymetrowym
#               ogrzewa ruszta pieca C.O.). Mediana 13.5 C = przystawka
#               NIE BYLA UZYWANA; wartosci to temperatura otoczenia.
#   8 rh      - DHT22, wilgotnosc (miejsce do potwierdzenia - mediana 92.9%)
#   9 cisn    - BMP280, cisnienie
#  10 dym     - czujnik dymu w kanale, ADC. UWAGA: stale tlo 240-300,
#               wiec "dym > 0" nie znaczy "sie pali". Prog ~400.
CH = dict(zewn=0, bojler=1, ogrz=2, ogrz_sr=3, powrot=4, panel=5,
          pokoj=6, trociny=7, rh=8, cisn=9, dym=10)


def utc(t):
    return datetime.datetime.utcfromtimestamp(t)


# ---------------------------------------------------------------- wczytywanie
def load(path):
    raw = json.load(open(path, encoding="utf-8"))
    recs = {}

    def walk(node):
        if not isinstance(node, dict):
            return
        if "ts" in node and "a" in node:
            recs[node["ts"]] = node
            return
        for v in node.values():
            walk(v)

    walk(raw)
    return recs, sorted(recs)


def to_series(recs, ts, ch):
    """Wartosci w jednostkach naturalnych; None = brak odczytu."""
    out = []
    for t in ts:
        v = recs[t]["a"][ch]
        if v is None or v == 65535:
            out.append(None)
        elif ch == CH["dym"]:
            out.append(float(v))      # 0 to prawidlowa wartosc (brak dymu)
        elif v <= 0:
            out.append(None)
        else:
            out.append(v / 10.0)
    return out


def hampel(vals, k=4, thr=6.0):
    """Usuwa pojedyncze skoki (artefakty czujnika): |v - mediana okna| > thr."""
    n = len(vals)
    out = list(vals)
    for i in range(n):
        if vals[i] is None:
            continue
        w = [vals[j] for j in range(max(0, i - k), min(n, i + k + 1))
             if vals[j] is not None]
        if len(w) < 3:
            continue
        if abs(vals[i] - st.median(w)) > thr:
            out[i] = None
    return out


def smooth(vals, k=2):
    n = len(vals)
    out = []
    for i in range(n):
        w = [vals[j] for j in range(max(0, i - k), min(n, i + k + 1)) if vals[j] is not None]
        out.append(st.median(w) if w else None)
    return out


def clean(recs, ts, ch, k=2, thr=6.0):
    return smooth(hampel(to_series(recs, ts, ch), 4, thr), k)


# ---------------------------------------------------------------- matematyka
def linreg(x, y):
    n = len(x)
    if n < 3:
        return None
    mx = sum(x) / n
    my = sum(y) / n
    sxx = sum((xi - mx) ** 2 for xi in x)
    if sxx == 0:
        return None
    sxy = sum((x[i] - mx) * (y[i] - my) for i in range(n))
    a = sxy / sxx
    b = my - a * mx
    ss_tot = sum((yi - my) ** 2 for yi in y)
    ss_res = sum((y[i] - (a * x[i] + b)) ** 2 for i in range(n))
    return a, b, (1 - ss_res / ss_tot if ss_tot > 0 else 0.0), n


def fit_exp(t, T):
    """T(t)=Tinf+(T0-Tinf)exp(-t/tau). Siatka po Tinf + regresja ln(T-Tinf) vs t."""
    n = len(t)
    if n < 20:
        return None
    best = None
    lo, hi = min(T) - 10.0, min(T) + 3.0
    for k in range(261):
        Tinf = lo + (hi - lo) * k / 260.0
        xs, ys, ok = [], [], True
        for i in range(n):
            d = T[i] - Tinf
            if d <= 0.1:
                ok = False
                break
            xs.append(t[i])
            ys.append(math.log(d))
        if not ok:
            continue
        r = linreg(xs, ys)
        if r is None or r[0] >= 0:
            continue
        slope, inter = r[0], r[1]
        ss = sum((ys[i] - (slope * xs[i] + inter)) ** 2 for i in range(n))
        if best is None or ss < best[0]:
            best = (ss, -1.0 / slope, Tinf, math.exp(inter) + Tinf, r[2], n)
    return None if best is None else best[1:]


def corr_lag(x, y, maxlag):
    best = None
    for lag in range(0, maxlag + 1):
        if len(x) - lag < 60:
            break
        a = x[: len(x) - lag]
        b = y[lag:]
        mx = sum(a) / len(a)
        my = sum(b) / len(b)
        sxx = sum((v - mx) ** 2 for v in a)
        syy = sum((v - my) ** 2 for v in b)
        if sxx == 0 or syy == 0:
            continue
        c = sum((a[i] - mx) * (b[i] - my) for i in range(len(a))) / math.sqrt(sxx * syy)
        if best is None or c > best[1]:
            best = (lag, c)
    return best


def main(path):
    recs, ts = load(path)
    N = len(ts)
    S = {n: clean(recs, ts, ch) for n, ch in CH.items()}
    S["dym"] = to_series(recs, ts, CH["dym"])   # bez filtra - 0 i 250 to realne stany
    pom = [bool(recs[t]["state"] & 1) for t in ts]
    dym = S["dym"]
    og, pk, zw, bo = S["ogrz"], S["pokoj"], S["zewn"], S["bojler"]
    po = S["powrot"]

    print("=" * 84)
    print(" ANALIZA TELEMETRII PIECA C.O. - WLASCIWOSCI CIEPLNE")
    print("=" * 84)
    print(" plik           :", path)
    print(" rekordow       : %d" % N)
    print(" zakres UTC     : %s -> %s" % (utc(ts[0]), utc(ts[-1])))
    print(" dlugosc        : %.2f dnia" % ((ts[-1] - ts[0]) / 86400.0))
    gaps = [ts[i] - ts[i - 1] for i in range(1, N)]
    print(" odstep probek  : mediana %d s, %.1f%% <= 61 s" % (
        st.median(gaps), 100.0 * sum(1 for g in gaps if g <= 61) / len(gaps)))
    print(" filtrowanie    : Hampel (skoki > 6 K) + mediana 5 probek")

    # ---------------------------------------------------------------- 1
    print("\n" + "-" * 84)
    print(" 1. JAKOSC DANYCH")
    print("-" * 84)
    print(" %-9s %8s %8s %9s %9s %9s" % ("kanal", "n", "braki", "min", "mediana", "max"))
    for name, ch in CH.items():
        good = [v for v in S[name] if v is not None]
        if not good:
            print(" %-9s %8d %8d %9s %9s %9s" % (name, 0, N, "-", "-", "-"))
        else:
            print(" %-9s %8d %8d %9.1f %9.1f %9.1f" % (
                name, len(good), N - len(good), min(good), st.median(good), max(good)))

    # ---------------------------------------------------------------- 2
    print("\n" + "-" * 84)
    print(" 2. SYGNAL PALENIA")
    print("-" * 84)
    print(" dym = 0        : %5d probek - czujnik nieaktywny (zalaczany temperaturowo)" % sum(1 for v in dym if v == 0))
    print(" 0 < dym < 300  : %5d probek - szum tla MQ-2, brak dymu" % sum(1 for v in dym if v and v < 300))
    print(" dym >= 300     : %5d probek - dym wykryty (palenie)" % sum(1 for v in dym if v and v >= 300))

    # ---------------------------------------------------------------- 3
    print("\n" + "-" * 84)
    print(" 3. EPIZODY GRZANIA (zasilanie > 40 C dluzej niz 30 min)")
    print("-" * 84)
    eps, start = [], None
    for i in range(N):
        hot = og[i] is not None and og[i] > 40
        if hot and start is None:
            start = i
        elif not hot and start is not None:
            if ts[i - 1] - ts[start] >= 1800:
                eps.append((start, i - 1))
            start = None
    if start is not None and ts[-1] - ts[start] >= 1800:
        eps.append((start, N - 1))
    print(" %-15s %-15s %6s %6s %7s %7s" % ("start UTC", "koniec UTC", "godz", "Tmax", "T_konc", "spadek"))
    for a, b in eps:
        v = [og[i] for i in range(a, b + 1) if og[i] is not None]
        if v:
            print(" %-15s %-15s %6.1f %6.1f %7.1f %7.1f" % (
                utc(ts[a]).strftime("%m-%d %H:%M"), utc(ts[b]).strftime("%m-%d %H:%M"),
                (ts[b] - ts[a]) / 3600.0, max(v), v[-1], max(v) - v[-1]))
    print(" epizodow: %d, lacznie %.1f h grania" % (
        len(eps), sum((ts[b] - ts[a]) / 3600.0 for a, b in eps)))

    # ---------------------------------------------------------------- 4
    print("\n" + "-" * 84)
    print(" 4. POMPA C.O. (bit0 stateBits)")
    print("-" * 84)
    print(" probek z pompa ON        : %5d (%.1f%%)" % (sum(pom), 100.0 * sum(pom) / N))
    print(" probek z bitem pompy     : %5d (%.1f%%) - bit zapisywany tylko przy aktywnym czujniku dymu"
          % (sum(1 for t in ts if recs[t]["state"] & 4), 100.0 * sum(1 for t in ts if recs[t]["state"] & 4) / N))

    # ---------------------------------------------------------------- 5
    print("\n" + "-" * 84)
    print(" 5. BEZWLADNOSC PIECA - STYGNIECIE")
    print("-" * 84)
    print(" Model: T(t) = Tinf + (T0-Tinf)*exp(-t/tau),  tau = C/UA  [C=pojemnosc, UA=straty]")
    print(" Fitowany na odcinkach ciaglych, R2 >= 0,95, spadek >= 10 K, dlugosc >= 2 h")
    for name in ("ogrz", "powrot"):
        y = S[name]
        segs, i = [], 0
        while i < N:
            if y[i] is None:
                i += 1
                continue
            j = i
            while j + 1 < N and ts[j + 1] - ts[j] <= 90 and y[j + 1] is not None:
                j += 1
            if j - i > 80:
                segs.append((i, j))
            i = j + 1
        found = []
        for a, b in segs:
            v = [y[k] for k in range(a, b + 1)]
            imax = max(range(len(v)), key=lambda k: (v[k] if v[k] is not None else -99))
            a2 = a + imax
            if b - a2 < 110:
                continue
            tt, TT = [], []
            for k in range(a2, b + 1):
                if y[k] is None:
                    continue
                tt.append((ts[k] - ts[a2]) / 3600.0)
                TT.append(y[k])
            if len(tt) < 120 or TT[0] - TT[-1] < 10:
                continue
            f = fit_exp(tt, TT)
            if f is None:
                continue
            tau, Tinf, T0, r2, n = f
            if r2 < 0.95:
                continue
            frac = sum(1 for k in range(a2, b + 1) if pom[k]) / float(b - a2 + 1)
            found.append((ts[a2], tau, T0, TT[-1], Tinf, r2, n, frac))
        found.sort(key=lambda x: x[0])
        print("\n --- %s ---" % name.upper())
        print(" %-15s %7s %6s %6s %6s %6s %5s %7s" % (
            "start UTC", "tau[h]", "T0", "T_konc", "Tinf", "R2", "n", "pompa"))
        for d in found[:14]:
            print(" %-15s %7.2f %6.1f %6.1f %6.1f %6.3f %5d %6.0f%%" % (
                utc(d[0]).strftime("%m-%d %H:%M"), d[1], d[2], d[3], d[4], d[5], d[6], 100 * d[7]))
        off = [d[1] for d in found if d[7] < 0.10]
        on = [d[1] for d in found if d[7] > 0.60]
        if off:
            print(" >>> tau mediana, pompa OFF = %.2f h  (n=%d)  <- straty postojowe" % (st.median(off), len(off)))
        if on:
            print(" >>> tau mediana, pompa ON  = %.2f h  (n=%d)  <- odbior grzejnikow" % (st.median(on), len(on)))
            if off:
                print(" >>> UA_grzejniki / UA_straty = %.2f" % (st.median(off) / st.median(on) - 1))

    # ---------------------------------------------------------------- 6
    print("\n" + "-" * 84)
    print(" 6. CZAS STYGNIECIA PIECA OD SZCZYTU (zasilanie)")
    print("-" * 84)
    i, rows = 0, []
    while i < N:
        if og[i] is None or og[i] < 60:
            i += 1
            continue
        j = i
        while j + 1 < N and ts[j + 1] - ts[j] <= 90 and og[j + 1] is not None and og[j + 1] >= og[j] - 0.5:
            j += 1
        tmax, t0 = og[j], ts[j]
        marks = {}
        for k in (20, 30, 40, 50):
            for m in range(j, min(j + 1400, N)):
                if og[m] is not None and og[m] <= tmax - k:
                    marks[k] = (ts[m] - t0) / 3600.0
                    break
        if marks:
            rows.append((t0, tmax, marks))
        i = j + 1
    out = []
    for r in rows:
        if out and r[0] - out[-1][0] < 5 * 3600:
            continue
        out.append(r)
    print(" %-15s %6s %8s %8s %8s %8s" % ("szczyt UTC", "Tmax", "-20 K", "-30 K", "-40 K", "-50 K"))
    for t0, tmax, m in out:
        f = lambda k: ("%.1f h" % m[k]) if k in m else "-"
        print(" %-15s %6.1f %8s %8s %8s %8s" % (
            utc(t0).strftime("%m-%d %H:%M"), tmax, f(20), f(30), f(40), f(50)))
    for k in (20, 30, 40, 50):
        v = [m[k] for _, _, m in out if k in m]
        if v:
            print(" mediana spadku o %d K: %.1f h (n=%d)" % (k, st.median(v), len(v)))

    # ---------------------------------------------------------------- 7
    print("\n" + "-" * 84)
    print(" 7. ODBIOR CIEPLA PRZEZ GRZEJNIKI")
    print("-" * 84)
    print(" Bilans: Q = m*cp*dT = UA*(T_sr - T_pokoj)")
    print("   => dT_zas-powrot = NTU*(T_sr - T_pokoj),   NTU = UA/(m*cp)")
    xs, ys = [], []
    for i in range(N):
        z, p, k = og[i], po[i], pk[i]
        if None in (z, p, k) or z < 30 or not pom[i]:
            continue
        tsr = (z + p) / 2.0
        if tsr - k <= 4:
            continue
        xs.append(tsr - k)
        ys.append(z - p)
    if len(xs) > 40:
        ntu = [ys[i] / xs[i] for i in range(len(xs))]
        r = linreg(xs, ys)
        print(" probek                   : %d" % len(xs))
        print(" NTU mediana              : %.3f" % st.median(ntu))
        print(" dT zasilanie-powrot      : mediana %.2f K  (zakres %.1f..%.1f K)" % (
            st.median(ys), min(ys), max(ys)))
        print(" T_sr - T_pokoj           : mediana %.1f K  (zakres %.1f..%.1f K)" % (
            st.median(xs), min(xs), max(xs)))
        print(" regresja dT = a*(T_sr-Tpok): a=%.3f  R2=%.3f" % (
            sum(xs[i] * ys[i] for i in range(len(xs))) / sum(x * x for x in xs), r[2]))
        print(" Moc odbioru przy zalozenym przeplywie (Q = 1,163 * V[m3/h] * dT):")
        for V in (0.5, 0.8, 1.2):
            print("   V=%.1f m3/h -> Q = %.2f kW przy dT=%.1f K" % (V, 1.163 * V * st.median(ys), st.median(ys)))
    else:
        print(" za malo probek z pompa ON: %d" % len(xs))

    # ---------------------------------------------------------------- 8
    print("\n" + "-" * 84)
    print(" 8. OPZNIENIE ZASILANIE -> POWROT")
    print("-" * 84)
    idx = [i for i in range(N) if og[i] is not None and po[i] is not None]
    c = corr_lag([og[i] for i in idx], [po[i] for i in idx], 120)
    if c:
        print(" najwyzsza korelacja przy opoznieniu %d min (r=%.3f)" % c)
        print(" = czas przejscia wody od czujnika zasilania do czujnika powrotu")

    # ---------------------------------------------------------------- 9
    print("\n" + "-" * 84)
    print(" 9. DYNAMIKA ROZPALANIA - moc chwilowa")
    print("-" * 84)
    print(" P = C * dT/dt  (pompa OFF => cale cieplo idzie w mase pieca)")
    cands = []
    for i in range(N - 20):
        if og[i] is None or og[i + 20] is None or ts[i + 20] - ts[i] > 1300:
            continue
        if any(pom[k] for k in range(i, i + 21)):
            continue
        win = [og[k] for k in range(i, i + 21) if og[k] is not None]
        if len(win) < 18:
            continue
        d = max(win) - min(win)
        if d < 2 or d > 25:
            continue
        r = (og[i + 20] - og[i]) / ((ts[i + 20] - ts[i]) / 3600.0)
        if r > 10:
            cands.append((r, i))
    cands.sort(reverse=True)
    seen = []
    print(" %-15s %9s %8s %8s" % ("moment UTC", "K/h", "T_start", "wzrost"))
    for r, i in cands:
        if any(abs(ts[i] - s) < 5400 for s in seen):
            continue
        seen.append(ts[i])
        print(" %-15s %9.1f %8.1f %8.1f" % (
            utc(ts[i]).strftime("%m-%d %H:%M"), r, og[i], og[i + 20] - og[i]))
        if len(seen) >= 8:
            break
    if seen:
        rr = st.median([r for r, i in cands[:max(1, len(seen))]])
    else:
        rr = 0.0
    print("\n Przeliczenie na moc (przy zalozeniu pompy OFF):")
    print(" %-32s %10s %12s %14s" % ("wariant pojemnosci pieca", "C[kJ/K]", "P przy 60 K/h", "P przy 100 K/h"))
    for label, Vw, Mst in (("40 l wody + 120 kg stali", 40, 120),
                           ("70 l wody + 150 kg stali", 70, 150),
                           ("100 l wody + 200 kg stali", 100, 200),
                           ("150 l wody + 250 kg stali", 150, 250)):
        C = Vw * 4.186 + Mst * 0.49
        print(" %-32s %10.0f %10.1f kW %11.1f kW" % (label, C, C * 60 / 3600.0, C * 100 / 3600.0))

    # ---------------------------------------------------------------- 10
    print("\n" + "-" * 84)
    print(" 10. PRZELICZENIE BEZWLADNOSCI NA MOCE (tau_off z sekcji 5)")
    print("-" * 84)
    off_taus = []
    y = og
    segs, i = [], 0
    while i < N:
        if y[i] is None:
            i += 1
            continue
        j = i
        while j + 1 < N and ts[j + 1] - ts[j] <= 90 and y[j + 1] is not None:
            j += 1
        if j - i > 80:
            segs.append((i, j))
        i = j + 1
    for a, b in segs:
        v = [y[k] for k in range(a, b + 1)]
        if not v:
            continue
        imax = max(range(len(v)), key=lambda k: (v[k] if v[k] is not None else -99))
        a2 = a + imax
        if b - a2 < 110 or any(pom[k] for k in range(a2, b + 1)):
            continue
        tt, TT = [], []
        for k in range(a2, b + 1):
            if y[k] is None:
                continue
            tt.append((ts[k] - ts[a2]) / 3600.0)
            TT.append(y[k])
        if len(tt) < 120 or TT[0] - TT[-1] < 10:
            continue
        f = fit_exp(tt, TT)
        if f and f[3] >= 0.95:
            off_taus.append(f[0])
    tau_off = st.median(off_taus) if off_taus else 3.8
    print(" tau_off (pompa OFF) = %.2f h = %d s   (n=%d odcinkow)" % (tau_off, tau_off * 3600, len(off_taus)))
    print(" %-32s %9s %9s %11s %11s" % ("wariant", "C[kJ/K]", "UA[W/K]", "Q@70C[kW]", "Q@50C[kW]"))
    for label, Vw, Mst in (("40 l wody + 120 kg stali", 40, 120),
                           ("70 l wody + 150 kg stali", 70, 150),
                           ("100 l wody + 200 kg stali", 100, 200),
                           ("150 l wody + 250 kg stali", 150, 250)):
        C = Vw * 4.186 + Mst * 0.49
        UA = C * 1000 / (tau_off * 3600)
        print(" %-32s %9.0f %9.1f %11.2f %11.2f" % (label, C, UA, UA * 50 / 1000, UA * 30 / 1000))
    print(" Q@70C = moc tracona przez kociol przy 70 C w kotlowni 20 C")

    # ---------------------------------------------------------------- 11
    print("\n" + "-" * 84)
    print(" 11. BEZWLADNOSC BUDYNKU (nocne stygniecie pokoju przy wylaczonym grzaniu)")
    print("-" * 84)
    i, res = 0, []
    while i < N:
        ok = (pk[i] is not None and og[i] is not None and og[i] < 35 and not pom[i])
        if not ok:
            i += 1
            continue
        j = i
        while (j + 1 < N and ts[j + 1] - ts[j] <= 90 and pk[j + 1] is not None
               and og[j + 1] is not None and og[j + 1] < 35 and not pom[j + 1]):
            j += 1
        if j - i >= 90:
            tt = [(ts[k] - ts[i]) / 3600.0 for k in range(i, j + 1)]
            TT = [pk[k] for k in range(i, j + 1)]
            if TT[0] - TT[-1] >= 1.0:
                f = fit_exp(tt, TT)
                if f and f[3] > 0.90:
                    z = [zw[k] for k in range(i, j + 1) if zw[k] is not None]
                    res.append((ts[i], f[0], TT[0], TT[-1], f[3], len(tt), st.median(z) if z else 0))
        i = j + 1
    print(" %-15s %8s %6s %6s %6s %5s %7s" % ("start UTC", "tau[h]", "T0", "T_konc", "R2", "n", "zewn"))
    for r in res[:10]:
        print(" %-15s %8.1f %6.1f %6.1f %6.3f %5d %7.1f" % (
            utc(r[0]).strftime("%m-%d %H:%M"), r[1], r[2], r[3], r[4], r[5], r[6]))
    if res:
        print(" mediana tau_budynku = %.1f h (n=%d) - to bezwladnosc calej bryly, nie pieca"
              % (st.median([r[1] for r in res]), len(res)))

    # ---------------------------------------------------------------- 12
    print("\n" + "-" * 84)
    print(" 12. UKLAD SOLARNY (kontrola, nie temat glowny)")
    print("-" * 84)
    pn = S["panel"]
    print(" %-8s %8s %8s %8s %8s %9s" % ("dzien", "max", "min", "sr", "Tmax-zewn", "h>40C"))
    days = {}
    for i in range(N):
        days.setdefault(utc(ts[i]).strftime("%m-%d"), []).append(i)
    for d in sorted(days):
        ii = days[d]
        v = [pn[i] for i in ii if pn[i] is not None]
        z = [zw[i] for i in ii if zw[i] is not None]
        if not v or not z:
            continue
        if max(v) > 35:
            print(" %-8s %8.1f %8.1f %8.1f %8.1f %9.1f" % (
                d, max(v), min(v), st.mean(v), max(v) - st.median(z),
                sum(1 for i in ii if pn[i] is not None and pn[i] > 40) / 60.0))

    # ---------------------------------------------------------------- 13
    print("\n" + "-" * 84)
    print(" 13. BILANS DOBOWY")
    print("-" * 84)
    print(" %-6s %7s %7s %7s %7s %7s %8s %8s" % (
        "dzien", "ogrzmax", "ogrzmin", "bojmax", "pokmax", "pokmin", "zewn_med", "h>40C"))
    for d in sorted(days):
        ii = days[d]
        def mm(name):
            v = [S[name][i] for i in ii if S[name][i] is not None]
            return (max(v), min(v)) if v else (float("nan"), float("nan"))
        om, omin = mm("ogrz")
        bm, _ = mm("bojler")
        pm, pmin = mm("pokoj")
        z = [zw[i] for i in ii if zw[i] is not None]
        hh = sum(1 for i in ii if og[i] is not None and og[i] > 40) / 60.0
        print(" %-6s %7.1f %7.1f %7.1f %7.1f %7.1f %8.1f %8.1f" % (
            d, om, omin, bm, pm, pmin, st.median(z) if z else float("nan"), hh))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else
         "google_drive/akwarium-367be-default-rtdb-2026-export.json")
