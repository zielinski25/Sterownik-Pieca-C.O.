#!/usr/bin/env bash
# ============================================================================
# Ile pamieci RAM trzyma firmware panelu - pomiar NA PC (bez plytki).
# ============================================================================
#   ./pomiar_pamieci.sh            pomiar + sprawdzenie limitow (kod wyjscia 1, gdy przekroczone)
#   ./pomiar_pamieci.sh --kto      dodatkowo: ktora funkcja firmware'u trzyma ile pamieci
#   ./pomiar_pamieci.sh --wszystko dodatkowo: wszystkie scenariusze symulatora w modelu 32-bitowym
#   ./pomiar_pamieci.sh --przypadki dodatkowo: czy RAM-u starczy w kazdym przypadku pracy
#                                  (kazdy ekran, wykresy na wszystkie sposoby, bledy, doba pracy,
#                                  od v1.4.0 takze aktualizacja panelu przez centrale)
#                                  i co robi panel, gdy wolnego RAM-u jest mniej (ok. 2 minut)
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
# Opcja --przypadki (2026-11-12) odpowiada na pytanie "czy RAM-u starczy":
#   [6] dla kazdego przypadku pracy podaje, ile pamieci firmware bierze NARAZ
#       ponad stan po starcie, i przelicza to na wolny RAM plytki z logu
#       (stale RAM_PO_STARCIE_V122_B i SIEC_* nizej);
#   [7] daje stercie modelu limit - jakby plytka miala mniej wolnego RAM-u - i
#       pokazuje, od ilu bajtow wszystko dziala bez jednej odmowy malloc(), a
#       co sie dzieje ponizej.
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
PRZYPADKI=0
for a in "$@"; do
  case "$a" in
    --kto) KTO=1 ;;
    --wszystko) WSZYSTKO=1 ;;
    --przypadki) PRZYPADKI=1 ;;
    *) echo "nieznana opcja: $a"; exit 2 ;;
  esac
done

# --- limity (bajty na stercie ESP32) ----------------------------------------
# Stan v1.2.2: 129 244 B po starcie (v1.1.0: 170 324 B, v1.2.0: 170 572 B),
# 5 384 B chwilowo na jedno odpytanie statusu (v1.2.0: 7 208 B), 0 B
# przyrostu przez 10 minut. Limity zostawiaja kilka kB zapasu na drobne
# zmiany; przekroczenie znaczy, ze doszedl duzy, staly przydzial.
# (Przy wydaniu v1.3.0 model liczyl kazde zmniejszenie bloku przez realloc()
# jak nowy przydzial i podawal szczyty o ok. 0,2 kB wyzsze: 5 588 B na
# odpytanie, v1.2.0: 7 412 B. Sterta ESP-IDF zmniejsza blok w miejscu - patrz
# realloc() w pomiar_sterty.c. Stad nieco wyzsze liczby w komentarzu wersji
# v1.3.0 w src/main.cpp: 2,9 i 5,6 kB zamiast 2,5 i 5,4 kB.)
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
# v1.4.0: 5 366 B. Doszlo 898 B na aktualizacje przez centrale: drugi obiekt
# pobierania (otaFetch, 488 B - osobny od pobierania wykresow, zeby jedno nie
# moglo przerwac drugiego), stan aktualizacji (152 B), kopia zapisu z NVS
# (64 B), identyfikator wlasnego programu i powod czekania (33 B); reszta to
# drobne zmienne i obiekty atrap (Update, partycja - 40 B). Tak jak przy
# wykresach: dzieki nim samo pobieranie nie bierze nic ze sterty (bufor
# sektora, 4 096 B, bierze biblioteka Update na czas zapisu - patrz przypadki
# "aktualizacja panelu" w [6]). Limit podniesiony z 5 000 na 6 000 B.
STATYCZNE_V120_B=3092
LIMIT_STATYCZNE_B=6000
# Plytka z logu 2026-11-12 (v1.2.0): 234 580 B wolnego RAM-u przed budowa
# ekranow (273 480 B z logu minus 38 900 B obszaru, z ktorego malloc() nie
# korzysta) i ok. 8 100 B po starcie. Ten model dla v1.2.0 daje 170 572 B,
# wiec na WiFi, lwIP i reszte systemu przypada 234 580 - 170 572 - 8 100.
RAM_PRZED_EKRANAMI_B=234580
SYSTEM_B=55900

# --- czy RAM-u starczy (--przypadki) ----------------------------------------
# Plytka z logu 2026-11-12 po wgraniu v1.2.2 (jedyny pomiar na sprzecie):
#   tag=PAMIEC zdarzenie=po_starcie ram=49676 ram_blok=42996
#   w spoczynku ram=50076..50516, najmniej w 30 s odpytywania ram_min=41644.
# Ten model dla v1.2.2: 129 244 B po starcie, 3 956 B zmiennych statycznych.
# Wolny RAM plytki dla sprawdzanych zrodel = 49 676 minus to, o ile wiecej
# trzymaja po starcie i w zmiennych statycznych niz v1.2.2.
RAM_PO_STARCIE_V122_B=49676
PO_STARCIE_V122_MODEL_B=129244
STATYCZNE_V122_B=3956
# Pamiec, ktora na plytce bierze CHWILOWO siec (WiFi i lwIP) - model jej nie
# liczy, wiec jest odejmowana osobno:
#   SIEC_ODPYTANIA_B - odpytania /api/status i /api/ustawienia-get. Z logu:
#     spoczynek 50 076..50 516 B, najmniej 41 644 B, czyli dolek 8,4-8,9 kB;
#     model daje na odpytanie 5 384 B, reszta (3,0-3,5 kB) to siec.
#   SIEC_WYKRES_B - pobieranie wykresu. Nie zmierzone na plytce (v1.3.0 nie
#     bylo na niej uruchamiane); gorna granica z ustawien lwIP w arduino-esp32:
#     okno odbioru TCP to 5 760 B (CONFIG_LWIP_TCP_WND_DEFAULT), czyli najwyzej
#     4 ramki po 1 436 B danych czekaja na odczyt, kazda w buforze WiFi ok.
#     1,6 kB, plus ok. 1 kB na samo polaczenie. Dotyczy tez calego CSV ze
#     starszej centrali - nadawca nie moze wyslac wiecej, niz miesci okno.
#     Ta sama granica jest brana dla pobierania pliku aktualizacji (v1.4.0):
#     to takie samo polaczenie, tylko dluzsze.
SIEC_ODPYTANIA_B=3500
SIEC_WYKRES_B=7500
# Limity: najwiecej pamieci naraz ponad stan po starcie (v1.3.0: 25 416 B,
# v1.4.0: 25 496 B) i najmniej wolnego RAM-u, jaki moze zostac na plytce w
# najgorszej chwili.
LIMIT_NAJGORSZY_B=27000
LIMIT_ZAPAS_PLYTKI_B=8000
# Wolny RAM (dla firmware'u, po starcie), dla ktorego [7] sprawdza zachowanie.
LISTA_MALO_RAMU="40000 30000 26000 24000 22000 20000 18000 16000 12000 8000 4000"

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
with open(plik.rsplit('/', 1)[0] + '/liczby.txt', 'w') as f:
    f.write('po_starcie=%d\nstatyczne=%d\ncykl=%d\n' % (start, stat, cykl))
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
    case "$scenariusz" in pomiar_*) continue ;; esac   # pomiary maja wlasne kroki ([3], [6], [7])
    "$BUD/symulator32" "$scenariusz" --wyjscie "$WYNIKI" > "$WYNIKI/$scenariusz.txt" 2>&1 || { NIEZALICZONE=$((NIEZALICZONE + 1)); echo "    NIEZALICZONY: $scenariusz (patrz $WYNIKI/$scenariusz.txt)"; }
  done
  if [ "$NIEZALICZONE" = 0 ]; then echo "    WSZYSTKIE SCENARIUSZE ZALICZONE"; else WERDYKT=1; fi
fi

if [ "$PRZYPADKI" = "1" ]; then
  echo "[6] czy RAM-u starczy: przypadki pracy (model 32-bitowy; trwa ok. 2 minut)"
  if ! "$BUD/symulator32" pomiar_przypadki --obserwacje --wyjscie "$WYNIKI" > "$WYNIKI/przypadki.txt" 2>&1; then
    echo "    BLAD: model zakonczyl sie awaria (patrz $WYNIKI/przypadki.txt)"
    WERDYKT=1
  fi
  # Aktualizacja panelu przez centrale (v1.4.0): osobny przebieg, bo konczy sie restartem panelu.
  if ! "$BUD/symulator32" pomiar_ota --obserwacje --wyjscie "$WYNIKI" > "$WYNIKI/przypadki_ota.txt" 2>&1; then
    echo "    BLAD: model zakonczyl sie awaria (patrz $WYNIKI/przypadki_ota.txt)"
    WERDYKT=1
  fi
  # [7]: ten sam ciag czynnosci z limitem sterty: lista do tabeli, co 250 B od 0 do 32 kB
  # (przedzialy zachowan, szukanie awarii modelu) i co 100 B wokol progu.
  for F in $LISTA_MALO_RAMU $(seq 0 250 32000) $(seq 23000 100 27000); do
    [ -f "$WYNIKI/malo_ramu_$F.txt.nowy" ] && continue
    : > "$WYNIKI/malo_ramu_$F.txt.nowy"
    LCD_WOLNY_RAM_B=$F "$BUD/symulator32" pomiar_malo_ramu --obserwacje > "$WYNIKI/malo_ramu_$F.txt" 2>&1 || true
  done
  rm -f "$WYNIKI"/malo_ramu_*.txt.nowy
  python3 - "$WYNIKI" "$LIMITY" "$RAM_PO_STARCIE_V122_B" "$PO_STARCIE_V122_MODEL_B" "$STATYCZNE_V122_B" "$SIEC_ODPYTANIA_B" "$SIEC_WYKRES_B" \
    "$LIMIT_NAJGORSZY_B" "$LIMIT_ZAPAS_PLYTKI_B" "$LIMIT_PRZYROST_B" "$BUD/symulator32" "$LISTA_MALO_RAMU" <<'PY' || WERDYKT=1
import sys, re, os, subprocess
kat, limity = sys.argv[1], sys.argv[2] == '1'
ram_v122, start_v122, stat_v122, siec_odp, siec_wykres, lim_najg, lim_zapas, lim_przyrost = map(int, sys.argv[3:11])
program, lista = sys.argv[11], [int(x) for x in sys.argv[12].split()]
def b(n): return format(n, ',').replace(',', ' ')
liczby = dict(l.strip().split('=') for l in open(kat + '/liczby.txt'))
start, stat = int(liczby['po_starcie']), int(liczby['statyczne'])
# Wolny RAM plytki po starcie dla tych zrodel (v1.2.2 zmierzone na plytce).
ram_plytki = ram_v122 - (start - start_v122) - (stat - stat_v122)
zle = []

# ---------------------------------------------------------------- [6] --------
r1, r2, uwagi, bledy, przyrost = {}, {}, {}, [], None
linie_przypadkow = list(open(kat + '/przypadki.txt', errors='replace'))
# aktualizacja przez centrale: osobny przebieg modelu; zrodla starsze niz v1.4.0 jej nie maja
linie_ota = list(open(kat + '/przypadki_ota.txt', errors='replace')) if os.path.exists(kat + '/przypadki_ota.txt') else []
bez_ota = any(l.startswith('PRZYPADEK-INFO ota=brak') for l in linie_ota)
for l in linie_przypadkow + linie_ota:
    m = re.match(r'(PRZYPADEK2?) (\S+) szczyt=(-?\d+) po=(-?\d+) ?(.*)', l)
    if m:
        (r1 if m.group(1) == 'PRZYPADEK' else r2)[m.group(2)] = (int(m.group(3)), int(m.group(4)))
        if m.group(1) == 'PRZYPADEK': uwagi[m.group(2)] = m.group(5)
    if l.startswith('PRZYPADEK-BLAD') or l.startswith('MODEL-AWARIA'): bledy.append(l.strip())
    m = re.match(r'PRZYPADEK-PRZYROST po_pierwszej_rundzie=(-?\d+) druga_runda=(-?\d+)', l)
    if m: przyrost = (int(m.group(1)), int(m.group(2)))
def pole(nazwa, klucz):
    m = re.search(r'\b%s=(-?\d+)' % klucz, uwagi.get(nazwa, ''))
    return int(m.group(1)) if m else None
# (nazwa w modelu, opis, czy w trakcie idzie pobieranie (wykresu albo pliku aktualizacji), czego oczekujemy: 'wykres' / 'napis' / None)
OPIS = [
    ('praca_10_min', 'praca bez dotyku, 10 minut', False, None),
    ('praca_24_h', 'praca bez dotyku, doba', False, None),
    ('alarm_przegrzania', 'ekran alarmu', False, None),
    ('*ekrany', 'pozostale ekrany, kazdy po kolei', False, None),
    ('polecenie_z_dotyku', 'polecenie z dotyku (POMPA: WLACZ)', False, None),
    ('wykresy_otwarte', 'ekran Wykresy otwarty, bez wykresu', False, None),
    ('wykres_godzina', 'wykres godzinny, 8 serii', True, 'wykres'),
    ('wykres_dzien', 'wykres dzienny, 8 serii', True, 'wykres'),
    ('wykres_tydzien_pelne_bufory', 'wykres tygodniowy, pelna historia centrali', True, 'wykres'),
    ('wykres_tydzien_z_przerwa', 'wykres tygodniowy z przerwa', True, 'wykres'),
    ('wykres_na_ekranie_10_s', 'wykres na ekranie, 10 s', False, None),
    ('napis_same_przerwy', 'centrala po restarcie: napis zamiast wykresu', True, 'napis'),
    ('napis_centrala_bez_historii', 'centrala bez historii: napis', True, 'napis'),
    ('napis_jeden_pomiar', 'jeden pomiar: napis', True, 'napis'),
    ('blad_centrala_503', 'centrala bez pamieci (503): napis', True, 'napis'),
    ('blad_strona_html', 'odpowiedz, ktora nie jest historia: napis', True, 'napis'),
    ('blad_odpowiedz_urwana', 'odpowiedz urwana: napis', True, 'napis'),
    ('blad_centrala_zamilkla', 'centrala zamilkla w polowie: napis', True, 'napis'),
    ('wykres_wolne_lacze_2_kB_s', 'wolne lacze (wykres idzie 9 s)', True, 'wykres'),
    ('wykres_wolne_lacze_limit_20_s', 'bardzo wolne lacze (limit 20 s): napis', True, 'napis'),
    ('wykres_najgorszy_zbieg', 'wykres w 80 roznych chwilach wzgledem odpytan', True, None),
    ('alarm_w_trakcie_pobierania', 'alarm w trakcie pobierania wykresu', True, None),
    ('stara_centrala_doba_caly_csv', 'starsza centrala: caly CSV doby (100 kB)', True, 'wykres'),
    ('stara_centrala_tydzien_caly_csv', 'starsza centrala: caly CSV tygodnia (8000 wierszy)', True, 'wykres'),
    ('stara_centrala_tydzien_limit_20_s', 'starsza centrala: CSV nie zdazyl w 20 s', True, 'wykres'),
    ('wykresy_30_z_rzedu', '30 wykresow z rzedu', True, None),
    ('wykresy_300_z_rzedu', '300 wykresow z rzedu', True, None),
    ('aktualizacja_przerwana', 'aktualizacja panelu przerwana w polowie', True, None),
    ('aktualizacja_do_konca', 'aktualizacja panelu: caly plik 1,2 MB', True, None),
]
# Przypadki poza ekranem Wykresy (do zdania "bez otwartych Wykresow najmniej ...").
BEZ_WYKRESOW = ('praca_10_min', 'praca_24_h', 'alarm_przegrzania', '*ekrany', 'polecenie_z_dotyku', 'aktualizacja_przerwana', 'aktualizacja_do_konca')
ekrany = {k: v for k, v in r1.items() if k.startswith('ekran_')}
if ekrany: r1['*ekrany'] = (max(v[0] for v in ekrany.values()), max(v[1] for v in ekrany.values()))
print('    plytka z logu 2026-11-12: po starcie %s B wolnego RAM-u (v1.2.2 zmierzone: %s B; te zrodla trzymaja o %s B %s)' % (
    b(ram_plytki), b(ram_v122), b(abs(ram_v122 - ram_plytki)), 'wiecej' if ram_plytki <= ram_v122 else 'mniej'))
print('    siec plytki (model jej nie liczy): odpytania centrali do %s B, pobieranie wykresu do %s B' % (b(siec_odp), b(siec_wykres)))
print('')
print('    %-52s %9s %9s %12s' % ('przypadek', 'trzyma B', 'naraz B', 'na plytce B'))
najg, najg_nazwa, najmniej_plytka, najmniej_bez_wykresow, najmniej_bez_wykresow_nazwa = 0, '', None, None, ''
for nazwa, opis, pobieranie, oczek in OPIS:
    if nazwa not in r1:
        if nazwa.startswith('aktualizacja_') and bez_ota: continue   # zrodla starsze niz v1.4.0
        zle.append('brak wyniku przypadku "%s"' % opis); continue
    szczyt, po = r1[nazwa]
    wolne = ram_plytki - szczyt - siec_odp - (siec_wykres if pobieranie else 0)
    # po calej aktualizacji panel sie restartuje - "trzyma" nie ma wtedy znaczenia
    print('    %-52s %9s %9s %12s' % (opis, 'restart' if nazwa == 'aktualizacja_do_konca' else b(po), b(szczyt), b(wolne)))
    if szczyt > najg: najg, najg_nazwa = szczyt, opis
    if najmniej_plytka is None or wolne < najmniej_plytka: najmniej_plytka = wolne
    if nazwa in BEZ_WYKRESOW and (najmniej_bez_wykresow is None or wolne < najmniej_bez_wykresow): najmniej_bez_wykresow, najmniej_bez_wykresow_nazwa = wolne, opis
    if oczek == 'wykres' and not (pole(nazwa, 'wykres') == 1 and pole(nazwa, 'serii') == 8):
        zle.append('przypadek "%s": wykresu z 8 seriami nie ma na ekranie' % opis)
    if oczek == 'napis' and not (pole(nazwa, 'wykres') == 0 and 'napis=""' not in uwagi.get(nazwa, '')):
        zle.append('przypadek "%s": mial byc napis zamiast wykresu' % opis)
print('      trzyma  = ile pamieci ponad stan po starcie firmware trzyma po tym przypadku (w tym jednorazowe opisy osi i legenda)')
print('      naraz   = najwiecej pamieci ponad stan po starcie w ktorejkolwiek chwili przypadku')
print('      plytka  = wolny RAM plytki w tej chwili: %s - naraz - siec (SZACUNEK; prawdziwa liczba: "ram_min=" w logu panelu)' % b(ram_plytki))
for k in ('wykresy_30_z_rzedu', 'wykresy_300_z_rzedu'):
    if k in r1 and (pole(k, 'narysowanych') or 0) < int(k.split('_')[1]): zle.append('%s: narysowanych %s' % (k, pole(k, 'narysowanych')))
print('')
print('    najwiecej naraz: %s B (%s)' % (b(najg), najg_nazwa))
print('    na plytce zostaje wtedy ok. %s B wolnego RAM-u; bez otwartych Wykresow najmniej ok. %s B (%s)' % (
    b(najmniej_plytka), b(najmniej_bez_wykresow if najmniej_bez_wykresow is not None else 0), najmniej_bez_wykresow_nazwa))
# Aktualizacja przez centrale (v1.4.0): musi dojsc do konca, a przerwana nie moze nic zostawic.
if 'aktualizacja_do_konca' in r1:
    if not (pole('aktualizacja_do_konca', 'uruchomiono') == 1 and pole('aktualizacja_do_konca', 'caly_plik') == 1 and pole('aktualizacja_do_konca', 'restart') == 1):
        zle.append('aktualizacja panelu w modelu nie doszla do konca: %s' % uwagi.get('aktualizacja_do_konca', ''))
    if pole('aktualizacja_do_konca', 'wdt') not in (0, None): zle.append('w trakcie aktualizacji zadzialal watchdog')
if 'aktualizacja_przerwana' in r1:
    if not (pole('aktualizacja_przerwana', 'nakladka') == 1 and pole('aktualizacja_przerwana', 'nakladka_po') == 0 and pole('aktualizacja_przerwana', 'porzucono') == 1
            and (pole('aktualizacja_przerwana', 'przerwana_ms') or -1) >= 0):
        zle.append('przerwana aktualizacja w modelu nie przebiegla jak trzeba: %s' % uwagi.get('aktualizacja_przerwana', ''))
    print('    po przerwanej aktualizacji zostaje %s B ponad stan sprzed niej (bufor zapisu i nakladka z paskiem postepu sa zwalniane)' % b(r1['aktualizacja_przerwana'][1]))
    if r1['aktualizacja_przerwana'][1] > lim_przyrost: zle.append('po przerwanej aktualizacji zostaje %s B > limit %s B (wyciek pamieci?)' % (b(r1['aktualizacja_przerwana'][1]), b(lim_przyrost)))
if przyrost is not None:
    print('    po pierwszym przejsciu wszystkich przypadkow zostaje %s B (tworzone raz), po drugim przybywa %s B' % (b(przyrost[0]), b(przyrost[1])))
    if przyrost[1] > lim_przyrost: zle.append('druga runda tych samych czynnosci: przybywa %s B > limit %s B (wyciek pamieci?)' % (b(przyrost[1]), b(lim_przyrost)))
else:
    zle.append('brak wyniku drugiej rundy')
for k, t in (('praca_24_h', 'doba pracy'), ('wykresy_300_z_rzedu', '300 wykresow'), ('koniec_na_pulpicie', 'calosc')):
    klucz = 'przyrost_od_drugiej_rundy' if k == 'koniec_na_pulpicie' else 'przyrost'
    if pole(k, klucz) is not None and pole(k, klucz) > lim_przyrost: zle.append('%s: przybywa %s B > limit %s B (wyciek pamieci?)' % (t, b(pole(k, klucz)), b(lim_przyrost)))
if pole('koniec_na_pulpicie', 'restart') not in (0, None) or pole('koniec_na_pulpicie', 'wdt') not in (0, None): zle.append('w trakcie przypadkow panel sie zrestartowal albo zadzialal watchdog')
# Druga runda: te same szczyty (z dokladnoscia do jednorazowych przydzialow pierwszej rundy).
for k, v in r1.items():
    if k in r2 and r2[k][0] > v[0] + (przyrost[0] if przyrost else 0) + 256: zle.append('druga runda, przypadek %s: szczyt %s B wobec %s B w pierwszej' % (k, b(r2[k][0]), b(v[0])))
for x in bledy: zle.append(x)
if najg > lim_najg: zle.append('najwiecej naraz %s B > limit %s B' % (b(najg), b(lim_najg)))
if najmniej_plytka is not None and najmniej_plytka < lim_zapas: zle.append('na plytce zostaje w najgorszej chwili %s B < limit %s B' % (b(najmniej_plytka), b(lim_zapas)))

# ---------------------------------------------------------------- [7] --------
print('[7] co robi panel, gdy wolnego RAM-u jest mniej (limit sterty w modelu)')
def wynik_malo(F):
    sciezka = '%s/malo_ramu_%d.txt' % (kat, F)
    if not os.path.exists(sciezka): return None
    e, awaria = {}, None
    for l in open(sciezka, errors='replace'):
        if l.startswith('MODEL-AWARIA'): awaria = l.strip()
        m = re.match(r'MALO-RAMU wolne=\d+ (?:etap=(\S+)|(koniec)) (.*)', l)
        if m: e[m.group(1) or 'koniec'] = m.group(3)
    def p(etap, klucz):
        m = re.search(r'\b%s=(-?\d+)' % klucz, e.get(etap, ''))
        return int(m.group(1)) if m else None
    return e, awaria, p
def opisz(F):
    w = wynik_malo(F)
    if w is None: return None, 'brak wyniku'
    e, awaria, p = w
    if awaria or 'koniec' not in e: return 'awaria', 'AWARIA MODELU (program siegnal pod zly adres - na plytce: wyjatek i restart)'
    odmow = p('koniec', 'odmow') or 0
    lvgl = 'lvgl_assert' in e.get('koniec', '')
    bufory = p('wykresy_otwarte', 'bufory')
    if odmow == 0 and p('koniec', 'restart') == 0 and p('wykres_dzien', 'wykres') == 1: return 'ok', 'wszystko dziala, zadnej odmowy pamieci'
    if p('praca_30_s', 'brak_pamieci_w_logu'): czesc = 'juz na pulpicie czesc odpytan centrali bez pamieci; '
    else: czesc = ''
    if lvgl and p('wykres_dzien', 'restart') == 1:
        return 'restart', czesc + 'wejscie na Wykresy (%d z 8 buforow) -> LVGL bez pamieci -> restart panelu z wpisem w logu' % (bufory or 0)
    if p('wykres_dzien', 'wykres') == 1:
        return 'czesc', czesc + 'wykres jest; w trakcie %d odpytan centrali bez pamieci (naglowek: BRAK PAMIECI), po wyjsciu z Wykresow wraca do normy' % (
            (p('wykres_dzien', 'brak_pamieci_w_logu') or 0) + (p('wykres_na_ekranie_20_s', 'brak_pamieci_w_logu') or 0))
    if 'Brak pamieci na wykres' in e.get('wykres_dzien', ''):
        dalej = '; potem restart (45 s bez danych z centrali)' if p('koniec', 'restart') == 1 else '; panel pracuje dalej'
        return 'napis', czesc + 'napis "Brak pamieci na wykres" (%d z 8 buforow)%s' % (bufory or 0, dalej)
    return 'inne', czesc + 'wykresu nie ma; restart=%s' % p('koniec', 'restart')
print('    %-22s %s' % ('wolny RAM dla firmware', 'co sie dzieje (praca 30 s, wejscie na Wykresy, wykres dzienny 8 serii, 20 s ogladania, powrot)'))
for F in lista:
    rodzaj, tekst = opisz(F)
    print('    %-22s %s' % (b(F) + ' B', tekst))
    if rodzaj == 'awaria': zle.append('wolny RAM %s B: awaria modelu' % b(F))
    if rodzaj is None: zle.append('wolny RAM %s B: brak wyniku' % b(F))
# Prog: najmniej wolnego RAM-u, od ktorego w gore nie ma ani jednej odmowy.
co250 = list(range(0, 32001, 250))
dokladne = sorted(set(range(23000, 27001, 100)) | set(lista) | set(co250))
rodzaje = {F: opisz(F)[0] for F in dokladne}
prog = None
for F in sorted(dokladne, reverse=True):
    if rodzaje[F] == 'ok': prog = F
    else: break
awarie = [F for F in dokladne if rodzaje[F] == 'awaria']
for F in dokladne:
    if rodzaje[F] is None: zle.append('wolny RAM %s B: brak wyniku' % b(F))
# Przedzialy zachowan z prob co 250 B.
czesc = [F for F in co250 if rodzaje[F] == 'czesc']
nizej = [F for F in co250 if czesc and F < min(czesc)]
print('    przedzialy (%d prob, co 250 B od 0 do 32 000 B):' % len(co250))
if prog is not None: print('      %-20s wszystko dziala, zadnej odmowy pamieci' % ('od %s B' % b(prog)))
if czesc: print('      %-20s wykres jest; czesc odpytan centrali bez pamieci, po wyjsciu z Wykresow wraca do normy' % ('%s - %s B' % (b(min(czesc)), b(max(czesc)))))
if nizej:
    ile = {r: sum(1 for F in nizej if rodzaje[F] == r) for r in ('restart', 'napis', 'inne')}
    print('      %-20s wejscie na Wykresy konczy sie restartem panelu z wpisem w logu (%d z %d prob) albo napisem (%d z %d)' % (
        'ponizej %s B' % b(min(czesc)), ile['restart'], len(nizej), ile['napis'] + ile['inne'], len(nizej)))
print('      awarii modelu (siegniecie pod zly adres): %d z %d prob' % (len(awarie), len(dokladne)))
if awarie:
    zle.append('awaria modelu przy wolnym RAM-ie: %s B' % ', '.join(b(F) for F in awarie[:8]))
    sciezka = '%s/malo_ramu_%d.txt' % (kat, awarie[0])
    for l in open(sciezka, errors='replace'):
        if l.startswith('MODEL-AWARIA') and 'stos=' in l:
            adresy = l.split('stos=')[1].split()
            try:
                wyj = subprocess.run(['addr2line', '-f', '-C', '-e', program] + adresy, capture_output=True, text=True).stdout.split('\n')
                print('    stos wywolan awarii (wolny RAM %s B):' % b(awarie[0]))
                for i in range(0, min(len(wyj) - 1, 24), 2): print('      ' + wyj[i][:110])
            except OSError:
                print('    ' + l.strip()[:200])
if prog is None:
    zle.append('nie znaleziono progu, od ktorego nie ma odmow pamieci')
else:
    wolne_dla_fw = ram_plytki - siec_odp - siec_wykres
    print('    prog: od %s B wolnego RAM-u w gore nie ma ani jednej odmowy pamieci' % b(prog))
    print('    plytka: %s B po starcie - %s B sieci = %s B dla firmware\'u, czyli %s B ponad prog' % (b(ram_plytki), b(siec_odp + siec_wykres), b(wolne_dla_fw), b(wolne_dla_fw - prog)))
    if wolne_dla_fw - prog < lim_zapas: zle.append('zapas ponad prog %s B < limit %s B' % (b(wolne_dla_fw - prog), b(lim_zapas)))
if not limity:
    print('    (bez sprawdzania limitow)'); sys.exit(0)
if zle:
    for z in zle: print('    NIE W NORMIE: ' + z)
    sys.exit(1)
print('    W NORMIE: najwiecej naraz <= %s B, na plytce w najgorszej chwili >= %s B, bez przyrostu, bez awarii modelu' % (b(lim_najg), b(lim_zapas)))
PY
fi
exit $WERDYKT
