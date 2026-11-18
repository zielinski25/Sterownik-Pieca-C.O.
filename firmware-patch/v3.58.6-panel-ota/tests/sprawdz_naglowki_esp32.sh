#!/usr/bin/env bash
# ============================================================================
# Kod centrali 3.58.6 dodany dla panelu LCD, sprawdzony PRAWDZIWYMI naglowkami ESP32
# ============================================================================
#   A. [v3.58.6 PANEL-OTA] - decyzje, stan i kod klejacy posrednictwa aktualizacji panelu,
#   B. [v3.58.4 PANEL-WYKRESY] - wykresy dla panelu (bez zmian wzgledem 3.58.5).
#
# Po co: testy z uruchom_testy.sh kompiluja wyciety kod zwyklym g++ z atrapami
# (String, Preferences, zapytanie HTTP, spinlock...). Atrapa nie wykryje pomylki
# w nazwie albo sygnaturze funkcji z arduino-esp32 czy ESPAsyncWebServer. Ten
# skrypt kompiluje te same wycinki w trybie "tylko sprawdz" (-fsyntax-only) z
# prawdziwymi naglowkami: Arduino.h (String, portENTER_CRITICAL), Preferences.h,
# LittleFS.h oraz - dla czesci A - ESPAsyncWebServer 3.10.3 i AsyncTCP 3.4.10
# (wersje z platformio.ini centrali; pobierane z GitHuba). Deklaracje otoczenia
# (zmienne alarmow, config, zapiszLog(), sendTelegramMessage(), DLOGF...) sa
# wyciete doslownie z pliku centrali.
#
# Korzysta ze skryptu panelu LCD z tego samego repozytorium
# (LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh): ten sam kompilator
# (clang z pakietu Pythona "ziglang") i te same pobrane naglowki.
#
# CZEGO TO NIE ZASTEPUJE:
#   - To nie jest build PlatformIO i nie jest to CALY plik centrali. Plik ma
#     ok. 50 tys. linii i wymaga bibliotek oraz plikow .h projektu, ktorych w
#     repozytorium nie ma. Sprawdzane sa tylko bloki PANEL-OTA i PANEL-WYKRESY.
#   - W czesci A zamiast history196::BootFilesystem (BootFilesystem.h - pliku
#     nie ma w repozytorium) podstawiony jest sam LittleFS: te same wywolania
#     exists() i request->send(system plikow, sciezka, typ).
#   - pobierzFirmwarePanelaZGithub() nie jest tu kompilowana w calosci
#     (wymaga HTTPClient, LittleFS i limitow miejsca z reszty pliku). Z jej
#     tresci wycinane sa tylko linie [v3.58.6 PANEL-OTA] i wstawiane do
#     funkcji o tych samych parametrach i typach zmiennych lokalnych.
#   - Jednolinijkowe wywolania w trasach HTTP, tgTaskFn() i setup() NIE sa
#     kompilowane w swoim miejscu - sa odtworzone na koncu wycinka czesci A.
#   - Naglowki sa dla plytki "esp32" (centrala to ESP32-S3). Pliki rdzenia
#     Arduino, z ktorych korzysta ten kod, sa dla obu plytek te same.
#
#   ./sprawdz_naglowki_esp32.sh                rdzen arduino-esp32 3.0.7
#   RDZEN=3.3.8 ./sprawdz_naglowki_esp32.sh    rdzen 3.3.8 (ten ma centrala: pioarduino 55.03.38)
#   OSTRZEZENIA=1 ./sprawdz_naglowki_esp32.sh  dodatkowo liczy ostrzezenia clang w sprawdzanym kodzie
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
# Biblioteki serwera WWW centrali - w tym samym katalogu podrecznym co naglowki panelu.
K="${LCD_TESTY_CACHE:-${TMPDIR:-/tmp}/lcd_testy_host}/naglowki_esp32"
mkdir -p "$K"
klon() {   # klon <katalog> <tag> <adres>
  if [ ! -d "$K/$1/.git" ]; then
    echo "    pobieram $3 ($2)"
    git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$2" "$3" "$K/$1"
  fi
}
echo "[A] wycinam kod z $CPP"
python3 wytnij_blok.py "$CPP" --ota-esp32 "$ROB/panel_ota_esp32.cpp" | sed 's/^/    /'
python3 wytnij_blok.py "$CPP" --esp32 "$ROB/panel_hist_esp32.cpp" | sed 's/^/    /'
echo "[B] biblioteki serwera WWW centrali (ESPAsyncWebServer 3.10.3, AsyncTCP 3.4.10)"
klon ESPAsyncWebServer v3.10.3 https://github.com/ESP32Async/ESPAsyncWebServer.git
klon AsyncTCP v3.4.10 https://github.com/ESP32Async/AsyncTCP.git
echo "[C] sprawdzanie prawdziwymi naglowkami (skrypt panelu LCD)"
SRC="$ROB" PLIKI="panel_ota_esp32.cpp panel_hist_esp32.cpp" DODATKOWE_I="-I$K/ESPAsyncWebServer/src -I$K/AsyncTCP/src" "$PANEL"
