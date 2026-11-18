#!/usr/bin/env bash
# ============================================================================
# Testy na PC kodu [v3.58.4 PANEL-WYKRESY] centrali (stan z v3.58.5) - opis: README.txt.
#   ./uruchom_testy.sh              zwykle uruchomienie
#   SANITIZER=1 ./uruchom_testy.sh  to samo z AddressSanitizer + UBSan (wolniej)
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
#   AKTUALIZUJ_WZORCE=1 ./uruchom_testy.sh   po SWIADOMEJ zmianie formatu odpowiedzi:
#                                   zapisuje nowe wzorce dla testow panelu LCD
# Osobno: ./porownaj_z_3.58.4.sh      - co zmienia 3.58.5 wzgledem 3.58.4 (tabela przed/po),
#         ./sprawdz_naglowki_esp32.sh - ten sam kod prawdziwymi naglowkami ESP32.
# Wymaga: g++ (C++17), python3. Linux albo WSL.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
ROB="${TMPDIR:-/tmp}/centrala_panel_hist_test"
# Wzorce odpowiedzi, z ktorych korzystaja testy panelu LCD (to samo repozytorium).
WZORCE_PANELU="${WZORCE_PANELU:-../../../LCD-Centrali/testy_host/wzorce_centrali}"
mkdir -p "$ROB"
echo "[1] wycinam kod z $CPP"
python3 wytnij_blok.py "$CPP" "$ROB/panel_hist_wycinek.inc" | sed 's/^/    /'
echo "[2] kompilacja (g++ -Wall -Wextra)"
SAN=""
if [ "${SANITIZER:-0}" = "1" ]; then SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"; fi
g++ -std=gnu++17 -O1 -g -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion $SAN -I "$ROB" -o "$ROB/test_panel_hist" test_panel_hist.cpp
echo "[3] testy"
"$ROB/test_panel_hist"
echo "[4] wzorce odpowiedzi dla testow panelu LCD"
rm -rf "$ROB/wzorce"
mkdir -p "$ROB/wzorce"
"$ROB/test_panel_hist" --wzorce "$ROB/wzorce" | sed 's/^/    /'
if [ "${AKTUALIZUJ_WZORCE:-0}" = "1" ]; then
  mkdir -p "$WZORCE_PANELU"
  cp "$ROB"/wzorce/*.txt "$WZORCE_PANELU"/
  echo "    zapisane w $WZORCE_PANELU - teraz uruchom testy panelu (LCD-Centrali/testy_host/uruchom_testy.sh)"
elif [ ! -d "$WZORCE_PANELU" ]; then
  echo "    (katalogu $WZORCE_PANELU nie ma - paczka centrali rozpakowana osobno; porownanie pominiete)"
else
  ROZNE=0
  for plik in "$ROB"/wzorce/*.txt; do
    nazwa="$(basename "$plik")"
    if ! cmp -s "$plik" "$WZORCE_PANELU/$nazwa"; then
      echo "    ROZNICA: $nazwa (centrala odpowiada teraz inaczej niz wzorzec, na ktorym testowany jest panel)"
      ROZNE=1
    fi
  done
  # W druga strone: wzorzec w katalogu panelu, ktorego ten kod centrali nie wypisuje, to plik
  # nie wiadomo skad (np. poprawiany recznie albo z innej wersji centrali).
  for plik in "$WZORCE_PANELU"/*.txt; do
    nazwa="$(basename "$plik")"
    if [ "$nazwa" != "CZYTAJ.txt" ] && [ ! -f "$ROB/wzorce/$nazwa" ]; then
      echo "    ROZNICA: $nazwa jest w katalogu panelu, a ten kod centrali takiego wzorca nie wypisuje"
      ROZNE=1
    fi
  done
  if [ "$ROZNE" != 0 ]; then
    echo "    Wzorce panelu sa nieaktualne. Jesli zmiana formatu jest zamierzona:"
    echo "      AKTUALIZUJ_WZORCE=1 ./uruchom_testy.sh   i ponownie testy panelu."
    exit 1
  fi
  echo "    zgodne z $WZORCE_PANELU (testy panelu uzywaja tych samych odpowiedzi)"
fi
