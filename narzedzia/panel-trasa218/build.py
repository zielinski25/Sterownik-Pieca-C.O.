#!/usr/bin/env python3
"""218: zbuduj v3.56.4 od zera i wyciagnij z niej PRAWDZIWE kawalki.

Ten skrypt nie ufa temu, co lezy w paczce:
  1. bierze przypiety plik v3.55.8 (SHA-256 sprawdzany),
  2. naklada 211 + 212 + 213 + 214 + 216 -> v3.56.3 (SHA sprawdzany z 216),
  3. naklada patch 218 -> v3.56.4, porownuje z patchem skumulowanym,
  4. liczy tabele tras w v3.56.3 i v3.56.4 (trasy.py):
       - w v3.56.3 MUSZA byc dokladnie dwie martwe trasy (/history/long,
         /history/link przechwycone przez /history),
       - w v3.56.4 NIE MOZE byc zadnej martwej trasy,
  5. wyciaga z v3.56.4 realny tekst trzech rejestracji -> Extracted218.inc,
  6. wyciaga z v3.56.4 realny fragment panelu (przyciski + skrypt)
     -> Panel218.html (dla panel218.mjs),
  7. sprawdza, ze naglowki uzyte w tescie sa te z poprzednich etapow
     (HistoriaPobranie.h v3.56.3, HistoriaBilety.h v3.56.1).

Uruchomienie:
    python3 -B narzedzia/panel-trasa218/build.py
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
NARZ = os.path.join(REPO, "narzedzia")
FIRMWARE = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
ZRODLO = os.path.join(FIRMWARE, "main_centrala.cpp")
PRZYKLAD = os.path.join(FIRMWARE, "secrets.h.example")

P211 = os.path.join(NARZ, "wysylka-zapinka211", "main_centrala-v3.55.8-to-v3.55.9.patch")
P212 = os.path.join(NARZ, "pobranie-trasa212", "main_centrala-v3.55.9-to-v3.56.0.patch")
P213 = os.path.join(NARZ, "panel-pobrania213", "main_centrala-v3.56.0-to-v3.56.1.patch")
P214 = os.path.join(NARZ, "brama-przed-wgraniem214", "main_centrala-v3.56.1-to-v3.56.2.patch")
P216 = os.path.join(NARZ, "od-przycisku216", "main_centrala-v3.56.2-to-v3.56.3.patch")
P218_INC = os.path.join(TU, "main_centrala-v3.56.3-to-v3.56.4.patch")
P218_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.4.patch")

WYJSCIE_INC = os.path.join(TU, "Extracted218.inc")
WYJSCIE_PANEL = os.path.join(TU, "Panel218.html")
WYJSCIE_TRASY = os.path.join(TU, "Trasy218.json")

NAGLOWEK_POBRANIA = os.path.join(NARZ, "od-przycisku216", "HistoriaPobranie.h")
NAGLOWEK_BILETY = os.path.join(NARZ, "panel-pobrania213", "HistoriaBilety.h")

SHA_ZRODLA = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"
SHA_V3563 = "f51b0b0749e2c1e3a8b5c02848a9c0e4902e0b641d6a7313a42304bbbcef91b3"
SHA_V3564 = "f89874f3390009acd79632db1031d04cd90e67435eda68e2007ea7c4b3168e38"
SHA_NAGLOWKA_POBRANIA = "b58d1a4c299e339798d3f229ad81177b0ee34f44dd37dae1776ba48279a4ede8"
SHA_NAGLOWKA_BILETY = "339f83fdda1219f7c925be468787c2d102c756b2da0366fab1833d8f88dbde30"

TRASY = ("/history", "/history/long", "/history/link")


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def wczytaj(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def zapisz(p, t):
    with open(p, "w", encoding="utf-8", newline="") as f:
        f.write(t)


def naloz(katalog, patch):
    t = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-i", patch],
                       cwd=katalog, capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("patch %s nie nalozyl sie: %s%s"
                         % (os.path.basename(patch), t.stdout, t.stderr))


def zbuduj(tmp):
    """v3.56.3 i v3.56.4 w dwoch katalogach, obie drogi (przyrostowa i skumulowana)."""
    drogi = {}
    for nazwa, patche in (("b", (P211, P212, P213, P214, P216)),
                          ("c", (P211, P212, P213, P214, P216, P218_INC)),
                          ("k", (P218_KUM,))):
        d = os.path.join(tmp, nazwa)
        os.makedirs(d)
        shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
        shutil.copyfile(PRZYKLAD, os.path.join(d, "secrets.h.example"))
        for p in patche:
            naloz(d, p)
        drogi[nazwa] = wczytaj(os.path.join(d, "main_centrala.cpp"))
    return drogi


def trasy(tekst, tmp, nazwa):
    """Zapisuje tekst do pliku i uruchamia na nim trasy.py."""
    plik = os.path.join(tmp, "do-analizy-%s.cpp" % nazwa)
    with open(plik, "w", encoding="utf-8", newline="") as f:
        f.write(tekst)
    cel = os.path.join(tmp, "trasy-%s.json" % nazwa)
    t = subprocess.run([sys.executable, "-B", os.path.join(TU, "trasy.py"), plik, cel],
                       capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("trasy.py nie zadzialal: %s%s" % (t.stdout, t.stderr))
    with open(cel, "r", encoding="utf-8") as f:
        return json.load(f)


def wytnij_rejestracje(tekst):
    """Wycinanie trzech rejestracji /history* - doslownie z pliku, w kolejnosci."""
    linie = tekst.split("\n")
    wynik = []
    for trasa in TRASY:
        naglowek = 'wifiWebServer.on(AsyncURIMatcher::exact("%s")' % trasa
        indeksy = [i for i, l in enumerate(linie) if l.startswith("  " + naglowek)]
        if len(indeksy) != 1:
            raise SystemExit("oczekiwano jednej rejestracji %s, jest %d" % (trasa, len(indeksy)))
        i = indeksy[0]
        blok = [linie[i]]
        j = i + 1
        while j < len(linie) and linie[j] != "  });":
            blok.append(linie[j])
            j += 1
        if j >= len(linie):
            raise SystemExit("rejestracja %s nie ma konca '  });'" % trasa)
        blok.append(linie[j])
        wynik.extend(blok)
    return wynik


def wytnij_adaptery(tekst):
    """Blok zrodla losowosci, zegara, biletow i podpiecia - doslownie z pliku."""
    linie = tekst.split("\n")
    start = [i for i, l in enumerate(linie) if l.startswith("static void historiaLosuj213(")]
    koniec = [i for i, l in enumerate(linie)
              if l.startswith("static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;")]
    if len(start) != 1 or len(koniec) != 1 or koniec[0] <= start[0]:
        raise SystemExit("nie umiem wyciac bloku adapterow biletow")
    return linie[start[0]:koniec[0] + 1]


def wytnij_panel(tekst):
    """Fragment panelu: od komentarza HTML do konca bloku <script>."""
    start = tekst.find("<!-- [v3.56.1 HISTORIA-POBRANIE]")
    if start < 0:
        raise SystemExit("brak znacznika poczatku panelu")
    skrypt = tekst.find("<script>", start)
    if skrypt < 0:
        raise SystemExit("panel nie ma bloku <script>")
    koniec = tekst.find("</script>", skrypt)
    if koniec < 0:
        raise SystemExit("panel nie ma konca </script>")
    return tekst[start:koniec + len("</script>")]


def sprawdz_panel(fragment):
    """To, co musi byc w panelu, i to, czego byc nie moze."""
    for wymagane in ('id="histBtn"', 'id="histPanel"', 'id="histShort"', 'id="histLong"',
                     'id="histInfo"', "document.getElementById('histBtn')",
                     "document.getElementById('histPanel')",
                     "fetch('/history/link?stream='", "var trasa =",
                     "'/history/long' : '/history'"):
        if wymagane not in fragment:
            raise SystemExit("w panelu brakuje: %s" % wymagane)
    if "SECRET_HISTORIA_TOKEN" in fragment or "HISTORIA_TOKEN" in fragment:
        raise SystemExit("w panelu jest staly token - tego nie wolno")
    if fragment.count("<script>") != 1 or fragment.count("</script>") != 1:
        raise SystemExit("panel musi miec dokladnie jeden blok <script>")
    if "&s=" in fragment:
        raise SystemExit("panel dokleja nieuzywany parametr s=")


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS build218 zrodlo v3.55.8 sha256 zgodny z 210")

    for sciezka, oczekiwany, opis in (
            (NAGLOWEK_POBRANIA, SHA_NAGLOWKA_POBRANIA, "HistoriaPobranie.h v3.56.3"),
            (NAGLOWEK_BILETY, SHA_NAGLOWKA_BILETY, "HistoriaBilety.h v3.56.1")):
        if sha256_sciezki(sciezka) != oczekiwany:
            raise SystemExit("%s nie jest tym z poprzedniego etapu" % opis)
        print("PASS build218 %s sha256 zgodny" % opis)

    tmp = tempfile.mkdtemp(prefix="b218-")
    try:
        pliki = zbuduj(tmp)
        if hashlib.sha256(pliki["b"].encode()).hexdigest() != SHA_V3563:
            raise SystemExit("v3.56.3 nie zgadza sie z etapem 216")
        print("PASS build218 v3.56.3 (przed poprawka) sha256 zgodny z etapem 216")
        if pliki["c"] != pliki["k"]:
            raise SystemExit("patch przyrostowy i skumulowany daja rozne pliki")
        if hashlib.sha256(pliki["c"].encode()).hexdigest() != SHA_V3564:
            raise SystemExit("v3.56.4 ma inny SHA niz zapisany w zrob_patch.py")
        print("PASS build218 v3.56.4 %d linii, sha256 zgodny z zrob_patch.py"
              % len(pliki["c"].split("\n")))

        przed = trasy(pliki["b"], tmp, "v3563")
        po = trasy(pliki["c"], tmp, "v3564")
        if przed["tras"] != po["tras"]:
            raise SystemExit("liczba tras zmienila sie: %d -> %d" % (przed["tras"], po["tras"]))
        martwe_przed = {m["martwa"] for m in przed["martwe"]}
        oczekiwane_przed = {'"/history/long" HTTP_ANY', '"/history/link" HTTP_GET'}
        if martwe_przed != oczekiwane_przed:
            raise SystemExit("v3.56.3 nie ma dokladnie tych dwoch martwych tras: %r" % (sorted(martwe_przed),))
        for m in przed["martwe"]:
            if m["przechwytuje"] != '"/history" HTTP_ANY':
                raise SystemExit("martwe trasy przechwytuje nie /history: %r" % (m,))
        if po["martwe"]:
            raise SystemExit("v3.56.4 nadal ma martwe trasy: %r" % (po["martwe"],))
        print("PASS build218 trasy: %d w pliku, w v3.56.3 martwe 2 (/history/long, /history/link),"
              " w v3.56.4 martwych 0" % po["tras"])

        for t in po["tabela"]:
            if (t["sciezka"] or "").startswith("/history") and t["typ"] != "exact":
                raise SystemExit("trasa %s nie jest exact: %s" % (t["sciezka"], t["typ"]))
        if po["nieznane"]:
            raise SystemExit("sa trasy, ktorych nie umiem rozstrzygnac statycznie")
        for t in po["tabela"]:
            if t["typ"] == "prefix" and t["sciezka"] == "/":
                raise SystemExit("nieoczekiwany prefix /")
        print("PASS build218 wszystkie trasy /history* uzywaja exact(), "
              "gole napisy: %d (zaden nie jest /history*)" % po["gole_napisy"])

        linie = wytnij_rejestracje(pliki["c"])
        if len(linie) != 9:
            raise SystemExit("rejestracje maja %d linii, oczekiwano 9" % len(linie))
        adaptery = wytnij_adaptery(pliki["c"])
        for wymagane in ("historiaLosuj213", "historiaZegar213",
                         "static historia213::Bilety historiaBilety(",
                         "historiaPobranie.ustawBilety(&historiaBilety);"):
            if not any(wymagane in l for l in adaptery):
                raise SystemExit("w bloku adapterow brakuje: %s" % wymagane)
        tresc_inc = (
            "// WYGENEROWANE przez narzedzia/panel-trasa218/build.py - nie edytowac.\n"
            "// Zrodlo: main_centrala.cpp v3.56.4 (sha256 %s).\n"
            "// Dwa doslowne fragmenty z pliku po patchu 218:\n"
            "//  1) adaptery biletow (losowosc, zegar) i podpiecie ich do trasy,\n"
            "//  2) trzy rejestracje tras /history* - tyle, ile widzi serwer.\n"
            "\n" % SHA_V3564)
        tresc_inc += "\n".join(adaptery) + "\n\n"
        tresc_inc += "inline void zarejestrujTrasy218() {\n" + "\n".join(linie) + "\n}\n"
        zapisz(WYJSCIE_INC, tresc_inc)
        print("PASS build218 Extracted218.inc %d linii" % len(tresc_inc.split("\n")))

        panel = wytnij_panel(pliki["c"])
        sprawdz_panel(panel)
        zapisz(WYJSCIE_PANEL, panel + "\n")
        print("PASS build218 Panel218.html %d linii (przyciski + skrypt z pliku)" % len(panel.split("\n")))

        with open(WYJSCIE_TRASY, "w", encoding="utf-8") as f:
            json.dump({"v3.56.3": przed, "v3.56.4": po}, f, indent=2, ensure_ascii=False)
            f.write("\n")
        print("PASS build218 Trasy218.json zapisany")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
