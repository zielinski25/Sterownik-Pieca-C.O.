#!/usr/bin/env bash
# ============================================================================
# Centrala 3.58.4 i 3.58.5 obok siebie: co dostaje panel LCD po restarcie
# ============================================================================
# Ten sam program testowy (test_panel_hist.cpp) jest kompilowany dwa razy: z
# kodem PANEL-WYKRESY wycietym z centrali 3.58.4 i z 3.58.5. Skrypt:
#
#   [1] odtwarza 5 sytuacji po restarcie centrali (zapis historii wylaczony /
#       wlaczony, stare pliki na flashu albo ich brak) i wypisuje, co kazda
#       wersja odpowiada panelowi po 0, 1, 2, 10, 30 i 120 minutach,
#   [2] dla 1834 losowych zestawow danych porownuje odpowiedzi obu wersji i
#       sprawdza, ze ROZNIA sie tylko te, ktore wedlug opisu zmian maja sie
#       roznic (stara historia przed zakresem albo historia konczaca sie w
#       przeszlosci), a wszystkie pozostale sa identyczne co do bajta.
#
# Wynik: ../WYNIKI/po-restarcie-przed-i-po.txt (i na ekran).
#
#   ./porownaj_z_3.58.4.sh
#   STARY_CPP=sciezka/main_centrala_3.58.4.cpp ./porownaj_z_3.58.4.sh
#
# Plik centrali 3.58.4 jest brany kolejno z: zmiennej STARY_CPP, katalogu
# ../../v3.58.4-panel-wykresy/ (pelne repozytorium), a gdy ich nie ma - jest
# odtwarzany z ../src/main_centrala.cpp przez cofniecie latki
# ../main_centrala-v3.58.4-do-v3.58.5.patch (wymaga programu "patch").
# Wymaga: g++ (C++17), python3. Linux albo WSL.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
NOWY_CPP="${CPP_PATH:-../src/main_centrala.cpp}"
ROB="${TMPDIR:-/tmp}/centrala_panel_hist_test/porownanie"
WYNIK="${WYNIK:-../WYNIKI/po-restarcie-przed-i-po.txt}"
rm -rf "$ROB"
mkdir -p "$ROB/stary" "$ROB/nowy" "$(dirname "$WYNIK")"

STARY_CPP="${STARY_CPP:-}"
if [ -z "$STARY_CPP" ] && [ -f ../../v3.58.4-panel-wykresy/src/main_centrala.cpp ]; then
  STARY_CPP=../../v3.58.4-panel-wykresy/src/main_centrala.cpp
fi
if [ -z "$STARY_CPP" ]; then
  LATKA=../main_centrala-v3.58.4-do-v3.58.5.patch
  if [ ! -f "$LATKA" ] || ! command -v patch >/dev/null 2>&1; then
    echo "Brak pliku centrali 3.58.4. Podaj go: STARY_CPP=sciezka ./porownaj_z_3.58.4.sh"
    echo "(albo zainstaluj program \"patch\" - skrypt odtworzy 3.58.4 z latki $LATKA)."
    exit 2
  fi
  cp "$NOWY_CPP" "$ROB/main_centrala_3.58.4.cpp"
  patch -R -s -p1 "$ROB/main_centrala_3.58.4.cpp" < "$LATKA"
  STARY_CPP="$ROB/main_centrala_3.58.4.cpp"
  echo "[0] plik 3.58.4 odtworzony z latki: $STARY_CPP"
fi
for plik in "$STARY_CPP" "$NOWY_CPP"; do
  if [ ! -f "$plik" ]; then echo "Nie ma pliku: $plik"; exit 2; fi
done
wersja() { { grep -m1 -E '^static const char\* FIRMWARE_VERSION = "' "$1" || true; } | sed -E 's/.*"(.*)".*/\1/'; }
W_STARY="$(wersja "$STARY_CPP")"
W_NOWY="$(wersja "$NOWY_CPP")"
if [ "$W_STARY" != "v3.58.4" ] || [ "$W_NOWY" != "v3.58.5" ]; then
  echo "Oczekiwane wersje v3.58.4 i v3.58.5, a jest: $W_STARY ($STARY_CPP) i $W_NOWY ($NOWY_CPP)."
  exit 2
fi

echo "[1] wycinam kod PANEL-WYKRESY z obu plikow i kompiluje ten sam test dwa razy"
python3 wytnij_blok.py "$STARY_CPP" "$ROB/stary/panel_hist_wycinek.inc" >/dev/null
python3 wytnij_blok.py "$NOWY_CPP" "$ROB/nowy/panel_hist_wycinek.inc" >/dev/null
# Kod 3.58.4 nie ma funkcji zegara, wiec czesc atrap testu jest dla niego nieuzywana (-Wno-unused-function).
g++ -std=gnu++17 -O1 -Wall -Wextra -Wno-unused-function -I "$ROB/stary" -o "$ROB/stary/test" test_panel_hist.cpp
g++ -std=gnu++17 -O1 -Wall -Wextra -Wno-unused-function -I "$ROB/nowy" -o "$ROB/nowy/test" test_panel_hist.cpp

echo "[2] sytuacje po restarcie i losowe zestawy na obu wersjach"
"$ROB/stary/test" --po-restarcie > "$ROB/stary/tabela.txt"
"$ROB/nowy/test" --po-restarcie > "$ROB/nowy/tabela.txt"
"$ROB/stary/test" --skroty > "$ROB/stary/skroty.txt"
"$ROB/nowy/test" --skroty > "$ROB/nowy/skroty.txt"

python3 - "$ROB" "$WYNIK" "$STARY_CPP" "$NOWY_CPP" <<'PYEOF'
import hashlib
import sys

rob, wynik, stary_cpp, nowy_cpp = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]


def sha(nazwa):
    return hashlib.sha256(open(nazwa, 'rb').read()).hexdigest()


def wczytaj(nazwa):
    return [l.rstrip('\n').split('\t') for l in open(nazwa, encoding='utf-8') if l.strip()]


stary = wczytaj(rob + '/stary/tabela.txt')
nowy = wczytaj(rob + '/nowy/tabela.txt')
assert len(stary) == len(nowy), 'rozna liczba wierszy tabeli'
bledy = []
out = []


def ile_temu(sek):
    sek = int(sek)
    if sek < 0:
        return ''
    if sek <= 360:
        return ''   # do 6 minut bez pomiaru to zwykly stan (ten sam prog, od ktorego centrala liczy przerwe)
    if sek < 3 * 3600:
        return 'sprzed %d min' % round(sek / 60)
    if sek < 2 * 86400:
        return 'sprzed %d godz.' % round(sek / 3600)
    return 'sprzed %d dni' % round(sek / 86400)


def panel(w):
    """Co panel LCD v1.3.0 robi z taka odpowiedzia (zasady z dashboard_lvgl_poc.cpp)."""
    punktow, przerw, od, do, temu = int(w[4]), int(w[5]), w[6].replace('_', ' '), w[7].replace('_', ' '), w[8]
    if punktow == 0:
        return 'NAPIS: brak danych z tego zakresu', False
    if punktow == 1:
        return 'NAPIS: za malo danych (jeden pomiar)', False
    stare = ile_temu(temu)
    tekst = 'wykres: %2d pkt + %2d przerw, %s - %s' % (punktow, przerw, od, do)
    if stare:
        tekst += '  <- DANE ' + stare.upper() + ' jako biezace'
    return tekst, bool(stare)


out.append('CENTRALA 3.58.4 I 3.58.5: CO DOSTAJE PANEL LCD PO RESTARCIE CENTRALI')
out.append('=====================================================================')
out.append('Wynik skryptu tests/porownaj_z_3.58.4.sh. Kod obu wersji wyciety z plikow centrali')
out.append('i uruchomiony na PC (nie na plytce):')
out.append('  3.58.4  main_centrala.cpp SHA-256 ' + sha(stary_cpp))
out.append('  3.58.5  main_centrala.cpp SHA-256 ' + sha(nowy_cpp))
out.append('W kazdej sytuacji jest 13.11.2026 13:00, panel pyta o 96 punktow. "minut" = ile')
out.append('pomiarow (po jednym na minute) centrala zrobila od restartu; pierwszy pomiar jest')
out.append('ok. minuty po starcie, wiec "0 minut" to pierwsza minuta pracy.')
out.append('')
out.append('Co robi panel LCD v1.3.0 z odpowiedzia centrali:')
out.append('  0 punktow z danymi  -> napis "Centrala nie ma jeszcze danych z tego zakresu..."')
out.append('  1 punkt             -> napis "Za malo danych na wykres (jeden pomiar)..."')
out.append('  2 i wiecej          -> wykres; "przerwy" to puste miejsca w linii wykresu')
out.append('Dopisek "DANE SPRZED ... jako biezace": okno wykresu konczy sie ponad 6 minut przed biezacym')
out.append('czasem centrali (6 minut to prog, od ktorego centrala liczy brak pomiarow jako przerwe).')
out.append('')

sytuacja = None
wiersze_nowe_stare = 0
for a, b in zip(stary, nowy):
    assert a[0] == b[0] and a[1] == b[1], (a, b)
    if a[0] == 'SYTUACJA':
        out.append('')
        out.append('SYTUACJA %s: %s' % (a[1], a[2]))
        out.append('-' * 100)
        out.append('  %-5s %-6s %-9s %s' % ('minut', 'tryb', 'centrala', 'co pokazuje panel'))
        sytuacja = a[1]
    elif a[0] == 'WIERSZ':
        assert a[2] == b[2] and a[3] == b[3], (a, b)
        ta, stare_a = panel(a)
        tb, stare_b = panel(b)
        if a[4:8] == b[4:8]:
            out.append('  %-5s %-6s %-9s %s' % (a[2], a[3], 'obie', ta))
        else:
            out.append('  %-5s %-6s %-9s %s' % (a[2], a[3], '3.58.4', ta))
            out.append('  %-5s %-6s %-9s %s' % ('', '', '3.58.5', tb))
        if stare_b:
            wiersze_nowe_stare += 1
            bledy.append('3.58.5: okno konczy sie ponad 6 minut przed biezacym czasem: sytuacja %s, %s min, tryb %s' % (a[1], a[2], a[3]))
    elif a[0] == 'NAPIS':
        assert a[2] == b[2], (a, b)

        def opis(n):
            n = int(n)
            return 'wykres od razu' if n == 0 else 'napis do drugiego pomiaru, potem wykres' if n == 1 else 'napis przez pierwsze %d minut' % n

        if a[2] == 'godz':
            out.append('  Jak dlugo po restarcie panel pokazuje napis zamiast wykresu (sprawdzone minuta po minucie, do 240):')
        if a[3] == b[3]:
            out.append('    tryb %-6s obie wersje: %s' % (a[2], opis(a[3])))
        else:
            out.append('    tryb %-6s 3.58.4: %s;  3.58.5: %s' % (a[2], opis(a[3]), opis(b[3])))
        if int(b[3]) > 1:
            bledy.append('3.58.5: napis dluzej niz minute: sytuacja %s tryb %s (%s min)' % (a[1], a[2], b[3]))
    else:
        bledy.append('nieznany wiersz: %r' % (a,))

# --- losowe zestawy --------------------------------------------------------
sa = wczytaj(rob + '/stary/skroty.txt')
sb = wczytaj(rob + '/nowy/skroty.txt')
assert len(sa) == len(sb) and len(sa) > 0
st = {'bez_zmian_identyczne': 0, 'bez_zmian_ROZNE': 0, 'zmiana_rozne': 0, 'zmiana_identyczne': 0}
for x, y in zip(sa, sb):
    assert x[:4] == y[:4] and x[5] == y[5], (x, y)
    takie_same = x[4] == y[4]
    if x[5] == '1':
        st['bez_zmian_identyczne' if takie_same else 'bez_zmian_ROZNE'] += 1
        if not takie_same:
            bledy.append('zestaw losowy %s/%s tryb %s pkt %s: odpowiedz inna niz w 3.58.4, a miala byc taka sama' % tuple(x[:4]))
    else:
        st['zmiana_rozne' if not takie_same else 'zmiana_identyczne'] += 1

out.append('')
out.append('')
out.append('LOSOWE ZESTAWY DANYCH: CZY 3.58.5 ZMIENIA TYLKO TO, CO MIALA ZMIENIC')
out.append('-' * 100)
out.append('%d losowych zestawow (te same, na ktorych biegna scenariusze 12 i 16 testow), odpowiedz kazdej' % len(sa))
out.append('wersji porownana co do bajta. O tym, czy odpowiedz MA byc taka sama, decyduje osobny rachunek')
out.append('w tescie (refBezZmianWzgledem3584), nie kod centrali:')
out.append('  - historia w calosci miesci sie w zakresie wykresu albo na poczatku zakresu jest pomiar,')
out.append('    i zegar centrali nie pokazuje, ze historia konczy sie w przeszlosci   -> BEZ ZMIAN')
out.append('  - pozostale (zakres zaczyna sie w przerwie albo przed pierwszym pomiarem z zakresu;')
out.append('    historia konczy sie w przeszlosci)                                     -> wolno sie roznic')
out.append('')
out.append('  mialy byc bez zmian i sa identyczne:      %5d' % st['bez_zmian_identyczne'])
out.append('  mialy byc bez zmian, a sie roznia:        %5d   (kazdy taki zestaw to blad)' % st['bez_zmian_ROZNE'])
out.append('  wolno im sie roznic i sie roznia:         %5d' % st['zmiana_rozne'])
out.append('  wolno im sie roznic, a sa identyczne:     %5d' % st['zmiana_identyczne'])
out.append('')
if bledy:
    out.append('SA BLEDY (%d):' % len(bledy))
    out.extend('  - ' + b for b in bledy[:40])
else:
    out.append('WYNIK: OK - w 3.58.5 po restarcie napis zamiast wykresu jest najwyzej do drugiego pomiaru,')
    out.append('zadne okno wykresu nie konczy sie ponad 6 minut przed biezacym czasem centrali, a poza')
    out.append('opisanymi zmianami odpowiedzi sa identyczne z 3.58.4.')
tekst = '\n'.join(out) + '\n'
open(wynik, 'w', encoding='utf-8').write(tekst)
sys.stdout.write(tekst)
sys.exit(1 if bledy else 0)
PYEOF
echo "Zapisano: $WYNIK"
