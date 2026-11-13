#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Czy testy z test_panel_ota.cpp zauwazylyby blad w kodzie [v3.58.6 PANEL-OTA]?

Skrypt po kolei psuje jedna rzecz w kodzie wycietym z main_centrala.cpp
(pominieta regula, zly prog, brak sprzatania - lista ponizej), kompiluje testy
z tak zepsutym kodem i sprawdza, czy testy to wykryly. Pliku centrali nie
zmienia: pracuje na kopii wycinka w katalogu tymczasowym.

    python3 sprawdz_czulosc_ota.py                       # ../src/main_centrala.cpp
    python3 sprawdz_czulosc_ota.py sciezka/main_centrala.cpp

Wynik 0 = kazdy z bledow zostal wykryty. Trwa ok. 2 minut (kazdy blad to
osobna kompilacja). Wymaga: g++ (C++17), python3.

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
CPP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(TU, '..', 'src', 'main_centrala.cpp')

# (opis bledu, tekst w kodzie centrali, tekst po zepsuciu). Kazdy tekst musi
# wystapic w wycinku dokladnie raz - inaczej skrypt przerywa (kod sie zmienil
# i liste trzeba dopasowac).
BLEDY = [
    # --- decyzje ---
    ('alarm nie ukrywa oferty przed panelem',
     'return s.aktywna && !alarm;',
     'return s.aktywna;'),
    ('plik wydawany takze w czasie alarmu',
     '  if (alarm) return 503;             // alarm: panel ma pokazywac alarm, a nie wgrywac firmware\n',
     ''),
    ('limit pobran o jedno za wysoki',
     'if (s.pobranZaczetych >= PANEL_OTA_MAX_POBRAN) {',
     'if (s.pobranZaczetych > PANEL_OTA_MAX_POBRAN) {'),
    ('pierwszy start z nowego pliku (jeszcze nie zatwierdzony) uznany za potwierdzenie',
     'if (maObraz && strcmp(z.obraz, s.obraz) == 0) {',
     'if ((maObraz && strcmp(z.obraz, s.obraz) == 0) || (maProbe && strcmp(z.obrazProba, s.obraz) == 0)) {'),
    ('odmowa panelu konczy oferte bez sprawdzenia numeru oferty',
     'if (z.jestOdmowa && z.odmowaNr == s.nr) {',
     'if (z.jestOdmowa) {'),
    ('panel v1.4.0+ na innym pliku traktowany jak starszy panel (regula wersji i powrotu po pobraniu)',
     '  if (maObraz) return PANEL_OTA_BEZ_ZMIAN;   // panel v1.4.0+ na innym pliku: sam zacznie, gdy bedzie mogl\n',
     ''),
    ('starszy panel po pobraniu nadal dostaje oferte (petla z 3.58.5)',
     '  if (s.pobranZaczetych > 0) {\n    // Taki panel pobiera plik tylko przy starcie',
     '  if (false) {\n    // Taki panel pobiera plik tylko przy starcie'),
    ('limit 24 h konczy oferte w trakcie wysylania pliku',
     '  if (panelOtaTransferTrwa(s, nowMs)) return PANEL_OTA_BEZ_ZMIAN;\n  if ((uint32_t)(nowMs - s.odMs) < PANEL_OTA_WAZNOSC_MS)',
     '  if ((uint32_t)(nowMs - s.odMs) < PANEL_OTA_WAZNOSC_MS)'),
    ('limit 24 h liczony bez uwzglednienia przekrecenia licznika czasu',
     'if ((uint32_t)(nowMs - s.odMs) < PANEL_OTA_WAZNOSC_MS) return PANEL_OTA_BEZ_ZMIAN;',
     'if (nowMs < s.odMs + PANEL_OTA_WAZNOSC_MS) return PANEL_OTA_BEZ_ZMIAN;'),
    ('firmware centrali (ESP32-S3) nie jest rozpoznawany',
     '  if (uklad == 0x0009) return "to program dla ESP32-S3 (centrala), a panel to ESP32 - zly plik w wydaniu";\n',
     ''),
    ('plik bez sumy SHA-256 na koncu jest przyjmowany',
     '  if (s.naglowek[23] != 1) return "plik nie ma sumy SHA-256 na koncu - nie da sie potwierdzic, co panel wgral";\n',
     ''),
    ('koniec oferty zeruje licznik polaczen (plik mozna podmienic w trakcie wysylania)',
     '  s.aktywna = false;\n  s.probaZgloszona = false;',
     '  s.aktywna = false;\n  s.transferow = 0;\n  s.probaZgloszona = false;'),
    ('zamkniecie polaczenia starej oferty zmniejsza licznik nowej',
     'if (s.nr == nr && s.transferow > 0) s.transferow--;',
     'if (s.transferow > 0) s.transferow--;'),
    ('polaczenie nigdy nie jest uznawane za zerwane (brak limitu 5 minut)',
     'return s.transferow > 0 && (uint32_t)(nowMs - s.transferOdMs) < PANEL_OTA_TRANSFER_MAX_MS;',
     'return s.transferow > 0;'),
    ('przypomnienie wysylane w kazdym obiegu',
     '  s.przypomniano = true;\n  return true;',
     '  return true;'),
    ('przypomnienie takze wtedy, gdy panel juz pobiera',
     '  if (s.pobranZaczetych > 0 || s.probaZgloszona) return false;\n',
     ''),
    ('panel "w kontakcie" bez limitu czasu',
     'return p.bylKontakt && (uint32_t)(nowMs - p.kontaktMs) < PANEL_OTA_KONTAKT_MS;',
     'return p.bylKontakt;'),
    ('numer odmowy dluzszy niz 9 cyfr jest przyjmowany',
     '    if (i >= 9) return false;                       // najwyzej 9 cyfr - miesci sie w uint32_t\n',
     ''),
    ('znaki specjalne w tagu wydania nie sa zamieniane',
     "cel[i] = zwykly ? c : '?';",
     'cel[i] = c;'),
    # --- kod klejacy ---
    ('otaOperacjaWTrakcie zostaje ustawiona po pobraniu z GitHuba (jak w 3.58.5)',
     '  otaOperacjaWTrakcie = false;\n  config.putBool("panOtaTrwa", false);\n  if (!pobrano) {',
     '  config.putBool("panOtaTrwa", false);\n  if (!pobrano) {'),
    ('zakonczona oferta zostaje w NVS (wraca po restarcie centrali)',
     '  if (!jestNastepna) config.putBool("panOtaGotowa", false);\n',
     ''),
    ('String uzyty w sekcji krytycznej',
     '  nr = g_panelOta.nr;\n  memcpy(tag, g_panelOta.tag, sizeof(tag));',
     '  nr = g_panelOta.nr;\n  j += F("");\n  memcpy(tag, g_panelOta.tag, sizeof(tag));'),
    ('sekcja krytyczna bez zamkniecia',
     '  const bool trwa = panelOtaTransferTrwa(g_panelOta, teraz);\n  portEXIT_CRITICAL(&g_panelOtaMux);',
     '  const bool trwa = panelOtaTransferTrwa(g_panelOta, teraz);'),
    ('oferta przywracana po restarcie bez sprawdzenia, czy plik jest na dysku',
     'if (nvsNr > 0U && plikJest && panelOtaObrazPoprawny(nvsObraz.c_str())) {',
     'if (nvsNr > 0U && panelOtaObrazPoprawny(nvsObraz.c_str())) {'),
    ('oferta zapisana przez 3.58.5 (bez identyfikatora pliku) przywracana po restarcie',
     'if (nvsNr > 0U && plikJest && panelOtaObrazPoprawny(nvsObraz.c_str())) {',
     'if (plikJest) {'),
    ('oferta wystawiana, choc panel pracuje juz na tym pliku',
     '  juzMa = panelOtaPanelMaObraz(g_panelOtaPanel, obraz, teraz);',
     '  juzMa = false;'),
    ('poprzednia oferta nie jest wycofywana przed podmiana pliku',
     'bylaOferta = (panelOtaAnuluj(g_panelOta) == PANEL_OTA_ANULOWANO);',
     'bylaOferta = g_panelOta.aktywna;'),
    ('/update_panel nie sprawdza, czy plik jest wlasnie wysylany',
     '  if (panelOtaKlejTransferTrwa()) {\n    sendTelegramMessage(F("Panel wlasnie pobiera plik z centrali.',
     '  if (false) {\n    sendTelegramMessage(F("Panel wlasnie pobiera plik z centrali.'),
    ('numer oferty rosnie ponad 9 cyfr',
     '  if (nr == 0U || nr > 999999999UL) nr = 1U;   // panel odsyla numer jako najwyzej 9 cyfr\n',
     ''),
    ('plik wydawany bez sprawdzenia hasla',
     '  if (token != String(OTA_HASLO)) {',
     '  if (false) {'),
    ('identyfikator pliku widoczny w /api/status takze w czasie alarmu',
     'if (flaga) j += obraz;',
     'j += obraz;'),
    ('decyzja z obslugi HTTP nie trafia do zadania Telegrama (brak wiadomosci o wyniku)',
     '  if (w != PANEL_OTA_BEZ_ZMIAN) panelOtaZapamietajDoWykonania(w, przed, powod);',
     ''),
    ('anulowanie nie wykonuje najpierw zaleglej decyzji',
     'static void panelOtaKlejKomendaAnuluj() {\n  panelOtaKlejOkresowo();   // najpierw to, co juz sie rozstrzygnelo\n',
     'static void panelOtaKlejKomendaAnuluj() {\n'),
    ('brak pliku na dysku zostawia licznik polaczen podniesiony',
     '    portENTER_CRITICAL(&g_panelOtaMux);\n    panelOtaKoniecPobierania(g_panelOta, nr);\n    portEXIT_CRITICAL(&g_panelOtaMux);\n    zapiszLog(',
     '    zapiszLog('),
    # --- pamiec robocza zadania Telegrama (sterta zamiast stosu) ---
    ('obieg zadania Telegrama nie zwalnia pamieci roboczej (wyciek 800 B co kilka sekund)',
     '  free(r);\n}\n\n// /update_panel_anuluj (Telegram: komenda i przycisk',
     '}\n\n// /update_panel_anuluj (Telegram: komenda i przycisk'),
    ('anulowanie bez oferty nie zwalnia pamieci roboczej',
     '    free(r);\n    sendTelegramMessage(F("Nie ma czego anulowac',
     '    sendTelegramMessage(F("Nie ma czego anulowac'),
    ('gdy sterta odmowi, czekajacy wynik aktualizacji przepada bez wiadomosci',
     '  if (r == nullptr) return;   // brak pamieci: nic nie zostalo zmienione, nastepny obieg sprobuje znowu\n',
     '  if (r == nullptr) {\n    portENTER_CRITICAL(&g_panelOtaMux);\n    g_panelOtaDoWykonania.jest = false;\n    portEXIT_CRITICAL(&g_panelOtaMux);\n    return;\n  }\n'),
    ('obieg bierze pamiec robocza takze wtedy, gdy nie ma oferty',
     '  if (!jestCoRobic) return;\n',
     ''),
    ('anulowanie przy odmowie sterty konczy oferte, choc mowi, ze nic nie anulowalo',
     '  if (r == nullptr) {\n    sendTelegramMessage(F("Centrali zabraklo pamieci',
     '  if (r == nullptr) {\n    portENTER_CRITICAL(&g_panelOtaMux);\n    panelOtaAnuluj(g_panelOta);\n    portEXIT_CRITICAL(&g_panelOtaMux);\n    sendTelegramMessage(F("Centrali zabraklo pamieci'),
    ('tekst bledu pobierania zbierany w zmiennej, w ktorej jest juz wiadomosc dla uzytkownika',
     '  const bool pobrano = pobierzFirmwarePanelaZGithub(tag, blad, skan, rozmiar);\n',
     '  const bool pobrano = pobierzFirmwarePanelaZGithub(tag, tekst, skan, rozmiar);\n  blad = tekst;\n'),
    ('zalegly wynik poprzedniej oferty kasuje w NVS znacznik nastepnej',
     '  if (!jestNastepna) config.putBool("panOtaGotowa", false);\n',
     '  config.putBool("panOtaGotowa", false);\n'),
    ('wynik poprzedniej oferty wysylany dopiero po wiadomosci o nowym pliku',
     '  panelOtaKlejOkresowo();\n  sendTelegramMessage(tekst);\n  if (!pobierac) return;\n',
     '  sendTelegramMessage(tekst);\n  if (!pobierac) return;\n'),
    ('pamiec robocza zwalniana w sekcji krytycznej',
     '  free(r);\n}\n\n// /update_panel_anuluj (Telegram: komenda i przycisk',
     '  portENTER_CRITICAL(&g_panelOtaMux);\n  free(r);\n  portEXIT_CRITICAL(&g_panelOtaMux);\n}\n\n// /update_panel_anuluj (Telegram: komenda i przycisk'),
]


def main():
    rob = tempfile.mkdtemp(prefix='centrala_czulosc_ota_')
    try:
        wycinek = os.path.join(rob, 'panel_ota_wycinek.inc')
        subprocess.run([sys.executable, os.path.join(TU, 'wytnij_blok.py'), CPP, '--ota', wycinek], check=True, stdout=subprocess.DEVNULL)
        baza = open(wycinek, encoding='utf-8').read()
        test = os.path.join(TU, 'test_panel_ota.cpp')

        def zbuduj_i_uruchom(tresc, nazwa):
            kat = os.path.join(rob, nazwa)
            os.makedirs(kat)
            open(os.path.join(kat, 'panel_ota_wycinek.inc'), 'w', encoding='utf-8').write(tresc)
            k = subprocess.run(['g++', '-std=gnu++17', '-O1', '-w', '-I', kat, '-o', os.path.join(kat, 't'), test],
                               capture_output=True, text=True)
            if k.returncode != 0:
                return None, k.stderr.strip().splitlines()[:1], ''
            try:
                u = subprocess.run([os.path.join(kat, 't')], capture_output=True, text=True, timeout=120)
            except subprocess.TimeoutExpired:
                return 1, ['test nie skonczyl sie w 120 s (zawieszenie)'], 'zawieszenie'
            nieudane = [l.strip() for l in u.stdout.splitlines() if '[BLAD]' in l]
            if u.returncode != 0 and not nieudane:
                nieudane = ['test przerwany (kod %d)' % u.returncode]
            ostatnia = (u.stdout.strip().splitlines() or [''])[-1]
            return u.returncode, nieudane, ostatnia

        print('Czulosc testow kodu [v3.58.6 PANEL-OTA]: %s' % os.path.relpath(CPP, TU))
        kod, nieudane, ostatnia = zbuduj_i_uruchom(baza, 'bez_zmian')
        print('  kod bez zmian: %s' % ostatnia)
        if kod != 0:
            print('  Testy nie przechodza na niezmienionym kodzie - najpierw ./uruchom_testy.sh')
            return 2
        wykryte = 0
        for nr, (opis, stary, nowy) in enumerate(BLEDY, 1):
            if baza.count(stary) != 1:
                print('  %2d. %s\n        PRZERWANE: szukany tekst wystepuje w kodzie %d razy (ma byc 1)' % (nr, opis, baza.count(stary)))
                return 2
            kod, nieudane, ostatnia = zbuduj_i_uruchom(baza.replace(stary, nowy), 'blad_%02d' % nr)
            if kod is None:
                print('  %2d. %s\n        NIE KOMPILUJE SIE: %s' % (nr, opis, ' '.join(nieudane)[:160]))
            elif kod != 0 and nieudane:
                wykryte += 1
                liczba = ostatnia.split(',')[-1].strip() if ',' in ostatnia else ostatnia
                print('  %2d. %s\n        WYKRYTY (%s), np.: %s' % (nr, opis, liczba, nieudane[0].replace('[BLAD] ', '')[:110]))
            else:
                print('  %2d. %s\n        NIEWYKRYTY - testy przeszly mimo bledu (%s)' % (nr, opis, ostatnia))
        print('Wykryte: %d z %d celowo wprowadzonych bledow' % (wykryte, len(BLEDY)))
        return 0 if wykryte == len(BLEDY) else 1
    finally:
        shutil.rmtree(rob, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
