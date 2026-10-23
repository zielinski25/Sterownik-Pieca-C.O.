#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Analiza zaawansowana eksportu RTDB - to, czego nie ma w analiza_pieca.py.

Sekcje:
  1. Energia sloneczna ze zbiornika (kWh/dobe) - kanal panel to czujnik W ZBIORNIKU
  2. Weryfikacja modelu prognostycznego stygniecia (blad po +4 h)
  3. Jakosc czujnikow: braki i artefakty
  4. Przeplyw z opoznienia zasilanie->powrot
  5. Dni bez palenia - czy dom sam trzyma temperature
  6. Przyrost energii w piecu (bilans, warianty pojemnosci)

Uruchomienie:
  python3 narzedzia/analiza_zaawansowana.py [sciezka.json]
"""
import sys
import math
import datetime
import statistics as st

sys.path.insert(0, "narzedzia")
from analiza_pieca import load, clean, to_series, hampel, fit_exp, CH, linreg


def utc(t):
    return datetime.datetime.utcfromtimestamp(t)


def main(path):
    recs, ts = load(path)
    N = len(ts)
    S = {n: clean(recs, ts, ch) for n, ch in CH.items()}
    S["dym"] = S["dym"]
    pom = [bool(recs[t]["state"] & 1) for t in ts]
    czuj = [bool(recs[t]["state"] & 4) for t in ts]
    days = {}
    for i in range(N):
        days.setdefault(utc(ts[i]).strftime("%m-%d"), []).append(i)

    print("=" * 84)
    print(" ANALIZA ZAAWANSOWANA - dodatkowe wnioski z telemetrii")
    print("=" * 84)
    print(" rekordow: %d, dni: %d" % (N, len(days)))

    # ---------------------------------------------------------------- 1
    print("\n" + "-" * 84)
    print(" 1. ENERGIA SLONECZNA ZE ZBIORNIKA")
    print("-" * 84)
    print(" Czujnik 'panel' (BUS_3, czujnik_temp_Panel_Plus) siedzi W ZBIORNIKU")
    print(" 100 l, nie na absorberze - mierzy mase wody, nie temperature rury.")
    print(" Energia netto = C * (Tmax - Tmin), C = 100 kg * 4,186 kJ/kgK = 0,1163 kWh/K")
    print(" To DOLNA granica uzysku: nie obejmuje cieplej wody pobranej ze zbiornika.")
    C = 100 * 4.186 / 3600.0
    tot = 0.0
    print("\n %-6s %6s %6s %7s %9s %8s %9s" % (
        "dzien", "Tmin", "Tmax", "dT", "kWh netto", "h>40C", "zewn med"))
    for d in sorted(days):
        ii = [i for i in days[d] if S["panel"][i] is not None]
        if not ii:
            continue
        v = [S["panel"][i] for i in ii]
        z = [S["zewn"][i] for i in days[d] if S["zewn"][i] is not None]
        if max(v) <= 38:
            continue
        kwh = C * (max(v) - min(v))
        tot += kwh
        print(" %-6s %6.1f %6.1f %7.1f %9.2f %8.1f %9.1f" % (
            d, min(v), max(v), max(v) - min(v), kwh,
            sum(1 for i in ii if S["panel"][i] > 40) / 60.0,
            st.median(z) if z else float("nan")))
    print(" SUMA dni z panelem > 38 C: %.1f kWh" % tot)
    print(" Energia zatrzymana w zbiorniku (ponad 20 C) przy szczycie:")
    for d in sorted(days):
        ii = [i for i in days[d] if S["panel"][i] is not None]
        if not ii:
            continue
        mx = max(S["panel"][i] for i in ii)
        if mx > 60:
            print("   %s: %.1f C -> %.2f kWh" % (d, mx, C * (mx - 20)))

    # ---------------------------------------------------------------- 2
    print("\n" + "-" * 84)
    print(" 2. WERYFIKACJA MODELU PROGNOSTYCZNEGO STYGNIECIA")
    print("-" * 84)
    print(" Model dopasowany na pierwszych ~2 h od szczytu, sprawdzony po +4 h.")
    print(" Jesli blad jest maly, model da sie uzyc do planowania palenia.")
    segs, i = [], 0
    while i < N:
        if S["ogrz"][i] is None:
            i += 1
            continue
        j = i
        while j + 1 < N and ts[j + 1] - ts[j] <= 90 and S["ogrz"][j + 1] is not None:
            j += 1
        if j - i > 80:
            segs.append((i, j))
        i = j + 1
    errs = []
    for a, b in segs:
        v = [S["ogrz"][k] for k in range(a, b + 1)]
        imax = max(range(len(v)), key=lambda k: (v[k] if v[k] is not None else -99))
        a2 = a + imax
        if b - a2 < 400 or any(pom[k] for k in range(a2, min(b, a2 + 400))):
            continue
        tt, TT = [], []
        for k in range(a2, min(b, a2 + 130)):
            if S["ogrz"][k] is None:
                continue
            tt.append((ts[k] - ts[a2]) / 3600.0)
            TT.append(S["ogrz"][k])
        if len(tt) < 100:
            continue
        f = fit_exp(tt, TT)
        if not f or f[3] < 0.95:
            continue
        tau, Tinf, T0 = f[0], f[1], f[2]
        tgt = (ts[min(b, a2 + 130)] - ts[a2]) / 3600.0 + 4.0
        kp = None
        for k in range(a2, b + 1):
            if S["ogrz"][k] is not None and (ts[k] - ts[a2]) / 3600.0 >= tgt:
                kp = k
                break
        if kp is None:
            continue
        pred = Tinf + (T0 - Tinf) * math.exp(-((ts[kp] - ts[a2]) / 3600.0) / tau)
        errs.append((pred - S["ogrz"][kp], tau, T0, S["ogrz"][kp], pred,
                     (ts[kp] - ts[a2]) / 3600.0))
    if errs:
        e = [x[0] for x in errs]
        print(" odcinkow sprawdzonych: %d" % len(errs))
        print(" blad po +4 h: mediana %.1f C, sredni %.1f C, zakres %.1f..%.1f C" % (
            st.median(e), st.mean(e), min(e), max(e)))
        print(" %-7s %6s %7s %8s %9s" % ("tau[h]", "T0", "czas[h]", "rzecz.", "prognoza"))
        for x in sorted(errs, key=lambda z: abs(z[0])):
            print(" %7.2f %6.1f %7.1f %8.1f %9.1f" % (x[1], x[2], x[5], x[3], x[4]))

    # ---------------------------------------------------------------- 3
    print("\n" + "-" * 84)
    print(" 3. JAKOSC CZUJNIKOW")
    print("-" * 84)
    print(" %-8s %10s %10s %10s" % ("kanal", "braki", "artefakty", "najgorszy"))
    for name in ("ogrz", "powrot", "bojler", "pokoj", "panel", "zewn", "trociny"):
        raw = to_series(recs, ts, CH[name])
        n_none = sum(1 for v in raw if v is None)
        h = hampel(raw, 4, 6.0)
        n_art = sum(1 for i in range(N) if raw[i] is not None and h[i] is None)
        print(" %-8s %6d (%4.1f%%) %6d (%5.2f%%)" % (
            name, n_none, 100 * n_none / N, n_art, 100 * n_art / N))
    print(" UWAGA: 'braki' to glownie pojedyncze minuty bez odczytu z magistrali 1-Wire.")

    # ---------------------------------------------------------------- 4
    print("\n" + "-" * 84)
    print(" 4. PRZEPLYW Z OPZNIENIA (niezalezne oszacowanie)")
    print("-" * 84)
    print(" Zmierzone opoznienie zasilanie->powrot: 6 min.")
    print(" Przeplyw = objetosc wody w instalacji / czas przejscia.")
    print(" %-24s %12s %10s" % ("objetosc instalacji", "przeplyw", "l/min"))
    for V in (80, 100, 120, 150, 200):
        print(" %-24s %9.2f m3/h %10.1f" % ("%d litrow" % V, V * 60 / 6 / 1000.0, V / 6.0))
    print(" Do porownania: przy dT=4,3 C i mocy 4 kW przeplyw = 0,80 m3/h;")
    print("               przy mocy 10 kW = 2,00 m3/h.")

    # ---------------------------------------------------------------- 5
    print("\n" + "-" * 84)
    print(" 5. DNI BEZ PALENIA - czy dom sam trzyma temperature")
    print("-" * 84)
    print(" %-6s %8s %8s %8s %10s" % (
        "dzien", "pokoj min", "pokoj max", "zewn med", "amplituda"))
    for d in sorted(days):
        ii = days[d]
        if any(S["dym"][i] and S["dym"][i] >= 300 for i in ii):
            continue
        v = [S["pokoj"][i] for i in ii if S["pokoj"][i] is not None]
        z = [S["zewn"][i] for i in ii if S["zewn"][i] is not None]
        if not v or not z:
            continue
        print(" %-6s %8.1f %8.1f %8.1f %10.1f" % (
            d, min(v), max(v), st.median(z), max(v) - min(v)))

    # ---------------------------------------------------------------- 6
    print("\n" + "-" * 84)
    print(" 6. ENERGIA ZAKUMULOWANA W PIECU")
    print("-" * 84)
    print(" E = C * (T - T_otoczenia). Ile kWh siedzi w piecu przy danej temperaturze.")
    print(" %-32s %9s %11s %11s %11s" % ("wariant", "C[kJ/K]", "przy 50 C", "przy 70 C", "przy 90 C"))
    for label, Vw, Mst in (("40 l wody + 120 kg stali", 40, 120),
                           ("70 l wody + 150 kg stali", 70, 150),
                           ("100 l wody + 200 kg stali", 100, 200),
                           ("150 l wody + 250 kg stali", 150, 250)):
        Ck = Vw * 4.186 + Mst * 0.49
        print(" %-32s %9.0f %8.1f kWh %8.1f kWh %8.1f kWh" % (
            label, Ck, Ck * 30 / 3600.0, Ck * 50 / 3600.0, Ck * 70 / 3600.0))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else
         "google_drive/akwarium-367be-default-rtdb-2026-export.json")
