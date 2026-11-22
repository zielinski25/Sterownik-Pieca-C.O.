#!/usr/bin/env python3
# Buduje main_centrala.cpp v3.58.11 z pliku v3.58.10 (SHA 0f5b7020...).
# Uzycie: zrob_35811.py <main_centrala.cpp 3.58.10> <wyjscie 3.58.11>
# Kazda podmiana musi trafic dokladnie raz - inaczej skrypt sie zatrzymuje.
import sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
if 'const char* FIRMWARE_VERSION = "v3.58.10";' not in t:
    sys.exit("to nie jest plik 3.58.10")

def zamien(stare, nowe, ile=1):
    global t
    n = t.count(stare)
    assert n == ile, (n, stare[:90])
    t = t.replace(stare, nowe)

# ── 1. naglowek pliku ───────────────────────────────────────────────────────
NAGLOWEK = '''// FIRMWARE_VERSION v3.58.10 -> v3.58.11 (2026-11-24) — ALARM DYMU Z SYMULACJI GASNIE RAZEM Z SYMULACJĄ.
//
// [v3.58.11 DYM-SYM] — 2026-11-24
// Zgłoszenie użytkownika (22.11, 18:25, panel WWW „Czujnik dymu i spalin”):
// po włączeniu symulacji dymu z wartością nad progiem jest alarm — i słusznie —
// ale po „Wyłącz teraz” alarm dalej świeci („Symulacja wyłączona; alarm
// sterownika nadal aktywny — sprawdź kotłownię”), kafelek pokazuje 1 ADC
// i „próg alarmu przekroczony”.
//
// PRZYCZYNA (zachowanie zamierzone od początku, ale niewygodne przy testach):
// alarm POŻAR jest zatrzaskowy — gaśnie wyłącznie na ŚWIEŻYM, prawdziwym
// odczycie czujnika poniżej progu minus histereza (500 − 30). Czujnik MQ w
// trybie impulsowym daje świeży odczyt tylko w ostatnich sekundach okna WŁ
// (po rozgrzewaniu), więc po wyłączeniu symulacji alarm potrafił trwać
// jeszcze 2,5–3 min (≤10 s + WYŁ 60 s + stabilizacja 90 s).
//
// CO ROBI 3.58.11 (tylko blok DYM-SYM i pozar()):
// [1] Przy aktywacji alarmu POŻAR zapamiętywane jest, czy odczyt, który go
//     wywołał, był SYMULOWANY (alarmDymuZSymulacji). Gdy w trakcie takiego
//     alarmu przyjdzie prawdziwy świeży odczyt nad progiem — alarm staje się
//     prawdziwy (flaga gaśnie) i dalej obowiązują stare zasady.
// [2] Gdy symulacja dymu się kończy (Wyłącz teraz w WWW, „symuluj_stop” z
//     Firebase/Telegrama albo upływ czasu), a alarm pochodzi WYŁĄCZNIE z
//     symulacji — pozar() kasuje go w najbliższym ticku (≤ 5 s), tym samym
//     sprzątaniem co zwykła dezaktywacja (buzzer, wyciszenie, nadzór po
//     skasowaniu). Log: tag=ALARM lvl=INFO zdarzenie=dezaktywacja typ=POZAR
//     powod=koniec_symulacji.
// [3] /api/status: nowe pole "dym_alarm_sym" (true = bieżący alarm powstał
//     na odczycie symulowanym). Panel LCD i aplikacja go nie wymagają.
// Bez zmian: alarm z PRAWDZIWEGO pomiaru gaśnie tylko prawdziwym pomiarem;
//     próg, histereza, ruch serw, Telegram, OTA, telemetria, Zew 2.
// Koszt: 2 bajty RAM, zero dodatkowych odczytów.
//
'''
assert t.startswith("// FIRMWARE_VERSION v3.58.9 -> v3.58.10 (2026-11-19)")
t = NAGLOWEK + t

# ── 2. historia wersji ──────────────────────────────────────────────────────
zamien("// [v3.58.10] — 2026-11-19 — ZEW 2 BEZ PRZERW: PONOWIENIE PO BLEDZIE DHT22\n",
       "// [v3.58.11] — 2026-11-24 — ALARM DYMU Z SYMULACJI GASNIE RAZEM Z SYMULACJA\n"
       "// Co: Alarm POZAR wywolany wylacznie symulacja dymu jest kasowany w <= 5 s\n"
       "//     po zakonczeniu symulacji (WWW/Firebase/uplyw czasu); alarm z prawdziwego\n"
       "//     pomiaru gasnie jak dotad - tylko prawdziwym pomiarem. /api/status: dym_alarm_sym.\n"
       "// Problem: po \"Wylacz teraz\" alarm swiecil jeszcze 2,5-3 min (do swiezego odczytu MQ).\n"
       "// Przyczyna: dezaktywacja wymaga swiezego odczytu < prog-histereza; czujnik impulsowy\n"
       "//     daje go dopiero po WYL 60 s + stabilizacji 90 s.\n"
       "// Naprawa: blok DYM-SYM przed pozar() (alarmDymuZSymulacji, dymAlarmSkasuj(),\n"
       "//     dymAlarmPoKoncuSymulacji()); szczegoly w naglowku [v3.58.11 DYM-SYM].\n"
       "//\n"
       "// [v3.58.10] — 2026-11-19 — ZEW 2 BEZ PRZERW: PONOWIENIE PO BLEDZIE DHT22\n")

# ── 3. numer wersji ─────────────────────────────────────────────────────────
zamien('static const char* FIRMWARE_VERSION = "v3.58.10";',
       'static const char* FIRMWARE_VERSION = "v3.58.11";')

# ── 4. blok DYM-SYM przed pozar() ──────────────────────────────────────────
BLOK = '''// --- DYM-SYM-POCZATEK --- [v3.58.11]
// Alarm POZAR wywolany WYLACZNIE symulacja dymu (od wlaczenia symulacji zaden
// prawdziwy swiezy odczyt nie przekroczyl progu) jest kasowany od razu po jej
// zakonczeniu - "Wylacz teraz" w WWW, "symuluj_stop" z Firebase/Telegrama,
// uplyw czasu. Alarm z prawdziwego pomiaru gasnie tylko prawdziwym pomiarem
// (odczyt < prog - HISTEREZA_DYMU), dokladnie jak dotad.
bool alarmDymuZSymulacji = false;          // true = biezacy alarm POZAR powstal na odczycie symulowanym
static bool dymSymulacjaBylaAktywna = false;   // wynik symulacjaTrwa() z poprzedniego ticku pozar()

// Wspolne sprzatanie po skasowaniu alarmu POZAR (dotad wpisane wprost w
// pozar()). powod == nullptr -> dotychczasowa linia logu bez zmian.
static void dymAlarmSkasuj(const char* powod) {
  if (powod == nullptr) {
    zapiszLogKrytyczny(F("tag=ALARM lvl=INFO zdarzenie=dezaktywacja typ=POZAR"));
  } else {
    zapiszLogKrytyczny(String(F("tag=ALARM lvl=INFO zdarzenie=dezaktywacja typ=POZAR powod=")) + powod);
  }
  czyAlarmAktywny = 0;
  alarmDymuZSymulacji = false;
  dymSkasowanieAlarmuMs = millis();
  dymNadzorPoSkasowaniuAktywny = true;
  dymKrokSyberkaUstawiony = false;
  alarmMuteRemainingMs(alarmMuteDym, false); // kasuje tylko mute "do skasowania"
  noTone(pinBuzer);
  digitalWrite(pinZasilanieBuzera, LOW);
  fazaAlarmuDzwiekowego = 0;
}

// Wolane raz na tick pozar() z wynikiem symulacjaTrwa(). Zwraca true, gdy
// w tym ticku skasowano alarm pochodzacy z symulacji.
static bool dymAlarmPoKoncuSymulacji(bool symDymTeraz) {
  const bool koniec = dymSymulacjaBylaAktywna && !symDymTeraz;
  dymSymulacjaBylaAktywna = symDymTeraz;
  if (!koniec || czyAlarmAktywny != 1 || !alarmDymuZSymulacji) return false;
  dymAlarmSkasuj("koniec_symulacji");
  return true;
}
// --- DYM-SYM-KONIEC ---

void pozar() {
'''
zamien("void pozar() {\n", BLOK)

# ── 5. pozar(): wywolanie po symulacjaTrwa() ───────────────────────────────
zamien('''  bool symDymTeraz = symulacjaTrwa(symulacjaDymAktywna, symulacjaDymDo, "dym");
  bool nadzorPoRuchuSerwa = dymNadzorAktywny();
''', '''  bool symDymTeraz = symulacjaTrwa(symulacjaDymAktywna, symulacjaDymDo, "dym");
  // [v3.58.11 DYM-SYM] koniec symulacji kasuje alarm, ktory z niej powstal.
  dymAlarmPoKoncuSymulacji(symDymTeraz);
  bool nadzorPoRuchuSerwa = dymNadzorAktywny();
''')

# ── 6. pozar(): aktywacja - zapamietaj zrodlo; prawdziwy odczyt "uprawdziwia" alarm
zamien('''  if (czujnikDymuSwiezyOdczyt && czujnikDymuOdczyt > progAlarmowyDymu) {
    czasNastepnejAkcjiDymu = millis() + 10000UL;

    if (czyAlarmAktywny == 0) {
''', '''  if (czujnikDymuSwiezyOdczyt && czujnikDymuOdczyt > progAlarmowyDymu) {
    czasNastepnejAkcjiDymu = millis() + 10000UL;
    // [v3.58.11 DYM-SYM] prawdziwy odczyt nad progiem w trakcie alarmu z
    // symulacji -> od tej chwili to zwykly alarm (koniec symulacji go nie skasuje).
    if (czyAlarmAktywny == 1 && !symDymTeraz) alarmDymuZSymulacji = false;

    if (czyAlarmAktywny == 0) {
''')
zamien('''      dymOstatniKrokSyberkaMs = millis();
      dymKrokSyberkaUstawiony = true;
      czyAlarmAktywny = 1;
    }
''', '''      dymOstatniKrokSyberkaMs = millis();
      dymKrokSyberkaUstawiony = true;
      czyAlarmAktywny = 1;
      alarmDymuZSymulacji = symDymTeraz;   // [v3.58.11 DYM-SYM] skad wzial sie ten alarm
    }
''')

# ── 7. pozar(): dezaktywacja przez wspolna funkcje (ta sama linia logu) ────
zamien('''    if (czyAlarmAktywny == 1) {
      zapiszLogKrytyczny(F("tag=ALARM lvl=INFO zdarzenie=dezaktywacja typ=POZAR"));
      czyAlarmAktywny = 0;
      dymSkasowanieAlarmuMs = millis();
      dymNadzorPoSkasowaniuAktywny = true;
      dymKrokSyberkaUstawiony = false;
      alarmMuteRemainingMs(alarmMuteDym, false); // kasuje tylko mute "do skasowania"
      noTone(pinBuzer);
      digitalWrite(pinZasilanieBuzera, LOW);
      fazaAlarmuDzwiekowego = 0;
    }
''', '''    if (czyAlarmAktywny == 1) {
      dymAlarmSkasuj(nullptr);   // [v3.58.11 DYM-SYM] wspolne sprzatanie; log bez zmian
    }
''')

# ── 7b. deklaracja przed /api/status (definicja w bloku DYM-SYM przed pozar())
zamien("bool dymNadzorAktywny();\nvoid odnowDymNadzorPoRuchu();\n",
       "bool dymNadzorAktywny();\nextern bool alarmDymuZSymulacji;   // [v3.58.11 DYM-SYM] definicja przed pozar(), czytane w /api/status\nvoid odnowDymNadzorPoRuchu();\n")

# ── 8. /api/status: dym_alarm_sym ──────────────────────────────────────────
zamien('''  j += F("\\"dym_alarm\\":"); j += (czyAlarmAktywny ? "true" : "false"); j += F(",");
  {
    int32_t dymMuteMs = alarmMuteRemainingMs(alarmMuteDym, czyAlarmAktywny == 1);
''', '''  j += F("\\"dym_alarm\\":"); j += (czyAlarmAktywny ? "true" : "false"); j += F(",");
  // [v3.58.11 DYM-SYM] true = biezacy alarm powstal na odczycie symulowanym
  // (zgasnie sam z koncem symulacji). Dodatkowe pole - panel LCD i aplikacja go nie wymagaja.
  j += F("\\"dym_alarm_sym\\":"); j += ((czyAlarmAktywny == 1 && alarmDymuZSymulacji) ? "true" : "false"); j += F(",");
  {
    int32_t dymMuteMs = alarmMuteRemainingMs(alarmMuteDym, czyAlarmAktywny == 1);
''')

open(dst, "w", encoding="utf-8", newline="\n").write(t)
print("zrob_35811.py: OK, zapisano", dst)
