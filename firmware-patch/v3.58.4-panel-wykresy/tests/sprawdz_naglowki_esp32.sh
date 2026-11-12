#!/usr/bin/env bash
# ============================================================================
# Kod [v3.58.4 PANEL-WYKRESY] centrali sprawdzony PRAWDZIWYMI naglowkami ESP32
# ============================================================================
# Po co: testy z uruchom_testy.sh kompiluja wyciety kod zwyklym g++ z atrapami
# (String, ps_malloc, spinlock, DLOGF). Atrapa nie wykryje pomylki w nazwie
# albo sygnaturze funkcji z arduino-esp32. Ten skrypt kompiluje ten sam
# wycinek w trybie "tylko sprawdz" (-fsyntax-only) z prawdziwym Arduino.h
# (String, ps_malloc, portENTER_CRITICAL, localtime_r, lroundf) i z
# deklaracjami logu (DLOGF, DiagCategory) wycietymi doslownie z pliku centrali.
#
# Korzysta ze skryptu panelu LCD z tego samego repozytorium
# (LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh): ten sam kompilator
# (clang z pakietu Pythona "ziglang") i te same pobrane naglowki.
#
# CZEGO TO NIE ZASTEPUJE:
#   - To nie jest build PlatformIO i nie jest to CALY plik centrali. Plik ma
#     ok. 49 tys. linii i wymaga bibliotek oraz plikow .h projektu, ktorych w
#     repozytorium nie ma. Sprawdzany jest tylko nowy blok kodu.
#   - Rejestracja trasy HTTP (wifiWebServer.on(...)) nie jest sprawdzana - uzywa
#     tych samych wywolan ESPAsyncWebServer co sasiednie trasy /api/history*.
#   - Naglowki sa dla plytki "esp32" (centrala to ESP32-S3). Pliki rdzenia
#     Arduino, z ktorych korzysta ten kod, sa dla obu plytek te same.
#
#   ./sprawdz_naglowki_esp32.sh                rdzen arduino-esp32 3.0.7
#   RDZEN=3.3.8 ./sprawdz_naglowki_esp32.sh    rdzen 3.3.8
# Wymaga: python3 z pakietem "ziglang", git; pierwsze uruchomienie pobiera
# naglowki (ok. 250 MB) do katalogu podrecznego skryptu panelu.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
PANEL="../../../LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh"
if [ ! -f "$PANEL" ]; then
  echo "Brak $PANEL - ten skrypt dziala tylko w pelnym repozytorium (paczka centrali rozpakowana osobno go nie ma)."
  exit 2
fi
ROB="${TMPDIR:-/tmp}/centrala_panel_hist_test/naglowki"
mkdir -p "$ROB"
echo "[A] wycinam kod z $CPP"
python3 wytnij_blok.py "$CPP" --esp32 "$ROB/panel_hist_esp32.cpp" | sed 's/^/    /'
echo "[B] sprawdzanie prawdziwymi naglowkami arduino-esp32 (skrypt panelu LCD)"
SRC="$ROB" PLIKI="panel_hist_esp32.cpp" "$PANEL"
