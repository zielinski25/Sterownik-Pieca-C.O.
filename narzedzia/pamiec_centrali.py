#!/usr/bin/env python3
# ============================================================================
# Ile pamieci bierze w centrali historia i adres GET /api/history/panel
# (wykresy dla panelu LCD) - RACHUNEK Z KODU, na PC, bez plytki.
# ============================================================================
#   python3 narzedzia/pamiec_centrali.py
#   python3 narzedzia/pamiec_centrali.py sciezka/do/main_centrala.cpp
#
# Co robi:
#   1. czyta z main_centrala.cpp rozmiary buforow i wzory rezerwacji pamieci,
#   2. rozmiary struktur bierze od kompilatora z PRAWDZIWYMI naglowkami
#      arduino-esp32 dla ukladu xtensa (ten sam mechanizm co
#      LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh; pierwsze uruchomienie
#      pobiera naglowki, ok. 250 MB) - bo wyrownanie pol na ESP32 jest inne niz
#      na PC,
#   3. kompiluje kod liczacy punkty (wyciety z pliku centrali, jak w testach
#      paczki) dla 32-bitowego x86 i podaje ramke stosu oraz rozmiar kodu -
#      to PRZYBLIZENIE, na ESP32 liczby beda podobnego rzedu, nie identyczne,
#   4. wypisuje limity systemu plikow (logi, pliki historii, rezerwy) tak, jak
#      sa zapisane w kodzie centrali - bez oceniania, ile plikow jest naprawde.
#
# Czego NIE robi: nie mierzy wolnej pamieci ani zajetosci systemu plikow na
# plytce. Wolna pamiec podaje sama centrala co 5 minut w logu:
#   tag=HEALTH-A ... int_free= int_min= psram_free= psram_largest= psram_min=
# a zajetosc systemu plikow - adres /api/fs-list (pola "used" i "total").
#
# Wymaga: python3 z pakietem "ziglang", git (jak sprawdz_naglowki_esp32.sh).
# ============================================================================
import hashlib
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACZKA = os.path.join(REPO, 'firmware-patch', 'v3.58.5-panel-wykresy-po-restarcie')
CPP = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(PACZKA, 'src', 'main_centrala.cpp')
TESTY_CENTRALI = os.path.join(PACZKA, 'tests')
TESTY_PANELU = os.path.join(REPO, 'LCD-Centrali', 'testy_host')
RDZEN = os.environ.get('RDZEN', '3.3.8')          # centrala: pioarduino 55.03.38 = arduino-esp32 3.3.8
PSRAM_B = 8 * 1024 * 1024                         # plytka ESP32-S3 N16R8: 8 MB PSRAM
STOS_ASYNC_TCP_B = 8192 * 2                       # AsyncTCP 3.4.10: CONFIG_ASYNC_TCP_STACK_SIZE (domyslne)
PROG_PSRAM_B = 4096                               # CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL w arduino-esp32
FS_B = 0x9E0000                                   # partycja systemu plikow w partitions.csv projektu centrali (10 354 688 B)


def b(n):
    return format(int(n), ',').replace(',', ' ')


def stala(tekst, wzor, opis):
    m = re.search(wzor, tekst, re.M)
    if not m:
        sys.exit('Nie znalazlem w pliku centrali: %s (wzor: %s)' % (opis, wzor))
    return int(m.group(1))


zrodlo = open(CPP, encoding='utf-8', errors='replace').read()
wersja = re.search(r'FIRMWARE_VERSION\s*=\s*"([^"]+)"', zrodlo)
print('PAMIEC CENTRALI - rachunek z kodu')
print('plik: %s' % os.path.relpath(CPP, REPO))
print('wersja: %s   SHA-256: %s' % (wersja.group(1) if wersja else '?', hashlib.sha256(open(CPP, 'rb').read()).hexdigest()))

N_KROTKI = stala(zrodlo, r'^#define\s+HISTORIA_MAX_PROBEK\s+(\d+)', 'HISTORIA_MAX_PROBEK')
N_DLUGI = stala(zrodlo, r'^#define\s+HISTORIA_DLUGA_MAX_PROBEK\s+(\d+)', 'HISTORIA_DLUGA_MAX_PROBEK')
PROBKA_MS = stala(zrodlo, r'^#define\s+HISTORIA_PROBKA_MS\s+(\d+)', 'HISTORIA_PROBKA_MS')
N_TELEMETRIA = stala(zrodlo, r'TELEMETRY_RING_CAPACITY\s*=\s*(\d+)U', 'TELEMETRY_RING_CAPACITY')
B_TELEMETRIA = stala(zrodlo, r'static_assert\(sizeof\(TelemetrySample\)\s*==\s*(\d+)', 'sizeof(TelemetrySample)')
B_LOG = stala(zrodlo, r'^#define\s+LOG_BUF_SIZE\s+(\d+)', 'LOG_BUF_SIZE')
B_DIAG = stala(zrodlo, r'^#define\s+DIAG_REMOTE_BUF_SIZE\s+(\d+)', 'DIAG_REMOTE_BUF_SIZE')
B_DIAG_TX = stala(zrodlo, r'DIAG_REMOTE_CHUNK_MAX_B\s*=\s*(\d+)U', 'DIAG_REMOTE_CHUNK_MAX_B') + 1
B_JSON_TEL = stala(zrodlo, r'TELEMETRY_JSON_BUFFER_BYTES\s*=\s*(\d+)U', 'TELEMETRY_JSON_BUFFER_BYTES')
PKT = stala(zrodlo, r'^#define\s+PANEL_HIST_PKT_DOMYSLNIE\s+(\d+)', 'PANEL_HIST_PKT_DOMYSLNIE')
PKT_MAX = stala(zrodlo, r'^#define\s+PANEL_HIST_PKT_MAX\s+(\d+)', 'PANEL_HIST_PKT_MAX')
m = re.search(r'out\.reserve\(\(unsigned int\)n \* (\d+)U \+ (\d+)U\)', zrodlo)
if not m:
    sys.exit('Nie znalazlem rezerwacji tekstu odpowiedzi w historiaPanelZbudujCsv()')
REZ_WIERSZ, REZ_STALE = int(m.group(1)), int(m.group(2))
REZ_DLUGI = stala(zrodlo, r'csv\.reserve\(liczbaProbekSnap \* (\d{3})\)', 'rezerwacja w historiaDlugaZbudujCsv()')
REZ_KROTKI = stala(zrodlo, r'csv\.reserve\(liczbaProbekSnap \* (\d{2})\)', 'rezerwacja w historiaZbudujCsv()')

rob = tempfile.mkdtemp(prefix='pamiec_centrali_')

# --- rozmiary struktur na ESP32 (xtensa, prawdziwe naglowki) ---------------
STRUKTURY = ['HistoriaProbka', 'HistoriaProbkaDluga', 'PanelHistKubelek', 'PanelHistWiersz']
sonda = os.path.join(rob, 'sonda_rozmiarow.cpp')
subprocess.run([sys.executable, os.path.join(TESTY_CENTRALI, 'wytnij_blok.py'), CPP, '--esp32', sonda], check=True, stdout=subprocess.DEVNULL)
with open(sonda, 'a', encoding='utf-8') as f:
    # Celowy blad kompilacji: w komunikacie kompilator podaje wartosc sizeof().
    f.write('\ntemplate <unsigned ROZMIAR, int NR> struct RozmiarTo;\n')
    for i, s in enumerate(STRUKTURY):
        f.write('RozmiarTo<sizeof(%s), %d> sonda_%d;\n' % (s, i, i))
srodowisko = dict(os.environ, SRC=rob, PLIKI='sonda_rozmiarow.cpp', RDZEN=RDZEN)
wynik = subprocess.run([os.path.join(TESTY_PANELU, 'sprawdz_naglowki_esp32.sh')], env=srodowisko, capture_output=True, text=True)
podreczny = os.path.join(os.environ.get('LCD_TESTY_CACHE', os.path.join(os.environ.get('TMPDIR', '/tmp'), 'lcd_testy_host')), 'naglowki_esp32')
plik_wyniku = os.path.join(podreczny, 'wynik_%s_sonda_rozmiarow.cpp.txt' % RDZEN)
rozmiar = {}
if os.path.exists(plik_wyniku):
    for mm in re.finditer(r"RozmiarTo<(\d+), (\d+)>", open(plik_wyniku, errors='replace').read()):
        rozmiar[STRUKTURY[int(mm.group(2))]] = int(mm.group(1))
if len(rozmiar) != len(STRUKTURY):
    print(wynik.stdout[-1500:])
    sys.exit('Nie udalo sie odczytac rozmiarow struktur z kompilatora (patrz wyzej; potrzebny "pip install ziglang" i dostep do GitHuba).')
print('rozmiary struktur: kompilator z naglowkami arduino-esp32 %s (xtensa): %s' % (RDZEN, ', '.join('%s %d B' % (s, rozmiar[s]) for s in STRUKTURY)))

# --- [1] bufory przydzielane raz, przy starcie -----------------------------
print('')
print('[1] PSRAM: bufory przydzielane RAZ, przy starcie (nie rosna z czasem - najstarsze wpisy sa nadpisywane)')
stale = [
    ('historia krotka (probka co %d s, %d h)' % (PROBKA_MS // 1000, N_KROTKI * PROBKA_MS // 3600000), N_KROTKI, rozmiar['HistoriaProbka']),
    ('historia dluga (zdarzenia)', N_DLUGI, rozmiar['HistoriaProbkaDluga']),
    ('kolejka telemetrii do Firebase', N_TELEMETRIA, B_TELEMETRIA),
    ('log', 1, B_LOG),
    ('diagnostyka zdalna', 1, B_DIAG + B_DIAG_TX),
    ('tekst paczki telemetrii', 1, B_JSON_TEL),
]
razem = 0
for opis, ile, jeden in stale:
    razem += ile * jeden
    szczegol = '%s x %s B' % (b(ile), b(jeden)) if ile > 1 else ''
    print('    %-40s %-18s %11s B' % (opis, szczegol, b(ile * jeden)))
print('    %-40s %-18s %11s B  = %.0f %% z %s B PSRAM' % ('razem', '', b(razem), 100.0 * razem / PSRAM_B, b(PSRAM_B)))
min_dni = N_DLUGI * (PROBKA_MS / 1000.0) / 86400.0
print('    historia dluga: najwyzej 1 wpis na probke (co %d s), wiec %s wpisow to CO NAJMNIEJ %s;' % (
    PROBKA_MS // 1000, b(N_DLUGI), ('%.1f doby' % min_dni).replace('.', ',')))
print('      wpis powstaje tylko przy zmianie wartosci o prog albo raz na godzine - im spokojniej, tym dluzszy okres.')
try:
    print('      (twardy limit w kodzie: nie czesciej niz co %d s - HIST_DLUGA_MIN_ODSTEP_MS)' % (
        int(re.search(r'^#define\s+HIST_DLUGA_MIN_ODSTEP_MS\s+\((\d+)UL\*1000UL\)', zrodlo, re.M).group(1))))
except AttributeError:
    pass
print('    Wylacznik zapisu na flash nie zmienia tych buforow: sa w pamieci zawsze.')

# --- [2] jedno zapytanie panelu ---------------------------------------------
wzorce = os.path.join(TESTY_PANELU, 'wzorce_centrali')
najdluzsza = max((os.path.getsize(os.path.join(wzorce, f)) for f in os.listdir(wzorce) if f.endswith('.txt') and f != 'CZYTAJ.txt'), default=0)
print('')
print('[2] Jedno zapytanie GET /api/history/panel (panel LCD prosi o pkt=%d; najwiecej, ile mozna: pkt=%d)' % (PKT, PKT_MAX))
print('    %-58s %12s %12s' % ('', 'pkt=%d' % PKT, 'pkt=%d' % PKT_MAX))
wiersze = []
for pkt in (PKT, PKT_MAX):
    kub = pkt * rozmiar['PanelHistKubelek']
    rez = pkt * REZ_WIERSZ + REZ_STALE
    kopia = najdluzsza * pkt // PKT          # najdluzsza odpowiedz z testow, przeskalowana liczba punktow
    wiersze.append((kub, rez, kopia, max(kub + rez, rez + kopia)))
for opis, i in (('punkty robocze: pkt x %d B (ps_malloc, zwalniane od razu)' % rozmiar['PanelHistKubelek'], 0),
                ('tekst odpowiedzi: String::reserve(pkt x %d + %d)' % (REZ_WIERSZ, REZ_STALE), 1),
                ('kopia tekstu w serwerze HTTP, do konca wysylania', 2),
                ('NAJWIECEJ NARAZ (dwa z trzech blokow jednoczesnie)', 3)):
    print('    %-58s %10s B %10s B' % (opis, b(wiersze[0][i]), b(wiersze[1][i])))
print('    Wszystkie trzy bloki maja ponad %s B, wiec malloc() arduino-esp32 bierze je z PSRAM' % b(PROG_PSRAM_B))
print('      (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=%d); gdy PSRAM odmowi - ze zwyklej sterty, a gdy i ta odmowi - kod 503.' % PROG_PSRAM_B)
print('    Najdluzsza odpowiedz w testach (pkt=%d): %s B. Zalezy od liczby punktow, NIE od ilosci historii:' % (PKT, b(najdluzsza)))
print('      pelne bufory (%s + %s wierszy) i pusta historia biora tyle samo albo mniej.' % (b(N_DLUGI), b(N_KROTKI)))
print('    Kilka zapytan naraz: handler liczy jedno po drugim; czekajace na wyslanie trzymaja tylko swoja kopie tekstu.')

# --- [3] dla porownania: stare adresy ---------------------------------------
print('')
print('[3] Dla porownania - stare adresy (strona WWW; panel LCD do v1.2.2 i panel 1.3.0 ze starsza centrala)')
for adres, n, rez in (('/api/history', N_KROTKI, REZ_KROTKI), ('/api/history/long', N_DLUGI, REZ_DLUGI)):
    print('    %-20s String::reserve(%s x %d) = %s B i kopia w serwerze HTTP: przy pelnym buforze ok. %s B naraz' % (adres, b(n), rez, b(n * rez), b(2 * n * rez)))
print('    Panel 1.3.0 z centrala 3.58.4+ nie wywoluje juz /api/history/long: %s B zamiast do ok. %s B na wykres tygodniowy.' % (b(wiersze[0][3]), b(2 * N_DLUGI * REZ_DLUGI)))

# --- [4] stos i rozmiar kodu (przyblizenie z kompilacji na x86-32) ----------
print('')
print('[4] Stos i rozmiar kodu - PRZYBLIZENIE (ten sam kod skompilowany dla 32-bitowego x86, -Os)')
try:
    inc = os.path.join(rob, 'panel_hist_wycinek.inc')
    subprocess.run([sys.executable, os.path.join(TESTY_CENTRALI, 'wytnij_blok.py'), CPP, inc], check=True, stdout=subprocess.DEVNULL)
    obiekt = os.path.join(rob, 'blok.o')
    k = subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-target', 'x86-linux-musl', '-std=gnu++17', '-Os', '-Wno-everything',
                        '-Wframe-larger-than=64', '-I', rob, '-c', '-o', obiekt, os.path.join(TESTY_CENTRALI, 'test_panel_hist.cpp')],
                       capture_output=True, text=True)
    ramki = {}
    for mm in re.finditer(r"stack frame size \((\d+)\) exceeds limit \(\d+\) in (?:function )?'([^'(]+)", k.stderr):
        if re.search(r'panelHist|historiaPanel', mm.group(2)):
            ramki[mm.group(2)] = int(mm.group(1))
    kod = 0
    for l in subprocess.run(['nm', '-S', '-C', obiekt], capture_output=True, text=True).stdout.split('\n'):
        p = l.split(None, 3)
        if len(p) == 4 and p[2] in 'tT' and re.search(r'panelHist|historiaPanel', p[3]):
            kod += int(p[1], 16)
    if not ramki or kod == 0:
        raise RuntimeError('kompilator nie podal ramek stosu:\n' + k.stderr[-600:])
    glowna = max(ramki.values())
    print('    ramka stosu historiaPanelZbudujCsv(): %s B; stos zadania async_tcp, w ktorym dziala handler: %s B' % (b(glowna), b(STOS_ASYNC_TCP_B)))
    print('      (do tego snprintf/mktime z biblioteki C i ramki serwera HTTP - takie same jak przy pozostalych adresach)')
    print('    kod funkcji panelHist*/historiaPanel*: %s B; zmienne statyczne tego kodu: 0 B' % b(kod))
except Exception as e:  # noqa: BLE001 - narzedzie pomocnicze: brak kompilatora to nie blad rachunku wyzej
    print('    (pominiete: %s)' % str(e).strip().split('\n')[0])

# --- [5] system plikow (flash na dane): limity zapisane w kodzie ------------
def iloczyn(nazwa):
    """Wartosc stalej zapisanej jako iloczyn, np. (128UL * 1024UL) albo 20UL * 64UL * 1024UL."""
    m = re.search(r'\b%s\b\s*=?\s*\(?([0-9UL *]+?)\)?\s*(?:;|//|$)' % re.escape(nazwa), zrodlo, re.M)
    if not m:
        raise RuntimeError('nie znalazlem stalej %s' % nazwa)
    wynik = 1
    for czynnik in m.group(1).split('*'):
        wynik *= int(re.sub(r'[UL\s]', '', czynnik))
    return wynik


print('')
print('[5] System plikow centrali (flash na dane, partycja %s B) - LIMITY Z KODU, nie pomiar' % b(FS_B))
try:
    MARG_K = iloczyn('HISTORIA_RESYNC_MARGINES')
    MARG_D = iloczyn('HISTORIA_DLUGA_RESYNC_MARGINES')
    PCT_ROT = iloczyn('LOG_FS_ROTATE_PERCENT')
    PCT_TRIM = iloczyn('LOG_FS_CIRCULAR_TRIGGER_PCT')
    LOG_KRYT = iloczyn('LOG_KRYTYCZNY_MAX_B')
    LOG_UST = iloczyn('LOG_USTAWIEN_MAX_B')
    REZ_ZRZUTY = iloczyn('FS_RESERVE_COREDUMP_B')
    REZ_HK = iloczyn('FS_RESERVE_HIST_SHORT_B')
    REZ_HD = iloczyn('FS_RESERVE_HIST_LONG_B')
    REZ_MIN = iloczyn('FS_CRITICAL_FREE_MIN_B')
    cialo = re.search(r'^void logCircularTrim\(\) \{(.*?)^\}', zrodlo, re.M | re.S)
    liczone = re.search(r'size_t uzyte = ([^;]+);', cialo.group(1)) if cialo else None
    if not liczone:
        raise RuntimeError('nie znalazlem rachunku zajetosci w logCircularTrim()')
    skladniki = re.findall(r'g_\w+', liczone.group(1))
    historia_liczona = any('hist' in x.lower() for x in skladniki)
    na_reszte = FS_B * PCT_TRIM // 100
    rezerwy = REZ_ZRZUTY + REZ_HK + REZ_HD + REZ_MIN
    w = '    %-36s %s'
    print(w % ('log biezacy (log_b.txt)', 'rosnie do %d %% pojemnosci = %s B; potem wysylka na Telegram i kasowanie albo rotacja' % (PCT_ROT, b(FS_B * PCT_ROT // 100))))
    print(w % ('logi + firmware panelu do OTA', 'logi przycinane, gdy poza tymi plikami zostaje mniej niz %d %% = %s B' % (PCT_TRIM, b(na_reszte))))
    print(w % ('log alarmow / log nastaw', 'najwyzej %s B / %s B' % (b(LOG_KRYT), b(LOG_UST))))
    print(w % ('historia krotka (hist_short.csv)', 'przycinana do %s linii po przekroczeniu %s; rezerwa wg firmware\'u %s B' % (b(N_KROTKI), b(N_KROTKI + MARG_K), b(REZ_HK))))
    print(w % ('historia dluga (hist_long.csv)', 'przycinana do %s linii po przekroczeniu %s; rezerwa wg firmware\'u %s B' % (b(N_DLUGI), b(N_DLUGI + MARG_D), b(REZ_HD))))
    print(w % ('zrzuty po awarii (coredumps)', 'rezerwa wg firmware\'u %s B' % b(REZ_ZRZUTY)))
    print(w % ('wolne minimum wg firmware\'u', '%s B' % b(REZ_MIN)))
    print(w % ('rezerwy firmware\'u razem', '%s B' % b(rezerwy)))
    print('    Przycinanie logow liczy zajetosc jako: %s - plikow historii %s.' % (' + '.join(skladniki), 'TEZ' if historia_liczona else 'NIE liczy'))
    if rezerwy > na_reszte and not historia_liczona:
        print('    Przy logach przy swoim limicie na reszte plikow zostaje %s B, a rezerwy wyliczone w firmware to %s B.' % (b(na_reszte), b(rezerwy)))
        print('      Z samego kodu NIE da sie wiec potwierdzic, ze miejsca starczy, gdy logi, pliki historii i zrzuty sa naraz przy limitach.')
        print('      Firmware sprawdza to sam ("FS quota shadow", ostrzezenie fs_quota_shadow_low), ale niczym jeszcze nie steruje.')
    else:
        print('    Rezerwy wyliczone w firmware (%s B) mieszcza sie w tym, co zostaje poza logami (%s B).' % (b(rezerwy), b(na_reszte)))
    print('    Pliki historii rosna tylko przy wlaczonym zapisie historii. Kod wykresow dla panelu LCD niczego nie zapisuje.')
    print('    Na plytce: adres /api/fs-list (pola "used", "total" i rozmiary plikow) albo raport diagnostyczny telemetrii')
    print('      w Telegramie (linie "FS fizyczny", "FS skladniki", "FS quota shadow").')
except Exception as e:  # noqa: BLE001 - starsze wersje centrali nie maja czesci tych stalych
    print('    (pominiete: %s)' % str(e).strip().split('\n')[0])

print('')
print('Na plytce: wolna pamiec podaje log centrali co 5 minut (tag=HEALTH-A: int_free, int_min, psram_free, psram_min).')
