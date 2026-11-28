// Test na PC (g++) logiki 3.58.11: alarm POZAR z symulacji gasnie z koncem
// symulacji, alarm z prawdziwego pomiaru - nie. Kod testowany jest WYCIETY z
// pliku centrali (wytnij_dym_sym.py -> dym_sym_wycinek.h), atrapy ponizej
// udaja Arduino, serwa i log.
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
using std::min;

// ── atrapy Arduino / centrali ──────────────────────────────────────────────
typedef std::string String;
#define F(x) x
static unsigned long g_ms = 100000;
static unsigned long millis() { return g_ms; }
static const int pinBuzer = 1, pinZasilanieBuzera = 2, LOW = 0;
static int g_buzerPin = 1; static bool g_noTone = false;
static void noTone(int) { g_noTone = true; }
static void digitalWrite(int, int v) { g_buzerPin = v; }
struct AlarmMuteTimer { int x; };
static AlarmMuteTimer alarmMuteDym;
static bool g_muteKasowany = false;
static int32_t alarmMuteRemainingMs(AlarmMuteTimer&, bool aktywny) { if (!aktywny) g_muteKasowany = true; return 0; }
static std::vector<String> g_log;
static void zapiszLogKrytyczny(const String& s) { g_log.push_back(s); }

typedef unsigned char byte;
byte czyAlarmAktywny = 0;
uint32_t dymSkasowanieAlarmuMs = 0;
bool dymNadzorPoSkasowaniuAktywny = false;
bool dymKrokSyberkaUstawiony = false;
uint32_t dymOstatniKrokSyberkaMs = 0;
byte fazaAlarmuDzwiekowego = 0;
bool czujnikDymuSwiezyOdczyt = false;
int czujnikDymuOdczyt = 0;
int progAlarmowyDymu = 500;
const int HISTEREZA_DYMU = 30;
unsigned long czasNastepnejAkcjiDymu = 0;
bool rozpalanieTrybAktywny = false, rozpalanieSerwaAktywne = false;
static bool rozpalanieSerwaWymuszenieAktywne() { return false; }
struct SerwoRuch { bool aktywny; int docelowa; };
SerwoRuch ruchKlapy = {false, 0}, ruchSyberka = {false, 0};
struct RawServo { int x; };
RawServo serwoKlapa, serwoSyberek;
int pozycjaKlapa = 0, pozycjaSyberek = 0;
static int g_zakonczono = 0, g_ruchow = 0;
static void zakonczTrybRozpalania(const char*, bool) { g_zakonczono++; }
static void zatrzymajRuchSerwa(SerwoRuch&) {}
static void zlecServoRuch(SerwoRuch&, RawServo&, int&, int, int, int, const char*) { g_ruchow++; }

#include "dym_sym_wycinek.h"

// ── pomocnicze ─────────────────────────────────────────────────────────────
static int g_ok = 0, g_blad = 0;
static void sprawdz(bool w, const char* opis) { printf("    [%s] %s\n", w ? " OK " : "BLAD", opis); (w ? g_ok : g_blad)++; }
static bool logMa(const char* frag) { for (auto& l : g_log) if (l.find(frag) != String::npos) return true; return false; }
static int ileLogow(const char* frag) { int n = 0; for (auto& l : g_log) if (l.find(frag) != String::npos) n++; return n; }
static void reset() {
  g_log.clear(); czyAlarmAktywny = 0; alarmDymuZSymulacji = false; dymSymulacjaBylaAktywna = false;
  dymNadzorPoSkasowaniuAktywny = false; dymKrokSyberkaUstawiony = false; fazaAlarmuDzwiekowego = 0;
  g_noTone = false; g_buzerPin = 1; g_muteKasowany = false; g_ms = 100000; pozycjaKlapa = 90; pozycjaSyberek = 0;
}
// Jeden tick pozar() w czesci, ktora nas dotyczy: najpierw koniec symulacji, potem odczyt.
static void tick(bool sym, int odczyt, bool swiezy = true) {
  g_ms += 4992;
  dymAlarmPoKoncuSymulacji(sym);
  czujnikDymuSwiezyOdczyt = swiezy; czujnikDymuOdczyt = odczyt;
  krokAlarmuDymu(sym);
}

int main() {
  printf("  [1] symulacja 2202 ADC -> alarm z symulacji; 'Wylacz teraz' -> alarm gasnie w tym samym ticku\n");
  reset();
  tick(true, 2202);
  sprawdz(czyAlarmAktywny == 1 && alarmDymuZSymulacji, "alarm aktywny, zrodlo = symulacja");
  sprawdz(logMa("zdarzenie=aktywacja typ=POZAR"), "log: aktywacja POZAR");
  tick(true, 2202);
  sprawdz(czyAlarmAktywny == 1, "symulacja trwa -> alarm trwa");
  fazaAlarmuDzwiekowego = 2;
  tick(false, 1, false);   // symulacja wylaczona, czujnik MQ bez swiezego odczytu (rozgrzewa sie)
  sprawdz(czyAlarmAktywny == 0 && !alarmDymuZSymulacji, "koniec symulacji -> alarm skasowany bez czekania na swiezy odczyt");
  sprawdz(logMa("zdarzenie=dezaktywacja typ=POZAR powod=koniec_symulacji"), "log: dezaktywacja ... powod=koniec_symulacji");
  sprawdz(g_noTone && g_buzerPin == LOW && fazaAlarmuDzwiekowego == 0 && g_muteKasowany, "buzzer wylaczony, faza dzwieku 0, wyciszenie skasowane");
  sprawdz(dymNadzorPoSkasowaniuAktywny && dymSkasowanieAlarmuMs == g_ms && !dymKrokSyberkaUstawiony, "nadzor po skasowaniu wlaczony jak przy zwyklej dezaktywacji");
  tick(false, 1, true);
  sprawdz(czyAlarmAktywny == 0 && ileLogow("dezaktywacja") == 1, "pozniejszy prawdziwy odczyt 1 ADC: nic nowego (jedna dezaktywacja)");

  printf("  [2] prawdziwy dym 900 ADC -> alarm prawdziwy; symulacja wlaczona i wylaczona NIE kasuje alarmu\n");
  reset();
  tick(false, 900);
  sprawdz(czyAlarmAktywny == 1 && !alarmDymuZSymulacji, "alarm aktywny, zrodlo = prawdziwy czujnik");
  tick(true, 2202);
  tick(false, 900, false);
  sprawdz(czyAlarmAktywny == 1 && !logMa("dezaktywacja"), "po symulacji alarm nadal trwa (bez wpisu dezaktywacja)");
  tick(false, 480);
  sprawdz(czyAlarmAktywny == 1, "480 ADC (>= 470 = prog - histereza): alarm trwa");
  tick(false, 469);
  sprawdz(czyAlarmAktywny == 0 && logMa("zdarzenie=dezaktywacja typ=POZAR") && !logMa("powod="), "469 ADC: zwykla dezaktywacja, dotychczasowa linia logu (bez powod=)");

  printf("  [3] alarm z symulacji; w chwili wylaczenia symulacji czujnik daje PRAWDZIWE 700 ADC\n");
  reset();
  tick(true, 2202);
  sprawdz(alarmDymuZSymulacji, "start: zrodlo = symulacja");
  tick(false, 700);   // koniec symulacji + swiezy prawdziwy odczyt nad progiem w tym samym ticku
  sprawdz(czyAlarmAktywny == 1 && !alarmDymuZSymulacji, "alarm trwa, ale jest juz prawdziwy (zrodlo = czujnik)");
  sprawdz(logMa("powod=koniec_symulacji") && ileLogow("zdarzenie=aktywacja typ=POZAR") == 2,
          "log: dezaktywacja powod=koniec_symulacji, zaraz potem ponowna aktywacja z prawdziwego odczytu");
  g_log.clear();
  tick(true, 2202);
  tick(false, 700, false);
  sprawdz(czyAlarmAktywny == 1 && !logMa("dezaktywacja"), "kolejna symulacja i jej koniec nie kasuja prawdziwego alarmu");
  tick(false, 100);
  sprawdz(czyAlarmAktywny == 0 && logMa("zdarzenie=dezaktywacja typ=POZAR") && !logMa("powod="), "100 ADC z czujnika: zwykla dezaktywacja");

  printf("  [4] symulacja PONIZEJ progu (100 ADC) i jej koniec -> nic sie nie dzieje\n");
  reset();
  tick(true, 100);
  tick(false, 100, false);
  sprawdz(czyAlarmAktywny == 0 && g_log.empty(), "bez alarmu, bez wpisow w logu");

  printf("  [5] symulacja wygasa sama (symulacjaTrwa() -> false) -> to tez koniec symulacji\n");
  reset();
  tick(true, 4095);
  tick(true, 4095);
  tick(false, 4095, false);   // symulacjaTrwa() zwrocilo false po uplywie czasu
  sprawdz(czyAlarmAktywny == 0 && logMa("powod=koniec_symulacji"), "wygasniecie symulacji kasuje alarm z symulacji");

  printf("  [6] brak alarmu, symulacja wlaczona i wylaczona -> dymAlarmPoKoncuSymulacji() zwraca false\n");
  reset();
  dymAlarmPoKoncuSymulacji(true);
  sprawdz(!dymAlarmPoKoncuSymulacji(false) && czyAlarmAktywny == 0, "zwraca false, stan bez zmian");
  sprawdz(!dymAlarmPoKoncuSymulacji(false), "drugi tick bez symulacji: dalej false (koniec liczony raz)");

  printf("  RAZEM: %d sprawdzen OK, %d BLAD\n", g_ok, g_blad);
  return g_blad ? 1 : 0;
}
