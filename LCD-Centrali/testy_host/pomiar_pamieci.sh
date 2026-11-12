#!/usr/bin/env bash
# ============================================================================
# Ile pamieci RAM trzyma firmware panelu - pomiar NA PC (bez plytki).
# ============================================================================
#   ./pomiar_pamieci.sh            pomiar + sprawdzenie limitow (kod wyjscia 1, gdy przekroczone)
#   ./pomiar_pamieci.sh --kto      dodatkowo: ktora funkcja firmware'u trzyma ile pamieci
#   ./pomiar_pamieci.sh --wszystko dodatkowo: wszystkie scenariusze symulatora w modelu 32-bitowym
#
# Inna wersja zrodel (same liczby, bez limitow):
#   SRC=/tmp/stary/src LIMITY=0 ./pomiar_pamieci.sh
#
# PO CO: log panelu z 2026-11-12 pokazal, ze panel pracowal z ok. 8 kB wolnego
# RAM-u i nie mial gdzie rozpakowac odpowiedzi centrali ("json=NoMemory").
# Zwykle testy (uruchom_testy.sh) tego nie widza - na PC pamieci jest dosc.
# Ten skrypt buduje firmware jako program 32-bitowy (jak ESP32: wskazniki po
# 4 bajty), podmienia malloc() na wlasny z licznikiem (pomiar_sterty.c) i
# liczy kazdy blok, o ktory firmware prosi - z narzutem sterty ESP-IDF.
#
# Osobno liczy zmienne statyczne firmware'u (patrz STATYCZNE_V120_B).
#
# Od v1.3.0 mierzy tez ekran Wykresy z danymi: ile pamieci przybywa chwilowo,
# gdy panel pobiera i rysuje wykres (z centrali 3.58.4 i po staremu, z calego
# CSV), i czy po kilku wykresach z rzedu cos zostaje. Z opcja --kto sprawdza
# dodatkowo, ze sam kod pobierania nie prosi o pamiec ze sterty.
#
# CZEGO TU NIE MA: pamieci, ktora na plytce biora WiFi, lwIP i zadania
# systemu - to ok. 56 kB (wyliczone z tego samego logu, patrz SYSTEM_B).
# Dlatego "wolny RAM na plytce" na koncu to SZACUNEK oparty na jednym logu;
# prawdziwa liczbe podaje sam panel: "tag=PAMIEC zdarzenie=po_starcie ram=".
#
# Wymaga: python3 z pakietem "ziglang" (pip install ziglang) - to kompilator
# C/C++ potrafiacy budowac programy 32-bitowe bez dodatkowych bibliotek
# systemowych; git; dla --kto takze addr2line (pakiet binutils).
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
TU="$(pwd)"
SRC="${SRC:-$TU/../src}"
INCLUDE="$TU/../include"
PODRECZNY="${LCD_TESTY_CACHE:-${TMPDIR:-/tmp}/lcd_testy_host}"
LIMITY="${LIMITY:-1}"
KTO=0
WSZYSTKO=0
for a in "$@"; do
  case "$a" in
    --kto) KTO=1 ;;
    --wszystko) WSZYSTKO=1 ;;
    *) echo "nieznana opcja: $a"; exit 2 ;;
  esac
done

# --- limity (bajty na stercie ESP32) ----------------------------------------
# Stan v1.2.2: 129 244 B po starcie (v1.1.0: 170 324 B, v1.2.0: 170 572 B),
# 5 588 B chwilowo na jedno odpytanie statusu (v1.2.0: 7 412 B), 0 B
# przyrostu przez 10 minut. Limity zostawiaja kilka kB zapasu na drobne
# zmiany; przekroczenie znaczy, ze doszedl duzy, staly przydzial.
LIMIT_PO_STARCIE_B=134000
LIMIT_CYKL_B=7500
LIMIT_PRZYROST_B=256
# Zmienne statyczne (globalne i "static") firmware'u: licznik sterty ich nie
# widzi, a na plytce zabieraja ten sam RAM (im wiecej ich jest, tym mniejsza
# sterta) i duza tablica statyczna potrafi przepelnic segment danych przy
# linkowaniu (patrz komentarz przy buf1 w src/lvgl_port.cpp). Liczone z
# plikow .o tego modelu: suma sekcji .bss/.data z main.cpp,
# dashboard_lvgl_poc.cpp i lvgl_port.cpp. To przyblizenie - atrapy bibliotek
# maja inne rozmiary obiektow niz prawdziwe - ale ROZNICA miedzy wersjami
# firmware'u jest wiarygodna. v1.2.0 w tym modelu: 3 092 B; v1.2.2: 3 956 B
# (doszedl glownie bufor logBezPamieci(), 640 B); v1.3.0: 4 468 B (doszedl
# staly bufor jednej linii pobieranej historii, 300 B, adres zapytania 64 B i
# stan pobierania - dzieki nim pobieranie wykresu nie bierze nic ze sterty).
# Limit podniesiony z 4 500 na 5 000 B: przy 4 500 v1.3.0 mialoby 32 B zapasu.
STATYCZNE_V120_B=3092
LIMIT_STATYCZNE_B=5000
# Plytka z logu 2026-11-12 (v1.2.0): 234 580 B wolnego RAM-u przed budowa
# ekranow (273 480 B z logu minus 38 900 B obszaru, z ktorego malloc() nie
# korzysta) i ok. 8 100 B po starcie. Ten model dla v1.2.0 daje 170 572 B,
# wiec na WiFi, lwIP i reszte systemu przypada 234 580 - 170 572 - 8 100.
RAM_PRZED_EKRANAMI_B=234580
SYSTEM_B=55900

ZCC="python3 -m ziglang cc -target x86-linux-musl"
ZCXX="python3 -m ziglang c++ -target x86-linux-musl"
if ! python3 -m ziglang version >/dev/null 2>&1; then
  echo "Brak kompilatora 32-bitowego. Zainstaluj:  pip install ziglang"
  exit 2
fi
mkdir -p "$PODRECZNY"
pobierz() {
  if [ ! -d "$PODRECZNY/$1/.git" ]; then
    echo "    pobieram $3 ($2)"
    git clone --quiet --depth 1 --branch "$2" "$3" "$PODRECZNY/$1" 2>/dev/null
  fi
}
echo "[1] biblioteki (LVGL 8.3.11, ArduinoJson 7.2.1)"
pobierz lvgl v8.3.11 https://github.com/lvgl/lvgl.git
pobierz ArduinoJson v7.2.1 https://github.com/bblanchon/ArduinoJson.git

# -fno-omit-frame-pointer: licznik odczytuje stos wywolan po wskaznikach ramek.
# -fno-inline: kazda funkcja firmware'u ma wlasna ramke, wiec w rozbiciu
#   "kto trzyma pamiec" (--kto) blok jest przypisany funkcji, ktora naprawde o
#   niego poprosila, a nie tej, w ktora kompilator ja wbudowal. Bez tego
#   sprawdzenie [4b] moglo przeoczyc przydzial w kodzie pobierania wykresu.
#   Na liczby pomiaru (bajty na stercie) nie ma to wplywu.
# -funwind-tables (LVGL): atrapa ESP.restart() rzuca wyjatek przez kod C.
OPT="-O1 -g -fno-inline -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer"
ODCISK="$(cksum < "$INCLUDE/lv_conf.h" | tr -d ' ')"
LIB="$PODRECZNY/liblvgl32_$ODCISK.a"
if [ ! -f "$LIB" ]; then
  echo "    buduje LVGL 32-bit (jednorazowo)"
  OBJ="$PODRECZNY/obj32_$ODCISK"
  mkdir -p "$OBJ"
  (cd "$PODRECZNY/lvgl" && find src -name '*.c') | while read -r plik; do
    echo "$ZCC -c $OPT -funwind-tables -w -DLV_CONF_INCLUDE_SIMPLE -I$INCLUDE -I$PODRECZNY/lvgl -o $OBJ/$(echo "$plik" | tr '/.' '__').o $PODRECZNY/lvgl/$plik"
  done | xargs -P "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)" -I{} sh -c "{}"
  ar rcs "$LIB" "$OBJ"/*.o
fi

echo "[2] budowanie 32-bitowego modelu firmware'u (zrodla: $SRC)"
BUD="$PODRECZNY/bud32_$(echo "$SRC" | cksum | tr -d ' ')"
WYNIKI="${WYNIKI:-$PODRECZNY/wyniki32}"
mkdir -p "$BUD" "$WYNIKI"
ATRAPA_HASEL="$PODRECZNY/atrapa_hasel"
mkdir -p "$ATRAPA_HASEL"
printf '#pragma once\n#define SECRET_WIFI_AP_PASSWORD "haslo-testowe"\n#define SECRET_OTA_HASLO "haslo-ota-testowe"\n' > "$ATRAPA_HASEL/secrets.h"
# -Wno-c++11-narrowing: clang traktuje jako blad zawezenie int->int16_t w
# klamrach, ktore GCC (kompilator ESP32) tylko zglasza ostrzezeniem.
WSPOLNE="-std=gnu++17 $OPT -Wno-c++11-narrowing -DTFT_BL=21 -DLV_CONF_INCLUDE_SIMPLE -DPOMIAR_STERTY \
  -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 \
  -I $TU/mock -I $INCLUDE -I $PODRECZNY/ArduinoJson/src -I $PODRECZNY/lvgl -I $SRC -I $ATRAPA_HASEL"
$ZCXX $WSPOLNE -w -c -o "$BUD/main.o" "$SRC/main.cpp" &
$ZCXX $WSPOLNE -w -c -o "$BUD/dash.o" "$SRC/dashboard_lvgl_poc.cpp" &
BLEDY=0
for zadanie in $(jobs -p); do wait "$zadanie" || BLEDY=1; done
$ZCXX $WSPOLNE -w -c -o "$BUD/port.o" "$SRC/lvgl_port.cpp" &
$ZCXX $WSPOLNE -w -c -o "$BUD/atrapa.o" mock/atrapa.cpp &
$ZCXX $WSPOLNE -w -c -o "$BUD/sym.o" symulator_panelu.cpp &
$ZCC $OPT -c -o "$BUD/pomiar.o" pomiar_sterty.c &
for zadanie in $(jobs -p); do wait "$zadanie" || BLEDY=1; done
if [ "$BLEDY" != 0 ]; then echo "    BLAD KOMPILACJI (patrz komunikaty wyzej)"; exit 1; fi
$ZCXX $OPT -o "$BUD/symulator32" "$BUD"/main.o "$BUD"/dash.o "$BUD"/port.o "$BUD"/atrapa.o "$BUD"/sym.o "$BUD"/pomiar.o \
  "$LIB" -Wl,--wrap=connect,--wrap=send,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free -lpthread
echo "    OK"

# Ekran Wykresy: odpowiedzi centrali i punkty z v1.2.2 (jak w uruchom_testy.sh).
export LCD_WZORCE_CENTRALI="$TU/wzorce_centrali"
export LCD_WZORCE_V122="$TU/wzorce_v122"

echo "[3] pomiar"
"$BUD/symulator32" pomiar_pamieci --obserwacje --wyjscie "$WYNIKI" > "$WYNIKI/pomiar.txt"
WERDYKT=0
python3 - "$WYNIKI/pomiar.txt" "$LIMITY" "$LIMIT_PO_STARCIE_B" "$LIMIT_CYKL_B" "$LIMIT_PRZYROST_B" "$RAM_PRZED_EKRANAMI_B" "$SYSTEM_B" \
  "$STATYCZNE_V120_B" "$LIMIT_STATYCZNE_B" "$BUD/main.o" "$BUD/dash.o" "$BUD/port.o" <<'PY' || WERDYKT=1
import sys, re, struct
plik, limity, lim_start, lim_cykl, lim_przyrost, ram_przed, system, stat_v120, lim_stat = sys.argv[1], sys.argv[2] == '1', *map(int, sys.argv[3:10])

def statyczne(plik_o):
    """Suma rozmiarow sekcji ze zmiennymi (.bss*, .data*, .tbss*, .tdata*) w pliku .o (ELF 32-bit)."""
    d = open(plik_o, 'rb').read()
    if d[:4] != b'\x7fELF' or d[4] != 1 or d[5] != 1:
        raise ValueError('%s: to nie jest 32-bitowy plik ELF' % plik_o)
    shoff = struct.unpack_from('<I', d, 0x20)[0]
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2E)
    def sekcja(i): return struct.unpack_from('<10I', d, shoff + i * shentsize)   # name, type, flags, addr, offset, size, link, ...
    if shnum == 0: shnum = sekcja(0)[5]            # bardzo duzo sekcji: prawdziwa liczba jest w sekcji 0
    if shstrndx == 0xFFFF: shstrndx = sekcja(0)[6]
    nazwy = sekcja(shstrndx)[4]
    suma = 0
    for i in range(1, shnum):
        s = sekcja(i)
        nazwa = d[nazwy + s[0]:d.index(b'\0', nazwy + s[0])].decode('ascii', 'replace')
        if nazwa.startswith('.data.rel.ro'): continue   # stale ze wskaznikami: na plytce leza we flashu
        if nazwa.startswith(('.bss', '.data', '.tbss', '.tdata')): suma += s[5]
    return suma
m = {}
for l in open(plik):
    t = re.match(r'POMIAR (\S+) zywe=(\d+) blokow=(\d+) szczyt=(\d+)', l)
    if t: m[t.group(1)] = tuple(map(int, t.groups()[1:]))
if 'po_setup' not in m or 'po_10min' not in m:
    print('    BLAD: brak wynikow pomiaru (patrz %s)' % plik); sys.exit(1)
def b(n): return format(n, ',').replace(',', ' ')
start, bloki, _ = m['po_setup']
cykl = m['cykl_10s'][2] - m['cykl_10s'][0]
przyrost = m['po_10min'][0] - m['po_setup'][0]
print('    po starcie firmware trzyma        %9s B w %d blokach' % (b(start), bloki))
print('    jedno odpytanie centrali, chwilowo %8s B wiecej' % b(cykl))
print('    przyrost po 10 minutach pracy     %9s B' % b(przyrost))
if 'alarm_przegrzania' in m: print('    ekran alarmu                      %9s B wiecej' % b(m['alarm_przegrzania'][0] - m['po_10min'][0]))
if 'wykresy_otwarte' in m:
    print('    ekran Wykresy otwarty             %9s B wiecej' % b(m['wykresy_otwarte'][0] - m['po_alarmie'][0]))
    print('    ...i po jego zamknieciu           %9s B wiecej' % b(m['wykresy_zamkniete'][0] - m['po_alarmie'][0]))
wykres_nowy = wykres_stary = wykres_przyrost = None
if 'wykres_nowy' in m:
    # szczyt = najwiecej naraz od poprzedniej migawki; roznica to pamiec brana CHWILOWO w sekundach
    # pobierania i rysowania - razem z odpytaniem /api/status, jesli wypadlo w tym samym czasie.
    wykres_nowy = m['wykres_nowy'][2] - m['wykres_przed'][0]
    wykres_stary = m['wykres_stary'][2] - m['wykres_stary_przed'][0]
    wykres_przyrost = m['wykres_powtorki'][0] - m['wykres_po_zamknieciu'][0]
    print('    wykres z centrali 3.58.4 (pobranie i rysowanie), chwilowo %s B wiecej' % b(wykres_nowy))
    print('    wykres po staremu (caly CSV, 352 kB), chwilowo        %s B wiecej' % b(wykres_stary))
    print('    po pierwszym wykresie zostaje     %9s B (opisy osi i legenda, tworzone raz)' % b(m['wykres_po_zamknieciu'][0] - m['po_alarmie'][0]))
    print('    przyrost po 5 kolejnych wykresach %9s B' % b(wykres_przyrost))
    for l in open(plik):
        if l.startswith('POMIAR-INFO') and ('widoczny=0' in l or 'punktow=0' in l):
            print('    BLAD: w pomiarze wykres sie nie narysowal: ' + l.strip()); sys.exit(1)
stat = sum(statyczne(o) for o in sys.argv[10:13])
print('    zmienne statyczne firmware\'u      %9s B (v1.2.0: %s B)' % (b(stat), b(stat_v120)))
wolne = ram_przed - system - start - (stat - stat_v120)
print('    szacunek dla plytki z logu 2026-11-12: po starcie zostaje ok. %s B wolnego RAM-u' % b(wolne))
roznica = stat - stat_v120
print('      (%s B przed budowa ekranow - %s B WiFi i system - %s B firmware %s;' % (b(ram_przed), b(system), b(start),
      ('- %s B zmiennych statycznych ponad v1.2.0' % b(roznica)) if roznica >= 0 else ('+ %s B, bo zmiennych statycznych jest mniej niz w v1.2.0' % b(-roznica))))
print('       v1.2.0 na plytce: ok. 8 100 B)')
if not limity:
    print('    (bez sprawdzania limitow)'); sys.exit(0)
zle = []
if start > lim_start: zle.append('po starcie %s B > limit %s B' % (b(start), b(lim_start)))
if cykl > lim_cykl: zle.append('odpytanie centrali %s B > limit %s B' % (b(cykl), b(lim_cykl)))
if przyrost > lim_przyrost: zle.append('przyrost w czasie pracy %s B > limit %s B (wyciek pamieci?)' % (b(przyrost), b(lim_przyrost)))
if stat > lim_stat: zle.append('zmienne statyczne %s B > limit %s B (duza tablica globalna albo "static"?)' % (b(stat), b(lim_stat)))
if wykres_nowy is not None:
    # Pobieranie wykresu nie moze brac wiecej niz jedno odpytanie statusu (czyta linia po linii do stalego bufora).
    if wykres_nowy > lim_cykl: zle.append('wykres z centrali %s B > limit %s B' % (b(wykres_nowy), b(lim_cykl)))
    if wykres_stary > lim_cykl: zle.append('wykres po staremu %s B > limit %s B' % (b(wykres_stary), b(lim_cykl)))
    if wykres_przyrost > lim_przyrost: zle.append('przyrost po kolejnych wykresach %s B > limit %s B (wyciek pamieci?)' % (b(wykres_przyrost), b(lim_przyrost)))
if zle:
    for z in zle: print('    PRZEKROCZONY LIMIT: ' + z)
    sys.exit(1)
print('    LIMITY: w normie (po starcie <= %s B, odpytanie <= %s B, przyrost <= %s B, statyczne <= %s B)' % (b(lim_start), b(lim_cykl), b(lim_przyrost), b(lim_stat)))
PY

if [ "$KTO" = "1" ]; then
  echo "[4] kto trzyma pamiec po starcie (funkcja firmware'u, ktora o nia poprosila)"
  ILE=25 python3 pomiar_rozbicie.py "$BUD/symulator32" "$WYNIKI/pamiec_po_setup.txt" | sed 's/^/    /'
  if [ -f "$WYNIKI/pamiec_wykres_nowy_szczyt.txt" ]; then
    # Odbior danych wykresu (StrumienHttpGet, obslugaWykresyFetch, ..._strumien_linia)
    # nie powinien prosic o pamiec wcale - czyta do stalego bufora.
    echo "[4b] czy kod pobierania wykresu prosil o pamiec ze sterty"
    for zrzut in wykres_nowy_szczyt wykres_stary_szczyt; do
      ILE=1000 python3 pomiar_rozbicie.py "$BUD/symulator32" "$WYNIKI/pamiec_$zrzut.txt" > "$WYNIKI/rozbicie_$zrzut.txt"
      if grep -E 'StrumienHttpGet|obslugaWykresyFetch|wykresyFetchStart|wykresyZakoncz|wykresyLinia|wykresy_strumien_(start|linia|koniec|zamknij|scal)' "$WYNIKI/rozbicie_$zrzut.txt" | sed 's/^/    /'; then
        echo "    UWAGA ($zrzut): powyzsze funkcje pobierania wykresu trzymaly bloki sterty"
        WERDYKT=1
      else
        echo "    $zrzut: nie - zaden blok nie pochodzi z kodu pobierania"
      fi
    done
  fi
fi

if [ "$WSZYSTKO" = "1" ]; then
  echo "[5] wszystkie scenariusze w modelu 32-bitowym"
  NIEZALICZONE=0
  for scenariusz in $("$BUD/symulator32" --lista); do
    [ "$scenariusz" = "pomiar_pamieci" ] && continue
    "$BUD/symulator32" "$scenariusz" --wyjscie "$WYNIKI" > "$WYNIKI/$scenariusz.txt" 2>&1 || { NIEZALICZONE=$((NIEZALICZONE + 1)); echo "    NIEZALICZONY: $scenariusz (patrz $WYNIKI/$scenariusz.txt)"; }
  done
  if [ "$NIEZALICZONE" = 0 ]; then echo "    WSZYSTKIE SCENARIUSZE ZALICZONE"; else WERDYKT=1; fi
fi
exit $WERDYKT
