#!/usr/bin/env python3
"""Wycina z main_centrala.cpp 3.58.21 PRAWDZIWY kod zadania wilgotnosc() razem
z blokiem Zew 2 (zapiszTempDiag/czytajTempDiag/jsonTempDiag + filtr ZEW2-FILTR
+ g_zew2Filtr) i zmiennymi dhtOstatniaProbaMs/dhtBylaProba, do testu na PC
(test_wilgotnosc.cpp dostarcza atrapy: millis, read_dht, telemetria, DLOGF).
Uzycie: wytnij_wilgotnosc.py <main_centrala.cpp> <wyjscie.h>"""
import sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
if 'const char* FIRMWARE_VERSION = "v3.58.21";' not in t:
    sys.exit('brak FIRMWARE_VERSION = "v3.58.21"')

def wytnij(poczatek, koniec, zawiera_koniec=False):
    if t.count(poczatek) != 1:
        sys.exit("nie ma dokladnie jednego: %r (%d)" % (poczatek[:60], t.count(poczatek)))
    i = t.index(poczatek)
    j = t.index(koniec, i)
    if zawiera_koniec:
        j += len(koniec)
    return t[i:j]

# 1. blok diagnostyczny Zew 2: od portMUX do konca "static Zew2Filtr g_zew2Filtr = ...;"
blok_zew2 = wytnij("static portMUX_TYPE g_tempDiagMux = portMUX_INITIALIZER_UNLOCKED;",
                   "static Zew2Filtr g_zew2Filtr = {NAN, NAN, 0, false, 0, 0, 0, 0, false, 0, 0, 0};\n", True)
# 2. zmienne DHT przy #define DHT_PIN
dht_zm = wytnij("static unsigned long dhtOstatniaProbaMs = 0;", "static bool dhtBylaProba = false;\n", True)
# 3. cala funkcja wilgotnosc()
wilg = wytnij("void wilgotnosc() {", "\n}\n", True)

for musi in ("zew2Filtruj(g_zew2Filtr", "powtorkaZew2", "zew2RamkaPoprawna(dhtErr", "zew2OdczytDs(&dsTemp)"):
    if musi not in wilg:
        sys.exit("w wilgotnosc() brak %r - to nie jest kod 3.58.21" % musi)

# Na ESP32 unsigned long = 32 bity (millis()); na PC 64 - test przepelnienia bylby falszywy.
out = ("// WYCINEK z main_centrala.cpp v3.58.21: blok Zew 2 + filtr + wilgotnosc() - wygenerowany przez wytnij_wilgotnosc.py.\n"
       "// unsigned long -> uint32_t (szerokosc millis() na ESP32), patrz skrypt.\n"
       + (blok_zew2 + "\n" + dht_zm + "\n" + wilg).replace("unsigned long", "uint32_t"))
open(dst, "w", encoding="utf-8").write(out)
print("zapisano %s (%d linii: blok Zew 2 %d, wilgotnosc() %d)" % (
    dst, out.count("\n"), blok_zew2.count("\n"), wilg.count("\n")))
