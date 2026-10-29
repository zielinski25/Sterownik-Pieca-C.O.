#!/usr/bin/env bash
# Buduje KOMPLETNY projekt Android Studio z tego repo (folder android/ 1:1).
#
# Po co: użytkownik pracuje na różnych komputerach. Paczka z samymi plikami .kt
# wymaga posiadania projektu — komplet nie wymaga niczego.
#
# Wynik: /tmp/pakiet/SterownikCO-Android-v0.31.43-PELNY-PROJEKT.zip
#
# Od 0.31.36 skrypt NIE podmienia plików ze starych katalogów android-sync/:
# bierze android/ dokładnie takie, jakie jest w repo (numer wersji też jest już
# podbity w app/build.gradle.kts). Wcześniej podmiana powodowała, że paczka
# zawierała starszy kod niż repo — dokładnie ten błąd ujawnił się przy 0.31.35.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT=/tmp/pakiet
NAME=SterownikCO-Android-v0.31.43
want="${NAME##*-v}"   # 0.31.43 - jedno zrodlo prawdy: nazwa paczki

rm -rf "$OUT"; mkdir -p "$OUT"
cp -r "$REPO/android" "$OUT/$NAME"
cd "$OUT/$NAME"

# Śmieci lokalne — nigdy do paczki.
rm -rf .gradle build app/build local.properties .idea .kotlin

# Kontrola zgodności numeru wersji z nazwą paczki.
ver=$(grep -o 'versionName = "[^"]*"' app/build.gradle.kts | head -1 | sed 's/.*"\(.*\)"/\1/')
code=$(grep -o 'versionCode = [0-9]*' app/build.gradle.kts | head -1 | sed 's/[^0-9]//g')
echo "wersja w paczce: $ver (versionCode $code)"
[ "$ver" = "$want" ] || { echo "BLAD: versionName=$ver, oczekiwano $want"; exit 1; }

cp "$REPO/narzedzia/wykresy225/CZYTAJ-NAJPIERW-pakiet.txt" CZYTAJ-NAJPIERW.txt
# Skrypt budowania: sam znajduje Jave 17-22 ORAZ Android SDK, zapisuje local.properties.
cp "$REPO/narzedzia/wykresy225/zbuduj.ps1" zbuduj.ps1

mkdir -p ZMIANY-KODU
for d in "$REPO"/android-sync/v0.31.*/ZMIANY-*.diff; do
    [ -f "$d" ] && cp "$d" ZMIANY-KODU/ || true
done
cp "$REPO"/android-sync/v0.31.43-pamiec-historii/README-WGRANIE.txt README-WGRANIE.txt

cd "$OUT"
zip -qr "$OUT/$NAME-PELNY-PROJEKT.zip" "$NAME" -x "*.DS_Store"
echo "gotowe: $OUT/$NAME-PELNY-PROJEKT.zip"
ls -l "$OUT/$NAME-PELNY-PROJEKT.zip"
sha256sum "$OUT/$NAME-PELNY-PROJEKT.zip"
