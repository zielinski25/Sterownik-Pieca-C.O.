// [ETAP 221] Emulator GNIAZDA SIECIOWEGO - CZESC NASZA.
//
// To jedyne miejsce, w ktorym nie ma prawdziwego kodu ESP32. Prawdziwe AsyncTCP
// robi gniazda lwIP i przerwania; tego na komputerze nie ma. Tutaj jest ten sam
// INTERFEJS (nazwy i sygnatury z AsyncTCP.h), a pod spodem bufor w pamieci:
//   * "dane z sieci"   -> Test.cpp wola Client::podajDane(),
//   * "dane do sieci"  -> wszystko, co biblioteka pisze (add/send/write),
//                         ląduje w Client::odpowiedz().
//
// Sygnatury callbackow sa te same co w AsyncTCP, bo biblioteka przypisuje do
// nich lambdy o dokladnie takich typach - kompilator to sprawdza.
#pragma once

#include <Arduino.h>
#include <Print.h>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

enum tcp_state : uint8_t {
  TCP_DISCONNECTED = 0,
  TCP_LISTEN = 1,
  TCP_SYN_SENT = 2,
  TCP_SYN_RECEIVED = 3,
  TCP_ESTABLISHED = 4,
  TCP_CLOSE_WAIT = 5,
  TCP_LAST_ACK = 6,
  TCP_TIME_WAIT = 7,
  TCP_CLOSE = 8,
};

#ifndef ASYNC_MAX_SYNC
#define ASYNC_MAX_SYNC 1460
#endif

class AsyncClient;

typedef std::function<void(void *, AsyncClient *)> AcConnectHandler;
typedef std::function<void(void *, AsyncClient *, void *, size_t)> AcDataHandler;
typedef std::function<void(void *, AsyncClient *, int8_t)> AcErrorHandler;
typedef std::function<void(void *, AsyncClient *, size_t, uint32_t)> AcAckHandler;
typedef std::function<void(void *, AsyncClient *)> AcDisconnectHandler;
typedef std::function<void(void *, AsyncClient *, uint32_t)> AcTimeoutHandler;

class AsyncClient : public Print {
 public:
  AsyncClient();
  ~AsyncClient();

  // ---- to, czego uzywa biblioteka -------------------------------------
  void onConnect(AcConnectHandler cb, void *arg = nullptr);
  void onData(AcDataHandler cb, void *arg = nullptr);
  void onError(AcErrorHandler cb, void *arg = nullptr);
  void onAck(AcAckHandler cb, void *arg = nullptr);
  void onDisconnect(AcDisconnectHandler cb, void *arg = nullptr);
  void onTimeout(AcTimeoutHandler cb, void *arg = nullptr);
  void onPoll(AcConnectHandler cb, void *arg = nullptr);

  bool connected() { return stan_ == TCP_ESTABLISHED; }
  bool freeable() { return stan_ == TCP_DISCONNECTED; }
  size_t space() { return 1460; }
  bool canSend() { return stan_ == TCP_ESTABLISHED && zajete_ <= space(); }
  size_t add(const char *data, size_t size, uint8_t apv = 0);
  bool send();
  void close(bool now = false);
  void abort();
  void stop() { close(); }
  void setNoDelay(bool nodelay) { (void)nodelay; }
  void setRxTimeout(uint32_t timeout) { (void)timeout; }
  void setAckTimeout(uint32_t timeout) { (void)timeout; }
  IPAddress remoteIP() { return IPAddress(192, 168, 1, 50); }
  uint16_t remotePort() { return 50000; }
  IPAddress localIP() { return IPAddress(192, 168, 1, 100); }
  uint16_t localPort() { return 80; }
  const char *stateToString();
  tcp_state status() { return stan_; }

  size_t write(uint8_t c) override;
  size_t write(const uint8_t *data, size_t len) override;
  using Print::write;

  // ---- narzedzia testu (nie ma ich w prawdziwym AsyncTCP) --------------
  // Wstrzykniecie bajtow "z sieci" - tak jak przerwanie lwIP wola onData.
  void podajDane(const char *dane, std::size_t dlugosc);
  void podajDane(const std::string &dane) { podajDane(dane.data(), dane.size()); }
  // Symulacja potwierdzenia TCP: mowi bibliotece, ile bajtow doszlo.
  void potwierdz(std::size_t ile);
  // Rozlaczenie: tak jak prawdziwe AsyncTCP wola onDisconnect (to kasuje obiekt
  // zadania i zwalnia znacznik zajetosci pobrania).
  void rozlacz();
  // Caly strumien, ktory biblioteka wyslala do gniazda.
  const std::string &odpowiedz() const { return wyslane_; }
  void wyczyscOdpowiedz() {
    wyslane_.clear();
    doWyslania_.clear();
  }
  bool zamkniete() const { return zamkniete_; }

 private:
  void wyslijBufor();

  AcDataHandler onData_ = nullptr;
  void *onDataArg_ = nullptr;
  AcConnectHandler onPoll_ = nullptr;
  void *onPollArg_ = nullptr;
  AcAckHandler onAck_ = nullptr;
  void *onAckArg_ = nullptr;
  AcDisconnectHandler onDisconnect_ = nullptr;
  void *onDisconnectArg_ = nullptr;

  tcp_state stan_ = TCP_ESTABLISHED;
  std::string doWyslania_;
  std::string wyslane_;
  std::size_t zajete_ = 0;
  bool zamkniete_ = false;
};

// Serwer TCP. W tescie nie przyjmuje polaczen - klienta tworzymy wprost,
// dokladnie tak, jak robi to AsyncWebServer::AsyncWebServer().
class AsyncServer {
 public:
  explicit AsyncServer(uint16_t port) : port_(port) {}
  void onClient(AcConnectHandler cb, void *arg = nullptr) {
    onClient_ = cb;
    onClientArg_ = arg;
  }
  void begin() { nasluchuje_ = true; }
  void end() { nasluchuje_ = false; }
  void setNoDelay(bool nodelay) { (void)nodelay; }
  tcp_state status() const { return nasluchuje_ ? TCP_LISTEN : TCP_DISCONNECTED; }
  bool nasluchuje() const { return nasluchuje_; }

 private:
  uint16_t port_;
  bool nasluchuje_ = false;
  AcConnectHandler onClient_ = nullptr;
  void *onClientArg_ = nullptr;
};
