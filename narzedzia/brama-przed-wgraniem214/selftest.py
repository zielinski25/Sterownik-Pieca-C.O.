#!/usr/bin/env python3
"""214: selftest bramy - dowod, ze brama NAPRAWDE lapie bledy.

Brama, ktora zawsze mowi "PASS", jest gorsza niz brak bramy. Ten skrypt:
  1. przepuszcza poprawny plik przez brame (musi byc 0 bledow),
  2. przepuszcza plik z ETAPU 213 (v3.56.1, z bledem kolejnosci include)
     i wymaga, zeby brama go ZATRZYMA A (S03),
  3. psuje poprawny plik na 11 sposobow - kazde zepsucie musi zostac zlapane
     przez konkretna kontrole.

Brak kompilacji w selftescie (S17 pomijana) - kompilacja jest sprawdzana
w build.py, a tu chodzi o logike kontroli.

Uruchomienie:
    python3 -B narzedzia/brama-przed-wgraniem214/selftest.py
"""
import os
import shutil
import subprocess
import sys
import tempfile

import sprawdz
import zrob_patch as Z

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
NARZ = os.path.join(REPO, "narzedzia")
FIRMWARE = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
ZRODLO = os.path.join(FIRMWARE, "main_centrala.cpp")
PRZYKLAD = os.path.join(FIRMWARE, "secrets.h.example")
PATCH_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.2.patch")

NAGLOWKI_ZRODLA = {
    "HistoriaPin.h": os.path.join(NARZ, "wysylka-zapinka211", "HistoriaPin.h"),
    "HistoriaPobranie.h": os.path.join(NARZ, "panel-pobrania213", "HistoriaPobranie.h"),
    "HistoriaBilety.h": os.path.join(NARZ, "panel-pobrania213", "HistoriaBilety.h"),
}


def wczytaj(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def naloz(katalog, patch):
    t = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-i", patch],
                       cwd=katalog, capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("patch %s sie nie nalozyl: %s" % (os.path.basename(patch), t.stdout))
    return t


def zbuduj_baze():
    """Zwraca (tekst v3.56.2, tekst v3.56.1, przyklad po patchu, naglowki)."""
    tmp = tempfile.mkdtemp(prefix="st214-")
    try:
        dobre = os.path.join(tmp, "dobre")
        zle = os.path.join(tmp, "zle")
        for d in (dobre, zle):
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(PRZYKLAD, os.path.join(d, "secrets.h.example"))
        naloz(dobre, PATCH_KUM)
        for p in (Z.P211, Z.P212, Z.P213):
            naloz(zle, p)
        return (wczytaj(os.path.join(dobre, "main_centrala.cpp")),
                wczytaj(os.path.join(zle, "main_centrala.cpp")),
                wczytaj(os.path.join(dobre, "secrets.h.example")))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def sprawdz_przypadek(nazwa, tekst, przyklad, naglowki, oczekiwane, opis):
    wyniki = sprawdz.uruchom(tekst, naglowki, przyklad, z_kompilacja=False, odwracalny=True)
    bledy = {x["id"] for x in wyniki if not x["ok"]}
    if oczekiwane is None:
        if bledy:
            print("FAIL selftest [%s] oczekiwano 0 bledow, jest %s" % (nazwa, sorted(bledy)))
            return False
        print("PASS selftest [%s] %s" % (nazwa, opis))
        return True
    if oczekiwane in bledy:
        print("PASS selftest [%s] zlapane przez %s (%s)" % (nazwa, oczekiwane, opis))
        return True
    print("FAIL selftest [%s] brama NIE zlapala %s (bledy: %s)" % (nazwa, oczekiwane, sorted(bledy)))
    return False


def main():
    dobre, zle, przyklad = zbuduj_baze()
    naglowki = {n: wczytaj(p) for n, p in NAGLOWKI_ZRODLA.items()}
    ok = True

    ok &= sprawdz_przypadek("baza", dobre, przyklad, naglowki, None,
                            "poprawny plik v3.56.2 przechodzi bez bledow")

    ok &= sprawdz_przypadek("plik-z-etapu-213", zle, przyklad, naglowki, "S03",
                            "plik v3.56.1 (dostarczony w 213) jest ZATRZYMYWANY")

    # obcinamy CALE linie (z komentarzami) i wstawiamy je z powrotem za HistoriaPin.h
    linie = dobre.split("\n")
    usuniete = [l for l in linie if sprawdz.INC_POBRANIE in l or sprawdz.INC_BILETY in l]
    if len(usuniete) != 2:
        raise SystemExit("selftest: oczekiwano 2 linii include do przeniesienia, jest %d" % len(usuniete))
    linie = [l for l in linie if l not in usuniete]
    for i, l in enumerate(linie):
        if sprawdz.INC_PIN in l:
            linie[i + 1:i + 1] = usuniete
            break
    zly = "\n".join(linie)
    ok &= sprawdz_przypadek("include-przed-serwerem", zly, przyklad, naglowki, "S03",
                            "przywrocony blad 212/213")

    ok &= sprawdz_przypadek("wersja-stara", dobre.replace('"v3.56.2"', '"v3.56.1"', 1),
                            przyklad, naglowki, "S01", "wersja nie podniesiona")

    ok &= sprawdz_przypadek("duplikat-tokenu",
                            dobre.replace("static const char* HISTORIA_TOKEN = SECRET_HISTORIA_TOKEN;",
                                          "static const char* HISTORIA_TOKEN = SECRET_HISTORIA_TOKEN;\n"
                                          "static const char* HISTORIA_TOKEN = SECRET_HISTORIA_TOKEN;", 1),
                            przyklad, naglowki, "S04", "dwie definicje tej samej zmiennej")

    ok &= sprawdz_przypadek("token-niepusty",
                            dobre.replace('#define SECRET_HISTORIA_TOKEN ""',
                                          '#define SECRET_HISTORIA_TOKEN "abc"', 1),
                            przyklad, naglowki, "S05", "domyslny token nie jest pusty")

    ok &= sprawdz_przypadek("link-post",
                            dobre.replace('wifiWebServer.on("/history/link", HTTP_GET,',
                                          'wifiWebServer.on("/history/link", HTTP_POST,', 1),
                            przyklad, naglowki, "S08", "/history/link nie jest GET")

    ok &= sprawdz_przypadek("panel-rozbija-literal",
                            dobre.replace('id="histInfo"', 'id="histInfo" )PIECHTML"', 1),
                            przyklad, naglowki, "S09", "koniec literalu HTML w srodku panelu")

    ok &= sprawdz_przypadek("panel-z-tokenem",
                            dobre.replace('id="histInfo"', 'id="histInfo" data-t="HISTORIA_TOKEN"', 1),
                            przyklad, naglowki, "S09", "staly token w tekscie panelu")

    ok &= sprawdz_przypadek("sekret-w-przykladzie",
                            dobre,
                            przyklad.replace("ZMIEN-MNIE-NA-DLUGI-LOSOWY-TOKEN-32-ZNAKI",
                                             "a1b2c3d4e5f60718293a4b5c6d7e8f90", 1),
                            naglowki, "S06", "prawdziwy sekret w secrets.h.example")

    zle_naglowki = dict(naglowki)
    zle_naglowki["HistoriaPobranie.h"] = naglowki["HistoriaPobranie.h"].replace(
        '#include "HistoriaBilety.h"', '#include "HistoriaBilety.h"\n#include <string>', 1)
    ok &= sprawdz_przypadek("stl-w-naglowku", dobre, przyklad, zle_naglowki, "S12",
                            "STL w naglowku firmware")

    zle_naglowki2 = dict(naglowki)
    zle_naglowki2["HistoriaPobranie.h"] = naglowki["HistoriaPobranie.h"].replace(
        "  void wyczysc() {", "  void wyczysc() { if (0) throw 1;", 1) if "void wyczysc() {" in naglowki["HistoriaPobranie.h"] else \
        naglowki["HistoriaPobranie.h"].replace("  void link(AsyncWebServerRequest *request) {",
                                               "  void link(AsyncWebServerRequest *request) { if (0) throw 1;", 1)
    ok &= sprawdz_przypadek("wyjatek-w-naglowku", dobre, przyklad, zle_naglowki2, "S13",
                            "wyjatek w naglowku firmware")

    ok &= sprawdz_przypadek("zapinka-zepsuta",
                            dobre.replace("if (!pin.close()) DLOG(", "if (false) DLOG(", 1),
                            przyklad, naglowki, "S14", "brak zamkniecia zapinki w wysylaczu")

    ok &= sprawdz_przypadek("bez-esp-system",
                            dobre.replace("#include <esp_system.h>", "// bez esp_system.h", 1),
                            przyklad, naglowki, "S16", "brak include'a dla esp_random()")

    ok &= sprawdz_przypadek("crlf",
                            dobre.replace("#include <Arduino.h>", "#include <Arduino.h>\r", 1),
                            przyklad, naglowki, "S11", "CRLF w pliku")

    if ok:
        print("\nPASS selftest214 przypadkow=16 wszystkie zachowania zgodne z opisem")
        return 0
    print("\nFAIL selftest214 - brama przepuscila zepsuty plik")
    return 1


if __name__ == "__main__":
    sys.exit(main())
