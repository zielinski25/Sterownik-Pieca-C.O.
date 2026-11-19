#!/usr/bin/env python3
"""Wycina z main_centrala.cpp 3.58.8 linie [v3.58.8 FW-W-STATUSIE] z wifiZbudujJsonStatus()
do sprawdzenia prawdziwymi naglowkami ESP32 (Arduino.h: String, F()).
Uzycie: wytnij_fw_status.py <main_centrala.cpp> <wyjscie.cpp>"""
import re, sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
i = t.index("String wifiZbudujJsonStatus() {")
j = t.index("\n}\n", i)
cialo = t[i:j]
m = re.search(r'\n(  j \+= F\("\\"fw\\":\\""\); j \+= FIRMWARE_VERSION; j \+= F\("\\",\"\);)\n', cialo)
if not m:
    sys.exit("w wifiZbudujJsonStatus() nie ma linii j += F(\"\\\"fw\\\":\\\"\"); j += FIRMWARE_VERSION; ...")
linia = m.group(1)
if 'const char* FIRMWARE_VERSION = "v3.58.8";' not in t:
    sys.exit("brak FIRMWARE_VERSION = \"v3.58.8\"")
out = '''// WYCINEK z main_centrala.cpp v3.58.8 ([v3.58.8 FW-W-STATUSIE]) - wygenerowany przez wytnij_fw_status.py.
#include <Arduino.h>
static const char* FIRMWARE_VERSION = "v3.58.8";
String wifiZbudujJsonStatus_fragment() {
  String j;
  j.reserve(512);
  j += F("{");
%s
  j += F("}");
  return j;
}
''' % linia
open(dst, "w", encoding="utf-8").write(out)
print("zapisano %s (linia z wifiZbudujJsonStatus(): %s)" % (dst, linia.strip()))
