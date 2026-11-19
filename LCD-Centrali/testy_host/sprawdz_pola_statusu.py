#!/usr/bin/env python3
"""Czy kazde pole, ktore panel czyta z /api/status, centrala naprawde wysyla.

Panel czyta /api/status przez filtr diag::POLA_STATUSU_CZYTANE[] (+ pola "symulacja_*"
z CZUJNIKI_SYMULOWANE[]) w src/diagnostyka_panelu.h - tak jak poleStatusuCzytane(). Centrala odpowiada na /api/status
funkcja wifiZbudujJsonStatus() w main_centrala.cpp (plus funkcje pomocnicze wolane z niej
z buforem `j`, np. panelOtaKlejJson(j)). Ten skrypt wyciaga nazwy pol z JEDNEGO i DRUGIEGO
zrodla i sprawdza, ze lista panelu jest podzbiorem tego, co centrala wysyla.

Po co: panel v1.4.6 dostal pole "fw" na podstawie opisu ("centrala wystawia fw od dawna"),
a prawdziwa centrala 3.58.6/3.58.7 wysylala "fw" tylko do Firebase i /api/lan/status - nie
w /api/status. Atrapa centrali w symulatorze podawala "fw", wiec testy byly zielone, a na
plytce panel pokazywal "Centrala -". Ten skrypt porownuje panel z PRAWDZIWYM kodem
centrali, nie z atrapa.

Wyjatki (pola, ktorych prawdziwa centrala celowo nie wysyla): WYJATKI ponizej, kazdy
z uzasadnieniem.

Uzycie:
    sprawdz_pola_statusu.py                      najnowsza paczka firmware-patch/v3.*/src/main_centrala.cpp
    sprawdz_pola_statusu.py <main_centrala.cpp>  wskazany plik centrali
    sprawdz_pola_statusu.py --pokaz [plik]       wypisz tez wszystkie pola centrali
Kod wyjscia 0 = zgodne, 1 = brakuje pol, 2 = nie znaleziono plikow/funkcji.
"""
import glob
import os
import re
import sys

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
NAGLOWEK_PANELU = os.path.join(TU, "..", "src", "diagnostyka_panelu.h")

# Pola z listy panelu, ktorych prawdziwa centrala NIE wysyla - swiadomie.
WYJATKI = {
    "symulator": "wysyla tylko centrala-testowa (symulator C3); prawdziwa centrala go nie ma, panel traktuje brak jako 'sterownik'",
}


def wersja_z_nazwy(sciezka):
    m = re.search(r"v(\d+)\.(\d+)\.(\d+)", sciezka)
    return tuple(int(x) for x in m.groups()) if m else (0, 0, 0)


def najnowsza_centrala():
    kandydaci = glob.glob(os.path.join(REPO, "firmware-patch", "v3.*", "src", "main_centrala.cpp"))
    if not kandydaci:
        return None
    return max(kandydaci, key=wersja_z_nazwy)


def cialo_funkcji(tekst, naglowek):
    """Tekst od naglowka funkcji do pasujacej klamry zamykajacej (liczy klamry; komentarze i
    napisy w tym pliku nie zawieraja niesparowanych klamer w badanych funkcjach)."""
    i = tekst.find(naglowek)
    if i < 0:
        return None
    j = tekst.index("{", i)
    glebokosc = 0
    for k in range(j, len(tekst)):
        c = tekst[k]
        if c == "{":
            glebokosc += 1
        elif c == "}":
            glebokosc -= 1
            if glebokosc == 0:
                return tekst[i:k + 1]
    return None


def pola_centrali(tekst):
    """Nazwy pol JSON wystawiane przez wifiZbudujJsonStatus() i funkcje pomocnicze wolane z
    niej z buforem j (wzorzec: nazwa(j) albo nazwa(j, ...))."""
    glowna = cialo_funkcji(tekst, "String wifiZbudujJsonStatus() {")
    if glowna is None:
        return None, []
    zrodla = [("wifiZbudujJsonStatus", glowna)]
    for nazwa in sorted(set(re.findall(r"\b([A-Za-z_]\w*)\(j[,)]", glowna))):
        m = re.search(r"\n(?:static\s+)?void\s+" + re.escape(nazwa) + r"\s*\(\s*String\s*&\s*j[^)]*\)\s*\{", tekst)
        if m:
            c = cialo_funkcji(tekst, m.group(0).lstrip("\n"))
            if c:
                zrodla.append((nazwa, c))
    pola = set()
    for _, c in zrodla:
        # Klucz JSON w zrodle C++: \"nazwa\": (w F("...") albo zwyklym literale).
        pola.update(re.findall(r'\\"([A-Za-z_][A-Za-z_0-9]*)\\":', c))
    return pola, [n for n, _ in zrodla]


def pola_panelu(tekst):
    def lista(nazwa):
        m = re.search(re.escape(nazwa) + r"\[\]\s*=\s*\{(.*?)\};", tekst, re.S)
        if not m:
            return None
        bez_komentarzy = re.sub(r"//[^\n]*", "", m.group(1))
        return re.findall(r'"([A-Za-z_][A-Za-z_0-9]*)"', bez_komentarzy)
    glowne = lista("POLA_STATUSU_CZYTANE")
    if glowne is None:
        return None
    # CZUJNIKI_SYMULOWANE[] = { {"symulacja_zewn", "zewn"}, ... } - pole JSON to pierwszy napis
    # w kazdej parze (tak samo czyta je poleStatusuCzytane()).
    m = re.search(r"CZUJNIKI_SYMULOWANE\[\]\s*=\s*\{(.*?)\};", tekst, re.S)
    czujniki = re.findall(r'\{\s*"([A-Za-z_][A-Za-z_0-9]*)"\s*,', m.group(1)) if m else []
    return list(glowne) + czujniki


def main(argv):
    pokaz = "--pokaz" in argv
    argv = [a for a in argv if a != "--pokaz"]
    centrala = argv[0] if argv else najnowsza_centrala()
    if not centrala or not os.path.isfile(centrala):
        print("    nie znaleziono pliku centrali (firmware-patch/v3.*/src/main_centrala.cpp)")
        return 2
    if not os.path.isfile(NAGLOWEK_PANELU):
        print("    nie znaleziono %s" % NAGLOWEK_PANELU)
        return 2
    tc = open(centrala, encoding="utf-8", errors="replace").read()
    tp = open(NAGLOWEK_PANELU, encoding="utf-8", errors="replace").read()
    m = re.search(r'FIRMWARE_VERSION\s*=\s*"([^"]+)"', tc)
    wersja = m.group(1) if m else "?"
    pc, funkcje = pola_centrali(tc)
    if pc is None:
        print("    w %s nie ma String wifiZbudujJsonStatus() {" % centrala)
        return 2
    pp = pola_panelu(tp)
    if pp is None:
        print("    w %s nie ma POLA_STATUSU_CZYTANE[]" % NAGLOWEK_PANELU)
        return 2
    print("    centrala %s (%s): /api/status = %s -> %d pol" % (wersja, os.path.relpath(centrala, REPO), " + ".join(funkcje), len(pc)))
    print("    panel czyta %d pol (POLA_STATUSU_CZYTANE + symulacja_*), wyjatki: %s" % (len(pp), ", ".join(sorted(WYJATKI)) or "-"))
    if pokaz:
        print("    pola centrali: " + " ".join(sorted(pc)))
    brak = [p for p in pp if p not in pc and p not in WYJATKI]
    zbedne_wyjatki = [w for w in WYJATKI if w in pc]
    for p in brak:
        print("    BRAK W /api/status CENTRALI: \"%s\" - panel to czyta, centrala %s tego nie wysyla (panel dostanie 0/false/pusty napis)" % (p, wersja))
    for w in zbedne_wyjatki:
        print("    WYJATEK NIEPOTRZEBNY: \"%s\" - centrala %s juz to wysyla, usun z WYJATKI" % (w, wersja))
    if brak or zbedne_wyjatki:
        return 1
    print("    zgodne: kazde pole czytane przez panel jest w /api/status centrali %s" % wersja)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
