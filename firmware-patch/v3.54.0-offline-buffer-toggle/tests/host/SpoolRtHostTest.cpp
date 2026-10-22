// SpoolRtHostTest.cpp — testy hostowe przelacznika bufora offline (v3.54.0).
//
// NIE powiela kodu firmware: plik extract.inc jest generowany przez
// run_host_tests.py, ktory wycina z main_centrala.cpp DOKLADNIE te funkcje,
// ktore trafily na urzadzenie:
//   - spoolRuntimePersistArmed / spoolRuntimeArm / spoolRuntimeStop /
//     spoolRuntimeRestoreFromNvs
//   - cztery linie JSON: spool_rt / spool_pages / spool_spill / spool_replay
//   - galezie ACK: spoolrt_wl / spoolrt_wyl
//
// Uruchomienie:  python3 run_host_tests.py          (z tego katalogu)
//                python3 run_host_tests.py -v       (pokazuje JSON i logi)

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "shim.h"

// ───────────────────────── implementacje atrap
NvsStore g_nvs;
bool g_nvsWriteFails = false;
bool g_nvsOpenFails = false;
std::vector<std::string> g_logs;

bool littlefsReady = true;
TelemetryRingStub g_telemetryRing;
bool g_fbWorkerBusy = false;
bool fbLoopWorkerZajety() { return g_fbWorkerBusy; }

bool g_spoolRuntimeArmed = false;
uint32_t g_spoolRuntimeOfflineSinceMs = 0U;
uint32_t g_spoolRuntimePagesCreated = 0U;
uint32_t g_spoolRuntimeSamplesSpilled = 0U;
uint32_t g_spoolRuntimeSamplesReplayed = 0U;

uint32_t g_nowMs = 1000000U;
uint32_t millis() { return g_nowMs; }

void zapiszLog(const String& s) { g_logs.push_back(s.s); }

// ───────────────────────── kod wyciety z firmware
#include "extract.inc"

// ───────────────────────── mini-harness
static int g_pass = 0;
static int g_fail = 0;

static void ok(bool cond, const char* what) {
  if (cond) {
    ++g_pass;
  } else {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

static void eqStr(const String& got, const char* want, const char* what) {
  if (got.s == want) {
    ++g_pass;
  } else {
    ++g_fail;
    std::printf("FAIL: %s\n      oczekiwano: %s\n      otrzymano: %s\n",
                what, want, got.s.c_str());
  }
}

static bool endsWith(const String& s, const char* suf) {
  const std::string a = s.s, b = suf;
  return a.size() >= b.size() && a.compare(a.size() - b.size(), b.size(), b) == 0;
}

static bool logHas(const char* frag) {
  for (size_t i = 0; i < g_logs.size(); ++i)
    if (g_logs[i].find(frag) != std::string::npos) return true;
  return false;
}

static bool nvsStored(const char* key, bool& value) {
  std::map<std::string, std::map<std::string, uint8_t> >::iterator it =
      g_nvs.ns.find(SPOOL_G_NVS_NAMESPACE);
  if (it == g_nvs.ns.end()) return false;
  std::map<std::string, uint8_t>::iterator k = it->second.find(key);
  if (k == it->second.end()) return false;
  value = (k->second != 0);
  return true;
}

// Reset do stanu "swiezy start po wgraniu firmware": RAM wyzerowany,
// NVS zostaje (to jest sens testu - NVS przezyna restart).
static void resetRam(bool keepNvs) {
  g_spoolRuntimeArmed = false;
  g_spoolRuntimeOfflineSinceMs = 0U;
  littlefsReady = true;
  g_telemetryRing.ok = true;
  g_fbWorkerBusy = false;
  g_nvsWriteFails = false;
  g_nvsOpenFails = false;
  g_logs.clear();
  if (!keepNvs) g_nvs.ns.clear();
}

int main(int argc, char** argv) {
  const bool verbose = (argc > 1 && std::strcmp(argv[1], "-v") == 0);

  std::printf("Testy hostowe: przelacznik bufora offline (firmware v3.54.0)\n");

  // ── T1: wersja wycieta z main_centrala.cpp
  eqStr(String(FIRMWARE_VERSION), "v3.54.0", "T1 stala FIRMWARE_VERSION");
  ok(std::strcmp(FIRMWARE_VERSION, "v3.53.1") != 0, "T1 stala nie jest v3.53.1");

  // ── T2: JSON statusu (parsowanie robi runner w Pythonie)
  g_spoolRuntimeArmed = true;
  g_spoolRuntimePagesCreated = 12345U;
  g_spoolRuntimeSamplesSpilled = 678U;
  g_spoolRuntimeSamplesReplayed = 9U;
  g_nowMs = 777000U;
  const String json = buildJsonSpoolFragment();
  std::printf("#JSON#%s\n", json.s.c_str());
  ok(json.s.find("\"spool_rt\":1") != std::string::npos, "T2 spool_rt=1");
  ok(json.s.find("\"spool_pages\":12345") != std::string::npos, "T2 spool_pages");
  ok(json.s.find("\"spool_spill\":678") != std::string::npos, "T2 spool_spill");
  ok(json.s.find("\"spool_replay\":9") != std::string::npos, "T2 spool_replay");
  ok(json.s.find("\"fw\":\"v3.54.0\"") != std::string::npos, "T2 fw");
  ok(json.s.find("\"uptime\":777") != std::string::npos, "T2 uptime");
  g_spoolRuntimeArmed = false;
  ok(buildJsonSpoolFragment().s.find("\"spool_rt\":0") != std::string::npos,
     "T2 spool_rt=0 gdy bufor OFF");

  // ── T3..T7: odtworzenie po restarcie (NVS)
  // T3a: NVS calkiem pusty (swiezy modul) — begin(readonly) zwraca false.
  resetRam(false);
  spoolRuntimeRestoreFromNvs();
  ok(!g_spoolRuntimeArmed, "T3a pusty NVS -> OFF");

  // T3b: TO JEST PRAWDZIWY PRZYPADEK AKTUALIZACJI. Namespace "spoolg" juz
  // istnieje (uzywa go Spool G od starszych wersji), ale klucza "rt_on"
  // jeszcze nie ma. begin(readonly) sie UDAJE, wiec sprawdzana jest wartosc
  // domyslna getBool(). Gdyby ktos zmienil ja na true — bufor wlaczylby sie
  // sam po kazdym OTA i po kazdym zaniku pradu. Ten test to lapie.
  resetRam(false);
  {
    Preferences p;
    if (!p.begin(SPOOL_G_NVS_NAMESPACE, false)) {
      ok(false, "T3b nie udalo sie zalozyc namespace spoolg");
    } else {
      p.putBool("pages", true);  // inny klucz, jak w starym firmware
      p.end();
    }
  }
  spoolRuntimeRestoreFromNvs();
  ok(!g_spoolRuntimeArmed, "T3b namespace jest, klucza rt_on nie ma -> OFF");

  resetRam(false);
  spoolRuntimePersistArmed(true);  // jakby uzytkownik wlaczyl przed restartem
  g_spoolRuntimeArmed = false;     // restart czysci RAM
  spoolRuntimeRestoreFromNvs();
  ok(g_spoolRuntimeArmed, "T4 klucz true -> po restarcie ON");
  ok(logHas("zdarzenie=restored_from_nvs"), "T4 wpis w logu restored_from_nvs");

  resetRam(true);
  g_telemetryRing.ok = false;
  spoolRuntimeRestoreFromNvs();
  ok(!g_spoolRuntimeArmed, "T5 ring niegotowy -> ON odrzucony");
  ok(logHas("zdarzenie=restore_skip"), "T5 wpis w logu restore_skip");

  resetRam(true);
  littlefsReady = false;
  spoolRuntimeRestoreFromNvs();
  ok(!g_spoolRuntimeArmed, "T6 LittleFS niegotowy -> ON odrzucony");

  resetRam(true);
  g_nvsOpenFails = true;
  spoolRuntimeRestoreFromNvs();
  ok(!g_spoolRuntimeArmed, "T7 NVS nie otwiera sie -> OFF bez wywrotki");

  // ── T8..T9: zapis do NVS
  resetRam(false);
  ok(spoolRuntimePersistArmed(true), "T8 zapis true zwraca sukces");
  bool stored = false;
  ok(nvsStored(SPOOL_RUNTIME_NVS_KEY_ARMED, stored) && stored, "T8 w NVS jest true");
  ok(spoolRuntimePersistArmed(false), "T8 zapis false zwraca sukces");
  ok(nvsStored(SPOOL_RUNTIME_NVS_KEY_ARMED, stored) && !stored, "T8 w NVS jest false");

  resetRam(false);
  g_nvsWriteFails = true;
  ok(!spoolRuntimePersistArmed(true), "T9 odmowa zapisu -> false");
  ok(!nvsStored(SPOOL_RUNTIME_NVS_KEY_ARMED, stored), "T9 nic nie zapisano");

  // ── T10..T14: ARM (wlaczenie)
  resetRam(false);
  String wynik;
  ok(spoolRuntimeArm(wynik), "T10 ARM konczy sie sukcesem");
  ok(g_spoolRuntimeArmed, "T10 flaga ARM ustawiona");
  ok(endsWith(wynik, "; NVS ON"), "T10 ACK konczy sie '; NVS ON'");
  ok(nvsStored(SPOOL_RUNTIME_NVS_KEY_ARMED, stored) && stored, "T10 NVS=true");

  resetRam(false);
  littlefsReady = false;
  wynik = "";
  ok(!spoolRuntimeArm(wynik), "T11 brak LittleFS -> ARM odrzucony");
  eqStr(wynik, "LittleFS nie gotowy", "T11 komunikat");
  ok(!nvsStored(SPOOL_RUNTIME_NVS_KEY_ARMED, stored), "T11 NVS nietkniete");

  resetRam(false);
  g_telemetryRing.ok = false;
  wynik = "";
  ok(!spoolRuntimeArm(wynik), "T12 ring niegotowy -> ARM odrzucony");
  eqStr(wynik, "TelemetryRing nie gotowy", "T12 komunikat");
  ok(!nvsStored(SPOOL_RUNTIME_NVS_KEY_ARMED, stored), "T12 NVS nietkniete");

  resetRam(false);
  g_fbWorkerBusy = true;
  wynik = "";
  ok(!spoolRuntimeArm(wynik), "T13 worker zajety -> ARM odrzucony");
  eqStr(wynik, "Firebase worker zajety", "T13 komunikat");

  resetRam(false);
  g_nvsWriteFails = true;
  wynik = "";
  ok(spoolRuntimeArm(wynik), "T14 NVS odmawia, ale ARM i tak dziala");
  ok(g_spoolRuntimeArmed, "T14 flaga ARM ustawiona mimo bledu NVS");
  ok(endsWith(wynik, "; NVS BLAD ZAPISU"), "T14 ACK mowi o bledzie zapisu");

  // ── T15..T16: STOP
  resetRam(false);
  spoolRuntimeArm(wynik);
  wynik = "";
  ok(spoolRuntimeStop(wynik), "T15 STOP konczy sie sukcesem");
  ok(!g_spoolRuntimeArmed, "T15 flaga zdjeta");
  ok(endsWith(wynik, "; NVS OFF"), "T15 ACK konczy sie '; NVS OFF'");
  ok(nvsStored(SPOOL_RUNTIME_NVS_KEY_ARMED, stored) && !stored, "T15 NVS=false");

  resetRam(false);
  spoolRuntimeArm(wynik);
  g_nvsWriteFails = true;
  wynik = "";
  ok(spoolRuntimeStop(wynik), "T16 STOP dziala mimo bledu NVS");
  ok(endsWith(wynik, "; NVS BLAD ZAPISU"), "T16 ACK mowi o bledzie zapisu");

  // ── T17..T19: komendy z aplikacji (ACK)
  resetRam(false);
  {
    bool wynikOk = false;
    String blad;
    const bool wywolane = cmdSpoolRt("spoolrt_wl", wynikOk, blad);
    ok(wywolane, "T17 komenda spoolrt_wl rozpoznana");
    ok(wynikOk, "T17 wynikOk=true");
    eqStr(blad, "", "T17 brak bledu");
    ok(g_spoolRuntimeArmed, "T17 bufor wlaczony");
    ok(logHas("akcja=spoolrt stan=ON"), "T17 wpis w logu");
    ok(nvsStored(SPOOL_RUNTIME_NVS_KEY_ARMED, stored) && stored, "T17 NVS=true");
  }

  resetRam(false);
  g_telemetryRing.ok = false;
  {
    bool wynikOk = true;
    String blad;
    cmdSpoolRt("spoolrt_wl", wynikOk, blad);
    ok(!wynikOk, "T18 wynikOk=false przy braku ringu");
    eqStr(blad, "TelemetryRing nie gotowy", "T18 blad w ACK");
    ok(!g_spoolRuntimeArmed, "T18 bufor pozostaje OFF");
    ok(logHas("stan=ON blad="), "T18 wpis WARN w logu");
  }

  resetRam(false);
  spoolRuntimeArm(wynik);
  {
    bool wynikOk = false;
    String blad;
    cmdSpoolRt("spoolrt_wyl", wynikOk, blad);
    ok(wynikOk, "T19 wynikOk=true");
    eqStr(blad, "", "T19 brak bledu");
    ok(!g_spoolRuntimeArmed, "T19 bufor wylaczony");
    ok(logHas("akcja=spoolrt stan=OFF"), "T19 wpis w logu");
  }

  // ── T20: pelny obieg przez "restart"
  resetRam(false);
  spoolRuntimeArm(wynik);          // uzytkownik wlacza w aplikacji
  g_spoolRuntimeArmed = false;     // restart / zanik pradu
  g_spoolRuntimeOfflineSinceMs = 0U;
  spoolRuntimeRestoreFromNvs();    // setup()
  ok(g_spoolRuntimeArmed, "T20 wlaczone przed restartem -> wlaczone po");
  bool okT20 = true;
  String bladT20;
  cmdSpoolRt("spoolrt_wyl", okT20, bladT20);
  g_spoolRuntimeArmed = false;
  spoolRuntimeRestoreFromNvs();
  ok(!g_spoolRuntimeArmed, "T20 wylaczone przed restartem -> wylaczone po");

  if (verbose) {
    std::printf("\n--- JSON ---\n%s\n\n--- logi ostatniego testu ---\n",
                json.s.c_str());
    for (size_t i = 0; i < g_logs.size(); ++i)
      std::printf("  %s\n", g_logs[i].c_str());
  }

  std::printf("\nWYNIK: %d PASS, %d FAIL\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
