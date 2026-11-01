#!/usr/bin/env python3
# ============================================================================
# [v3.55.1] Uruchamianie testu hostowego wczytywania historii z LittleFS.
#
# Skrypt wycina z ../../main_centrala.cpp DOKLADNIE klase HistoriaStrumien
# oraz funkcje historiaWczytajZFS()/historiaDlugaWczytajZFS() i wstawia je
# w miejsce znacznika // @@SLICE@@ w HistoryStreamHostTest.cpp. Reszta
# (File, LittleFS, String, zegar, WDT) to atrapy — kompiluje sie zwyklym g++,
# bez Arduino i bez ESP32.
#
# Uzycie (z katalogu tests/host):
#   python3 run_history_tests.py
#
# Wynik: "WYNIK KONCOWY: OK" + kod wyjscia 0.
# ============================================================================
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.abspath(os.path.join(HERE, "..", "..", "main_centrala.cpp"))
TEST_SRC = os.path.join(HERE, "HistoryStreamHostTest.cpp")

START = "#define HISTORIA_STRUMIEN_PORCJA_BAJTOW     4096U"
END = "// [v3.55.1 FS-BLANK-FORMAT] Czy partycja LittleFS"


def main():
    if not os.path.isfile(FIRMWARE):
        sys.exit("BLAD: brak pliku %s" % FIRMWARE)
    with open(FIRMWARE, encoding="utf-8") as f:
        firmware = f.read()

    a = firmware.find(START)
    if a < 0:
        sys.exit("BLAD: nie znaleziono poczatku bloku HistoriaStrumien w main_centrala.cpp")
    b = firmware.find(END, a)
    if b < 0:
        sys.exit("BLAD: nie znaleziono konca bloku HistoriaStrumien w main_centrala.cpp")
    sliced = firmware[a:b].rstrip()

    for wymagane in ("class HistoriaStrumien", "void historiaWczytajZFS()",
                     "void historiaDlugaWczytajZFS()", "vTaskDelay(1)"):
        if wymagane not in sliced:
            sys.exit("BLAD: w wycietym bloku brakuje: %s" % wymagane)

    with open(TEST_SRC, encoding="utf-8") as f:
        test = f.read()
    if "// @@SLICE@@" not in test:
        sys.exit("BLAD: brak znacznika // @@SLICE@@ w tescie")

    gotowy = test.replace("// @@SLICE@@", sliced)

    tmpdir = tempfile.mkdtemp(prefix="hist55_")
    cpp = os.path.join(tmpdir, "test.cpp")
    exe = os.path.join(tmpdir, "test.exe" if os.name == "nt" else "test")
    with open(cpp, "w", encoding="utf-8") as f:
        f.write(gotowy)

    kompilator = os.environ.get("CXX", "g++")
    cmd = [kompilator, "-std=c++11", "-O1", "-Wall", "-Wextra",
           "-Wno-unused-parameter", "-Wno-unused-variable", "-Wno-unused-but-set-variable",
           cpp, "-o", exe]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout)
        print(r.stderr)
        sys.exit("BLAD: kompilacja nie powiodla sie")

    r = subprocess.run([exe], capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if r.stderr.strip():
        sys.stderr.write(r.stderr)
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()
