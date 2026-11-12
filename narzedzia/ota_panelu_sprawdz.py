#!/usr/bin/env python3
# ============================================================================
# Sprawdzenie PRZED aktualizacja panelu LCD przez centrale i Telegram
# (komenda /update_panel albo przycisk "Aktualizuj Panel LCD").
# ============================================================================
#   python3 narzedzia/ota_panelu_sprawdz.py
#       pyta GitHuba o to samo wydanie, o ktore zapyta centrala, i mowi,
#       co sie stanie po /update_panel
#   python3 narzedzia/ota_panelu_sprawdz.py --tag 3.58.6 --bin sciezka/firmware.bin
#       sprawdza wydanie, ktore dopiero chcesz opublikowac: tag i plik
#       (PlatformIO: LCD-Centrali/.pio/build/esp32dev/firmware.bin)
#   python3 narzedzia/ota_panelu_sprawdz.py --autotest
#       sprawdza samo narzedzie na przykladowych danych (bez sieci)
#
# Po co: centrala uznaje aktualizacje panelu za wykonana TYLKO wtedy, gdy
# panel zglosi wersje identyczna co do znaku z tagiem wydania na GitHubie
# (main_centrala.cpp: "zgloszona == g_panelOtaWersja"). Gdy teksty sie roznia,
# panel pobiera i wgrywa ten sam plik przy kazdym starcie, a centrala blokuje
# /update i /update_panel. To narzedzie wylapuje taka sytuacje przed
# wyslaniem komendy.
#
# Niczego nie zmienia: czyta zrodla w repozytorium i (bez --tag) jedna
# odpowiedz api.github.com. Nie laczy sie z centrala ani z panelem.
# Kod wyjscia: 0 = mozna (albo nic sie nie stanie), 1 = NIE wysylaj komendy,
# 2 = narzedzie nie moglo sprawdzic (brak sieci, brak pliku).
#
# Opis calej drogi i tego, czego NIE da sie sprawdzic z PC:
# RAPORT_OTA_PANELU_PRZEZ_CENTRALE.md
# ============================================================================
import argparse
import glob
import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DYSK_CENTRALI_B = 0x9E0000   # partycja systemu plikow centrali (partitions.csv projektu centrali): 10 354 688 B
UKLADY = {0x0000: 'ESP32', 0x0002: 'ESP32-S2', 0x0005: 'ESP32-C3', 0x0009: 'ESP32-S3',
          0x000C: 'ESP32-C2', 0x000D: 'ESP32-C6', 0x0010: 'ESP32-H2'}


def b(n):
    return format(int(n), ',').replace(',', ' ')


# --- zrodla ------------------------------------------------------------------
def domyslna_centrala():
    """Najnowsza paczka centrali w firmware-patch/ (wg numeru w nazwie katalogu)."""
    kandydaci = glob.glob(os.path.join(REPO, 'firmware-patch', 'v*', 'src', 'main_centrala.cpp'))

    def klucz(sciezka):
        m = re.search(r'v(\d+)\.(\d+)\.(\d+)', sciezka)
        return tuple(int(x) for x in m.groups()) if m else (0, 0, 0)
    return max(kandydaci, key=klucz) if kandydaci else None


def iloczyn(tekst, nazwa):
    m = re.search(r'\b%s\b\s*=\s*([0-9UL *]+);' % re.escape(nazwa), tekst)
    if not m:
        raise ValueError('w pliku centrali nie ma stalej %s' % nazwa)
    wynik = 1
    for czynnik in m.group(1).split('*'):
        wynik *= int(re.sub(r'[UL\s]', '', czynnik))
    return wynik


def czytaj_zrodla(sciezka_panelu, sciezka_centrali, sciezka_partycji):
    z = {}
    panel = open(sciezka_panelu, encoding='utf-8', errors='replace').read()
    m = re.search(r'FIRMWARE_VERSION_PANEL\s*=\s*"([^"]*)"', panel)
    if not m:
        raise ValueError('nie znalazlem FIRMWARE_VERSION_PANEL w %s' % sciezka_panelu)
    z['wersja_panelu'] = m.group(1)

    cen = open(sciezka_centrali, encoding='utf-8', errors='replace').read()

    def tekst(nazwa):
        mm = re.search(r'\b%s\s*=\s*"([^"]*)"' % nazwa, cen)
        if not mm:
            raise ValueError('w pliku centrali nie ma %s' % nazwa)
        return mm.group(1)
    z['wlasciciel'], z['repo'], z['plik'] = tekst('GITHUB_OWNER'), tekst('GITHUB_REPO'), tekst('GITHUB_ASSET_PANEL')
    z['wersja_centrali'] = tekst('FIRMWARE_VERSION')
    # Dwa miejsca, od ktorych zalezy wynik: potwierdzenie i "panel juz ma te wersje".
    z['rownosc_scisla'] = ('zgloszona == g_panelOtaWersja' in cen) and ('tag == g_panelWersjaZgloszona' in cen)
    z['limit_centrali'] = iloczyn(cen, 'FS_PANEL_FIRMWARE_MAX_B')
    rezerwy = sum(iloczyn(cen, n) for n in ('FS_CRITICAL_FREE_MIN_B', 'FS_RESERVE_COREDUMP_B', 'FS_RESERVE_HIST_SHORT_B',
                                              'FS_RESERVE_HIST_LONG_B', 'FS_PANEL_FIRMWARE_MAX_B'))
    z['rezerwy_centrali'] = rezerwy

    z['partycja_panelu'] = None
    for linia in open(sciezka_partycji, encoding='utf-8', errors='replace'):
        pola = [p.strip() for p in linia.split('#')[0].split(',')]
        if len(pola) >= 5 and pola[1] == 'app' and pola[2].startswith('ota_'):
            z['partycja_panelu'] = int(pola[4], 0)
            break
    if not z['partycja_panelu']:
        raise ValueError('nie znalazlem partycji programu w %s' % sciezka_partycji)
    return z


# --- GitHub: to samo zapytanie, ktore wysyla centrala ---------------------------
def najnowsze_wydanie(wlasciciel, repo):
    adres = 'https://api.github.com/repos/%s/%s/releases?per_page=1' % (wlasciciel, repo)
    zadanie = urllib.request.Request(adres, headers={'User-Agent': 'SterownikCO-sprawdzenie'})
    with urllib.request.urlopen(zadanie, timeout=20) as odp:
        lista = json.loads(odp.read().decode('utf-8'))
    if not lista:
        return None
    w = lista[0]
    return {'tag': w['tag_name'], 'data': (w.get('published_at') or '')[:10],
            'pliki': {a['name']: a['size'] for a in w.get('assets', [])}}


# --- ocena: czysta funkcja (sprawdzana w --autotest) --------------------------
def ocena(tag, pliki, z, panel_zglasza=None):
    """Zwraca (kod, linie). kod: 0 = mozna / nic sie nie stanie, 1 = nie wysylaj komendy."""
    linie, kod = [], 0
    wersja = z['wersja_panelu']
    if panel_zglasza is not None and panel_zglasza == tag:
        linie.append('NIC: panel zglasza juz "%s" - centrala odpowie "Panel juz ma najnowsza wersje".' % tag)
        return 0, linie
    if z['plik'] not in pliki:
        linie.append('NIC: w wydaniu "%s" nie ma pliku %s.' % (tag, z['plik']))
        linie.append('     Centrala odpowie: "Pobieranie firmware panelu nieudane: ... kod 404". Nic sie nie zmieni.')
        return 0, linie
    rozmiar = pliki[z['plik']]
    if rozmiar is not None:
        if rozmiar > z['limit_centrali']:
            kod = 1
            linie.append('BLAD: %s ma %s B - centrala odmowi pobrania (limit %s B).' % (z['plik'], b(rozmiar), b(z['limit_centrali'])))
        elif rozmiar > z['partycja_panelu']:
            kod = 1
            linie.append('BLAD: %s ma %s B, a partycja programu panelu %s B - panel pobierze naglowek i odmowi.' % (
                z['plik'], b(rozmiar), b(z['partycja_panelu'])))
        else:
            linie.append('OK:   rozmiar %s B miesci sie w partycji panelu (%s B).' % (b(rozmiar), b(z['partycja_panelu'])))
    if tag == wersja:
        linie.append('OK:   tag wydania "%s" = wersja w zrodlach panelu. Po wgraniu panel zglosi "%s"' % (tag, wersja))
        linie.append('      i centrala potwierdzi: "Panel zaktualizowany do %s."' % wersja)
        linie.append('      (o ile plik w wydaniu powstal z TYCH zrodel - sprawdz go opcja --bin)')
    else:
        kod = 1
        linie.append('BLAD: tag wydania "%s" rozni sie od wersji w zrodlach panelu "%s".' % (tag, wersja))
        if tag.lstrip('vV') == wersja.lstrip('vV'):
            linie.append('      Roznia sie tylko litera "v", ale centrala porownuje tekst znak po znaku.')
        linie.append('      Panel wgra plik, zglosi "%s", centrala nie uzna tego za potwierdzenie i panel' % wersja)
        linie.append('      bedzie pobieral i wgrywal ten sam plik przy KAZDYM starcie (czarny ekran w kolko).')
        linie.append('      Centrala zablokuje /update i /update_panel do czasu, az panel zglosi "%s".' % tag)
        linie.append('      Wyjscie: nadaj wydaniu tag "%s" albo zbuduj panel z FIRMWARE_VERSION_PANEL = "%s".' % (wersja, tag))
    if not z['rownosc_scisla']:
        linie.append('UWAGA: w pliku centrali nie znalazlem znanego warunku potwierdzenia - ta ocena moze byc nieaktualna.')
    return kod, linie


def sprawdz_bin(dane, tag, z):
    """Plik, ktory ma trafic do wydania jako firmware_panel.bin. Zwraca (kod, linie)."""
    linie, kod = [], 0
    if len(dane) < 24 or dane[0] != 0xE9:
        return 1, ['BLAD: to nie jest obraz programu ESP32 (pierwszy bajt nie jest 0xE9) - centrala odrzuci plik.']
    uklad = int.from_bytes(dane[12:14], 'little')
    nazwa = UKLADY.get(uklad, 'nieznany (0x%04X)' % uklad)
    if uklad != 0x0000:
        kod = 1
        linie.append('BLAD: plik jest dla ukladu %s, a panel to ESP32%s.' % (nazwa, ' - to wyglada na firmware CENTRALI' if uklad == 0x0009 else ''))
    else:
        linie.append('OK:   obraz dla ukladu ESP32.')
    if len(dane) > z['partycja_panelu']:
        kod = 1
        linie.append('BLAD: plik ma %s B, partycja programu panelu %s B.' % (b(len(dane)), b(z['partycja_panelu'])))
    else:
        linie.append('OK:   rozmiar %s B z %s B partycji (%d %%).' % (b(len(dane)), b(z['partycja_panelu']), 100 * len(dane) // z['partycja_panelu']))
    if dane[23] == 1:
        if len(dane) > 32 and hashlib.sha256(dane[:-32]).digest() == dane[-32:]:
            linie.append('OK:   plik jest caly (suma SHA-256 na koncu obrazu sie zgadza).')
        else:
            kod = 1
            linie.append('BLAD: suma SHA-256 na koncu obrazu sie nie zgadza - plik uciety albo uszkodzony.')
    else:
        linie.append('INFO: obraz nie ma sumy SHA-256 na koncu - nie da sie sprawdzic, czy plik jest caly.')
    # Tekst wersji jako osobny napis w programie (zakonczony bajtem zerowym).
    if tag.encode() + b'\x00' in dane:
        linie.append('OK:   w pliku jest napis "%s" - taka wersje panel zglosi centrali.' % tag)
    else:
        kod = 1
        linie.append('BLAD: w pliku nie ma napisu "%s". Po wgraniu panel zglosi inna wersje niz tag' % tag)
        linie.append('      wydania i bedzie wgrywal ten plik przy kazdym starcie.')
        if z['wersja_panelu'] != tag and z['wersja_panelu'].encode() + b'\x00' in dane:
            linie.append('      W pliku jest za to "%s" (wersja ze zrodel).' % z['wersja_panelu'])
    return kod, linie


# --- sprawdzenie samego narzedzia ----------------------------------------------
def obraz_testowy(uklad=0x0000, napis=b'v9.9.9', uciety=False):
    naglowek = bytearray(24)
    naglowek[0], naglowek[1] = 0xE9, 1
    naglowek[12:14] = uklad.to_bytes(2, 'little')
    naglowek[23] = 1
    tresc = bytes(naglowek) + b'\x00' * 8 + b'tag=PANEL fw=%s\x00' + napis + b'\x00' + b'\x55' * 200
    obraz = tresc + hashlib.sha256(tresc).digest()
    return obraz[:-40] + obraz[-32:] if uciety else obraz


def autotest():
    z = {'wersja_panelu': 'v9.9.9', 'plik': 'firmware_panel.bin', 'limit_centrali': 2097152, 'partycja_panelu': 2031616,
         'rownosc_scisla': True}
    przypadki = [
        ('brak pliku w wydaniu', ocena('3.57.9', {'firmware.bin': 2472384}, z), 0, 'kod 404'),
        ('tag = wersja panelu', ocena('v9.9.9', {'firmware_panel.bin': 1500000}, z), 0, 'Panel zaktualizowany do v9.9.9'),
        ('tag centrali, panel z wlasna wersja', ocena('3.58.6', {'firmware_panel.bin': 1500000}, z), 1, 'przy KAZDYM starcie'),
        ('roznica tylko w literze v', ocena('9.9.9', {'firmware_panel.bin': 1500000}, z), 1, 'tylko litera "v"'),
        ('plik wiekszy niz partycja panelu', ocena('v9.9.9', {'firmware_panel.bin': 2031617}, z), 1, 'partycja programu panelu'),
        ('plik wiekszy niz limit centrali', ocena('v9.9.9', {'firmware_panel.bin': 2097153}, z), 1, 'centrala odmowi'),
        ('panel juz ma te wersje', ocena('v9.9.9', {'firmware_panel.bin': 1500000}, z, panel_zglasza='v9.9.9'), 0, 'juz ma najnowsza'),
        ('bin: dobry obraz', sprawdz_bin(obraz_testowy(), 'v9.9.9', z), 0, 'plik jest caly'),
        ('bin: inny napis wersji niz tag', sprawdz_bin(obraz_testowy(), '3.58.6', z), 1, 'nie ma napisu "3.58.6"'),
        ('bin: firmware centrali (ESP32-S3)', sprawdz_bin(obraz_testowy(uklad=0x0009), 'v9.9.9', z), 1, 'firmware CENTRALI'),
        ('bin: plik uciety', sprawdz_bin(obraz_testowy(uciety=True), 'v9.9.9', z), 1, 'uciety albo uszkodzony'),
        ('bin: nie obraz ESP32', sprawdz_bin(b'<!DOCTYPE html>' + b' ' * 40, 'v9.9.9', z), 1, 'nie jest obraz'),
    ]
    zle = 0
    for opis, (kod, linie), oczekiwany_kod, fragment in przypadki:
        ok = (kod == oczekiwany_kod) and any(fragment in l for l in linie)
        print('  [%s] %s' % ('OK' if ok else 'BLAD', opis))
        if not ok:
            zle += 1
            print('         kod=%d, oczekiwany %d, szukany fragment: %r' % (kod, oczekiwany_kod, fragment))
            for l in linie:
                print('         | ' + l)
    print('autotest: %d z %d przypadkow OK' % (len(przypadki) - zle, len(przypadki)))
    return 0 if zle == 0 else 1


def main():
    p = argparse.ArgumentParser(description='Sprawdzenie przed /update_panel (aktualizacja panelu LCD przez centrale).')
    p.add_argument('--tag', help='tag wydania, ktore chcesz opublikowac (bez tej opcji: najnowsze wydanie z GitHuba)')
    p.add_argument('--bin', help='plik, ktory ma byc dolaczony do wydania jako firmware_panel.bin')
    p.add_argument('--panel-ma', help='wersja, ktora panel zglasza teraz (z logu centrali: tag=OTA-PANEL ... wersja_zgloszona)')
    p.add_argument('--panel', default=os.path.join(REPO, 'LCD-Centrali', 'src', 'main.cpp'), help='main.cpp panelu')
    p.add_argument('--centrala', default=None, help='main_centrala.cpp (domyslnie najnowsza paczka w firmware-patch/)')
    p.add_argument('--autotest', action='store_true', help='sprawdz samo narzedzie na przykladowych danych')
    a = p.parse_args()
    if a.autotest:
        return autotest()

    centrala = a.centrala or domyslna_centrala()
    if not centrala:
        print('Nie znalazlem main_centrala.cpp (podaj --centrala).')
        return 2
    dane_bin = None
    if a.bin:
        try:
            dane_bin = open(a.bin, 'rb').read()
        except OSError as e:
            print('Nie moge odczytac pliku z --bin: %s' % e)
            return 2
    try:
        z = czytaj_zrodla(a.panel, centrala, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(a.panel))), 'partitions.csv'))
    except (OSError, ValueError) as e:
        print('Nie moge odczytac zrodel: %s' % e)
        return 2

    print('AKTUALIZACJA PANELU PRZEZ CENTRALE - sprawdzenie przed /update_panel')
    print('[1] zrodla')
    print('    panel:    FIRMWARE_VERSION_PANEL = "%s"  (%s)' % (z['wersja_panelu'], os.path.relpath(a.panel, REPO)))
    print('    centrala: %s, szuka pliku "%s" w repozytorium %s/%s  (%s)' % (
        z['wersja_centrali'], z['plik'], z['wlasciciel'], z['repo'], os.path.relpath(centrala, REPO)))
    print('    centrala potwierdza aktualizacje, gdy panel zglosi tekst IDENTYCZNY z tagiem wydania: %s' % (
        'tak (warunek znaleziony w kodzie)' if z['rownosc_scisla'] else 'NIE ZNALAZLEM tego warunku'))

    print('[2] wydanie')
    if a.tag:
        tag = a.tag
        pliki = {z['plik']: len(dane_bin) if dane_bin is not None else None}
        print('    planowane: tag "%s"%s' % (tag, ', plik %s' % a.bin if a.bin else ' (bez --bin: zakladam, ze plik bedzie dolaczony)'))
    else:
        try:
            wydanie = najnowsze_wydanie(z['wlasciciel'], z['repo'])
        except (urllib.error.URLError, OSError, ValueError, KeyError) as e:
            print('    Nie moge zapytac GitHuba (%s). Uzyj --tag, zeby sprawdzic bez sieci.' % e)
            return 2
        if wydanie is None:
            print('    W repozytorium nie ma zadnego wydania - centrala odpowie "Nie udalo sie sprawdzic wersji".')
            return 0
        tag, pliki = wydanie['tag'], wydanie['pliki']
        print('    najnowsze na GitHubie (to zobaczy centrala): tag "%s" z %s, pliki: %s' % (
            tag, wydanie['data'], ', '.join('%s (%s B)' % (n, b(r)) for n, r in pliki.items()) or 'brak'))

    print('[3] co zrobi /update_panel')
    kod, linie = ocena(tag, pliki, z, a.panel_ma)
    for l in linie:
        print('    ' + l)

    if dane_bin is not None:
        print('[4] plik %s' % a.bin)
        kod_bin, linie = sprawdz_bin(dane_bin, tag, z)
        kod = max(kod, kod_bin)
        for l in linie:
            print('    ' + l)

    print('[%d] czego to narzedzie NIE widzi' % (5 if dane_bin is not None else 4))
    print('    - miejsce na dysku centrali: logi i pozostale pliki moga zajmowac najwyzej %s B minus rozmiar' % b(DYSK_CENTRALI_B - z['rezerwy_centrali']))
    print('      firmware\'u panelu (dla 1,5 MB: ok. 2,9 MB). Inaczej: "Brak miejsca na bezpieczny staging panelu".')
    print('    - alarm (przegrzanie, dym) i inna aktualizacja w toku: centrala odmowi.')
    print('    - panel zaczyna pobierac dopiero po restarcie (wylacz i wlacz zasilanie panelu).')
    print('WYNIK: %s' % ('MOZNA wyslac /update_panel (albo nic sie nie stanie)' if kod == 0 else 'NIE WYSYLAJ /update_panel - patrz BLAD wyzej'))
    return kod


if __name__ == '__main__':
    sys.exit(main())
