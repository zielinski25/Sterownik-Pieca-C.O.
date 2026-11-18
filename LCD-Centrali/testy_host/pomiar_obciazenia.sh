#!/usr/bin/env bash
# ============================================================================
# Pomiar obciazenia panelu W SPOCZYNKU - na PC, bez plytki. Same liczby.
# ============================================================================
# Odpowiada na pytania:
#   - ile ekranu panel przerysowuje, gdy NIC sie na nim nie zmienia,
#   - ile razy na minute laczy sie z centrala,
#   - ile razy na minute prosi o pamiec (malloc/calloc/realloc),
#   - czy po kilkunastu minutach bez dotyku, na samej baterii, usypia albo
#     przygasza ekran.
#
#   ./pomiar_obciazenia.sh                 stan firmware'u z ../src
#   ./pomiar_obciazenia.sh --prototyp      to samo + szacunek: ile zostanie, gdy
#                                          dashboard bedzie ustawial tylko to, co sie zmienilo
#                                          (prototypy/ustaw_tylko_zmiany.c - to nie poprawka firmware'u)
#   SRC=/sciezka/do/src ./pomiar_obciazenia.sh    inne zrodla (np. starsza wersja - do porownania)
#
# Co te liczby znacza na plytce i czego w nich nie ma - opis w README.md
# ("Pomiar obciazenia w spoczynku") i w komentarzu nad
# scenariuszPomiarObciazenia() w symulator_panelu.cpp. W skrocie: piksele i
# zapytania sa takie same jak na plytce; pradu i czasu tu nie ma.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"

uruchom() {   # uruchom <PROTOTYP 0|1>
  PROTOTYP="$1" OBSERWACJE=1 BEZ_LOGU=1 ./uruchom_testy.sh pomiar_obciazenia 2>&1 | grep -E '^(OBCIAZENIE|    BLAD|BLAD)' || {
    echo "pomiar sie nie wykonal - uruchom: OBSERWACJE=1 ./uruchom_testy.sh pomiar_obciazenia" >&2
    return 1
  }
}

echo "== stan obecny (zrodla: ${SRC:-../src})"
uruchom 0
if [ "${1:-}" = "--prototyp" ]; then
  echo
  echo "== szacunek po zmianie \"ustawiaj tylko to, co sie zmienilo\" (prototyp, firmware bez zmian)"
  uruchom 1
fi
