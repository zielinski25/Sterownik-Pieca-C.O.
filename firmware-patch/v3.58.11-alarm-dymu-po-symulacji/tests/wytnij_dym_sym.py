#!/usr/bin/env python3
"""Wycina z main_centrala.cpp 3.58.11 PRAWDZIWY kod:
  * blok // --- DYM-SYM-POCZATEK/KONIEC --- (flaga, dymAlarmSkasuj(), dymAlarmPoKoncuSymulacji()),
  * fragment pozar() od "if (czujnikDymuSwiezyOdczyt && czujnikDymuOdczyt > progAlarmowyDymu)"
    do konca galezi dezaktywacji - opakowany w krokAlarmuDymu(symDymTeraz),
  * samo wywolanie dymAlarmPoKoncuSymulacji() z poczatku pozar() (sprawdzane tekstowo).
Uzycie: wytnij_dym_sym.py <main_centrala.cpp> <wyjscie.h>"""
import sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
if 'const char* FIRMWARE_VERSION = "v3.58.11";' not in t:
    sys.exit('brak FIRMWARE_VERSION = "v3.58.11"')

def wytnij(od, do_, z_koncem=False):
    a = t.index(od)
    b = t.index(do_, a)
    return t[a:b + (len(do_) if z_koncem else 0)]

blok = wytnij("// --- DYM-SYM-POCZATEK ---", "// --- DYM-SYM-KONIEC ---", True)
poz = t[t.index("void pozar() {"):]
# wywolanie na poczatku pozar()
musi = ['bool symDymTeraz = symulacjaTrwa(symulacjaDymAktywna, symulacjaDymDo, "dym");',
        'dymAlarmPoKoncuSymulacji(symDymTeraz);',
        'alarmDymuZSymulacji = symDymTeraz;',
        'if (czyAlarmAktywny == 1 && !symDymTeraz) alarmDymuZSymulacji = false;',
        'dymAlarmSkasuj(nullptr);']
for m in musi:
    if m not in poz[:12000]:
        sys.exit("w pozar() brak %r" % m)
a = poz.index("  if (czujnikDymuSwiezyOdczyt && czujnikDymuOdczyt > progAlarmowyDymu) {")
b = poz.index("  // W zwyklej pracy syberek otwiera sie na poczatku alarmu", a)
segment = poz[a:b]
assert segment.count("czyAlarmAktywny = 1;") == 1 and segment.count("dymAlarmSkasuj(nullptr);") == 1

out = ("// WYCINEK z main_centrala.cpp v3.58.11: blok DYM-SYM + decyzja alarmu z pozar() - wygenerowany przez wytnij_dym_sym.py.\n"
       "// NIE EDYTOWAC - zrodlem jest plik centrali.\n\n"
       + blok + "\n\n"
       "// Fragment pozar() (bez zmian wzgledem pliku centrali) - aktywacja / dezaktywacja alarmu POZAR.\n"
       "static void krokAlarmuDymu(bool symDymTeraz) {\n"
       "  int krokSyberka = 1;\n"
       + segment +
       "}\n")
open(dst, "w", encoding="utf-8").write(out)
print("wytnij_dym_sym.py: blok DYM-SYM %d linii, fragment pozar() %d linii -> %s" % (blok.count("\n"), segment.count("\n"), dst))
