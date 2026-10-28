// [ETAP 221] Prawdziwy serwer HTTP biblioteki na komputerze.
//
// CO TU JEST PRAWDZIWE (skompilowane z zrodel biblioteki, nie z modelu):
//   * parser zadania HTTP (WebRequest.cpp: _onData, _parseReqHead, _parseLine),
//   * wybor handlera (WebServer.cpp: _attachHandler + AsyncURIMatcher::matches),
//   * budowa odpowiedzi i wysylka (WebResponses.cpp: _respond, _ack),
//   * nasz kod z v3.56.4 (wyciagniety skryptem zrob_ekstrakcje.py):
//     bramka historii, trasy /history, /history/long, /history/link,
//     HistoriaPobranie.h, HistoriaPin.h, HistoriaBilety.h, CheckedHistoryIO.h.
//
// CO JEST NASZE (emulacja, opisana w README):
//   * gniazdo sieciowe (platforma/AsyncTCP.*) - bufor w pamieci zamiast lwIP,
//   * dysk "flash" (platforma/FS.cpp + KorzenCalls) - katalog zamiast LittleFS,
//   * zegar i esp_random - powtarzalne.
//
// Test podaje SUROWE BAJTY zadania HTTP, tak jak przyszlyby z sieci, i czyta
// SUROWE BAJTY odpowiedzi, tak jak wyszlyby do telefonu.
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "FreeRTOS.h"
#include "HistoriaBilety.h"
#include "HistoriaPobranie.h"
#include "HistoriaPin.h"

// ===========================================================================
// Stan, ktory na urzadzeniu nalezy do reszty firmware. Tu sa to zwykle zmienne.
// ===========================================================================
bool littlefsReady = true;
size_t g_histShortSize = 0;
size_t g_histLongSize = 0;
static bool g_magazynGotowy = true;
static bool historiaDostepGotowy() { return g_magazynGotowy; }
SemaphoreHandle_t logMutex = xSemaphoreCreateMutex();

// Zegar testu: pozwala przesunac czas, zeby sprawdzic wygasanie biletu.
void etap221PrzesunZegar(unsigned long ms);

// Sciezki historii na urzadzeniu zaczynaja sie od /littlefs/... . Tutaj
// "dysk" to katalog z HIST_ROOT, wiec tylko przepisujemy prefiks. Reszta
// (open/stat/read/seek/close) to prawdziwe POSIX-owe wywolania.

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

static const char *korzenDysku() {
  const char *r = std::getenv("HIST_ROOT");
  return (r == nullptr) ? "./dysk" : r;
}
KorzenCalls historiaSys(korzenDysku());

// Token z secrets.h - na urzadzeniu stala kompilacyjna. Tutaj zmienna
// srodowiskowa, zeby ten sam program sprawdzil oba profile (z tokenem i bez).
// Ustawiane przy starcie programu (przed konstrukcja historiaPobranie), bo
// Pobranie zapamietuje WSKAZNIK na token, nie jego wartosc.
static const char *HISTORIA_TOKEN = std::getenv("ETAP221_TOKEN") ? std::getenv("ETAP221_TOKEN") : "";

// WYCIAGNIETE Z v3.56.4 - patrz zrob_ekstrakcje.py (SHA w MANIFEST.json).
#include "nasz/bramka221.inc"

AsyncWebServer wifiWebServer(80);

static void zarejestrujTrasy() {
#include "nasz/trasy221.inc"
}

// ===========================================================================
// Narzedzia testu
// ===========================================================================
static int g_kontrol = 0;
static int g_bledow = 0;
static std::string g_ostatnia;

static void wynik(const char *nazwa, bool ok, const std::string &szczegol) {
  ++g_kontrol;
  if (!ok) ++g_bledow;
  std::printf("KONTROLA %-28s %s %s\n", nazwa, ok ? "PASS" : "FAIL", szczegol.c_str());
}

struct Odpowiedz {
  int kod = 0;
  std::string status;
  std::string naglowki;
  std::string cialo;
  std::string surowe;

  std::string naglowek(const std::string &nazwa) const {
    const std::string calosc = "\r\n" + naglowki;
    const std::string szukaj = "\r\n" + nazwa + ":";
    const std::size_t i = calosc.find(szukaj);
    if (i == std::string::npos) return "";
    const std::size_t od = i + szukaj.size();
    const std::size_t koniec = calosc.find("\r\n", od);
    std::string w = calosc.substr(od, koniec == std::string::npos ? std::string::npos : koniec - od);
    while (!w.empty() && (w[0] == ' ' || w[0] == '\t')) w.erase(w.begin());
    while (!w.empty() && (w.back() == ' ' || w.back() == '\t')) w.pop_back();
    return w;
  }
};

static Odpowiedz rozbierz(const std::string &surowe) {
  Odpowiedz o;
  o.surowe = surowe;
  const std::size_t koniec = surowe.find("\r\n\r\n");
  const std::string head = (koniec == std::string::npos) ? surowe : surowe.substr(0, koniec);
  o.cialo = (koniec == std::string::npos) ? "" : surowe.substr(koniec + 4);
  const std::size_t przelamanie = head.find("\r\n");
  o.status = head.substr(0, przelamanie);
  if (std::getenv("ETAP221_DEBUG") != nullptr) {
    std::printf("DEBUG surowe[%zu]: ", surowe.size());
    for (unsigned char c : surowe.substr(0, 160)) std::printf("%c", (c >= 32 && c < 127) ? c : (c == '\r' ? '~' : c == '\n' ? '|' : '?'));
    std::printf("\nDEBUG status=[%s]\n", o.status.c_str());
  }
  o.naglowki = (przelamanie == std::string::npos) ? "" : head.substr(przelamanie + 2);
  if (o.status.rfind("HTTP/1.1 ", 0) == 0) o.kod = std::atoi(o.status.c_str() + 9);
  return o;
}

// Jedno zadanie przez prawdziwy serwer. "dopompowac" = symulowac potwierdzenia
// TCP, zeby biblioteka doslala resztę ciala (tak jak okno sieciowe).
static Odpowiedz zadanie(const std::string &bajty, bool dopompowac = true) {
  AsyncClient *klient = new AsyncClient();
  std::shared_ptr<AsyncWebServerRequest> req = AsyncWebServerRequest::create(&wifiWebServer, klient);
  klient->podajDane(bajty);
  if (dopompowac) {
    for (int i = 0; i < 100000; ++i) {
      const std::size_t przed = klient->odpowiedz().size();
      klient->potwierdz(1460);
      if (klient->odpowiedz().size() == przed) break;
    }
  }
  const std::string kopia = klient->odpowiedz();
  // Rozlaczenie jak na urzadzeniu: biblioteka kasuje zadanie, a razem z nim
  // obiekt odpowiedzi - dopiero wtedy zwalnia sie znacznik zajetosci.
  klient->rozlacz();
  return rozbierz(kopia);
}

static std::string zadanieGet(const std::string &sciezka, const std::string &naglowki = "") {
  return "GET " + sciezka + " HTTP/1.1\r\nHost: sterownik\r\n" + naglowki + "\r\n";
}
static std::string zadaniePost(const std::string &sciezka, const std::string &naglowki = "") {
  return "POST " + sciezka + " HTTP/1.1\r\nHost: sterownik\r\nContent-Length: 0\r\n" + naglowki + "\r\n";
}

static std::string wczytaj(const std::string &sciezka) {
  std::string w;
  std::FILE *f = std::fopen(sciezka.c_str(), "rb");
  if (f == nullptr) return w;
  char bufor[4096];
  std::size_t n;
  while ((n = std::fread(bufor, 1, sizeof(bufor), f)) > 0) w.append(bufor, n);
  std::fclose(f);
  return w;
}

static bool wygladaNaHex32(const std::string &s) {
  if (s.size() != 32) return false;
  for (char c : s) {
    const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!hex) return false;
  }
  return true;
}

// ===========================================================================
// Scenariusze
// ===========================================================================
int main() {
  const std::string korzen = korzenDysku();

  const std::string krotki = wczytaj(korzen + "/littlefs/hist_short.csv");
  const std::string dlugi = wczytaj(korzen + "/littlefs/hist_long.csv");
  g_histShortSize = krotki.size();
  g_histLongSize = dlugi.size();

  zarejestrujTrasy();
  wifiWebServer.begin();

  wynik("S00_start", !krotki.empty() && !dlugi.empty() && wifiWebServer.state() == TCP_LISTEN,
        "krotki=" + std::to_string(krotki.size()) + "B dlugi=" + std::to_string(dlugi.size()) + "B nasluch=1");

  const bool zTokenem = (HISTORIA_TOKEN[0] != '\0');
  const std::string bearer = std::string("Authorization: Bearer ") + HISTORIA_TOKEN + "\r\n";

  if (zTokenem) {
    // --- A: trasy i przeladowanie (blokada z etapu 218) ------------------
    Odpowiedz a1 = zadanie(zadanieGet("/history/long", bearer));
    wynik("A01_dluga_200", a1.kod == 200 && a1.cialo == dlugi,
          "kod=" + std::to_string(a1.kod) + " cialo=" + std::to_string(a1.cialo.size()) + "/" + std::to_string(dlugi.size()));

    Odpowiedz a2 = zadanie(zadanieGet("/history", bearer));
    wynik("A02_krotka_200", a2.kod == 200 && a2.cialo == krotki,
          "kod=" + std::to_string(a2.kod) + " cialo=" + std::to_string(a2.cialo.size()) + "/" + std::to_string(krotki.size()));

    wynik("A03_naglowki_csv",
          a2.naglowek("Content-Type") == "text/csv" &&
              a2.naglowek("Content-Disposition") == "attachment; filename=\"historia-krotka.csv\"" &&
              a2.naglowek("Content-Length") == std::to_string(krotki.size()),
          "typ=" + a2.naglowek("Content-Type") + " disp=" + a2.naglowek("Content-Disposition") +
              " len=" + a2.naglowek("Content-Length"));

    wynik("A04_dluga_nazwa_pliku",
          a1.naglowek("Content-Disposition") == "attachment; filename=\"historia-dluga.csv\"",
          "disp=" + a1.naglowek("Content-Disposition"));

    // --- B: token ---------------------------------------------------------
    Odpowiedz b1 = zadanie(zadanieGet("/history"));
    wynik("B01_bez_tokenu_401", b1.kod == 401 && b1.naglowek("WWW-Authenticate") == "Bearer",
          "kod=" + std::to_string(b1.kod) + " auth=" + b1.naglowek("WWW-Authenticate"));

    Odpowiedz b2 = zadanie(zadanieGet("/history", "Authorization: Bearer zle-haslo\r\n"));
    wynik("B02_zly_token_401", b2.kod == 401, "kod=" + std::to_string(b2.kod));

    Odpowiedz b3 = zadanie(zadanieGet("/history", std::string("authorization: bearer ") + HISTORIA_TOKEN + "\r\n"));
    wynik("B03_male_litery_ok", b3.kod == 200 && b3.cialo == krotki, "kod=" + std::to_string(b3.kod));

    Odpowiedz b4 = zadanie(zadaniePost("/history", bearer));
    wynik("B04_post_405", b4.kod == 405 && b4.naglowek("Allow") == "GET",
          "kod=" + std::to_string(b4.kod) + " allow=" + b4.naglowek("Allow"));

    // --- C: bilet z panelu ------------------------------------------------
    Odpowiedz c1 = zadanie(zadanieGet("/history/link"));
    const std::string bilet = c1.cialo;
    wynik("C01_link_200_hex32", c1.kod == 200 && wygladaNaHex32(bilet),
          "kod=" + std::to_string(c1.kod) + " bilet=" + (bilet.size() > 8 ? bilet.substr(0, 8) + "..." : bilet));

    wynik("C02_link_no_store", c1.naglowek("Cache-Control") == "no-store",
          "cache=" + c1.naglowek("Cache-Control"));

    Odpowiedz c3 = zadanie(zadanieGet("/history?k=" + bilet));
    wynik("C03_bilet_dziala", c3.kod == 200 && c3.cialo == krotki,
          "kod=" + std::to_string(c3.kod) + " cialo=" + std::to_string(c3.cialo.size()));

    Odpowiedz c4 = zadanie(zadanieGet("/history?k=" + bilet));
    wynik("C04_bilet_jednorazowy", c4.kod == 401, "kod=" + std::to_string(c4.kod));

    Odpowiedz c5 = zadanie(zadanieGet("/history/link?stream=long"));
    Odpowiedz c6 = zadanie(zadanieGet("/history?k=" + c5.cialo));
    wynik("C05_bilet_dlugi_na_krotka_401", c6.kod == 401, "kod=" + std::to_string(c6.kod));

    Odpowiedz c7 = zadanie(zadanieGet("/history/long?k=" + c5.cialo));
    wynik("C06_bilet_dlugi_na_dluga_200", c7.kod == 200 && c7.cialo == dlugi,
          "kod=" + std::to_string(c7.kod) + " cialo=" + std::to_string(c7.cialo.size()));

    // bilet wygasa po 5 minutach
    Odpowiedz c8 = zadanie(zadanieGet("/history/link"));
    etap221PrzesunZegar(5UL * 60UL * 1000UL + 1000UL);
    Odpowiedz c9 = zadanie(zadanieGet("/history?k=" + c8.cialo));
    wynik("C07_bilet_wygasl_401", c9.kod == 401, "kod=" + std::to_string(c9.kod));

    // --- D: zajetosc i stan magazynu --------------------------------------
    // Pierwsze pobranie zatrzymujemy w polowie (bez potwierdzen TCP), drugie
    // w tym czasie musi dostac 503, a nie drugi strumien z tego samego pliku.
    {
      AsyncClient *k1 = new AsyncClient();
      std::shared_ptr<AsyncWebServerRequest> r1 = AsyncWebServerRequest::create(&wifiWebServer, k1);
      k1->podajDane(zadanieGet("/history", bearer));
      const bool zajeta = historiaPobranie.zajeta();
      Odpowiedz d1 = zadanie(zadanieGet("/history", bearer));
      wynik("D01_drugie_pobranie_503", zajeta && d1.kod == 503,
            "zajeta=" + std::string(zajeta ? "1" : "0") + " kod=" + std::to_string(d1.kod));
      // dokonczmy pierwsze, zeby zwolnic znacznik
      for (int i = 0; i < 100000; ++i) {
        const std::size_t przed = k1->odpowiedz().size();
        k1->potwierdz(1460);
        if (k1->odpowiedz().size() == przed) break;
      }
      wynik("D02_pierwsze_dokonczone", k1->odpowiedz().find("\r\n\r\n") != std::string::npos &&
                                        k1->odpowiedz().size() >= krotki.size(),
            "bajtow=" + std::to_string(k1->odpowiedz().size()));
      k1->rozlacz();
      r1.reset();  // destruktor zadania kasuje klienta - tak jak na urzadzeniu
    }

    Odpowiedz d3 = zadanie(zadanieGet("/history", bearer));
    wynik("D03_po_zwolnieniu_200", d3.kod == 200 && d3.cialo == krotki, "kod=" + std::to_string(d3.kod));

    g_magazynGotowy = false;
    Odpowiedz d4 = zadanie(zadanieGet("/history", bearer));
    wynik("D04_magazyn_niegotowy_503", d4.kod == 503, "kod=" + std::to_string(d4.kod));
    g_magazynGotowy = true;

    // --- E: rozmiar potwierdzony ------------------------------------------
    g_histShortSize = krotki.size() / 2;
    Odpowiedz e1 = zadanie(zadanieGet("/history", bearer));
    wynik("E01_tylko_potwierdzony_prefiks",
          e1.kod == 200 && e1.cialo == krotki.substr(0, krotki.size() / 2),
          "kod=" + std::to_string(e1.kod) + " cialo=" + std::to_string(e1.cialo.size()) +
              "/" + std::to_string(krotki.size() / 2));
    g_histShortSize = krotki.size();

    // Potwierdzony rozmiar wiekszy niz plik: zapinka bierze MNIEJSZA z dwoch
    // wartosci, wiec wysyla tylko to, co fizycznie istnieje (nie ufa licznikowi).
    g_histShortSize = 60ull * 1024ull * 1024ull;
    Odpowiedz e2 = zadanie(zadanieGet("/history", bearer));
    wynik("E02_potwierdzony_wiekszy_niz_plik", e2.kod == 200 && e2.cialo == krotki,
          "kod=" + std::to_string(e2.kod) + " cialo=" + std::to_string(e2.cialo.size()));
    g_histShortSize = krotki.size();

    // Plik fizycznie wiekszy niz limit dokumentu: musi byc 413, a nie ucieta
    // "udana" odpowiedz.
    const std::string sciezkaKrotka = korzen + "/littlefs/hist_short.csv";
    if (truncate(sciezkaKrotka.c_str(), 51ll * 1024ll * 1024ll) == 0) {
      g_histShortSize = 51ull * 1024ull * 1024ull;
      Odpowiedz e2b = zadanie(zadanieGet("/history", bearer));
      wynik("E03_plik_ponad_limit_413", e2b.kod == 413, "kod=" + std::to_string(e2b.kod));
    } else {
      wynik("E03_plik_ponad_limit_413", false, "nie udalo sie powiekszyc pliku testowego");
    }
    // przywroc plik i licznik
    {
      std::FILE *f = std::fopen(sciezkaKrotka.c_str(), "wb");
      if (f != nullptr) {
        std::fwrite(krotki.data(), 1, krotki.size(), f);
        std::fclose(f);
      }
    }
    g_histShortSize = krotki.size();

    g_histShortSize = 0;
    Odpowiedz e3 = zadanie(zadanieGet("/history", bearer));
    wynik("E04_brak_potwierdzonych_404", e3.kod == 404, "kod=" + std::to_string(e3.kod));
    g_histShortSize = krotki.size();
  } else {
    // --- profil bez tokenu: pobieranie zamkniete --------------------------
    Odpowiedz f1 = zadanie(zadanieGet("/history", "Authorization: Bearer cokolwiek\r\n"));
    wynik("F01_bez_tokenu_503", f1.kod == 503, "kod=" + std::to_string(f1.kod));

    Odpowiedz f2 = zadanie(zadanieGet("/history/long"));
    wynik("F02_dluga_503", f2.kod == 503, "kod=" + std::to_string(f2.kod));

    Odpowiedz f3 = zadanie(zadanieGet("/history/link"));
    wynik("F03_link_503", f3.kod == 503, "kod=" + std::to_string(f3.kod));

    // trasy nadal istnieja: 503, a nie 404 z catchAll
    wynik("F04_trasa_istnieje", f1.surowe.find("HTTP/1.1 404") == std::string::npos,
          "kod=" + std::to_string(f1.kod));
  }

  std::printf("PODSUMOWANIE kontrola=%d bledow=%d\n", g_kontrol, g_bledow);
  return g_bledow == 0 ? 0 : 1;
}
