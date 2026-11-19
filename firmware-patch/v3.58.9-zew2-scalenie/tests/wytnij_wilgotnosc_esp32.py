#!/usr/bin/env python3
"""Wycina z main_centrala.cpp 3.58.9 blok Zew 2 (zapiszTempDiag/czytajTempDiag/
jsonTempDiag + filtr ZEW2-FILTR) i cala funkcje wilgotnosc() do sprawdzenia
PRAWDZIWYMI naglowkami ESP32 (Arduino.h: String, millis, portMUX_TYPE,
esp_timer_get_time, isfinite/fabsf/isnan). Funkcje centrali spoza wycinka
dostaja deklaracje/atrapy o sygnaturach z pliku centrali.
Uzycie: wytnij_wilgotnosc_esp32.py <main_centrala.cpp> <wyjscie.cpp>"""
import sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
if 'const char* FIRMWARE_VERSION = "v3.58.9";' not in t:
    sys.exit('brak FIRMWARE_VERSION = "v3.58.9"')

def wytnij(poczatek, koniec, zawiera_koniec=False):
    if t.count(poczatek) != 1:
        sys.exit("nie ma dokladnie jednego: %r (%d)" % (poczatek[:60], t.count(poczatek)))
    i = t.index(poczatek); j = t.index(koniec, i)
    if zawiera_koniec: j += len(koniec)
    return t[i:j]

blok_zew2 = wytnij("static portMUX_TYPE g_tempDiagMux = portMUX_INITIALIZER_UNLOCKED;",
                   "static Zew2Filtr g_zew2Filtr = {NAN, NAN, 0, false, 0, 0, 0, 0};\n", True)
dht_zm = wytnij("static unsigned long dhtOstatniaProbaMs = 0;", "static bool dhtBylaProba = false;\n", True)
wilg = wytnij("void wilgotnosc() {", "\n}\n", True)
# deklaracje z pliku centrali (sygnatury 1:1)
dekl = [
    wytnij("enum DiagCategory : uint8_t {", "};\n", True),
    wytnij("enum DiagLevel : uint8_t {", "\n", True),
    wytnij("void diagEmitf(DiagCategory cat, DiagLevel level, const char* fmt, ...);", "\n", True),
    wytnij("#define DLOGF(cat, level, fmt, ...)", "\n", True),
]
for musi in ("zew2Filtruj(g_zew2Filtr", "powtorkaZew2"):
    if musi not in wilg:
        sys.exit("w wilgotnosc() brak %r" % musi)

out = '''// WYCINEK z main_centrala.cpp v3.58.9 (blok Zew 2 + filtr + wilgotnosc()) do sprawdzenia
// prawdziwymi naglowkami ESP32 - wygenerowany przez wytnij_wilgotnosc_esp32.py.
#include <Arduino.h>
#include <esp_timer.h>
%s
// --- atrapy funkcji/zmiennych centrali spoza wycinka (sygnatury jak w pliku) ---
static const uint8_t DHT_OK = 0;
static const uint8_t DHT22 = 22;
static uint8_t read_dht(float &temperature, float &humidity, uint8_t pin, uint8_t dhttype);
#define DHT_PIN 6
enum TelemetryChannel : uint8_t { TELEMETRY_CH_RH = 8 };
static uint64_t telemetryMonotonicMs();
static void telemetryMarkAttempt(TelemetryChannel ch, uint64_t nowMs);
static void telemetryMarkInvalid(TelemetryChannel ch);
static void telemetryMarkAccepted(TelemetryChannel ch, uint64_t nowMs, bool simulated);
unsigned long czasOstatniegoPomiaru = 0;
unsigned long interwalPomiaru = 600000;
void ustawInterwalPomiaru();
bool symulacjaTrwa(bool &aktywna, unsigned long &do_, const char* nazwaCzujnika);
bool symulacjaWilgotnosciAktywna = false;
unsigned long symulacjaWilgotnosciDo = 0;
float symulowanaWilgotnosc = 0;
float temperaturaNaZewnatrz = 0;
float wilgotnoscSkorygowana = 0.0;
float wilgotnoscOdczytana = 0.0;
int strzalkaWilgotnosc = 0;
float obliczSkorygowanaWilgotnoscWzgledna(float wilgotnoscAbsolutna, float temperaturaNaZewnatrz);
float obliczWilgotnoscAbsolutna(float temperaturaNaZewnatrz, float wilgotnoscWzgledna);

// --- kod centrali 3.58.9 ---
%s
%s
%s
// --- definicje atrap (zeby plik byl kompletny dla -fsyntax-only) ---
void diagEmitf(DiagCategory, DiagLevel, const char*, ...) {}
static uint8_t read_dht(float &temperature, float &humidity, uint8_t, uint8_t) { temperature = 0; humidity = 0; return DHT_OK; }
static uint64_t telemetryMonotonicMs() { return millis(); }
static void telemetryMarkAttempt(TelemetryChannel, uint64_t) {}
static void telemetryMarkInvalid(TelemetryChannel) {}
static void telemetryMarkAccepted(TelemetryChannel, uint64_t, bool) {}
void ustawInterwalPomiaru() {}
bool symulacjaTrwa(bool &aktywna, unsigned long &, const char*) { return aktywna; }
float obliczSkorygowanaWilgotnoscWzgledna(float a, float) { return a; }
float obliczWilgotnoscAbsolutna(float, float b) { return b; }
String uzyjJson() { return jsonTempDiag(); }
''' % ("".join(dekl), blok_zew2, dht_zm, wilg)
open(dst, "w", encoding="utf-8").write(out)
print("zapisano %s (%d linii)" % (dst, out.count("\n")))
