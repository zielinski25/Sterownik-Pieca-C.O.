// [ETAP 221] Implementacja emulatora gniazda - CZESC NASZA.
#include "AsyncTCP.h"

#include <algorithm>

AsyncClient::AsyncClient() = default;
AsyncClient::~AsyncClient() = default;

void AsyncClient::onConnect(AcConnectHandler cb, void *arg) {
  (void)cb;
  (void)arg;
}
void AsyncClient::onData(AcDataHandler cb, void *arg) {
  onData_ = cb;
  onDataArg_ = arg;
}
void AsyncClient::onError(AcErrorHandler cb, void *arg) {
  (void)cb;
  (void)arg;
}
void AsyncClient::onAck(AcAckHandler cb, void *arg) {
  onAck_ = cb;
  onAckArg_ = arg;
}
void AsyncClient::onDisconnect(AcDisconnectHandler cb, void *arg) {
  onDisconnect_ = cb;
  onDisconnectArg_ = arg;
}
void AsyncClient::onTimeout(AcTimeoutHandler cb, void *arg) {
  (void)cb;
  (void)arg;
}
void AsyncClient::onPoll(AcConnectHandler cb, void *arg) {
  onPoll_ = cb;
  onPollArg_ = arg;
}

size_t AsyncClient::add(const char *data, size_t size, uint8_t apv) {
  (void)apv;
  if (data == nullptr || size == 0) return 0;
  const size_t ile = std::min(size, space());
  doWyslania_.append(data, ile);
  zajete_ += ile;
  return ile;
}

bool AsyncClient::send() {
  if (doWyslania_.empty()) return true;
  wyslane_ += doWyslania_;
  doWyslania_.clear();
  return true;
}

size_t AsyncClient::write(uint8_t c) { return write(&c, 1); }

size_t AsyncClient::write(const uint8_t *data, size_t len) {
  // Prawdziwe AsyncTCP: write() = add() + send(), czyli dane najpierw trafiaja
  // do bufora wysylki, a potem sa wysylane. Kolejnosc wobec add() musi byc
  // zachowana, inaczej naglowek i cialo odpowiedzi by sie rozjechaly.
  if (data == nullptr || len == 0) return 0;
  const size_t ile = add(reinterpret_cast<const char *>(data), len);
  send();
  return ile;
}

void AsyncClient::close(bool now) {
  (void)now;
  // UWAGA: prawdziwe AsyncTCP po zamknieciu wola onDisconnect (i to kasuje
  // obiekt zadania). Tutaj tylko oznaczamy stan - patrz README, rozdzial
  // "Granica emulacji".
  zamkniete_ = true;
  stan_ = TCP_CLOSE;
  wyslijBufor();
}

void AsyncClient::abort() {
  zamkniete_ = true;
  stan_ = TCP_CLOSE;
}

const char *AsyncClient::stateToString() {
  if (zamkniete_) return "Closed";
  return "Established";
}

void AsyncClient::wyslijBufor() {
  if (!doWyslania_.empty()) {
    wyslane_ += doWyslania_;
    doWyslania_.clear();
  }
}

void AsyncClient::rozlacz() {
  zamkniete_ = true;
  stan_ = TCP_CLOSE;
  wyslijBufor();
  if (onDisconnect_ != nullptr) onDisconnect_(onDisconnectArg_, this);
}

void AsyncClient::podajDane(const char *dane, std::size_t dlugosc) {
  if (onData_ != nullptr && dane != nullptr && dlugosc != 0) {
    // Przerwanie lwIP przekazuje bufor niezapisywalny; biblioteka I tak go
    // tylko czyta. Kopia, zeby emulator mial wlasny bufor.
    std::vector<char> bufor(dane, dane + dlugosc);
    onData_(onDataArg_, this, bufor.data(), bufor.size());
  }
  if (onPoll_ != nullptr) onPoll_(onPollArg_, this);
}

void AsyncClient::potwierdz(std::size_t ile) {
  zajete_ = 0;  // emulator: potwierdzenie zawsze oproznia caly bufor
  if (onAck_ != nullptr) onAck_(onAckArg_, this, ile, 0);
}
