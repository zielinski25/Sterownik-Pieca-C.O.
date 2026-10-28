#!/usr/bin/env python3
"""219: mutacje pliku v3.56.4 - kazda MUSI zrobic audyt czerwonym.

Mutacja to celowe zepsucie JEDNEJ rzeczy w pliku. Jesli audyt tego nie
zauwazy, to znaczy, ze dana kontrola nic nie sprawdza.
"""

# (nazwa, opis, [(stary, nowy), ...], [kody kontroli, ktore MUSZA byc czerwone])
MUTANTY = [
    ('M1_urwany_nawias',
     'usuniety jeden zamykajacy nawias klamrowy',
     [('    historiaPobranie.link(request);\n  });',
       '    historiaPobranie.link(request);\n  );')],
     ['B02']),

    ('M2_raw_string_bez_cudzyslowu',
     'raw string strony bez zamykajacego cudzyslowu',
     [(')PIECHTML";', ')PIECHTML;')],
     ['B03']),

    ('M3_panel_bez_id',
     'panel nie ma id="histBtn" (skrypt wyszedlby wczesniej)',
     [('id="histBtn"', 'id="histBtnX"')],
     ['B04']),

    ('M4_zla_kolejnosc_include',
     'HistoriaPobranie.h przed ESPAsyncWebServer.h (blad D1 z etapu 214)',
     [('#include <ESPAsyncWebServer.h>\n'
       '#include "HistoriaPobranie.h"   // [v3.56.0 HISTORIA-POBRANIE] trasa pobrania historii\n',
       '#include "HistoriaPobranie.h"   // [v3.56.0 HISTORIA-POBRANIE] trasa pobrania historii\n'
       '#include <ESPAsyncWebServer.h>\n')],
     ['B09']),

    ('M5_stara_wersja',
     'FIRMWARE_VERSION cofniete na v3.56.3',
     [('static const char* FIRMWARE_VERSION = "v3.56.4";',
       'static const char* FIRMWARE_VERSION = "v3.56.3";')],
     ['B11']),

    ('M6_goly_napis_trasy',
     '/history/long zarejestrowane golym napisem (blad v3.56.3)',
     [('AsyncURIMatcher::exact("/history/long")', '"/history/long"')],
     # B13 lapie sam typ matchera; B14 (martwe trasy) nie musi tu zadzialac,
     # bo po /history/long nie ma juz zadnej trasy z tym prefiksem.
     ['B13']),

    ('M7_blad_skladni_js',
     'nawias w skrypcie panelu (JavaScript sie nie sparsuje)',
     [("var trasa = (strumien === 'long') ? '/history/long' : '/history';",
       "var trasa = (strumien === 'long' ? '/history/long' : '/history';")],
     ['B08']),

    ('M8_nadmiarowy_div',
     'dodatkowy </div> przed </body> (niedomkniety tag w stronie)',
     [('\n</body>\n</html>\n', '\n</div>\n</body>\n</html>\n')],
     ['B06']),

    ('M9_duplikat_globalnej',
     'druga definicja historiaBramka (duplikat globalnej)',
     [('static HistoriaBramka212 historiaBramka;\n',
       'static HistoriaBramka212 historiaBramka;\nstatic HistoriaBramka212 historiaBramka;\n')],
     ['B12']),

    ('M10_konce_linii_crlf',
     'konce linii CRLF w calym pliku',
     [],   # osobna obsluga - patrz run.py
     ['B01']),
]


def zastosuj(tekst, mutacja):
    """Zwraca (nowy_tekst, blad). Kazda podmiana musi trafic dokladnie raz."""
    for stary, nowy in mutacja:
        ile = tekst.count(stary)
        if ile != 1:
            return None, 'oczekiwano dokladnie jednego wystapienia %r, jest %d' % (stary[:50], ile)
        tekst = tekst.replace(stary, nowy, 1)
    return tekst, None
