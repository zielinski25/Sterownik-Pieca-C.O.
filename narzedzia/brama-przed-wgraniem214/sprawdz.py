#!/usr/bin/env python3
"""214: brama przed wgraniem - statyczne kontrole pliku po patchu.

Po co: etapy 211-213 byly sprawdzane tylko testami hostowymi. Test hostowy
NIE WIDZI kolejnosci include'ow w main_centrala.cpp - i dokladnie taki blad
przeszedl przez 212 i 213 (HistoriaPobranie.h wciagniety przed
<ESPAsyncWebServer.h>). Ta brama sprawdza rzeczy, ktorych test nie sprawdzi.

Kazda kontrola ma numer (S01..S19). Bramy NIE WOLNO "naprawiac" przez
oslabienie kontroli - jesli kontrola nie przechodzi, to plik jest zly.
Selftest (selftest.py) psuje dane wejsciowe i wymaga, zeby brama to zlapala.

Uruchomienie:
    python3 -B narzedzia/brama-przed-wgraniem214/sprawdz.py
"""
import os
import re
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
FIRMWARE = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
D211 = os.path.join(REPO, "narzedzia", "wysylka-zapinka211")
D213 = os.path.join(REPO, "narzedzia", "panel-pobrania213")

WERSJA = "v3.56.2"
SERWER = "#include <ESPAsyncWebServer.h>"
INC_POBRANIE = '#include "HistoriaPobranie.h"'
INC_BILETY = '#include "HistoriaBilety.h"'
INC_PIN = '#include "HistoriaPin.h"'

# Symbole wprowadzone przez 211-213, ktore MUSZA miec dokladnie jedna definicje.
DEFINICJE = [
    "static const char* HISTORIA_TOKEN = SECRET_HISTORIA_TOKEN;",
    "static HistoriaBramka212 historiaBramka;",
    "static historia212::Pobranie historiaPobranie(",
    "static historia213::Bilety historiaBilety(",
    "static void historiaLosuj213(",
    "static std::uint32_t historiaZegar213(",
    "struct HistoriaBiletyPodpiecie213 {",
    "static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;",
]

DOZWOLONE_INCLUDE = {
    "cstddef", "cstdint", "cstring", "fcntl.h", "sys/stat.h",
    "CheckedHistoryIO.h", "HistoriaPin.h", "HistoriaBilety.h",
}

ZAKAZANE_W_NAGLOWKACH = ["<string>", "<vector>", "<functional>", "<iostream>", "<map>",
                         "throw ", "dynamic_cast", "typeid", "std::function"]

RE_ZNAK = re.compile(r"include\s*[<\"]([^>\"]+)[>\"]")


def _wynik(i, nazwa, ok, opis):
    return {"id": i, "nazwa": nazwa, "ok": bool(ok), "opis": opis}


def _pierwsze_uzycie(tekst, fragment):
    """Pozycja fragmentu poza komentarzami (// i /* */)."""
    bez = re.sub(r"/\*.*?\*/", "", tekst, flags=re.S)
    przesuniecie = 0
    for linia in bez.split("\n"):
        kod = linia.split("//")[0]
        i = kod.find(fragment)
        if i >= 0:
            return przesuniecie + i
        przesuniecie += len(linia) + 1
    return -1


def _linia_na(tekst, pozycja):
    return tekst.count("\n", 0, pozycja) + 1 if pozycja >= 0 else -1


def _linia(tekst, fragment):
    for i, l in enumerate(tekst.split("\n"), 1):
        if fragment in l:
            return i
    return -1


def uruchom(tekst, naglowki, przyklad, *, z_kompilacja=True, katalog_zrodla=None,
            secrets_h_istnieje=False, odwracalny=True):
    """Uruchamia wszystkie kontrole na podanym tekscie. Zwraca liste wynikow."""
    w = []
    linie = tekst.split("\n")

    # S01 - wersja firmware
    wersje = [i for i, l in enumerate(linie, 1) if l.startswith("static const char* FIRMWARE_VERSION")]
    ok = len(wersje) == 1 and ('FIRMWARE_VERSION = "%s";' % WERSJA) in tekst
    w.append(_wynik("S01", "FIRMWARE_VERSION = %s dokladnie raz" % WERSJA, ok,
                    "linie: %s" % wersje))

    # S02 - kazdy nasz include dokladnie raz
    problemy = []
    for inc in (INC_PIN, INC_POBRANIE, INC_BILETY):
        n = tekst.count(inc)
        if n != 1:
            problemy.append("%s x%d" % (inc, n))
    w.append(_wynik("S02", "kazdy include etapow 211-213 wystepuje raz", not problemy,
                    "; ".join(problemy) or "3/3 po jednym"))

    # S03 - KOLEJNOSC: naglowki HTTP po <ESPAsyncWebServer.h>
    i_serwera = tekst.find(SERWER)
    problemy = []
    if i_serwera < 0:
        problemy.append("brak <ESPAsyncWebServer.h>")
    else:
        for inc in (INC_POBRANIE, INC_BILETY):
            i = tekst.find(inc)
            if i < 0:
                problemy.append("brak %s" % inc)
            elif i < i_serwera:
                problemy.append("%s PRZED serwerem (linia %d < %d)"
                                % (inc, _linia(tekst, inc), _linia(tekst, SERWER)))
    w.append(_wynik("S03", "naglowki HTTP PO <ESPAsyncWebServer.h>", not problemy,
                    "; ".join(problemy) or "kolejnosc prawidlowa"))

    # S04 - dokladnie jedna definicja kazdego nowego symbolu globalnego
    problemy = []
    for d in DEFINICJE:
        n = tekst.count(d)
        if n != 1:
            problemy.append("%s x%d" % (d[:46], n))
    w.append(_wynik("S04", "kazdy nowy symbol globalny ma 1 definicje", not problemy,
                    "; ".join(problemy) or "%d/%d po jednej" % (len(DEFINICJE), len(DEFINICJE))))

    # S05 - domyslny token pusty
    ok = ('#ifndef SECRET_HISTORIA_TOKEN' in tekst and
          re.search(r'#define SECRET_HISTORIA_TOKEN\s+""', tekst) is not None)
    w.append(_wynik("S05", "domyslny token pusty (pobieranie wylaczone bez secrets.h)", ok,
                    "znaleziono" if ok else "brak #ifndef albo domysl nie jest pusty"))

    # S06 - secrets.h.example: jeden wpis, tylko zastepnik, zero sekretu
    problemy = []
    if przyklad.count("SECRET_HISTORIA_TOKEN") != 1:
        problemy.append("wpis SECRET_HISTORIA_TOKEN x%d" % przyklad.count("SECRET_HISTORIA_TOKEN"))
    if "ZMIEN-MNIE-NA-DLUGI-LOSOWY-TOKEN-32-ZNAKI" not in przyklad:
        problemy.append("brak zastepnika")
    for wartosc in re.findall(r'#define\s+\w+\s+"([^"]*)"', przyklad):
        if re.fullmatch(r"[A-Za-z0-9+/=_-]{24,}", wartosc) and not wartosc.startswith("ZMIEN-MNIE"):
            problemy.append("wyglada na prawdziwy sekret: %s..." % wartosc[:6])
    w.append(_wynik("S06", "secrets.h.example: zastepnik, nie sekret", not problemy,
                    "; ".join(problemy) or "czysto"))

    # S07 - w repo nie ma secrets.h
    w.append(_wynik("S07", "w repo nie ma pliku secrets.h", not secrets_h_istnieje,
                    "brak" if not secrets_h_istnieje else "PLIK ISTNIEJE - nie commituj!"))

    # S08 - trzy rejestracje z wlasciwymi metodami
    problemy = []
    for sciezka, metoda in (("/history", "HTTP_ANY"), ("/history/long", "HTTP_ANY"),
                            ("/history/link", "HTTP_GET")):
        wzor = 'wifiWebServer.on("%s", %s,' % (sciezka, metoda)
        if wzor not in tekst:
            problemy.append("%s nie %s" % (sciezka, metoda))
    w.append(_wynik("S08", "rejestracje: /history ANY, /history/long ANY, /history/link GET",
                    not problemy, "; ".join(problemy) or "3/3"))

    # S09 - panel: w literalzie HTML, bez tokenu, z biletem
    problemy = []
    i_html = tekst.find('R"PIECHTML(')
    j_html = tekst.find(')PIECHTML"')
    i_btn = tekst.find('id="histBtn"')
    if i_html < 0 or j_html < 0:
        problemy.append("brak literalu R\"PIECHTML(...)")
    elif not (i_html < i_btn < j_html):
        problemy.append("przycisk POZA literalem HTML (i_btn=%d zakres=%d..%d)" % (i_btn, i_html, j_html))
    i_konca = tekst.find("</script>", i_btn) if i_btn >= 0 else -1
    panel = tekst[i_btn:i_konca] if 0 <= i_btn and i_konca > i_btn else ""
    if ")PIECHTML" in panel:
        problemy.append("panel zawiera koniec literalu")
    for zakaz in ("HISTORIA_TOKEN", "SECRET_HISTORIA_TOKEN", "Bearer "):
        if zakaz in panel:
            problemy.append("panel zawiera %r" % zakaz)
    for wymog in ("/history/link?stream=", "/history?k=", "length !== 32"):
        if wymog not in panel:
            problemy.append("panel nie zawiera %r" % wymog)
    w.append(_wynik("S09", "panel w literalzie HTML, bez tokenu, z biletem", not problemy,
                    "; ".join(problemy) or "przycisk %d znakow" % len(panel)))

    # S10 - patch odwracalny (plik jest DOKLADNIE tym, co opisuje patch)
    w.append(_wynik("S10", "patch odwracalny (--dry-run -R)", odwracalny,
                    "odwraca sie czysto" if odwracalny else "patch nie odwraca sie czysto"))

    # S11 - bez CRLF i BOM
    problemy = []
    if "\r" in tekst:
        problemy.append("CRLF x%d" % tekst.count("\r"))
    if tekst.startswith("\ufeff"):
        problemy.append("BOM")
    w.append(_wynik("S11", "bez CRLF i bez BOM", not problemy, "; ".join(problemy) or "czysto"))

    # S12 - naglowki firmware: tylko dozwolone include'y
    problemy = []
    for nazwa, tresc in naglowki.items():
        for inc in RE_ZNAK.findall(tresc):
            if inc not in DOZWOLONE_INCLUDE:
                problemy.append("%s: %s" % (nazwa, inc))
    w.append(_wynik("S12", "naglowki firmware bez STL (tylko 6 dozwolonych)", not problemy,
                    "; ".join(problemy) or "czysto"))

    # S13 - naglowki firmware: bez wyjatkow/RTTI/STL
    problemy = []
    for nazwa, tresc in naglowki.items():
        for zakaz in ZAKAZANE_W_NAGLOWKACH:
            if zakaz in tresc:
                problemy.append("%s: %s" % (nazwa, zakaz))
    w.append(_wynik("S13", "naglowki bez wyjatkow, RTTI i std::string", not problemy,
                    "; ".join(problemy) or "czysto"))

    # S14 - zapinka 211 nietknieta: dwa wysylacze, dwa otwarcia, dwa zamkniecia
    problemy = []
    for fragment, ile in (("historia211::Pin pin;", 2), ("pin.open(", 2), ("pin.close()", 2)):
        if tekst.count(fragment) != ile:
            problemy.append("%s x%d (oczekiwane %d)" % (fragment, tekst.count(fragment), ile))
    w.append(_wynik("S14", "oba wysylacze Telegramu nadal na zapince 211", not problemy,
                    "; ".join(problemy) or "2 wysylacze x (pin.open + pin.close)"))

    # S15 - historiaSys zdefiniowany przed uzyciem w bramce
    problemy = []
    i_def = _linia(tekst, "static history193::SystemCalls historiaSys;")
    i_uzyj = _linia(tekst, "struct HistoriaBramka212")
    if i_def < 0 or i_uzyj < 0:
        problemy.append("brak definicji albo bramki")
    elif i_def > i_uzyj:
        problemy.append("historiaSys (linia %d) PO bramce (linia %d)" % (i_def, i_uzyj))
    w.append(_wynik("S15", "historiaSys zdefiniowany przed bramka", not problemy,
                    "; ".join(problemy) or "linia %d < %d" % (i_def, i_uzyj)))

    # S16 - esp_system.h przed pierwszym UZYCIEM esp_random() (komentarze nie licza sie)
    i_h = tekst.find("#include <esp_system.h>")
    i_r = _pierwsze_uzycie(tekst, "esp_random(")
    ok = i_h >= 0 and i_r >= 0 and i_h < i_r
    w.append(_wynik("S16", "esp_system.h przed pierwszym esp_random()", ok,
                    "include linia %d, uzycie linia %d"
                    % (_linia(tekst, "#include <esp_system.h>"), _linia_na(tekst, i_r))))

    # S17 - naglowki kompiluja sie jako gnu++11 (ESP32 Arduino: domyslnie C++11)
    if z_kompilacja:
        w.append(_kontrola_cpp11(katalog_zrodla))
    else:
        w.append(_wynik("S17", "naglowki kompiluja sie jako gnu++11", True, "pominieto"))

    # S18 - brak pustego #if z niezdefiniowanym makrem (zabezpieczenie przed literowka)
    ok = tekst.count("#ifndef SECRET_HISTORIA_TOKEN") == 1
    w.append(_wynik("S18", "sekcja tokenu dokladnie jedna", ok,
                    "1" if ok else "x%d" % tekst.count("#ifndef SECRET_HISTORIA_TOKEN")))

    return w


TU_Z11 = """// 214: jednostka kompilacji do sprawdzenia C++11 (ESP32 Arduino: gnu++11).
#include "Arduino.h"
#include "Serwer213.h"
#include "HistoriaBilety.h"
#include "HistoriaPobranie.h"
int main() { return 0; }
"""


def _kontrola_cpp11(katalog_zrodla):
    cele = [
        ("-I" + D213, "-I" + os.path.join(D213, "host")),
        ("-I" + D211, "-I" + os.path.join(D211, "host")),
        ("-I" + FIRMWARE,),
    ]
    if katalog_zrodla:
        cele.append(("-I" + katalog_zrodla,))
    flagi = [x for para in cele for x in para]
    with tempfile.TemporaryDirectory() as td:
        zrodlo = os.path.join(td, "kompilacja11.cpp")
        with open(zrodlo, "w", encoding="utf-8") as f:
            f.write(TU_Z11)
        for std in ("gnu++11", "gnu++17"):
            cmd = ["c++", "-std=" + std, "-fsyntax-only", "-Wall", "-Wextra", "-Werror",
                   "-fno-exceptions", "-fno-rtti", *flagi, zrodlo]
            t = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            if t.returncode != 0:
                return _wynik("S17", "naglowki kompiluja sie jako gnu++11", False,
                              "%s: %s" % (std, t.stderr.strip().split("\n")[0][:200]))
    return _wynik("S17", "naglowki kompiluja sie jako gnu++11 i gnu++17", True, "oba przeszly")
