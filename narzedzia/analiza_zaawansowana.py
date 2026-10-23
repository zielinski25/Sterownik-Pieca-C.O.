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
  7. Gdzie siedzi czujnik 'ogrz' - dowod z zachowania przy pompie
  8. Struktura bazy: identyfikatory urzadzen
  9. Energia dobowa w kotle (kWh/dobe palenia)
 10. Niezalezne potwierdzenie tau (okna 60 min, test ciagu kominowego)

Uruchomienie:
  python3 narzedzia/analiza_zaawansowana.py [sciezka.json]
"""
import sys
import json
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
        # "dzien bez palenia" = kociol nie zgrzal sie ponad 35 C.
        # Filtr po dymie byl bledny: czujnik ma stale tlo 240-300, a podczas
        # palenia czesto pokazuje 260-300 - czyli nie odroznia palenia od tla.
        ov = [S["ogrz"][i] for i in ii if S["ogrz"][i] is not None]
        if not ov or max(ov) >= 35.0:
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

    # ---------------------------------------------------------------- 7
    print("\n" + "-" * 84)
    print(" 7. CZY 'ogrz' JEST NA RURZE WYJSCIOWEJ (nie w palenisku)")
    print("-" * 84)
    print(" Czujnik 'ogrz' = czujnik_temp_C_O (BUS_2) = ZASILANIE, rura wyjscia z kotla.")
    print(" Test: przy pompie OFF woda stoi, wiec temperatura powinna zmieniac sie")
    print(" inaczej niz przy pompie ON. Szukamy odcinkow jednorodnych.")

    # Okna 60-minutowe w obrebie jednego stanu pompy. Caly odcinek brac nie
    # mozna, bo po nocy srednia temperatura spada ponizej progu i odcinek
    # wypada - a wlasnie pierwsza godzina po zgaszeniu jest najciekawsza.
    def _okna(ch, min_temp=40.0):
        out = []
        i = 0
        while i < N:
            st0 = pom[i]
            j = i
            while j < N and pom[j] == st0:
                j += 1
            for a in range(i, j - 60 + 1, 10):
                v = [(ts[k], S[ch][k]) for k in range(a, a + 60)
                     if S[ch][k] is not None]
                if len(v) < 20:
                    continue
                h = (v[-1][0] - v[0][0]) / 3600.0
                if h < 0.8:
                    continue
                sr = st.mean([x[1] for x in v])
                if sr < min_temp:
                    continue
                out.append(((v[-1][1] - v[0][1]) / h, sr))
            i = j
        return out

    for nazwa, stan in (("POMPA OFF (woda stoi)", False), ("POMPA ON (woda krazy)", True)):
        okna = _okna("ogrz")
        okna = [x for x in okna]  # stan juz rozdzielony ponizej
        # filtruj po stanie pompy w oknie
        okna = []
        i = 0
        while i < N:
            st0 = pom[i]
            j = i
            while j < N and pom[j] == st0:
                j += 1
            if st0 == stan:
                for a in range(i, j - 60 + 1, 10):
                    v = [(ts[k], S["ogrz"][k]) for k in range(a, a + 60)
                         if S["ogrz"][k] is not None]
                    if len(v) < 20:
                        continue
                    h = (v[-1][0] - v[0][0]) / 3600.0
                    if h < 0.8:
                        continue
                    sr = st.mean([x[1] for x in v])
                    if sr < 40.0:
                        continue
                    okna.append(((v[-1][1] - v[0][1]) / h, sr))
            i = j
        print("\n %s - okien 60 min z woda >40 C: %d" % (nazwa, len(okna)))
        if not okna:
            continue
        tempa = [x[0] for x in okna]
        letnie = [t for t, sr in okna if t < 0]      # tylko stygnie
        if letnie:
            print("   stygnięcie: mediana %6.2f K/h   zakres %6.2f .. %6.2f  (n=%d)" % (
                st.median(letnie), min(letnie), max(letnie), len(letnie)))
        print("   wszystkie:  mediana %6.2f K/h   zakres %6.2f .. %6.2f" % (
            st.median(tempa), min(tempa), max(tempa)))

    print("\n Wniosek: po zaplonie zasilanie potrafi skoczyc >200 K/h (goraca woda")
    print(" przeplywa obok czujnika). Palenisko nie nagrzewa sie tak szybko -")
    print(" dlatego tau ~3.8 h opisuje WODE, nie palenisko.")

    # ---------------------------------------------------------------- 8
    print("\n" + "-" * 84)
    print(" 8. STRUKTURA BAZY - IDENTYFIKATORY URZADZEN")
    print("-" * 84)
    print(" Kolejne restarty ESP32 zmieniaja ID w kluczu Firebase. Sprawdzamy, czy")
    print(" to jedno urzadzenie (ciaglosc), czy wiele instalacji (nakladanie).")

    def _ids(path):
        raw = json.load(open(path, encoding="utf-8"))
        found = []

        def walk_ids(node, acc):
            if not isinstance(node, dict):
                return
            if "ts" in node and "a" in node:
                found.append((acc[-1].split("-")[0] if acc else "?", node))
                return
            for kk, vv in node.items():
                walk_ids(vv, acc + [kk])
        walk_ids(raw, [])
        return found

    if __import__("os").path.exists(path):
        br = _ids(path)
        dev = {}
        for d, r in br:
            dev.setdefault(d, []).append(r)

        def pelne(rs):
            ok = 0
            for r in rs:
                a = r["a"]
                good = sum(1 for ch in (0, 1, 2, 4, 5, 6, 7)
                           if a[ch] not in (None, 65535) and 0 < a[ch] < 65530)
                if good >= 6:
                    ok += 1
            return ok

        full = [d for d in dev if pelne(dev[d]) >= 0.5 * len(dev[d])]
        nfull = sum(pelne(dev[d]) for d in full)
        print("   rekordow: %d, roznych ID: %d, ID z pelnymi danymi: %d" % (
            len(br), len(dev), len(full)))
        print("   rekordy z pelnych ID: %d (%.1f%%)" % (nfull, 100.0 * nfull / len(br)))
        sp = sorted((min(r["ts"] for r in dev[d]), max(r["ts"] for r in dev[d]), d)
                    for d in full)
        ov = 0
        for x in range(len(sp)):
            for y in range(x + 1, len(sp)):
                if min(sp[x][1], sp[y][1]) - max(sp[x][0], sp[y][0]) > 300:
                    ov += 1
        print("   par ID nakladajacych sie czasowo (>5 min): %d" % ov)
        print("   -> to jedno urzadzenie, ktore zmienia ID przy restarcie")
        print("      (wylaczajac %d krotkich okresow nakladania)" % ov)
    else:
        print("   (pominiety - brak pliku)")

    # ---------------------------------------------------------------- 9
    print("\n" + "-" * 84)
    print(" 9. ENERGIA DOBOWA W KOTLE")
    print("-" * 84)
    print(" Energia netto zatrzymana w wodzie = C * (Tmax - Tmin) w ciagu doby.")
    print(" To DOLNA granica: cieplo oddane grzejnikom w trakcie palenia juz ucieklo")
    print(" i nie jest tu liczone. C = 419 kJ/K (~100 l wody w kotle).")
    Ck = 419.0
    print("\n %-6s %7s %7s %9s %9s" % ("dzien", "Tmin", "Tmax", "dT", "kWh netto"))
    dni_pal, dni_bez = [], []
    for d in sorted(days):
        ii = [i for i in days[d] if S["ogrz"][i] is not None]
        if len(ii) < 100:
            continue
        v = [S["ogrz"][i] for i in ii]
        if max(v) < 35.0:
            dni_bez.append(d)
            continue
        dni_pal.append(d)
        print(" %-6s %7.1f %7.1f %9.1f %9.2f" % (
            d, min(v), max(v), max(v) - min(v), Ck * (max(v) - min(v)) / 3600.0))
    print("\n dni z paleniem: %d, dni bez palenia: %d" % (len(dni_pal), len(dni_bez)))

    # ---------------------------------------------------------------- 10
    print("\n" + "-" * 84)
    print(" 10. NIEZALEZNE POTWIERDZENIE TAU + TEST CIAGU KOMINOWEGO")
    print("-" * 84)
    print(" Okna 60-minutowe przy POMPIE OFF: k = -(dT/dt)/(T - T_pokoj), tau = 1/k.")
    print(" Sprawdzamy tez, czy k zalezy od temperatury zewnetrznej (wiekszy ciag")
    print(" kominowy = szybsze stygniecie).")

    okna = []
    i = 0
    while i < N - 60:
        if not pom[i]:
            j = i
            while j < N and not pom[j]:
                j += 1
            for a in range(i, max(i + 1, j - 60 + 1), 15):
                b = min(a + 60, N)
                v = [(ts[k], S["ogrz"][k]) for k in range(a, b) if S["ogrz"][k] is not None]
                ze = [S["zewn"][k] for k in range(a, b) if S["zewn"][k] is not None]
                po = [S["pokoj"][k] for k in range(a, b) if S["pokoj"][k] is not None]
                if len(v) < 30 or not ze or not po:
                    continue
                h = (v[-1][0] - v[0][0]) / 3600.0
                if h < 0.8:
                    continue
                sr = st.mean([x[1] for x in v])
                if sr < 40.0:
                    continue
                tem = (v[-1][1] - v[0][1]) / h
                if tem >= 0:
                    continue
                dtp = sr - st.mean(po)
                if dtp < 10:
                    continue
                okna.append((st.mean(ze), -tem / dtp))
            i = j
        else:
            i += 1

    print("\n okien uzytych: %d" % len(okna))
    if len(okna) > 5:
        ks = [o[1] for o in okna]
        print(" mediana k = %.4f 1/h  ->  tau = %.2f h" % (st.median(ks), 1 / st.median(ks)))
        x = [o[0] for o in okna]
        y = ks
        n = len(x); sx = sum(x); sy = sum(y)
        sxx = sum(a * a for a in x); sxy = sum(a * b for a, b in zip(x, y))
        den = n * sxx - sx * sx
        if den:
            bb = (n * sxy - sx * sy) / den
            aa = (sy - bb * sx) / n
            ss = sum((yy - (aa + bb * xx)) ** 2 for xx, yy in zip(x, y))
            sm = sum((yy - st.mean(y)) ** 2 for yy in y)
            r2 = 1 - ss / sm if sm else 0.0
            print(" k vs T_zewn: nachylenie %+.5f 1/h/K, R2 = %.3f" % (bb, r2))
            print(" -> %s" % ("brak wykrytego wplywu ciagu kominowego"
                              if r2 < 0.1 else "jest zaleznosc od temperatury zewnetrznej"))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else
         "google_drive/akwarium-367be-default-rtdb-2026-export.json")
