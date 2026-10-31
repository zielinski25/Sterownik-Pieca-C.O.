#!/usr/bin/env python3
"""215: brama API - kompilacja naszych naglowkow wobec PRAWDZIWEGO
ESPAsyncWebServer, a nie wobec modelu napisanego na potrzeby testu.

Po co: etapy 211-213 byly sprawdzane modelem serwera (Serwer212.h/Serwer213.h).
Model powstal na moich zalozeniach - i przepuscil blad D1 (kolejnosc include),
ktory na ESP jest bledem kompilacji.

Ta brama bierze PRAWDZIWY naglowek biblioteki (przypiety w repo, SHA-256),
kompiluje go na komputerze (shim Arduino) i kompiluje wobec niego:

  A. sam naglowek biblioteki                       - sensownosc shimu,
  B. nasze trzy naglowki firmware (211/213/214)    - zgodnosc z API,
  C. szkielet z PRAWDZIWYM kodem wyciagnietym z pliku po patchu v3.56.2,
  D. kontrola negatywna: kolejnosc include z etapu 213 MUSI sie NIE kompilowac,
  E. kontrola negatywna: podmieniony podpis _fillBuffer MUSI byc wykryty.

D i E to dowod, ze brama nie jest ozdoba.

Kazda kompilacja: gnu++11 (tak kompiluje ESP32 Arduino), -fno-exceptions,
-fno-rtti, -Werror na NASZYCH naglowkach (biblioteka idzie przez -isystem).

Uruchomienie:
    python3 -B narzedzia/brama-api-esp215/kompiluj.py
"""
import os
import shutil
import subprocess
import sys
import tempfile

import przygotuj_api as API

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
N = os.path.join(REPO, "narzedzia")
FW = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
ZRODLO = os.path.join(FW, "main_centrala.cpp")

D211 = os.path.join(N, "wysylka-zapinka211")
D213 = os.path.join(N, "panel-pobrania213")
D214 = os.path.join(N, "brama-przed-wgraniem214")
PATCH_KUM = os.path.join(D214, "main_centrala-v3.55.8-to-v3.56.2.patch")

# Dwa standardy, bo rozne wersje rdzenia ESP32 Arduino kompiluja roznie:
# 2.x domyslnie gnu++11, 3.x gnu++17. Kod musi dzialac w obu.
STANDARDY = ["gnu++11", "gnu++17"]
BAZA = ["-DARDUINO=10607", "-DHOST", "-fno-exceptions", "-fno-rtti", "-Wall", "-Wextra"]

SZKIELET = r'''// [215] jednostka kompilacji: PRAWDZIWE API + nasze naglowki + kod z pliku.
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
static AsyncWebServer wifiWebServer(80);

// --- PRAWDZIWY kod wyciagniety z pliku po patchu v3.56.2 -----------------
#include "Extracted215.inc"

// --- co musi byc prawda o typach (sprawdza kompilator, nie ja) -----------
static_assert(std::is_base_of<AsyncAbstractResponse, historia212::Odpowiedz>::value,
              "Odpowiedz musi dziedziczyc po PRAWDZIWYM AsyncAbstractResponse");
static_assert(std::is_same<decltype(&historia212::Odpowiedz::_fillBuffer),
                           size_t (historia212::Odpowiedz::*)(uint8_t *, size_t)>::value,
              "_fillBuffer musi miec podpis z prawdziwej biblioteki");
static_assert(std::is_same<decltype(&historia212::Odpowiedz::_sourceValid),
                           bool (historia212::Odpowiedz::*)() const>::value,
              "_sourceValid musi byc const, jak w bibliotece");

int main() { return 0; }
'''

NEGATYWNA_KOLEJNOSC = r'''// [215 D] kolejnosc include DOKLADNIE taka, jaka mialy etapy 212 i 213.
#include "HistoriaPin.h"
#include "HistoriaPobranie.h"
#include "HistoriaBilety.h"
#include <ESPAsyncWebServer.h>
int main() { return 0; }
'''

ZATRUTE_FILLBUFFER = r'''// [215 E] celowo zepsuty podpis _fillBuffer - brama musi to zauwazyc.
#include <ESPAsyncWebServer.h>
#include "HistoriaPin.h"
namespace historia212 {
class Zatruta : public AsyncAbstractResponse {
 public:
  Zatruta() : AsyncAbstractResponse(nullptr) {}
  bool _sourceValid() const override { return true; }
  size_t _fillBuffer(uint8_t *buf, unsigned maxLen) override { (void)buf; (void)maxLen; return 0; }
};
}  // namespace historia212
int main() { return 0; }
'''


def uruchom(cmd, timeout=600):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def kompiluj(zrodlo, api, shim, werror=True, standard="gnu++11"):
    cmd = ["c++", "-fsyntax-only", "-std=" + standard, *BAZA]
    if werror:
        cmd.append("-Werror")
    cmd += ["-Wno-unused-parameter", "-isystem", api, "-I" + shim,
            "-I" + D211, "-I" + D213, "-I" + FW, zrodlo]
    return uruchom(cmd)


def pierwszy_blad(wynik):
    for linia in wynik.stderr.split("\n"):
        if "error:" in linia:
            return linia.strip()[:220]
    return wynik.stderr.strip().split("\n")[0][:220] if wynik.stderr else ""


def _blok(tekst, od, do):
    i = tekst.find(od)
    if i < 0:
        raise SystemExit("215: brak fragmentu w pliku po patchu: %r" % od[:60])
    j = tekst.find(do, i)
    if j < 0:
        raise SystemExit("215: brak konca fragmentu: %r" % do[:60])
    return tekst[i:j + len(do)]


def wyciagnij_kawalki(tekst):
    """Z pliku po patchu bierze DOKLADNIE fragmenty dodane przez 212-214."""
    czesci = ["// sekcja tokenu (212)\n",
              _blok(tekst, "#ifndef SECRET_HISTORIA_TOKEN",
                    'static const char* HISTORIA_TOKEN = SECRET_HISTORIA_TOKEN;\n'),
              "\n\n// adaptery losowosci i zegara (213)\n",
              _blok(tekst, "static void historiaLosuj213(",
                    "static std::uint32_t historiaZegar213() "
                    "{ return static_cast<std::uint32_t>(millis()); }"),
              "\n\n// bramka trasy (212)\n",
              _blok(tekst, "struct HistoriaBramka212 : historia212::Bramka {", "};\n"),
              "\n// obiekty trasy i biletow (212/213)\n",
              _blok(tekst, "static HistoriaBramka212 historiaBramka;",
                    "static historia212::Pobranie historiaPobranie(historiaBramka, HISTORIA_TOKEN);\n"),
              "\n",
              _blok(tekst, "static historia213::Bilety historiaBilety(",
                    "static historia213::Bilety historiaBilety(historiaLosuj213, historiaZegar213);\n"),
              "\n",
              _blok(tekst, "struct HistoriaBiletyPodpiecie213 {",
                    "static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;\n"),
              "\n// rejestracje tras (212/213) - w pliku leza w ciele funkcji,\n"
              "// wiec tutaj tez musza byc w funkcji, inaczej to nie ten sam kod.\n",
              "static void rejestracja215() {\n"]
    for sciezka in ('"/history"', '"/history/long"', '"/history/link"'):
        i = tekst.find("wifiWebServer.on(" + sciezka)
        if i < 0:
            raise SystemExit("215: brak rejestracji %s" % sciezka)
        j = tekst.find("  });\n", i)
        czesci.append(tekst[i:j + len("  });\n")])
    czesci.append("}\n")
    return "".join(czesci)


def zbuduj_plik_po_patchu(katalog):
    shutil.copyfile(ZRODLO, os.path.join(katalog, "main_centrala.cpp"))
    shutil.copyfile(os.path.join(FW, "secrets.h.example"),
                    os.path.join(katalog, "secrets.h.example"))
    t = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-i", PATCH_KUM],
                       cwd=katalog, capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("215: patch v3.56.2 sie nie nalozyl: %s%s" % (t.stdout, t.stderr))
    with open(os.path.join(katalog, "main_centrala.cpp"), "r", encoding="utf-8", newline="") as f:
        return f.read()


def main():
    tmp = tempfile.mkdtemp(prefix="api215-")
    wyniki = []
    try:
        info = API.przygotuj(tmp)
        api = os.path.join(tmp, "api")
        shim = os.path.join(tmp, "shim")
        print("PASS api215 przygotowano %d plikow API i %d shimow"
              % (len(info["api"]), len(info["shim"])))

        # --- A ------------------------------------------------------
        a = os.path.join(tmp, "a.cpp")
        with open(a, "w", encoding="utf-8") as f:
            f.write('#include <ESPAsyncWebServer.h>\nint main() { return 0; }\n')
        t = kompiluj(a, api, shim, werror=False)
        wyniki.append({"id": "A", "opis": "prawdziwy ESPAsyncWebServer.h kompiluje sie na komputerze",
                       "ok": t.returncode == 0,
                       "szczegoly": "czysto" if t.returncode == 0 else pierwszy_blad(t)})
        print("%s A  %s" % ("PASS" if t.returncode == 0 else "FAIL", wyniki[-1]["szczegoly"]))

        # --- B ------------------------------------------------------
        b = os.path.join(tmp, "b.cpp")
        with open(b, "w", encoding="utf-8") as f:
            f.write('#include <ESPAsyncWebServer.h>\n#include "HistoriaPin.h"\n'
                    '#include "HistoriaPobranie.h"\n#include "HistoriaBilety.h"\n'
                    'int main() { return 0; }\n')
        for std in STANDARDY:
            t = kompiluj(b, api, shim, standard=std)
            wyniki.append({"id": "B-" + std, "opis": "3 naglowki firmware wobec prawdziwego API (%s)" % std,
                           "ok": t.returncode == 0,
                           "szczegoly": "czysto" if t.returncode == 0 else pierwszy_blad(t)})
            print("%s %-10s %s" % ("PASS" if t.returncode == 0 else "FAIL", "B-" + std,
                                   wyniki[-1]["szczegoly"]))

        # --- C ------------------------------------------------------
        katalog = os.path.join(tmp, "plik")
        os.makedirs(katalog, exist_ok=True)
        tekst = zbuduj_plik_po_patchu(katalog)
        kawalki = wyciagnij_kawalki(tekst)
        c = os.path.join(tmp, "c.cpp")
        with open(os.path.join(tmp, "Extracted215.inc"), "w", encoding="utf-8") as f:
            f.write(kawalki)
        with open(c, "w", encoding="utf-8") as f:
            f.write(SZKIELET)
        for std in STANDARDY:
            t = kompiluj(c, api, shim, standard=std)
            wyniki.append({"id": "C-" + std,
                           "opis": "szkielet z PRAWDZIWYM kodem z pliku po patchu v3.56.2 (%s)" % std,
                           "ok": t.returncode == 0,
                           "szczegoly": "czysto (%d linii wyciagnietych)" % len(kawalki.split("\n"))
                           if t.returncode == 0 else pierwszy_blad(t)})
            print("%s %-10s %s" % ("PASS" if t.returncode == 0 else "FAIL", "C-" + std,
                                   wyniki[-1]["szczegoly"]))
        with open(os.path.join(TU, "Extracted215.inc"), "w", encoding="utf-8") as f:
            f.write("// WYGENEROWANE przez narzedzia/brama-api-esp215/kompiluj.py - nie edytowac.\n"
                    "// Dokladny kod dodany przez etapy 212-214, wyciagniety z pliku po patchu.\n\n"
                    + kawalki)

        # --- D (kontrola negatywna) ----------------------------------
        d = os.path.join(tmp, "d.cpp")
        with open(d, "w", encoding="utf-8") as f:
            f.write(NEGATYWNA_KOLEJNOSC)
        t = kompiluj(d, api, shim)
        ok = t.returncode != 0 and any(s in t.stderr for s in
                                       ("does not have any field named 'AsyncAbstractResponse'",
                                        "expected class-name",
                                        "has not been declared"))
        wyniki.append({"id": "D", "opis": "kolejnosc include z etapu 213 NIE kompiluje sie",
                       "ok": ok, "szczegoly": pierwszy_blad(t) if t.returncode else "KOMPILUJE SIE!"})
        print("%s D  %s" % ("PASS" if ok else "FAIL", wyniki[-1]["szczegoly"]))

        # --- E (kontrola negatywna) ----------------------------------
        e = os.path.join(tmp, "e.cpp")
        with open(e, "w", encoding="utf-8") as f:
            f.write(ZATRUTE_FILLBUFFER)
        t = kompiluj(e, api, shim)
        ok = t.returncode != 0 and ("does not override" in t.stderr or "error:" in t.stderr)
        wyniki.append({"id": "E", "opis": "zly podpis _fillBuffer jest wykryty",
                       "ok": ok, "szczegoly": pierwszy_blad(t) if t.returncode else "KOMPILUJE SIE!"})
        print("%s E  %s" % ("PASS" if ok else "FAIL", wyniki[-1]["szczegoly"]))

        if not all(w["ok"] for w in wyniki):
            print("\nFAIL api215 sprawdzen=%d bledow=%d"
                  % (len(wyniki), sum(1 for w in wyniki if not w["ok"])))
            return 1
        print("\nPASS api215 sprawdzen=%d bledow=0" % len(wyniki))
        return 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
