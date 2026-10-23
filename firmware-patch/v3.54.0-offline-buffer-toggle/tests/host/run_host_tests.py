#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_host_tests.py — testy hostowe przelacznika bufora offline (firmware v3.54.0).

Co robi:
 1. Czyta ../../main_centrala.cpp (ten sam plik, ktory idzie na urzadzenie).
 2. Wycina z niego DOKLADNIE te fragmenty, ktore dodala v3.54.0:
      - spoolRuntimePersistArmed / spoolRuntimeArm / spoolRuntimeStop /
        spoolRuntimeRestoreFromNvs          (pelne cialo funkcji),
      - cztery linie JSON spool_rt/spool_pages/spool_spill/spool_replay
        (+ fw/uptime/updatedAt, zeby sprawdzic poprawnosc calego obiektu),
      - galezie ACK spoolrt_wl / spoolrt_wyl.
 3. Kompiluje SpoolRtHostTest.cpp z g++ i uruchamia.
 4. Parsuje wyemitowany JSON prawdziwym parserem Pythona i sprawdza wartosci.
 5. Sprawdza statycznie, ze spoolRuntimeRestoreFromNvs() jest wolane w setup()
    PO g_telemetryRing.init() (inaczej po restarcie ring bylby jeszcze pusty).

Kazde wyciecie ma asercje na liczbe trafien — jesli ktos przestawi kod tak,
ze kotwica zniknie, test KRZYCZY zamiast po cichu testowac cos innego.

Uruchomienie (z katalogu tests/host):
    python3 run_host_tests.py
    python3 run_host_tests.py -v
"""

import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "..", "..", "main_centrala.cpp"))
BUILD = os.path.join(HERE, "build")


def fail(msg):
    print("BLAD: " + msg)
    sys.exit(2)


def read_source():
    if not os.path.exists(SRC):
        fail("nie ma pliku %s" % SRC)
    with open(SRC, encoding="utf-8") as f:
        return f.read()


def cut(text, start_marker, end_marker, what, include_end=True):
    """Wycina [start_marker, end_marker] i pilnuje, zeby kotwica byla JEDNA."""
    n = text.count(start_marker)
    if n != 1:
        fail("kotwica startowa '%s' wystepuje %d razy (oczekiwano 1) — %s"
             % (start_marker[:60], n, what))
    i = text.index(start_marker)
    j = text.find(end_marker, i)
    if j < 0:
        fail("kotwica koncowa '%s' nie znaleziona — %s" % (end_marker[:60], what))
    if include_end:
        j += len(end_marker)
    return text[i:j]


def main():
    verbose = "-v" in sys.argv
    src = read_source()
    os.makedirs(BUILD, exist_ok=True)

    # ── 1. wersja
    m = re.search(r'static const char\*\s+FIRMWARE_VERSION\s*=\s*"([^"]+)"\s*;', src)
    if not m:
        fail("nie znaleziono stalej FIRMWARE_VERSION")
    wersja = m.group(1)
    if wersja != "v3.54.0":
        fail("FIRMWARE_VERSION = '%s', a paczka jest v3.54.0" % wersja)
    print("  wersja w pliku: %s" % wersja)

    # ── 2. NVS + ARM/STOP + RESTORE
    region_nvs = cut(
        src,
        "// [v3.54.0] Jeden bajt do NVS.",
        "static bool spoolRuntimeMaybeSpill() {",
        "funkcje NVS/ARM/STOP/restore",
        include_end=False,
    )
    for wymagane in ("spoolRuntimePersistArmed", "spoolRuntimeArm",
                     "spoolRuntimeStop", "spoolRuntimeRestoreFromNvs"):
        if wymagane not in region_nvs:
            fail("w wycietym obszarze brak %s" % wymagane)

    # ── 3. JSON statusu
    region_json = cut(
        src,
        "  // [v3.54.0] Bufor offline — stan przelacznika (NVS) + liczniki spoola.",
        'ja("\\"updatedAt\\":" + String(millis()) + "}");',
        "pola JSON statusu",
    )
    for pole in ("spool_rt", "spool_pages", "spool_spill", "spool_replay"):
        if region_json.count('\\"%s\\"' % pole) != 1:
            fail("pole %s nie wystepuje dokladnie raz w JSON" % pole)

    # ── 4. ACK komend
    ack_start = '  else if (cmd == "spoolrt_wl") {'
    region_ack = cut(src, ack_start, "  else {\n    wynikOk = false;\n    wynikBlad = String(\"nieznana komenda: \")",
                     "galezie ACK spoolrt_wl/wyl", include_end=False)
    if 'cmd == "spoolrt_wyl"' not in region_ack:
        fail("w wycietym obszarze brak galezi spoolrt_wyl")

    # ── 5. statyczna kontrola miejsca wywolania restore w setup()
    if src.count("spoolRuntimeRestoreFromNvs();") != 1:
        fail("spoolRuntimeRestoreFromNvs() musi byc wolane dokladnie raz")
    i_init = src.find("g_telemetryRing.init()")
    i_call = src.find("spoolRuntimeRestoreFromNvs();")
    if i_init < 0 or i_call < i_init:
        fail("restore jest PRZED g_telemetryRing.init() — po restarcie ring bylby pusty")

    # ── 6. skladanie pliku z wycinkami
    inc = []
    inc.append("// PLIK GENEROWANY przez run_host_tests.py — nie edytuj.\n")
    inc.append("// Zrodlo: main_centrala.cpp (firmware %s)\n\n" % wersja)
    inc.append('static constexpr char SPOOL_G_NVS_NAMESPACE[] = "spoolg";\n')
    inc.append('static constexpr char SPOOL_RUNTIME_NVS_KEY_ARMED[] = "rt_on";\n')
    inc.append('static const char* FIRMWARE_VERSION = "%s";\n\n' % wersja)
    inc.append(region_nvs)
    inc.append("\n\n// --- JSON statusu (wyciete z zbudujJsonStatusPelny) ---\n")
    inc.append("static String buildJsonSpoolFragment() {\n")
    inc.append("  String buf;\n")
    inc.append("  auto ja = [&](const String& s) { buf += s; };\n")
    inc.append('  buf += "{";\n')
    inc.append(region_json)
    inc.append("\n  return buf;\n}\n\n")
    inc.append("// --- ACK komend (wyciete z wykonajKomendePanelu) ---\n")
    inc.append("static bool cmdSpoolRt(const String& cmd, bool& wynikOk, String& wynikBlad) {\n")
    inc.append("  wynikOk = true;\n")
    inc.append('  wynikBlad = "";\n')
    inc.append('  if (cmd == "pompa_wl") { /* placeholder, zeby lancuch else-if byl legalny */ }\n')
    inc.append(region_ack)
    inc.append("\n  return wynikOk;\n}\n")

    inc_path = os.path.join(BUILD, "extract.inc")
    with open(inc_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("".join(inc))

    # ── 7. kompilacja
    exe = os.path.join(BUILD, "spoolrt-test")
    cmd = ["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
           "-I" + HERE, "-I" + BUILD,
           os.path.join(HERE, "SpoolRtHostTest.cpp"), "-o", exe]
    cp = subprocess.run(cmd, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stdout)
        print(cp.stderr)
        fail("kompilacja nie przeszla")
    print("  kompilacja: OK (g++, -Wall -Wextra -Werror)")

    # ── 8. uruchomienie
    run = subprocess.run([exe] + (["-v"] if verbose else []),
                         capture_output=True, text=True)
    out = run.stdout
    print(out)

    # ── 9. parser JSON
    blobs = [ln[len("#JSON#"):] for ln in out.splitlines() if ln.startswith("#JSON#")]
    if len(blobs) != 1:
        fail("program nie wypisal dokladnie jednego JSON-a (jest %d)" % len(blobs))
    try:
        obj = json.loads(blobs[0])
    except json.JSONDecodeError as e:
        print("JSON: " + blobs[0])
        fail("JSON statusu sie NIE parsuje: %s" % e)
    oczekiwane = {"spool_rt": 1, "spool_pages": 12345,
                  "spool_spill": 678, "spool_replay": 9,
                  "fw": "v3.54.0", "uptime": 777}
    for k, v in oczekiwane.items():
        if obj.get(k) != v:
            fail("JSON: %s = %r, oczekiwano %r" % (k, obj.get(k), v))
    print("  JSON: parsuje sie, %d pol, wartosci zgodne" % len(obj))

    if run.returncode != 0:
        fail("testy nie przeszly (patrz FAIL wyzej)")
    print("\nWYNIK KONCOWY: OK")


if __name__ == "__main__":
    main()
