#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Czy scenariusze ota_* zauwazylyby blad w kodzie aktualizacji OTA panelu (v1.4.0)?

Skrypt po kolei psuje jedna rzecz w KOPII zrodel firmware'u (pominiete
sprawdzenie, brak przerwania przy alarmie, brak zapisu stanu - lista ponizej),
buduje z niej symulator i uruchamia scenariusze ota_* oraz testy jednostkowe.
Blad jest "wykryty", gdy ktorys scenariusz albo test jednostkowy przestaje
przechodzic. Zrodel w src/ skrypt nie zmienia.

    python3 sprawdz_czulosc_ota.py            wszystkie bledy z listy (ok. 10 minut)
    python3 sprawdz_czulosc_ota.py 3 7        tylko bledy o podanych numerach

Wynik 0 = kazdy z bledow zostal wykryty. Wymaga tego samego co
uruchom_testy.sh (g++, LVGL i ArduinoJson w katalogu podrecznym).

To sprawdzenie TESTOW, nie kodu: mowi, ze kazda z wymienionych regul jest
pilnowana przez co najmniej jedno sprawdzenie. Nie dowodzi, ze testy wykryja
kazdy mozliwy blad.
"""
import os
import shutil
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(TU, '..', 'src')

# (opis, plik w src/, tekst w kodzie, tekst po zepsuciu). Tekst musi wystapic
# w pliku dokladnie raz - inaczej skrypt przerywa (kod sie zmienil).
BLEDY = [
    ('alarm w trakcie pobierania go nie przerywa', 'main.cpp',
     '  if (stan.dymAlarm || stan.alarmOgrzewanie || stan.alarmPanel) {\n    otaProbaPrzerwana("alarm");\n    return;\n  }\n',
     ''),
    ('wycofanie oferty w trakcie pobierania go nie przerywa', 'main.cpp',
     '  if (!otaOfertaBezZmian()) {\n    otaProbaPrzerwana("oferta_wycofana");\n    return;\n  }\n',
     ''),
    ('panel zaczyna pobierac mimo nieudanego zapisu stanu do NVS', 'main.cpp',
     '  if (!otaZapisZachowaj()) {\n    diag::otaZapisOddajProbe(otaZapis);',
     '  if (false) {\n    diag::otaZapisOddajProbe(otaZapis);'),
    ('identyfikator pobranego pliku nie jest porownywany z oferta', 'main.cpp',
     'if (otaP.nowaCentrala && strcmp(obrazPliku, otaP.obrazOferty) != 0) {',
     'if (false) {'),
    ('naglowek pliku nie jest sprawdzany przed zapisem', 'main.cpp',
     'if (diag::otaSkanOcenNaglowek(otaP.skan) != diag::OTA_POWOD_BRAK) {',
     'if (false) {'),
    ('rozpoczety zapis nie jest porzucany po nieudanej probie', 'main.cpp',
     '  if (otaP.zapisZaczety) Update.abort();   // nic z niedokonczonego zapisu nie zostaje do uruchomienia\n',
     ''),
    ('plik, z ktorego panel juz pracuje, jest wgrywany ponownie (starsza centrala)', 'main.cpp',
     "if (obrazPliku[0] != '\\0' && strcmp(obrazPliku, otaObrazWlasny) == 0) {",
     'if (false) {'),
    ('panel nie zglasza centrali odmowy', 'main.cpp',
     'otaZglosOdmowe = (d.akcja == diag::OTA_AKCJA_ODMOW && o.nowaCentrala);',
     'otaZglosOdmowe = false;'),
    ('panel nie zglasza centrali powodu czekania', 'main.cpp',
     '(d.akcja == diag::OTA_AKCJA_CZEKAJ && o.nowaCentrala) ? d.powod : ""',
     '""'),
    ('pierwszy start po wgraniu zglaszany jak program juz zatwierdzony', 'main.cpp',
     '  otaObrazZatwierdzony = !(biezaca != nullptr && esp_ota_get_state_partition(biezaca, &stanOta) == ESP_OK &&\n                           stanOta == ESP_OTA_IMG_PENDING_VERIFY);',
     '  otaObrazZatwierdzony = true;\n  (void)stanOta;'),
    ('po restarcie panel nie ocenia, czy wgrany program dziala', 'main.cpp',
     'const int ocena = diag::otaOcenPoStarcie(otaZapis, otaObrazWlasny);',
     'const int ocena = 0;'),
    ('proba przerwana alarmem liczy sie do limitu prob', 'main.cpp',
     '  diag::otaZapisOddajProbe(otaZapis);\n  otaZapisZachowaj();\n  logBezPamieci("tag=OTA-PANEL lvl=WARN zdarzenie=przerwana',
     '  otaZapisZachowaj();\n  logBezPamieci("tag=OTA-PANEL lvl=WARN zdarzenie=przerwana'),
    ('odrzucone haslo OTA nie konczy prob', 'main.cpp',
     'otaProbaNieudana(diag::OTA_POWOD_HASLO, true, "centrala odrzucila haslo OTA");',
     'otaProbaNieudana(diag::OTA_POWOD_HASLO, false, "centrala odrzucila haslo OTA");'),
    ('haslo OTA trafia do logu sieci', 'siec_nieblokujaca_LCD.h',
     '    siecSciezkaDoLogu(sciezka, bezHasla, sizeof(bezHasla));',
     '    snprintf(bezHasla, sizeof(bezHasla), "%s", sciezka);'),
    ('drabina ratunkowa pracuje w trakcie pobierania (wymusza ponowne polaczenie WiFi)', 'main.cpp',
     '  if (trwaAktualizacjaOtaPanelu) return;\n  const diag::Przyczyna przyczyna = panelPrzyczynaTeraz();',
     '  const diag::Przyczyna przyczyna = panelPrzyczynaTeraz();'),
    ('nakladka zostaje na ekranie po nieudanej probie', 'main.cpp',
     '  otaKoniecProbyMs = millis();\n  lvgl_dashboard_poc_ota(-1, nullptr);\n}',
     '  otaKoniecProbyMs = millis();\n}'),
    ('niepelny plik jest przekazywany do uruchomienia', 'main.cpp',
     'if (!otaP.zapisZaczety || !otaFetch.kompletna() || otaP.zapisano != otaP.rozmiar) {',
     'if (false) {'),
    ('po wgraniu panel nie zapisuje, ze plik jest wgrany', 'main.cpp',
     '  diag::otaZapisWgrane(otaZapis, obrazPliku);\n  const bool zapisanoStan = otaZapisZachowaj();',
     '  const bool zapisanoStan = true;'),
    ('oferta ze starszej centrali: identyfikator z poprzedniej odpowiedzi zostaje w pamieci', 'main.cpp',
     'snprintf(stan.panelOtaObraz, sizeof(stan.panelOtaObraz), "%s", diag::otaObrazPoprawny(obrazOferty) ? obrazOferty : "");',
     'if (diag::otaObrazPoprawny(obrazOferty)) snprintf(stan.panelOtaObraz, sizeof(stan.panelOtaObraz), "%s", obrazOferty);'),
    ('decyzja: alarm nie wstrzymuje poczatku aktualizacji', 'diagnostyka_panelu.h',
     '  if (w.alarm) return {OTA_AKCJA_CZEKAJ, "alarm", OTA_POWOD_BRAK};\n',
     ''),
    ('decyzja: plik juz wgrany (starsza centrala nie zdjela oferty) jest pobierany ponownie', 'diagnostyka_panelu.h',
     '    if (z.stan == OTA_ZAPIS_DZIALA) return {OTA_AKCJA_POMIN, "juz_wgrane", OTA_POWOD_BRAK};\n',
     ''),
    ('decyzja: brak limitu trzech prob', 'diagnostyka_panelu.h',
     '  if (ostateczna || z.prob >= OTA_MAX_PROB) {',
     '  if (ostateczna) {'),
    ('pobieranie: tresc ponad zapowiedziana dlugosc trafia do odbiorcy', 'siec_nieblokujaca_LCD.h',
     '            else if ((long)ile > zostalo) ile = (size_t)zostalo;   // to, co ponad zapowiedziana dlugosc, przepada\n',
     ''),
]


def uruchom(polecenie, srodowisko=None, limit=900):
    try:
        w = subprocess.run(polecenie, capture_output=True, text=True, env=srodowisko, timeout=limit, cwd=TU)
        return w.returncode, w.stdout + w.stderr
    except subprocess.TimeoutExpired:
        return 124, 'przekroczony czas'


def main():
    wybrane = [int(a) for a in sys.argv[1:]]
    rob = tempfile.mkdtemp(prefix='lcd_czulosc_ota_')
    try:
        print('Czulosc scenariuszy ota_* i testow jednostkowych (firmware panelu v1.4.0)')
        wykryte = 0
        sprawdzane = 0
        for nr, (opis, plik, stary, nowy) in enumerate(BLEDY, 1):
            if wybrane and nr not in wybrane:
                continue
            sprawdzane += 1
            kopia = os.path.join(rob, 'src_%02d' % nr)
            shutil.copytree(SRC, kopia)
            sciezka = os.path.join(kopia, plik)
            tresc = open(sciezka, encoding='utf-8').read()
            if tresc.count(stary) != 1:
                print('  %2d. %s\n        PRZERWANE: szukany tekst wystepuje w %s %d razy (ma byc 1)' % (nr, opis, plik, tresc.count(stary)))
                return 2
            open(sciezka, 'w', encoding='utf-8').write(tresc.replace(stary, nowy))
            # testy jednostkowe z naglowkiem z kopii (test wlacza "../src/diagnostyka_panelu.h")
            kat_testu = os.path.join(rob, 'test_%02d' % nr, 'testy_host')
            os.makedirs(kat_testu)
            shutil.copy(os.path.join(TU, 'test_diagnostyka.cpp'), kat_testu)
            os.symlink(kopia, os.path.join(rob, 'test_%02d' % nr, 'src'))
            kod_j, wyj_j = uruchom(['g++', '-std=c++17', '-O1', '-w', '-o', os.path.join(kat_testu, 't'), os.path.join(kat_testu, 'test_diagnostyka.cpp')])
            if kod_j == 0:
                kod_j, wyj_j = uruchom([os.path.join(kat_testu, 't')])
            srodowisko = dict(os.environ, SRC=kopia, TYLKO='^ota_')
            kod_s, wyj_s = uruchom(['./uruchom_testy.sh'], srodowisko)
            if 'BLAD KOMPILACJI' in wyj_s:
                print('  %2d. %s\n        NIE KOMPILUJE SIE' % (nr, opis))
                continue
            nieudane = [l.strip() for l in wyj_s.splitlines() if '[BLAD]' in l]
            scenariusze = [l.strip() for l in wyj_s.splitlines() if 'NIEZALICZONY:' in l]
            if kod_s != 0 and nieudane:
                wykryte += 1
                print('  %2d. %s\n        WYKRYTY przez %d scenariusz(e), np.: %s' % (nr, opis, len(scenariusze), nieudane[0].replace('[BLAD] ', '')[:120]))
            elif kod_j != 0:
                wykryte += 1
                print('  %2d. %s\n        WYKRYTY przez testy jednostkowe (test_diagnostyka.cpp)' % (nr, opis))
            elif kod_s != 0:
                wykryte += 1
                print('  %2d. %s\n        WYKRYTY (scenariusze przerwane, kod %d)' % (nr, opis, kod_s))
            else:
                print('  %2d. %s\n        NIEWYKRYTY - testy przeszly mimo bledu' % (nr, opis))
            shutil.rmtree(kopia, ignore_errors=True)
        print('Wykryte: %d z %d celowo wprowadzonych bledow' % (wykryte, sprawdzane))
        return 0 if wykryte == sprawdzane else 1
    finally:
        shutil.rmtree(rob, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
