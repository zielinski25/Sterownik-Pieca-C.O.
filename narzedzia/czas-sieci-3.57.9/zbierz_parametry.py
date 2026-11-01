#!/usr/bin/env python3
"""Wyciaga z main_centrala.cpp (3.57.8) parametry timeoutow sieciowych.

Ten skrypt nie ocenia kodu — tylko czyta, co w nim naprawde stoi:
  * jakie wartosci trafiaja do setHandshakeTimeout()/setTimeout(),
  * czy pobierzPrognozeDeszczu() ponawia probe (petla / wiecej niz jedno GET),
  * jakie sa progi nadzoru i bramka 15-minutowa.

Dzieki temu model czasu (model_czasu.py) liczy na LICZBACH Z FIRMWARE,
a nie na przepisanych z pamieci.

Uzycie:
    python3 zbierz_parametry.py --src /sciezka/main_centrala.cpp
    python3 zbierz_parametry.py --src ... --json      # maszynowo
"""
import argparse
import hashlib
import json
import re
import sys


def wytnij_funkcje(zrodlo, sygnatura):
    """Zwraca (tresc_funkcji, numer_linii_start) dla 'typ nazwa(...){...}'.

    Prosty balans nawiasow z pomijaniem komentarzy, literali znakowych
    i napisowych — wystarczajacy dla tego pliku (brak raw stringow).
    """
    idx = zrodlo.find(sygnatura)
    if idx < 0:
        return None, None
    linia = zrodlo.count("\n", 0, idx) + 1
    start = zrodlo.find("{", idx)
    if start < 0:
        return None, None
    glebokosc = 0
    i = start
    n = len(zrodlo)
    while i < n:
        c = zrodlo[i]
        if c == "/" and i + 1 < n and zrodlo[i + 1] == "/":
            i = zrodlo.find("\n", i)
            if i < 0:
                break
            continue
        if c == "/" and i + 1 < n and zrodlo[i + 1] == "*":
            i = zrodlo.find("*/", i + 2)
            if i < 0:
                break
            i += 2
            continue
        if c in "\"'":
            cyt = c
            i += 1
            while i < n and zrodlo[i] != cyt:
                if zrodlo[i] == "\\":
                    i += 1
                i += 1
            i += 1
            continue
        if c == "{":
            glebokosc += 1
        elif c == "}":
            glebokosc -= 1
            if glebokosc == 0:
                return zrodlo[start:i + 1], linia
        i += 1
    return None, None


def bez_komentarzy(tekst):
    """Zamienia komentarze na spacje, ZACHOWUJAC dlugosc i linie.

    Musi byc reczne, nie regexowe: w tym pliku sa dlugie literaly z HTML/JS,
    ktore zawieraja '//' i '/*' — regexowe czyszczenie zjadloby razem z nimi
    prawdziwy kod (i przesunelo numery linii). Literaly sa tu pomijane.
    """
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
            out[i] = " "
            out[i + 1] = " "
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


def wartosci_wywolan(tekst, wzor):
    return [int(m.group(1)) for m in re.finditer(wzor, tekst)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--json", action="store_true", help="tylko JSON na stdout")
    args = ap.parse_args()

    zrodlo = open(args.src, encoding="utf-8", errors="replace").read()
    sha = hashlib.sha256(zrodlo.encode("utf-8", "replace")).hexdigest()

    cialo, linia = wytnij_funkcje(zrodlo, "void pobierzPrognozeDeszczu()")
    if cialo is None:
        print("BLAD: nie znalazlem pobierzPrognozeDeszczu()", file=sys.stderr)
        return 1
    kod = bez_komentarzy(cialo)

    # Ile razy funkcja faktycznie wychodzi do sieci i czy robi to w petli.
    get_y = len(re.findall(r"\.GET\s*\(", kod))
    get_w_petli = 0
    # kazda petla for/while: sprawdz, czy w jej ciele jest wywolanie sieciowe
    for m in re.finditer(r"\b(for|while)\s*\(", kod):
        i = m.end() - 1
        glebokosc = 0
        while i < len(kod):
            if kod[i] == "(":
                glebokosc += 1
            elif kod[i] == ")":
                glebokosc -= 1
                if glebokosc == 0:
                    break
            i += 1
        otwarcie = kod.find("{", i)
        if otwarcie < 0:
            continue
        glebokosc = 0
        j = otwarcie
        while j < len(kod):
            if kod[j] == "{":
                glebokosc += 1
            elif kod[j] == "}":
                glebokosc -= 1
                if glebokosc == 0:
                    break
            j += 1
        cialo_petli = kod[otwarcie:j]
        if re.search(r"\.GET\s*\(|\.begin\s*\(|\.connect\s*\(", cialo_petli):
            get_w_petli += 1

    dane = {
        "zrodlo": {"plik": args.src, "sha256": sha, "funkcja_linia": linia},
        "deszcz": {
            "setHandshakeTimeout": wartosci_wywolan(
                kod, r"setHandshakeTimeout\s*\(\s*(\d+)\s*\)"),
            "client_setTimeout": wartosci_wywolan(
                kod, r"\bclient\.setTimeout\s*\(\s*(\d+)\s*\)"),
            "http_setTimeout": wartosci_wywolan(
                kod, r"\bhttp\.setTimeout\s*\(\s*(\d+)\s*\)"),
            "get_w_funkcji": get_y,
            "get_w_petli": get_w_petli,
            "petle_w_funkcji": len(re.findall(r"\b(?:for|while)\s*\(", kod)),
            "setConnectTimeout": wartosci_wywolan(
                kod, r"setConnectTimeout\s*\(\s*(\d+)\s*\)"),
            "setFollowRedirects": len(re.findall(r"setFollowRedirects", kod)),
        },
        "wszystkie_setHandshakeTimeout": [
            {"linia": zrodlo.count("\n", 0, m.start()) + 1, "wartosc": int(m.group(1))}
            for m in re.finditer(r"setHandshakeTimeout\s*\(\s*(\d+)\s*\)", bez_komentarzy(zrodlo))
        ],
        "stale": {},
    }

    for nazwa in ("NADZOR_PROG_MS", "NADZOR_MAX_RESTARTOW", "NADZOR_LICZNIK_WAZNY_MS",
                  "NADZOR_ROZBIEG_MS"):
        m = re.search(rf"{nazwa}\s*=\s*([0-9]+)UL?", zrodlo)
        if m:
            dane["stale"][nazwa] = int(m.group(1))
    m = re.search(r"OW_DESZCZ_FETCH_INTERVAL_MS\s*=\s*(\d+)UL\s*\*\s*(\d+)UL\s*\*\s*(\d+)UL",
                  zrodlo)
    if m:
        dane["stale"]["OW_DESZCZ_FETCH_INTERVAL_MS"] = int(m.group(1)) * int(m.group(2)) * int(m.group(3))

    if args.json:
        print(json.dumps(dane, indent=2))
        return 0

    print(f"Zrodlo: {args.src}")
    print(f"  SHA-256: {sha}")
    print(f"  pobierzPrognozeDeszczu(): linia {linia}")
    d = dane["deszcz"]
    print("\n-- co ustawia funkcja --")
    print(f"  setHandshakeTimeout(...) : {d['setHandshakeTimeout']}")
    print(f"  client.setTimeout(...)   : {d['client_setTimeout']}")
    print(f"  http.setTimeout(...)     : {d['http_setTimeout']}")
    print(f"  setConnectTimeout(...)   : {d['setConnectTimeout'] or 'BRAK (domyslne 5000 ms)'}")
    print(f"  setFollowRedirects(...)  : {d['setFollowRedirects'] or 'BRAK (domyslnie wylaczone)'}")
    print("\n-- czy ponawia probe --")
    print(f"  http.GET() w funkcji     : {d['get_w_funkcji']}")
    print(f"  GET w ciele petli        : {d['get_w_petli']}")
    print(f"  petle w funkcji (liczac razem): {d['petle_w_funkcji']} "
          f"(wszystkie do skanowania JSON-a, bez sieci)")
    print("\n-- wszystkie miejsca z setHandshakeTimeout w pliku --")
    for w in dane["wszystkie_setHandshakeTimeout"]:
        print(f"  linia {w['linia']:>6}: {w['wartosc']}")
    print("\n-- progi --")
    for k, v in dane["stale"].items():
        print(f"  {k} = {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
