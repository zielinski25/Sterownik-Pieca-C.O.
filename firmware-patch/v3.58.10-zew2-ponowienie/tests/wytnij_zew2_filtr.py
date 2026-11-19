#!/usr/bin/env python3
"""Wycina z main_centrala.cpp 3.58.10 blok [v3.58.10 ZEW2-FILTR]
(--- ZEW2-FILTR-POCZATEK --- ... --- ZEW2-FILTR-KONIEC ---) do testu na PC.
Uzycie: wytnij_zew2_filtr.py <main_centrala.cpp> <wyjscie.h>"""
import sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
P, K = "// --- ZEW2-FILTR-POCZATEK ---\n", "// --- ZEW2-FILTR-KONIEC ---\n"
if t.count(P) != 1 or t.count(K) != 1:
    sys.exit("brak dokladnie jednego bloku ZEW2-FILTR w %s" % src)
blok = t[t.index(P) + len(P):t.index(K)]
if 'const char* FIRMWARE_VERSION = "v3.58.10";' not in t:
    sys.exit('brak FIRMWARE_VERSION = "v3.58.10"')
# Na ESP32 (Xtensa, 32-bit) unsigned long = 32 bity jak millis(); na PC x86-64 ma
# 64 bity, wiec test przepelnienia millis() [11] bylby falszywy. Podmiana TYLKO
# w wycinku testowym - firmware zostaje z unsigned long (typ millis()).
blok = blok.replace("unsigned long", "uint32_t")
open(dst, "w", encoding="utf-8").write(
    "// WYCINEK z main_centrala.cpp v3.58.10 ([v3.58.10 ZEW2-FILTR]) - wygenerowany przez wytnij_zew2_filtr.py.\n"
    "// unsigned long -> uint32_t (szerokosc millis() na ESP32), patrz skrypt.\n"
    + blok)
print("zapisano %s (%d linii bloku ZEW2-FILTR)" % (dst, blok.count("\n")))
