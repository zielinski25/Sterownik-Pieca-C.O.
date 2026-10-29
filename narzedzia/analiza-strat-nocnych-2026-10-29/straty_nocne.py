#!/usr/bin/env python3
"""
Straty nocne zbiornika solarnego i bojlera — rozliczenie na prawdziwych danych.

Wejscie:
  - log centrali (STATUS co 5 min): piec, bojler, panel, zewn
  - zachmurzenie nocne z Open-Meteo (nocne srednie, 19:00-06:00)

Model (ten sam, co w aplikacji 0.31.52):
  C * dT/dt = -(tankUa + skyRadWk*(1-zachmurzenie)) * (T - Ta)
  C = 116,3 Wh/K (100 l wody + rury), tankUa = 1,4 W/K, skyRadWk = 8,2 W/K

Uruchomienie:  python3 straty_nocne.py <katalog_z_logami>
"""
import sys, os, re, json, datetime as dt, statistics as st

C_WK      = 116.3   # Wh/K
TANK_UA   = 1.4     # W/K
SKY_WK    = 8.2     # W/K przy czystym niebie
H_NOC     = 11      # 19:00 -> 06:00

PAT = re.compile(
    r"t=(2026-\d\d-\d\d)T(\d\d):(\d\d):(\d\d).*?"
    r"piec=([-\d.]+)C piecSr=([-\d.]+)C bojler=([-\d.]+)C panel=([-\d.]+)C "
    r"pom=([-\d.]+)C zewn=([-\d.]+)C")

# nocne zachmurzenie [%] z Open-Meteo dla Trzemchy Górnej
CLOUD = {"2026-10-18": 98, "2026-10-19": 98, "2026-10-20": 85, "2026-10-21": 85,
         "2026-10-22": 68, "2026-10-23": 46, "2026-10-24": 60, "2026-10-25": 70,
         "2026-10-26": 48, "2026-10-27": 63, "2026-10-28": 100}


def wczytaj(logi):
    recs = []
    for fn in logi:
        for line in open(fn, encoding="utf-8", errors="ignore"):
            m = PAT.search(line)
            if not m:
                continue
            d, h, mi, s = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
            ts = dt.datetime.strptime(d, "%Y-%m-%d").replace(hour=h, minute=mi, second=s)
            recs.append((ts, float(m.group(5)), float(m.group(7)), float(m.group(8)), float(m.group(10))))
    recs.sort()
    return recs


def noc(recs, d0):
    day = dt.date.fromisoformat(d0)
    nd = day + dt.timedelta(days=1)
    t0 = dt.datetime(day.year, day.month, day.day, 19, 0)
    t1 = dt.datetime(nd.year, nd.month, nd.day, 6, 0)
    return [r for r in recs if t0 <= r[0] <= t1]


def policz_noc(seria, T_start, cloud):
    """Symulacja 5-minutowa na RZECZYWISTEJ temperaturze zewnetrznej z logu."""
    T = T_start
    prev = seria[0][0]
    energia_wh = 0.0
    dtbar = []
    for r in seria:
        h = (r[0] - prev).total_seconds() / 3600.0
        prev = r[0]
        if h <= 0 or h > 0.5:
            continue
        Ta = r[4]
        ua = TANK_UA + SKY_WK * (1.0 - cloud / 100.0)
        energia_wh += ua * (T - Ta) * h
        T += -(ua * (T - Ta) / C_WK) * h
        dtbar.append(T - Ta)
    return T - T_start, energia_wh / 1000.0, st.mean(dtbar) if dtbar else float("nan")


def main(logi):
    recs = wczytaj(logi)
    print(f"rekordow STATUS: {len(recs)}   od {recs[0][0]} do {recs[-1][0]}\n")

    wiersze = []
    print("=" * 108)
    print("STRATY ZBIORNIKA NA DACHU (czujnik panel) — 19:00 -> 06:00")
    print("=" * 108)
    print(f"{'noc':11s} {'T19':>6s} {'T06':>6s} {'zmierz':>7s} {'model':>7s} {'blad':>6s} "
          f"{'UA zmierz':>9s} {'dT sr':>6s} {'kWh':>6s} {'chmury':>6s}")
    bledy = []
    for d0 in sorted(CLOUD):
        s = noc(recs, d0)
        if len(s) < 60:
            print(f"{d0}  za malo probek ({len(s)})")
            continue
        Ts = st.mean(r[3] for r in s[:3])
        Te = st.mean(r[3] for r in s[-3:])
        pred, kwh, dtbar = policz_noc(s, Ts, CLOUD[d0])
        ua = TANK_UA + SKY_WK * (1 - CLOUD[d0] / 100.0)
        num = sum(max(0.0, (r[3] - r[4])) * 5 / 60 for r in s)
        ua_meas = C_WK * (Ts - Te) / num if num else float("nan")
        e = pred - (Te - Ts)
        bledy.append(abs(e))
        wiersze.append([d0, Ts, Te, Te - Ts, pred, e, ua_meas, dtbar, kwh, CLOUD[d0]])
        print(f"{d0} {Ts:6.1f} {Te:6.1f} {Te-Ts:7.1f} {pred:7.1f} {e:6.1f} "
              f"{ua_meas:9.2f} {dtbar:6.1f} {kwh:6.2f} {CLOUD[d0]:5d}%")

    print("-" * 108)
    print(f"  MAE = {st.mean(bledy):.2f} K   mediana = {st.median(bledy):.2f} K   "
          f"max = {max(bledy):.2f} K")

    print("\n" + "=" * 108)
    print("GDZIE UCIEKA CIEPLO — podzial na powietrze i niebo")
    print("=" * 108)
    print(f"{'noc':11s} {'chmury':>6s} {'UA razem':>9s} {'niebo':>7s} {'kWh razem':>10s} {'kWh niebo':>10s}")
    kwh_all = kwh_sky = 0.0
    for w in wiersze:
        d0, Ts, Te, d, pred, e, ua_meas, dtbar, kwh, cloud = w
        ua_sky = SKY_WK * (1 - cloud / 100.0)
        ua_all = TANK_UA + ua_sky
        share = ua_sky / ua_all
        kwh_all += kwh
        kwh_sky += kwh * share
        print(f"{d0} {cloud:5d}% {ua_all:9.2f} {100*share:6.0f}% {kwh:10.2f} {kwh*share:10.2f}")
    print("-" * 108)
    print(f"{'SUMA':11s} {'':6s} {'':9s} {'':7s} {kwh_all:10.2f} {kwh_sky:10.2f}   "
          f"-> niebo {100*kwh_sky/kwh_all:.0f}% strat")
    print(f"srednio na noc: {kwh_all/len(wiersze):.2f} kWh, w tym niebo {kwh_sky/len(wiersze):.2f} kWh")
    print(f"1 kWh = {1000/C_WK:.1f} K na zbiorniku 100 l")

    print("\n" + "=" * 108)
    print("CZY SAM MODEL WYSTARCZY — co sie dzieje przy zlym zachmurzeniu")
    print("=" * 108)
    for nazwa, fix in [("chmury z prognozy", None), ("stale 50% (zgadywanka)", 50),
                       ("stale 100% (model przed 0.31.52)", 100), ("stale 0% (zawsze czysto)", 0)]:
        e = []
        for d0 in sorted(CLOUD):
            s = noc(recs, d0)
            if len(s) < 60:
                continue
            Ts = st.mean(r[3] for r in s[:3]); Te = st.mean(r[3] for r in s[-3:])
            c = CLOUD[d0] if fix is None else fix
            pred, _, _ = policz_noc(s, Ts, c)
            e.append(abs(pred - (Te - Ts)))
        print(f"  {nazwa:36s} MAE = {st.mean(e):.2f} K   mediana {st.median(e):.2f}   max {max(e):.2f}")

    print("\n" + "=" * 108)
    print("BOJLER PIECA (w kotlowni) — inna historia niz zbiornik na dachu")
    print("=" * 108)
    print(f"{'noc':11s} {'T19':>6s} {'T06':>6s} {'spad':>6s} {'piec19':>7s} {'piec06':>7s}")
    bo, zo = [], []
    for d0 in sorted(CLOUD):
        s = noc(recs, d0)
        if len(s) < 60:
            continue
        Ts = st.mean(r[3] for r in s[:3]); Te = st.mean(r[3] for r in s[-3:])
        Tbs = st.mean(r[2] for r in s[:3]); Tbe = st.mean(r[2] for r in s[-3:])
        Ps = st.mean(r[1] for r in s[:3]); Pe = st.mean(r[1] for r in s[-3:])
        bo.append(Tbe - Tbs); zo.append(Te - Ts)
        print(f"{d0} {Tbs:6.1f} {Tbe:6.1f} {Tbe-Tbs:6.1f} {Ps:7.1f} {Pe:7.1f}")

    def corr(a, b):
        ma, mb = st.mean(a), st.mean(b)
        num = sum((x - ma) * (y - mb) for x, y in zip(a, b))
        den = (sum((x - ma) ** 2 for x in a) * sum((y - mb) ** 2 for y in b)) ** 0.5
        return num / den if den else float("nan")

    chm = [CLOUD[d] for d in sorted(CLOUD) if len(noc(recs, d)) >= 60]
    print(f"\n  spadek bojlera : min {min(bo):.1f}  max {max(bo):.1f}  "
          f"sredni {st.mean(bo):.1f}  mediana {st.median(bo):.1f} K")
    print(f"  spadek zbiornika: sredni {st.mean(zo):.1f} K")
    print(f"  korelacja bojlera z chmurami : {corr(bo, chm):+.2f}   <- pogoda NIE tlumaczy")
    print(f"  korelacja zbiornika z chmurami: {corr(zo, chm):+.2f}   <- tlumaczy")


if __name__ == "__main__":
    katalog = sys.argv[1] if len(sys.argv) > 1 else "."
    logi = [os.path.join(katalog, f) for f in sorted(os.listdir(katalog))
            if f.startswith("piec_log") and f.endswith(".txt")]
    main(logi)
