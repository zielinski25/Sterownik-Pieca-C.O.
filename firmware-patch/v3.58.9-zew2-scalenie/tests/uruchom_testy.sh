#!/usr/bin/env bash
# ============================================================================
# Testy na PC firmware'u centrali 3.58.9 - opis: README.txt.
#   A. filtr Zew 2 (blok [v3.58.9 ZEW2-FILTR] wyciety z pliku centrali): 12 scenariuszy.
#   B. PRAWDZIWE zadanie wilgotnosc() z atrapami sprzetu: bramka 120 s / 2 s,
#      powtorka po 3 s, zapis Zew 2, wygaszanie 135 s, zla ramka, symulacja, millis().
#   C. scalenie: oba patche nakladaja sie na swoje podstawy i daja plik o tej samej
#      sumie; znaczniki obu linii (3.57.1x i 3.58.x) sa w pliku.
#   D. pola /api/status a pola czytane przez panel LCD (skrypt panelu).
#   E. testy paczek 3.58.7 i 3.58.6 (START-TG, OTA panelu, wykresy) na tym pliku.
#   ./uruchom_testy.sh              zwykle uruchomienie
#   SANITIZER=1 ./uruchom_testy.sh  A i B dodatkowo z AddressSanitizer + UBSan
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: g++ (C++17), python3, patch, sha256sum. Linux albo WSL. Czesci C-E
# wymagaja pelnego repozytorium (firmware-patch/v3.58.8-*, v3.57.13-*, v3.58.7-*, LCD-Centrali/).
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
ROB="${TMPDIR:-/tmp}/centrala_3589_test"
rm -rf "$ROB"; mkdir -p "$ROB"
CXX="${CXX:-g++}"
FLAGI=(-std=c++17 -Wall -Wextra -Werror -O1 -I"$ROB")
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)

echo "=== A. [v3.58.9 ZEW2-FILTR] filtr blednych ramek DHT22 (wycinek z $CPP) ==="
python3 wytnij_zew2_filtr.py "$CPP_ABS" "$ROB/zew2_filtr_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr"
"$ROB/test_zew2_filtr" | sed 's/^/    /'
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr_san"
  "$ROB/test_zew2_filtr_san" | tail -1 | sed 's/^/    [ASan+UBSan] /'
fi

echo
echo "=== B. wilgotnosc() z pliku centrali + atrapy: powtorka po 3 s, bramki, Zew 2 ==="
python3 wytnij_wilgotnosc.py "$CPP_ABS" "$ROB/wilgotnosc_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc"
"$ROB/test_wilgotnosc" | sed 's/^/    /'
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc_san"
  "$ROB/test_wilgotnosc_san" | tail -1 | sed 's/^/    [ASan+UBSan] /'
fi

echo
echo "=== C. scalenie 3.57.13 + 3.58.8 -> 3.58.9 ==="
echo "[C1] znaczniki obu linii w pliku"
python3 - "$CPP_ABS" <<'PY'
import sys, re
t = open(sys.argv[1], encoding="utf-8").read()
musi = {
  # linia 3.57.10-3.57.13 (Zew 2, harmonogram DS, 12 bitow)
  'FIRMWARE_VERSION = "v3.58.9"': 1,
  'j += F("\\"t_zewn_2\\":"); j += jsonTempDiag();': 1,          # /api/status
  'ja("\\"t_zewn_2\\":" + jsonTempDiag() + ",");': 1,             # Firebase /piec/status
  'dodajTemp("t_zewn_2", s.tDhtDiagX10, -400, 800);': 1,          # probki minutowe
  'static_assert(offsetof(TelemetrySample, tDhtDiagX10) == 76': 1,
  'sensors1.begin(12);': 1,
  'dodajZadanie(Czujnik_DS,             DS_PLAN_TICK_MS);': 1,
  'TELEMETRY_STALE_DHT22_S  = 135;': 1,
  'const size_t FB_JSON_CAP = 4096;': 1,
  'static void dsPlanRead(DsPlanId id, uint32_t now)': 1,
  # linia 3.58.0-3.58.8
  'j += F("\\"fw\\":\\""); j += FIRMWARE_VERSION; j += F("\\",");': 1,   # 3.58.8
  '[v3.58.7 START-TG]': None,                                      # 3.58.7 (>=1)
  'panelOtaKlejJson(': None,                                       # 3.58.6
  '/api/history/panel': None,                                      # 3.58.4/5
  # 3.58.9
  '// --- ZEW2-FILTR-POCZATEK ---': 1,
  '// --- ZEW2-FILTR-KONIEC ---': 1,
  'zew2Filtruj(g_zew2Filtr, ramkaOk, dhtTempPomijana, czasBiezacy, &tZew2)': 1,
  'const bool powtorkaZew2 = g_zew2Filtr.weryfikacja;': 1,
}
bledy = 0
for k, n in musi.items():
    c = t.count(k)
    ok = (c == n) if n is not None else (c >= 1)
    print("    [%s] %-70s x%d" % ("OK" if ok else "BLAD", k[:70], c))
    bledy += 0 if ok else 1
kod = "\n".join(l for l in t.split("\n") if not l.lstrip().startswith("//"))   # bez komentarzy (changelog wspomina t_zewn_3)
for zle in ('\n<<<<<<< ', '\n>>>>>>> ', '\n=======\n', '@@NAGLOWEK@@', 'FIRMWARE_VERSION = "v3.57.13"', 'FIRMWARE_VERSION = "v3.58.8"',
            '"t_zewn_3"', 'tBmpDiagX10', 'zapiszTempDiag(dhtErr == DHT_OK'):
    c = kod.count(zle)
    print("    [%s] nie ma w kodzie: %-52s x%d" % ("OK" if c == 0 else "BLAD", zle.strip()[:52], c))
    bledy += 0 if c == 0 else 1
sys.exit(1 if bledy else 0)
PY
if [ -f ../../v3.58.8-fw-w-statusie/src/main_centrala.cpp ] && [ -f ../../v3.57.13-zew2-kandydat-z-dysku/src/main_centrala.cpp ]; then
  echo "[C2] patche: 3.58.8 -> 3.58.9 i 3.57.13 -> 3.58.9 daja ten sam plik"
  mkdir -p "$ROB/p1/src" "$ROB/p2/src"
  cp ../../v3.58.8-fw-w-statusie/src/main_centrala.cpp "$ROB/p1/src/"
  cp ../../v3.57.13-zew2-kandydat-z-dysku/src/main_centrala.cpp "$ROB/p2/src/"
  ( cd "$ROB/p1" && patch -p1 -s < "$OLDPWD/../main_centrala-v3.58.8-do-v3.58.9.patch" )
  ( cd "$ROB/p2" && patch -p1 -s < "$OLDPWD/../main_centrala-v3.57.13-do-v3.58.9.patch" )
  S0=$(sha256sum "$CPP_ABS" | cut -d' ' -f1); S1=$(sha256sum "$ROB/p1/src/main_centrala.cpp" | cut -d' ' -f1); S2=$(sha256sum "$ROB/p2/src/main_centrala.cpp" | cut -d' ' -f1)
  if [ "$S0" = "$S1" ] && [ "$S0" = "$S2" ]; then echo "    OK: $S0"; else echo "    BLAD: $S0 / $S1 / $S2"; exit 1; fi
  if [ -f ../SHA256SUMS.txt ]; then
    if grep -q "$S0" ../SHA256SUMS.txt; then echo "    OK: suma zgodna z SHA256SUMS.txt"; else echo "    BLAD: brak sumy w SHA256SUMS.txt"; exit 1; fi
  fi
else
  echo "[C2] (brak paczek 3.58.8 / 3.57.13 obok - sprawdzenie patchy pominiete)"
fi

SKRYPT_PANELU="../../../LCD-Centrali/testy_host/sprawdz_pola_statusu.py"
echo
echo "=== D. pola /api/status a pola czytane przez panel LCD ==="
if [ -f "$SKRYPT_PANELU" ]; then
  python3 "$SKRYPT_PANELU" "$CPP_ABS"
else
  echo "    (brak $SKRYPT_PANELU - paczka rozpakowana osobno; czesc D pominieta)"
fi

TESTY_3587="${TESTY_3587:-../../v3.58.7-start-telegram/tests}"
echo
echo "=== E. testy paczek 3.58.7 i 3.58.6 (START-TG, OTA panelu, wykresy) na pliku 3.58.9 ==="
if [ -d "$TESTY_3587" ]; then
  ( cd "$TESTY_3587" && CPP_PATH="$CPP_ABS" SANITIZER="${SANITIZER:-0}" ./uruchom_testy.sh ) | grep -E 'sprawdzen (OK|,)|WSZYSTKO|BLAD|nie przeszlo|^ *zgodn' | sed 's/^/    /'
else
  echo "    (katalogu $TESTY_3587 nie ma - czesc E pominieta)"
fi
echo
echo "WSZYSTKIE CZESCI ZAKONCZONE BEZ BLEDU"
