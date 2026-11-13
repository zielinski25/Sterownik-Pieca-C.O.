#!/usr/bin/env bash
# ============================================================================
# Testy na PC firmware'u centrali 3.58.6 - opis: README.txt.
#   czesc A: kod [v3.58.6 PANEL-OTA] - posrednictwo aktualizacji panelu LCD (nowy w tej wersji),
#   czesc B: kod [v3.58.4 PANEL-WYKRESY] - wykresy dla panelu (bez zmian wzgledem 3.58.5;
#            te same testy co w paczce 3.58.5, uruchomione na pliku 3.58.6).
#   ./uruchom_testy.sh              zwykle uruchomienie
#   SANITIZER=1 ./uruchom_testy.sh  to samo z AddressSanitizer + UBSan (wolniej)
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
#   AKTUALIZUJ_WZORCE=1 ./uruchom_testy.sh   po SWIADOMEJ zmianie kodu centrali, z ktorego
#                                   korzystaja testy panelu LCD: zapisuje nowa kopie bloku
#                                   decyzji OTA i nowe wzorce odpowiedzi historii
# Osobno: python3 sprawdz_czulosc_ota.py - czy testy czesci A wykrywaja celowo wprowadzone bledy,
#         python3 sprawdz_czulosc.py     - to samo dla czesci B,
#         ./sprawdz_naglowki_esp32.sh    - ten sam kod prawdziwymi naglowkami ESP32.
# Wymaga: g++ (C++17), python3. Linux albo WSL.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
ROB="${TMPDIR:-/tmp}/centrala_panel_hist_test"
# Wzorce odpowiedzi, z ktorych korzystaja testy panelu LCD (to samo repozytorium).
WZORCE_PANELU="${WZORCE_PANELU:-../../../LCD-Centrali/testy_host/wzorce_centrali}"
# Kopia bloku decyzji OTA, ktora wlaczaja testy panelu LCD (to samo repozytorium).
KOD_PANELU="${KOD_PANELU:-../../../LCD-Centrali/testy_host/kod_centrali}"
# Poprzednia wersja pliku centrali (to samo repozytorium) - do porownania kodu wykresow.
CPP_3585="${CPP_3585:-../../v3.58.5-panel-wykresy-po-restarcie/src/main_centrala.cpp}"
mkdir -p "$ROB"
SAN=""
if [ "${SANITIZER:-0}" = "1" ]; then SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"; fi

echo "=== A. [v3.58.6 PANEL-OTA] posrednictwo aktualizacji panelu LCD ==="
echo "[A1] wycinam kod z $CPP"
python3 wytnij_blok.py "$CPP" --ota "$ROB/panel_ota_wycinek.inc" | sed 's/^/    /'
python3 wytnij_blok.py "$CPP" --ota-blok "$ROB/panel_ota_blok.inc" | sed 's/^/    /'
echo "[A2] kompilacja (g++ -Wall -Wextra -Wshadow -Wconversion)"
# shellcheck disable=SC2086
g++ -std=gnu++17 -O1 -g -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion $SAN -I "$ROB" -o "$ROB/test_panel_ota" test_panel_ota.cpp
echo "[A3] testy"
"$ROB/test_panel_ota"
echo "[A4] kopia bloku decyzji dla testow panelu LCD"
if [ "${AKTUALIZUJ_WZORCE:-0}" = "1" ]; then
  mkdir -p "$KOD_PANELU"
  cp "$ROB/panel_ota_blok.inc" "$KOD_PANELU/panel_ota_blok.inc"
  echo "    zapisana w $KOD_PANELU - teraz uruchom testy panelu (LCD-Centrali/testy_host/uruchom_testy.sh)"
elif [ ! -d "$KOD_PANELU" ]; then
  echo "    (katalogu $KOD_PANELU nie ma - paczka centrali rozpakowana osobno; porownanie pominiete)"
elif cmp -s "$ROB/panel_ota_blok.inc" "$KOD_PANELU/panel_ota_blok.inc"; then
  echo "    zgodna z $KOD_PANELU/panel_ota_blok.inc (testy panelu uzywaja TEGO kodu jako centrali)"
else
  echo "    ROZNICA: $KOD_PANELU/panel_ota_blok.inc to nie jest blok [v3.58.6 PANEL-OTA] z tego pliku centrali."
  echo "    Testy panelu rozmawiaja z nieaktualnym kodem. Jesli zmiana jest zamierzona:"
  echo "      AKTUALIZUJ_WZORCE=1 ./uruchom_testy.sh   i ponownie testy panelu."
  exit 1
fi

echo "=== B. [v3.58.4 PANEL-WYKRESY] wykresy dla panelu LCD (kod bez zmian wzgledem 3.58.5) ==="
echo "[1] wycinam kod z $CPP"
python3 wytnij_blok.py "$CPP" "$ROB/panel_hist_wycinek.inc" | sed 's/^/    /'
if [ -f "$CPP_3585" ]; then
  python3 wytnij_blok.py "$CPP_3585" "$ROB/panel_hist_wycinek_3585.inc" > /dev/null
  # Pierwsza linia wycinka zawiera numer wersji - porownujemy reszte.
  if cmp -s <(tail -n +2 "$ROB/panel_hist_wycinek.inc") <(tail -n +2 "$ROB/panel_hist_wycinek_3585.inc"); then
    echo "    kod wykresow identyczny z 3.58.5 (wycinek bajt w bajt, poza linia z numerem wersji)"
  else
    echo "    ROZNICA: kod [v3.58.4 PANEL-WYKRESY] rozni sie od 3.58.5, a ta wersja nie miala go zmieniac"
    exit 1
  fi
else
  echo "    (pliku $CPP_3585 nie ma - paczka rozpakowana osobno; porownanie z 3.58.5 pominiete)"
fi
echo "[2] kompilacja (g++ -Wall -Wextra)"
# shellcheck disable=SC2086
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
