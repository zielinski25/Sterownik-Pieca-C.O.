#!/usr/bin/env python3
"""218: patch v3.56.3 -> v3.56.4 - PRZYCISK POBRANIA NIE DZIALAL W OGOLE.

CO BYLO ZLE (znalezione dopiero testem "adres z panelu podany serwerowi"):

  D4 - trasa /history, zarejestrowana gołym napisem, przechwytywala
       /history/long i /history/link.
       W ESPAsyncWebServer (fork ESP32Async/mathieucarbou, ref c47917d):
         * goly napis jako URI tworzy matcher `BackwardCompatible`, ktory
           pasuje do `_value == path` ALBO `path.startsWith(_value + "/")`
           (WebServer.cpp, AsyncURIMatcher::matches),
         * `_attachHandler()` bierze PIERWSZY pasujacy handler w kolejnosci
           rejestracji, nie najbardziej precyzyjny (WebServer.cpp:142).
       /history bylo zarejestrowane PRZED /history/long i /history/link,
       wiec oba te adresy trafialy do handlera strumienia KROTKIEGO:
         - GET /history/link?stream=short -> handler pobrania -> 401
           "Brak poswiadczen" (brak naglowka Authorization i brak ?k=),
           czyli panel NIGDY nie dostawal biletu: oba przyciski konczyly sie
           komunikatem "Nie udalo sie: HTTP 401",
         - GET /history/long?k=<bilet> -> handler KROTKI -> zuzyj() porownuje
           strumien -> 401, wiec dlugiej historii nie dalo sie pobrac zadna
           droga.
       Kazdy element osobno byl poprawny: link(stream=long) wydawal bilet,
       /history/long z biletem dawalo 200 w tescie 213 - tylko nigdy przez
       prawdziwa tabele tras. To ten sam blad co [FIX 17] przy /api/history*,
       ktory w tym pliku juz naprawiono; nowe trasy powtorzyly go.

CO ROBI TEN PATCH:
  1. /history, /history/long i /history/link rejestrowane przez
     AsyncURIMatcher::exact(...) - dopasowanie przestaje zalezec od
     kolejnosci rejestracji (ta sama metoda co [FIX 17]),
  2. komentarz przy /history/link mowi juz o obu trasach (byl nieaktualny
     po v3.56.3, gdzie panel wybiera trase wedlug strumienia),
  3. podnosi wersje na v3.56.4.
  Uklad include'ow, panel, HistoriaPobranie.h i cala reszta pliku zostaja
  bajt w bajt bez zmian.

Skrypt nie pisze patcha recznie: bierze przypiety plik v3.55.8, naklada
211 + 212 + 213 + 214 + 216, poprawia trasy, podnosi wersje i zapisuje diff.
Na koncu sprawdza, ze oba patche (przyrostowy i skumulowany) nakladaja sie
na czysty plik i daja DOKLADNIE ten sam wynik.

Uruchomienie:
    python3 -B narzedzia/panel-trasa218/zrob_patch.py
"""
import hashlib
import os
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
WSTEPNE = (P211, P212, P213, P214, P216)

WYJSCIE_INC = os.path.join(TU, "main_centrala-v3.56.3-to-v3.56.4.patch")
WYJSCIE_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.4.patch")

SHA_ZRODLA = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"
SHA_V3563 = "f51b0b0749e2c1e3a8b5c02848a9c0e4902e0b641d6a7313a42304bbbcef91b3"

# --- 1. dwie stare rejestracje -> exact() + komentarz --------------------
TRASY_STARE = (
    '  wifiWebServer.on("/history", HTTP_ANY, [](AsyncWebServerRequest *request) {\n'
    '    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);\n'
    '  });\n'
    '  wifiWebServer.on("/history/long", HTTP_ANY, [](AsyncWebServerRequest *request) {\n'
    '    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);\n'
    '  });\n'
)

TRASY_NOWE = (
    '  // [v3.56.4 HISTORIA-TRASY] Wszystkie trzy trasy /history* przez\n'
    '  // AsyncURIMatcher::exact. Goly napis to matcher BackwardCompatible\n'
    '  // (^{uri}(/.*)?$, WebServer.cpp), a _attachHandler() bierze PIERWSZY\n'
    '  // pasujacy handler w kolejnosci rejestracji - "/history" zarejestrowane\n'
    '  // golym napisem pasowalo wiec TAKZE do "/history/long" i "/history/link".\n'
    '  // Oba te adresy nigdy nie dochodzily do swoich handlerow: panel dostawal\n'
    '  // 401 na prosbe o bilet (zaden przycisk pobierania nie dzialal), a dluga\n'
    '  // historia nie miala jak trafic na wlasciwy strumien. Ten sam blad co\n'
    '  // [FIX 17] przy /api/history* - tam exact() juz jest.\n'
    '  wifiWebServer.on(AsyncURIMatcher::exact("/history"), HTTP_ANY, [](AsyncWebServerRequest *request) {\n'
    '    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);\n'
    '  });\n'
    '  wifiWebServer.on(AsyncURIMatcher::exact("/history/long"), HTTP_ANY, [](AsyncWebServerRequest *request) {\n'
    '    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);\n'
    '  });\n'
)

LINK_STARY = (
    '  // [v3.56.1 HISTORIA-BILETY] Panel prosi o bilet, dostaje 32 znaki hex i sam\n'
    '  // otwiera /history?k=<bilet>. Tu NIE ma tokenu z secrets.h - tylko bilet.\n'
    '  wifiWebServer.on("/history/link", HTTP_GET, [](AsyncWebServerRequest *request) {\n'
)

LINK_NOWY = (
    '  // [v3.56.1 HISTORIA-BILETY, v3.56.4 exact()] Panel prosi o bilet, dostaje\n'
    '  // 32 znaki hex i sam otwiera trase swojego strumienia: /history?k=<bilet>\n'
    '  // (krotka) albo /history/long?k=<bilet> (dluga). Tu NIE ma tokenu z\n'
    '  // secrets.h - tylko bilet.\n'
    '  wifiWebServer.on(AsyncURIMatcher::exact("/history/link"), HTTP_GET, [](AsyncWebServerRequest *request) {\n'
)

WERSJA_STARA = 'static const char* FIRMWARE_VERSION = "v3.56.3";'
WERSJA_NOWA = 'static const char* FIRMWARE_VERSION = "v3.56.4";'

KOMENTARZ = """// FIRMWARE_VERSION v3.56.3 -> v3.56.4 (KANDYDAT: przycisk pobrania historii nie dzialal w ogole; NIE WGRYWAC).
// v3.56.4: /history bylo zarejestrowane golym napisem, czyli matcherem
// BackwardCompatible (^{uri}(/.*)?$), a serwer bierze PIERWSZY pasujacy
// handler w kolejnosci rejestracji. /history przechwytywalo wiec
// /history/link (panel dostawal 401 zamiast biletu - zaden przycisk
// pobierania nie dzialal) i /history/long (bilet dlugi nie pasowal do
// handlera krotkiego). Teraz wszystkie trzy trasy /history* uzywaja
// AsyncURIMatcher::exact, tak jak /api/history* po [FIX 17].
// Poza trzema rejestracjami i numerem wersji NIC sie nie zmienia:
// panel, HistoriaPobranie.h, uklad include'ow i reszta pliku bez zmian.
"""


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


def bez_znacznikow_czasu(diff):
    wynik = []
    for linia in diff.split("\n"):
        if linia.startswith("--- ") or linia.startswith("+++ "):
            linia = linia.split("\t")[0]
        wynik.append(linia)
    return "\n".join(wynik)


def podmien(tekst, stary, nowy, opis):
    if tekst.count(stary) != 1:
        raise SystemExit("oczekiwano dokladnie jednego wystapienia: %s (jest %d)"
                         % (opis, tekst.count(stary)))
    return tekst.replace(stary, nowy, 1)


def podnies_wersje(tekst):
    tekst = podmien(tekst, TRASY_STARE, TRASY_NOWE, "rejestracje /history i /history/long")
    tekst = podmien(tekst, LINK_STARY, LINK_NOWY, "rejestracja /history/link")
    tekst = podmien(tekst, WERSJA_STARA, WERSJA_NOWA, "linia FIRMWARE_VERSION v3.56.3")
    # Zadna trasa /history* nie moze zostac na golym napisie.
    for trasa in ('wifiWebServer.on("/history"', 'wifiWebServer.on("/history/long"',
                  'wifiWebServer.on("/history/link"'):
        if trasa in tekst:
            raise SystemExit("zostala rejestracja na golym napisie: %s" % trasa)
    for trasa in ('AsyncURIMatcher::exact("/history")', 'AsyncURIMatcher::exact("/history/long")',
                  'AsyncURIMatcher::exact("/history/link")'):
        if tekst.count(trasa) != 1:
            raise SystemExit("brak dokladnie jednej rejestracji %s" % trasa)
    return KOMENTARZ + tekst


def diff(katalog, a, b):
    d = subprocess.run(["diff", "-u", "-r", a, b], cwd=katalog, capture_output=True, text=True)
    if d.returncode != 1:
        raise SystemExit("diff %s..%s zwrocil %d" % (a, b, d.returncode))
    return bez_znacznikow_czasu(d.stdout)


def sprawdz_nakladanie(patch, baza_przed):
    """Nakłada patch na czysty plik i porownuje SHA z oczekiwanym."""
    tmp = tempfile.mkdtemp(prefix="v218-")
    try:
        shutil.copyfile(PRZYKLAD, os.path.join(tmp, "secrets.h.example"))
        if baza_przed is None:
            shutil.copyfile(ZRODLO, os.path.join(tmp, "main_centrala.cpp"))
        else:
            shutil.copyfile(ZRODLO, os.path.join(tmp, "main_centrala.cpp"))
            for p in baza_przed:
                naloz(tmp, p)
        naloz(tmp, patch)
        return sha256_sciezki(os.path.join(tmp, "main_centrala.cpp"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS patch218 zrodlo v3.55.8 sha256 zgodny z 210")

    tmp = tempfile.mkdtemp(prefix="p218-")
    try:
        baza = os.path.join(tmp, "a")     # v3.55.8
        stare = os.path.join(tmp, "b")    # v3.56.3
        nowe = os.path.join(tmp, "c")     # v3.56.4
        for d in (baza, stare, nowe):
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(PRZYKLAD, os.path.join(d, "secrets.h.example"))
        for d in (stare, nowe):
            for p in WSTEPNE:
                naloz(d, p)

        v3563 = wczytaj(os.path.join(stare, "main_centrala.cpp"))
        if hashlib.sha256(v3563.encode("utf-8")).hexdigest() != SHA_V3563:
            raise SystemExit("v3.56.3 nie zgadza sie z etapem 216")
        print("PASS patch218 baza v3.56.3 sha256 zgodny z etapem 216")

        v3564 = podnies_wersje(v3563)
        zapisz(os.path.join(nowe, "main_centrala.cpp"), v3564)

        zapisz(WYJSCIE_INC, diff(tmp, "b", "c"))
        zapisz(WYJSCIE_KUM, diff(tmp, "a", "c"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    plus = minus = hunki = 0
    for plik in (WYJSCIE_INC, WYJSCIE_KUM):
        tresc = wczytaj(plik)
        p = sum(1 for l in tresc.split("\n") if l.startswith("+") and not l.startswith("+++"))
        m = sum(1 for l in tresc.split("\n") if l.startswith("-") and not l.startswith("---"))
        h = sum(1 for l in tresc.split("\n") if l.startswith("@@"))
        print("PASS patch218 %s +%d -%d hunki=%d" % (os.path.basename(plik), p, m, h))
        if plik == WYJSCIE_INC:
            plus, minus, hunki = p, m, h
    # 3 hunki: komentarz na gorze pliku, linia wersji (5601), rejestracje tras.
    if hunki != 3:
        raise SystemExit("patch przyrostowy powinien miec 3 hunki, jest %d" % hunki)

    sha_inc = sprawdz_nakladanie(WYJSCIE_INC, WSTEPNE)
    sha_kum = sprawdz_nakladanie(WYJSCIE_KUM, None)
    if sha_inc != sha_kum:
        raise SystemExit("patch przyrostowy i skumulowany daja rozne pliki: %s vs %s"
                         % (sha_inc[:16], sha_kum[:16]))
    print("PASS patch218 oba patche daja ten sam plik v3.56.4 sha256 %s" % sha_kum)
    return 0


if __name__ == "__main__":
    sys.exit(main())
