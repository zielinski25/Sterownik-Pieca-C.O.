#!/usr/bin/env bash
# ============================================================================
# Sprawdzenie SKLADNI I TYPOW zrodel panelu PRAWDZIWYMI naglowkami ESP32
# ============================================================================
# Po co: testy z uruchom_testy.sh uzywaja atrap (mock/), wiec nie wykryja
# pomylki w nazwie albo sygnaturze funkcji z arduino-esp32 / ESP-IDF /
# TFT_eSPI. Ten skrypt kompiluje src/main.cpp, dashboard_lvgl_poc.cpp i
# lvgl_port.cpp w trybie "tylko sprawdz" (-fsyntax-only) z prawdziwymi
# naglowkami:
#   - rdzen i biblioteki arduino-esp32 (domyslnie 3.0.7; RDZEN=3.3.8 - patrz nizej),
#   - ESP-IDF 5.1.4 z sdkconfig plytki esp32 (repozytorium esp32-arduino-libs),
#   - newlib 4.1.0 (wersja Espressif), TFT_eSPI 2.5.43, LVGL 8.3.11,
#     ArduinoJson 7.4.3, flagi -D z platformio.ini, include/lv_conf.h projektu.
# Kompilator: clang z pakietu Pythona "ziglang" (ma cel xtensa). Wszystko
# pobiera sie z PyPI i GitHuba do katalogu podrecznego (ok. 250 MB).
#
# CZEGO TO NIE ZASTEPUJE - to NIE jest build PlatformIO:
#   - inny kompilator (clang zamiast GCC Espressif) i inna biblioteka C++
#     (libc++ zamiast libstdc++), bez linkowania, bez pliku .bin;
#   - RDZEN=3.3.8: naglowki rdzenia/bibliotek 3.3.8 na naglowkach ESP-IDF 5.1
#     (IDF 5.5 nie da sie pobrac z GitHuba jako repozytorium), dlatego dwa typy
#     znane tylko nowszemu IDF sa podstawione zastepczo (-Dtouch_value_t...,
#     -Dwifi_band_t...).
# Wynik "bez bledow" znaczy: nazwy, sygnatury i typy uzyte w zrodlach panelu
# zgadzaja sie z prawdziwymi naglowkami. Nie znaczy, ze firmware dziala.
#
#   ./sprawdz_naglowki_esp32.sh                  rdzen 3.0.7 (spojny komplet z IDF 5.1)
#   RDZEN=3.3.8 ./sprawdz_naglowki_esp32.sh      naglowki rdzenia 3.3.8
#   SRC=/sciezka/do/src ./sprawdz_naglowki_esp32.sh   inne zrodla (np. starsza wersja)
#   SRC=/katalog PLIKI="a.cpp b.cpp" ./sprawdz_naglowki_esp32.sh   inne pliki z katalogu SRC
#       (tak sprawdzany jest fragment firmware'u centrali - patrz
#        firmware-patch/v3.58.5-panel-wykresy-po-restarcie/tests/sprawdz_naglowki_esp32.sh)
#   OSTRZEZENIA=1 ./sprawdz_naglowki_esp32.sh    dodatkowo liczy ostrzezenia clang
#       (-Wall -Wextra -Wformat=2 -Wshadow) w plikach z katalogu SRC i pokazuje pierwsze z nich.
#       Wynik skryptu zalezy nadal tylko od bledow: ostrzezenia clang to nie ostrzezenia GCC.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
TU="$(pwd)"
SRC="${SRC:-$TU/../src}"
INCLUDE="$TU/../include"
RDZEN="${RDZEN:-3.0.7}"
K="${LCD_TESTY_CACHE:-${TMPDIR:-/tmp}/lcd_testy_host}/naglowki_esp32"
mkdir -p "$K"

echo "[1] kompilator (clang z pakietu ziglang)"
if ! python3 -c "import ziglang" 2>/dev/null; then
  python3 -m pip install -q ziglang 2>/dev/null || python3 -m pip install -q --break-system-packages ziglang
fi
ZD="$(python3 -c 'import ziglang, os; print(os.path.dirname(ziglang.__file__))')"
python3 -m ziglang clang --version | head -1 | sed 's/^/    /'

echo "[2] naglowki (arduino-esp32 $RDZEN, ESP-IDF 5.1, newlib, TFT_eSPI, LVGL, ArduinoJson)"
klon() {   # klon <katalog> <tag/galaz> <adres> [sciezki do sparse-checkout...]
  local kat="$1" ref="$2" adres="$3"; shift 3
  if [ ! -d "$K/$kat/.git" ]; then
    echo "    pobieram $adres ($ref)"
    if [ $# -gt 0 ]; then
      git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$ref" --filter=blob:none --sparse "$adres" "$K/$kat"
      git -C "$K/$kat" sparse-checkout set "$@" >/dev/null
    else
      git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$ref" "$adres" "$K/$kat"
    fi
  fi
}
klon "rdzen_$RDZEN" "$RDZEN" https://github.com/espressif/arduino-esp32.git cores/esp32 variants/esp32 libraries
klon idf_libs idf-release/v5.1 https://github.com/espressif/esp32-arduino-libs.git esp32/flags esp32/dio_qspi/include esp32/include
klon newlib esp-4.1.0_20230208 https://github.com/espressif/newlib-esp32.git newlib/libc/include newlib/libc/sys/xtensa
klon TFT_eSPI V2.5.43 https://github.com/Bodmer/TFT_eSPI.git
klon lvgl v8.3.11 https://github.com/lvgl/lvgl.git
klon ArduinoJson v7.4.3 https://github.com/bblanchon/ArduinoJson.git

# Naglowki biblioteki C tak, jak wygladaja w toolchainie: newlib + dwa pliki,
# ktore normalnie generuje jego configure (tu odtworzone recznie).
SYS="$K/sysroot/include"
if [ ! -f "$SYS/newlib.h.gotowe" ]; then
  rm -rf "$SYS"; mkdir -p "$SYS/sys"
  cp -r "$K/newlib/newlib/libc/include/." "$SYS/"
  cp -r "$K/newlib/newlib/libc/sys/xtensa/include/." "$SYS/"
  cp "$K/newlib/newlib/libc/sys/xtensa/sys/file.h" "$SYS/sys/"
  printf '#ifndef _NEWLIB_VERSION_H__\n#define _NEWLIB_VERSION_H__ 1\n#define _NEWLIB_VERSION "4.1.0"\n#define __NEWLIB__ 4\n#define __NEWLIB_MINOR__ 1\n#define __NEWLIB_PATCHLEVEL__ 0\n#endif\n' > "$SYS/_newlib_version.h"
  printf '#ifndef __NEWLIB_H__\n#define __NEWLIB_H__ 1\n#include <_newlib_version.h>\n#define _WANT_IO_C99_FORMATS 1\n#define _WANT_IO_LONG_LONG 1\n#define _WANT_REGISTER_FINI 1\n#define _MB_LEN_MAX 1\n#define HAVE_INITFINI_ARRAY 1\n#define _ATEXIT_DYNAMIC_ALLOC 1\n#define _HAVE_LONG_DOUBLE 1\n#define _HAVE_CC_INHIBIT_LOOP_TO_LIBCALL 1\n#define _LDBL_EQ_DBL 1\n#define _FVWRITE_IN_STREAMIO 1\n#define _FSEEK_OPTIMIZATION 1\n#define _WIDE_ORIENT 1\n#define _UNBUF_STREAM_OPT 1\n#define _RETARGETABLE_LOCKING 1\n#endif\n' > "$SYS/newlib.h"
  touch "$SYS/newlib.h.gotowe"
fi
# Hasla: jak w uruchom_testy.sh - atrapa, gdy nie ma src/secrets.h.
mkdir -p "$K/atrapa_hasel"
printf '#pragma once\n#define SECRET_WIFI_AP_PASSWORD "haslo-testowe"\n#define SECRET_OTA_HASLO "haslo-ota-testowe"\n' > "$K/atrapa_hasel/secrets.h"

R="$K/rdzen_$RDZEN"
L="$K/idf_libs/esp32"
BIBLIOTEKI=""
for b in WiFi Network HTTPClient HTTPUpdate Update Preferences SPI FS NetworkClientSecure SPIFFS LittleFS Wire; do
  if [ -d "$R/libraries/$b/src" ]; then BIBLIOTEKI="$BIBLIOTEKI -I$R/libraries/$b/src"; fi
done
# Ustawienia libc++ (zig podaje je flagami -D zamiast pliku __config_site).
# Bez lokalizacji = bez <ostream>, stad ARDUINOJSON_ENABLE_STD_STREAM=0 nizej.
LIBCXX="-D_LIBCPP_ABI_VERSION=1 -D_LIBCPP_ABI_NAMESPACE=__1 -D_LIBCPP_HAS_THREADS=1 -D_LIBCPP_HAS_THREAD_API_PTHREAD=1 \
 -D_LIBCPP_HAS_THREAD_API_EXTERNAL=0 -D_LIBCPP_HAS_THREAD_API_WIN32=0 -D_LIBCPP_HAS_MONOTONIC_CLOCK=1 -D_LIBCPP_HAS_TERMINAL=1 \
 -D_LIBCPP_HAS_MUSL_LIBC=0 -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS -D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS \
 -D_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS=0 -D_LIBCPP_HAS_FILESYSTEM=0 -D_LIBCPP_HAS_RANDOM_DEVICE=0 \
 -D_LIBCPP_HAS_LOCALIZATION=0 -D_LIBCPP_HAS_UNICODE=1 -D_LIBCPP_HAS_WIDE_CHARACTERS=1 -D_LIBCPP_HAS_TIME_ZONE_DATABASE=0 \
 -D_LIBCPP_PSTL_BACKEND_SERIAL -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE \
 -D_LIBCPP_ASSERTION_SEMANTIC_DEFAULT=_LIBCPP_ASSERTION_SEMANTIC_IGNORE"
# Flagi -D z build_flags w platformio.ini (bez komentarzy po sredniku).
PIO="$(sed 's/;.*//' "$TU/../platformio.ini" | sed -n '/^build_flags/,/^[a-z_.]* *=/p' | grep -o -- '-D[A-Za-z0-9_]*\(=[A-Za-z0-9_]*\)\?' | tr '\n' ' ')"
ZASTEPCZE=""
if [ "$RDZEN" != "3.0.7" ]; then ZASTEPCZE="-Dtouch_value_t=uint32_t -Dwifi_band_t=int -Dwifi_band_mode_t=int"; fi

# Ostrzezenia: domyslnie wylaczone (-Wno-everything). clang traktuje jak blad m.in. zawezanie
# w {} (GCC tylko ostrzega) - z OSTRZEZENIA=1 jest ono ostrzezeniem, jak w GCC.
OSTRZ="-Wno-everything"
if [ "${OSTRZEZENIA:-0}" = "1" ]; then OSTRZ="-Wall -Wextra -Wformat=2 -Wshadow -Wno-error=c++11-narrowing"; fi

echo "[3] sprawdzanie zrodel: $SRC"
echo "    flagi z platformio.ini: $PIO"
BLEDY=0
for plik in ${PLIKI:-main.cpp dashboard_lvgl_poc.cpp lvgl_port.cpp}; do
  WYNIK="$K/wynik_${RDZEN}_$plik.txt"
  # $OSTRZ (domyslnie -Wno-everything): ostrzezenia sa sprawa prawdziwego kompilatora (GCC); tu
  # liczą sie bledy. (clang traktuje jak blad m.in. zawezanie w {} - GCC nie.)
  # shellcheck disable=SC2046,SC2086
  if python3 -m ziglang clang --target=xtensa-esp32-elf -x c++ -std=gnu++2b -fno-rtti -fexceptions -fsyntax-only \
      -nostdinc -nostdinc++ -ferror-limit=50 $OSTRZ $LIBCXX \
      -isystem "$ZD/lib/libcxx/include" -isystem "$ZD/lib/libcxxabi/include" -isystem "$SYS" -isystem "$ZD/lib/include" \
      $(sed 's/\\"/"/g' "$L/flags/defines") \
      -DARDUINO=10812 -DARDUINO_ESP32_DEV -DARDUINO_ARCH_ESP32 '-DARDUINO_BOARD="ESP32_DEV"' '-DARDUINO_VARIANT="esp32"' \
      -DARDUINO_PARTITION_default -DESP32 -DCORE_DEBUG_LEVEL=0 -DARDUINO_RUNNING_CORE=1 -DARDUINO_EVENT_RUNNING_CORE=1 \
      -DARDUINO_USB_CDC_ON_BOOT=0 -DARDUINOJSON_ENABLE_STD_STREAM=0 $PIO $ZASTEPCZE \
      -iprefix "$L/include/" $(cat "$L/flags/includes") -I"$L/dio_qspi/include" \
      -I"$R/cores/esp32" -I"$R/variants/esp32" $BIBLIOTEKI \
      -I"$INCLUDE" -I"$SRC" -I"$K/atrapa_hasel" -I"$K/TFT_eSPI" -I"$K/ArduinoJson/src" -I"$K/lvgl" -I"$K/lvgl/src" \
      "$SRC/$plik" > "$WYNIK" 2>&1; then
    echo "    $plik: bez bledow"
    if [ "${OSTRZEZENIA:-0}" = "1" ]; then
      # Tylko ostrzezenia wskazujace pliki z katalogu SRC (nie naglowki bibliotek).
      ILE="$(grep -c "^$SRC/[^:]*:[0-9]*:[0-9]*: warning" "$WYNIK" || true)"
      echo "    $plik: ostrzezen clang w plikach z $SRC: $ILE"
      grep "^$SRC/[^:]*:[0-9]*:[0-9]*: warning" "$WYNIK" | sed "s|^$SRC/||" | head -10 | sed 's/^/      /' || true
    fi
  else
    BLEDY=1
    echo "    $plik: BLEDY ($(grep -c 'error:' "$WYNIK" || true)) - pelny wynik: $WYNIK"
    grep 'error:' "$WYNIK" | head -15 | sed 's/^/      /'
  fi
done
if [ "$BLEDY" = 0 ]; then
  echo "BEZ BLEDOW (naglowki arduino-esp32 $RDZEN)"
else
  exit 1
fi
