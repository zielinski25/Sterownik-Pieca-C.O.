#!/usr/bin/env bash
# ============================================================================
# Testy panelu LCD NA PC (bez plytki ESP32) - opis: README.md w tym katalogu.
# ============================================================================
#   ./uruchom_testy.sh                 wszystko: testy jednostkowe + symulator
#   ./uruchom_testy.sh --szybko        tylko testy jednostkowe (sam g++, 1 s)
#   ./uruchom_testy.sh dwie_centrale   jeden scenariusz symulatora, z logiem
#   SANITIZER=1 ./uruchom_testy.sh     to samo z AddressSanitizer + UBSan (wolniej)
#   TYLKO='^ota_' ./uruchom_testy.sh   tylko scenariusze, ktorych nazwa pasuje do wzorca (grep -E)
#   CENTRALA_CPP=sciezka/main_centrala.cpp ./uruchom_testy.sh   krok [1b] z innym plikiem centrali
#
# Porownanie ze STARSZA wersja firmware'u (bez oceniania, same obserwacje):
#   mkdir -p /tmp/stary && unzip -q "../../LCD Centrali.zip" -d /tmp/stary   # v1.1.0
#   SRC=/tmp/stary/src OBSERWACJE=1 ./uruchom_testy.sh
#
# Wymaga: g++ (C++17), gcc, ar, git (Linux albo WSL). Za pierwszym razem
# pobiera z GitHuba LVGL 8.4.0 i ArduinoJson 7.4.3 do katalogu podrecznego
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
echo "[1b] pola czytane przez panel a /api/status PRAWDZIWEJ centrali (najnowsza paczka firmware-patch/)"
# [v1.4.6 erratum] Atrapa centrali w symulatorze podaje to, co jej kazemy - to
# porownanie jest z kodem centrali (wifiZbudujJsonStatus() w main_centrala.cpp).
python3 sprawdz_pola_statusu.py ${CENTRALA_CPP:+"$CENTRALA_CPP"}
if [ "${1:-}" = "--szybko" ]; then exit 0; fi

echo "[2] biblioteki (LVGL 8.4.0, ArduinoJson 7.4.3)"
pobierz() {   # pobierz <katalog> <tag> <adres>
  if [ ! -d "$PODRECZNY/$1/.git" ]; then
    echo "    pobieram $3 ($2)"
    git clone --quiet --depth 1 --branch "$2" "$3" "$PODRECZNY/$1" 2>/dev/null
  fi
}
pobierz lvgl v8.4.0 https://github.com/lvgl/lvgl.git
pobierz ArduinoJson v7.4.3 https://github.com/bblanchon/ArduinoJson.git

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
  -I $TU/mock -I $INCLUDE -isystem $PODRECZNY/ArduinoJson/src -isystem $PODRECZNY/lvgl -I $SRC -I $ATRAPA_HASEL"
# -isystem dla bibliotek: -Wall ma pilnowac NASZEGO kodu, nie naglowkow ArduinoJson/LVGL.
# [v1.4.4 C8] Firmware tez z -Wall -Wextra (do v1.4.3: -w). Oczekiwany wynik:
# ZERO ostrzezen - nowe ostrzezenie = cos do poprawienia, nie do ukrycia.
# -Wno-format-truncation: jedyny wyjatek, jak przy test_diagnostyka wyzej
# (snprintf do bufora o znanej dlugosci - obciecie jest zamierzone).
OSTRZ="-Wall -Wextra -Wno-format-truncation"
# Z sanitizerem GCC zglasza -Wmaybe-uninitialized WEWNATRZ ArduinoJson 7.4.3
# (ObjectImpl.hpp, iterator()) - biblioteka, nie nasz kod; -isystem tego nie
# wycisza, bo funkcja jest wlaczana (inline) do naszych wywolan doc["..."].
if [ "$SANITIZER" = "1" ]; then OSTRZ="$OSTRZ -Wno-maybe-uninitialized"; fi
g++ $WSPOLNE $OSTRZ -c -o "$BUD/main.o" "$SRC/main.cpp" &
g++ $WSPOLNE $OSTRZ -c -o "$BUD/dash.o" "$SRC/dashboard_lvgl_poc.cpp" &
g++ $WSPOLNE $OSTRZ -c -o "$BUD/port.o" "$SRC/lvgl_port.cpp" &
# [v1.4.5 D] czcionki z polskimi literami (C, wygenerowane - patrz src/czcionki/CZYTAJ.txt)
for n in 14 20 28; do gcc -O1 -g $SAN -w -DLV_CONF_INCLUDE_SIMPLE -I "$INCLUDE" -isystem "$PODRECZNY/lvgl" -c -o "$BUD/czc$n.o" "$SRC/czcionki/lv_font_pl_$n.c" & done
g++ $WSPOLNE -Wall -Wextra -c -o "$BUD/atrapa.o" mock/atrapa.cpp &
g++ $WSPOLNE -Wall -Wextra -c -o "$BUD/sym.o" symulator_panelu.cpp &
BLEDY=0
for zadanie in $(jobs -p); do wait "$zadanie" || BLEDY=1; done
if [ "$BLEDY" != 0 ]; then echo "    BLAD KOMPILACJI (patrz komunikaty wyzej)"; exit 1; fi
# --wrap: polaczenia panelu do 192.168.4.1:80 ida do serwera w symulator_panelu.cpp
g++ -o "$BUD/symulator_panelu" "$BUD"/main.o "$BUD"/dash.o "$BUD"/port.o "$BUD"/czc14.o "$BUD"/czc20.o "$BUD"/czc28.o "$BUD"/atrapa.o "$BUD"/sym.o \
    "$LIB" $SAN -Wl,--wrap=connect,--wrap=send,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free -lpthread
PROGRAM="$BUD/symulator_panelu"
# PROTOTYP=1 (tylko pomiar_obciazenia.sh --prototyp): ten sam firmware, ale cztery
# funkcje LVGL sa podmienione przy linkowaniu na wersje z prototypy/ustaw_tylko_zmiany.c.
# Zwykle testy tego nie uzywaja.
if [ "${PROTOTYP:-0}" = "1" ]; then
  gcc -c -O1 -Wall -Wextra -DLV_CONF_INCLUDE_SIMPLE -I "$INCLUDE" -I "$PODRECZNY/lvgl" -o "$BUD/prototyp.o" prototypy/ustaw_tylko_zmiany.c
  PROGRAM="$BUD/symulator_panelu_prototyp"
  g++ -o "$PROGRAM" "$BUD"/main.o "$BUD"/dash.o "$BUD"/port.o "$BUD"/czc14.o "$BUD"/czc20.o "$BUD"/czc28.o "$BUD"/atrapa.o "$BUD"/sym.o "$BUD"/prototyp.o \
      "$LIB" $SAN -Wl,--wrap=connect,--wrap=send,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free \
      -Wl,--wrap=lv_label_set_text,--wrap=lv_obj_set_local_style_prop,--wrap=lv_img_set_src,--wrap=lv_obj_clear_flag -lpthread
fi
echo "    OK"

echo "[4] scenariusze (log Seriala i zrzuty ekranu: $WYNIKI)"
# Ekran Wykresy: odpowiedzi centrali (3.58.4 / 3.58.5) zapisane jej kodem (wzorce_centrali/)
# i punkty, ktore z tych samych starych CSV rysowal firmware v1.2.2 (wzorce_v122/).
export LCD_WZORCE_CENTRALI="$TU/wzorce_centrali"
export LCD_WZORCE_V122="$TU/wzorce_v122"
OPCJE=(--wyjscie "$WYNIKI")
if [ "$OBSERWACJE" = "1" ]; then OPCJE+=(--obserwacje); fi
if [ -n "${1:-}" ]; then
  # BEZ_LOGU=1: bez wypisywania calego logu Seriala (uzywa tego pomiar_obciazenia.sh).
  if [ "${BEZ_LOGU:-0}" = "1" ]; then
    "$PROGRAM" "$1" "${OPCJE[@]}"
  else
    "$PROGRAM" "$1" --log "${OPCJE[@]}"
  fi
  exit $?
fi
NIEZALICZONE=0
for scenariusz in $("$PROGRAM" --lista | { grep -E -- "${TYLKO:-.}" || true; }); do
  case "$scenariusz" in pomiar_*) continue ;; esac   # pomiary to nie testy: maja wlasne skrypty (pomiar_*.sh)
  "$PROGRAM" "$scenariusz" "${OPCJE[@]}" || NIEZALICZONE=$((NIEZALICZONE + 1))
done
if [ "$OBSERWACJE" = "1" ]; then
  echo "KONIEC (tryb obserwacji - bez oceny)"
elif [ "$NIEZALICZONE" = 0 ]; then
  echo "WSZYSTKIE SCENARIUSZE ZALICZONE"
else
  echo "NIEZALICZONE SCENARIUSZE: $NIEZALICZONE"
  exit 1
fi
