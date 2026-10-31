#!/usr/bin/env python3
# [ETAP 223] v3.56.4 -> v3.56.5
# Blok historii (bramka, obiekt Pobranie, zrodlo losowosci, bilety) stal
# WEWNATRZ wifiZarejestrujTrasy(); wewnatrz funkcji nie wolno definiowac
# funkcji statycznych. Przenosimy blok PRZED funkcje, na zakres globalny.
# Tresci nie zmieniamy - tylko polozenie. Wersja podniesiona na v3.56.5.
import re
import sys

ZRODLO = "/tmp/p221/main_centrala.cpp"
CEL = "/tmp/p221/main_centrala_v3565.cpp"

START = "// [v3.56.0 HISTORIA-POBRANIE] Bramka trasy pobrania:"
KONIEC = "static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;"
FUNKCJA = "void wifiZarejestrujTrasy() {"


def zamaskuj_surowe(t):
    wz = re.compile(r'R"([A-Za-z0-9_]*)\(', re.S)
    out = []
    i = 0
    while True:
        m = wz.search(t, i)
        if not m:
            break
        k = t.find(')' + m.group(1) + '"', m.end())
        if k < 0:
            break
        out.append(t[i:m.end()])
        out.append("".join("\n" if c == "\n" else " " for c in t[m.end():k]))
        out.append(')' + m.group(1) + '"')
        i = k + len(')' + m.group(1) + '"')
    out.append(t[i:])
    return "".join(out)


def oczysc(t):
    w = []
    i = 0
    n = len(t)
    s = "kod"
    while i < n:
        c = t[i]
        d = t[i + 1] if i + 1 < n else ""
        if s == "kod":
            if c == "/" and d == "/":
                s = "linia"
                i += 2
                continue
            if c == "/" and d == "*":
                s = "blok"
                i += 2
                continue
            if c == '"':
                s = "napis"
                w.append(" ")
                i += 1
                continue
            if c == "'":
                s = "znak"
                w.append(" ")
                i += 1
                continue
            w.append(c)
            i += 1
        elif s == "linia":
            if c == "\n":
                s = "kod"
                w.append(c)
            i += 1
        elif s == "blok":
            if c == "*" and d == "/":
                s = "kod"
                i += 2
                continue
            w.append("\n" if c == "\n" else " ")
            i += 1
        else:
            if c == "\\":
                w.append("  ")
                i += 2
                continue
            if (s == "napis" and c == '"') or (s == "znak" and c == "'"):
                s = "kod"
            elif c == "\n":
                s = "kod"
                w.append(c)
                i += 1
                continue
            w.append(" ")
            i += 1
    return "".join(w)


def otwiera(s):
    if re.match(r'^(class|struct|union)\b', s):
        return 'klasa'
    if re.match(r'^(namespace|extern)\b', s):
        return 'namespace'
    if re.match(r'^enum\b', s):
        return 'enum'
    if re.match(r'^(if|for|while|switch|else|do|catch|try)\b', s):
        return 'blok'
    if re.match(r'^[A-Za-z_][\w:<>,\s\*&]*?[\s\*&]+~?[A-Za-z_]\w*\s*\([^;]*\)\s*(const)?\s*(override)?\s*\{?\s*$', s):
        return 'funkcja'
    return 'blok'


def funkcje_w_funkcjach(tekst):
    czysty = oczysc(zamaskuj_surowe(tekst))
    wc = czysty.split("\n")
    wr = tekst.split("\n")
    stos = []
    zle = []
    for i, w in enumerate(wc):
        ot = w.count('{')
        za = w.count('}')
        zakres = stos[-1][0] if stos else 'global'
        if ot > 0:
            s = wr[i].strip()
            r = otwiera(s)
            if r == 'funkcja' and zakres == 'funkcja':
                zle.append((i + 1, s[:100]))
            for _ in range(ot):
                stos.append((r, i + 1))
        for _ in range(za):
            if stos:
                stos.pop()
    return zle


def glebokosc_w(tekst, offset):
    czysty = oczysc(zamaskuj_surowe(tekst))[:offset]
    return czysty.count("{") - czysty.count("}")


def main():
    tekst = open(ZRODLO, encoding="utf-8").read()

    print("=== PRZED: definicje funkcji w ciele funkcji ===")
    przed = funkcje_w_funkcjach(tekst)
    for nr, t in przed:
        print(f"  linia {nr}: {t}")
    print(f"  razem: {len(przed)}")

    i_start = tekst.find(START)
    i_konca = tekst.find(KONIEC)
    i_funkcji = tekst.find(FUNKCJA)
    assert i_start > 0 and i_konca > i_start and i_funkcji > 0, "nie znalazlem granic"
    i_konca += len(KONIEC)
    if tekst[i_konca] == "\n":
        i_konca += 1

    print(f"\nglebokosc nawiasow na poczatku bloku (PRZED): {glebokosc_w(tekst, i_start)}  <- 1 = w ciele funkcji")
    print(f"glebokosc nawiasow w miejscu funkcji (PRZED): {glebokosc_w(tekst, i_funkcji)}  <- 0 = zakres globalny")

    blok = tekst[i_start:i_konca]
    assert blok.count("struct HistoriaBramka212") == 1
    assert "historiaLosuj213" in blok
    assert "historiaBiletyPodpiecie" in blok

    bez = tekst[:i_start] + tekst[i_konca:]
    i_f2 = bez.find(FUNKCJA)
    nowy = bez[:i_f2] + blok + "\n" + bez[i_f2:]

    stara = 'static const char* FIRMWARE_VERSION = "v3.56.4";'
    nowa = 'static const char* FIRMWARE_VERSION = "v3.56.5";'
    assert nowy.count(stara) == 1
    nowy = nowy.replace(stara, nowa, 1)

    komentarz = (
        "// FIRMWARE_VERSION v3.56.4 -> v3.56.5 (KANDYDAT: blok historii wyniesiony z ciala funkcji; NIE WGRYWAC).\n"
        "// v3.56.5: bramka historii, obiekt Pobranie, zrodlo losowosci i bilety byly\n"
        "// wpisane W SRODKU wifiZarejestrujTrasy(). Wewnatrz funkcji nie wolno\n"
        "// definiowac funkcji statycznych, wiec kompilator zatrzymywal sie na\n"
        "// \"a function-definition is not allowed here before '{' token\" (linia 28745).\n"
        "// Caly blok przeniesiony PRZED funkcje, na zakres globalny; tresc bez zmian.\n"
        "// Trasy /history* zostaja tam, gdzie byly - wewnatrz funkcji.\n"
    )
    pierwszy = nowy.find("\n") + 1
    nowy = nowy[:pierwszy] + komentarz + nowy[pierwszy:]

    print("\n=== PO: definicje funkcji w ciele funkcji ===")
    po = funkcje_w_funkcjach(nowy)
    for nr, t in po:
        print(f"  linia {nr}: {t}")
    print(f"  razem: {len(po)}")

    i_nowy = nowy.find(START)
    print(f"\nglebokosc nawiasow na poczatku bloku (PO): {glebokosc_w(nowy, i_nowy)}  <- musi byc 0")

    if po or glebokosc_w(nowy, i_nowy) != 0:
        print("\nBLAD: poprawka nie dziala - nie zapisuje")
        return 1

    open(CEL, "w", encoding="utf-8", newline="").write(nowy)
    print(f"\nzapisano {CEL}: {len(nowy.encode())} B, {nowy.count(chr(10))} linii")
    return 0


if __name__ == "__main__":
    sys.exit(main())
