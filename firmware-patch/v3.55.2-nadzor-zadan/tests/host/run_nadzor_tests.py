#!/usr/bin/env python3
# ============================================================================
# [v3.55.2] Uruchamianie testu hostowego nadzoru zadan sieciowych.
#
# Skrypt wycina z ../../main_centrala.cpp DOKLADNIE fragment z nadzorDecyzja()
# (stale + enum + struct + czysta funkcja decyzyjna) i wstawia go w miejsce
# znacznika // @@SLICE@@ w NadzorHostTest.cpp. Reszta to zwykly g++ — bez
# Arduino, bez ESP32, bez plytki.
#
# Uzycie (z katalogu tests/host):
#   python3 run_nadzor_tests.py
#
# Wynik: "WYNIK KONCOWY: OK" + kod wyjscia 0.
# ============================================================================
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.abspath(os.path.join(HERE, "..", "..", "main_centrala.cpp"))
TEST_SRC = os.path.join(HERE, "NadzorHostTest.cpp")

START = "static const uint32_t NADZOR_PROG_MS"
END = "static const char *nadzorKtoNazwa("


def main():
    if not os.path.isfile(FIRMWARE):
        sys.exit("BLAD: brak pliku %s" % FIRMWARE)
    with open(FIRMWARE, encoding="utf-8") as f:
        firmware = f.read()

    a = firmware.find(START)
    if a < 0:
        sys.exit("BLAD: nie znaleziono poczatku bloku nadzoru w main_centrala.cpp")
    b = firmware.find(END, a)
    if b < 0:
        sys.exit("BLAD: nie znaleziono konca bloku nadzoru w main_centrala.cpp")

    blok = firmware[a:b]
    # Zabezpieczenie: w wycietym fragmencie nie moze byc I/O ani ESP.
    for zakaz in ("millis(", "config.", "ESP.", "zapiszLog", "delay("):
        if zakaz in blok:
            sys.exit("BLAD: wyciety blok nadzoru zawiera '%s' — test nie bylby czysty." % zakaz)

    with open(TEST_SRC, encoding="utf-8") as f:
        test = f.read()
    if "// SLICE_NADZOR" not in test:
        sys.exit("BLAD: w %s brak znacznika // SLICE_NADZOR" % TEST_SRC)
    zrodlo = test.replace("// SLICE_NADZOR", blok, 1)

    tmp = tempfile.mkdtemp(prefix="nadzor_")
    cpp = os.path.join(tmp, "NadzorHostTest_gen.cpp")
    exe = os.path.join(tmp, "nadzor_test")
    with open(cpp, "w", encoding="utf-8") as f:
        f.write(zrodlo)

    kompilator = os.environ.get("CXX", "g++")
    cmd = [kompilator, "-std=c++11", "-O1", "-Wall", "-Wextra", "-o", exe, cpp]
    print("Kompilacja: %s" % " ".join(cmd))
    wynik = subprocess.run(cmd, capture_output=True, text=True)
    if wynik.stdout:
        print(wynik.stdout)
    if wynik.returncode != 0:
        print(wynik.stderr)
        sys.exit("BLAD: kompilacja nie powiodla sie")

    print("-" * 70)
    uruchom = subprocess.run([exe], capture_output=True, text=True)
    print(uruchom.stdout)
    if uruchom.returncode != 0:
        if uruchom.stderr:
            print(uruchom.stderr)
        sys.exit("BLAD: testy nie przeszly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
