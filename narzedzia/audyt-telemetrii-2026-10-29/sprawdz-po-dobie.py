#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sprawdz-po-dobie.py — odczyt liczników z logu centrali po wgraniu 3.56.8

Wejście: plik z logiem (to, co zwraca http://<IP>/api/log-lines?lines=5000
albo komenda Telegram /logi).

Co robi:
  1. Wyciąga wszystkie linie tag=HEALTH-A (są co 5 minut).
  2. Czyta z nich liczniki telemetrii: produced / dropped / sent / ring /
     gap_events / gap_samples.
  3. Robi bilans: produced − sent − ring − dropped = ile próbek zniknęło.
  4. Pokazuje, KIEDY (który boot, jaka godzina) pojawił się nowy gap.
  5. Sprawdza, czy po restarcie jest dziura w "Ogrz. śr." (kanał t_ogrz_sr).

Uruchomienie:
    python sprawdz-po-dobie.py logi-centrala.txt

Nie zmienia żadnych plików. Tylko czyta i wypisuje.
"""

import re
import sys
from datetime import datetime

LICZNIKI = ["telemetry_produced", "telemetry_dropped", "telemetry_sent",
            "telemetry_ring", "telemetry_gap_events", "telemetry_gap_samples"]

RE_HEALTH = re.compile(r"tag=HEALTH-A\b.*")
RE_T = re.compile(r"t=(\S+ \S+|\S+)")
RE_BOOT = re.compile(r"boot=(\d+)")
RE_SEQ = re.compile(r"seq=(\d+)")


def parsuj(linia):
    d = {}
    for k in LICZNIKI:
        m = re.search(k + r"=(\d+)", linia)
        if m:
            d[k] = int(m.group(1))
    for klucz, rx in (("t", RE_T), ("boot", RE_BOOT), ("seq", RE_SEQ)):
        m = rx.search(linia)
        if m:
            d[klucz] = m.group(1)
    return d


def znacznik(s):
    """'2026-10-30 06:05:00' -> czytelnie; jak się nie da, oddaj jak jest."""
    for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%dT%H:%M:%S", "%d.%m.%Y %H:%M:%S"):
        try:
            return datetime.strptime(s, fmt).strftime("%d.%m %H:%M")
        except ValueError:
            pass
    return s


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    sciezka = sys.argv[1]
    try:
        with open(sciezka, encoding="utf-8", errors="replace") as f:
            linie = f.readlines()
    except OSError as e:
        print("Nie mogę otworzyć pliku:", e)
        return 2

    wpisy = []
    for L in linie:
        if RE_HEALTH.search(L):
            d = parsuj(L)
            if "telemetry_produced" in d:
                wpisy.append(d)

    print("=" * 66)
    print("LOG:", sciezka)
    print("linii w pliku:", len(linie), "| wpisów HEALTH-A z licznikami:", len(wpisy))
    print("=" * 66)

    if not wpisy:
        print()
        print("BRAK wpisów HEALTH-A. Możliwe przyczyny:")
        print("  - w logu są tylko najnowsze linie (za mało) -> pobierz z lines=5000")
        print("  - log wyczyszczony / inny plik")
        print("  - firmware bez liczników (czy na pewno 3.56.8?)")
        return 1

    pierwszy, ostatni = wpisy[0], wpisy[-1]
    print()
    print("ZAKRES")
    print("  pierwszy wpis :", znacznik(pierwszy.get("t", "?")), "boot=", pierwszy.get("boot", "?"))
    print("  ostatni wpis  :", znacznik(ostatni.get("t", "?")), "boot=", ostatni.get("boot", "?"))
    booty = sorted({w.get("boot") for w in wpisy if "boot" in w})
    print("  bootów w logu :", len(booty), "->", ", ".join(booty[:12]))
    if len(booty) > 1:
        print("  UWAGA: w logu jest więcej niż jeden start centrali.")

    print()
    print("BILANS — LICZONY OSOBNO DLA KAŻDEGO STARTU")
    print("(liczniki siedzą w RAM i zerują się przy restarcie, więc nie wolno")
    print(" odejmować ich przez granicę startu)")
    print()

    grupy = []
    for w in wpisy:
        b = w.get("boot")
        if not grupy or grupy[-1][0] != b:
            grupy.append((b, []))
        grupy[-1][1].append(w)

    suma_zniklo = 0
    suma_prod = 0
    for b, g in grupy:
        a, z = g[0], g[-1]
        prod = z.get("telemetry_produced", 0) - a.get("telemetry_produced", 0)
        sent = z.get("telemetry_sent", 0) - a.get("telemetry_sent", 0)
        drop = z.get("telemetry_dropped", 0) - a.get("telemetry_dropped", 0)
        ring = z.get("telemetry_ring", 0)
        if prod < 0:      # po restarcie liczniki od zera — ten wpis jest pierwszy
            prod = z.get("telemetry_produced", 0)
        zniklo = prod - sent - drop - ring
        suma_zniklo += zniklo
        suma_prod += prod
        print("  boot=%-4s %s -> %s" % (b, znacznik(a.get("t", "?")), znacznik(z.get("t", "?"))))
        print("      wyprodukowano %-6d wysłano %-6d odrzucono %-4d w kolejce %-4d"
              % (prod, sent, drop, ring))
        print("      ZNIKNĘŁO: %d" % zniklo)
    print()
    print("  RAZEM: wyprodukowano %d, zniknęło %d (%.2f%%)"
          % (suma_prod, suma_zniklo, 100.0 * suma_zniklo / suma_prod if suma_prod else 0))
    print()
    if suma_zniklo <= 0:
        print("  -> Nic nie zginęło. Potok szczelny.")
    elif suma_zniklo <= 5:
        print("  -> Poziom z audytu (0,50%% na dobę). Detektor powinien to wskazać.")
    else:
        print("  -> Duża strata. Sprawdź, czy to restart (patrz niżej), czy gubienie w paczkach.")

    print()
    print("DETEKTOR LUK (to jest cała pointa 3.56.8)")
    ge0 = sum(g[0].get("telemetry_gap_events", 0) if g[0].get("telemetry_gap_events", 0) > g[-1].get("telemetry_gap_events", 0) else 0 for _, g in grupy)
    ge_sum = sum(max(0, g[-1].get("telemetry_gap_events", 0) - (g[0].get("telemetry_gap_events", 0) if g[0].get("telemetry_gap_events", 0) <= g[-1].get("telemetry_gap_events", 0) else 0)) for _, g in grupy)
    gs_sum = sum(max(0, g[-1].get("telemetry_gap_samples", 0) - (g[0].get("telemetry_gap_samples", 0) if g[0].get("telemetry_gap_samples", 0) <= g[-1].get("telemetry_gap_samples", 0) else 0)) for _, g in grupy)
    ge0, ge1 = 0, ge_sum
    gs0, gs1 = 0, gs_sum
    print("  zdarzeń przerwy w numeracji :", ge1 - ge0)
    print("  próbek łącznie przeskoczonych:", gs1 - gs0)
    if ge1 - ge0 == 0:
        print()
        print("  -> Detektor MILCZY, a strata jest. To znaczy, że próbki znikają")
        print("     ŚCIEŻKĄ, KTÓREJ DETEKTOR NIE WIDZI (np. spool / fałszywe")
        print("     potwierdzenie paczki). Wtedy 3.56.9 ma sens — ale najpierw")
        print("     musimy wiedzieć, KTÓRA to ścieżka.")
    else:
        print()
        print("  -> Są konkretne przerwy. Poniżej: kiedy.")
        poprz = None
        for w in wpisy:
            g = w.get("telemetry_gap_events", 0)
            if poprz is not None and g > poprz:
                print("     %s  boot=%s  nowych przerw: %d  (razem próbek: %d)" % (
                    znacznik(w.get("t", "?")), w.get("boot", "?"),
                    g - poprz, w.get("telemetry_gap_samples", 0)))
            poprz = g

    print()
    print("RESTARTY (każdy boot = osobny start; po restarcie sprawdź 'Ogrz. śr.')")
    poprz = None
    for w in wpisy:
        b = w.get("boot")
        if b is None:
            continue
        if b != poprz:
            print("  %s  boot=%s  (pierwszy wpis po starcie)" % (znacznik(w.get("t", "?")), b))
            poprz = b

    print()
    print("JAK DOKOŃCZYĆ SPRAWDZENIE DZIURY W 'Ogrz. śr.'")
    print("  Potrzebny wykres z aplikacji (zakładka Wykresy, kanał 'Ogrz. śr.').")
    print("  Po restarcie ma NIE być 20-minutowej przerwy.")
    print("  Wyjątek: PIERWSZY restart po wgraniu 3.56.8 może ją jeszcze mieć —")
    print("  klucz srOstPompa powstaje dopiero przy pierwszym zaniku pompy.")
    print("  Od drugiego restartu dziura musi zniknąć.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
