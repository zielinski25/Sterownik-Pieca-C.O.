#!/usr/bin/env python3
"""214: patch v3.56.1 -> v3.56.2 - POPRAWKA KOLEJNOSCI INCLUDE.

CO BYLO ZLE (znalezione w tym etapie, nie w testach):
  w pliku po patchu 213 linie 5140-5141 wciagaly "HistoriaPobranie.h"
  i "HistoriaBilety.h", a <ESPAsyncWebServer.h> bylo dopiero w linii 5227.
  HistoriaPobranie.h dziedziczy po AsyncAbstractResponse i wola metody
  AsyncWebServerRequest, ktorych w tym miejscu jeszcze NIE MA zadeklarowanych.
  Na komputerze tego nie widac, bo testy najpierw wciagaja model serwera.
  Na prawdziwym toolchainie to blad kompilacji.

CO ROBI TEN PATCH:
  przenosi oba include'y ZA <ESPAsyncWebServer.h> i podnosi wersje na v3.56.2.
  Niczego innego nie zmienia - caly tekst trasy, biletow, panelu i zapinki 211
  zostaje bajt w bajt taki sam.

Skrypt nie pisze patcha recznie: bierze przypiety plik v3.55.8, naklada
211 + 212 + 213, przenosi dwie linie, podnosi wersje i zapisuje diff.

Uruchomienie:
    python3 -B narzedzia/brama-przed-wgraniem214/zrob_patch.py
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

WYJSCIE_INC = os.path.join(TU, "main_centrala-v3.56.1-to-v3.56.2.patch")
WYJSCIE_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.2.patch")

SHA_ZRODLA = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"

INCLUDE_POBRANIE = ('#include "HistoriaPobranie.h"   '
                    '// [v3.56.0 HISTORIA-POBRANIE] trasa pobrania historii\n')
INCLUDE_BILETY = ('#include "HistoriaBilety.h"   '
                  '// [v3.56.1 HISTORIA-BILETY] jednorazowe linki do pobrania\n')
INCLUDE_SERWER = "#include <ESPAsyncWebServer.h>\n"

KOMENTARZ = """// FIRMWARE_VERSION v3.56.1 -> v3.56.2 (KANDYDAT: poprawka KOLEJNOSCI include; NIE WGRYWAC).
// v3.56.2: "HistoriaPobranie.h" i "HistoriaBilety.h" byly wciagniete PRZED
// <ESPAsyncWebServer.h> (linia 5140-5141 vs 5227), wiec AsyncAbstractResponse
// i AsyncWebServerRequest nie byly jeszcze zadeklarowane - na ESP to blad
// kompilacji, na tescie hostowym niewidoczny, bo test najpierw wciaga model
// serwera. Teraz oba include'y sa PO <ESPAsyncWebServer.h>.
// Poza kolejnoscia dwoch linii i numerem wersji NIC sie nie zmienia.
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
        raise SystemExit("patch %s nie nalozyl sie: %s%s" % (os.path.basename(patch), t.stdout, t.stderr))


def bez_znacznikow_czasu(diff):
    """diff wypisuje czasy plikow - usuwamy je, zeby patch byl powtarzalny."""
    wynik = []
    for linia in diff.split("\n"):
        if linia.startswith("--- ") or linia.startswith("+++ "):
            linia = linia.split("\t")[0]
        wynik.append(linia)
    return "\n".join(wynik)


def przenies_include(tekst):
    """Wyjmnij oba include'y HTTP i wstaw je ZA <ESPAsyncWebServer.h>."""
    for fragment in (INCLUDE_POBRANIE, INCLUDE_BILETY):
        if tekst.count(fragment) != 1:
            raise SystemExit("oczekiwano dokladnie jednego wystapienia: %r" % fragment)
    if tekst.count(INCLUDE_SERWER) != 1:
        raise SystemExit("oczekiwano dokladnie jednego <ESPAsyncWebServer.h>")
    if tekst.index(INCLUDE_POBRANIE) > tekst.index(INCLUDE_SERWER):
        raise SystemExit("include trasy juz jest po serwerze - nic do poprawienia")
    tekst = tekst.replace(INCLUDE_POBRANIE, "", 1)
    tekst = tekst.replace(INCLUDE_BILETY, "", 1)
    tekst = tekst.replace(INCLUDE_SERWER, INCLUDE_SERWER + INCLUDE_POBRANIE + INCLUDE_BILETY, 1)
    return tekst


def podnies_wersje(tekst):
    if "FIRMWARE-BLEED" in tekst:
        raise SystemExit("nieoczekiwany stan pliku")
    if tekst.count('static const char* FIRMWARE_VERSION = "v3.56.1";') != 1:
        raise SystemExit("brak FIRMWARE_VERSION v3.56.1")
    tekst = tekst.replace('static const char* FIRMWARE_VERSION = "v3.56.1";',
                          'static const char* FIRMWARE_VERSION = "v3.56.2";', 1)
    return KOMENTARZ + tekst


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS patch214 zrodlo v3.55.8 sha256 zgodny z 210")

    tmp = tempfile.mkdtemp(prefix="p214-")
    try:
        baza = os.path.join(tmp, "a")
        stare = os.path.join(tmp, "b")   # v3.56.1
        nowe = os.path.join(tmp, "c")    # v3.56.2
        for d in (baza, stare, nowe):
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(PRZYKLAD, os.path.join(d, "secrets.h.example"))
        for d in (stare, nowe):
            naloz(d, P211)
            naloz(d, P212)
            naloz(d, P213)

        v3561 = wczytaj(os.path.join(stare, "main_centrala.cpp"))
        v3562 = podnies_wersje(przenies_include(v3561))
        zapisz(os.path.join(nowe, "main_centrala.cpp"), v3562)

        d1 = subprocess.run(["diff", "-u", "-r", "b", "c"], cwd=tmp, capture_output=True, text=True)
        if d1.returncode != 1:
            raise SystemExit("diff 214 zwrocil %d" % d1.returncode)
        zapisz(WYJSCIE_INC, bez_znacznikow_czasu(d1.stdout))

        d2 = subprocess.run(["diff", "-u", "-r", "a", "c"], cwd=tmp, capture_output=True, text=True)
        if d2.returncode != 1:
            raise SystemExit("diff skumulowany zwrocil %d" % d2.returncode)
        zapisz(WYJSCIE_KUM, bez_znacznikow_czasu(d2.stdout))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    for plik in (WYJSCIE_INC, WYJSCIE_KUM):
        tresc = wczytaj(plik)
        plus = sum(1 for l in tresc.split("\n") if l.startswith("+") and not l.startswith("+++"))
        minus = sum(1 for l in tresc.split("\n") if l.startswith("-") and not l.startswith("---"))
        hunki = sum(1 for l in tresc.split("\n") if l.startswith("@@"))
        print("PASS patch214 %s +%d -%d hunki=%d" %
              (os.path.basename(plik), plus, minus, hunki))
    return 0


if __name__ == "__main__":
    sys.exit(main())
