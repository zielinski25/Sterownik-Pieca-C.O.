#!/usr/bin/env bash
# ============================================================================
# Prawdziwe obrazy programow ESP przez kod centrali, ktory ocenia plik z wydania
# ============================================================================
# Po co: w uruchom_testy.sh "program ESP32" buduje sam test (programEsp32() w
# test_panel_ota.cpp) - wedlug tego samego opisu formatu, wedlug ktorego
# powstal kod. Ten skrypt bierze pliki zbudowane narzedziami Espressif
# (katalog test/images repozytorium esptool) i sprawdza na nich dwa zalozenia:
#   1. gdy bajt 23 naglowka ma wartosc 1, plik konczy sie suma SHA-256
#      wszystkiego, co przed nia (z tych 32 bajtow centrala i panel biora
#      identyfikator pliku),
#   2. bajty 12-13 naglowka to numer ukladu (0 = ESP32).
# Pliki ida przez panelOtaSkan*() wyciete z main_centrala.cpp, kawalkami po
# 512 B - tak jak w pobierzFirmwarePanelaZGithub(). W pelnym repozytorium te
# same pliki ida tez przez kod panelu v1.4.0 (diag::otaSkan*() z
# LCD-Centrali/src/diagnostyka_panelu.h): identyfikator ma wyjsc ten sam.
#
# CZEGO TO NIE SPRAWDZA: ze panel na plytce odczyta te sama sume z wlasnej
# pamieci (esp_partition_get_sha256()). To pokaze pierwszy start panelu v1.4.0:
# linia "stan_przy_starcie obraz=..." ma byc rowna "Identyfikator pliku" z
#   python3 narzedzia/ota_panelu_sprawdz.py --tag <tag> --bin <firmware.bin>
#
# Wymaga: sieci (api.github.com), curl, g++, python3.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
PANEL_H="${PANEL_H:-../../../LCD-Centrali/src/diagnostyka_panelu.h}"
ROB="${TMPDIR:-/tmp}/centrala_panel_hist_test/obrazy"
mkdir -p "$ROB"
# nazwa, identyfikator obiektu w repozytorium espressif/esptool (git blob), oczekiwana ocena centrali
PLIKI="
bootloader_esp32_v5_2.bin d7a5cfb96d3df1f23a02c659eefb62ea88541955 przyjety
bootloader_esp32.bin 6b234435d2d1618efd6be31aa366236e5c89414d bez_sumy
esp_idf_blink_esp32s2.bin 32717c7930d31efc661e7a9418aab8a2901ee2c5 inny_uklad
"
echo "[1] pobieram obrazy z repozytorium espressif/esptool (test/images)"
echo "$PLIKI" | while read -r nazwa blob _; do
  [ -z "$nazwa" ] && continue
  if [ ! -s "$ROB/$nazwa" ]; then
    curl -fsSL -H "Accept: application/vnd.github.raw" -o "$ROB/$nazwa" "https://api.github.com/repos/espressif/esptool/git/blobs/$blob"
  fi
  # identyfikator obiektu git = sha1("blob <rozmiar>\0" + tresc): plik jest tym, o ktory prosilismy
  python3 - "$ROB/$nazwa" "$blob" <<'PY'
import hashlib, sys
d = open(sys.argv[1], 'rb').read()
if hashlib.sha1(b'blob %d\0' % len(d) + d).hexdigest() != sys.argv[2]:
    sys.exit('BLAD: pobrany plik %s nie jest obiektem %s' % (sys.argv[1], sys.argv[2]))
print('    %s  %d B' % (sys.argv[1].split('/')[-1], len(d)))
PY
done
echo "[2] wycinam blok decyzji z $CPP i kompiluje"
python3 wytnij_blok.py "$CPP" --ota-blok "$ROB/panel_ota_blok.inc" | sed 's/^/    /'
cat > "$ROB/obrazy.cpp" <<'CXX'
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "panel_ota_blok.inc"
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    FILE* f = fopen(argv[i], "rb");
    if (f == nullptr) return 2;
    PanelOtaSkan s;
    panelOtaSkanStart(s);
    uint8_t kawalek[512];
    size_t n;
    while ((n = fread(kawalek, 1, sizeof(kawalek), f)) > 0) panelOtaSkanDodaj(s, kawalek, n);
    fclose(f);
    const char* powod = panelOtaSkanOcen(s);
    char obraz[17];
    panelOtaSkanObraz(s, obraz);
    printf("%s|%s|%s\n", argv[i], obraz, powod != nullptr ? powod : "");
  }
  return 0;
}
CXX
# -Wno-unused-function: z bloku decyzji ten program wola tylko panelOtaSkan*().
g++ -std=gnu++17 -O1 -Wall -Wextra -Wno-unused-function -I "$ROB" -o "$ROB/obrazy" "$ROB/obrazy.cpp"
PANEL_JEST=0
if [ -f "$PANEL_H" ]; then
  PANEL_JEST=1
  cat > "$ROB/obrazy_panel.cpp" <<'CXX'
#include <stdio.h>
#include "diagnostyka_panelu.h"
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    FILE* f = fopen(argv[i], "rb");
    if (f == nullptr) return 2;
    diag::OtaSkan s;
    diag::otaSkanStart(s);
    uint8_t kawalek[512];
    size_t n;
    while ((n = fread(kawalek, 1, sizeof(kawalek), f)) > 0) diag::otaSkanDodaj(s, kawalek, n);
    fclose(f);
    char obraz[17];
    diag::otaSkanObraz(s, obraz);
    printf("%s|%s|%s\n", argv[i], obraz, diag::otaSkanOcenNaglowek(s) == diag::OTA_POWOD_BRAK ? "" : "odrzucony");
  }
  return 0;
}
CXX
  g++ -std=gnu++17 -O1 -Wall -Wextra -Wno-format-truncation -I "$(dirname "$PANEL_H")" -o "$ROB/obrazy_panel" "$ROB/obrazy_panel.cpp"
  echo "    kod panelu: $PANEL_H"
else
  echo "    (pliku $PANEL_H nie ma - paczka rozpakowana osobno; kod panelu pominiety)"
fi
echo "[3] ocena"
PANEL_JEST="$PANEL_JEST" python3 - "$ROB" <<PY
import hashlib, os, subprocess, sys
rob = sys.argv[1]
pliki = [l.split() for l in """$PLIKI""".strip().split('\n')]
sciezki = [rob + '/' + p[0] for p in pliki]
wynik = subprocess.run([rob + '/obrazy'] + sciezki, capture_output=True, text=True, check=True).stdout.strip().split('\n')
panel = None
if os.environ.get('PANEL_JEST') == '1':
    panel = subprocess.run([rob + '/obrazy_panel'] + sciezki, capture_output=True, text=True, check=True).stdout.strip().split('\n')
bledow = 0
for nr, ((nazwa, _blob, oczekiwane), linia) in enumerate(zip(pliki, wynik)):
    _sciezka, obraz, powod = linia.split('|')
    d = open(rob + '/' + nazwa, 'rb').read()
    uklad = int.from_bytes(d[12:14], 'little')
    suma_ok = hashlib.sha256(d[:-32]).digest() == d[-32:]
    print('    %s: uklad 0x%04X, bajt 23 = %d, SHA-256 pliku bez 32 ostatnich bajtow: %s'
          % (nazwa, uklad, d[23], 'rowne tym 32 bajtom' if suma_ok else 'inne niz te 32 bajty'))
    print('        centrala: %s' % (powod if powod else 'PRZYJETY, identyfikator ' + obraz))
    if d[23] == 1 and not suma_ok:
        bledow += 1
        print('        BLAD: plik zapowiada sume na koncu, a suma sie nie zgadza - zalozenie 1 nie trzyma')
    if oczekiwane == 'przyjety' and (powod or obraz != d[-32:-24].hex() or not suma_ok or uklad != 0):
        bledow += 1
        print('        BLAD: plik dla ESP32 z suma powinien byc przyjety z identyfikatorem %s' % d[-32:-24].hex())
    if oczekiwane == 'bez_sumy' and ('SHA-256' not in powod or d[23] != 0):
        bledow += 1
        print('        BLAD: plik bez sumy powinien byc odrzucony z tego powodu')
    if oczekiwane == 'inny_uklad' and ('ukladu' not in powod or uklad == 0):
        bledow += 1
        print('        BLAD: plik dla innego ukladu powinien byc odrzucony z tego powodu')
    if panel is not None:
        _s, obraz_p, odrzucony_p = panel[nr].split('|')
        print('        panel:    %s' % ('naglowek odrzucony (inny uklad)' if odrzucony_p else
                                        ('naglowek przyjety, identyfikator ' + obraz_p if obraz_p else 'naglowek przyjety, pliku bez sumy nie umie nazwac')))
        dobrze = {'przyjety': (not odrzucony_p and obraz_p == obraz), 'bez_sumy': (not odrzucony_p and obraz_p == ''),
                  'inny_uklad': bool(odrzucony_p)}[oczekiwane]
        if not dobrze:
            bledow += 1
            print('        BLAD: kod panelu ocenia ten plik inaczej niz kod centrali')
print('BEZ BLEDOW: format prawdziwych obrazow zgadza sie z tym, co zaklada kod' if bledow == 0 else 'SA BLEDY: %d' % bledow)
sys.exit(0 if bledow == 0 else 1)
PY
