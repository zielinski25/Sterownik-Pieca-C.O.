#!/usr/bin/env bash
# ============================================================================
# Testy na PC firmware'u centrali 3.58.11 - opis: README.txt.
#   A. DYM-SYM: blok przed pozar() + PRAWDZIWY fragment pozar() (aktywacja /
#      dezaktywacja alarmu POZAR) wyciety z pliku centrali; 6 scenariuszy.
#   B. plik 3.58.11 = 3.58.10 + DYM-SYM: znaczniki, brak starych fragmentow,
#      patch 3.58.10 -> 3.58.11 i skrypt zrob_35811.py daja ten sam plik (SHA).
#   C. pola /api/status a pola czytane przez panel LCD (skrypt panelu).
#   D. testy paczki 3.58.10 (filtr Zew 2, wilgotnosc()) i 3.58.7/3.58.6
#      (START-TG, OTA panelu, wykresy) na tym pliku - kod tych blokow bez zmian.
#   ./uruchom_testy.sh              zwykle uruchomienie
#   SANITIZER=1 ./uruchom_testy.sh  A dodatkowo z AddressSanitizer + UBSan
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: g++ (C++17), python3, patch, sha256sum. Linux albo WSL. Czesci B-D
# wymagaja pelnego repozytorium (firmware-patch/v3.58.10-*, v3.58.7-*, LCD-Centrali/).
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
ROB="${TMPDIR:-/tmp}/centrala_35811_test"
rm -rf "$ROB"; mkdir -p "$ROB"
CXX="${CXX:-g++}"
FLAGI=(-std=c++17 -Wall -Wextra -Werror -O1 -I"$ROB")
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)

echo "=== A. DYM-SYM: alarm POZAR z symulacji gasnie z koncem symulacji (wycinek z $CPP) ==="
python3 wytnij_dym_sym.py "$CPP_ABS" "$ROB/dym_sym_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_dym_sym.cpp -o "$ROB/test_dym_sym"
"$ROB/test_dym_sym" | sed 's/^/    /'
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_dym_sym.cpp -o "$ROB/test_dym_sym_san"
  "$ROB/test_dym_sym_san" | tail -1 | sed 's/^/    [ASan+UBSan] /'
fi

echo
echo "=== B. plik 3.58.11 = 3.58.10 + DYM-SYM ==="
echo "[B1] znaczniki 3.58.11 w pliku, brak starych fragmentow"
python3 - "$CPP_ABS" <<'PY'
import sys
t = open(sys.argv[1], encoding="utf-8").read()
musi = {
  'FIRMWARE_VERSION = "v3.58.11"': 1,
  '// --- DYM-SYM-POCZATEK ---': 1,
  '// --- DYM-SYM-KONIEC ---': 1,
  'bool alarmDymuZSymulacji = false;': 1,
  'extern bool alarmDymuZSymulacji;': 1,
  'static void dymAlarmSkasuj(const char* powod)': 1,
  'static bool dymAlarmPoKoncuSymulacji(bool symDymTeraz)': 1,
  'dymAlarmPoKoncuSymulacji(symDymTeraz);': 1,
  'alarmDymuZSymulacji = symDymTeraz;': 1,
  'if (czyAlarmAktywny == 1 && !symDymTeraz) alarmDymuZSymulacji = false;': 1,
  'dymAlarmSkasuj(nullptr);': 1,
  'dymAlarmSkasuj("koniec_symulacji");': 1,
  'j += F("\\"dym_alarm_sym\\":");': 1,
  'zapiszLogKrytyczny(F("tag=ALARM lvl=INFO zdarzenie=dezaktywacja typ=POZAR"));': 1,   # tylko w dymAlarmSkasuj()
  'zapiszLogKrytyczny(F("tag=ALARM lvl=CRIT zdarzenie=aktywacja typ=POZAR"));': 1,
  # 3.58.10 i wczesniej - bez zmian
  'static const unsigned long ZEW2_PONOWIENIE_MS = 5000UL;': 1,
  'static const uint32_t TEMP_DIAG_WYGASZANIE_S = 255;': 1,
  'j += F("\\"fw\\":\\""); j += FIRMWARE_VERSION; j += F("\\",");': 1,
  '[v3.58.7 START-TG]': None,
  'panelOtaKlejJson(': None,
  '/api/history/panel': None,
}
bledy = 0
for k, n in musi.items():
    c = t.count(k)
    ok = (c == n) if n is not None else (c >= 1)
    print("    [%s] %-76s x%d" % ("OK" if ok else "BLAD", k[:76], c))
    bledy += 0 if ok else 1
kod = "\n".join(l for l in t.split("\n") if not l.lstrip().startswith("//"))
for zle in ('\n<<<<<<< ', '\n>>>>>>> ', '\n=======\n', 'FIRMWARE_VERSION = "v3.58.10"', 'FIRMWARE_VERSION = "v3.58.9"'):
    c = kod.count(zle)
    print("    [%s] nie ma w kodzie: %-56s x%d" % ("OK" if c == 0 else "BLAD", zle.strip()[:56], c))
    bledy += 0 if c == 0 else 1
# dezaktywacja w pozar() tylko przez dymAlarmSkasuj(): stary blok sprzatania nie moze zostac zdublowany
poz = t[t.index("void pozar() {"):]
poz = poz[:poz.index("\n}\n") + 3]   # tylko cialo pozar()
c = poz.count("alarmMuteRemainingMs(alarmMuteDym, false);")
print("    [%s] w pozar() sprzatanie po alarmie nie jest zdublowane (x%d, oczekiwane 0)" % ("OK" if c == 0 else "BLAD", c))
bledy += 0 if c == 0 else 1
sys.exit(1 if bledy else 0)
PY
if [ -f ../../v3.58.10-zew2-ponowienie/src/main_centrala.cpp ]; then
  echo "[B2] patch 3.58.10 -> 3.58.11 na pliku 3.58.10 i skrypt zrob_35811.py daja ten sam plik"
  mkdir -p "$ROB/p1/src"
  cp ../../v3.58.10-zew2-ponowienie/src/main_centrala.cpp "$ROB/p1/src/"
  ( cd "$ROB/p1" && patch -p1 -s < "$OLDPWD/../main_centrala-v3.58.10-do-v3.58.11.patch" )
  python3 zrob_35811.py ../../v3.58.10-zew2-ponowienie/src/main_centrala.cpp "$ROB/zrob.cpp" | sed 's/^/    /'
  S0=$(sha256sum "$CPP_ABS" | cut -d' ' -f1); S1=$(sha256sum "$ROB/p1/src/main_centrala.cpp" | cut -d' ' -f1); S2=$(sha256sum "$ROB/zrob.cpp" | cut -d' ' -f1)
  if [ "$S0" = "$S1" ] && [ "$S0" = "$S2" ]; then echo "    OK: $S0"; else echo "    BLAD: $S0 / $S1 / $S2"; exit 1; fi
  if [ -f ../SHA256SUMS.txt ]; then
    if grep -q "$S0" ../SHA256SUMS.txt; then echo "    OK: suma zgodna z SHA256SUMS.txt"; else echo "    BLAD: brak sumy w SHA256SUMS.txt"; exit 1; fi
  fi
else
  echo "[B2] (brak paczki 3.58.10 obok - sprawdzenie patcha pominiete)"
fi

SKRYPT_PANELU="../../../LCD-Centrali/testy_host/sprawdz_pola_statusu.py"
echo
echo "=== C. pola /api/status a pola czytane przez panel LCD ==="
if [ -f "$SKRYPT_PANELU" ]; then
  python3 "$SKRYPT_PANELU" "$CPP_ABS"
else
  echo "    (brak $SKRYPT_PANELU - paczka rozpakowana osobno; czesc C pominieta)"
fi

echo
echo "=== D. testy paczek 3.58.10 (Zew 2, wilgotnosc) i 3.58.7/3.58.6 na pliku 3.58.11 ==="
TESTY_35810="${TESTY_35810:-../../v3.58.10-zew2-ponowienie/tests}"
if [ -d "$TESTY_35810" ]; then
  # Skrypty wycinajace z 3.58.10 sprawdzaja numer wersji w pliku - kopia z
  # numerem 3.58.10 (jedyna roznica), wycinane bloki Zew 2 sa identyczne.
  sed 's/FIRMWARE_VERSION = "v3.58.11"/FIRMWARE_VERSION = "v3.58.10"/' "$CPP_ABS" > "$ROB/jako_35810.cpp"
  ( cd "$TESTY_35810"
    python3 wytnij_zew2_filtr.py "$ROB/jako_35810.cpp" "$ROB/zew2_filtr_wycinek.h" | sed 's/^/    /'
    "$CXX" "${FLAGI[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr" && "$ROB/test_zew2_filtr" | tail -1 | sed 's/^/    [A 3.58.10] /'
    python3 wytnij_wilgotnosc.py "$ROB/jako_35810.cpp" "$ROB/wilgotnosc_wycinek.h" | sed 's/^/    /'
    "$CXX" "${FLAGI[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc" && "$ROB/test_wilgotnosc" | tail -1 | sed 's/^/    [B 3.58.10] /' )
else
  echo "    (katalogu $TESTY_35810 nie ma - testy 3.58.10 pominiete)"
fi
TESTY_3587="${TESTY_3587:-../../v3.58.7-start-telegram/tests}"
if [ -d "$TESTY_3587" ]; then
  ( cd "$TESTY_3587" && CPP_PATH="$CPP_ABS" SANITIZER="${SANITIZER:-0}" ./uruchom_testy.sh ) | grep -E 'sprawdzen (OK|,)|WSZYSTKO|BLAD|nie przeszlo|^ *zgodn' | sed 's/^/    /'
else
  echo "    (katalogu $TESTY_3587 nie ma - testy 3.58.7/3.58.6 pominiete)"
fi
echo
echo "WSZYSTKIE CZESCI ZAKONCZONE BEZ BLEDU"
