#!/usr/bin/env python3
"""212: naloz patch v3.55.8 -> v3.56.0 na PRAWDZIWY plik centrali i sprawdz wynik.

Sprawdza trzy rzeczy, ktorych nie da sie sprawdzic "na oko":
  * patch skumulowany (v3.55.8 -> v3.56.0) daje DOKLADNIE ten sam plik, co
    patch 211 (v3.55.8 -> v3.55.9) plus maly patch 212 (v3.55.9 -> v3.56.0),
  * w wyniku sa wszystkie elementy trasy: include, sekcja tokenu, bramka,
    dwie rejestracje HTTP_ANY i wlasciwe strumienie,
  * poprawki etapu 211 (zapinka w obu wysylaczach) sa nadal na miejscu.

Wynik: Extracted212.inc (prawdziwy adapter + obiekty, wyciagniete z pliku).
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
PATCH_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.0.patch")
PATCH_INC = os.path.join(TU, "main_centrala-v3.55.9-to-v3.56.0.patch")
PATCH211 = os.path.join(NARZ, "wysylka-zapinka211", "main_centrala-v3.55.8-to-v3.55.9.patch")
WYJSCIE = os.path.join(TU, "Extracted212.inc")
SHA_ZRODLA211 = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"


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


def wyciagnij_blok(tekst, poczatek, koniec):
    i = tekst.find(poczatek)
    if i < 0:
        raise SystemExit("nie znaleziono: %s" % poczatek[:40])
    j = tekst.find(koniec, i)
    if j < 0:
        raise SystemExit("nie znaleziono konca: %s" % koniec[:40])
    return tekst[i:j + len(koniec)]


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA211:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS build212 zrodlo v3.55.8 sha256 zgodny z 210")

    tmp = tempfile.mkdtemp(prefix="b212-")
    try:
        kum = os.path.join(tmp, "kum")
        inc = os.path.join(tmp, "inc")
        os.makedirs(kum)
        os.makedirs(inc)
        shutil.copyfile(ZRODLO, os.path.join(kum, "main_centrala.cpp"))
        shutil.copyfile(ZRODLO, os.path.join(inc, "main_centrala.cpp"))
        przyklad = os.path.join(FIRMWARE, "secrets.h.example")
        shutil.copyfile(przyklad, os.path.join(kum, "secrets.h.example"))
        shutil.copyfile(przyklad, os.path.join(inc, "secrets.h.example"))
        naloz(kum, PATCH_KUM)
        naloz(inc, PATCH211)
        naloz(inc, PATCH_INC)
        r_kum = wczytaj(os.path.join(kum, "main_centrala.cpp"))
        r_inc = wczytaj(os.path.join(inc, "main_centrala.cpp"))
        p_kum = wczytaj(os.path.join(kum, "secrets.h.example"))
        p_inc = wczytaj(os.path.join(inc, "secrets.h.example"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if r_kum != r_inc:
        raise SystemExit("patch skumulowany i lancuch 211+212 daja ROZNE pliki")
    if p_kum != p_inc:
        raise SystemExit("secrets.h.example rozni sie miedzy wariantami patcha")
    print("PASS build212 patch skumulowany == patch 211 + patch 212 (bajt w bajt)")
    if p_kum.count("SECRET_HISTORIA_TOKEN") != 1:
        raise SystemExit("secrets.h.example: oczekiwany dokladnie jeden wpis tokenu")
    if "ZMIEN-MNIE-NA-DLUGI-LOSOWY-TOKEN-32-ZNAKI" not in p_kum:
        raise SystemExit("secrets.h.example: brak zastepnika, nie wolno wpisywac sekretu")
    print("PASS build212 secrets.h.example: jeden wpis, zastepnik bez sekretu")

    if 'static const char* FIRMWARE_VERSION = "v3.56.0";' not in r_kum:
        raise SystemExit("brak FIRMWARE_VERSION v3.56.0")
    print("PASS build212 FIRMWARE_VERSION = v3.56.0")

    # --- elementy trasy ---
    warunki = [
        ('#include "HistoriaPobranie.h"', "brak include'a trasy"),
        ("#ifndef SECRET_HISTORIA_TOKEN", "brak sekcji tokenu z bezpiecznym domyslem"),
        ("static const char* HISTORIA_TOKEN = SECRET_HISTORIA_TOKEN;", "brak zmiennej tokenu"),
        ("struct HistoriaBramka212 : historia212::Bramka {", "brak bramki"),
        ("static historia212::Pobranie historiaPobranie(historiaBramka, HISTORIA_TOKEN);",
         "brak obiektu trasy"),
        ('wifiWebServer.on("/history", HTTP_ANY,', "brak rejestracji /history"),
        ('wifiWebServer.on("/history/long", HTTP_ANY,', "brak rejestracji /history/long"),
        ("historiaPobranie.obsluz(request, historia212::Strumien::Krotki);", "zly strumien krotki"),
        ("historiaPobranie.obsluz(request, historia212::Strumien::Dluga);", "zly strumien dlugi"),
    ]
    for wzor, opis in warunki:
        if wzor not in r_kum:
            raise SystemExit("212: %s" % opis)
    if r_kum.count('wifiWebServer.on("/history"') != 1 or \
       r_kum.count('wifiWebServer.on("/history/long"') != 1:
        raise SystemExit("212: trasa zarejestrowana wiecej niz raz")
    print("PASS build212 trasa: include, token, bramka, dwie rejestracje HTTP_ANY")

    # --- 211 nietkniete ---
    for wzor, ile in (("historia211::Pin pin;", 2), ("pin.open(historiaSys,", 2),
                      ("if (!cialoOk) goto tgDocHistSSendDone;", 1),
                      ("if (!cialoOk) goto tgDocHistLSendDone;", 1)):
        if r_kum.count(wzor) != ile:
            raise SystemExit("211 przestalo obowiazywac: %s (%d)" % (wzor, r_kum.count(wzor)))
    if r_kum.count("if (!pin.close()) DLOG(") != 2:
        raise SystemExit("211: zapinka nie jest zamykana w obu wysylaczach")
    print("PASS build212 poprawki 211 sa nietkniete (obie funkcje, oba zamkniecia)")

    # --- sekcja tokenu: domysl musi byc pusty ---
    if not re.search(r'#define SECRET_HISTORIA_TOKEN ""', r_kum):
        raise SystemExit("domyslny token nie jest pusty")
    print("PASS build212 domyslny token pusty (pobieranie wylaczone bez secrets.h)")

    # --- extract: bramka + obiekty ---
    blok = wyciagnij_blok(r_kum, "struct HistoriaBramka212 : historia212::Bramka {", "};")
    obiekty = ("static HistoriaBramka212 historiaBramka;\n"
               "static historia212::Pobranie historiaPobranie(historiaBramka, HISTORIA_TOKEN);\n")
    if obiekty not in r_kum:
        raise SystemExit("brak deklaracji obiektow trasy")
    naglowek = (
        "// WYGENEROWANE przez narzedzia/pobranie-trasa212/build.py - nie edytowac.\n"
        "// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp\n"
        "//         + main_centrala-v3.55.8-to-v3.56.0.patch (sha256 %s)\n"
        "// To jest dokladny tekst bramki i obiektow trasy z pliku PO patchu.\n\n"
    ) % sha256_sciezki(PATCH_KUM)
    with open(WYJSCIE, "w", encoding="utf-8", newline="") as f:
        f.write(naglowek + blok + "\n\n" + obiekty)
    print("PASS build212 Extracted212.inc %d linii" % len(wczytaj(WYJSCIE).split("\n")))
    print("PASS build212 sha256 wyniku %s" % hashlib.sha256(r_kum.encode()).hexdigest())
    return 0


if __name__ == "__main__":
    sys.exit(main())
