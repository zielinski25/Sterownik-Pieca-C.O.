#!/usr/bin/env python3
"""Wycina z main_centrala.cpp kod, ktory liczy punkty wykresu dla panelu LCD.

Po co: firmware centrali to jeden plik na ok. 49 tys. linii, ktorego nie da
sie skompilowac na PC (potrzebuje bibliotek ESP32). Fragment dodany w v3.58.4
(miedzy znacznikami PANEL-WYKRESY-BEGIN / -END) korzysta jednak tylko z dwoch
struktur historii, kilku zmiennych i zwyklego C - mozna go wyciac i
skompilowac zwyklym g++. Testy sprawdzaja wiec DOKLADNIE ten tekst, ktory
trafia na plytke, a nie jego kopie przepisana recznie.

Uzycie:
  wytnij_blok.py <main_centrala.cpp> <plik_wyjsciowy.inc>      zapisz wycinek
  wytnij_blok.py <main_centrala.cpp> --sprawdz <plik.inc>      porownaj z istniejacym
  wytnij_blok.py <main_centrala.cpp> --esp32 <plik.cpp>        wycinek jako samodzielny plik .cpp
        dla sprawdz_naglowki_esp32.sh: zamiast atrap z test_panel_hist.cpp ma
        #include <Arduino.h> i deklaracje logu (DLOGF, DiagCategory...) wyciete
        doslownie z pliku centrali.
"""
import re
import sys


def wytnij(sciezka):
    with open(sciezka, encoding="utf-8") as f:
        tekst = f.read()

    def jeden(wzor, flagi=0):
        trafienia = re.findall(wzor, tekst, flagi)
        if len(trafienia) != 1:
            sys.exit("BLAD: wzor %r pasuje %d razy (oczekiwano 1) w %s" % (wzor, len(trafienia), sciezka))
        return trafienia[0]

    wersja = jeden(r'static const char\* FIRMWARE_VERSION = "(v[0-9.]+)";')
    czesci = [
        # stale i struktury historii - doslownie z pliku centrali
        jeden(r"^#define HISTORIA_MAX_PROBEK\s+\d+[^\n]*$", re.M),
        jeden(r"^#define HISTORIA_DLUGA_MAX_PROBEK\s+\d+[^\n]*$", re.M),
        jeden(r"^struct HistoriaProbka \{.*?^\};", re.M | re.S),
        jeden(r"^struct HistoriaProbkaDluga \{.*?^\};", re.M | re.S),
        # zmienne, z ktorych korzysta wyciety kod - te same deklaracje co w centrali
        jeden(r"^HistoriaProbka\* historiaBufor = nullptr;$", re.M),
        jeden(r"^int  historiaLiczbaProbek = 0;[^\n]*$", re.M),
        jeden(r"^int  historiaGlowa = 0;[^\n]*$", re.M),
        jeden(r"^bool historiaPelna = false;$", re.M),
        jeden(r"^static portMUX_TYPE historiaMux = portMUX_INITIALIZER_UNLOCKED;$", re.M),
        jeden(r"^HistoriaProbkaDluga\* historiaDlugaBufor = nullptr;$", re.M),
        jeden(r"^int  historiaDlugaLiczbaProbek = 0;[^\n]*$", re.M),
        jeden(r"^int  historiaDlugaGlowa = 0;[^\n]*$", re.M),
        jeden(r"^bool historiaDlugaPelna = false;$", re.M),
        # sam kod v3.58.4
        jeden(r"^// \[v3\.58\.4 PANEL-WYKRESY-BEGIN\]\n.*?^// \[v3\.58\.4 PANEL-WYKRESY-END\]$", re.M | re.S),
    ]
    naglowek = (
        "// WYCINEK z main_centrala.cpp %s - wygenerowany przez wytnij_blok.py.\n"
        "// NIE EDYTOWAC RECZNIE: zmiany rob w main_centrala.cpp i wytnij ponownie.\n"
        "// Zawiera: stale i struktury historii, deklaracje buforow oraz kod\n"
        "// [v3.58.4 PANEL-WYKRESY] (GET /api/history/panel).\n\n" % wersja
    )
    return naglowek + "\n\n".join(czesci) + "\n"


def wytnij_esp32(sciezka):
    """Wycinek z otoczeniem z samej centrali - do kompilacji prawdziwymi naglowkami ESP32."""
    with open(sciezka, encoding="utf-8") as f:
        tekst = f.read()

    def jeden(wzor, flagi=0):
        trafienia = re.findall(wzor, tekst, flagi)
        if len(trafienia) != 1:
            sys.exit("BLAD: wzor %r pasuje %d razy (oczekiwano 1) w %s" % (wzor, len(trafienia), sciezka))
        return trafienia[0]

    otoczenie = [
        # te same naglowki systemowe, ktore plik centrali wlacza przed tym kodem
        "#include <Arduino.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <atomic>\n"
        "#include <time.h>\n#include <stdlib.h>\n#include <stdio.h>\n#include <stdarg.h>",
        # log diagnostyczny - deklaracje doslownie z pliku centrali
        jeden(r"^enum DiagCategory : uint8_t \{.*?^\};", re.M | re.S),
        jeden(r"^enum DiagLevel : uint8_t \{[^\n]*\};$", re.M),
        jeden(r"^void diagEmitf\(DiagCategory cat, DiagLevel level, const char\* fmt, \.\.\.\);$", re.M),
        jeden(r"^#define DLOGF\(cat, level, fmt, \.\.\.\)[^\n]*$", re.M),
        # przelacznik zapisu historii na flash - sama deklaracja (definicja jest w centrali)
        jeden(r"^(bool historiaZapisNaFsWlaczony\(\)) \{$", re.M) + ";",
    ]
    koniec = (
        "// Wywolanie takie jak w trasie HTTP /api/history/panel (tam: tryb.c_str(), pkt, csv).\n"
        "int panelHistSprawdzenieSkladni(const String &tryb, int pkt, String &csv) {\n"
        "  return historiaPanelZbudujCsv(tryb.c_str(), pkt, csv);\n"
        "}\n"
    )
    naglowek = (
        "// Wygenerowane przez wytnij_blok.py --esp32 - NIE EDYTOWAC RECZNIE.\n"
        "// Kod [v3.58.4 PANEL-WYKRESY] z main_centrala.cpp z otoczeniem z tego samego\n"
        "// pliku, do sprawdzenia skladni i typow prawdziwymi naglowkami arduino-esp32.\n\n"
    )
    return naglowek + "\n\n".join(otoczenie) + "\n\n" + wytnij(sciezka) + "\n" + koniec


def main():
    if len(sys.argv) == 4 and sys.argv[2] == "--esp32":
        wynik = wytnij_esp32(sys.argv[1])
        with open(sys.argv[3], "w", encoding="utf-8", newline="") as f:
            f.write(wynik)
        print("zapisano %s (%d linii)" % (sys.argv[3], wynik.count("\n")))
        return
    if len(sys.argv) == 3:
        wynik = wytnij(sys.argv[1])
        with open(sys.argv[2], "w", encoding="utf-8", newline="") as f:
            f.write(wynik)
        print("zapisano %s (%d linii)" % (sys.argv[2], wynik.count("\n")))
        return
    if len(sys.argv) == 4 and sys.argv[2] == "--sprawdz":
        wynik = wytnij(sys.argv[1])
        with open(sys.argv[3], encoding="utf-8") as f:
            istniejacy = f.read()
        if wynik != istniejacy:
            sys.exit("BLAD: %s rozni sie od kodu w %s - wytnij ponownie" % (sys.argv[3], sys.argv[1]))
        print("wycinek zgodny z %s" % sys.argv[1])
        return
    sys.exit(__doc__)


if __name__ == "__main__":
    main()
