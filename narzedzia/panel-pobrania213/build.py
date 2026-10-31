#!/usr/bin/env python3
"""213: sprawdz patch i wyciagnij z niego PRAWDZIWE kawalki do testow.

Ten skrypt nie ufa temu, co jest w paczce. Robi wszystko od zera:
  1. bierze przypiety plik v3.55.8 (SHA-256 sprawdzany),
  2. naklada 211 + 212 + 213 -> v3.56.1,
  3. to samo osiaga z patcha skumulowanego (v3.55.8 -> v3.56.1) i porownuje
     plik bajt w bajt,
  4. regeneruje HistoriaPobranie.h i host/Serwer213.h z plikow etapu 212
     i porownuje z tym, co lezy w paczce (dowod, ze to 212 + wstawki),
  5. sprawdza miejsca, ktore musza byc, i miejsca, ktorych byc nie moze
     (staly token w HTML panelu!),
  6. wyciaga z pliku po patchu realne kawalki: adapter losowosci, rejestracje
     tras i tekst panelu -> Extracted213.inc.

Uruchomienie:
    python3 -B narzedzia/panel-pobrania213/build.py
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

import zrob_patch as Z

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
NARZ = os.path.join(REPO, "narzedzia")
FIRMWARE = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
ZRODLO = os.path.join(FIRMWARE, "main_centrala.cpp")
D212 = os.path.join(NARZ, "pobranie-trasa212")
PATCH211 = os.path.join(NARZ, "wysylka-zapinka211", "main_centrala-v3.55.8-to-v3.55.9.patch")
PATCH212 = os.path.join(D212, "main_centrala-v3.55.9-to-v3.56.0.patch")
PATCH213_INC = os.path.join(TU, "main_centrala-v3.56.0-to-v3.56.1.patch")
PATCH213_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.1.patch")
WYJSCIE = os.path.join(TU, "Extracted213.inc")

SHA_ZRODLA = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def wczytaj(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def naloz(katalog, patch):
    t = subprocess.run(["patch", "-p1", "-i", patch], cwd=katalog, capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("patch %s nie nalozyl sie: %s%s" % (os.path.basename(patch), t.stdout, t.stderr))


def wyciagnij_rejestracje(tekst):
    """Zamienia lambdy wifiWebServer.on(...) w zwykle funkcje o tej samej tresci."""
    wzor = re.compile(r'wifiWebServer\.on\("([^"]+)",\s*HTTP_(\w+),\s*'
                      r'\[\]\(AsyncWebServerRequest \*request\) \{\n(.*?)\n  \}\);', re.S)
    nazwy = {"/history": "obslugaHistory", "/history/long": "obslugaHistoryLong",
             "/history/link": "obslugaHistoryLink"}
    wynik = {}
    for sciezka, metoda, cialo in wzor.findall(tekst):
        if sciezka not in nazwy:
            continue   # inne trasy centrali nie sa przedmiotem tego etapu
        wynik[sciezka] = (metoda, cialo, nazwy[sciezka])
    return wynik


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS build213 zrodlo v3.55.8 sha256 zgodny z 210")

    # --- 1. regeneracja plikow "212 + wstawki" ---------------------------
    if Z.dodaj_pobrania(wczytaj(Z.POBRANIA212)) != wczytaj(Z.WYJSCIE_POBRANIA):
        raise SystemExit("HistoriaPobranie.h nie jest regenerowalny z etapu 212")
    print("PASS build213 HistoriaPobranie.h = plik 212 + wstawki 213 (bajt w bajt)")
    if Z.dodaj_serwer(wczytaj(Z.SERWER212)) != wczytaj(Z.WYJSCIE_SERWER):
        raise SystemExit("host/Serwer213.h nie jest regenerowalny z etapu 212")
    print("PASS build213 host/Serwer213.h = model 212 + parametry zapytania")

    # --- 2. patch: lancuch i skumulowany musza dac ten sam plik ----------
    tmp = tempfile.mkdtemp(prefix="b213-")
    try:
        lancuch = os.path.join(tmp, "lancuch")
        kum = os.path.join(tmp, "kum")
        for d in (lancuch, kum):
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(os.path.join(FIRMWARE, "secrets.h.example"),
                            os.path.join(d, "secrets.h.example"))
        naloz(lancuch, PATCH211)
        naloz(lancuch, PATCH212)
        naloz(lancuch, PATCH213_INC)
        naloz(kum, PATCH213_KUM)
        r_lancuch = wczytaj(os.path.join(lancuch, "main_centrala.cpp"))
        r_kum = wczytaj(os.path.join(kum, "main_centrala.cpp"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if r_lancuch != r_kum:
        raise SystemExit("patch skumulowany 3.55.8->3.56.1 != lancuch 211+212+213")
    print("PASS build213 patch skumulowany == 211 + 212 + 213 (bajt w bajt)")
    sha_wyniku = hashlib.sha256(r_lancuch.encode()).hexdigest()
    print("PASS build213 wynik sha256 %s" % sha_wyniku)

    # --- 3. co musi byc ------------------------------------------------
    musza = [
        ('#include "HistoriaBilety.h"   // [v3.56.1 HISTORIA-BILETY]', "brak include'a biletow"),
        ('static const char* FIRMWARE_VERSION = "v3.56.1";', "brak FIRMWARE_VERSION v3.56.1"),
        ("static historia213::Bilety historiaBilety(historiaLosuj213, historiaZegar213);",
         "brak obiektu biletow"),
        ("historiaPobranie.ustawBilety(&historiaBilety);", "brak podpiecia biletow do trasy"),
        ('wifiWebServer.on("/history/link", HTTP_GET,', "brak rejestracji /history/link"),
        ('id="histBtn"', "brak przycisku w panelu"),
        ("'/history/link?stream=' + strumien", "panel nie pyta o bilet"),
        ('wifiWebServer.on("/history", HTTP_ANY,', "brak trasy 212 (krotka)"),
        ('wifiWebServer.on("/history/long", HTTP_ANY,', "brak trasy 212 (dluga)"),
    ]
    for fragment, opis in musza:
        if fragment not in r_lancuch:
            raise SystemExit("build213: %s (%s)" % (opis, fragment))
    print("PASS build213 wszystkie elementy trasy i panelu sa w pliku")

    # --- 4. czego byc nie moze: staly token w HTML ----------------------
    panel = Z.PANEL
    if panel not in r_lancuch:
        raise SystemExit("build213: tekst panelu w pliku rozni sie od generatora")
    for zakaz in ("HISTORIA_TOKEN", "SECRET_HISTORIA_TOKEN", "Bearer "):
        if zakaz in panel:
            raise SystemExit("build213: staly token/sekret w panelu (%s)" % zakaz)
    if "'/history?k='" not in panel:
        raise SystemExit("build213: panel nie otwiera /history?k=")
    if "length !== 32" not in panel:
        raise SystemExit("build213: panel nie sprawdza dlugosci biletu")
    print("PASS build213 panel nie zawiera stalego tokenu, sprawdza bilet")

    # --- 5. ekstrakcja realnych kawalkow -------------------------------
    rej = wyciagnij_rejestracje(r_lancuch)
    for sciezka in ("/history", "/history/long", "/history/link"):
        if sciezka not in rej:
            raise SystemExit("build213: brak rejestracji %s do ekstrakcji" % sciezka)
    if rej["/history/link"][0] != "GET":
        raise SystemExit("build213: /history/link nie jest GET")
    if rej["/history"][0] != "ANY" or rej["/history/long"][0] != "ANY":
        raise SystemExit("build213: trasy 212 zmienily metode")

    losuj = Z.wczytaj(Z.WYJSCIE_POBRANIA)  # kontrola spojnosci nazw
    if "historiaLosuj213" not in r_lancuch or "historiaZegar213" not in r_lancuch:
        raise SystemExit("build213: brak adapterow losowosci/zegara")

    # funkcje adaptera - od 'static void historiaLosuj213' do konca historiaZegar213
    i = r_lancuch.index("static void historiaLosuj213")
    j = r_lancuch.index("static historia213::Bilety historiaBilety", i)
    adaptery = r_lancuch[i:j].rstrip() + "\n"

    czesci = [
        "// WYGENEROWANE przez narzedzia/panel-pobrania213/build.py - nie edytowac.\n",
        "// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp\n",
        "//         + main_centrala-v3.56.0-to-v3.56.1.patch (sha256 %s)\n" % sha256_sciezki(PATCH213_INC),
        "// Ponizej: prawdziwy kod z pliku po patchu v3.56.1.\n\n",
        adaptery, "\n",
    ]
    for sciezka in ("/history", "/history/long", "/history/link"):
        metoda, cialo, nazwa = rej[sciezka]
        czesci.append("// %s (%s) - tresc lambdy z main_centrala.cpp\n" % (sciezka, metoda))
        czesci.append("static void %s(AsyncWebServerRequest *request) {\n%s\n}\n\n" % (nazwa, cialo))
    czesci.append("// Tekst panelu (przycisk + JS), dokladnie taki, jaki trafia do WWW.\n")
    czesci.append('static const char *PANEL_213 = R"PANEL213(')
    czesci.append(panel)
    czesci.append(')PANEL213";\n')

    tresc = "".join(czesci)
    with open(WYJSCIE, "w", encoding="utf-8", newline="") as f:
        f.write(tresc)
    print("PASS build213 Extracted213.inc %d linii" % len(tresc.split("\n")))
    print("PASS build213 HistoriaPobranie.h %d linii" % len(losuj.split("\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
