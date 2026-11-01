#!/usr/bin/env python3
"""Brama kroku 3: czy poprawki sieci sa zrobione DOKLADNIE tak, jak ustalono.

Uruchamiaj PO zmianie w main_centrala.cpp (przed zbudowaniem paczki 3.57.9).
Przed zmianą celowo pokazuje FAIL — to jest lista rzeczy do zrobienia.

Sprawdza:
  1. kazde setHandshakeTimeout(...) ma wartosc 8 (decyzja: 8 s),
  2. kazde client.setTimeout(...) na WiFiClientSecure ma wartosc w sekundach
     <= 20 (czyli 15 s OTA, 8 s, 7 s, 4 s — a nie 4000/7000/15000),
  3. http.setTimeout(...) na HTTPClient zostaje w milisekundach (bez zmian),
  4. diff wzgledem 3.57.8 dotyka WYLACZNIE:
       - linii setHandshakeTimeout / setTimeout,
       - FIRMWARE_VERSION,
       - naglowka changelogu (pierwsze N linii pliku),
     czyli paczka naprawde podmienia tylko to, co zapowiedziano.

Uzycie:
    python3 sprawdz_po.py --src <nowy/main_centrala.cpp> \
                          --baza firmware-patch/v3.57.8-raport-i-wersja/src/main_centrala.cpp
"""
import argparse
import difflib
import re
import sys

MAX_HANDSHAKE_S = 8
MAX_SETTER_S = 20

OK, FAIL = [], []


def spr(warunek, nazwa, szczegol=""):
    (OK if warunek else FAIL).append((nazwa, szczegol))
    print(("  OK   " if warunek else "  FAIL ") + nazwa + (f" - {szczegol}" if szczegol else ""))


def bez_komentarzy(tekst):
    out = list(tekst)
    i, n = 0, len(tekst)
    while i < n:
        c = tekst[i]
        if c in "\"'":
            cyt = c
            i += 1
            while i < n and tekst[i] != cyt:
                if tekst[i] == "\\":
                    i += 1
                i += 1
            i += 1
            continue
        if c == "/" and i + 1 < n and tekst[i + 1] == "/":
            while i < n and tekst[i] != "\n":
                out[i] = " "
                i += 1
            continue
        if c == "/" and i + 1 < n and tekst[i + 1] == "*":
            out[i] = out[i + 1] = " "
            i += 2
            while i < n and not (tekst[i] == "*" and i + 1 < n and tekst[i + 1] == "/"):
                if tekst[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
                if i + 1 < n:
                    out[i + 1] = " "
                i += 2
            continue
        i += 1
    return "".join(out)


def deklaracje(kod):
    dekl = {}
    wzor = re.compile(
        r"(?:^|[;{}\n])\s*(?:static\s+|const\s+|extern\s+)*"
        r"(WiFiClientSecure|WiFiClient|HTTPClient)\s*(?:[*&]\s*)?([A-Za-z_]\w*)\s*(?=[;=(,\[])")
    for m in wzor.finditer(kod):
        dekl.setdefault(m.group(2), m.group(1))
    return dekl


def wywolania(kod, dekl):
    wynik = []
    for m in re.finditer(r"([A-Za-z_]\w*)\s*\.\s*(setHandshakeTimeout|setTimeout)\s*\(\s*(\d+)\s*\)",
                         kod):
        obiekt, metoda, wartosc = m.group(1), m.group(2), int(m.group(3))
        wynik.append({
            "linia": kod.count("\n", 0, m.start()) + 1,
            "obiekt": obiekt,
            "typ": dekl.get(obiekt, "?"),
            "metoda": metoda,
            "wartosc": wartosc,
        })
    return wynik


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="main_centrala.cpp po zmianie")
    ap.add_argument("--baza", required=True, help="main_centrala.cpp z 3.57.8")
    args = ap.parse_args()

    zrodlo = open(args.src, encoding="utf-8", errors="replace").read()
    kod = bez_komentarzy(zrodlo)
    dekl = deklaracje(kod)
    w = wywolania(kod, dekl)

    handshake = [x for x in w if x["metoda"] == "setHandshakeTimeout"]
    sett = [x for x in w if x["metoda"] == "setTimeout"]
    sec_sett = [x for x in sett if x["typ"] in ("WiFiClientSecure", "WiFiClient")]
    ms_sett = [x for x in sett if x["typ"] == "HTTPClient"]

    print(f"Zrodlo: {args.src}")
    print(f"Wywolan: setHandshakeTimeout={len(handshake)}, setTimeout={len(sett)} "
          f"(sekundy: {len(sec_sett)}, milisekundy: {len(ms_sett)})\n")

    # 1. handshake
    zle = [x for x in handshake if x["wartosc"] != MAX_HANDSHAKE_S]
    spr(handshake and not zle,
        f"wszystkie setHandshakeTimeout maja wartosc {MAX_HANDSHAKE_S} s",
        f"{len(zle)} do poprawy: "
        + ", ".join(f"linia {x['linia']}={x['wartosc']}" for x in zle[:6])
        + (" ..." if len(zle) > 6 else ""))

    # 2. setTimeout w sekundach
    zle2 = [x for x in sec_sett if x["wartosc"] > MAX_SETTER_S]
    spr(sec_sett and not zle2,
        f"wszystkie client.setTimeout (sekundy) <= {MAX_SETTER_S} s",
        f"{len(zle2)} do poprawy: "
        + ", ".join(f"linia {x['linia']}={x['wartosc']}" for x in zle2[:6])
        + (" ..." if len(zle2) > 6 else ""))

    # 3. HTTPClient bez zmian (milisekundy)
    zrodlo_baza = open(args.baza, encoding="utf-8", errors="replace").read()
    kod_baza = bez_komentarzy(zrodlo_baza)
    dekl_baza = deklaracje(kod_baza)
    w_baza = wywolania(kod_baza, dekl_baza)
    ms_baza = sorted(x["wartosc"] for x in w_baza
                     if x["metoda"] == "setTimeout" and x["typ"] == "HTTPClient")
    ms_teraz = sorted(x["wartosc"] for x in ms_sett)
    spr(ms_baza == ms_teraz, "http.setTimeout (milisekundy) nietkniete",
        f"baza={ms_baza} teraz={ms_teraz}")

    # 4. diff tylko w dozwolonych miejscach
    linie_baza = zrodlo_baza.splitlines()
    linie_nowe = zrodlo.splitlines()
    dozwolone = re.compile(r"setHandshakeTimeout\s*\(\s*\d+\s*\)|"
                           r"\.setTimeout\s*\(\s*\d+\s*\)|FIRMWARE_VERSION")
    opcodes = difflib.SequenceMatcher(None, linie_baza, linie_nowe).get_opcodes()
    zmienione = []
    for tag, i1, i2, j1, j2 in opcodes:
        if tag == "equal":
            continue
        for nr in range(j1, j2):
            zmienione.append((nr + 1, linie_nowe[nr]))
    # Dozwolony tez blok changelogu na gorze pliku (dopisany akapit 3.57.8 -> 3.57.9)
    # Zmiany obejmujace POCZATEK pliku (i1 == 0) to dopisany blok changelogu.
    granica_changelogu = max([j2 for tag, i1, i2, j1, j2 in opcodes
                              if tag != "equal" and i1 == 0] or [0])
    niedozwolone = [(nr, l) for nr, l in zmienione
                    if not dozwolone.search(l) and nr > granica_changelogu]
    spr(not niedozwolone, "diff dotyka tylko timeoutow, FIRMWARE_VERSION i changelogu",
        f"{len(niedozwolone)} innych linii: "
        + "; ".join(f"{nr}: {l.strip()[:60]}" for nr, l in niedozwolone[:5]))

    print(f"\nBRAMA KROKU 3: {len(OK)} OK / {len(FAIL)} FAIL")
    if FAIL:
        print("(Przed zmiana to jest oczekiwane — lista do zrobienia.)")
    return 0 if not FAIL else 1


if __name__ == "__main__":
    sys.exit(main())
