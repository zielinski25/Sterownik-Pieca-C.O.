#!/usr/bin/env python3
"""Krok 3: zamiana wartosci timeoutow na jednostki sekundowe (3.57.8 -> 3.57.9).

Zmienia DOKLADNIE te miejsca, ktore wskazal audyt — po pozycji w pliku, nie
globalnym "sedem", zeby nie ruszyc http.setTimeout (milisekundy) ani tych
wywolan, ktore juz sa poprawne (client.setTimeout(4)).

Zakres zatwierdzony przez uzytkownika (2026-11-01):
  * 22 x setHandshakeTimeout(...)            -> 8   (decyzja: limit TLS 8 s)
  * 6  x client.setTimeout(...) w sekundach  -> /1000 (4000->4, 7000->7,
                                                    8000->8, 15000->15)
  * 9  x client.setTimeout(4)                -> bez zmian
  * 5  x http.setTimeout(...) w ms           -> bez zmian
  * FIRMWARE_VERSION                          -> v3.57.9
  * nowy blok changelogu na gorze pliku

Uzycie:
    python3 zrob_3_57_9.py --src <3.57.8/main_centrala.cpp> --dst <3.57.9/main_centrala.cpp>
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from audyt_timeoutow import bez_komentarzy, zbierz_deklaracje  # noqa: E402

HANDSHAKE_S = 8
# Wartosci sekundowe, ktore maja zostac po zmianie (reszta: /1000).
ZAMIERZONE_SEKUNDY = {4000: 4, 7000: 7, 8000: 8, 15000: 15}
MAX_SEKUND = 20          # wszystko powyzej = blad jednostek

CHANGELOG = """// FIRMWARE_VERSION v3.57.8 -> v3.57.9 (2026-11-01) — LIMITY SIECI W SEKUNDACH.
//
// Zgloszenie uzytkownika (1.11): raport nadzoru po restarcie pokazal
// "Ostatni znacznik: DESZCZ / BEFORE" (31.10, 23:49) oraz "FB_LOOP / BEFORE"
// (1.11, 03:35). Zadanie sieciowe zamilklo na 300 s, nadzor zrestartowal
// centrale. Zawieszenie bylo rzadkie (~4 h odstepu), ale kosztowalo 5 minut
// bez Telegrama i Firebase przy kazdym zdarzeniu.
//
// [1] PRZYCZYNA: ZLA JEDNOSTKA W setHandshakeTimeout(). W arduino-esp32 2.0.x
//     metoda mnozy argument przez 1000 (WiFiClientSecure.cpp:
//     "sslclient->handshake_timeout = handshake_timeout * 1000"), czyli
//     przyjmuje SEKUNDY. W firmware stalo 4000 i 7000 - czyli limit handshake
//     TLS wynosil 4000 s (66 min) i 7000 s (117 min), a nie 4 s i 7 s.
//     Jedno pobranie moglo wiec wisiec dluzej niz prog nadzoru (300 s).
//     Dotyczy to TAKZE pompy fbApp.loop() na koncu tgTaskFn - czyli obu
//     znacznikow z 1.11.
//     Poprawka: 22 miejsca, wszystkie na 8 s (decyzja uzytkownika; 8 s to
//     nadal 37x mniej niz prog nadzoru, a wystarcza na wolne lacze).
// [2] TA SAMA POMYLKA W client.setTimeout(). WiFiClientSecure::setTimeout()
//     tez liczy w sekundach. W szesciu miejscach staly wartosci w
//     milisekundach (4000, 7000, 8000, 15000), czyli 4000-15000 SEKUND.
//     Dzis przykrywa je HTTPClient::connect(), ktore po polaczeniu nadpisuje
//     timeout klienta wartoscia z http.setTimeout() - ale to ta sama pomylka
//     i wracalaby przy kazdym uproszczeniu warstwy HTTP. Poprawka: te same
//     wartosci w sekundach (4, 7, 8, 15).
//     NIE zmieniono: 9 miejsc z juz poprawnym client.setTimeout(4) oraz
//     5 miejsc http.setTimeout(...) - HTTPClient liczy w MILISEKUNDACH.
//
// Efekt: najgorszy czas jednego GET spada z ~66 min do 17 s
// (TCP 5 s + TLS 8 s + odczyt 4 s), czyli 17x mniej niz prog 300 s.
//
// Czego ta wersja NIE naprawia:
//   * odpowiedzi, ktora "kapie" (1 bajt co 3 s) - limit odczytu resetuje sie
//     przy kazdym bajcie; potrzebny limit CALEGO pobrania, nie bezczynnosci,
//   * rotacji historii i pliku .rebuild - osobno w 3.57.10 (decyzja).
//
// TA WERSJA ZAWIERA TAKZE POPRAWKI Z 3.57.8 (3.57.8 nie byla wgrywana):
//   * koniec podwojnego "vv" w wersji pokazywanej przez Telegram,
//   * klucz NVS "nadzorWiekS" - raport nadzoru podaje, ile sekund przed
//     restartem powstal ostatni znacznik.
//
// Bez zmian: progi nadzoru, WDT, formaty JSON, telemetria, Telegram, OTA,
// historia na flashu, archiwum solarne, zadne inne ustawienie.
//
"""


def zmien_wywolania(zrodlo):
    """Podmienia wartosci w wywolaniach timeoutow. Zwraca (nowy, lista zmian)."""
    kod = bez_komentarzy(zrodlo)
    dekl = zbierz_deklaracje(kod)
    zmiany = []
    # Zbieramy (pozycja_start, pozycja_end, nowy_tekst) i podmieniamy od konca,
    # zeby offsety wczesniejszych wywolan pozostaly wazne.
    podmiany = []
    for m in re.finditer(r"([A-Za-z_]\w*)\s*\.\s*(setHandshakeTimeout|setTimeout)\s*\(\s*(\d+)\s*\)",
                         kod):
        obiekt, metoda, wartosc = m.group(1), m.group(2), int(m.group(3))
        typ = dekl.get(obiekt)
        linia = kod.count("\n", 0, m.start()) + 1
        if metoda == "setHandshakeTimeout":
            if wartosc != HANDSHAKE_S:
                podmiany.append((m.start(3), m.end(3), str(HANDSHAKE_S), linia, obiekt,
                                 metoda, wartosc, HANDSHAKE_S))
            continue
        # setTimeout: tylko sekundowe klasy i tylko wartosci oczywiscie zle
        if typ not in ("WiFiClientSecure", "WiFiClient"):
            continue
        if wartosc <= MAX_SEKUND:
            continue
        nowa = ZAMIERZONE_SEKUNDY.get(wartosc, wartosc // 1000)
        podmiany.append((m.start(3), m.end(3), str(nowa), linia, obiekt, metoda, wartosc, nowa))

    nowy = zrodlo
    for start, end, tekst, linia, obiekt, metoda, stara, nowa in sorted(podmiany, reverse=True):
        nowy = nowy[:start] + tekst + nowy[end:]
        zmiany.append({"linia": linia, "obiekt": obiekt, "metoda": metoda,
                       "stara": stara, "nowa": nowa})
    zmiany.sort(key=lambda x: x["linia"])
    return nowy, zmiany


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--dst", required=True)
    args = ap.parse_args()

    zrodlo = open(args.src, encoding="utf-8").read()
    nowy, zmiany = zmien_wywolania(zrodlo)

    # FIRMWARE_VERSION
    if '"v3.57.8"' in nowy:
        nowy = nowy.replace('static const char* FIRMWARE_VERSION = "v3.57.8";',
                            'static const char* FIRMWARE_VERSION = "v3.57.9";', 1)
    else:
        print("BLAD: nie znalazlem FIRMWARE_VERSION = v3.57.8", file=sys.stderr)
        return 1

    # Changelog na samej gorze pliku
    nowy = CHANGELOG + nowy

    os.makedirs(os.path.dirname(args.dst), exist_ok=True)
    open(args.dst, "w", encoding="utf-8", newline="\n").write(nowy)

    handshake = [z for z in zmiany if z["metoda"] == "setHandshakeTimeout"]
    sett = [z for z in zmiany if z["metoda"] == "setTimeout"]
    print(f"Zapisano: {args.dst}")
    print(f"  setHandshakeTimeout: {len(handshake)} zmian -> 8 s")
    print(f"  setTimeout (sekundy): {len(sett)} zmian")
    for z in sett:
        print(f"    linia {z['linia']:>6}  {z['obiekt']}.setTimeout({z['stara']}) -> ({z['nowa']})")
    print(f"  RAZEM: {len(zmiany)} zmian")
    return 0 if len(zmiany) == 28 else 1


if __name__ == "__main__":
    sys.exit(main())
