#!/usr/bin/env python3
"""216: compilacja NOWEGO HistoriaPobranie.h wobec PRAWDZIWEGO ESPAsyncWebServer.

Po co: etap 215 udowodnil, ze naglowki z v3.56.2 kompiluja sie wobec
prawdziwego API biblioteki. Etap 216 zmienia ten naglowek (dodaje
Content-Disposition), wiec dowod trzeba powtorzyc - inaczej powtarzamy blad D1:
"na tescie hostowym dziala" != "skompiluje sie na ESP".

Kontrole:
  A      sam naglowek biblioteki (sensownosc shimu),
  B11/17 nasze trzy naglowki (211/213/216) wobec prawdziwego API, -Werror,
  C11/17 szkielet z PRAWDZIWYM kodem z pliku po patchu v3.56.3
         (bramka, obiekt trasy, trzy lambdy, tabela tras),
  F      kontrola negatywna: kolejnosc include z etapu 213 (nasze naglowki
         przed <ESPAsyncWebServer.h>) MUSI sie nie kompilowac. Na modelu
         hostowym kompilowala sie - i tak przeszedl blad D1.

Dwa standardy: rdzen ESP32 Arduino 2.x kompiluje gnu++11, 3.x gnu++17.

Uruchomienie:
    python3 -B narzedzia/od-przycisku216/api216.py
"""
import os
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
N = os.path.join(REPO, "narzedzia")
D211 = os.path.join(N, "wysylka-zapinka211")
D213 = os.path.join(N, "panel-pobrania213")
D215 = os.path.join(N, "brama-api-esp215")
FW = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")

sys.path.insert(0, D215)
import przygotuj_api as API  # noqa: E402

STANDARDY = ["gnu++11", "gnu++17"]
BAZA = ["-DARDUINO=10607", "-DHOST", "-fno-exceptions", "-fno-rtti", "-Wall", "-Wextra"]

SZKIELET = r'''// [216] jednostka kompilacji: PRAWDZIWE API + nasze naglowki + kod z pliku.
#include <ESPAsyncWebServer.h>
#include <cstdint>
#include <type_traits>

#include "HistoriaPin.h"
#include "HistoriaPobranie.h"
#include "HistoriaBilety.h"

// --- minimum z centrali, ktorego wymaga wyciagniety kod -------------------
class Sem {};
typedef Sem *SemaphoreHandle_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
static Sem g_sem;
static SemaphoreHandle_t logMutex = &g_sem;
static inline int xSemaphoreTake(SemaphoreHandle_t, unsigned) { return pdTRUE; }
static inline int xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }

static bool littlefsReady = true;
static volatile size_t g_histShortSize = 0;
static volatile size_t g_histLongSize = 0;
static bool historiaDostepGotowy() { return true; }

static history193::SystemCalls historiaSys;
static const char *HISTORIA_TOKEN = "token-kompilacyjny";

// --- PRAWDZIWY kod z pliku po patchu v3.56.3 -----------------------------
#include "Extracted216.inc"

// --- co musi byc prawda o typach (sprawdza kompilator, nie ja) -----------
static_assert(std::is_base_of<AsyncAbstractResponse, historia212::Odpowiedz>::value,
              "Odpowiedz musi dziedziczyc po PRAWDZIWYM AsyncAbstractResponse");
static_assert(std::is_same<decltype(&historia212::Odpowiedz::_fillBuffer),
                           size_t (historia212::Odpowiedz::*)(uint8_t *, size_t)>::value,
              "_fillBuffer musi miec podpis z prawdziwej biblioteki");
static_assert(std::is_same<decltype(&historia212::Odpowiedz::_sourceValid),
                           bool (historia212::Odpowiedz::*)() const>::value,
              "_sourceValid musi byc const, jak w bibliotece");
static_assert(sizeof(TRASY_OBSLUGI216) / sizeof(TRASY_OBSLUGI216[0]) == 3,
              "tabela tras musi miec trzy wpisy");

int main() { return 0; }
'''

# Kontrola F: kolejnosc include z etapow 212/213 (nasze naglowki PRZED serwerem).
# Na modelu hostowym to sie kompilowalo - i wlasnie dlatego blad D1 przeszedl.
# Wobec prawdziwego naglowka MUSI byc blad. Jesli przejdzie, brama patrzy na
# model, a nie na biblioteke.
KOLEJNOSC = r'''// [216 F] nasze naglowki przed <ESPAsyncWebServer.h> - MUSI sie nie kompilowac.
#include "HistoriaPin.h"
#include "HistoriaPobranie.h"
#include "HistoriaBilety.h"
#include <ESPAsyncWebServer.h>
int main() { return 0; }
'''


def uruchom(cmd, timeout=900):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def kompiluj(zrodlo, api, shim, standard, werror=True):
    cmd = ["c++", "-fsyntax-only", "-std=" + standard, *BAZA]
    if werror:
        cmd.append("-Werror")
    cmd += ["-Wno-unused-parameter", "-isystem", api, "-I" + shim,
            "-I" + TU, "-I" + D213, "-I" + D211, "-I" + FW, zrodlo]
    return uruchom(cmd)


def pierwszy_blad(wynik):
    for linia in wynik.stderr.split("\n"):
        if "error:" in linia:
            return linia.strip()[:220]
    return wynik.stderr.strip().split("\n")[0][:220] if wynik.stderr else ""


def main():
    tmp = tempfile.mkdtemp(prefix="api216-")
    bledy = []
    try:
        info = API.przygotuj(tmp)
        api = os.path.join(tmp, "api")
        shim = os.path.join(tmp, "shim")
        print("PASS api216 przygotowano %d plikow API i %d shimow"
              % (len(info["api"]), len(info["shim"])))

        baza = os.path.join(tmp, "api.h")
        with open(baza, "w", encoding="utf-8") as f:
            f.write("#include <ESPAsyncWebServer.h>\nint main() { return 0; }\n")
        for std in STANDARDY:
            w = kompiluj(baza, api, shim, std)
            if w.returncode != 0:
                bledy.append("A/%s: %s" % (std, pierwszy_blad(w)))
                break
        else:
            print("PASS api216 A naglowek biblioteki kompiluje sie (%s)" % ", ".join(STANDARDY))

        naglowki = os.path.join(tmp, "naglowki.cpp")
        with open(naglowki, "w", encoding="utf-8") as f:
            f.write('#include <ESPAsyncWebServer.h>\n'
                    '#include "HistoriaPin.h"\n'
                    '#include "HistoriaPobranie.h"\n'
                    '#include "HistoriaBilety.h"\n'
                    'int main() { return 0; }\n')
        for std in STANDARDY:
            w = kompiluj(naglowki, api, shim, std)
            if w.returncode != 0:
                bledy.append("B/%s: %s" % (std, pierwszy_blad(w)))
            else:
                print("PASS api216 B-%s nasze naglowki v3.56.3 wobec prawdziwego API"
                      % std)

        szkielet = os.path.join(tmp, "szkielet.cpp")
        with open(szkielet, "w", encoding="utf-8") as f:
            f.write(SZKIELET)
        for std in STANDARDY:
            w = kompiluj(szkielet, api, shim, std)
            if w.returncode != 0:
                bledy.append("C/%s: %s" % (std, pierwszy_blad(w)))
            else:
                print("PASS api216 C-%s kod z pliku v3.56.3 kompiluje sie, "
                      "static_asserty przechodza" % std)

        zla = os.path.join(tmp, "zla-kolejnosc.cpp")
        with open(zla, "w", encoding="utf-8") as f:
            f.write(KOLEJNOSC)
        w = kompiluj(zla, api, shim, "gnu++17")
        if w.returncode == 0:
            bledy.append("F: kolejnosc include z 213 SKOMPILOWALA SIE - brama nie widzi "
                         "prawdziwego API")
        else:
            print("PASS api216 F kontrola negatywna: kolejnosc include z 213 odrzucona "
                  "(%s)" % pierwszy_blad(w)[:120])
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)

    for b in bledy:
        print("FAIL api216 " + b)
    print("%s api216 sprawdzen=%d bledow=%d"
          % ("FAIL" if bledy else "PASS", 1 + 2 + 2 + 1, len(bledy)))
    return 1 if bledy else 0


if __name__ == "__main__":
    sys.exit(main())
