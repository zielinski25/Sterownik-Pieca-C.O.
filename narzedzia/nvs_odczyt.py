#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ============================================================================
# Odczyt CAŁEJ zawartości partycji NVS z pliku (zrzut z ESP przez esptool) -
# bez plytki, na PC, bez ESP-IDF i bez PlatformIO.
# ============================================================================
#   python narzedzia/nvs_odczyt.py zrzut.bin                 # partycja NVS albo caly flash
#            (jesli dasz caly zrzut flashu, sam znajdzie partycje NVS:
#             najpierw z tablicy partycji spod 0x8000, potem po naglowkach stron)
#   python narzedzia/nvs_odczyt.py zrzut.bin --pokaz-usuniete --maskuj
#   python narzedzia/nvs_odczyt.py zrzut.bin --json
#   python narzedzia/nvs_odczyt.py zrzut.bin --csv odczyt.csv
#   python narzedzia/nvs_odczyt.py caly_flash.bin --wykryj
#   python narzedzia/nvs_odczyt.py caly_flash.bin --offset 0x9000 --rozmiar 0x5000
#   python narzedzia/nvs_odczyt.py --tablica-partycji plik_z_0x8000.bin
#   python narzedzia/nvs_odczyt.py --autotest
#
# Skad wziac plik (skrot, pelna instrukcja: NVS_ODCZYT_JAK.md):
#   python -m esptool --port COM16 read-flash 0x8000 0xC00 tablica.bin
#   python narzedzia/nvs_odczyt.py --tablica-partycji tablica.bin
#   python -m esptool --port COM16 read-flash <offset> <rozmiar> nvs.bin
#
# UWAGA (esptool 5.x, a taki jest w PlatformIO: 5.2.0):
#   - polecenia nie maja juz koncowki .py: "esptool", nie "esptool.py"
#     (jesli "esptool" nie jest w PATH: python -m esptool ...),
#   - "--port", "--baud", "--chip" MUSZA byc PRZED podkomenda (read_flash),
#     inaczej "No such option '--port'",
#   - podkomenda nazywa sie teraz "read-flash"; stara "read_flash" jeszcze
#     dziala, ale wypisuje ostrzezenie o wycofaniu,
#   - "0x9000/0x5000" to partycja NVS PANELU LCD (LCD-Centrali/partitions.csv);
#     Centrala ma wlasny plik partycji - zeby nie zgadywac, odczytaj tablice
#     partycji z urzadzenia (0x8000, 0xC00) i podaj ten plik z --tablica-partycji.
#
# Co czyta: WSZYSTKO, co jest w partycji NVS - wszystkie przestrzenie (namespace),
# wszystkie klucze, wartosci, typy, oraz to, co zostalo po nadpisanych i usunietych
# kluczach (wpisy "usuniety", ktorych NVS juz nie zwraca przez Preferences).
#
# Czego NIE czyta: partycji programu, LittleFS, coredump - to inne partycje.
#
# Format NVS (ESP-IDF): partycja dzieli sie na strony po 4096 B. W stronie:
#   0..31    naglowek: stan (u32), numer kolejny (u32), wersja (1 B),
#            19 B rezerwy, CRC32 z bajtow 4..27
#   32..63   tablica stanow wpisow - po 2 bity na wpis, 126 wpisow
#            3 = pusty, 2 = zapisany, 0 = usuniety, 1 = blad zapisu
#   64..     wpisy po 32 B: nsIndex, typ, span, chunkIndex, CRC32,
#            klucz (16 B, zakonczony 0), dane (8 B)
#   Wpisy o typie zmiennym (tekst, blob) zajmuja "span" wpisow: naglowek ma
#   rozmiar i CRC danych, a same dane leza w kolejnych wpisach 32-bajtowych.
#   Blob wiekszy niz strona (v2) to kilka wpisow BLOB_DATA + jeden BLOB_IDX.
#
# Zrodla formatu (sprawdzone, nie z pamieci):
#   components/nvs_flash/src/nvs_page.hpp     (stany strony, EntryState, 126 wpisow)
#   components/nvs_flash/src/nvs_page.cpp     (mLoadEntryTable, writeItem, findItem)
#   components/nvs_flash/src/nvs_types.hpp    (Item, VerOffset, typy)
#   components/nvs_flash/src/nvs_types.cpp    (calculateCrc32 - CRC bez koncowego XOR)
#   components/nvs_flash/src/nvs_storage.cpp  (writeMultiPageBlob - chunki + BLOB_IDX)
#   components/nvs_flash/src/compressed_enum_table.hpp (2 bity na wpis, 16 wpisow na slowo)
#   components/nvs_flash/nvs_partition_tool/  (nvs_parser.py - ten sam uklad pol)
#
# Zgodnosc sprawdzona na obrazach wygenerowanych oficjalnym narzedziem
# esp-idf-nvs-partition-gen 0.3.0 (u8/i32/string/krotki string) - wynik zgadza
# sie bajt w bajt z tym, co czyta ten skrypt; CRC liczone tak samo jak
# w nvs_types.cpp i w tymze narzedziu: zlib.crc32(dane, 0xFFFFFFFF).
#
# Kod wyjscia: 0 = odczytane, 1 = plik nie jest partycja NVS (albo zly offset),
#              2 = blad uzycia / brak pliku.
# ============================================================================
import argparse
import csv
import io
import json
import os
import struct
import sys
import zlib

PAGE_SIZE = 4096
ENTRY_SIZE = 32
ENTRY_COUNT = 126          # nvs_page.hpp: ENTRY_COUNT = 126
HEADER_SIZE = 32
ENTRY_TABLE_OFFSET = 32
ENTRY_DATA_OFFSET = 64
MAX_KLUCZ = 15             # NVS_KEY_NAME_MAX_SIZE - 1; dluzej NVS nie przyjmie
MAX_PRZESTRZEN = 15

STRONA_STANY = {
    0xFFFFFFFF: 'pusta (nie zainicjowana)',
    0xFFFFFFFE: 'aktywna',
    0xFFFFFFFC: 'pelna',
    0xFFFFFFF8: 'przepisywanie',
}
STAN_AKTYWNA = 0xFFFFFFFE
STAN_PELNA = 0xFFFFFFFC
STAN_PRZEPISYWANIE = 0xFFFFFFF8
STAN_PUSTA = 0xFFFFFFFF

# nvs_types.hpp: ItemType
T_U8, T_I8 = 0x01, 0x11
T_U16, T_I16 = 0x02, 0x12
T_U32, T_I32 = 0x04, 0x14
T_U64, T_I64 = 0x08, 0x18
T_STR = 0x21
T_BLOB = 0x41
T_BLOB_DATA = 0x42
T_BLOB_IDX = 0x48

TYPY_LICZBOWE = {
    T_U8: ('u8', '<B'), T_I8: ('i8', '<b'),
    T_U16: ('u16', '<H'), T_I16: ('i16', '<h'),
    T_U32: ('u32', '<I'), T_I32: ('i32', '<i'),
    T_U64: ('u64', '<Q'), T_I64: ('i64', '<q'),
}
NAZWY_TYPOW = dict((k, v[0]) for k, v in TYPY_LICZBOWE.items())
NAZWY_TYPOW.update({T_STR: 'string', T_BLOB: 'blob', T_BLOB_DATA: 'blob_data',
                    T_BLOB_IDX: 'blob_index'})

# Klucze, ktorych wartosc jest sekretem - do opcjonalnego maskowania w raportach.
WZORCE_SEKRETOW = ('pass', 'pswd', 'token', 'api_key', 'auth_pass', 'secret',
                   'haslo', 'chatid')


class NvsBlad(Exception):
    pass


def crc32_nvs(dane):
    """CRC32 dokladnie tak, jak Item::calculateCrc32() i naglowek strony:
    esp_rom_crc32_le(0xFFFFFFFF, dane). To NIE jest zwykle CRC-32 (zlib.crc32):
    esp_rom_* liczy z negacja wejscia i wyjscia, wiec dla init 0xFFFFFFFF
    wychodzi zlib.crc32(dane, 0xFFFFFFFF) - i tylko tego wolno tu uzywac.
    Kontrola: crc32_nvs(b'123456789') == 0xD202D277."""
    return zlib.crc32(dane, 0xFFFFFFFF) & 0xFFFFFFFF


def crc_ok(raw, poczatek, koniec, zapisane):
    return crc32_nvs(raw[poczatek:koniec]) == zapisane


# ---------------------------------------------------------------------------
# Jedna strona NVS
# ---------------------------------------------------------------------------
class Strona(object):
    def __init__(self, numer, raw):
        self.numer = numer
        self.raw = raw
        self.stan = struct.unpack_from('<I', raw, 0)[0]
        self.sekwencja = struct.unpack_from('<I', raw, 4)[0]
        self.wersja = raw[8]
        self.crc_zapisane = struct.unpack_from('<I', raw, 28)[0]
        self.crc_poprawne = crc_ok(raw, 4, 28, self.crc_zapisane)
        self.stany_wpisow = []
        for i in range(ENTRY_COUNT):
            slowo = struct.unpack_from('<I', raw, ENTRY_TABLE_OFFSET + 4 * (i // 16))[0]
            self.stany_wpisow.append((slowo >> ((i % 16) * 2)) & 3)

    def nazwa_stanu(self):
        return STRONA_STANY.get(self.stan, 'uszkodzona (0x%08x)' % self.stan)

    def czy_do_odczytu(self):
        return (self.stan in (STAN_AKTYWNA, STAN_PELNA, STAN_PRZEPISYWANIE)
                and self.crc_poprawne)

    def wpis(self, indeks):
        """Zwraca dict z jednym wpisem albo None, gdy wpis nie jest zapisany."""
        stan = self.stany_wpisow[indeks]
        if stan == 3:
            return None
        poczatek = ENTRY_DATA_OFFSET + indeks * ENTRY_SIZE
        raw = self.raw[poczatek:poczatek + ENTRY_SIZE]
        if len(raw) < ENTRY_SIZE:
            return None
        ns, typ, span, chunk = raw[0], raw[1], raw[2], raw[3]
        crc_zap = struct.unpack_from('<I', raw, 4)[0]
        poprawny = (crc32_nvs(raw[0:4] + raw[8:32]) == crc_zap)
        klucz = raw[8:24].split(b'\x00')[0]
        # NVS wymaga klucza ASCII bez znakow sterujacych. Bajty 0xFF w kluczu
        # (albo smieci po uszkodzonym wpisie) pokazujemy jako podglad hex,
        # zeby raport nie wypluwal przypadkowych znakow.
        if klucz and all(32 <= b < 127 for b in klucz):
            klucz_txt = klucz.decode('ascii')
        elif not klucz:
            klucz_txt = ''
        else:
            klucz_txt = '(hex: %s)' % klucz.hex()
        return {
            'strona': self.numer,
            'indeks': indeks,
            'stan': {2: 'zapisany', 0: 'usuniety', 1: 'blad'}.get(stan, '?'),
            'zapisany': stan == 2,
            'ns': ns,
            'typ': typ,
            'typ_nazwa': NAZWY_TYPOW.get(typ, '0x%02x' % typ),
            'span': span,
            'chunk': chunk,
            'crc_poprawne': poprawny,
            'klucz': klucz_txt,
            'dane': raw[24:32],
            'raw': raw,
        }

    def surowe_wpisy(self, indeks, ile):
        poczatek = ENTRY_DATA_OFFSET + indeks * ENTRY_SIZE
        return self.raw[poczatek:poczatek + ile * ENTRY_SIZE]


# ---------------------------------------------------------------------------
# Cala partycja
# ---------------------------------------------------------------------------
class Partycja(object):
    def __init__(self, raw, opis=''):
        if len(raw) % PAGE_SIZE:
            raise NvsBlad('rozmiar %d B nie jest wielokrotnoscia strony 4096 B' % len(raw))
        self.raw = raw
        self.opis = opis
        self.strony = [Strona(i, raw[i * PAGE_SIZE:(i + 1) * PAGE_SIZE])
                       for i in range(len(raw) // PAGE_SIZE)]
        self.ostrzezenia = []
        self.usuniete = []
        self.usuniete_przestrzenie = []
        self.wpisy = []         # wszystkie wpisy "zapisany", surowe
        self.przestrzenie = {}  # indeks przestrzeni -> nazwa (np. 1 -> "piec")
        self.dane = {}          # (ns, klucz) -> dict z typem i wartoscia
        self._zbierz()

    # --- zbieranie -------------------------------------------------------
    def _zbierz(self):
        if not any(s.czy_do_odczytu() for s in self.strony):
            raise NvsBlad('w tym miejscu nie ma zadnej poprawnej strony NVS - '
                          'zly offset albo partycja nie jest partycja NVS')

        for strona in self.strony:
            if strona.stan == STAN_PUSTA:
                # naglowek nietkniety, ale reszta strony nie - NVS uznaje taka
                # strone za uszkodzona i nie czyta z niej nic. Warto to pokazac.
                reszta = strona.raw[HEADER_SIZE:]
                if reszta != b'\xff' * (PAGE_SIZE - HEADER_SIZE):
                    self.ostrzezenia.append(
                        'strona %d: naglowek pusty, a dane nie - NVS pominie te strone'
                        % strona.numer)
                continue
            if not strona.crc_poprawne:
                self.ostrzezenia.append(
                    'strona %d: zle CRC naglowka - NVS pominie cala strone' % strona.numer)
                continue
            if strona.wersja not in (0xFE, 0xFF):
                self.ostrzezenia.append(
                    'strona %d: nieznana wersja formatu 0x%02x' % (strona.numer, strona.wersja))
            if not strona.czy_do_odczytu():
                continue
            i = 0
            while i < ENTRY_COUNT:
                wpis = strona.wpis(i)
                if wpis is None:
                    i += 1
                    continue
                span = wpis['span'] if wpis['span'] not in (0, 0xFF) else 1
                if not wpis['crc_poprawne']:
                    if wpis['zapisany']:
                        self.ostrzezenia.append(
                            'strona %d wpis %d: zle CRC wpisu (klucz %r) - NVS go pomija'
                            % (strona.numer, i, wpis['klucz']))
                    i += span
                    continue
                if wpis['zapisany']:
                    wpis['payload'] = strona.surowe_wpisy(i + 1, max(0, span - 1))
                    self.wpisy.append(wpis)
                else:
                    self.usuniete.append(wpis)
                i += span

        # przestrzenie nazw: wpis w ns 0, typ u8, wartosc = indeks przestrzeni
        # (nvs_storage.cpp: write_namespace -> wpis w ns 0 z indeksem w danych)
        for w in self.wpisy:
            if w['ns'] == 0 and w['typ'] == T_U8:
                self.przestrzenie[w['dane'][0]] = w['klucz']
        for w in self.usuniete:
            if w['ns'] == 0 and w['typ'] == T_U8 and w['klucz']:
                self.usuniete_przestrzenie.append(w['klucz'])

        indeksy_blobow = []
        for w in self.wpisy:
            if w['ns'] == 0:
                continue
            if w['typ'] == T_BLOB_IDX:
                indeksy_blobow.append(w)
                continue
            if w['typ'] == T_BLOB_DATA:
                continue   # skladane nizej, razem z indeksem
            self.dane[(w['ns'], w['klucz'])] = self._wartosc(w)

        for idx in indeksy_blobow:
            self.dane[(idx['ns'], idx['klucz'])] = self._blob(idx)

        # Chunki bez indeksu: zostaja po usunietym/przepisanym blobie, NVS ich
        # nie zwraca i sprzata je przy nastepnym zapisie. Warto pokazac, bo to
        # zajete miejsce (i resztka danych, ktorej "juz nie ma").
        zakresy = {}
        for idx in indeksy_blobow:
            dane_idx = idx['dane']
            start = dane_idx[5]
            zakresy.setdefault((idx['ns'], idx['klucz']), []).append(
                (start, start + max(1, dane_idx[4])))
        osierocone = 0
        for w in self.wpisy:
            if w['typ'] != T_BLOB_DATA:
                continue
            pasuje = any(a <= w['chunk'] < b
                         for a, b in zakresy.get((w['ns'], w['klucz']), []))
            if not pasuje:
                osierocone += 1
        if osierocone:
            self.ostrzezenia.append(
                '%d chunkow bloba bez indeksu (resztka po usunietym albo przepisanym '
                'blobie) - NVS ich nie zwraca, ale zajmuja miejsce' % osierocone)

        for (ns, klucz), poz in self.dane.items():
            if len(klucz.encode('utf-8')) > MAX_KLUCZ:
                self.ostrzezenia.append(
                    'klucz %r ma %d znakow - NVS przyjmuje najwyzej %d'
                    % (klucz, len(klucz), MAX_KLUCZ))
        for ns, nazwa in self.przestrzenie.items():
            if len(nazwa.encode('utf-8')) > MAX_PRZESTRZEN:
                self.ostrzezenia.append(
                    'przestrzen %r ma %d znakow - NVS przyjmuje najwyzej %d'
                    % (nazwa, len(nazwa), MAX_PRZESTRZEN))
        bez_ns = sorted(set(ns for (ns, _) in self.dane if ns not in self.przestrzenie))
        if bez_ns:
            self.ostrzezenia.append(
                'klucze w przestrzeniach bez nazwy %s - przestrzen usunieta albo wpis '
                'przestrzeni lezy na stronie, ktorej nie ma w tym zrzucie' % bez_ns)

    def _wartosc(self, w):
        d = w['dane']
        wynik = {'typ': w['typ_nazwa'], 'strona': w['strona'], 'indeks': w['indeks'],
                 'span': w['span'], 'surowy': d.hex()}
        if w['typ'] in TYPY_LICZBOWE:
            nazwa, fmt = TYPY_LICZBOWE[w['typ']]
            rozmiar = struct.calcsize(fmt)
            wynik['wartosc'] = struct.unpack(fmt, d[:rozmiar])[0]
            wynik['typ'] = nazwa
        elif w['typ'] in (T_STR, T_BLOB):
            # varLength w nvs_types.hpp: dataSize (u16)@0, reserved (u16)@2,
            # dataCrc32 (u32)@4 - dlatego CRC czytamy od bajtu 4, nie 2.
            rozmiar = struct.unpack_from('<H', d, 0)[0]
            crc_danych = struct.unpack_from('<I', d, 4)[0]
            payload = (w['payload'] + b'\xff' * ENTRY_SIZE)[:rozmiar]
            crc_liczone = crc32_nvs(payload)
            wynik['rozmiar'] = rozmiar
            wynik['crc_danych_poprawne'] = (crc_liczone == crc_danych)
            if w['typ'] == T_STR:
                # nvs_set_str zapisuje tekst RAZEM ze znakiem konczacym, a obraz
                # z nvs_partition_gen - bez niego. Na uzytek raportu obcinamy
                # JEDEN koncowy NUL; CRC liczone jest z calosci, jak w NVS.
                tekst = payload[:-1] if payload.endswith(b'\x00') else payload
                wynik['wartosc'] = tekst.decode('utf-8', 'replace')
                if not wynik['crc_danych_poprawne']:
                    self.ostrzezenia.append(
                        'strona %d wpis %d: zle CRC tekstu %r - NVS zwroci blad odczytu'
                        % (w['strona'], w['indeks'], w['klucz']))
            else:
                wynik['wartosc'] = payload
            wynik['surowy'] = payload.hex()
        else:
            wynik['wartosc'] = bytes(d)
            wynik['typ'] = 'nieznany(0x%02x)' % w['typ']
        return wynik

    def _blob(self, idx):
        d = idx['dane']
        rozmiar = struct.unpack_from('<I', d, 0)[0]
        ile_chunkow = d[4]
        start = d[5]
        czesci = []
        for w in self.wpisy:
            if w['typ'] != T_BLOB_DATA or w['ns'] != idx['ns'] or w['klucz'] != idx['klucz']:
                continue
            if start <= w['chunk'] < start + max(1, ile_chunkow):
                czesci.append(w)
        czesci.sort(key=lambda x: x['chunk'])
        dane = b''.join(c['payload'] for c in czesci)
        # kazdy chunk ma wlasny rozmiar i CRC w swoim naglowku
        for c in czesci:
            rozmiar_ch, crc_ch = struct.unpack_from('<HI', c['dane'], 0)[0], \
                struct.unpack_from('<I', c['dane'], 4)[0]
            if crc32_nvs(c['payload'][:rozmiar_ch]) != crc_ch:
                self.ostrzezenia.append(
                    'blob %r: chunk %d ma zle CRC danych - NVS zwroci blad odczytu'
                    % (idx['klucz'], c['chunk']))
        if len(czesci) != max(1, ile_chunkow):
            self.ostrzezenia.append(
                'blob %r: indeks mowi o %d chunkach, znaleziono %d (brakujace strony?)'
                % (idx['klucz'], ile_chunkow, len(czesci)))
        if not czesci:
            self.ostrzezenia.append(
                'blob %r: jest indeks, nie ma zadnego chunku - NVS zwroci blad odczytu'
                % idx['klucz'])
        return {'typ': 'blob (%d chunk.)' % len(czesci), 'wartosc': dane[:rozmiar],
                'rozmiar': rozmiar, 'strona': idx['strona'], 'indeks': idx['indeks'],
                'span': idx['span'], 'surowy': dane[:rozmiar].hex()[:160]}

    # --- pomocnicze ------------------------------------------------------
    def nazwa_przestrzeni(self, ns):
        return self.przestrzenie.get(ns, '?%d' % ns)

    def ile_blobowych(self):
        return sum(1 for w in self.wpisy if w['typ'] in (T_BLOB_DATA, T_BLOB_IDX))

    def ile_wpisow(self):
        zapisane = sum(1 for s in self.strony if s.czy_do_odczytu()
                       for st in s.stany_wpisow if st == 2)
        usuniete = sum(1 for s in self.strony if s.czy_do_odczytu()
                       for st in s.stany_wpisow if st == 0)
        return zapisane, usuniete


# ---------------------------------------------------------------------------
# Wyjscie tekstowe
# ---------------------------------------------------------------------------
def sekret(klucz):
    k = klucz.lower()
    return any(w in k for w in WZORCE_SEKRETOW)


def odmiana(ile, jeden, kilka, wiele, forma_skrocona=False):
    """Polska odmiana: 1 strona / 2-4 strony / 5 stron (z wyjatkiem 12-14)."""
    if forma_skrocona:
        # w podsumowaniu "wpisy: 41 zapisanych" - samo slowo, bez liczby
        if ile == 1:
            return jeden
        reszta = ile % 10
        return kilka if (2 <= reszta <= 4 and not 12 <= ile % 100 <= 14) else wiele
    if ile == 1:
        return '%d %s' % (ile, jeden)
    reszta = ile % 10
    if 2 <= reszta <= 4 and not 12 <= ile % 100 <= 14:
        return '%d %s' % (ile, kilka)
    return '%d %s' % (ile, wiele)


def tekst_wartosci(w, maskuj):
    v = w['wartosc']
    if maskuj and sekret(str(w.get('klucz', ''))):
        return '(zamaskowane, %d B)' % (len(v) if isinstance(v, (bytes, str)) else 8)
    if isinstance(v, bytes):
        try:
            t = v.decode('utf-8')
            if all(32 <= ord(c) < 127 or c in '\r\n\t' for c in t):
                return repr(t) if len(t) <= 120 else repr(t[:120]) + '...'
        except UnicodeDecodeError:
            pass
        return v[:48].hex() + ('...' if len(v) > 48 else '') + ' (%d B)' % len(v)
    if isinstance(v, str):
        return repr(v) if len(v) <= 120 else repr(v[:120]) + '...'
    return str(v)


def wypisz_tekst(p, maskuj, pokaz_usuniete):
    print('NVS: %s' % (p.opis or '(plik)'))
    zapisane, usuniete = p.ile_wpisow()
    print('%d B = %s po %d B; wpisy: %d %s, %d %s'
          % (len(p.raw), odmiana(len(p.strony), 'strona', 'strony', 'stron'), PAGE_SIZE,
             zapisane, odmiana(zapisane, 'zapisany', 'zapisane', 'zapisanych', True),
             usuniete, odmiana(usuniete, 'usuniety', 'usuniete', 'usunietych', True)))
    if p.ile_blobowych():
        print('(w tym %d wpisow blobow: naglowki danych i indeksy - zlozone w klucze nizej)'
              % p.ile_blobowych())
    print('')
    print('strony:')
    for s in p.strony:
        if s.stan == STAN_PUSTA:
            print('  %2d  wolna (nie zainicjowana, wszystkie bajty 0xFF)' % s.numer)
            continue
        print('  %2d  %-24s seq=%-5d format=%s  %s'
              % (s.numer, s.nazwa_stanu(), s.sekwencja,
                 {0xFE: 'v2 (blob wielostronicowy)', 0xFF: 'v1'}.get(s.wersja, '?'),
                 'CRC ok' if s.crc_poprawne else 'CRC ZLE'))
    print('')
    if not p.przestrzenie:
        print('brak przestrzeni nazw - partycja jest pusta (albo same wpisy usuniete)')
    for ns in sorted(p.przestrzenie):
        nazwa = p.przestrzenie[ns]
        klucze = sorted(k for (n, k) in p.dane if n == ns)
        print('przestrzen "%s" (indeks %d) - %d kluczy' % (nazwa, ns, len(klucze)))
        if not klucze:
            print('  (brak zapisanych kluczy)')
        for k in klucze:
            w = p.dane[(ns, k)]
            w['klucz'] = k
            print('  %-16s %-16s = %s' % (k, w['typ'], tekst_wartosci(w, maskuj)))
        print('')
    osierocone = sorted(set(k for (n, k) in p.dane if n not in p.przestrzenie))
    if osierocone:
        print('klucze bez przestrzeni nazw (usuniete przestrzenie): %s' % ', '.join(osierocone))
    if pokaz_usuniete and p.usuniete:
        print('')
        print('wpisy USUNIETE (NVS ich nie zwraca, ale sa jeszcze w pamieci):')
        for w in p.usuniete:
            print('  strona %d wpis %3d  ns=%-3d %-18s %-14s klucz=%r'
                  % (w['strona'], w['indeks'], w['ns'], p.nazwa_przestrzeni(w['ns']),
                     w['typ_nazwa'], w['klucz']))
    if p.ostrzezenia:
        print('')
        print('ostrzezenia:')
        for o in p.ostrzezenia:
            print('  - %s' % o)


def wypisz_json(p, maskuj, pokaz_usuniete):
    out = {
        'rozmiar_b': len(p.raw),
        'strony': [],
        'przestrzenie': {},
        'ostrzezenia': p.ostrzezenia,
    }
    for s in p.strony:
        out['strony'].append({'numer': s.numer, 'stan': s.nazwa_stanu(),
                              'sekwencja': s.sekwencja, 'format': s.wersja,
                              'crc_ok': s.crc_poprawne})
    for ns in sorted(p.przestrzenie):
        nazwa = p.przestrzenie[ns]
        grupa = {}
        for k in sorted(k for (n, k) in p.dane if n == ns):
            w = p.dane[(ns, k)]
            w = dict(w)
            w['klucz'] = k
            if maskuj and sekret(k):
                grupa[k] = {'typ': w['typ'], 'wartosc': '(zamaskowane)'}
                continue
            v = w['wartosc']
            grupa[k] = {'typ': w['typ']}
            if isinstance(v, bytes):
                grupa[k]['wartosc_hex'] = v.hex()
                grupa[k]['rozmiar'] = len(v)
            else:
                grupa[k]['wartosc'] = v
        out['przestrzenie'][nazwa] = grupa
    if pokaz_usuniete:
        out['usuniete'] = [{'przestrzen': p.nazwa_przestrzeni(w['ns']), 'klucz': w['klucz'],
                            'typ': w['typ_nazwa'], 'strona': w['strona']}
                           for w in p.usuniete]
    print(json.dumps(out, indent=2, ensure_ascii=False))


def wypisz_csv(p, sciezka):
    """CSV w formacie wejsciowym narzedzia esp-idf-nvs-partition-gen - mozna
    z niego odtworzyc partycje (write_flash) na druga plytke."""
    with io.open(sciezka, 'w', encoding='utf-8', newline='') as f:
        w = csv.writer(f)
        w.writerow(['key', 'type', 'encoding', 'value'])
        for ns in sorted(p.przestrzenie):
            w.writerow([p.przestrzenie[ns], 'namespace', '', ''])
            for k in sorted(k for (n, k) in p.dane if n == ns):
                d = p.dane[(ns, k)]
                v = d['wartosc']
                if isinstance(v, str):
                    w.writerow([k, 'data', 'string', v])
                elif isinstance(v, bytes):
                    w.writerow([k, 'data', 'hex2bin', v.hex()])
                else:
                    w.writerow([k, 'data', d['typ'], v])
    print('zapisano CSV: %s (%d przestrzeni, %d kluczy)'
          % (sciezka, len(p.przestrzenie), len(p.dane)))



# ---------------------------------------------------------------------------
# Tablica partycji ESP-IDF (offset 0x8000, zwykle 0xC00 bajtow)
# ---------------------------------------------------------------------------
# Format wpisu (esp_partition_info_t, components/bootloader_support/include/
# esp_flash_partitions.h, sprawdzone w ESP-IDF v5.3):
#   0..1   magic 0xAA 0x50 (u16 LE 0x50AA)
#   2      type:  0x00 = aplikacja, 0x01 = dane
#   3      subtype - patrz enum esp_partition_subtype_t W TYM SAMYM naglowku:
#            dane: 0x00 ota (wybor partycji), 0x01 phy, 0x02 NVS !!!,
#                  0x03 coredump, 0x04 nvs_keys, 0x05 efuse, 0x06 undefined,
#                  0x80 esphttpd, 0x81 fat, 0x82 spiffs, 0x83 littlefs
#            app:  0x00 fabryczna, 0x10..0x1F ota_0..ota_15, 0x20 test
#          (uwaga: NVS to 0x02 - 0x01 to PHY; latwo sie pomylic)
#   4..7   offset (u32 LE), 8..11 rozmiar (u32 LE)
#   12..27 etykieta (16 B, konczona zerem), 28..31 flagi
#   Koniec tablicy: wpis 0xFF..FF albo wpis MD5 (pierwsze dwa bajty 0xEB 0xEB).
TABLICA_PARTYCJI_OFFSET = 0x8000
TABLICA_PARTYCJI_BAJTOW = 0xC00
MAGIC_PARTYCJI = (0xAA, 0x50)
TYP_APP = 0x00
TYP_DANYCH = 0x01
PODTYP_DATA_NVS = 0x02
PODTYP_DATA_NVS_KEYS = 0x04

PODTYPY_DANYCH = {
    0x00: 'ota (wybor partycji)', 0x01: 'phy', 0x02: 'NVS', 0x03: 'coredump',
    0x04: 'nvs_keys', 0x05: 'efuse', 0x06: 'undefined',
    0x80: 'esphttpd', 0x81: 'fat', 0x82: 'spiffs', 0x83: 'littlefs',
}
PODTYPY_APP = {0x00: 'fabryczna', 0x20: 'test'}


def czytaj_tablice_partycji(raw):
    """Zwraca liste dictow z partycjami albo None, gdy to nie tablica."""
    if len(raw) < 32:
        return None
    partycje = []
    for i in range(0, len(raw) - 31, 32):
        wpis = raw[i:i + 32]
        if wpis == b'\xff' * 32:
            break
        if wpis[0] != MAGIC_PARTYCJI[0] or wpis[1] != MAGIC_PARTYCJI[1]:
            if wpis[0] == 0xEB and wpis[1] == 0xEB:
                break          # wpis z MD5 calej tablicy
            if not partycje:
                return None
            break
        typ, podtyp = wpis[2], wpis[3]
        off = struct.unpack_from('<I', wpis, 4)[0]
        rozmiar = struct.unpack_from('<I', wpis, 8)[0]
        etykieta = wpis[12:28].split(b'\x00')[0].decode('utf-8', 'replace')
        partycje.append({'typ': typ, 'podtyp': podtyp, 'offset': off,
                         'rozmiar': rozmiar, 'etykieta': etykieta})
    return partycje or None


def nazwa_podtypu(typ, podtyp):
    if typ == TYP_APP:
        if 0x10 <= podtyp <= 0x1F:
            return 'ota_%d' % (podtyp - 0x10)
        return PODTYPY_APP.get(podtyp, 'app 0x%02x' % podtyp)
    return PODTYPY_DANYCH.get(podtyp, 'dane 0x%02x' % podtyp)


def znajdz_partycje_nvs(partycje):
    """Partycja NVS to typ dane + podtyp 0x02 (nie 0x01 - to PHY)."""
    for p in partycje or []:
        if p['typ'] == TYP_DANYCH and p['podtyp'] == PODTYP_DATA_NVS:
            return p
    return None


def wypisz_tablice(partycje, opis=''):
    if not partycje:
        print('to nie wyglada na tablice partycji ESP-IDF (brak wpisu 0xAA 0x50)')
        print('tablica jest na 0x8000; zrzut:')
        print('  python -m esptool --port COM16 read_flash 0x8000 0xC00 tablica.bin')
        return 1
    print('Tablica partycji%s:' % ((' z ' + opis) if opis else ''))
    print('  %-10s %-20s %-10s %-10s %s'
          % ('etykieta', 'typ/podtyp', 'offset', 'rozmiar', 'koniec'))
    for p in partycje:
        print('  %-10s %-20s 0x%06x   0x%06x   0x%06x'
              % (p['etykieta'], '%s / %s' % ('app' if p['typ'] == TYP_APP else 'dane',
                                             nazwa_podtypu(p['typ'], p['podtyp'])),
                 p['offset'], p['rozmiar'], p['offset'] + p['rozmiar']))
    nvs = znajdz_partycje_nvs(partycje)
    if nvs is None:
        print('')
        print('UWAGA: w tej tablicy nie ma partycji "dane / NVS" (podtyp 0x02).')
        return 1
    print('')
    print('Partycja NVS: "%s", offset 0x%x, rozmiar 0x%x (%d B, %s po 4096 B).'
          % (nvs['etykieta'], nvs['offset'], nvs['rozmiar'], nvs['rozmiar'],
             odmiana(nvs['rozmiar'] // PAGE_SIZE, 'strona', 'strony', 'stron')))
    print('Zrzut i odczyt:')
    print('  python -m esptool --port COM16 read_flash 0x%x 0x%x nvs.bin'
          % (nvs['offset'], nvs['rozmiar']))
    print('  python narzedzia/nvs_odczyt.py nvs.bin')
    return 0


# ---------------------------------------------------------------------------
# Wykrywanie partycji NVS w zrzucie calego flashu
# ---------------------------------------------------------------------------
def wyglada_na_strone_nvs(raw):
    if len(raw) < PAGE_SIZE:
        return False
    stan = struct.unpack_from('<I', raw, 0)[0]
    if stan not in (STAN_AKTYWNA, STAN_PELNA, STAN_PRZEPISYWANIE, STAN_PUSTA):
        return False
    if stan == STAN_PUSTA:
        return True
    zapisane_crc = struct.unpack_from('<I', raw, 28)[0]
    return crc32_nvs(raw[4:28]) == zapisane_crc and raw[8] in (0xFE, 0xFF)


def wykryj(raw):
    """Zwraca liste (offset, rozmiar, wolne_przed, wolne_po) ciagow stron NVS.
    Wolnymi stronami nazywamy strony jeszcze nie zainicjowane (same 0xFF) -
    NVS traktuje je jak rezerwe, wiec nie wliczamy ich do samej partycji,
    tylko podajemy ile ich bylo przed i po."""
    kandydaci = []
    for off in range(0, len(raw) - PAGE_SIZE + 1, PAGE_SIZE):
        if not wyglada_na_strone_nvs(raw[off:off + PAGE_SIZE]):
            continue
        if kandydaci and kandydaci[-1][0] + kandydaci[-1][1] == off:
            kandydaci[-1][1] += PAGE_SIZE
        else:
            kandydaci.append([off, PAGE_SIZE])
    wynik = []
    for off, dl in kandydaci:
        ile = dl // PAGE_SIZE
        zainicjowane = [i for i in range(ile)
                        if struct.unpack_from('<I', raw, off + i * PAGE_SIZE)[0] != STAN_PUSTA]
        if not zainicjowane:
            continue   # sam 0xFF - NVS, ktore nigdy nic nie zapisalo
        pierwsza, ostatnia = zainicjowane[0], zainicjowane[-1]
        wynik.append((off + pierwsza * PAGE_SIZE, (ostatnia - pierwsza + 1) * PAGE_SIZE,
                      pierwsza, ile - 1 - ostatnia))
    return wynik


# ---------------------------------------------------------------------------
# Autotest: buduje obrazy NVS wlasnym pisarzem i czyta je tym samym czytnikiem,
# a dodatkowo sprawdza liczenie CRC na stalym, recznie policzonym bajcie.
# ---------------------------------------------------------------------------
NVS_AKTYWNA, NVS_PELNA = 0xFFFFFFFE, 0xFFFFFFFC


class Pisarz(object):
    """Minimalny pisarz formatu NVS - tylko na potrzeby --autotest."""

    def __init__(self, ile_stron=2):
        self.strony = [bytearray(b'\xff' * PAGE_SIZE) for _ in range(ile_stron)]
        self.nr_strony = 0
        self.wpis = 0
        self.przestrzenie = {}
        self.nastepna = 1
        self._naglowek()

    def _naglowek(self):
        s = self.strony[self.nr_strony]
        stan = NVS_AKTYWNA if self.nr_strony == len(self.strony) - 1 else NVS_PELNA
        struct.pack_into('<I', s, 0, stan)
        struct.pack_into('<I', s, 4, self.nr_strony)
        s[8] = 0xFE

    def _zamknij_strone(self):
        for i in range(len(self.strony)):
            s = self.strony[i]
            if i == len(self.strony) - 1:
                struct.pack_into('<I', s, 0, NVS_AKTYWNA)
            else:
                struct.pack_into('<I', s, 0, NVS_PELNA)
            struct.pack_into('<I', s, 28, crc32_nvs(bytes(s[4:28])))

    def przestrzen(self, nazwa):
        if nazwa in self.przestrzenie:
            return self.przestrzenie[nazwa]
        idx = self.nastepna
        self.nastepna += 1
        self.przestrzenie[nazwa] = idx
        self._wpis(0, T_U8, 1, 0xFF, nazwa, bytes([idx]) + b'\xff' * 7, b'')
        return idx

    def _zaznacz(self, indeks, stan):
        slowo_off = ENTRY_TABLE_OFFSET + 4 * (indeks // 16)
        slowo = struct.unpack_from('<I', self.strony[self.nr_strony], slowo_off)[0]
        przes = (indeks % 16) * 2
        slowo = (slowo & ~(3 << przes)) | (stan << przes)
        struct.pack_into('<I', self.strony[self.nr_strony], slowo_off, slowo)

    def _wpis(self, ns, typ, span, chunk, klucz, dane8, payload):
        calkowity = span
        if self.wpis + calkowity > ENTRY_COUNT:
            self.nr_strony += 1
            self.wpis = 0
        s = self.strony[self.nr_strony]
        off = ENTRY_DATA_OFFSET + self.wpis * ENTRY_SIZE
        b = bytearray(b'\xff' * (ENTRY_SIZE * span))
        b[0], b[1], b[2], b[3] = ns, typ, span, chunk
        kb = klucz.encode()[:15]
        b[8:24] = kb + b'\x00' * (16 - len(kb))
        b[24:32] = dane8
        struct.pack_into('<I', b, 4, crc32_nvs(bytes(b[0:4]) + bytes(b[8:32])))
        if payload:
            b[32:32 + len(payload)] = payload
        s[off:off + len(b)] = b
        for i in range(span):
            self._zaznacz(self.wpis, 2)
            self.wpis += 1

    def wartosc(self, ns, klucz, typ, wartosc):
        if typ in TYPY_LICZBOWE:
            fmt = TYPY_LICZBOWE[typ][1]
            d = struct.pack(fmt, wartosc) + b'\xff' * (8 - struct.calcsize(fmt))
            self._wpis(ns, typ, 1, 0xFF, klucz, d, b'')
            return
        if isinstance(wartosc, str):
            dane = wartosc.encode('utf-8')
            typ = T_STR
        else:
            dane = bytes(wartosc)
            typ = T_BLOB
        span = 1 + (len(dane) + ENTRY_SIZE - 1) // ENTRY_SIZE
        d8 = struct.pack('<HHI', len(dane), 0xFFFF, crc32_nvs(dane))
        self._wpis(ns, typ, span, 0xFF, klucz, d8, dane)

    def blob_wielostronicowy(self, ns, klucz, dane):
        """Zapis jak Storage::writeMultiPageBlob: chunki BLOB_DATA + BLOB_IDX."""
        chunk_start = 0x00
        pozostalo = len(dane)
        offset = 0
        nr = 0
        while pozostalo:
            # ile wpisow zostalo w biezacej stronie
            wolne_wpisy = ENTRY_COUNT - self.wpis
            if wolne_wpisy < 2:
                self.nr_strony += 1
                self.wpis = 0
                wolne_wpisy = ENTRY_COUNT
            pojemnosc = (wolne_wpisy - 1) * ENTRY_SIZE
            rozmiar = min(pozostalo, pojemnosc)
            czesc = dane[offset:offset + rozmiar]
            span = 1 + (rozmiar + ENTRY_SIZE - 1) // ENTRY_SIZE
            d8 = struct.pack('<HHI', rozmiar, 0xFFFF, crc32_nvs(czesc))
            self._wpis(ns, T_BLOB_DATA, span, chunk_start + nr, klucz, d8, czesc)
            pozostalo -= rozmiar
            offset += rozmiar
            nr += 1
        d8 = struct.pack('<IBBH', len(dane), nr, chunk_start, 0xFFFF)
        self._wpis(ns, T_BLOB_IDX, 1, 0xFF, klucz, d8, b'')

    def usun(self, ns, klucz, typ):
        for i in range(len(self.strony)):
            s = self.strony[i]
            for e in range(ENTRY_COUNT):
                off = ENTRY_DATA_OFFSET + e * ENTRY_SIZE
                if s[off] == ns and s[off + 1] == typ and \
                        s[off + 8:off + 24].split(b'\x00')[0] == klucz.encode():
                    slowo_off = ENTRY_TABLE_OFFSET + 4 * (e // 16)
                    slowo = struct.unpack_from('<I', s, slowo_off)[0]
                    przes = (e % 16) * 2
                    struct.pack_into('<I', s, slowo_off, (slowo & ~(3 << przes)))

    def bajty(self):
        self._zamknij_strone()
        return b''.join(bytes(s) for s in self.strony)


def autotest():
    zle = 0
    nie = 0

    def cicho(f, *a, **kw):
        """Wywolaj funkcje, ktora cos wypisuje, i schowaj jej wyjscie."""
        import contextlib
        with contextlib.redirect_stdout(io.StringIO()):
            return f(*a, **kw)

    def sprawdz(opis, warunek, szczegol=''):
        nonlocal zle, nie
        nie += 1
        if not warunek:
            zle += 1
            print('  NIE OK: %s %s' % (opis, szczegol))

    print('autotest nvs_odczyt.py')

    # 1) CRC - policzony recznie z definicji, niezaleznie od zlib. NVS porownuje
    #    esp_rom_crc32_le(0xffffffff, ...), wiec wynik to ~crc_recznie(~init).
    def reczne_crc(dane):
        crc = 0x00000000          # esp_rom_crc32_le(~0, ...) startuje od zera
        for b in dane:
            crc ^= b
            for _ in range(8):
                crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
        return crc ^ 0xFFFFFFFF   # esp_rom_crc32_le zwraca zanegowany wynik
    sprawdz('CRC z definicji == crc32_nvs', reczne_crc(b'123456789') == crc32_nvs(b'123456789'),
            hex(crc32_nvs(b'123456789')))
    sprawdz('CRC liczony tak jak w nvs_page/nvs_types',
            crc32_nvs(b'123456789') == 0xD202D277, hex(crc32_nvs(b'123456789')))
    sprawdz('CRC pustych danych', crc32_nvs(b'') == 0xFFFFFFFF)

    # 2) Pelny obraz: liczba, tekst krotszy i dluzszy niz wpis (span>1),
    #    blob wielostronicowy, klucz usuniety.
    pis = Pisarz(6)
    ns = pis.przestrzen('piec')
    pis.wartosc(ns, 'wybor', T_I32, 7)
    pis.wartosc(ns, 'tempOn', T_U8, 60)
    pis.wartosc(ns, 'antystopWlacz', T_U8, 1)
    pis.wartosc(ns, 'krotki', T_STR, 'abc')
    dlugi = 'x' * 100
    pis.wartosc(ns, 'dlugi', T_STR, dlugi)
    pis.wartosc(ns, 'pusty', T_STR, '')
    wifi = pis.przestrzen('wifi')
    pis.wartosc(wifi, 'ssid0', T_STR, 'MojeWiFi')
    pis.wartosc(wifi, 'count', T_I32, 1)
    blob = bytes(range(256)) * 40          # 10240 B - wiecej niz strona, kilka chunkow
    pis.wartosc(pis.przestrzen('otapanel'), 'zapis', T_BLOB, b'krotki zapis')
    pis.blob_wielostronicowy(wifi, 'duzy', blob)
    pis.wartosc(ns, 'stary', T_U8, 9)
    pis.usun(ns, 'stary', T_U8)
    obraz = pis.bajty()

    p = Partycja(obraz, 'autotest')
    sprawdz('wczytane 6 stron', len(p.strony) == 6)
    sprawdz('przestrzenie piec/wifi/otapanel',
            set(p.przestrzenie.values()) == {'piec', 'wifi', 'otapanel'},
            p.przestrzenie)
    sprawdz('wybor == 7', p.dane[(ns, 'wybor')]['wartosc'] == 7)
    sprawdz('tempOn == 60', p.dane[(ns, 'tempOn')]['wartosc'] == 60)
    sprawdz('krotki tekst', p.dane[(ns, 'krotki')]['wartosc'] == 'abc')
    sprawdz('tekst 100 B (span > 1)', p.dane[(ns, 'dlugi')]['wartosc'] == dlugi,
            p.dane[(ns, 'dlugi')].get('wartosc'))
    sprawdz('pusty tekst', p.dane[(ns, 'pusty')]['wartosc'] == '')
    sprawdz('ssid0', p.dane[(wifi, 'ssid0')]['wartosc'] == 'MojeWiFi')
    ns_ota = pis.przestrzenie['otapanel']
    sprawdz('krotki blob', p.dane[(ns_ota, 'zapis')]['wartosc'] == b'krotki zapis')
    duzy = p.dane[(wifi, 'duzy')]
    sprawdz('blob wielostronicowy: 10240 B', duzy.get('rozmiar') == len(blob), duzy.get('rozmiar'))
    sprawdz('blob wielostronicowy: tresc', duzy.get('wartosc') == blob)
    sprawdz('klucz usuniety nie jest zwracany', (ns, 'stary') not in p.dane)
    sprawdz('klucz usuniety jest widoczny w historii',
            any(w['klucz'] == 'stary' for w in p.usuniete))
    sprawdz('brak ostrzezen', not p.ostrzezenia, p.ostrzezenia)

    # 3) Uszkodzona strona: zle CRC naglowka -> strona pomijana, ostrzezenie.
    zepsuty = bytearray(obraz)
    struct.pack_into('<I', zepsuty, 28, 0)   # strona 0: zle CRC
    p2 = Partycja(bytes(zepsuty), 'zepsuty')
    # strona 0 jest pelna (przepisana dalej) - jej klucze zyja tez na pozniejszych
    # stronach, wiec po jej odrzuceniu czytnik nadal widzi przestrzen "piec",
    # ale nie widzi juz wpisu, ktory byl TYLKO na niej (tu: "pusty" jest na
    # stronie 0, a "duzy" na stronach 1-3).
    sprawdz('ostrzezenie o zlym CRC', any('zle CRC naglowka' in o for o in p2.ostrzezenia),
            p2.ostrzezenia)
    sprawdz('zla strona nie jest czytana', 0 not in [w['strona'] for w in p2.wpisy])

    # 4) Zly offset: dane, ktore nie sa NVS.
    try:
        Partycja(b'\x00' * (PAGE_SIZE * 2), 'zly')
        sprawdz('obraz nie-NVS zglasza blad', False)
    except NvsBlad:
        sprawdz('obraz nie-NVS zglasza blad', True)

    # 5) Wykrywanie w zrzucie calego flashu: NVS na offsetcie 0x9000, przed nim
    #    obraz programu (0xE9), na koncu partycji dwie strony jeszcze nietkniete.
    flash = (bytearray(b'\xe9' * 0x9000) + obraz + bytearray(b'\xff' * (2 * PAGE_SIZE))
             + bytearray(b'\x00' * 0x1000))
    znalezione = wykryj(bytes(flash))
    sprawdz('wykrycie offsetu 0x9000 i 6 stron',
            any(off == 0x9000 and dl == len(obraz) for off, dl, _, _ in znalezione),
            znalezione)
    # wolne strony: 2 ostatnie strony obrazu testowego sa nietkniete (0xFF)
    sprawdz('wolne strony po policzone', any(po == 2 for _, _, _, po in znalezione), znalezione)
    sprawdz('wolne strony przed = 0 (przed partycja nie ma stron 0xFF)',
            any(przed == 0 for _, _, przed, _ in znalezione), znalezione)

    # 5b) Osierocone chunki bloba: indeks usuniety, chunki zostaly.
    z = bytearray(obraz)
    for strona_nr in range(len(z) // PAGE_SIZE):
        strona = Strona(strona_nr, bytes(z[strona_nr * PAGE_SIZE:(strona_nr + 1) * PAGE_SIZE]))
        for i in range(ENTRY_COUNT):
            w = strona.wpis(i)
            if w and w['zapisany'] and w['typ'] == T_BLOB_IDX and w['klucz'] == 'duzy':
                off_tab = strona_nr * PAGE_SIZE + ENTRY_TABLE_OFFSET + 4 * (i // 16)
                slowo = struct.unpack_from('<I', z, off_tab)[0] & ~(3 << ((i % 16) * 2))
                struct.pack_into('<I', z, off_tab, slowo)
                off_hdr = strona_nr * PAGE_SIZE + 28
                struct.pack_into('<I', z, off_hdr,
                                 crc32_nvs(bytes(z[strona_nr * PAGE_SIZE + 4:off_hdr])))
    p3 = Partycja(bytes(z), 'bez-indeksu')
    sprawdz('blob bez indeksu nie jest zwracany', (wifi, 'duzy') not in p3.dane)
    sprawdz('osierocone chunki zgloszone',
            any('bez indeksu' in o for o in p3.ostrzezenia), p3.ostrzezenia)

    # 6) CSV w formacie wejsciowym narzedzia Espressif.
    import tempfile
    with tempfile.NamedTemporaryFile('r', suffix='.csv', delete=False, encoding='utf-8') as f:
        nazwa = f.name
    cicho(wypisz_csv, p, nazwa)
    with io.open(nazwa, encoding='utf-8') as f:
        linie = f.read().splitlines()
    os.unlink(nazwa)
    sprawdz('CSV: naglowek', linie[0] == 'key,type,encoding,value')
    sprawdz('CSV: wiersz przestrzeni', 'piec,namespace,,' in linie, linie[:4])
    sprawdz('CSV: tekst', 'dlugi,data,string,%s' % dlugi in linie)
    sprawdz('CSV: liczba', any(l.startswith('wybor,data,i32,7') for l in linie), linie)

    # 7) Tablica partycji ESP-IDF.
    def wpis_part(typ, podtyp, off, rozmiar, etykieta):
        e = bytearray(32)
        e[0], e[1] = 0xAA, 0x50
        e[2], e[3] = typ, podtyp
        struct.pack_into('<I', e, 4, off)
        struct.pack_into('<I', e, 8, rozmiar)
        lab = etykieta.encode()
        e[12:12 + len(lab)] = lab
        return bytes(e)

    def tablica(wpisy, zakonczenie='md5'):
        t = bytearray(b'\xff' * TABLICA_PARTYCJI_BAJTOW)
        o = 0
        for w in wpisy:
            t[o:o + 32] = w
            o += 32
        if zakonczenie == 'md5':
            m = bytearray(32)
            m[0], m[1] = 0xEB, 0xEB
            t[o:o + 32] = m
        return bytes(t)

    # 7a) typowa tablica: NVS to podtyp 0x02 (0x01 to PHY - najlatwiejsza pomylka)
    t1 = czytaj_tablice_partycji(tablica([
        wpis_part(TYP_DANYCH, 0x02, 0x9000, 0x5000, 'nvs'),
        wpis_part(TYP_DANYCH, 0x01, 0xE000, 0x2000, 'phy_init'),
        wpis_part(TYP_APP, 0x10, 0x10000, 0x1F0000, 'ota_0'),
    ]))
    sprawdz('tablica: 3 partycje', t1 is not None and len(t1) == 3, t1)
    nvs1 = znajdz_partycje_nvs(t1)
    sprawdz('tablica: NVS znalezione po podtypie 0x02',
            nvs1 and nvs1['offset'] == 0x9000 and nvs1['rozmiar'] == 0x5000, nvs1)
    sprawdz('tablica: podtyp 0x01 to phy, nie NVS',
            nazwa_podtypu(TYP_DANYCH, 0x01) == 'phy', nazwa_podtypu(TYP_DANYCH, 0x01))
    sprawdz('tablica: nazwa ota_1', nazwa_podtypu(TYP_APP, 0x11) == 'ota_1')
    sprawdz('tablica: etykieta odczytana', t1[2]['etykieta'] == 'ota_0')

    # 7b) koniec tablicy samymi 0xFF - czytanie musi sie zatrzymac
    t2 = czytaj_tablice_partycji(tablica([
        wpis_part(TYP_DANYCH, 0x02, 0x9000, 0x5000, 'nvs')], zakonczenie='ff'))
    sprawdz('tablica: koniec 0xFF konczy czytanie', t2 is not None and len(t2) == 1, t2)

    # 7c) bez partycji NVS -> szukanie zwraca None, wypisanie zwraca 1
    t3 = czytaj_tablice_partycji(tablica([
        wpis_part(TYP_DANYCH, 0x03, 0x3F0000, 0x10000, 'coredump')]))
    sprawdz('tablica bez NVS', znajdz_partycje_nvs(t3) is None)
    sprawdz('wypisz_tablice bez NVS -> 1', cicho(wypisz_tablice, t3) == 1)

    # 7d) nie-tablica (same zera) -> None, a nie wyjatek
    sprawdz('same zera to nie tablica',
            czytaj_tablice_partycji(b'\x00' * 0xC00) is None)
    sprawdz('wypisz_tablice na smieciach -> 1',
            cicho(wypisz_tablice, czytaj_tablice_partycji(b'\x00' * 0xC00)) == 1)

    # 8) Caly zrzut flashu podany zamiast partycji: automat znajduje NVS
    #    po tablicy partycji (offset 0x8000).
    caly = bytearray(b'\xff' * 0x40000)
    caly[TABLICA_PARTYCJI_OFFSET:TABLICA_PARTYCJI_OFFSET + TABLICA_PARTYCJI_BAJTOW] = \
        tablica([wpis_part(TYP_DANYCH, 0x02, 0x9000, len(obraz), 'nvs')])
    caly[0x9000:0x9000 + len(obraz)] = obraz
    traf = znajdz_partycje_w_pliku(bytes(caly))
    sprawdz('automat: NVS z tablicy partycji',
            traf and traf[0] == 0x9000 and traf[1] == len(obraz), traf)
    sprawdz('automat: zrodlo opisane jako tablica partycji',
            traf and 'tablica partycji' in traf[2], traf)

    # 8b) bez tablicy partycji - zostaje heurystyka po naglowkach stron
    caly2 = bytearray(b'\xff' * 0x40000)
    caly2[0x9000:0x9000 + len(obraz)] = obraz
    traf2 = znajdz_partycje_w_pliku(bytes(caly2))
    sprawdz('automat: heurystyka bez tablicy',
            traf2 and traf2[0] == 0x9000 and 'naglowki stron' in traf2[2], traf2)

    # 8c) "wyglada na partycje" nie moze uznac pustej strony 0xFF za NVS
    sprawdz('puste 0xFF to nie partycja NVS', not wyglada_na_partycje(b'\xff' * 0x2000))
    sprawdz('wlasny obraz to partycja NVS', wyglada_na_partycje(obraz))

    # 9) CSV -> generator Espressif -> nasz czytnik: pelne kolko.
    #    CSV to format wejsciowy esp-idf-nvs-partition-gen, wiec jesli generator
    #    jest zainstalowany, sprawdzamy prawdziwy round-trip (a nie tylko CSV).
    try:
        import importlib.util
        generator = importlib.util.find_spec('esp_idf_nvs_partition_gen') is not None
    except (ImportError, ValueError):
        generator = False
    if not generator:
        print('  (pomijam test CSV->generator->czytnik: brak esp-idf-nvs-partition-gen)')
    else:
        import tempfile
        import subprocess
        katalog = tempfile.mkdtemp(prefix='nvs_csv_')
        csv_in = os.path.join(katalog, 'in.csv')
        bin_out = os.path.join(katalog, 'out.bin')
        cicho(wypisz_csv, p, csv_in)
        wynik = subprocess.run(
            [sys.executable, '-m', 'esp_idf_nvs_partition_gen', 'generate',
             csv_in, bin_out, hex(len(obraz))],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        sprawdz('CSV->generator: wygenerowal plik', os.path.exists(bin_out), wynik.stdout[-200:])
        if os.path.exists(bin_out):
            with open(bin_out, 'rb') as f:
                p_gen = Partycja(f.read(), 'z-generatora')
            sprawdz('CSV->generator->czytnik: te same przestrzenie',
                    set(p_gen.przestrzenie.values()) == set(p.przestrzenie.values()),
                    p_gen.przestrzenie)
            sprawdz('CSV->generator->czytnik: wybor == 7',
                    p_gen.dane.get((ns, 'wybor'), {}).get('wartosc') == 7)
            sprawdz('CSV->generator->czytnik: tekst 100 B',
                    p_gen.dane.get((ns, 'dlugi'), {}).get('wartosc') == dlugi)
            sprawdz('CSV->generator->czytnik: blob 10240 B',
                    p_gen.dane.get((wifi, 'duzy'), {}).get('wartosc') == blob,
                    p_gen.dane.get((wifi, 'duzy'), {}).get('rozmiar'))
            sprawdz('CSV->generator->czytnik: bez ostrzezen',
                    not p_gen.ostrzezenia, p_gen.ostrzezenia)
        import shutil
        shutil.rmtree(katalog, ignore_errors=True)

    print('autotest: %d z %d przypadkow OK' % (nie - zle, nie))
    return 0 if zle == 0 else 1


# ---------------------------------------------------------------------------
# Wczytanie pliku, offset, rozmiar
# ---------------------------------------------------------------------------
def znajdz_partycje_w_pliku(raw):
    """Gdy ktos da caly zrzut flashu bez --offset/--rozmiar: znajdz NVS.
    Zwraca (offset, rozmiar, sposob) albo None. Najpierw tablica partycji
    (dokladna), potem heurystyka po naglowkach stron."""
    # 1) tablica partycji na 0x8000
    if len(raw) >= TABLICA_PARTYCJI_OFFSET + 32:
        tab = czytaj_tablice_partycji(
            raw[TABLICA_PARTYCJI_OFFSET:TABLICA_PARTYCJI_OFFSET + TABLICA_PARTYCJI_BAJTOW])
        p_nvs = znajdz_partycje_nvs(tab)
        if p_nvs and p_nvs['offset'] + p_nvs['rozmiar'] <= len(raw):
            return (p_nvs['offset'], p_nvs['rozmiar'],
                    'tablica partycji z 0x8000, etykieta "%s"' % p_nvs['etykieta'])
    # 2) heurystyka
    znalezione = wykryj(raw)
    if len(znalezione) == 1:
        off, dl, _, _ = znalezione[0]
        return (off, dl, 'naglowki stron NVS')
    if znalezione:
        off = max(znalezione, key=lambda z: z[1])[0]
        dl = max(znalezione, key=lambda z: z[1])[1]
        return (off, dl, 'naglowki stron NVS (najdluzszy ciag)')
    return None


def wyglada_na_partycje(raw):
    """Czy plik zaczyna sie od ZAINICJOWANEJ strony NVS (stan != 0xFF)?
    Sam poprawny naglowek to za malo: strona NVS wypelniona samymi 0xFF tez
    ma poprawny naglowek, wiec caly obraz flashu zaczynalby sie "poprawnie"."""
    if len(raw) < PAGE_SIZE or not wyglada_na_strone_nvs(raw[:PAGE_SIZE]):
        return False
    return struct.unpack_from('<I', raw, 0)[0] != STAN_PUSTA


def wczytaj(sciezka, offset, rozmiar):
    if not os.path.exists(sciezka):
        sys.exit('nie ma pliku: %s' % sciezka)
    with open(sciezka, 'rb') as f:
        raw = f.read()
    if offset:
        if offset >= len(raw):
            sys.exit('offset 0x%x jest poza plikiem (plik ma %d B)' % (offset, len(raw)))
        raw = raw[offset:]
    if rozmiar:
        if rozmiar > len(raw):
            sys.exit('rozmiar 0x%x przekracza to, co jest w pliku od offsetu (%d B)'
                     % (rozmiar, len(raw)))
        raw = raw[:rozmiar]
    else:
        # bez rozmiaru bierzemy pelne strony; strony na koncu, ktore sa jeszcze
        # nietkniete (same 0xFF), tylko zasmiecaja raport - obcinamy je
        raw = raw[:len(raw) - len(raw) % PAGE_SIZE]
        while len(raw) > PAGE_SIZE and \
                raw[-PAGE_SIZE:] == b'\xff' * PAGE_SIZE:
            raw = raw[:-PAGE_SIZE]
    opis = '%s' % sciezka
    if offset:
        opis += ' od 0x%x' % offset
    if rozmiar:
        opis += ', %d B' % rozmiar
    return raw, opis


def main():
    ap = argparse.ArgumentParser(
        description='Odczyt calej zawartosci partycji NVS z pliku (zrzut esptool).',
        epilog='Przyklad: python3 narzedzia/nvs_odczyt.py nvs_centrala.bin '
               '--json --pokaz-usuniete')
    ap.add_argument('plik', nargs='?', help='zrzut partycji NVS albo calego flashu')
    ap.add_argument('--offset', type=lambda s: int(s, 0), default=0,
                    help='poczatek partycji NVS w pliku (np. 0x9000); domyslnie 0')
    ap.add_argument('--rozmiar', type=lambda s: int(s, 0), default=0,
                    help='dlugosc partycji NVS w bajtach (np. 0x5000); domyslnie do konca pliku')
    ap.add_argument('--wykryj', action='store_true',
                    help='szukaj partycji NVS w zrzucie calego flashu (nic nie parsuje)')
    ap.add_argument('--tablica-partycji', metavar='PLIK', dest='tablica',
                    help='plik z tablica partycji (zrzut 0x8000, 0xC00) - pokaze, '
                         'gdzie jest NVS, i poda gotowe polecenie zrzutu')
    ap.add_argument('--json', action='store_true', help='wyjscie JSON')
    ap.add_argument('--csv', metavar='PLIK', help='zapisz CSV w formacie narzedzia Espressif')
    ap.add_argument('--maskuj', action='store_true',
                    help='ukryj wartosci kluczy z haslami/tokenami (pass, token, api_key...)')
    ap.add_argument('--pokaz-usuniete', action='store_true',
                    help='pokaz tez wpisy usuniete (zostaja w pamieci do przepisania strony)')
    ap.add_argument('--autotest', action='store_true', help='sprawdz samo narzedzie')
    a = ap.parse_args()

    if a.autotest:
        return autotest()

    if a.tablica:
        if not os.path.exists(a.tablica):
            sys.exit('nie ma pliku: %s' % a.tablica)
        with open(a.tablica, 'rb') as f:
            raw = f.read()
        return wypisz_tablice(czytaj_tablice_partycji(raw), a.tablica)

    if not a.plik:
        ap.error('podaj plik ze zrzutem (albo --autotest, albo --tablica-partycji PLIK)')

    if a.wykryj:
        if not os.path.exists(a.plik):
            sys.exit('nie ma pliku: %s' % a.plik)
        with open(a.plik, 'rb') as f:
            raw = f.read()
        znalezione = wykryj(raw)
        if not znalezione:
            print('nie znalazlem partycji NVS w %s (%d B)' % (a.plik, len(raw)))
            return 1
        print('partycje NVS w %s (%d B):' % (a.plik, len(raw)))
        for off, dl, przed, po in znalezione:
            print('  0x%06x  %d B (%d stron zapisanych%s%s)'
                  % (off, dl, dl // PAGE_SIZE,
                     ', %d wolnych stron przed' % przed if przed else '',
                     ', %d wolnych stron po' % po if po else ''))
            try:
                p = Partycja(raw[off:off + dl], '')
            except NvsBlad:
                continue
            for ns in sorted(p.przestrzenie):
                klucze = sorted(k for (n, k) in p.dane if n == ns)
                print('      "%s": %s' % (p.przestrzenie[ns], ', '.join(klucze) or '(pusto)'))
        return 0

    raw, opis = wczytaj(a.plik, a.offset, a.rozmiar)
    try:
        if not a.offset and not a.rozmiar and not wyglada_na_partycje(raw):
            raise NvsBlad('pod tym adresem nie ma naglowka strony NVS')
        p = Partycja(raw, opis)
    except NvsBlad as e:
        # Najczestszy przypadek: ktos dał CALY zrzut flashu zamiast partycji NVS.
        # Zamiast kazac mu zgadywac offset, szukamy partycji sami.
        if not a.offset and not a.rozmiar:
            with open(a.plik, 'rb') as f:
                caly = f.read()
            trafienie = znajdz_partycje_w_pliku(caly)
            if trafienie:
                off, dl, sposob = trafienie
                # W trybie --json wszystko, co nie jest JSON-em, idzie na stderr,
                # inaczej nie da sie wyjscia podac do json.load / jq.
                wyj = sys.stderr if a.json else sys.stdout
                print('To nie jest sama partycja NVS - to wiekszy obraz.', file=wyj)
                print('Znalazlem NVS na 0x%x, %d B (%s). Czytam ta czesc.' % (off, dl, sposob),
                      file=wyj)
                if not a.json:
                    print('', file=wyj)
                    print('(na przyszlosc: python %s %s --offset 0x%x --rozmiar 0x%x)'
                          % (os.path.basename(sys.argv[0]), os.path.basename(a.plik), off, dl),
                          file=wyj)
                    print('', file=wyj)
                else:
                    print('', file=wyj)
                try:
                    p = Partycja(caly[off:off + dl], '%s od 0x%x' % (a.plik, off))
                except NvsBlad as e2:
                    print('BLAD: %s' % e2)
                    return 1
            else:
                print('BLAD: %s' % e)
                print('sprobuj: python %s %s --wykryj' % (sys.argv[0], a.plik))
                return 1
        else:
            print('BLAD: %s' % e)
            print('sprobuj: python %s %s --wykryj' % (sys.argv[0], a.plik))
            return 1

    if a.json:
        wypisz_json(p, a.maskuj, a.pokaz_usuniete)
    else:
        wypisz_tekst(p, a.maskuj, a.pokaz_usuniete)
    if a.csv:
        wypisz_csv(p, a.csv)
    return 0


if __name__ == '__main__':
    sys.exit(main())
