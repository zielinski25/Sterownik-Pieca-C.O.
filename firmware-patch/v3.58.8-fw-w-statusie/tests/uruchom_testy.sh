#!/usr/bin/env bash
# ============================================================================
# Testy na PC firmware'u centrali 3.58.8 - opis: README.txt.
#   D. [v3.58.8 FW-W-STATUSIE]: czy /api/status (wifiZbudujJsonStatus()) wystawia
#      kazde pole, ktore panel LCD czyta - w tym nowe "fw". Skrypt panelu
#      LCD-Centrali/testy_host/sprawdz_pola_statusu.py czyta TEN plik centrali
#      i src/diagnostyka_panelu.h panelu (nie atrapy).
#   C, A, B: testy z paczki 3.58.7 (START-TG; w srodku testy 3.58.6: OTA panelu,
#      wykresy) na pliku 3.58.8 - ten kod sie nie zmienil.
#   ./uruchom_testy.sh              zwykle uruchomienie
#   SANITIZER=1 ./uruchom_testy.sh  to samo z AddressSanitizer + UBSan
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: g++ (C++17), python3. Linux albo WSL. Czesci D oraz C/A/B wymagaja
# pelnego repozytorium (katalogi LCD-Centrali/ i firmware-patch/v3.58.7-*).
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
SKRYPT_PANELU="../../../LCD-Centrali/testy_host/sprawdz_pola_statusu.py"
TESTY_3587="${TESTY_3587:-../../v3.58.7-start-telegram/tests}"

echo "=== D. [v3.58.8 FW-W-STATUSIE] pola /api/status a pola czytane przez panel LCD ==="
if [ -f "$SKRYPT_PANELU" ]; then
  python3 "$SKRYPT_PANELU" "$CPP_ABS"
  echo "[D2] dla porownania - ten sam skrypt na pliku 3.58.7 (ma zglosic brak \"fw\"):"
  if python3 "$SKRYPT_PANELU" ../../v3.58.7-start-telegram/src/main_centrala.cpp; then
    echo "    BLAD: skrypt nie wykryl braku \"fw\" w 3.58.7 - sprawdzenie nic nie sprawdza"; exit 1
  fi
  echo "    (zgodnie z oczekiwaniem: 3.58.7 bez \"fw\", kod wyjscia 1)"
else
  echo "    (brak $SKRYPT_PANELU - paczka rozpakowana osobno; czesc D pominieta)"
fi
echo "[D3] linia w wifiZbudujJsonStatus() jest jedna i stoi zaraz po \"rssi\""
python3 - "$CPP_ABS" <<'PY'
import sys, re
t = open(sys.argv[1], encoding="utf-8").read()
i = t.index("String wifiZbudujJsonStatus() {"); j = t.index("\n}\n", i); c = t[i:j]
linie = [l for l in c.split("\n") if '\\"fw\\"' in l]
assert len(linie) == 1, linie
k = c.index(linie[0]); przed = c[:k].rstrip().split("\n")[-1]
assert '\\"rssi\\"' in przed or 'FW-W-STATUSIE' in przed or przed.strip().startswith('//'), przed
print("    OK:", linie[0].strip())
PY

if [ -d "$TESTY_3587" ]; then
  echo
  echo "=== C + A + B. testy z paczek 3.58.7 i 3.58.6 na pliku 3.58.8 ==="
  ( cd "$TESTY_3587" && CPP_PATH="$CPP_ABS" SANITIZER="${SANITIZER:-0}" ./uruchom_testy.sh )
else
  echo "    (katalogu $TESTY_3587 nie ma - testy C/A/B pominiete)"
fi
