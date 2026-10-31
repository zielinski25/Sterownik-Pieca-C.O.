#!/usr/bin/env python3
"""211: naloz patch v3.55.8 -> v3.55.9 na PRAWDZIWY plik centrali, sprawdz
wynik i wyciagnij z niego obie funkcje wysylajace do testu hostowego.

Ten skrypt nie kompiluje i nie wysyla nic na urzadzenie. Sprawdza tylko, ze:
  * patch naklada sie czysto na plik o przypietym SHA-256 (ten sam, ktory
    audytowal etap 210),
  * zmienilo sie DOKLADNIE tyle, ile opisuje patch (5 fragmentow),
  * obie funkcje faktycznie korzystaja z zapinki i nie otwieraja juz pliku
    historii po nazwie,
  * stala sciezka VFS w HistoriaPin.h jest identyczna z ta, ktorej uzywa
    prawdziwy history193::Engine::name().

Wynik: Extracted211.inc (dokladny tekst obu funkcji z pliku po patchu).
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
FIRMWARE = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
ZRODLO = os.path.join(FIRMWARE, "main_centrala.cpp")
PATCH = os.path.join(TU, "main_centrala-v3.55.8-to-v3.55.9.patch")
WYJSCIE = os.path.join(TU, "Extracted211.inc")
SHA_ZRODLA211 = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"
FUNKCJE = ("sendTelegramDocumentHistKrotka", "sendTelegramDocumentHistDluga")


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def wczytaj(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def wytnij_funkcje(tekst, nazwa):
    linie = tekst.split("\n")
    start = None
    for i, l in enumerate(linie):
        if l.startswith("bool %s()" % nazwa):
            start = i
            break
    if start is None:
        raise SystemExit("brak funkcji %s" % nazwa)
    glebokosc = 0
    otwarte = False
    for j in range(start, len(linie)):
        for znak in linie[j]:
            if znak == "{":
                glebokosc += 1
                otwarte = True
            elif znak == "}":
                glebokosc -= 1
                if otwarte and glebokosc == 0:
                    return "\n".join(linie[start:j + 1]) + "\n"
    raise SystemExit("niedomknieta funkcja %s" % nazwa)


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA211:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS build211 zrodlo v3.55.8 sha256 zgodny z 210")

    tmp = tempfile.mkdtemp(prefix="b211-")
    try:
        cel = os.path.join(tmp, "main_centrala.cpp")
        shutil.copyfile(ZRODLO, cel)
        sprawdzenie = subprocess.run(["patch", "-p1", "--dry-run", "-i", PATCH],
                                     cwd=tmp, capture_output=True, text=True)
        # patch -p1 szuka a/main_centrala.cpp -> main_centrala.cpp
        if sprawdzenie.returncode != 0:
            raise SystemExit("patch nie naklada sie: %s%s" % (sprawdzenie.stdout, sprawdzenie.stderr))
        naloz = subprocess.run(["patch", "-p1", "-i", PATCH], cwd=tmp,
                               capture_output=True, text=True)
        if naloz.returncode != 0:
            raise SystemExit("patch nie nalozyl sie: %s%s" % (naloz.stdout, naloz.stderr))
        wynik = wczytaj(cel)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if 'static const char* FIRMWARE_VERSION = "v3.55.9";' not in wynik:
        raise SystemExit("po patchu brak FIRMWARE_VERSION v3.55.9")
    print("PASS build211 patch nalozony, FIRMWARE_VERSION v3.55.9")

    # Piec zmian tekstowych daje dziesiec fragmentow diffu (kazda funkcja
    # dzieli sie na dwa, bo srodek - budowa naglowka multipart - jest bez zmian).
    fragmenty = len([l for l in wczytaj(PATCH).split("\n") if l.startswith("@@")])
    if fragmenty != 10:
        raise SystemExit("oczekiwano 10 fragmentow patcha, jest %d" % fragmenty)
    print("PASS build211 patch: 5 zmian, 10 fragmentow diffu, nic wiecej")

    # Wlasciciel VFS zadeklarowany dokladnie raz.
    deklaracje = [l for l in wynik.split("\n")
                  if l.lstrip().startswith("static history193::SystemCalls historiaSys;")]
    if len(deklaracje) != 1:
        raise SystemExit("historiaSys: oczekiwano jednej definicji")
    print("PASS build211 historiaSys ma jedna definicje po patchu")

    # Sciezki VFS zgodne z prawdziwym Engine::name().
    engine = wczytaj(os.path.join(FIRMWARE, "CheckedHistoryIO.h"))
    linia_engine = [l for l in engine.split("\n") if "const char*name(Stream s)" in l]
    if len(linia_engine) != 1:
        raise SystemExit("nie odczytalem linii name() z CheckedHistoryIO.h")
    pary = re.findall(r'Stream::Short\?"([^"]+)":"([^"]+)"', linia_engine[0])
    if len(pary) != 1:
        raise SystemExit("nie odczytalem sciezek z name() w CheckedHistoryIO.h")
    krotka_eng, dluga_eng = pary[0]
    pin_h = wczytaj(os.path.join(TU, "HistoriaPin.h"))
    krotka_pin = re.search(r'kShortVfsPath = "([^"]+)"', pin_h)
    dluga_pin = re.search(r'kLongVfsPath = "([^"]+)"', pin_h)
    if not krotka_pin or not dluga_pin:
        raise SystemExit("brak stalych sciezek w HistoriaPin.h")
    if krotka_pin.group(1) != krotka_eng or dluga_pin.group(1) != dluga_eng:
        raise SystemExit("sciezki VFS w HistoriaPin.h nie zgadzaja sie z Engine::name()")
    print("PASS build211 sciezki VFS zgodne z Engine::name() (%s, %s)" % (krotka_eng, dluga_eng))

    # Obie funkcje: zapinka jest, wlasnej petli open/read nie ma.
    teksty = {}
    for nazwa in FUNKCJE:
        cialo = wytnij_funkcje(wynik, nazwa)
        teksty[nazwa] = cialo
        warunki = [
            ("historia211::Pin pin;", "brak zapinki"),
            ("pin.open(historiaSys,", "zapinka nie jest otwierana na wlasnym VFS historii"),
            ("xSemaphoreTake(logMutex", "brak logMutexa przy otwieraniu zapinki"),
            ("pin.close()", "brak jawnego zamkniecia zapinki"),
            ("if (!cialoOk) goto", "brak przerwania po nieudanym ciele"),
            ("historia211::ReadCode::Data", "brak rozroznienia bledu odczytu od konca"),
            ("zapisano != odczyt.bytes", "krotki zapis do gniazda nadal przechodzi"),
        ]
        for wzor, opis in warunki:
            if wzor not in cialo:
                raise SystemExit("%s: %s" % (nazwa, opis))
        for zakaz, opis in [
            ('systemFS196.exists(HISTORIA', "nadal sprawdza istnienie przez Arduino FS"),
            ("File plik = ", "nadal otwiera plik po nazwie"),
            ("g_histShortSize;", "nadal bierze dlugosc z cache bez zapinki"),
            ("g_histLongSize;", "nadal bierze dlugosc z cache bez zapinki"),
            ("if (n == 0) break;", "krotki odczyt nadal jest koncem"),
        ]:
            if zakaz in cialo:
                raise SystemExit("%s: %s" % (nazwa, opis))
    print("PASS build211 obie funkcje korzystaja z zapinki i nie otwieraja pliku po nazwie")

    # Zapis pliku wynikowego.
    naglowek = (
        "// WYGENEROWANE przez narzedzia/wysylka-zapinka211/build.py - nie edytowac.\n"
        "// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp\n"
        "//         (sha256 %s) + main_centrala-v3.55.8-to-v3.55.9.patch\n"
        "// To jest dokladny tekst obu funkcji z pliku PO patchu, bez zadnych zmian.\n\n"
    ) % SHA_ZRODLA211
    with open(WYJSCIE, "w", encoding="utf-8", newline="") as f:
        f.write(naglowek + teksty[FUNKCJE[0]] + "\n" + teksty[FUNKCJE[1]])
    print("PASS build211 Extracted211.inc %d linii" % len(wczytaj(WYJSCIE).split("\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
