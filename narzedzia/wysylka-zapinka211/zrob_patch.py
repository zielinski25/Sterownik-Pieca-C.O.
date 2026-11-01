#!/usr/bin/env python3
"""211: wygeneruj patch v3.55.8 -> v3.55.9 (zapinka historii w wysylaczach TG).

Patch NIE jest pisany recznie: ten skrypt bierze przypiety plik z etapu 210
(SHA-256 sprawdzany), podmienia DOKLADNIE cztery miejsca i zapisuje wynik jako
zwykly diff -u. Dzieki temu mozna odtworzyc patch z zrodla i sprawdzic, ze
zmienilo sie tylko to, co jest opisane w README.

Uruchomienie (Linux, Python 3):
    python3 -B narzedzia/wysylka-zapinka211/zrob_patch.py
"""
import hashlib
import os
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
ZRODLO = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie", "main_centrala.cpp")
SHA_ZRODLA211 = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"
WYJSCIE = os.path.join(TU, "main_centrala-v3.55.8-to-v3.55.9.patch")


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def wczytaj(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def zakres_funkcji(linie, poczatek_od):
    """Zwraca (i_start, i_koniec) obejmujacy cialo funkcji zaczynajacej sie
    po linii `poczatek_od` — po nawiasach klamrowych."""
    i = poczatek_od
    while i < len(linie):
        if linie[i].startswith("bool sendTelegramDocumentHist"):
            break
        i += 1
    if i == len(linie):
        raise SystemExit("nie znaleziono poczatku funkcji")
    glebokosc = 0
    otwarte = False
    for j in range(i, len(linie)):
        for znak in linie[j]:
            if znak == "{":
                glebokosc += 1
                otwarte = True
            elif znak == "}":
                glebokosc -= 1
                if otwarte and glebokosc == 0:
                    return i, j
        if otwarte and glebokosc < 0:
            raise SystemExit("niedomkniety nawias")
    raise SystemExit("nie znaleziono konca funkcji")


def podmien_region(tekst, marker, plik_inc, opis):
    linie = tekst.split("\n")
    indeksy = [n for n, l in enumerate(linie) if marker in l]
    if len(indeksy) != 1:
        raise SystemExit("%s: marker wystepuje %d razy" % (opis, len(indeksy)))
    start, koniec = zakres_funkcji(linie, indeksy[0])
    if start < indeksy[0]:
        raise SystemExit("%s: funkcja zaczyna sie przed komentarzem" % opis)
    nowe = wczytaj(plik_inc).rstrip("\n").split("\n")
    wynik = linie[:indeksy[0]] + nowe + linie[koniec + 1:]
    return "\n".join(wynik)


def main():
    if not os.path.exists(ZRODLO):
        raise SystemExit("brak pliku zrodlowego: %s" % ZRODLO)
    sha = sha256_sciezki(ZRODLO)
    if sha != SHA_ZRODLA211:
        raise SystemExit("SHA-256 zrodla inny niz przypiety w 210: %s" % sha)

    tekst = wczytaj(ZRODLO)

    # 1. Naglowek wersji.
    stary_naglowek = ("// FIRMWARE_VERSION v3.55.7 -> v3.55.8 (KANDYDAT: blokada calego "
                      "dostepu Arduino FS po bledzie startu; NIE WGRYWAC).\n")
    if tekst.count(stary_naglowek) != 1:
        raise SystemExit("naglowek wersji: oczekiwano jednego wystapienia")
    nowy_naglowek = (
        "// FIRMWARE_VERSION v3.55.8 -> v3.55.9 (KANDYDAT: zapinka historii w wysylaczach "
        "dokumentow Telegram; NIE WGRYWAC).\n"
        "// v3.55.9: wysylacze hist_short/hist_long czytaja plik przez zapinke (dlugosc pod\n"
        "// logMutexem, jedno otwarcie, tylko zapowiedziany prefiks). Krotki odczyt i krotki\n"
        "// zapis do gniazda koncza sie zerwaniem polaczenia, nie \"udanym\" ucietym dokumentem.\n"
        "// Zapis historii, rotacja, CSV, NVS, OTA, sterowanie i rytm 8s/60s bez zmian.\n"
        + stary_naglowek)
    tekst = tekst.replace(stary_naglowek, nowy_naglowek, 1)

    # 2. Include nowego naglowka.
    anchor_include = '#include "EspVfsCalls.h"\n'
    if tekst.count(anchor_include) != 1:
        raise SystemExit("include: oczekiwano jednego wystapienia")
    tekst = tekst.replace(
        anchor_include,
        anchor_include + '#include "HistoriaPin.h"   // [v3.55.9 HISTORIA-PIN] zapinka prefiksu historii\n',
        1)

    # 3. Wlasciciel VFS historii musi byc znany przed wysylaczami TG.
    anchor_fwd = "bool historiaFlushDoFS();\n"
    if tekst.count(anchor_fwd) != 1:
        raise SystemExit("forward-deklaracja: oczekiwano jednego wystapienia")
    tekst = tekst.replace(
        anchor_fwd,
        anchor_fwd +
        "// [v3.55.9 HISTORIA-PIN] Wlasciciel VFS historii przeniesiony wyzej.\n"
        "// Powod: sendTelegramDocumentHistKrotka()/...Dluga() (sekcja Telegram, nizej)\n"
        "// otwieraja zapinke tym samym wywolaniem VFS, ktorym pisze historiaFlushDoFS(),\n"
        "// a ich definicje sa PRZED sekcja [v3.55.5 HISTORIA-IO], gdzie ten obiekt stal.\n"
        "// Typ jest kompletny (CheckedHistoryIO.h jest wciagniety wyzej), obiekt bez stanu.\n"
        "static history193::SystemCalls historiaSys;\n",
        1)

    stara_definicja = "static history193::SystemCalls historiaSys;\nstatic void historiaIoService()"
    if tekst.count(stara_definicja) != 1:
        raise SystemExit("definicja historiaSys: oczekiwano jednego wystapienia")
    tekst = tekst.replace(
        stara_definicja,
        "// [v3.55.9 HISTORIA-PIN] static history193::SystemCalls historiaSys; - patrz\n"
        "// forward-deklaracja wyzej (sekcja HISTORIA-PENDING), tam tez uzasadnienie.\n"
        "static void historiaIoService()",
        1)

    # 4. Wersja widziana przez panel/OTA.
    stara_wersja = 'static const char* FIRMWARE_VERSION = "v3.55.8";'
    if tekst.count(stara_wersja) != 1:
        raise SystemExit("FIRMWARE_VERSION: oczekiwano jednego wystapienia")
    tekst = tekst.replace(stara_wersja, 'static const char* FIRMWARE_VERSION = "v3.55.9";', 1)

    # 5. Obie funkcje wysylajace.
    tekst = podmien_region(
        tekst,
        "// [PLAN_HISTORIA_I_USTAWIENIA_TELEGRAM.md, Czesc 7] Wysylka hist_short.csv",
        os.path.join(TU, "NowaKrotka.inc"), "sendTelegramDocumentHistKrotka")
    tekst = podmien_region(
        tekst,
        "// [PLAN_HISTORIA_I_USTAWIENIA_TELEGRAM.md, Czesc 7] Identyczna jak",
        os.path.join(TU, "NowaDluga.inc"), "sendTelegramDocumentHistDluga")

    with tempfile.TemporaryDirectory() as tmp:
        a = os.path.join(tmp, "a")
        b = os.path.join(tmp, "b")
        os.makedirs(a)
        os.makedirs(b)
        with open(os.path.join(a, "main_centrala.cpp"), "w", encoding="utf-8", newline="") as f:
            f.write(wczytaj(ZRODLO))
        with open(os.path.join(b, "main_centrala.cpp"), "w", encoding="utf-8", newline="") as f:
            f.write(tekst)
        diff = subprocess.run(["diff", "-u", "-r", "a", "b"], cwd=tmp,
                              capture_output=True, text=True)
        if diff.returncode != 1:
            raise SystemExit("diff zwrocil %d (oczekiwano 1 = sa roznice)" % diff.returncode)
        tresc = diff.stdout
        if not tresc.endswith("\n"):
            tresc += "\n"
        with open(WYJSCIE, "w", encoding="utf-8", newline="") as f:
            f.write(tresc)

    zmienione = sum(1 for l in tresc.split("\n") if l.startswith("+") and not l.startswith("+++"))
    usuniete = sum(1 for l in tresc.split("\n") if l.startswith("-") and not l.startswith("---"))
    print("PASS patch211 zapisany %s" % os.path.relpath(WYJSCIE, REPO))
    print("PASS patch211 linie +%d -%d" % (zmienione, usuniete))
    return 0


if __name__ == "__main__":
    sys.exit(main())
