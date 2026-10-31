#!/usr/bin/env python3
"""216: zbuduj plik po patchu v3.56.3 i wyciagnij z niego PRAWDZIWE kawalki.

Ten skrypt nie ufa temu, co lezy w katalogu etapu. Robi wszystko od zera:
  1. bierze przypiety plik v3.55.8 (SHA-256 sprawdzany),
  2. naklada 211 + 212 + 213 + 214 + 216 -> v3.56.3,
  3. to samo osiaga z patcha skumulowanego (v3.55.8 -> v3.56.3) i porownuje
     plik bajt w bajt,
  4. sprawdza, co ma byc, a czego byc nie moze (stary adres w JS, staly token),
  5. wyciaga z pliku po patchu:
       panel/panel.html      - DOKLADNY tekst panelu (HTML + <script>) z WWW,
       panel/panel.js        - sam skrypt panelu (do "node --check" i do DOM),
       panel/panel-stary.html - panel z v3.56.2, do KONTROLI NEGATYWNEJ,
       Trasy216.inc          - lista tras webserwera z pliku (dowód, ze trasa,
                               ktora otwiera panel, istnieje w firmware),
       Extracted216.inc      - adaptery losowosci/zegara i tresci trzech lambd.

Uruchomienie:
    python3 -B narzedzia/od-przycisku216/build.py
"""
import hashlib
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

PATCH_INC = os.path.join(TU, "main_centrala-v3.56.2-to-v3.56.3.patch")
PATCH_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.3.patch")
PATCHE_WSTECZ = [
    os.path.join(NARZ, "wysylka-zapinka211", "main_centrala-v3.55.8-to-v3.55.9.patch"),
    os.path.join(NARZ, "pobranie-trasa212", "main_centrala-v3.55.9-to-v3.56.0.patch"),
    os.path.join(NARZ, "panel-pobrania213", "main_centrala-v3.56.0-to-v3.56.1.patch"),
    os.path.join(NARZ, "brama-przed-wgraniem214", "main_centrala-v3.56.1-to-v3.56.2.patch"),
]

SHA_ZRODLA = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"

PANEL_START = "<!-- [v3.56.1 HISTORIA-POBRANIE]"
WYJSCIE_HANDLER = os.path.join(TU, "Extracted216.inc")
WYJSCIE_TRASY = os.path.join(TU, "Trasy216.inc")
KATALOG_PANELU = os.path.join(TU, "panel")


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
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="") as f:
        f.write(t)


def naloz(katalog, patch):
    t = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-i", patch],
                       cwd=katalog, capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("patch %s nie nalozyl sie: %s%s"
                         % (os.path.basename(patch), t.stdout, t.stderr))


def wytnij_panel(tekst):
    """Dokladny kawalek HTML+JS panelu, taki, jaki dostaje przegladarka."""
    i = tekst.find(PANEL_START)
    if i < 0:
        raise SystemExit("216: brak komentarza panelu w pliku")
    j = tekst.find("</script>", i)
    if j < 0:
        raise SystemExit("216: panel nie ma konca </script>")
    return tekst[i:j + len("</script>")]


def wytnij_skrypt(html):
    bloki = re.findall(r"<script>([\s\S]*?)</script>", html)
    if len(bloki) != 1:
        raise SystemExit("216: panel ma %d blokow <script>, oczekiwano 1" % len(bloki))
    return bloki[0]


def trasy_serwera(tekst):
    """Wszystkie sciezki rejestrowane przez wifiWebServer.on("...", ...)."""
    return sorted(set(re.findall(r'wifiWebServer\.on\("([^"]+)"', tekst)))


def wyciagnij_handlery(tekst):
    wzor = re.compile(r'wifiWebServer\.on\("([^"]+)",\s*HTTP_(\w+),\s*'
                      r'\[\]\(AsyncWebServerRequest \*request\) \{\n(.*?)\n  \}\);', re.S)
    nazwy = {"/history": "obslugaHistory", "/history/long": "obslugaHistoryLong",
             "/history/link": "obslugaHistoryLink"}
    wynik = {}
    for sciezka, metoda, cialo in wzor.findall(tekst):
        if sciezka in nazwy:
            wynik[sciezka] = (metoda, cialo, nazwy[sciezka])
    return wynik


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS build216 zrodlo v3.55.8 sha256 zgodny z 210")

    tmp = tempfile.mkdtemp(prefix="b216-")
    try:
        lancuch = os.path.join(tmp, "lancuch")
        kum = os.path.join(tmp, "kum")
        stary = os.path.join(tmp, "stary")     # v3.56.2 - tylko do kontroli
        for d in (lancuch, kum, stary):
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(PRZYKLAD, os.path.join(d, "secrets.h.example"))
        for p in PATCHE_WSTECZ:
            naloz(lancuch, p)
            naloz(stary, p)
        naloz(lancuch, PATCH_INC)
        naloz(kum, PATCH_KUM)
        # Odwracalnosc: patch skumulowany nalozony W TYLE musi oddac plik v3.55.8.
        cofnij = subprocess.run(["patch", "-R", "-p1", "--no-backup-if-mismatch",
                                 "-i", PATCH_KUM], cwd=kum, capture_output=True, text=True)
        if cofnij.returncode != 0:
            raise SystemExit("216: patch skumulowany nie cofa sie: %s%s"
                             % (cofnij.stdout, cofnij.stderr))
        if wczytaj(os.path.join(kum, "main_centrala.cpp")) != wczytaj(ZRODLO):
            raise SystemExit("216: po cofnieciu patcha plik nie jest plikiem v3.55.8")
        print("PASS build216 patch skumulowany cofa sie dokladnie do v3.55.8")
        naloz(kum, PATCH_KUM)

        r_lancuch = wczytaj(os.path.join(lancuch, "main_centrala.cpp"))
        r_kum = wczytaj(os.path.join(kum, "main_centrala.cpp"))
        r_stary = wczytaj(os.path.join(stary, "main_centrala.cpp"))
        panel_stary = wytnij_panel(r_stary)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if r_lancuch != r_kum:
        raise SystemExit("216: patch skumulowany 3.55.8->3.56.3 != lancuch 211+212+213+214+216")
    print("PASS build216 patch skumulowany == 211 + 212 + 213 + 214 + 216 (bajt w bajt)")
    sha_wyniku = hashlib.sha256(r_lancuch.encode()).hexdigest()
    print("PASS build216 wynik sha256 %s, linii %d"
          % (sha_wyniku, len(r_lancuch.split("\n"))))

    # --- co musi byc, a czego byc nie moze -------------------------------
    musza = [
        ('static const char* FIRMWARE_VERSION = "v3.56.3";', "brak FIRMWARE_VERSION v3.56.3"),
        ("var trasa = (strumien === 'long') ? '/history/long' : '/history';",
         "panel nie wybiera trasy wedlug strumienia"),
        ('#include "HistoriaPobranie.h"   // [v3.56.0 HISTORIA-POBRANIE]',
         "brak include'a trasy pobrania"),
        ('wifiWebServer.on("/history", HTTP_ANY,', "brak trasy /history"),
        ('wifiWebServer.on("/history/long", HTTP_ANY,', "brak trasy /history/long"),
        ('wifiWebServer.on("/history/link", HTTP_GET,', "brak trasy /history/link"),
    ]
    for fragment, opis in musza:
        if fragment not in r_lancuch:
            raise SystemExit("build216: %s (%s)" % (opis, fragment))

    zakazane = [
        ("+ '&s=' + strumien", "panel dokleja parametr s=, ktorego trasa nie czyta"),
        ("HISTORIA_TOKEN", "staly token w pliku panelu"),
        ("Bearer ", "naglowek Bearer w panelu"),
    ]
    panel = wytnij_panel(r_lancuch)
    for fragment, opis in zakazane:
        if fragment in panel:
            raise SystemExit("build216: %s (%s)" % (opis, fragment))
    if "HISTORIA_TOKEN" in panel_stary:
        raise SystemExit("build216: stary panel zawiera token - zle wyciete")
    print("PASS build216 panel: nowy adres, bez staleho tokenu i bez s=")

    # --- include'y musza zostac w kolejnosci z v3.56.2 -------------------
    i_serwer = r_lancuch.find("#include <ESPAsyncWebServer.h>")
    i_pobr = r_lancuch.find('#include "HistoriaPobranie.h"')
    i_bil = r_lancuch.find('#include "HistoriaBilety.h"')
    if not (0 < i_serwer < i_pobr < i_bil):
        raise SystemExit("build216: 216 zepsul kolejnosc include z v3.56.2")
    print("PASS build216 kolejnosc include z v3.56.2 zachowana (D1 nie wrocil)")

    # --- trasy z pliku ---------------------------------------------------
    trasy = trasy_serwera(r_lancuch)
    for wymagana in ("/history", "/history/long", "/history/link"):
        if wymagana not in trasy:
            raise SystemExit("build216: trasa %s nie jest zarejestrowana" % wymagana)
    linie = ["// WYGENEROWANE przez narzedzia/od-przycisku216/build.py - nie edytowac.\n",
             "// Trasy zarejestrowane w pliku po patchu v3.56.3 (wifiWebServer.on).\n",
             "static const char *TRASY216[] = {\n"]
    linie += ['  "%s",\n' % t for t in trasy]
    linie += ["};\n", "static const int TRASY216_N = %d;\n" % len(trasy)]
    zapisz(WYJSCIE_TRASY, "".join(linie))
    print("PASS build216 Trasy216.inc %d tras (w tym 3 nasze)" % len(trasy))

    # --- handlery + adaptery --------------------------------------------
    rej = wyciagnij_handlery(r_lancuch)
    for sciezka in ("/history", "/history/long", "/history/link"):
        if sciezka not in rej:
            raise SystemExit("build216: brak rejestracji %s do ekstrakcji" % sciezka)
    if rej["/history/link"][0] != "GET":
        raise SystemExit("build216: /history/link nie jest GET")
    if rej["/history"][0] != "ANY" or rej["/history/long"][0] != "ANY":
        raise SystemExit("build216: trasy pobrania zmienily metode")

    i = r_lancuch.index("static void historiaLosuj213")
    j = r_lancuch.index("static historia213::Bilety historiaBilety", i)
    adaptery = r_lancuch[i:j].rstrip() + "\n"

    # Bramka i obiekt trasy (z etapu 212) - bez tego lambdy nie maja czego wolac.
    i = r_lancuch.index("struct HistoriaBramka212 : historia212::Bramka {")
    j = r_lancuch.index("static historia212::Pobranie historiaPobranie", i)
    k = r_lancuch.index("\n", j) + 1
    bramka = r_lancuch[i:k].rstrip() + "\n"

    czesci = [
        "// WYGENEROWANE przez narzedzia/od-przycisku216/build.py - nie edytowac.\n",
        "// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp\n",
        "//         + main_centrala-v3.56.2-to-v3.56.3.patch (sha256 %s)\n" % sha256_sciezki(PATCH_INC),
        "// Ponizej: prawdziwy kod z pliku po patchu v3.56.3.\n\n",
        adaptery, "\n",
        "// bramka i obiekt trasy (212) - jedyne miejsce, ktore siega do stanu historii\n",
        bramka, "\n",
    ]
    for sciezka in ("/history", "/history/long", "/history/link"):
        metoda, cialo, nazwa = rej[sciezka]
        czesci.append("// %s (%s) - tresc lambdy z main_centrala.cpp\n" % (sciezka, metoda))
        czesci.append("static void %s(AsyncWebServerRequest *request) {\n%s\n}\n\n"
                      % (nazwa, cialo))
    # Tabela "sciezka -> funkcja" tez jest generowana z rejestracji w pliku,
    # zeby test nie mial wlasnej, recznej mapy tras.
    czesci.append("// Sciezka -> obsluga, w kolejnosci rejestracji w main_centrala.cpp.\n")
    czesci.append("struct Trasa216 {\n  const char *sciezka;\n"
                  "  void (*obsluz)(AsyncWebServerRequest *);\n};\n")
    czesci.append("static const Trasa216 TRASY_OBSLUGI216[] = {\n")
    for sciezka in ("/history", "/history/long", "/history/link"):
        czesci.append('  {"%s", %s},\n' % (sciezka, rej[sciezka][2]))
    czesci.append("};\n")
    czesci.append("static const int TRASY_OBSLUGI216_N = %d;\n" % len(rej))
    tresc = "".join(czesci)
    zapisz(WYJSCIE_HANDLER, tresc)
    print("PASS build216 Extracted216.inc %d linii" % len(tresc.split("\n")))

    # --- panel: HTML dla przegladarki i sam skrypt -----------------------
    zapisz(os.path.join(KATALOG_PANELU, "panel.html"), panel + "\n")
    skrypt = wytnij_skrypt(panel)
    zapisz(os.path.join(KATALOG_PANELU, "panel.js"), skrypt)
    zapisz(os.path.join(KATALOG_PANELU, "panel-stary.html"), panel_stary + "\n")
    print("PASS build216 panel/panel.html %d znakow, panel.js %d znakow, panel-stary.html"
          % (len(panel), len(skrypt)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
