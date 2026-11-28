#!/usr/bin/env python3
"""Wycina z main_centrala.cpp 3.58.22 kod telemetrii minutowej potrzebny do testu
pol "dht_kod"/"dht_surowa" na PC: struct TelemetrySample + stale TEMP_DIAG_*/
TELEMETRY_SIM_* + telemetryHasTempDiag/encodeTempDiag, telemetryJsonAppend(),
telemetryFormatRecordId(), telemetryAppendPathForSample() oraz fragment
telemetryBuildSnapshot() wypelniajacy pola DHT (jako funkcja testowa).
Uzycie: wytnij_telemetria.py <main_centrala.cpp> <wyjscie.h>"""
import sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
if 'const char* FIRMWARE_VERSION = "v3.58.22";' not in t:
    sys.exit('brak FIRMWARE_VERSION = "v3.58.22"')

def wytnij(poczatek, koniec, zawiera_koniec=False):
    if t.count(poczatek) != 1:
        sys.exit("nie ma dokladnie jednego: %r (%d)" % (poczatek[:60], t.count(poczatek)))
    i = t.index(poczatek)
    j = t.index(koniec, i)
    if zawiera_koniec:
        j += len(koniec)
    return t[i:j]

kanaly = wytnij("enum TelemetryChannel : uint8_t {", "};\n", True)
probka = wytnij("struct TelemetrySample {", 'static_assert(offsetof(TelemetrySample, tempDiagReserved) == 78, "TelemetrySample reserved offset changed");\n', True)
dht_glob = wytnij("static volatile uint8_t g_dhtOstatniKod = 0;", "static volatile bool    g_dhtBylOdczyt = false;\n", True)
json_app = wytnij("static void telemetryJsonAppend(char *buf, size_t cap, size_t &used, const char *fmt, ...) {", "\n}\n", True)
rec_id = wytnij("static void telemetryFormatRecordId(const TelemetrySample &s, char *out, size_t outCap) {", "\n}\n", True)
path_fn = wytnij("static bool telemetryAppendPathForSample(const TelemetrySample &s, char *buf, size_t cap, size_t &used, bool &first) {", "\n}\n", True)
# fragment telemetryBuildSnapshot(): od "out.tDhtDiagX10 = encodeTempDiag" do "return true;" - opakowany w funkcje testowa
frag = wytnij("  out.tDhtDiagX10 = encodeTempDiag(czytajTempDiag(), -40.0f, 80.0f);", "  return true;\n", True)
frag_fn = ("// fragment telemetryBuildSnapshot() 1:1 (czytajTempDiag() dostarcza test)\n"
           "static bool telemetryWypelnijDht(TelemetrySample &out) {\n" + frag + "}\n")
for musi in ('dht_kod', 'dht_surowa', 'TELEMETRY_SIM_CH_MASK'):
    if musi not in path_fn:
        sys.exit("w telemetryAppendPathForSample() brak %r - to nie jest kod 3.58.22" % musi)
out = ("// WYCINEK z main_centrala.cpp v3.58.22: telemetria minutowa (pola dht_kod/dht_surowa) - wygenerowany przez wytnij_telemetria.py.\n"
       + kanaly + "\n" + probka + "\n" + dht_glob + "\n" + json_app + "\n" + rec_id + "\n" + path_fn + "\n" + frag_fn)
open(dst, "w", encoding="utf-8").write(out)
print("zapisano %s (%d linii)" % (dst, out.count("\n")))
