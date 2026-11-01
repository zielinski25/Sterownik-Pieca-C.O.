#!/usr/bin/env bash
# Symulator logiki solarnej — kompiluje PRAWDZIWY kod z android/ i uruchamia testy.
#
# Wymaga: JVM (java) + kompilator Kotlin w postaci kotlin-compiler.jar.
# Na maszynie, na ktorej to powstawalo:
#   pip install --break-system-packages jdk4py      # daje JRE
#   npm pack kotlin-compiler@2.4.21                 # daje lib/kotlin-compiler.jar
#
# Uzycie:  ./zbuduj-i-uruchom.sh /sciezka/do/lib/kotlin-compiler.jar
set -euo pipefail
KC="${1:?podaj sciezke do kotlin-compiler.jar}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/android/app/src/main/java/com/sterownikco/pro/core"
OUT="$(mktemp -d)"

# Kluczowe: pliki zrodlowe NIE sa kopiowane do repo — bierzemy je z android/,
# zeby symulator zawsze testowal to, co jest aktualnie w projekcie.
java -cp "$KC" org.jetbrains.kotlin.cli.jvm.K2JVMCompiler \
  "$SRC/SolarAnalytics.kt" "$SRC/SolarThermalModel.kt" \
  "$ROOT"/narzedzia/symulator-solar-2026-10-29/stub/*.kt \
  "$ROOT"/narzedzia/symulator-solar-2026-10-29/harness/Main.kt \
  -include-runtime -d "$OUT/sim.jar" -nowarn

java -Duser.timezone=Europe/Warsaw -Dstdout.encoding=UTF-8 -jar "$OUT/sim.jar"
rm -rf "$OUT"
