// symulacja.h
// ============================================================================
// CENTRALA TESTOWA NA ESP32-C3 — silnik symulacji, JSON-y i historia (rdzen)
// ============================================================================
// Ten plik udaje "fizyczny swiat" prawdziwego pieca: temperatury, pompe,
// serwo (klapa/syberek), czujnik dymu, alarmy, historię do wykresow. Nie
// steruje ZADNYM przekaźnikiem i nie czyta ZADNEGO czujnika — na C3 (bez
// PSRAM, z 6 uzytecznymi GPIO mniej niz S3) to jedyny sensowny sposob, zeby
// panel LCD mial z czym rozmawiac.
//
// KONTRAKT: kazda funkcja budujaca JSON/CSV odtwarza 1:1 to, co wysyla
// main_centrala.cpp (nazwy pol, kolejnosc, liczba kolumn CSV). Panel LCD
// czyta te dane "na slepo" (ArduinoJson + indeksy kolumn), wiec brak pola
// NIE jest bledem krytycznym, ale ZLA LICZBA KOLUMN w CSV już psuje wykresy —
// patrz komentarz przy historiaCsv() nizej.
//
// DYNAMIKA: celowo PRZYSPIESZONA ok. 5x wzgledem realnego pieca (realne
// nagrzewanie 20->80 C to dziesiatki minut, tu minuty), zeby test panelu na
// biurku nie wymagal czekania godziny na pierwszy cykl pompy. Stale czasowe sa
// w jednym miejscu (sekcja "STALE SYMULACJI"), latwe do spowolnienia.
// ============================================================================
#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "wspolne.h"
#include "ustawienia.h"

namespace sym {

// ---------------------------------------------------------------------------
// STALE SYMULACJI (wszystko w sekundach / stopniach)
// ---------------------------------------------------------------------------
static const int   HIST_KROTKA_MAX = 360;   // 360 probek x 60 s = 6 h
static const int   HIST_DLUGA_MAX  = 120;   // 120 probek x 1 h  = 5 dni
static const unsigned long PROBKA_CO_MS = 60000UL;   // probka historii krotkiej

// Bezwladnosc cieplna (tau w sekundach). Realny piec: setki sekund-minuty;
// tu podzielone przez ~5, zeby test byl szybki, a wskazania nadal plynne.
static const float TAU_GRZANIE_S      = 150.0f;
static const float TAU_STYGNIECIE_S   = 600.0f;
static const float TAU_BOJLER_S       = 900.0f;
static const float TAU_POKOJ_S        = 1800.0f;
static const float TAU_POWROT_S       = 120.0f;
static const float TAU_TROCINY_S      = 300.0f;
static const float SREDNIA_ALFA       = 1.0f / 60.0f;  // wygladzanie t_ogrz_sr

// W centrali servo() w trybie AUTO jest wolane RZADKO — tylko przy zmianach
// fazy pompy (co minuty), nie co sekunde. Gdyby symulator ruszal klapa co
// tick, temperatura (tau 150 s) nie nadazylaby za nia i klapa latalaby 0-180
// w kolko. Dlatego w AUTO obowiazuje ten sam "rzadki krok" co w produkcie:
// jeden krok (skokKlapy/skokSyberka) nie czesciej niz co SERWO_AUTO_MIN_MS.
// W trybie RECZNY/BEZPIECZNA predkosc jest wieksza (reakcja na suwak).
static const unsigned long SERWO_AUTO_MIN_MS = 30000UL;  // 30 s

// Pozycje serw w STOPNIACH — dokladnie tak, jak raportuje je centrala
// (pozycjaKlapa 0..180, pozycjaSyberek 0..90; patrz [v3.31.24] w
// main_centrala.cpp). UWAGA: panel LCD wyswietla te wartosci z dopiskiem "%",
// wiec przy pelnym otwarciu klapy pokaze "180 %" — to istniejąca niezgodnosc
// jednostek po stronie panelu (patrz README, sekcja "Znalezione przy okazji"),
// a nie blad symulatora. Symulator CELOWO raportuje stopnie, zeby test
// odzwierciedlal produkt.
static const int KLAPA_MAX_DEG   = 180;
static const int SYBERKA_MAX_DEG = 90;

// ---------------------------------------------------------------------------
// Probki historii — format zgodny z main_centrala.cpp:
//  * krotka: 13 wartosci + etykieta + czas_unix (14 kolumn CSV, czas = kol. 13)
//  * dluga : 11 pol x (sr,min,max) + etykieta + czas_unix (35 kolumn, czas = 34)
// Wartosci trzymane jako int16 w skali x10 (temperatury, cisnienie), zeby
// zmiescic 6 h + 5 dni historii w ~23 KB RAM (C3 nie ma PSRAM).
// ---------------------------------------------------------------------------
struct ProbkaKrotka {
  char etykieta[6];      // "HH:MM"
  int16_t t_zewn_x10;
  int16_t t_bojler_x10;
  int16_t t_ogrz_x10;
  int16_t t_ogrz_sr_x10;
  int16_t t_powrot_x10;
  int16_t t_panel_x10;
  int16_t t_pokoj_x10;
  uint8_t pompa;         // 0/1
  int16_t cisnienie_x10;
  int16_t wilgotnosc_x10;
  int16_t klapa_deg;
  int16_t syberka_deg;
  uint32_t czas_unix;    // 0 = czas nieznany
};

struct ProbkaDluga {
  char etykieta[12];     // "DD.MM HH:MM"
  int16_t sr_x10[11];
  int16_t min_x10[11];
  int16_t max_x10[11];
  uint32_t czas_unix;
};

// Kolejnosc pol bufora dlugiego — MUSI byc identyczna z WYKRESY_SERIE[]
// w panelu (dashboard_lvgl_poc.cpp): ZEWN, BOJLER, Temp C.O, Temp C.O Sr.,
// POWROT, PANEL, Wewnetrzna, CISNIENIE, WILGOTNOSC, KLAPA, Syberek.
static const int POL_DLUGICH = 11;

// ---------------------------------------------------------------------------
// Kontekst z warstwy Arduino (siec, OTA, diagnostyka) — rdzen go tylko
// przepisuje do JSON-a, nie interpretuje.
// ---------------------------------------------------------------------------
struct Kontekst {
  const char* ip = "192.168.4.1";
  int rssi = 0;
  bool internet = false;
  const char* wifiMode = "AP";
  const char* wifiSsid = "";
  bool panelOtaDostepna = false;
  const char* panelOtaWersja = "";
  const char* panelFwZgloszony = "";
  const char* panelResetPowod = "";
  unsigned long uptimeS = 0;
  unsigned long heapFree = 0;
  int stacje = 0;
  // millis() przekazane z warstwy Arduino — potrzebne do pol czasu
  // ("pompa_ms_od_zmiany", override'y); rdzen sam nie zna zegara systemowego.
  unsigned long terazMs = 0;
};

// ---------------------------------------------------------------------------
// Stan symulacji
// ---------------------------------------------------------------------------
struct Stan {
  // --- pomiary ---
  float tZewn = 8.0f;
  float tBojler = 42.0f;
  float tOgrz = 20.0f;
  float tOgrzSr = 20.0f;
  float tPanel = 0.0f;
  float tPokoj = 21.0f;
  float tTrociny = 20.0f;
  float tPowrot = 19.0f;
  float wilgotnosc = 48.0f;
  float cisnienie = 1013.0f;
  int   dym = 250;
  bool  dymSwiezy = true;
  bool  dymWlaczonyTemp = false;
  bool  alarmDym = false;

  // --- elementy wykonawcze ---
  int   klapaDeg = 0;
  int   syberkaDeg = 0;
  bool  pompaOn = false;
  bool  mieszadloOn = false;
  bool  rozpalanie = false;

  // --- alarmy ---
  bool  alarmOgrz = false;
  bool  alarmPanel = false;

  // --- pogoda (deszcz z prognozy w centrali; tu wstrzykiwany) ---
  int   deszczStan = 0;    // 0=brak 1=nadciaga 2=pada teraz
  int   deszczMin = -1;    // -1 = nieznane

  // --- zadania z API / reczne ---
  int   celKlapaDeg = -1;    // -1 = brak zadania (tylko tryb reczny)
  int   celSyberkaDeg = -1;
  unsigned long pompaOverrideDo = 0;
  bool  pompaOverrideStan = false;
  unsigned long serwoOverrideDo = 0;
  unsigned long mieszadloOverrideDo = 0;
  unsigned long pompaZmianaMs = 0;

  // --- wstrzykiwanie awarii (testy panelu: alarmy, deszcz) ---
  bool  injDym = false;
  bool  injAlarmOgrz = false;
  bool  injAlarmPanel = false;

  // --- stan wewnetrzny symulacji ---
  unsigned long pompaCyklStartMs = 0;
  bool  pompaCyklStart = false;
  bool  zatrzaskPompy = false;
  bool  blokadaNiskiejTemp = false;
  unsigned long ostatniCyklNr = 0;
  unsigned long dymNadProgiemOd = 0;
  unsigned long dymPodProgiemOd = 0;
  unsigned long mieszadloFazaMs = 0;
  unsigned long serwoAutoKrokMs = 0;   // chwila ostatniego kroku auto-serwa
  unsigned long ostatniaProbkaSek = 0;
  int   ostatniaGodzina = -1;
};

extern Stan st;
extern ProbkaKrotka histKrotka[HIST_KROTKA_MAX];
extern ProbkaDluga histDluga[HIST_DLUGA_MAX];
extern int histKrotkaLiczba;
extern int histKrotkaGlowa;
extern int histDlugaLiczba;
extern int histDlugaGlowa;

// Akumulator godziny dla bufora dlugiego (sr/min/max z probek 60-sekundowych).
struct Akumulator {
  int n = 0;
  float sum[POL_DLUGICH] = {0};
  float min_[POL_DLUGICH] = {0};
  float max_[POL_DLUGICH] = {0};
  uint32_t czas = 0;
  char etykieta[12] = {0};   // "DD.MM HH:00" z pierwszej probki tej godziny
};
extern Akumulator akum;

// ---------------------------------------------------------------------------
// Reset do stanu poczatkowego (uzywane tez przez testy na PC).
// ---------------------------------------------------------------------------
void reset();

// ---------------------------------------------------------------------------
// Jeden krok symulacji. Wywolywany co ~1000 ms z loop().
//   terazMs — millis()
//   czas    — czas cywilny (moze byc nieznany: czas.ok == false)
//   u       — biezace ustawienia (symulacja ich slucha, np. progAlarmTemp)
// ---------------------------------------------------------------------------
void tick(unsigned long terazMs, const wsp::Czas& czas, ustaw::Stan& u);

// ---------------------------------------------------------------------------
// Budowanie JSON-ow. Zwracaja dlugosc zapisanego tekstu; `false` z
// JsonBuf::ok() (sprawdzane w testach) oznacza, ze bufor byl za maly.
// ---------------------------------------------------------------------------
size_t zbudujStatus(char* out, size_t n, const Kontekst& k, const ustaw::Stan& u,
                    const wsp::Czas& czas);
size_t zbudujUstawienia(char* out, size_t n, const ustaw::Stan& u);

// ---------------------------------------------------------------------------
// CSV historii. `dlugi=false` -> /api/history (14 kolumn),
// `dlugi=true`  -> /api/history/long (35 kolumn).
// Wiersze sa oddawane po jednym przez `pisarz` — na C3 nie budujemy calego
// CSV w pamieci (oszczednosc ~25 KB szczytu).
// ---------------------------------------------------------------------------
void historiaCsv(bool dlugi, wsp::PisarzFn pisarz, void* ctx);
void wyczyscHistorie();
// Wypelnia oba bufory historii wymyslonymi, ale ciaglymi probkami (6 h krotkiej,
// 24 h dlugiej) — zeby ekran "Wykresy" mial od razu co rysowac i nie trafial na
// szereg z jednym punktem albo min == max.
void zasiejHistorie(const wsp::Czas& czas);

// Ile minut zostalo do wygasniecia override'u (0 = brak) — to samo wyliczenie
// co w wifiZbudujJsonStatus() centrali.
int minutyDo(unsigned long terazMs, unsigned long docelowy);

// Ustawienie zadania serwa (jak /api/servo-manual w centrali). `procent`
// 0..100 przeliczane na stopnie: klapa 0..180, syberek 0..90.
// Zwraca false, gdy os nie jest klapa/syberek.
bool zlecSerwo(const char* os, int procent, ustaw::Stan& u, unsigned long terazMs,
               char* err, size_t errLen);

// Reczny override pompy (jak /api/pompa-manual): "auto" / "wl" / "wyl".
bool zlecPompe(const char* stan, unsigned long terazMs, char* err, size_t errLen);

// Reczny override mieszadla (jak /api/mieszadlo-manual).
bool zlecMieszadlo(const char* stan, unsigned long terazMs, char* err, size_t errLen);

}  // namespace sym
