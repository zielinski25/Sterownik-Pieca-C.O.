#pragma once
// 211: swiat zewnetrzny obu funkcji wysylajacych. Kazda rzecz, ktora na
// urzadzeniu jest sprzetem/biblioteka (gniazdo TLS, semafory, WiFi, Serial,
// VFS), jest tu modelem z mozliwoscia wstrzykniecia awarii. Zaden fragment
// SAMYCH funkcji wysylajacych nie jest modelowany - sa wyciagniete z pliku
// po patchu (Extracted211.inc) i kompilowane bez zmian.
#include <cstdint>
#include <functional>
#include <string>

#include "Arduino.h"
#include "CheckedHistoryIO.h"
#include "RamCalls.h"

#define TG_HOST "api.telegram.org"

// ---------- gniazdo TLS (model) -----------------------------------------
class WiFiClientSecure {
 public:
  std::string kabel;          // wszystko, co poszlo "po kablu"
  std::string odpowiedz;      // bajty, ktore klient ma do przeczytania z powrotem
  size_t limitZapisu = 0;     // 0 = bez limitu; inaczej max bajtow na jedno write()
  long long zerowyZapisPo = -1;  // >=0: write() zwraca 0 po tylu wyslanych bajtach
  bool polaczony = true;
  std::function<void(size_t)> poZapisie;  // hook: "w trakcie wysylki cos sie dzieje"

  int fd() const { return polaczony ? 3 : -1; }

  size_t write(const uint8_t *b, std::size_t n) {
    if (!polaczony) return 0;
    if (zerowyZapisPo >= 0 && static_cast<long long>(kabel.size()) >= zerowyZapisPo) return 0;
    std::size_t k = n;
    if (limitZapisu && k > limitZapisu) k = limitZapisu;
    kabel.append(reinterpret_cast<const char *>(b), k);
    if (poZapisie) poZapisie(k);
    return k;
  }
  // Warstwa TLS w Arduino dopisuje caly bufor albo zglasza blad (petla jest
  // w srodku biblioteki), wiec print() jest "wszystko albo ile sie da".
  // Pisanie CIALA dokumentu idzie przez write(), ktory moze byc czesciowy -
  // to jest dokladnie ta roznica, ktora wysylacz musi obsluzyc sam.
  size_t print(const String &s) {
    return wypisz(reinterpret_cast<const uint8_t *>(s.c_str()), s.length());
  }
  size_t print(const char *s) {
    return wypisz(reinterpret_cast<const uint8_t *>(s), std::strlen(s));
  }
  size_t wypisz(const uint8_t *b, std::size_t n) {
    std::size_t done = 0;
    while (done < n) {
      const std::size_t k = write(b + done, n - done);
      if (k == 0) break;
      done += k;
    }
    return done;
  }
  int available() const { return static_cast<int>(odpowiedz.size() - czytane); }
  int read() { return czytane < odpowiedz.size() ? odpowiedz[czytane++] : -1; }
  bool connected() const { return polaczony && czytane < odpowiedz.size(); }
  void stop() { polaczony = false; }
  // "nowe gniazdo" - czysci kabel i licznik odczytu, jak connect() w lwIP
  void podlacz() { polaczony = true; kabel.clear(); czytane = 0; }

 private:
  size_t czytane = 0;
};

// ---------- globalne obiekty centrali, ktorych uzywaja wysylacze ---------
extern RamCalls g_ram211;
extern history193::Calls &historiaSys;  // na urzadzeniu: history193::SystemCalls

extern bool tgEnabled;
extern String tgBotToken;
extern String tgChatId;
extern bool littlefsReady;
extern volatile size_t g_histShortSize;
extern volatile size_t g_histLongSize;

extern SemaphoreHandle_t logMutex;
extern SemaphoreHandle_t tgSendMutex;
extern WiFiClientSecure tgClient;
extern bool tgClientReady;
extern unsigned long tgClientLastUse;

// przełączniki testowe
extern bool g_polaczenieOk211;
extern bool g_guardOk211;
extern bool g_sendMutexOk211;
extern int g_netFail211;
extern int g_serwis211;
extern std::string g_ostatniTag211;
extern bool g_historiaGotowa211;

inline bool historiaDostepGotowy() { return g_historiaGotowa211; }
inline void serwisujTWDTJesliZarejestrowany() {
  ++g_serwis211;
  ++g_zegar211;  // czas plynie - petla odpowiedzi zawsze sie konczy
}

#define TG_NET_FAIL() do { ++g_netFail211; } while (0)

inline bool tgEnsureConnected(WiFiClientSecure &c, bool &ready, unsigned long &lastUse) {
  if (WiFi.status() != WL_CONNECTED) { ready = false; return false; }
  if (tgBotToken.length() < 10) return false;
  if (!g_polaczenieOk211) { ready = false; return false; }
  c.podlacz();
  ready = true;
  lastUse = millis();
  return true;
}

inline bool tgWriteGuard(WiFiClientSecure &c, int = 1500) {
  if (!g_guardOk211) return false;
  return c.fd() > 0;
}

inline bool tgWezmijSendMutex(const char *tag) {
  g_ostatniTag211 = tag ? tag : "";
  if (!g_sendMutexOk211) return false;
  if (!tgSendMutex) return false;
  return xSemaphoreTake(tgSendMutex, 1) == pdTRUE;
}
