#!/usr/bin/env python3
"""220: mutacje - kazde zepsucie MUSI zrobic kontrakt czerwonym.

Mutacja zmienia JEDNA rzecz: albo w naszym naglowku (to, co wyslemy na
ESP), albo w kopii prawdziwego naglowka biblioteki (zeby sprawdzic, czy
kontrola naprawde czyta te pliki).
"""

# (nazwa, opis, plik, [(stary, nowy)], oczekiwane_czerwone, dodatkowe_flagi)
MUTANTY = [
    ('M1_fillbuffer_const',
     'nasze _fillBuffer staje sie const - nie pasuje do wirtualnej w bibliotece',
     'HistoriaPobranie.h',
     [('size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {',
       'size_t _fillBuffer(uint8_t *buf, size_t maxLen) const override {')],
     ['Z03'], []),

    ('M2_override_respond',
     'nadpisujemy _respond, ktore w bibliotece jest final',
     'HistoriaPobranie.h',
     [('  bool _sourceValid() const override',
       '  void _respond(AsyncWebServerRequest *r) override { (void)r; }\n'
       '  bool _sourceValid() const override')],
     ['Z03'], []),

    ('M3_std_str',
     'w naglowku firmware pojawia sie metoda z naszego modelu (std_str)',
     'HistoriaPobranie.h',
     [('const String bilet = request->getParam("k")->value();',
       'const String bilet = request->getParam("k")->value();\n'
       '      const std::string sprawdz = bilet.std_str();\n'
       '      (void)sprawdz;')],
     ['Z05'], []),

    ('M4_addheader_int',
     'addHeader z dwiema liczbami - zadne przeciazenie tego nie przyjmie',
     'HistoriaPobranie.h',
     [('if (r != nullptr) r->addHeader("Cache-Control", "no-store");',
       'if (r != nullptr) r->addHeader(1, 2);')],
     ['Z04'], []),

    ('M5_calls_openn',
     'zapinka wola io.openn(), ktorego w CheckedHistoryIO.h nie ma',
     'HistoriaPin.h',
     [('const int fd = io.open(path, O_RDONLY);',
       'const int fd = io.openn(path, O_RDONLY);')],
     ['Z06'], []),

    ('M6_pole_prywatne',
     'w bibliotece _code staje sie prywatne - nasz konstruktor nie ma prawa go ustawic',
     'ESPAsyncWebServer.h',
     [('protected:\n  int _code;', 'private:\n  int _code;')],
     ['Z02'], ['--bez-sha']),

    ('M7_inny_sha',
     'biblioteka inna niz przypieta (ktos podmienil naglowek)',
     'ESPAsyncWebServer.h',
     [('// Namespace for web request method defines',
       '// Namespace for web request method defines (ZMIENIONY PLIK)')],
     ['Z01'], []),

    ('M8_brak_http_any',
     'biblioteka nie definiuje HTTP_ANY',
     'ESPAsyncWebServer.h',
     [('constexpr WebRequestMethodComposite HTTP_ANY = HTTP_ALL;',
       'constexpr WebRequestMethodComposite HTTP_NIE_MA = HTTP_ALL;')],
     ['Z07'], ['--bez-sha']),

    ('M9_include_modelu',
     'naglowek firmware wciaga plik z zaslepka testowa',
     'HistoriaPobranie.h',
     [('#include "HistoriaPin.h"', '#include "HistoriaPin.h"\n#include "Serwer218.h"')],
     ['Z05'], []),

    ('M10_brak_exact',
     'biblioteka nie ma AsyncURIMatcher::exact (etap 218 by sie nie skompilowal)',
     'ESPAsyncWebServer.h',
     [('static inline AsyncURIMatcher exact(String c, uint16_t modifiers = None) {',
       'static inline AsyncURIMatcher exactX(String c, uint16_t modifiers = None) {')],
     ['Z07'], ['--bez-sha']),
]


def zastosuj(tekst, podmiany):
    for stary, nowy in podmiany:
        ile = tekst.count(stary)
        if ile != 1:
            return None, 'oczekiwano dokladnie jednego wystapienia %r, jest %d' % (stary[:60], ile)
        tekst = tekst.replace(stary, nowy, 1)
    return tekst, None
