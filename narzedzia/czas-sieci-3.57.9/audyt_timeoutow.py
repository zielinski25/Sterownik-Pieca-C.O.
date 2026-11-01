#!/usr/bin/env python3
"""Audyt WSZYSTKICH wywolan setTimeout/setHandshakeTimeout w main_centrala.cpp.

Po co: setHandshakeTimeout() przyjmuje SEKUNDY (mnozy przez 1000), ale
setTimeout() zalezy od klasy:

  * WiFiClientSecure::setTimeout(uint32_t seconds)  -> SEKUNDY
  * WiFiClient::setTimeout(uint16_t seconds)        -> SEKUNDY
  * HTTPClient::setTimeout(uint16_t ms)             -> MILISEKUNDY

Zeby orzec, ktora wersja jest w danym miejscu, trzeba wiedziec, JAKIEGO TYPU
jest zmienna. Ten skrypt znajduje deklaracje zmiennych i przypisuje kazde
wywolanie do typu — a potem liczy, ile realnie wychodzi z podanej liczby.

Uzycie:
    python3 audyt_timeoutow.py --src firmware-patch/v3.57.8-raport-i-wersja/src/main_centrala.cpp
    python3 audyt_timeoutow.py --src ... --json
"""
import argparse
import json
import re
import sys

# Typy znane z arduino-esp32 2.0.x: (jednostka parametru, mnoznik na ms)
TYPY = {
    "WiFiClientSecure": ("sekundy", 1000),
    "WiFiClient": ("sekundy", 1000),
    "HTTPClient": ("milisekundy", 1),
}


def bez_komentarzy(tekst):
    """Komentarze -> spacje, z zachowaniem dlugosci i numerow linii."""
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


def zbierz_deklaracje(kod):
    """Nazwa zmiennej -> typ, dla deklaracji w calym pliku.

    Bierze pod uwage: 'WiFiClientSecure fbSSLClient;', 'static WiFiClientSecure
    tgClient;', 'WiFiClientSecure client;' (lokalne tez — sa w jednej funkcji,
    ale typ jest ten sam w calym pliku), referencje i wskazniki.
    """
    dekl = {}
    wzor = re.compile(
        r"(?:^|[;{}\n])\s*(?:static\s+|const\s+|extern\s+)*"
        r"(WiFiClientSecure|WiFiClient|HTTPClient)\s*(?:[*&]\s*)?([A-Za-z_]\w*)\s*(?=[;=(,\[])")
    for m in wzor.finditer(kod):
        dekl.setdefault(m.group(2), m.group(1))
    return dekl


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    zrodlo = open(args.src, encoding="utf-8", errors="replace").read()
    kod = bez_komentarzy(zrodlo)
    dekl = zbierz_deklaracje(kod)

    wpisy = []
    for m in re.finditer(r"([A-Za-z_]\w*)\s*\.\s*(setHandshakeTimeout|setTimeout)\s*\(\s*(\d+)\s*\)", kod):
        obiekt, metoda, wartosc = m.group(1), m.group(2), int(m.group(3))
        linia = kod.count("\n", 0, m.start()) + 1
        typ = dekl.get(obiekt)
        if metoda == "setHandshakeTimeout":
            jednostka, mnoznik = "sekundy", 1000
            oczekiwane_ms = 8000
        elif typ in TYPY:
            jednostka, mnoznik = TYPY[typ]
            oczekiwane_ms = 8000
        else:
            jednostka, mnoznik = "?", 1
            oczekiwane_ms = None
        faktyczne_ms = wartosc * mnoznik
        wpisy.append({
            "linia": linia,
            "obiekt": obiekt,
            "typ": typ or "?",
            "metoda": metoda,
            "wartosc": wartosc,
            "jednostka": jednostka,
            "faktyczne_ms": faktyczne_ms,
            "faktyczne_s": round(faktyczne_ms / 1000.0, 3),
            # "zle" = wynik odbiega od zamiaru 8 s o wiecej niz 2x
            "podejrzane": oczekiwane_ms is not None and faktyczne_ms > oczekiwane_ms * 2,
            # "do zmiany" = wartosc inna niz cel (dla handshake: != 8 s)
            "do_zmiany": (wartosc != 8) if metoda == "setHandshakeTimeout"
                          else (faktyczne_ms > 20000),
            # "zgodne z zamiarem 8 s" — w granicach 4..16 s
            "okolo_8s": oczekiwane_ms is not None and 4000 <= faktyczne_ms <= 16000,
        })

    handshake = [w for w in wpisy if w["metoda"] == "setHandshakeTimeout"]
    auto = [w for w in wpisy if w["metoda"] == "setTimeout"]
    podejrzane = [w for w in wpisy if w["podejrzane"]]

    if args.json:
        print(json.dumps({"wszystkie": wpisy,
                          "handshake": len(handshake),
                          "settimeout": len(auto),
                          "podejrzane": podejrzane,
                          "deklaracje": dekl}, ensure_ascii=False, indent=2))
        return 0

    print(f"Zrodlo: {args.src}")
    print(f"Znane zmienne: {', '.join(f'{k}:{v}' for k, v in sorted(dekl.items()))}")
    print(f"\n=== setHandshakeTimeout (zawsze sekundy): {len(handshake)} miejsc ===")
    from collections import Counter
    for w, ile in Counter(x["wartosc"] for x in handshake).most_common():
        print(f"  wartosc {w:>5} -> limit {w*1000/1000:>6.0f} s  x {ile}")
    print("\n=== setTimeout (jednostka zalezna od klasy) ===")
    for w in auto:
        znak = "!!" if w["podejrzane"] else "  "
        print(f"  {znak} linia {w['linia']:>6}  {w['obiekt']}.setTimeout({w['wartosc']})  "
              f"[{w['typ']}, {w['jednostka']}] = {w['faktyczne_s']} s")
    print("\n=== do zmiany w 3.57.9 ===")
    sek = [w for w in auto if w["jednostka"] == "sekundy"]
    zle_sek = [w for w in sek if w["do_zmiany"]]
    dobre_sek = [w for w in sek if not w["do_zmiany"]]
    mili = [w for w in auto if w["jednostka"] == "milisekundy"]
    hs_do_zmiany = [w for w in handshake if w["do_zmiany"]]
    print(f"  setHandshakeTimeout (zawsze sekundy):        {len(handshake)}")
    print(f"    - DO POPRAWY (nie 8 s):                    {len(hs_do_zmiany)}")
    print(f"    - juz poprawne (8 s):                      {len(handshake) - len(hs_do_zmiany)}")
    print(f"  setTimeout w SEKUNDACH razem:                {len(sek)}")
    print(f"    - BŁĘDNE (do poprawy):                     {len(zle_sek)}")
    print(f"    - POPRAWNE (setTimeout(4) - nie ruszamy):  {len(dobre_sek)}")
    print(f"  setTimeout w milisekundach (nie ruszamy):    {len(mili)}")
    print(f"  RAZEM DO ZMIANY:                             {len(hs_do_zmiany) + len(zle_sek)}")
    if not hs_do_zmiany and not zle_sek:
        print("  (wszystko juz w sekundach - nic do zrobienia)")
    return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
