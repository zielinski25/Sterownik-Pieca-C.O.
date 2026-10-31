// [ETAP 222] Mostek: prawdziwe gniazdo TCP <-> prawdziwy kod biblioteki.
//
// CO ROBI: nasluchuje na 127.0.0.1:<port>, przyjmuje polaczenie, czyta z niego
// bajty i podaje je do TEGO SAMEGO obiektu AsyncClient, ktorego uzywa
// biblioteka. Wszystko, co biblioteka (i nasze trasy) wypisze, jest odsylane
// do gniazda. Czyli: prawdziwy parser, prawdziwy dyspozytor, prawdziwe
// odpowiedzi - po prawdziwym TCP.
//
// CZEGO NIE MA: prawdziwego lwIP. Gniazdo jest systemowe (POSIX), a po stronie
// biblioteki siedzi emulator AsyncClient z etapu 221 (bufor w pamieci).
// Potwierdzenia TCP symulujemy: ile bajtow wyszlo do gniazda, tyle "ack".
// UWAGA: Arduino.h MUSI byc przed naglowkami systemowymi. Rdzen Arduino
// deklaruje wlasne INADDR_NONE, a <netinet/in.h> ma na to makro - odwrotna
// kolejnosc konczy sie bledem kompilacji.
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "FreeRTOS.h"
#include "HistoriaBilety.h"
#include "HistoriaPobranie.h"
#include "HistoriaPin.h"

// --- stan, ktory na urzadzeniu nalezy do reszty firmware (jak w etapie 221) ---
bool littlefsReady = true;
size_t g_histShortSize = 0;
size_t g_histLongSize = 0;
static bool g_magazynGotowy = true;
static bool historiaDostepGotowy() { return g_magazynGotowy; }
SemaphoreHandle_t logMutex = xSemaphoreCreateMutex();

class KorzenCalls final : public history193::Calls {
 public:
  explicit KorzenCalls(std::string korzen) : korzen_(std::move(korzen)) {}
  int open(const char *p, int f) override { return sys_.open(pelna(p).c_str(), f); }
  int stat(int fd, struct stat *s) override { return sys_.stat(fd, s); }
  off_t seek(int fd, off_t o) override { return sys_.seek(fd, o); }
  ssize_t read(int fd, void *b, std::size_t n) override { return sys_.read(fd, b, n); }
  ssize_t write(int fd, const void *b, std::size_t n) override { return sys_.write(fd, b, n); }
  int sync(int fd) override { return sys_.sync(fd); }
  int close(int fd) override { return sys_.close(fd); }
  int rename(const char *a, const char *b) override { return sys_.rename(pelna(a).c_str(), pelna(b).c_str()); }
  int error() const override { return sys_.error(); }

 private:
  std::string pelna(const char *p) const {
    if (p == nullptr) return korzen_;
    std::string w = korzen_;
    if (p[0] != '/') w += "/";
    w += p;
    return w;
  }
  history193::SystemCalls sys_;
  std::string korzen_;
};

static const char *HISTORIA_TOKEN = std::getenv("ETAP222_TOKEN") ? std::getenv("ETAP222_TOKEN") : "";
KorzenCalls historiaSys(std::getenv("HIST_ROOT") ? std::getenv("HIST_ROOT") : "./dysk");

#include "nasz/bramka221.inc"

AsyncWebServer wifiWebServer(80);

static void zarejestrujTrasy() {
#include "nasz/trasy221.inc"
}

// ---------------------------------------------------------------------------
// Mostek
// ---------------------------------------------------------------------------
struct Polaczenie {
  int fd = -1;
  AsyncClient *klient = nullptr;
  std::shared_ptr<AsyncWebServerRequest> zadanie;
  std::size_t wyslano = 0;
  bool zajete = false;
};

static std::size_t wyslijDoGniazda(Polaczenie &p) {
  const std::string &out = p.klient->odpowiedz();
  if (out.size() <= p.wyslano) return 0;
  const std::size_t ile = out.size() - p.wyslano;
  std::size_t zrobione = 0;
  while (zrobione < ile) {
    const ssize_t n = ::send(p.fd, out.data() + p.wyslano + zrobione, ile - zrobione, MSG_NOSIGNAL);
    if (n <= 0) break;
    zrobione += static_cast<std::size_t>(n);
  }
  p.wyslano += zrobione;
  return zrobione;
}

// Po kazdym wyslaniu do gniazda mowimy bibliotece, ze bajty doszly - dokladnie
// tak, jak zrobilby to lwIP, gdy bufor sie oprozni.
static void obsluzZapis(Polaczenie &p) {
  for (int i = 0; i < 10000; ++i) {
    const std::size_t n = wyslijDoGniazda(p);
    if (n == 0) break;
    if (p.klient->zamkniete()) continue;
    p.klient->potwierdz(n);
  }
}

static void zakoncz(Polaczenie &p) {
  if (p.klient != nullptr) {
    obsluzZapis(p);
    p.klient->rozlacz();
  }
  p.zadanie.reset();  // kasuje zadanie i klienta
  p.klient = nullptr;
  if (p.fd >= 0) {
    ::close(p.fd);
    p.fd = -1;
  }
}

// Rdzen Arduino-API deklaruje main() bez argumentow (weak) i to jest jedyna
// sygnatura, ktora mozna tu uzyc. Port bierzemy ze zmiennej srodowiskowej.
int main() {
  const char *portTekst = std::getenv("ETAP222_PORT");
  const int port = (portTekst != nullptr) ? std::atoi(portTekst) : 18080;

  zarejestrujTrasy();
  wifiWebServer.begin();

  // Rozmiary potwierdzone to po prostu rozmiary plikow na "dysku".
  {
    const std::string korzen = std::getenv("HIST_ROOT") ? std::getenv("HIST_ROOT") : "./dysk";
    std::FILE *f = std::fopen((korzen + "/littlefs/hist_short.csv").c_str(), "rb");
    if (f != nullptr) {
      std::fseek(f, 0, SEEK_END);
      g_histShortSize = static_cast<size_t>(std::ftell(f));
      std::fclose(f);
    }
    f = std::fopen((korzen + "/littlefs/hist_long.csv").c_str(), "rb");
    if (f != nullptr) {
      std::fseek(f, 0, SEEK_END);
      g_histLongSize = static_cast<size_t>(std::ftell(f));
      std::fclose(f);
    }
  }

  const int nasluch = ::socket(AF_INET, SOCK_STREAM, 0);
  if (nasluch < 0) {
    std::perror("socket");
    return 1;
  }
  const int tak = 1;
  ::setsockopt(nasluch, SOL_SOCKET, SO_REUSEADDR, &tak, sizeof(tak));
  sockaddr_in adres{};
  adres.sin_family = AF_INET;
  adres.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  adres.sin_port = htons(static_cast<uint16_t>(port));
  if (::bind(nasluch, reinterpret_cast<sockaddr *>(&adres), sizeof(adres)) != 0) {
    std::perror("bind");
    return 1;
  }
  if (::listen(nasluch, 8) != 0) {
    std::perror("listen");
    return 1;
  }
  std::printf("NASLUCH %d\n", port);
  std::fflush(stdout);

  std::vector<Polaczenie> polaczenia;
  for (;;) {
    std::vector<pollfd> fds;
    fds.push_back({nasluch, POLLIN, 0});
    for (auto &p : polaczenia) fds.push_back({p.fd, POLLIN, 0});
    const int gotowe = ::poll(fds.data(), fds.size(), 1000);
    if (gotowe < 0) break;

    if (fds[0].revents & POLLIN) {
      const int fd = ::accept(nasluch, nullptr, nullptr);
      if (fd >= 0) {
        Polaczenie p;
        p.fd = fd;
        p.klient = new AsyncClient();
        // Dokladnie to samo, co robi AsyncWebServer::AsyncWebServer() po
        // przyjeciu klienta: tworzy zadanie na tym kliencie.
        p.zadanie = AsyncWebServerRequest::create(&wifiWebServer, p.klient);
        polaczenia.push_back(std::move(p));
      }
      continue;
    }

    for (std::size_t i = 0; i < polaczenia.size();) {
      Polaczenie &p = polaczenia[i];
      const short rev = fds[i + 1].revents;
      bool usunac = false;
      if (rev & (POLLHUP | POLLERR)) {
        usunac = true;
      } else if (rev & POLLIN) {
        char bufor[4096];
        const ssize_t n = ::recv(p.fd, bufor, sizeof(bufor), 0);
        if (n <= 0) {
          usunac = true;
        } else {
          p.klient->podajDane(bufor, static_cast<std::size_t>(n));
          obsluzZapis(p);
        }
      }
      if (!usunac && p.klient != nullptr && p.klient->zamkniete()) {
        obsluzZapis(p);
        usunac = true;
      }
      if (usunac) {
        zakoncz(p);
        polaczenia.erase(polaczenia.begin() + static_cast<long>(i));
      } else {
        ++i;
      }
    }
  }
  for (auto &p : polaczenia) zakoncz(p);
  ::close(nasluch);
  return 0;
}
