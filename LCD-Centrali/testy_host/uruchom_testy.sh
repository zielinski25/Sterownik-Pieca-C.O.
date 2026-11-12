#!/usr/bin/env bash
# ============================================================================
# Testy panelu LCD NA PC (bez plytki ESP32) - opis: README.md w tym katalogu.
# ============================================================================
#   ./uruchom_testy.sh                 wszystko: testy jednostkowe + symulator
#   ./uruchom_testy.sh --szybko        tylko testy jednostkowe (sam g++, 1 s)
#   ./uruchom_testy.sh dwie_centrale   jeden scenariusz symulatora, z logiem
#   SANITIZER=1 ./uruchom_testy.sh     to samo z AddressSanitizer + UBSan (wolniej)
#
# Porownanie ze STARSZA wersja firmware'u (bez oceniania, same obserwacje):
#   mkdir -p /tmp/stary && unzip -q "../../LCD Centrali.zip" -d /tmp/stary   # v1.1.0
#   SRC=/tmp/stary/src OBSERWACJE=1 ./uruchom_testy.sh
#
# Wymaga: g++ (C++17), gcc, ar, git (Linux albo WSL). Za pierwszym razem
# pobiera z GitHuba LVGL 8.3.11 i ArduinoJson 7.2.1 do katalogu podrecznego
# i buduje LVGL; kolejne uruchomienia trwaja kilka sekund.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
TU="$(pwd)"
SRC="${SRC:-$TU/../src}"
INCLUDE="$TU/../include"
PODRECZNY="${LCD_TESTY_CACHE:-${TMPDIR:-/tmp}/lcd_testy_host}"
WYNIKI="${WYNIKI:-$PODRECZNY/wyniki}"
OBSERWACJE="${OBSERWACJE:-0}"
SANITIZER="${SANITIZER:-0}"
ROWNOLEGLE="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
mkdir -p "$PODRECZNY" "$WYNIKI"

echo "[1] testy jednostkowe src/diagnostyka_panelu.h"
# -Wno-format-truncation: test celowo podaje za male bufory (sprawdza obcinanie).
g++ -std=c++17 -O1 -Wall -Wextra -Wno-format-truncation -o "$PODRECZNY/test_diagnostyka" test_diagnostyka.cpp
"$PODRECZNY/test_diagnostyka" | sed 's/^/    /'
if [ "${1:-}" = "--szybko" ]; then exit 0; fi

echo "[2] biblioteki (LVGL 8.3.11, ArduinoJson 7.2.1)"
pobierz() {   # pobierz <katalog> <tag> <adres>
  if [ ! -d "$PODRECZNY/$1/.git" ]; then
    echo "    pobieram $3 ($2)"
    git clone --quiet --depth 1 --branch "$2" "$3" "$PODRECZNY/$1" 2>/dev/null
  fi
}
pobierz lvgl v8.3.11 https://github.com/lvgl/lvgl.git
pobierz ArduinoJson v7.2.1 https://github.com/bblanchon/ArduinoJson.git

# LVGL jest budowany z lv_conf.h TEGO projektu; zmiana lv_conf.h = nowa biblioteka.
ODCISK="$(cksum < "$INCLUDE/lv_conf.h" | tr -d ' ')"
LIB="$PODRECZNY/liblvgl_$ODCISK.a"
if [ ! -f "$LIB" ]; then
  echo "    buduje LVGL (jednorazowo)"
  OBJ="$PODRECZNY/obj_$ODCISK"
  mkdir -p "$OBJ"
  (cd "$PODRECZNY/lvgl" && find src -name '*.c') | while read -r plik; do
    echo "gcc -c -O1 -w -DLV_CONF_INCLUDE_SIMPLE -I$INCLUDE -I$PODRECZNY/lvgl -o $OBJ/$(echo "$plik" | tr '/.' '__').o $PODRECZNY/lvgl/$plik"
  done | xargs -P "$ROWNOLEGLE" -I{} sh -c "{}"
  ar rcs "$LIB" "$OBJ"/*.o
fi

echo "[3] budowanie firmware'u panelu na PC (zrodla: $SRC)"
BUD="$PODRECZNY/bud_$(echo "$SRC $SANITIZER" | cksum | tr -d ' ')"
mkdir -p "$BUD"
SAN=""
if [ "$SANITIZER" = "1" ]; then SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
# Hasla: firmware wymaga src/secrets.h, ktorego nie ma w gicie. Testy nie
# potrzebuja prawdziwych hasel (atrapa WiFi hasla nie sprawdza), wiec gdy
# pliku brak, kompilator znajdzie te atrape. Gdy src/secrets.h istnieje,
# ma pierwszenstwo (szukany jest najpierw obok main.cpp).
ATRAPA_HASEL="$PODRECZNY/atrapa_hasel"
mkdir -p "$ATRAPA_HASEL"
printf '#pragma once\n#define SECRET_WIFI_AP_PASSWORD "haslo-testowe"\n#define SECRET_OTA_HASLO "haslo-ota-testowe"\n' > "$ATRAPA_HASEL/secrets.h"
# Te same makra co build_flags w platformio.ini, ktorych uzywa kod (TFT_BL),
# plus ustawienia ArduinoJson dla atrapy String.
WSPOLNE="-std=gnu++17 -O1 -g $SAN -DTFT_BL=21 -DLV_CONF_INCLUDE_SIMPLE \
  -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 \
  -I $TU/mock -I $INCLUDE -I $PODRECZNY/ArduinoJson/src -I $PODRECZNY/lvgl -I $SRC -I $ATRAPA_HASEL"
# Firmware: -w (jego ostrzezenia to sprawa prawdziwego kompilatora ESP32);
# atrapy i symulator: z ostrzezeniami.
g++ $WSPOLNE -w -c -o "$BUD/main.o" "$SRC/main.cpp" &
g++ $WSPOLNE -w -c -o "$BUD/dash.o" "$SRC/dashboard_lvgl_poc.cpp" &
g++ $WSPOLNE -w -c -o "$BUD/port.o" "$SRC/lvgl_port.cpp" &
g++ $WSPOLNE -Wall -Wextra -c -o "$BUD/atrapa.o" mock/atrapa.cpp &
g++ $WSPOLNE -Wall -Wextra -c -o "$BUD/sym.o" symulator_panelu.cpp &
BLEDY=0
for zadanie in $(jobs -p); do wait "$zadanie" || BLEDY=1; done
if [ "$BLEDY" != 0 ]; then echo "    BLAD KOMPILACJI (patrz komunikaty wyzej)"; exit 1; fi
# --wrap: polaczenia panelu do 192.168.4.1:80 ida do serwera w symulator_panelu.cpp
g++ -o "$BUD/symulator_panelu" "$BUD"/main.o "$BUD"/dash.o "$BUD"/port.o "$BUD"/atrapa.o "$BUD"/sym.o \
    "$LIB" $SAN -Wl,--wrap=connect,--wrap=send,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free -lpthread
echo "    OK"

echo "[4] scenariusze (log Seriala i zrzuty ekranu: $WYNIKI)"
# Ekran Wykresy: odpowiedzi centrali 3.58.4 zapisane jej kodem (wzorce_centrali/)
# i punkty, ktore z tych samych starych CSV rysowal firmware v1.2.2 (wzorce_v122/).
export LCD_WZORCE_CENTRALI="$TU/wzorce_centrali"
export LCD_WZORCE_V122="$TU/wzorce_v122"
OPCJE=(--wyjscie "$WYNIKI")
if [ "$OBSERWACJE" = "1" ]; then OPCJE+=(--obserwacje); fi
if [ -n "${1:-}" ]; then
  "$BUD/symulator_panelu" "$1" --log "${OPCJE[@]}"
  exit $?
fi
NIEZALICZONE=0
for scenariusz in $("$BUD/symulator_panelu" --lista); do
  "$BUD/symulator_panelu" "$scenariusz" "${OPCJE[@]}" || NIEZALICZONE=$((NIEZALICZONE + 1))
done
if [ "$OBSERWACJE" = "1" ]; then
  echo "KONIEC (tryb obserwacji - bez oceny)"
elif [ "$NIEZALICZONE" = 0 ]; then
  echo "WSZYSTKIE SCENARIUSZE ZALICZONE"
else
  echo "NIEZALICZONE SCENARIUSZE: $NIEZALICZONE"
  exit 1
fi
