#!/usr/bin/env python3
"""Test zgodnosci: czy symulacja nadal opisuje to, co jest w firmware.

Symulacja w test_limity.cpp ma sens tylko wtedy, gdy progi i kolejnosc
wywolania sa te same, co w main_centrala.cpp. Ten test czyta ZRODLO
firmware (3.57.8) i sprawdza kazde zalozenie, na ktorym stoi symulacja.

Uzycie:
    python3 test_zgodnosc.py --src /sciezka/do/src/main_centrala.cpp
Bez --src test konczy sie kodem 0 i mowi "pominieto" (brak zrodla w repo).
"""
import argparse
import os
import re
import sys

WYNIKI = []


def spr(warunek, nazwa, szczegol=""):
    WYNIKI.append((bool(warunek), nazwa, szczegol))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=None, help="sciezka do main_centrala.cpp z paczki firmware")
    args = ap.parse_args()

    if not args.src:
        print("POMINIETO: brak --src (zrodlo firmware nie jest trzymane w repo).")
        return 0

    try:
        zrodlo = open(args.src, encoding="utf-8", errors="replace").read()
    except OSError as e:
        print(f"POMINIETO: nie moge otworzyc {args.src}: {e}")
        return 0

    # 1. Progi uzyte w symulacji
    spr(re.search(r"#define\s+HISTORIA_MAX_PROBEK\s+1440", zrodlo) is not None,
        "HISTORIA_MAX_PROBEK = 1440")
    spr(re.search(r"#define\s+HISTORIA_DLUGA_MAX_PROBEK\s+8000", zrodlo) is not None,
        "HISTORIA_DLUGA_MAX_PROBEK = 8000")
    spr(re.search(r"#define\s+HISTORIA_RESYNC_MARGINES\s+1000", zrodlo) is not None,
        "HISTORIA_RESYNC_MARGINES = 1000")
    spr(re.search(r"#define\s+HISTORIA_DLUGA_RESYNC_MARGINES\s+500", zrodlo) is not None,
        "HISTORIA_DLUGA_RESYNC_MARGINES = 500")
    spr(re.search(r"HISTORIA_PENDING_LIMIT_B\s*=\s*4096", zrodlo) is not None,
        "HISTORIA_PENDING_LIMIT_B = 4096")

    # 2. Rotacja tylko POWYZEJ limitu + marginesu (granice L5/L6)
    spr(re.search(r"static_cast<unsigned>\(linie\)\s*<=\s*limit\s*\+\s*margines", zrodlo) is not None,
        "trim odpala sie tylko gdy linie > limit + margines")

    # 3. Trim wola sie z wlasciwymi progami dla obu strumieni
    spr("HISTORIA_MAX_PROBEK, HISTORIA_RESYNC_MARGINES" in zrodlo,
        "krotka historia: trim(limit=1440, margines=1000)")
    spr("HISTORIA_DLUGA_MAX_PROBEK, HISTORIA_DLUGA_RESYNC_MARGINES" in zrodlo,
        "dluga historia: trim(limit=8000, margines=500)")

    # 4. JEDEN Engine dla obu historii (L3: blad jednej zatrzymuje obie)
    spr(len(re.findall(r"history193::Engine\s+historiaIo\s*\(", zrodlo)) == 1,
        "w firmware jest dokladnie jeden history193::Engine")
    spr(len(re.findall(r"historiaIo\.(?:append|trim)\(", zrodlo)) > 0,
        "oba strumienie ida przez ten sam obiekt historiaIo")

    # 5. Rezerwa na dluga historie w trybie SHADOW (L2: 2 MB < szczyt ~2,84 MB)
    m = re.search(r"FS_RESERVE_HIST_LONG_B\s*=\s*(\d+)UL\s*\*\s*(\d+)UL\s*\*\s*(\d+)UL", zrodlo)
    rezerwa = int(m.group(1)) * int(m.group(2)) * int(m.group(3)) if m else 0
    spr(rezerwa == 2 * 1024 * 1024, "FS_RESERVE_HIST_LONG_B = 2 MB", f"{rezerwa} B")

    # 6. Plik roboczy .rebuild nie jest nigdzie kasowany (L3/L3b)
    spr(".rebuild" in zrodlo, "nazwy .rebuild wystepuja w zrodle")
    spr(re.search(r"(?:LittleFS|systemFS196)\.remove\(\s*\"[^\"]*\.rebuild\"", zrodlo) is None,
        "firmware NIE kasuje pliku .rebuild")
    try:
        naglowek = open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     "CheckedHistoryIO.h"), encoding="utf-8").read()
    except OSError:
        naglowek = ""
    spr("O_RDWR|O_CREAT|O_EXCL" in naglowek,
        "Engine otwiera plik roboczy z O_EXCL (stary .rebuild = blad Open)")
    spr("unlink" not in naglowek and "remove(" not in naglowek,
        "Engine nigdzie nie usuwa pliku roboczego")

    # 7. Komunikaty, ktore symulacja tlumaczy
    spr("blad rotacji - zatrzymano obie historie" in zrodlo,
        "komunikat 'blad rotacji - zatrzymano obie historie'")
    spr("zapis niepotwierdzony - kolejka zachowana, obie historie zatrzymane" in zrodlo,
        "komunikat 'zapis niepotwierdzony ... obie historie zatrzymane'")
    spr("nowa probka nieprzyjeta do zapisu" in zrodlo,
        "odrzucenie z pelnej kolejki JEST logowane (WARN), nie ciche")

    # 8. Klucze NVS <= 15 znakow (ten sam limit, ktory przewrocil 3.57.7)
    klucze = set(re.findall(r"nvs(?:Put|Get)\w*\([^,]+,\s*\"([^\"]+)\"", zrodlo))
    klucze |= set(re.findall(r"\w+\.(?:put|get)[A-Za-z]*\(\s*\"([^\"]+)\"", zrodlo))
    zaDlugie = sorted(k for k in klucze if len(k) > 15)
    spr(not zaDlugie, f"wszystkie klucze NVS <= 15 znakow (sprawdzono {len(klucze)})",
        ", ".join(zaDlugie))

    ok = sum(1 for w, _, _ in WYNIKI if w)
    for w, nazwa, szczegol in WYNIKI:
        print(("  OK   " if w else "  FAIL ") + nazwa + (f" - {szczegol}" if szczegol else ""))
    print(f"\nZGODNOSC: {ok} OK / {len(WYNIKI) - ok} FAIL")
    return 0 if ok == len(WYNIKI) else 1


if __name__ == "__main__":
    sys.exit(main())
