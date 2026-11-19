// ============================================================================
// Testy na PC bloku [v3.58.7 START-TG] centrali: decyzja i tresc wiadomosci
// startowej na Telegram oraz rodzaj niezgodnosci TAGCHECK po aktualizacji.
//
// Testowany jest tekst WYCIETY z ../src/main_centrala.cpp miedzy znacznikami
// "[v3.58.7 START-TG-BEGIN]" i "[v3.58.7 START-TG-END]" (plik
// start_tg_wycinek.inc powstaje przy kazdym uruchomieniu testow), a nie jego
// kopia. Blok nie uzywa String ani niczego z Arduino - potrzebuje tylko
// esp_reset_reason_t (ponizej ta sama lista co w esp_system.h ESP-IDF 5.x).
// Uruchomienie: ./uruchom_testy.sh
// ============================================================================
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

typedef enum {
  ESP_RST_UNKNOWN = 0, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT,
  ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO,
} esp_reset_reason_t;

#include "start_tg_wycinek.inc"

static int g_ok = 0, g_zle = 0;
#define SPRAWDZ(w) do { if (w) g_ok++; else { g_zle++; std::printf("      NIE PRZESZLO  %s:%d  %s\n", __FILE__, __LINE__, #w); } } while (0)
static bool ma(const char* tekst, const char* fragment) { return std::strstr(tekst, fragment) != nullptr; }

static void testDecyzja() {
  // Zwykle starty bez innego raportu -> wiadomosc startowa.
  SPRAWDZ(startTgDecyzja(ESP_RST_SW, false, false) == START_TG_WYSLIJ);
  SPRAWDZ(startTgDecyzja(ESP_RST_EXT, false, false) == START_TG_WYSLIJ);
  SPRAWDZ(startTgDecyzja(ESP_RST_UNKNOWN, false, false) == START_TG_WYSLIJ);
  SPRAWDZ(startTgDecyzja(ESP_RST_DEEPSLEEP, false, false) == START_TG_WYSLIJ);
  // Starty, o ktorych centrala pisze juz gdzie indziej -> bez dubla.
  SPRAWDZ(startTgDecyzja(ESP_RST_POWERON, false, false) == START_TG_POWERON_OSOBNO);
  SPRAWDZ(startTgDecyzja(ESP_RST_PANIC, true, false) == START_TG_CRASH_OSOBNO);
  SPRAWDZ(startTgDecyzja(ESP_RST_TASK_WDT, true, false) == START_TG_CRASH_OSOBNO);
  SPRAWDZ(startTgDecyzja(ESP_RST_BROWNOUT, true, false) == START_TG_CRASH_OSOBNO);
  SPRAWDZ(startTgDecyzja(ESP_RST_SW, false, true) == START_TG_NADZOR_OSOBNO);   // nadzor restartuje przez ESP.restart() = SW
  // POWERON ma pierwszenstwo nawet z dziwnymi flagami; crash przed nadzorem.
  SPRAWDZ(startTgDecyzja(ESP_RST_POWERON, true, true) == START_TG_POWERON_OSOBNO);
  SPRAWDZ(startTgDecyzja(ESP_RST_PANIC, true, true) == START_TG_CRASH_OSOBNO);
}

static void testTresc() {
  char b[200];
  // Restart po /update na tag 3.58.7, partycja czeka na potwierdzenie.
  int n = startTgTresc(b, sizeof(b), "v3.58.7", 123, ESP_RST_SW, true, "3.58.7");
  SPRAWDZ(n > 0 && (size_t)n < sizeof(b));
  SPRAWDZ(std::strcmp(b, "Centrala wystartowala: v3.58.7 (boot=123). Powod restartu: programowy (SW) - po aktualizacji OTA (tag 3.58.7).") == 0);
  char d[200];
  n = startTgDopisek(d, sizeof(d), true, 10);
  SPRAWDZ(n > 0 && (size_t)n < sizeof(d));
  SPRAWDZ(ma(d, "10 min stabilnej pracy") && ma(d, "Aktualizacja zakonczona pomyslnie"));
  SPRAWDZ(std::strlen(b) + std::strlen(d) < 400);   // cala wiadomosc krotka (Telegram: limit 4096, powiadomienie na telefonie pokazuje ok. 200)
  // Zwykly restart programowy (np. /restart): bez OTA, bez dopisku.
  n = startTgTresc(b, sizeof(b), "v3.58.7", 7, ESP_RST_SW, false, "");
  SPRAWDZ(std::strcmp(b, "Centrala wystartowala: v3.58.7 (boot=7). Powod restartu: programowy (SW).") == 0);
  n = startTgDopisek(d, sizeof(d), false, 10);
  SPRAWDZ(n == 0 && d[0] == '\0');
  // OTA bez zapisanego tagu (znacznik jest, tag pusty) - bez nawiasu.
  startTgTresc(b, sizeof(b), "v3.58.7", 8, ESP_RST_SW, true, "");
  SPRAWDZ(std::strcmp(b, "Centrala wystartowala: v3.58.7 (boot=8). Powod restartu: programowy (SW) - po aktualizacji OTA.") == 0);
  startTgTresc(b, sizeof(b), "v3.58.7", 8, ESP_RST_SW, true, nullptr);
  SPRAWDZ(ma(b, "po aktualizacji OTA.") && !ma(b, "(tag"));
  // Przycisk RESET i nieznany powod.
  startTgTresc(b, sizeof(b), "v3.58.7", 9, ESP_RST_EXT, false, nullptr);
  SPRAWDZ(ma(b, "przycisk RESET / pin EN (EXT)"));
  startTgTresc(b, sizeof(b), "v3.58.7", 9, ESP_RST_UNKNOWN, false, nullptr);
  SPRAWDZ(ma(b, "nieznany (UNKNOWN)"));
  // Za maly bufor: snprintf obcina, zwraca pelna dlugosc, nie przepelnia.
  char m[40];
  std::memset(m, 'x', sizeof(m));
  n = startTgTresc(m, 32, "v3.58.7", 123, ESP_RST_SW, true, "3.58.7");
  SPRAWDZ(n > 32 && m[31] == '\0' && m[32] == 'x');
  n = startTgDopisek(m, 0, true, 10);
  SPRAWDZ(n > 0);
  n = startTgDopisek(m, 0, false, 10);
  SPRAWDZ(n == 0);
  // Najdluzszy realny przypadek: dlugie wersje i tag.
  n = startTgTresc(b, sizeof(b), "v3.100.100", 4294967295U, ESP_RST_DEEPSLEEP, true, "3.100.100-rc1");
  SPRAWDZ(n > 0 && (size_t)n < 200);
}

static void testTagcheck() {
  // Znak porownania: porownajWersje(FIRMWARE_VERSION, tag).
  SPRAWDZ(tagcheckRodzaj(0) == TAGCHECK_ZGODNA);
  SPRAWDZ(tagcheckRodzaj(-1) == TAGCHECK_BINARKA_STARSZA);
  SPRAWDZ(tagcheckRodzaj(1) == TAGCHECK_BINARKA_NOWSZA);
  char b[560];
  // 19.11: Release 3.58.5, dziala v3.58.6 -> Release ma za maly numer.
  int n = tagcheckTresc(b, sizeof(b), TAGCHECK_BINARKA_NOWSZA, "3.58.5", "v3.58.6");
  SPRAWDZ(n > 0 && (size_t)n < sizeof(b));
  SPRAWDZ(ma(b, "dziala wersja v3.58.6") && ma(b, "NOWSZA niz tag Release (3.58.5)"));
  SPRAWDZ(ma(b, "Kod jest dobry") && ma(b, "Popraw tag i nazwe Release na numer dzialajacej wersji (v3.58.6, bez \"v\""));
  SPRAWDZ(ma(b, "Petli nie bedzie") && !ma(b, "UWAGA") && !ma(b, "PRZED podniesieniem"));
  // Klasyczny blad: Release 3.58.7, dziala v3.58.6 -> nie podbito wersji w kodzie.
  n = tagcheckTresc(b, sizeof(b), TAGCHECK_BINARKA_STARSZA, "3.58.7", "v3.58.6");
  SPRAWDZ(n > 0 && (size_t)n < sizeof(b));
  SPRAWDZ(ma(b, "UWAGA") && ma(b, "dziala wersja v3.58.6") && ma(b, "mial byc zainstalowany 3.58.7"));
  SPRAWDZ(ma(b, "STARSZA niz jego tag") && ma(b, "FIRMWARE_VERSION = v3.58.7") && ma(b, "w kolko probowal to samo"));
  SPRAWDZ(!ma(b, "Kod jest dobry"));
  // Zgodna: dotychczasowy tekst sukcesu, co do znaku.
  tagcheckTresc(b, sizeof(b), TAGCHECK_ZGODNA, "3.58.7", "v3.58.7");
  SPRAWDZ(std::strcmp(b, "Aktualizacja zakonczona pomyslnie. Dziala wersja: v3.58.7") == 0);
  // Najdluzsze realne tagi mieszcza sie w buforze kodu klejacego (560 B).
  n = tagcheckTresc(b, sizeof(b), TAGCHECK_BINARKA_STARSZA, "3.100.100", "v3.100.99");
  SPRAWDZ(n > 0 && (size_t)n < 560);
  n = tagcheckTresc(b, sizeof(b), TAGCHECK_BINARKA_NOWSZA, "3.100.99", "v3.100.100");
  SPRAWDZ(n > 0 && (size_t)n < 560);
  std::printf("    (binarka nowsza, 19.11)  %s\n", b);
}

int main() {
  testDecyzja();
  testTresc();
  testTagcheck();
  std::printf("    start_telegram: %d sprawdzen OK, %d nie przeszlo\n", g_ok, g_zle);
  return g_zle == 0 ? 0 : 1;
}
