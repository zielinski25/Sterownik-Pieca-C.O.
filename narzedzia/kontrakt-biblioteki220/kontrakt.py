#!/usr/bin/env python3
"""220: KONTRAKT Z PRAWDZIWA BIBLIOTEKA.

Po co: caly nasz nowy kod (HistoriaPobranie.h, HistoriaPin.h, HistoriaBilety.h
i wstawki w main_centrala.cpp) byl kompilowany WYLACZNIE przeciwko naszym
wlasnym zaslepkom (model AsyncWebServerRequest, AsyncAbstractResponse...).
Jesli zaslepka ma inna sygnature niz prawdziwa biblioteka, to ze testy sa
zielone NIC nie wynika - u uzytkownika plik sie nie skompiluje.

Ten skrypt bierze PRAWDZIWE naglowki biblioteki (przypiete SHA-256 z etapow
196-215) i sprawdza nasz kod na nich:

  Z01 naglowki biblioteki sa te same (SHA-256)
  Z02 czlonkowie klasy bazowej, ktore nasz kod ustawia, sa protected/public
  Z03 nasze nadpisania (override) pasuja do prawdziwych metod wirtualnych
  Z04 kazde wywolanie metody biblioteki ma DOKLADNIE JEDNEGO kandydata
      (rozstrzyganie przeciazen: dokladne dopasowanie bije konwersje)
  Z05 w kodzie firmware nie ma nazw, ktore istnieja tylko w naszym modelu
  Z06 wywolania historii (historia193::Calls) istnieja w prawdziwym naglowku
  Z07 nowe symbole (HTTP_ANY, AsyncURIMatcher::exact, on(...)) sa zdefiniowane
      w tej samej bibliotece, na ktorej bazowy plik juz dziala

Wyjscie: JSON z lista kontroli + tabela wszystkich wywolan.

Uruchomienie:
    python3 -B narzedzia/kontrakt-biblioteki220/kontrakt.py [--wynik plik.json]
"""
import argparse
import hashlib
import json
import os
import re
import sys

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
NARZ = os.path.join(REPO, "narzedzia")
FW = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")

SRC = {
    "ESPAsyncWebServer.h": (os.path.join(NARZ, "cykl-http139", "zrodla", "ESPAsyncWebServer.h"),
                            "25db7c8fb30943307b28b181722187a65576f6ee778eeb99e29b1ef8db7915ba"),
    "WebResponseImpl.h": (os.path.join(NARZ, "cykl-http139", "zrodla", "WebResponseImpl.h"),
                          "b90de8322a97af18e8cd6817b7a5c27999c6a5e0097f23ecd77a4e167d9ced75"),
    "WebHandlerImpl.h": (os.path.join(NARZ, "naglowki-upload206", "upstream", "WebHandlerImpl.h"),
                         "e2ab3f7b6af3544934bea31a06c803240a3d8a6c3bbeda9669854cf5d840a006"),
    "literals.h": (os.path.join(NARZ, "naglowki-upload206", "upstream", "literals.h"),
                   "2df08b5947a05fdb6b890a486708b046cc2bc903aa9c7d7a227c0a4a49b51fb3"),
}
HISTORIA_IO = os.path.join(FW, "CheckedHistoryIO.h")

NASZE = {
    "HistoriaPobranie.h": os.path.join(NARZ, "od-przycisku216", "HistoriaPobranie.h"),
    "HistoriaBilety.h": os.path.join(NARZ, "panel-pobrania213", "HistoriaBilety.h"),
    "HistoriaPin.h": os.path.join(NARZ, "wysylka-zapinka211", "HistoriaPin.h"),
}

# Nasz model (zaslepka) - zeby pokazac, gdzie rozni sie od prawdziwej biblioteki.
MODEL = os.path.join(NARZ, "panel-trasa218", "host", "Serwer218.h")


# ─── narzedzia tekstowe ─────────────────────────────────────────────────
def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def bez_komentarzy(t):
    """Usuwa komentarze, zachowuje dlugosc (zeby offsety nie uciekaly)."""
    out = list(t)
    i, n = 0, len(t)
    while i < n:
        if t.startswith("//", i):
            j = t.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif t.startswith("/*", i):
            j = t.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != "\n":
                    out[k] = " "
            i = j
        else:
            i += 1
    return "".join(out)


def cialo_klasy(tekst, nazwa):
    """Zwraca (cialo, offset) klasy 'class <nazwa> ... { ... }'."""
    m = re.search(r'\b(?:class|struct)\s+%s\b[^;{]*\{' % re.escape(nazwa), tekst)
    if not m:
        return None, -1
    start = m.end() - 1
    glebokosc = 0
    for i in range(start, len(tekst)):
        c = tekst[i]
        if c == '{':
            glebokosc += 1
        elif c == '}':
            glebokosc -= 1
            if glebokosc == 0:
                return tekst[start + 1:i], start + 1
    return None, -1


def podziel_top(s, separator=','):
    """Dzieli po separatorze na poziomie 0 (nawiasy, <>, klamry, cudzyslowy)."""
    czesci, bufor = [], []
    gleb = 0
    i, n = 0, len(s)
    while i < n:
        c = s[i]
        if c in '([{<':
            gleb += 1
        elif c in ')]}>':
            gleb -= 1
        if c == separator and gleb == 0:
            czesci.append(''.join(bufor))
            bufor = []
        else:
            bufor.append(c)
        i += 1
    czesci.append(''.join(bufor))
    return [x.strip() for x in czesci if x.strip() != '']


def wyjmij_domyslne(param):
    """Usuwa '= domyslna' z parametru (na poziomie 0)."""
    gleb = 0
    for i, c in enumerate(param):
        if c in '([{<':
            gleb += 1
        elif c in ')]}>':
            gleb -= 1
        elif c == '=' and gleb == 0:
            return param[:i].strip(), True
    return param.strip(), False


def typ_parametru(param):
    """'const uint8_t *content' -> ('constuint8_t*', 'content')."""
    bez, _ = wyjmij_domyslne(param)
    bez = ' '.join(bez.split())
    m = re.match(r'^(.*?)([A-Za-z_]\w*)\s*(\[\s*\])?$', bez)
    if not m:
        return bez.replace(' ', ''), None
    typ, nazwa, tablica = m.group(1).strip(), m.group(2), m.group(3)
    if tablica:
        typ = (typ + '*').strip()
    if nazwa in ('const', 'unsigned', 'int', 'long', 'short', 'char', 'bool', 'size_t'):
        # brak nazwy parametru (sam typ)
        return bez.replace(' ', ''), None
    return typ.replace(' ', ''), nazwa


RODZAJE = {
    'constchar*': 'cstr',
    'char*': 'cstr',
    'constchar*const': 'cstr',
    'constString&': 'string',
    'String': 'string',
    'int': 'int',
    'long': 'int',
    'size_t': 'size',
    'unsignedint': 'size',
    'uint32_t': 'size',
    'bool': 'bool',
    'AsyncWebServerResponse*': 'resp',
    'AsyncAbstractResponse*': 'resp',
    'constuint8_t*': 'u8',
    'uint8_t*': 'u8',
    'constchar*const*': 'nieznany',
    'AwsTemplateProcessor': 'fn',
    'constAsyncWebHeader*': 'hdr',
    'constAsyncWebParameter*': 'par',
    'WebRequestMethodComposite': 'method',
    'ArRequestHandlerFunction': 'handler',
    'ArUploadHandlerFunction': 'upload',
    'ArBodyHandlerFunction': 'body',
    'const__FlashStringHelper*': 'flash',
    'AsyncURIMatcher': 'matcher',
    'fs::FS&': 'fs',
    'FS&': 'fs',
    'File': 'file',
    'Stream&': 'stream',
    'AwsResponseFiller': 'filler',
    'AsyncWebServer*': 'srv',
    'AsyncWebServerRequest*': 'req',
    'constString*': 'nstr',
}


def rodzaj(typ):
    return RODZAJE.get(typ.replace(' ', ''), 'nieznany:' + typ.replace(' ', ''))


def parsuj_klase(tekst, nazwa):
    """Lista skladowych klasy: dict(nazwa, zwrot, parametry, dostep, wirtualna,
    finalna, statyczna, const, tekst)."""
    cialo, offset = cialo_klasy(tekst, nazwa)
    if cialo is None:
        return None
    dostep = 'private' if re.search(r'\bclass\s+%s\b' % re.escape(nazwa), tekst) else 'public'
    wynik = []
    bufor = []
    i, n = 0, len(cialo)
    glebokosc = 0

    def domknij(surowy):
        # linie preprocesora (#ifdef/#endif/#if) nie sa czescia deklaracji
        linie = [l for l in ''.join(surowy).split('\n') if not l.strip().startswith('#')]
        dekl = ' '.join(linie).strip()
        if dekl:
            wynik.extend(parsuj_deklaracje(dekl, dostep, offset))

    while i < n:
        c = cialo[i]
        # etykieta dostepu na poziomie 0
        if glebokosc == 0 and c == ':' and ''.join(bufor).strip() in ('public', 'protected', 'private'):
            dostep = ''.join(bufor).strip()
            bufor = []
            i += 1
            continue
        # cialo funkcji inline - przeskakujemy w calosci, deklaracja konczy sie tutaj
        if glebokosc == 0 and c == '{':
            g = 0
            while i < n:
                if cialo[i] == '{':
                    g += 1
                elif cialo[i] == '}':
                    g -= 1
                    if g == 0:
                        i += 1
                        break
                i += 1
            domknij(bufor)
            bufor = []
            continue
        if glebokosc == 0 and c == ';':
            domknij(bufor)
            bufor = []
            i += 1
            continue
        if c in '([<':
            glebokosc += 1
        elif c in ')]>':
            glebokosc = max(0, glebokosc - 1)
        bufor.append(c)
        i += 1
    return wynik


def parsuj_deklaracje(dekl, dostep, offset):
    tekst = ' '.join(dekl.split())
    if not tekst or tekst.startswith('friend') or tekst.startswith('typedef') \
            or tekst.startswith('using') or tekst.startswith('enum') or tekst.startswith('#'):
        return []
    # '= 0', '= default', '= delete' na koncu (czyste wirtualne)
    czysty = re.sub(r'\s*=\s*(0|default|delete)\s*$', '', tekst)
    # __attribute__((unused)) w parametrach - to nie jest czesc typu
    while '__attribute__' in czysty:
        m_attr = re.search(r'__attribute__\s*\(', czysty)
        if not m_attr:
            break
        g, koniec = 0, len(czysty)
        for i in range(m_attr.end() - 1, len(czysty)):
            if czysty[i] == '(':
                g += 1
            elif czysty[i] == ')':
                g -= 1
                if g == 0:
                    koniec = i + 1
                    break
        czysty = (czysty[:m_attr.start()] + ' ' + czysty[koniec:]).strip()
    # znajdz liste parametrow: pierwszy nawias na poziomie 0 i jego domkniecie
    otwarcie = czysty.find('(')
    if otwarcie < 0:
        # skladowa danych: 'int _code'
        czysty = re.sub(r'\s*=.*$', '', czysty)   # 'bool _sent = false' -> 'bool _sent'
        m = re.match(r'^(?P<typ>.+?)\s*(?P<nazwa>[A-Za-z_]\w*)$', czysty)
        if not m:
            return []
        return [{'nazwa': m.group('nazwa'), 'zwrot': m.group('typ').strip(), 'parametry': [],
                 'dostep': dostep, 'wirtualna': bool(re.search(r'\bvirtual\b', czysty)),
                 'finalna': False, 'statyczna': bool(re.search(r'\bstatic\b', czysty)),
                 'const': False, 'dane': True, 'tekst': tekst}]
    gleb = 0
    zamkniecie = -1
    for i in range(otwarcie, len(czysty)):
        if czysty[i] == '(':
            gleb += 1
        elif czysty[i] == ')':
            gleb -= 1
            if gleb == 0:
                zamkniecie = i
                break
    if zamkniecie < 0:
        return []
    prefiks = czysty[:otwarcie].strip()
    paramy_tekst = czysty[otwarcie + 1:zamkniecie]
    ogon = czysty[zamkniecie + 1:].strip()
    m = re.match(r'^(?P<prefiks>.*?)(?P<nazwa>~?[A-Za-z_]\w*)\s*$', prefiks)
    if not m:
        return []
    nazwa = m.group('nazwa')
    prefiks_typu = m.group('prefiks')
    if not re.fullmatch(r'(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?', ogon):
        return []
    paramy = [p for p in podziel_top(paramy_tekst) if p.strip() not in ('void',)]
    typy = []
    for p in paramy:
        typ, _ = typ_parametru(p)
        _, domyslny = wyjmij_domyslne(p)
        typy.append({'typ': typ, 'rodzaj': rodzaj(typ), 'domyslny': domyslny})
    ret = prefiks_typu
    for slowo in ('virtual', 'static', 'inline', 'explicit', 'constexpr'):
        ret = re.sub(r'\b%s\b' % slowo, '', ret)
    return [{
        'nazwa': nazwa,
        'zwrot': ' '.join(ret.split()),
        'parametry': typy,
        'dostep': dostep,
        'wirtualna': bool(re.search(r'\bvirtual\b', prefiks_typu)),
        'finalna': bool(re.search(r'\bfinal\b', ogon)),
        'statyczna': bool(re.search(r'\bstatic\b', prefiks_typu)),
        'const': bool(re.match(r'^const\b', ogon)),
        'dane': False,
        'tekst': tekst,
    }]


# ─── rozstrzyganie przeciazen ───────────────────────────────────────────
def konwersja(od, do):
    """Ranga konwersji argumentu 'od' na parametr 'do'. None = niedozwolona."""
    if od == do:
        return 0
    if do == 'string' and od == 'cstr':
        return 2                     # String(const char*) - konwersja uzytkownika
    if do == 'cstr' and od == 'string':
        return None                  # String -> const char* NIE jest automatyczne
    if do == 'size' and od == 'int':
        return 1                     # konwersja calkowita
    if do == 'int' and od == 'size':
        return None                  # zwezenie - w C++ dozwolone, ale tu celowo odrzucamy
    if od == 'null':
        if do in ('cstr', 'u8', 'resp', 'hdr', 'par', 'fn', 'upload', 'body', 'filler',
                  'nstr', 'srv', 'req'):
            return 0
        return None
    if od == 'resp_pochodna' and do == 'resp':
        return 0
    if od == 'flag' and do == 'bool':
        return 0
    if od == 'method' and do == 'method':
        return 0
    if od == 'matcher' and do == 'matcher':
        return 0
    if od == 'fn' and do == 'fn':
        return 0
    if od == 'handler' and do == 'handler':
        return 0
    return None


def dopasuj(kandydaci, argumenty):
    """Zwraca (dobrzy, opis). 'dobrzy' to lista (ranga, kandydat)."""
    dobrzy = []
    odrzuceni = []
    for k in kandydaci:
        paramy = k['parametry']
        wymagane = sum(1 for p in paramy if not p['domyslny'])
        if len(argumenty) > len(paramy) or len(argumenty) < wymagane:
            odrzuceni.append((k, 'liczba argumentow'))
            continue
        ranga = 0
        ok = True
        for a, p in zip(argumenty, paramy):
            r = konwersja(a, p['rodzaj'])
            if r is None:
                ok = False
                odrzuceni.append((k, 'argument %s -> %s' % (a, p['rodzaj'])))
                break
            ranga += r
        if ok:
            dobrzy.append((ranga, k))
    dobrzy.sort(key=lambda x: x[0])
    return dobrzy, odrzuceni


# ─── typy w naszym kodzie ───────────────────────────────────────────────
WZORY_TYPOW = [
    (r'const\s+AsyncWebHeader\s*\*\s*(\w+)', 'hdr'),
    (r'AsyncWebServerResponse\s*\*\s*(\w+)', 'resp'),
    (r'Odpowiedz\s*\*\s*(\w+)', 'resp_pochodna'),
    (r'const\s+char\s*\*\s*(\w+)', 'cstr'),
    (r'char\s+(\w+)\s*\[', 'cstr'),
    (r'const\s+String\s+(\w+)', 'string'),
    (r'\bString\s+(\w+)', 'string'),
    (r'\bbool\s+(\w+)', 'bool'),
    (r'\bint\s+(\w+)', 'int'),
    (r'size_t\s+(\w+)', 'size'),
    (r'AsyncWebServerRequest\s*\*\s*(\w+)', 'req'),
    (r'historia193::Calls\s*&\s*(\w+)', 'calls'),
]


def zbierz_typy(tekst, dodatkowe=None):
    """Lista (linia, nazwa, rodzaj) - przy wywolaniu bierzemy ostatnia
    deklaracje PRZED nim (tak działa zaslanianie nazw w C++)."""
    wpisy = []
    for wzor, rodzaj_ in WZORY_TYPOW:
        for m in re.finditer(wzor, tekst):
            wpisy.append((tekst.count('\n', 0, m.start()) + 1, m.group(1), rodzaj_))
    wpisy.sort()
    return wpisy


def typ_w_miejscu(wpisy, nazwa, linia):
    najlepszy = None
    for l, n, r in wpisy:
        if n == nazwa and l <= linia:
            najlepszy = r
    return najlepszy


def rodzaj_wyrazenia(wyrazenie, typy, linia=0):
    w = wyrazenie.strip()
    if re.fullmatch(r'"(?:[^"\\]|\\.)*"', w):
        return 'cstr'
    if re.fullmatch(r"'(?:[^'\\]|\\.)'", w):
        return 'int'
    if re.fullmatch(r'-?\d+[uUlL]*', w):
        return 'int'
    if w in ('true', 'false'):
        return 'bool'
    if w == 'nullptr':
        return 'null'
    if re.fullmatch(r'HTTP_[A-Z_]+', w):
        return 'method'
    if re.fullmatch(r'AsyncURIMatcher::(exact|prefix|all|dir|ext|regex)\s*\(.*\)', w, re.S):
        return 'matcher'
    if re.fullmatch(r'String\s*\(.*\)', w, re.S):
        return 'string'
    if re.fullmatch(r'[A-Za-z_]\w*(?:->|\.)c_str\(\)', w):
        return 'cstr'
    if re.fullmatch(r'[A-Za-z_]\w*(?:->|\.)length\(\)', w):
        return 'size'
    if re.fullmatch(r'[A-Za-z_]\w*(?:->|\.)size\(\)', w):
        return 'size'
    if re.fullmatch(r'[A-Za-z_]\w*(?:->)?value\(\)', w):
        return 'string'
    if re.fullmatch(r'\{[^{}]*\}', w):
        return 'lista'
    # wyrazenie warunkowe: oba ramiona tego samego rodzaju
    m = re.match(r'^(.*?)\?(.*?):(.*)$', w, re.S)
    if m and '?' in w:
        a = rodzaj_wyrazenia(m.group(2), typy, linia)
        b = rodzaj_wyrazenia(m.group(3), typy, linia)
        return a if a == b else 'nieznany(%s/%s)' % (a, b)
    if re.fullmatch(r'[A-Za-z_]\w*', w):
        znany = typ_w_miejscu(typy, w, linia)
        return znany or 'nieznany-identyfikator' 
    return 'nieznane-wyrazenie'


def wyciagnij_argumenty(tekst, otwarcie):
    """Argumenty wywolania - od pozycji '(' do zbilansowanego ')'."""
    gleb = 0
    for j in range(otwarcie, len(tekst)):
        if tekst[j] == '(':
            gleb += 1
        elif tekst[j] == ')':
            gleb -= 1
            if gleb == 0:
                return podziel_top(tekst[otwarcie + 1:j])
    return []


def wywolania(tekst):
    """(nazwa, argumenty, linia) dla kazdego '->metoda(' / '.metoda(' w kodzie."""
    wynik = []
    for m in re.finditer(r'(?:->|\.)\s*([A-Za-z_]\w*)\s*\(', tekst):
        nazwa = m.group(1)
        i = m.end() - 1
        gleb = 0
        for j in range(i, len(tekst)):
            if tekst[j] == '(':
                gleb += 1
            elif tekst[j] == ')':
                gleb -= 1
                if gleb == 0:
                    argumenty = podziel_top(tekst[i + 1:j])
                    wynik.append((nazwa, argumenty, tekst.count('\n', 0, i) + 1))
                    break
    return wynik


# ─── kontrole ───────────────────────────────────────────────────────────
class Audyt:
    def __init__(self):
        self.checks = []

    def dodaj(self, kod, nazwa, ok, szczegoly=''):
        self.checks.append({'kod': kod, 'nazwa': nazwa, 'ok': bool(ok), 'szczegoly': szczegoly})
        return ok

    @property
    def ok(self):
        return all(c['ok'] for c in self.checks)


# Metody biblioteki, ktore nasz kod wola - z nazwa klasy, do ktorej naleza.
METODY = {
    'send': 'AsyncWebServerRequest',
    'beginResponse': 'AsyncWebServerRequest',
    'method': 'AsyncWebServerRequest',
    'url': 'AsyncWebServerRequest',
    'getHeader': 'AsyncWebServerRequest',
    'hasParam': 'AsyncWebServerRequest',
    'getParam': 'AsyncWebServerRequest',
    'addHeader': 'AsyncWebServerResponse',
    'value': 'AsyncWebParameter',
    'name': 'AsyncWebParameter',
    'length': None,        # String - sprawdzane osobno
    'c_str': None,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--wynik', default=None)
    ap.add_argument('--zrodla', default=None, help='katalog z podmienionymi naglowkami (mutanty)')
    ap.add_argument('--nasze', default=None, help='katalog z podmienionymi naszymi naglowkami')
    ap.add_argument('--model', default=None)
    ap.add_argument('--bez-sha', action='store_true',
                    help='nie porownuj SHA naglowkow (tylko do mutacji)')
    a = ap.parse_args()

    audyt = Audyt()
    tabela_wywolan = []

    # ---- Z01 naglowki biblioteki -------------------------------------
    tresci = {}
    braki = []
    for nazwa, (sciezka, oczekiwany) in SRC.items():
        if a.zrodla:
            sciezka = os.path.join(a.zrodla, nazwa)
        if not os.path.exists(sciezka):
            braki.append('%s: brak pliku' % nazwa)
            continue
        sha = sha256(sciezka)
        tresci[nazwa] = bez_komentarzy(open(sciezka, encoding='utf-8', errors='replace').read())
        if not a.bez_sha and sha != oczekiwany:
            braki.append('%s: SHA %s != %s' % (nazwa, sha[:12], oczekiwany[:12]))
    audyt.dodaj('Z01', 'naglowki biblioteki sa te same co w etapach 196-215 (SHA-256)',
                not braki, '; '.join(braki) if braki else '%d plikow, SHA zgodne' % len(tresci))

    if 'ESPAsyncWebServer.h' not in tresci or 'WebResponseImpl.h' not in tresci:
        audyt.dodaj('Z02', 'klasy bazowe', False, 'brak naglowkow')
        _zapisz(audyt, tabela_wywolan, a.wynik)
        return 1

    # ---- Z02 klasa bazowa: dostep do pol ------------------------------
    bazowa = parsuj_klase(tresci['ESPAsyncWebServer.h'], 'AsyncWebServerResponse') or []
    abstract = parsuj_klase(tresci['WebResponseImpl.h'], 'AsyncAbstractResponse') or []
    pola = {m['nazwa']: m for m in bazowa if not m['parametry'] and m['nazwa'] in
            ('_code', '_contentType', '_contentLength', '_sendContentLength', '_chunked')}
    nasze_pola = ['_code', '_contentType', '_contentLength', '_sendContentLength', '_chunked']
    problemy = []
    for p in nasze_pola:
        m = pola.get(p)
        if m is None:
            problemy.append('%s: nie ma w AsyncWebServerResponse' % p)
        elif m['dostep'] not in ('protected', 'public'):
            problemy.append('%s: %s (nieprotected)' % (p, m['dostep']))
    ctor = [m for m in abstract if m['nazwa'] == 'AsyncAbstractResponse']
    if not ctor:
        problemy.append('brak konstruktora AsyncAbstractResponse')
    elif all(c['dostep'] != 'public' for c in ctor):
        problemy.append('konstruktor AsyncAbstractResponse nie jest public')
    dtor = [m for m in bazowa if m['nazwa'] == '~AsyncWebServerResponse']
    if not dtor or not dtor[0]['wirtualna']:
        problemy.append('~AsyncWebServerResponse nie jest wirtualny')
    audyt.dodaj('Z02', 'nasz kod ustawia tylko pola protected/public klasy bazowej',
                not problemy,
                '; '.join(problemy) if problemy else
                '%d pol protected, konstruktor public, destruktor wirtualny' % len(pola))

    # ---- Z03 nasze nadpisania ----------------------------------------
    nasze = {}
    for nazwa, sciezka in NASZE.items():
        if a.nasze:
            sciezka = os.path.join(a.nasze, nazwa)
        nasze[nazwa] = bez_komentarzy(open(sciezka, encoding='utf-8', errors='replace').read())
    tresc_pobrania = nasze['HistoriaPobranie.h']
    klasa_odp = parsuj_klase(tresc_pobrania, 'Odpowiedz') or []
    problemy = []
    for m in klasa_odp:
        if not (m['wirtualna'] or m['finalna'] or m['nazwa'].startswith('~')):
            if 'override' not in m['tekst']:
                continue
        nazwa = m['nazwa']
        if nazwa.startswith('~'):
            problemy.append('destruktor: OK (wirtualny w bazie)') if False else None
            continue
        prawdziwe = [x for x in (abstract + bazowa) if x['nazwa'] == nazwa]
        if not prawdziwe:
            problemy.append('%s: nie ma takiej metody w klasie bazowej' % nazwa)
            continue
        dopasowana = False
        for p in prawdziwe:
            if p['finalna']:
                problemy.append('%s: w bibliotece jest final - nie wolno nadpisywac' % nazwa)
                continue
            if p['const'] == m['const'] and len(p['parametry']) == len(m['parametry']) \
                    and all(x['rodzaj'] == y['rodzaj'] for x, y in zip(p['parametry'], m['parametry'])) \
                    and p['zwrot'].strip() == m['zwrot'].strip():
                dopasowana = True
        if not dopasowana and not any('final' in x for x in problemy):
            problemy.append('%s: sygnatura %s(%s)%s nie pasuje do biblioteki'
                            % (nazwa, m['zwrot'], ', '.join(p['typ'] for p in m['parametry']),
                               ' const' if m['const'] else ''))
    nadpisane = sorted({m['nazwa'] for m in klasa_odp if 'override' in m['tekst']})
    audyt.dodaj('Z03', 'nadpisania w naszym kodzie pasuja do prawdziwych metod wirtualnych',
                not problemy,
                '; '.join(problemy) if problemy else 'nadpisane: ' + ', '.join(nadpisane))

    # ---- Z04 rozstrzyganie przeciazen --------------------------------
    klasy = {
        'AsyncWebServerRequest': parsuj_klase(tresci['ESPAsyncWebServer.h'], 'AsyncWebServerRequest') or [],
        'AsyncWebServerResponse': bazowa,
        'AsyncWebParameter': parsuj_klase(tresci['ESPAsyncWebServer.h'], 'AsyncWebParameter') or [],
        'AsyncWebServer': parsuj_klase(tresci['ESPAsyncWebServer.h'], 'AsyncWebServer') or [],
        'AsyncURIMatcher': parsuj_klase(tresci['ESPAsyncWebServer.h'], 'AsyncURIMatcher') or [],
    }
    model_klasy = {}
    if os.path.exists(a.model or MODEL):
        model_tekst = bez_komentarzy(open(a.model or MODEL, encoding='utf-8', errors='replace').read())
        for n in ('AsyncWebServerRequest', 'AsyncWebServerResponse', 'AsyncWebParameter'):
            model_klasy[n] = parsuj_klase(model_tekst, n) or []

    problemy = []
    liczba = 0
    for nazwa_pliku, tekst in nasze.items():
        typy = zbierz_typy(tekst)
        for metoda, argumenty, linia in wywolania(tekst):
            klasa = METODY.get(metoda)
            if klasa is None or klasa not in klasy:
                continue
            liczba += 1
            rodzaje = [rodzaj_wyrazenia(x, typy, linia) for x in argumenty]
            kandydaci = [k for k in klasy[klasa] if k['nazwa'] == metoda and k['dostep'] != 'private'
                         and not k['statyczna']]
            dobrzy, odrzuceni = dopasuj(kandydaci, rodzaje)
            opis = {
                'plik': nazwa_pliku, 'linia': linia, 'klasa': klasa, 'metoda': metoda,
                'argumenty': argumenty, 'rodzaje': rodzaje,
                'kandydatow': len(kandydaci),
                'dobrych': len(dobrzy),
                'wybrany': dobrzy[0][1]['tekst'] if dobrzy else None,
                'ranga': dobrzy[0][0] if dobrzy else None,
            }
            if not dobrzy:
                problemy.append('%s:%d %s(%s) - ZADEN przeciazenie nie pasuje (rodzaje: %s)'
                                % (nazwa_pliku, linia, metoda, ', '.join(argumenty), rodzaje))
            elif len(dobrzy) > 1 and dobrzy[0][0] == dobrzy[1][0]:
                problemy.append('%s:%d %s(%s) - NIEJEDNOZNACZNE (%d kandydatow z ta sama ranga)'
                                % (nazwa_pliku, linia, metoda, ', '.join(argumenty), len(dobrzy)))
            elif any(r.startswith('nieznany') for r in rodzaje):
                problemy.append('%s:%d %s(%s) - nie umiem ustalic typu argumentu (%s)'
                                % (nazwa_pliku, linia, metoda, ', '.join(argumenty), rodzaje))
            # co by wybral model (nasza zaslepka)?
            if klasa in model_klasy:
                mk = [k for k in model_klasy[klasa] if k['nazwa'] == metoda]
                md, _ = dopasuj(mk, rodzaje)
                opis['model_wybrany'] = md[0][1]['tekst'] if md else None
                opis['model_zgodny'] = (opis['model_wybrany'] is not None and dobrzy
                                        and opis['model_wybrany'] == dobrzy[0][1]['tekst'])
            tabela_wywolan.append(opis)
    audyt.dodaj('Z04', 'kazde wywolanie metody biblioteki ma dokladnie jednego kandydata',
                not problemy, '; '.join(problemy) if problemy else
                '%d wywolan, kazde z jednym dopasowaniem' % liczba)

    # ---- Z05 nazwy z modelu w kodzie firmware ------------------------
    ZAKAZANE = ['std_str', 'jakoStrumien', 'dodajParametr', 'dodajNaglowek', 'model_ps_malloc',
                'model_malloc', 'g_psmallocFail', 'g_mallocFail', 'RamCalls', 'Model213',
                'Serwer218', 'Arduino.h', 'kontrola_', 'esp_random_fake']
    znalezione = []
    for nazwa, tekst in nasze.items():
        for z in ZAKAZANE:
            if z in tekst:
                znalezione.append('%s: %s' % (nazwa, z))
    audyt.dodaj('Z05', 'w naglowkach firmware nie ma nazw istniejacych tylko w zaslepkach',
                not znalezione, '; '.join(znalezione) if znalezione else
                '%d zakazanych nazw, zadnej nie ma' % len(ZAKAZANE))

    # ---- Z06 historia193::Calls --------------------------------------
    io_tekst = bez_komentarzy(open(HISTORIA_IO, encoding='utf-8', errors='replace').read())
    calls = parsuj_klase(io_tekst, 'Calls') or []
    dostepne = {m['nazwa']: m for m in calls}
    problemy = []
    uzyte = set()
    for m in re.finditer(r'\b(io|io_)\s*(?:->|\.)\s*([A-Za-z_]\w*)\s*\(', nasze['HistoriaPin.h']):
        nazwa = m.group(2)
        uzyte.add(nazwa)
        argumenty = wyciagnij_argumenty(nasze['HistoriaPin.h'], m.end() - 1)
        if nazwa not in dostepne:
            problemy.append('%s: nie ma w historia193::Calls' % nazwa)
            continue
        if len(argumenty) != len(dostepne[nazwa]['parametry']):
            problemy.append('%s: %d argumentow, w naglowku %d'
                            % (nazwa, len(argumenty), len(dostepne[nazwa]['parametry'])))
    audyt.dodaj('Z06', 'wywolania historii w zapince istnieja w prawdziwym CheckedHistoryIO.h',
                not problemy, '; '.join(problemy) if problemy else
                'uzyte: ' + ', '.join(sorted(uzyte)))

    # ---- Z07 nowe symbole biblioteki --------------------------------
    problemy = []
    if not re.search(r'\bconstexpr\s+WebRequestMethodComposite\s+HTTP_ALL\b', tresci['ESPAsyncWebServer.h']):
        problemy.append('brak HTTP_ALL w bibliotece')
    if not re.search(r'HTTP_ANY\s*=\s*HTTP_ALL', tresci['ESPAsyncWebServer.h']):
        problemy.append('brak definicji HTTP_ANY')
    if 'using namespace AsyncWebRequestMethod' not in tresci['ESPAsyncWebServer.h']:
        problemy.append('HTTP_* nie sa importowane do przestrzeni globalnej')
    if not re.search(r'static\s+inline\s+AsyncURIMatcher\s+exact\s*\(', tresci['ESPAsyncWebServer.h']):
        problemy.append('brak AsyncURIMatcher::exact')
    on_ok = any(m['nazwa'] == 'on' and m['parametry'] and m['parametry'][0]['rodzaj'] == 'matcher'
                for m in klasy['AsyncWebServer'])
    if not on_ok:
        problemy.append('brak AsyncWebServer::on(AsyncURIMatcher, ...)')
    # czy nasze wywolania uzywaja dokladnie tych symboli
    nasze_zrodlo = ' '.join(nasze.values())
    if 'HTTP_ANY' in nasze_zrodlo and 'HTTP_ANY' not in re.findall(r'HTTP_[A-Z_]+', tresci['ESPAsyncWebServer.h']):
        problemy.append('uzywamy HTTP_ANY, ktorego biblioteka nie ma')
    audyt.dodaj('Z07', 'nowe symbole (HTTP_ANY, exact, on z matcherem) sa w tej samej bibliotece',
                not problemy, '; '.join(problemy) if problemy else
                'HTTP_ALL/HTTP_ANY + exact() + on(matcher) znalezione')

    return _zapisz(audyt, tabela_wywolan, a.wynik)


def _zapisz(audyt, tabela, sciezka):
    raport = {'ok': audyt.ok, 'kontrol': len(audyt.checks), 'checks': audyt.checks,
              'wywolania': tabela}
    tekst = json.dumps(raport, indent=2, ensure_ascii=False) + '\n'
    if sciezka:
        with open(sciezka, 'w', encoding='utf-8') as f:
            f.write(tekst)
    for c in audyt.checks:
        print('%s %s %s%s' % ('PASS' if c['ok'] else 'FAIL', c['kod'], c['nazwa'],
                              ('  [' + c['szczegoly'] + ']') if c['szczegoly'] else ''))
    rozne = [w for w in tabela if w.get('model_zgodny') is False]
    print('INFO wywolan biblioteki: %d, w tym model wybralby inna sygnature: %d'
          % (len(tabela), len(rozne)))
    for w in rozne:
        print('     %s:%d %s  PRAWDZIWA: %s | MODEL: %s'
              % (w['plik'], w['linia'], w['metoda'], w['wybrany'], w['model_wybrany']))
    print('%s kontrakt220 kontrol=%d bledow=%d'
          % ('PASS' if audyt.ok else 'FAIL', len(audyt.checks),
             sum(1 for c in audyt.checks if not c['ok'])))
    return 0 if audyt.ok else 1


if __name__ == '__main__':
    sys.exit(main())
