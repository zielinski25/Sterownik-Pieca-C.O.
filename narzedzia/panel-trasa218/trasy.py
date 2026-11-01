#!/usr/bin/env python3
"""218: statyczna analiza TABELI TRAS w main_centrala.cpp.

Po co: 213 przetestowal trase pobrania, ale wolal handlery BEZPOSREDNIO.
Nie sprawdzil wiec najwazniejszej rzeczy - ze zadanie HTTP w ogole do tego
handlera dotrze. ESPAsyncWebServer wybiera handler po KOLEJNOSCI rejestracji
(`_attachHandler()` w WebServer.cpp, ref c47917d, zwraca pierwszy, ktory
pasuje), a goly napis jako URI to matcher `BackwardCompatible`, czyli
`^{uri}(/.*)?$` (WebServer.cpp, konstruktor AsyncURIMatcher). Trasa
`/history` zarejestrowana gołym napisem pasuje wiec TAKZE do `/history/long`
i `/history/link`.

Ten skrypt liczy wszystkie trasy w pliku po patchu, w kolejnosci z pliku,
i wskazuje te, ktore sa OSIAGALNE tylko teoretycznie - bo wczesniejsza trasa
przechwytuje ich adres.

Wyjscie: JSON na stdout (albo do pliku podanego argumentem).
"""
import json
import os
import re
import sys

# --- reguly matcherow: port z WebServer.cpp (ref c47917d) -----------------
# _toFlags:  "" -> All, konczy sie '*' -> Prefix (z obcieciem '*'),
#            zawiera '/*.' -> Extension, inaczej BackwardCompatible.
# matches(): Exact:      _value == path
#            Prefix:     path.startsWith(_value)
#            Backward:   _value == path || path.startsWith(_value + "/")
#            Extension:  path.startsWith(_prefix) && path.endsWith(_ext)

METHODY = {
    'HTTP_GET': 0x01,
    'HTTP_HEAD': 0x02,
    'HTTP_POST': 0x04,
    'HTTP_PUT': 0x08,
    'HTTP_PATCH': 0x10,
    'HTTP_DELETE': 0x20,
    'HTTP_OPTIONS': 0x40,
    'HTTP_ANY': 0xFF,
}


def typ_z_wyrazenia(wyrazenie):
    """Zwraca (typ, wartosc) dla wyrazenia URI z pliku."""
    w = wyrazenie.strip()
    m = re.fullmatch(r'AsyncURIMatcher::exact\("((?:[^"\\]|\\.)*)"\)', w)
    if m:
        return 'exact', m.group(1)
    m = re.fullmatch(r'AsyncURIMatcher::prefix\("((?:[^"\\]|\\.)*)"\)', w)
    if m:
        return 'prefix', m.group(1)
    m = re.fullmatch(r'AsyncURIMatcher::all\(\)', w)
    if m:
        return 'all', ''
    if len(w) >= 2 and w[0] == '"' and w[-1] == '"':
        wartosc = w[1:-1]
        if wartosc == '':
            return 'all', ''
        if wartosc.endswith('*'):
            return 'prefix', wartosc[:-1]
        if '/*.' in wartosc:
            return 'extension', wartosc
        return 'backward', wartosc
    # AsyncURIMatcher jako zmienna/konstrukcja - nie umiemy tego rozstrzygnac
    # statycznie; zwracamy typ nieznany, zeby analiza byla uczciwa.
    return 'nieznany', None


def pasuje(typ, wartosc, sciezka):
    if typ == 'all':
        return True
    if typ == 'exact':
        return sciezka == wartosc
    if typ == 'prefix':
        return sciezka.startswith(wartosc)
    if typ == 'backward':
        return sciezka == wartosc or sciezka.startswith(wartosc + '/')
    if typ == 'extension':
        prefiks, _, rozszerzenie = wartosc.partition('/*')
        return sciezka.startswith(prefiks) and sciezka.endswith(rozszerzenie[1:])
    return False


WZOR = re.compile(
    r'wifiWebServer\.on\(\s*'
    r'(?P<uri>"(?:[^"\\]|\\.)*"|AsyncURIMatcher::(?:exact|prefix|all|regex)\([^)]*\))\s*,\s*'
    r'(?P<metoda>HTTP_[A-Z]+)\s*,',
    re.S,
)


def wczytaj_trasy(tekst):
    trasy = []
    for trafienie in WZOR.finditer(tekst):
        linia = tekst.count('\n', 0, trafienie.start()) + 1
        typ, wartosc = typ_z_wyrazenia(trafienie.group('uri'))
        metoda = METHODY.get(trafienie.group('metoda'), 0)
        trasy.append({
            'linia': linia,
            'wyrazenie': ' '.join(trafienie.group('uri').split()),
            'typ': typ,
            'sciezka': wartosc,
            'metoda': trafienie.group('metoda'),
            'mask': metoda,
        })
    return trasy


def przechwycone(trasy):
    """Trasy, do ktorych zadanie o ich WLASNY adres nie dotrze.

    Dla kazdej pary (i < j): jesli trasa i pasuje do adresu trasy j i ich
    metody sie przecinaja, to zadanie trafi do i - trasa j jest martwa
    dla wlasnego adresu.
    """
    wynik = []
    for j, pozna in enumerate(trasy):
        if not pozna['sciezka']:
            continue
        for i in range(j):
            wczesna = trasy[i]
            if not wczesna['sciezka'] and wczesna['typ'] != 'all':
                continue
            if not (wczesna['mask'] & pozna['mask']):
                continue
            if pasuje(wczesna['typ'], wczesna['sciezka'], pozna['sciezka']):
                wynik.append({
                    'martwa_linia': pozna['linia'],
                    'martwa': pozna['wyrazenie'] + ' ' + pozna['metoda'],
                    'przechwytuje_linia': wczesna['linia'],
                    'przechwytuje': wczesna['wyrazenie'] + ' ' + wczesna['metoda'],
                })
                break
    return wynik


def main():
    if len(sys.argv) < 2:
        raise SystemExit('uzycie: trasy.py main_centrala.cpp [wynik.json]')
    with open(sys.argv[1], 'r', encoding='utf-8', errors='replace') as f:
        tekst = f.read()
    trasy = wczytaj_trasy(tekst)
    martwe = przechwycone(trasy)
    raport = {
        'plik': os.path.basename(sys.argv[1]),
        'tras': len(trasy),
        'gole_napisy': sum(1 for t in trasy if t['typ'] in ('backward', 'prefix', 'all')),
        'exact': sum(1 for t in trasy if t['typ'] == 'exact'),
        'nieznane': sum(1 for t in trasy if t['typ'] == 'nieznany'),
        'martwe': martwe,
        'tabela': trasy,
    }
    tekst_wyniku = json.dumps(raport, indent=2, ensure_ascii=False) + '\n'
    if len(sys.argv) > 2:
        with open(sys.argv[2], 'w', encoding='utf-8') as f:
            f.write(tekst_wyniku)
    else:
        sys.stdout.write(tekst_wyniku)


if __name__ == '__main__':
    main()
