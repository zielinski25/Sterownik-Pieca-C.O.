#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Audyt telemetrii Sterownik C.O. — 28 dni danych z Firebase RTDB.

Wejscie: eksport RTDB (wezel korzeniowy, dni "01".."29").
Wyjscie: raport na stdout + plik RAPORT-AUDYTU.md

Nie zaklada niczego o strukturze poza tym, co widac w danych.
"""
import json, sys, os, datetime, collections

SRC = sys.argv[1] if len(sys.argv) > 1 else "akwarium-367be-default-rtdb-10-export (1).json"

CH = ["zewn", "bojler", "kociol", "OGRZ_SR", "powrot", "panel", "pokoj",
      "trociny", "wilgotnosc", "cisnienie", "dym"]

def lt(ts):
    return datetime.datetime.utcfromtimestamp(ts + 3600)  # CET

# ---------------------------------------------------------------- wczytanie
def load(path):
    d = json.load(open(path, encoding="utf-8"))
    recs = []
    for day, node in d.items():
        if not isinstance(node, dict):
            continue
        for key, v in node.items():
            if not isinstance(v, dict) or "ts" not in v or "-" not in key:
                continue
            bid, seq = key.rsplit("-", 1)
            try:
                seq_i = int(seq, 16)
            except ValueError:
                continue
            recs.append({
                "seq": seq_i, "boot": bid, "ts": v["ts"],
                "t": lt(v["ts"]), "a": v.get("a") or [],
                "age": v.get("age") or [], "q": v.get("q"),
                "sim": v.get("sim"), "state": v.get("state"),
                "k": v.get("k"), "s": v.get("s"),
                "day": day,
            })
    recs.sort(key=lambda r: r["ts"])
    return recs

# ---------------------------------------------------------------- analiza
def main():
    recs = load(SRC)
    out = []
    def P(*a):
        line = " ".join(str(x) for x in a)
        print(line); out.append(line)

    P(f"# AUDYT TELEMETRII — {os.path.basename(SRC)}")
    P()
    P(f"Rekordow: **{len(recs)}**")
    P(f"Zakres: **{recs[0]['t']}** → **{recs[-1]['t']}**")
    dni = sorted({r["t"].date() for r in recs})
    P(f"Dni kalendarzowych: **{len(dni)}**  ({dni[0]} … {dni[-1]})")

    # --- 1. ciaglosc w obrebie rozruchu
    P()
    P("## 1. Ciaglosc licznikow seq w obrebie rozruchu")
    P()
    byboot = collections.defaultdict(list)
    for r in recs:
        byboot[r["boot"]].append(r)
    P("| boot | probek | seq | brakujacych seq | od | do | czas |")
    P("|---|---|---|---|---|---|---|")
    total_missing_seq = 0
    worst = []
    for b, rs in sorted(byboot.items(), key=lambda kv: min(x["ts"] for x in kv[1])):
        rs.sort(key=lambda r: r["seq"])
        s = [r["seq"] for r in rs]
        have = set(s)
        missing = [x for x in range(s[0], s[-1] + 1) if x not in have]
        total_missing_seq += len(missing)
        t0, t1 = rs[0]["t"], rs[-1]["t"]
        dur = (rs[-1]["ts"] - rs[0]["ts"]) / 3600.0
        mark = f"**{len(missing)}**" if missing else "0"
        P(f"| `{b[:10]}` | {len(rs)} | {s[0]}–{s[-1]} | {mark} | {t0:%d.%m %H:%M} | {t1:%d.%m %H:%M} | {dur:.1f} h |")
        if missing:
            worst.append((b, missing, len(rs)))
    P()
    P(f"**Razem brakujacych seq w obrebie rozruchow: {total_missing_seq}**")
    for b, m, n in worst:
        P(f"- `{b[:10]}`: brak {len(m)} — seq {m[0]}…{m[-1]}")

    # --- 2. przerwy czasowe
    P()
    P("## 2. Przerwy czasowe > 90 s")
    P()
    gaps = []
    for i in range(1, len(recs)):
        dt = recs[i]["ts"] - recs[i-1]["ts"]
        if dt > 90:
            gaps.append((recs[i-1], recs[i], dt))
    P(f"Liczba przerw: **{len(gaps)}**")
    same = [g for g in gaps if g[0]["boot"] == g[1]["boot"]]
    cross = [g for g in gaps if g[0]["boot"] != g[1]["boot"]]
    P(f"- w obrebie TEGO SAMEGO rozruchu (prawdziwa utrata): **{len(same)}**")
    P(f"- na granicy rozruchow (restart urzadzenia): **{len(cross)}**")
    P()
    tot_same = sum(g[2] for g in same)
    tot_cross = sum(g[2] for g in cross)
    P(f"Czas: prawdziwa utrata **{tot_same/60:.1f} min**, restarty **{tot_cross/60:.1f} min**")
    P()
    if same:
        P("| od | do | przerwa | boot |")
        P("|---|---|---|---|")
        for a, b, dt in same:
            P(f"| {a['t']:%d.%m %H:%M:%S} | {b['t']:%d.%m %H:%M:%S} | {dt//60}m{dt%60:02d}s | `{a['boot'][:10]}` |")
        P()
        hist = collections.Counter(int(g[2]//60) for g in same)
        P("Histogram dlugosci prawdziwych przerw (minuty: liczba):")
        P("```")
        for k in sorted(hist):
            P(f"  {k:>3} min : {hist[k]:>3}x")
        P("```")

    # --- 3. kanaly: brak odczytu
    P()
    P("## 3. Kanaly — ile razy `age == 65535` (brak poprawnego odczytu)")
    P()
    P("| idx | kanal | brakow | % | najdluzszy ciag | poczatek najdluzszego |")
    P("|---|---|---|---|---|---|")
    ch_stat = {}
    for ci in range(11):
        cnt = 0
        best = 0; best_start = None; run = 0
        for r in recs:
            bad = ci < len(r["age"]) and r["age"][ci] == 65535
            if bad:
                cnt += 1
                if run == 0:
                    start = r["t"]
                run += 1
                if run > best:
                    best = run; best_start = start
            else:
                run = 0
        ch_stat[ci] = (cnt, best, best_start)
        pct = 100.0 * cnt / len(recs)
        bs = f"{best_start:%d.%m %H:%M}" if best_start else "—"
        P(f"| {ci} | {CH[ci]} | {cnt} | {pct:.1f}% | {best} | {bs} |")

    # --- 4. przebiegi braku dla kanalow z duza liczba brakow
    P()
    P("## 4. Przebiegi braku odczytu dla kanalow podejrzanych")
    P()
    for ci in range(11):
        cnt, best, _ = ch_stat[ci]
        if cnt < 20:
            continue
        P(f"### kanal {ci} ({CH[ci]}) — {cnt} brakow")
        P()
        runs = []
        run = None
        for r in recs:
            bad = ci < len(r["age"]) and r["age"][ci] == 65535
            if bad:
                if run is None:
                    run = [r["t"], r["t"], 1, r["boot"]]
                else:
                    run[1] = r["t"]; run[2] += 1
                    if r["boot"] != run[3]:
                        run[3] = "kilka"
            elif run is not None:
                runs.append(run); run = None
        if run: runs.append(run)
        P(f"Przebiegow: **{len(runs)}**")
        if runs:
            lens = [x[2] for x in runs]
            P(f"Dlugosci (probek): min {min(lens)}, mediana {sorted(lens)[len(lens)//2]}, max {max(lens)}")
            P()
            P("| od | do | probek | boot |")
            P("|---|---|---|---|")
            for a, b, n, bo in runs[:25]:
                P(f"| {a:%d.%m %H:%M} | {b:%d.%m %H:%M} | {n} | `{bo[:10]}` |")
            if len(runs) > 25:
                P(f"| … | … | ({len(runs)-25} dalszych) | |")
        P()

    # --- 5. bity jakosci i symulacji
    P()
    P("## 5. Bity jakosci (`q`) i symulacji (`sim`)")
    P()
    qc = collections.Counter(r["q"] for r in recs)
    P(f"Roznych wartosci `q`: **{len(qc)}**")
    P("```")
    for q, n in qc.most_common(20):
        P(f"  q={q:<10} (0x{q:08X})  {n:>6}x   {100.0*n/len(recs):5.1f}%")
    P("```")
    sc = collections.Counter(r["sim"] for r in recs)
    P(f"Roznych wartosci `sim`: **{len(sc)}**")
    P("```")
    for s, n in sc.most_common(20):
        P(f"  sim={s:<10} (0x{s:08X})  {n:>6}x   {100.0*n/len(recs):5.1f}%")
    P("```")
    P()
    P("Korelacja `q` z kanalami bez odczytu:")
    P("```")
    key = collections.Counter()
    for r in recs:
        bad = tuple(ci for ci in range(11) if ci < len(r["age"]) and r["age"][ci] == 65535)
        key[(r["q"], bad)] += 1
    for (q, bad), n in key.most_common(15):
        P(f"  q=0x{q:08X}  brakujace kanaly={bad}  ->  {n}x")
    P("```")

    # --- 6. wartosci kanalow
    P()
    P("## 6. Zakresy wartosci kanalow (tylko poprawne odczyty)")
    P()
    P("| idx | kanal | min | max | mediana | zer |")
    P("|---|---|---|---|---|---|")
    for ci in range(11):
        vals = [r["a"][ci] for r in recs
                if ci < len(r["a"]) and ci < len(r["age"]) and r["age"][ci] != 65535]
        if not vals:
            P(f"| {ci} | {CH[ci]} | — | — | — | — |")
            continue
        vals.sort()
        zeros = sum(1 for x in vals if x == 0)
        P(f"| {ci} | {CH[ci]} | {vals[0]} | {vals[-1]} | {vals[len(vals)//2]} | {zeros} |")

    with open("RAPORT-AUDYTU.md", "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    print(f"\n[zapisano RAPORT-AUDYTU.md, {len(out)} linii]", file=sys.stderr)

if __name__ == "__main__":
    main()
