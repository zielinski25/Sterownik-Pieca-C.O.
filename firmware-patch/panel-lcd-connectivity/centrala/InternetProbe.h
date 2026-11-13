#pragma once
// Nieblokujaca proba WAN: DNS A example.com do dwoch publicznych resolverow.
// Wynik informuje o mozliwosci wykonania TEJ proby, nie gwarantuje dostepu
// do kazdej strony/serwisu. Zadnych danych kotla ani hasel nie wysylamy.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

namespace InternetProbe {
static const uint32_t INTERWAL_MS = 15000;
static const uint32_t TIMEOUT_MS = 2000;
static const uint32_t MAX_WIEK_MS = 45000;
// Status w API: utrata STA => brak WAN; nieaktualny pomiar => null.
inline int widok(bool connected, uint32_t now, int stan, uint32_t czas) {
  if (!connected) return 0;
  if (stan < 0 || uint32_t(now - czas) > MAX_WIEK_MS) return -1;
  return stan;
}
// Zakodowane pytanie bez identyfikatora: RD=1, jedna kwestia A/IN.
static const uint8_t QUERY[] = {0,0,1,0,0,1,0,0,0,0,0,0,
  7,'e','x','a','m','p','l','e',3,'c','o','m',0,0,1,0,1};

inline bool poprawnaOdpowiedz(const uint8_t* p, size_t n, uint16_t id) {
  if (n < sizeof(QUERY) + 16 || p[0] != (id >> 8) || p[1] != (id & 255)) return false;
  // Odpowiedz, zwykle QUERY, bez truncation, NOERROR, jeden question, >=1 answer.
  if ((p[2] & 0xFA) != 0x80 || (p[3] & 0x0F) != 0 ||
      p[4] != 0 || p[5] != 1 || (p[6] == 0 && p[7] == 0)) return false;
  if (memcmp(p + 12, QUERY + 12, sizeof(QUERY) - 12) != 0) return false;
  // Pierwsza odpowiedz A/IN, pelne RDATA IPv4. Nazwa: pointer albo etykiety.
  size_t k = sizeof(QUERY);
  if ((p[k] & 0xC0) == 0xC0) {
    if (k + 2 > n) return false;
    k += 2;
  } else {
    while (k < n && p[k] != 0) {
      uint8_t len = p[k++];
      if (len > 63 || k + len >= n) return false;
      k += len;
    }
    if (k >= n) return false;
    ++k;
  }
  return k + 14 <= n && p[k] == 0 && p[k+1] == 1 &&
         p[k+2] == 0 && p[k+3] == 1 && p[k+8] == 0 && p[k+9] == 4;
}

class Udp {
  int fd_ = -1;
  uint32_t peer_ = 0;
public:
  ~Udp() { stop(); }
  bool start(uint32_t ipSta, int serwer, uint16_t id) {
    stop();
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) return false;
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) { stop(); return false; }
    sockaddr_in local{}; local.sin_family = AF_INET; local.sin_addr.s_addr = ipSta;
    if (bind(fd_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) { stop(); return false; }
    // Ustawienie zrodlowego adresu interfejsu STA zapobiega trasowaniu przez SoftAP.
    peer_ = htonl(serwer == 0 ? 0x01010101UL : 0x08080808UL);
    sockaddr_in dst{}; dst.sin_family = AF_INET; dst.sin_port = htons(53); dst.sin_addr.s_addr = peer_;
    uint8_t q[sizeof(QUERY)]; memcpy(q, QUERY, sizeof(q)); q[0] = id >> 8; q[1] = id & 255;
    if (sendto(fd_, q, sizeof(q), 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) != (int)sizeof(q)) {
      stop(); return false;
    }
    return true;
  }
  bool odebrano(uint16_t id) {
    if (fd_ < 0) return false;
    uint8_t buf[512]; sockaddr_in src{}; socklen_t len = sizeof(src);
    int n = recvfrom(fd_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&src), &len);
    return n > 0 && src.sin_family == AF_INET && src.sin_port == htons(53) &&
           src.sin_addr.s_addr == peer_ && poprawnaOdpowiedz(buf, (size_t)n, id);
  }
  void stop() { if (fd_ >= 0) { close(fd_); fd_ = -1; } }
};

// Transport podmienialny w testach; wolany wylacznie z jednej petli.
// publish(stan, czas) zapisuje -1=nieznany, 0=oba timeouty, 1=odpowiedz.
template<typename Transport, typename Publish>
class Monitor {
  Transport& udp_; Publish publish_;
  bool connected_ = false, waiting_ = false, scheduled_ = false;
  uint32_t started_ = 0, completed_ = 0, ip_ = 0;
  int server_ = 0; uint16_t id_ = 0;
  void begin(uint32_t now, uint32_t ip, uint16_t nonce) {
    id_ = nonce;
    if (!udp_.start(ip, server_, id_)) {
      // Blad lokalnego zasobu/gniazda nie dowodzi braku internetu.
      waiting_ = false; scheduled_ = true; completed_ = now; publish_(-1, now); return;
    }
    started_ = now; waiting_ = true;
  }
public:
  Monitor(Transport& udp, Publish publish) : udp_(udp), publish_(publish) {}
  void tick(uint32_t now, bool connected, uint32_t ip, uint16_t nonce) {
    if (!connected || ip == 0) {
      if (connected_ || waiting_) { udp_.stop(); publish_(-1, now); }
      connected_ = false; waiting_ = false; scheduled_ = false; return;
    }
    if (!connected_ || ip_ != ip) {
      udp_.stop(); connected_ = true; waiting_ = false; scheduled_ = false; ip_ = ip;
      publish_(-1, now); // poprzednia siec i poprzedni wynik nie sa aktualne
    }
    if (!waiting_) {
      if (!scheduled_ || uint32_t(now - completed_) >= INTERWAL_MS) {
        server_ = 0; begin(now, ip, nonce);
      }
      return;
    }
    if (udp_.odebrano(id_)) {
      udp_.stop(); waiting_ = false; scheduled_ = true; completed_ = now; publish_(1, now); return;
    }
    if (uint32_t(now - started_) >= TIMEOUT_MS) {
      udp_.stop();
      if (server_ == 0) { server_ = 1; begin(now, ip, nonce); }
      else { waiting_ = false; scheduled_ = true; completed_ = now; publish_(0, now); }
    }
  }
};
} // namespace InternetProbe
