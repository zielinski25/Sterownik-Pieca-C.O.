#!/usr/bin/env python3
# ============================================================================
# [v3.55.0] Uruchamianie testu hostowego telemetrii.
#
# Skrypt wycina z ../../main_centrala.cpp DOKLADNIE te funkcje, ktore sa w
# firmware (telemetryPendingAbandon, telemetryFinalizeCompletedRequest,
# telemetrySendPendingBatch, telemetryTrySendOneBatch), podmienia tylko
# wywolanie wysylki na atrape i kompiluje calosc zwyklym g++.
#
# Uzycie (z katalogu tests/host):
#   python3 run_tests.py
#
# Wynik: "WYNIK KONCOWY: OK" + kod wyjscia 0.
# ============================================================================
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.abspath(os.path.join(HERE, "..", "..", "main_centrala.cpp"))
TEST_SRC = os.path.join(HERE, "TelemetryBatchHostTest.cpp")

START_ABANDON = "static void telemetryPendingAbandon(const char *why) {"
END_ABANDON = "// [G2 LOCAL] Persistent Spool V2 runtime exercise."
START_SEND = "// [v3.55.0 ONE-BATCH-RESEND] Wysyla to, co lezy w g_telemetryJsonBuffer"
END_SEND = "// Zamiennik petli inline w fbInitialize()"


def slice_between(text, start_marker, end_marker, label):
    start = text.find(start_marker)
    if start < 0:
        sys.exit("BLAD: nie znaleziono poczatku %s w main_centrala.cpp" % label)
    end = text.find(end_marker, start)
    if end < 0:
        sys.exit("BLAD: nie znaleziono konca %s w main_centrala.cpp" % label)
    return text[start:end]


def main():
    if not os.path.isfile(FIRMWARE):
        sys.exit("BLAD: brak pliku %s" % FIRMWARE)
    with open(FIRMWARE, encoding="utf-8") as f:
        firmware = f.read()

    part_a = slice_between(firmware, START_ABANDON, END_ABANDON, "bloku telemetryPendingAbandon/finalize")
    part_b = slice_between(firmware, START_SEND, END_SEND, "bloku send/trySend")

    # Podmiany wylacznie po to, zeby atrapa transportu miala te sama sygnature
    # co firmware. Logika (kolejnosc sprawdzen, poprawki) zostaje nietknieta.
    part_b = part_b.replace(
        'object_t payload(g_telemetryJsonBuffer);',
        'ObjT payload(g_telemetryJsonBuffer);',
    )
    part_b = re.sub(
        r"fbDatabase\.update<object_t>\(fbAsyncClient,\s*\"/piec/telemetry/1m/v1\",\s*payload,\s*g_fbTelemetryResult\);",
        "g_fbDatabase.update(payload, g_fbTelemetryResult, g_telemetryPendingLastSeq);",
        part_b,
    )
    if "object_t" in part_b or "fbDatabase.update<" in part_b:
        sys.exit("BLAD: nie udalo sie podmienic wysylki na atrape — zmienil sie kod w main_centrala.cpp")

    tmpdir = tempfile.mkdtemp(prefix="v3550-telemetry-")
    try:
        shutil.copy(TEST_SRC, os.path.join(tmpdir, "TelemetryBatchHostTest.cpp"))
        with open(os.path.join(tmpdir, "batch_extract.inc"), "w", encoding="utf-8") as f:
            f.write(part_a)
            f.write("\n")
            f.write(part_b)
        binary = os.path.join(tmpdir, "telemetry-batch-test")
        cmd = ["g++", "-std=c++11", "-O1", "-Wall", "-Wextra",
               os.path.join(tmpdir, "TelemetryBatchHostTest.cpp"), "-o", binary]
        build = subprocess.run(cmd, capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout)
            print(build.stderr)
            sys.exit("BLAD KOMPILACJI")
        run = subprocess.run([binary], capture_output=True, text=True)
        print(run.stdout, end="")
        if run.returncode != 0:
            print(run.stderr, end="")
            sys.exit(run.returncode)
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    main()
