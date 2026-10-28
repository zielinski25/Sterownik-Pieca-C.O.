#!/usr/bin/env bash
# Buduje KOMPLETNY projekt Android Studio w wersji 0.31.35 z tego repo.
#
# Po co: użytkownik pracuje na różnych komputerach. Paczka z samymi 5 plikami .kt
# wymaga posiadania projektu — komplet nie wymaga niczego.
#
# Wynik: /tmp/pakiet/SterownikCO-Android-v0.31.35-PELNY-PROJEKT.zip
# (folder android/ z repo + podmienione 5 plików + podbity versionCode).
#
# Nie jest commitowany do repo: to artefakt generowany (~1,1 MB).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$REPO/android-sync/v0.31.35-solar-z-chmury"
OUT=/tmp/pakiet
NAME=SterownikCO-Android-v0.31.35

rm -rf "$OUT"; mkdir -p "$OUT"
cp -r "$REPO/android" "$OUT/$NAME"
cd "$OUT/$NAME"

# Śmieci lokalne — nigdy do paczki.
rm -rf .gradle build app/build local.properties .idea .kotlin

# Pięć plików zmienionych względem 0.31.33.
A=app/src/main/java/com/sterownikco/pro
cp "$SRC/AppModel.kt"       "$A/core/AppModel.kt"
cp "$SRC/RtdbClient.kt"     "$A/core/RtdbClient.kt"
cp "$SRC/SolarAnalytics.kt" "$A/core/SolarAnalytics.kt"
cp "$SRC/ChartEngine.kt"    "$A/ui/chart/ChartEngine.kt"
cp "$SRC/Sensors.kt"        "$A/ui/screens/sheets/Sensors.kt"

# Numery wersji. versionCode 31035 jest celowo wysoki: musi być wyższy od tego,
# co użytkownik ma już w telefonie, inaczej Android odmówi instalacji.
python3 - <<'PY'
p = "app/build.gradle.kts"
s = open(p, encoding="utf-8").read()
s = s.replace("versionCode = 3017", "versionCode = 31035")
s = s.replace('versionName = "v0.31.11"', 'versionName = "0.31.35"')
open(p, "w", encoding="utf-8").write(s)
PY

cp "$REPO/narzedzia/wykresy225/CZYTAJ-NAJPIERW-pakiet.txt" CZYTAJ-NAJPIERW.txt
# Skrypt budowania: chroni przed Java 25 z Android Studio (blad "25.0.3").
cp "$REPO/narzedzia/wykresy225/zbuduj.ps1" zbuduj.ps1

mkdir -p ZMIANY-KODU
cp "$REPO/android-sync/v0.31.34-dziury-i-pompa/ZMIANY-0.31.33-do-0.31.34.diff" ZMIANY-KODU/ || true
cp "$REPO/android-sync/v0.31.35-solar-z-chmury/ZMIANY-0.31.34-do-0.31.35.diff" ZMIANY-KODU/ || true

cd "$OUT"
zip -q -r -9 "$NAME-PELNY-PROJEKT.zip" "$NAME"
echo "gotowe: $OUT/$NAME-PELNY-PROJEKT.zip"
sha256sum "$OUT/$NAME-PELNY-PROJEKT.zip"
