#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "HistoriaPin.h"
#include "HistoriaBilety.h"

// [v3.56.0 HISTORIA-POBRANIE] Trasa HTTP pobrania historii dla panelu i aplikacji.
//
// Wpięcie w centralę: main_centrala.cpp includuje ten plik PO <ESPAsyncWebServer.h>
// (dlatego tutaj nie ma zadnego include'a serwera — tak samo jak przy
// SnapshotResponse.h z etapu 208). Bramka (Bramka) jest implementowana przy
// rejestracji trasy, bo tylko tam widac logMutex, historiaSys i cache rozmiarow.
//
// Kolejnosc decyzji jest ta sama co w etapie 209 (metoda -> poswiadczenia ->
// zajetosc -> magazyn -> odpowiedz). Kazda odmowa jest PRAWDZIWA odpowiedzia
// HTTP; zadna nie otwiera zapinki i nie dotyka pliku historii.

namespace historia212 {

enum class Strumien : std::uint8_t { Krotki, Dluga };

enum class Kod : std::uint8_t {
  Ok,
  ZlaMetoda,     // 405
  BrakTokenu,    // 401
  Nieskonfigurowane,  // 503 - token w secrets.h pusty, pobieranie wylaczone
  Zajeta,        // 503 - trwa inne pobranie
  Niegotowe,     // 503 - magazyn historii zatrzymany albo brak LittleFS
  BrakPliku,     // 404 - pliku nie ma albo nie ma potwierdzonego prefiksu
  ZaDuzo,        // 413 - prefiks wiekszy niz limit dokumentu
  BladVfs,       // 500 - VFS zawiodl przy otwieraniu
};

// Porownanie tokenu o stalym czasie: praca zalezy od DLUGOSCI podanego tokenu,
// nigdy od tego, na ktorym znaku sie roznia. Pusty token oczekiwany = zawsze false.
inline bool tokenZgodny(const char *podany, std::size_t n, const char *oczekiwany) {
  if (oczekiwany == nullptr || *oczekiwany == '\0') return false;
  if (podany == nullptr) return false;
  const std::size_t m = std::strlen(oczekiwany);
  unsigned char roznica = static_cast<unsigned char>(n != m ? 1 : 0);
  const std::size_t zasieg = (n > m) ? n : m;
  for (std::size_t i = 0; i < zasieg; ++i) {
    const unsigned char a = (i < n) ? static_cast<unsigned char>(podany[i]) : 0;
    const unsigned char b = (i < m) ? static_cast<unsigned char>(oczekiwany[i]) : 0;
    roznica = static_cast<unsigned char>(roznica | (a ^ b));
  }
  return roznica == 0;
}

// "Bearer <token>" - schemat bez wzgledu na wielkosc liter, token bez spacji
// w srodku. Zwraca false dla: braku naglowka, innego schematu, pustego tokenu,
// spacji w srodku tokenu.
inline bool wyciagnijToken(const char *naglowek, std::size_t dlugosc,
                           const char *&token, std::size_t &tokenDlugosc) {
  token = nullptr;
  tokenDlugosc = 0;
  if (naglowek == nullptr) return false;
  // porownujemy z malymi literami: wejscie jest sprowadzane do malych
  static const char schemat[] = "bearer ";
  const std::size_t schematDlugosc = sizeof(schemat) - 1;
  if (dlugosc <= schematDlugosc) return false;
  for (std::size_t i = 0; i < schematDlugosc; ++i) {
    char a = naglowek[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (a != schemat[i]) return false;
  }
  std::size_t od = schematDlugosc;
  std::size_t doK = dlugosc;
  while (od < doK && (naglowek[od] == ' ' || naglowek[od] == '\t')) ++od;
  while (doK > od && (naglowek[doK - 1] == ' ' || naglowek[doK - 1] == '\t')) --doK;
  if (doK == od) return false;
  for (std::size_t i = od; i < doK; ++i) {
    if (naglowek[i] == ' ' || naglowek[i] == '\t') return false;
  }
  token = naglowek + od;
  tokenDlugosc = doK - od;
  return true;
}

// Bramka: jedyny sposob, w jaki trasa siega do stanu pisarza historii.
// Implementacja w main_centrala.cpp bierze logMutex, wola pin.open() i oddaje
// blokade NATYCHMIAST - nigdy na czas wysylki.
class Bramka {
 public:
  virtual ~Bramka() {}
  virtual history193::Calls &io() = 0;
  virtual bool magazynGotowy() = 0;               // historiaDostepGotowy() && littlefsReady
  virtual std::uint64_t potwierdzony(Strumien s) = 0;  // g_histShortSize / g_histLongSize
  virtual const char *sciezka(Strumien s) = 0;    // kShortVfsPath / kLongVfsPath
  virtual bool zamknij() = 0;                     // wez logMutex; false = zajety
  virtual void otworz() = 0;                      // oddaj logMutex
};

// Odpowiedz o STALEJ dlugosci: kazdy bajt pochodzi z zapinki, nigdy z pliku
// "na teraz". Destruktor zwalnia zapinke i znacznik zajetosci - biblioteka
// usuwa obiekt odpowiedzi po zakonczeniu wysylki, takze po zerwaniu.
//
// UWAGA (wazne): zapinka powstaje NA KAZDE zadanie i jest ODDANA odpowiedzi.
// Nie uzywamy jednego obiektu Pin do dwoch pobran: Pin z etapu 211 nie zeruje
// swojego stanu przy ponownym open() (licznik wydanych bajtow i znacznik
// uszkodzenia zostaja), wiec jeden obiekt = jedno pobranie.
class Odpowiedz final : public AsyncAbstractResponse {
 public:
  Odpowiedz(historia211::Pin *pin, bool &zajeta, const char *typ)
      : AsyncAbstractResponse(nullptr), pin_(pin), zajeta_(zajeta) {
    _code = 200;
    _contentType = typ;
    _contentLength = static_cast<size_t>(pin ? pin->length() : 0);
    _sendContentLength = true;
    _chunked = false;
  }
  ~Odpowiedz() override {
    if (pin_ != nullptr) {
      pin_->close();
      delete pin_;
      pin_ = nullptr;
    }
    zajeta_ = false;
  }
  bool _sourceValid() const override { return !awaria_ && pin_ != nullptr && pin_->active(); }
  size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
    if (pin_ == nullptr) return 0;
    const historia211::ReadResult r = pin_->read(buf, maxLen);
    if (r.code == historia211::ReadCode::Data) return r.bytes;
    if (r.code != historia211::ReadCode::End) awaria_ = true;  // Short/Fault
    return 0;
  }
  bool awaria() const { return awaria_; }

 private:
  historia211::Pin *pin_;
  bool &zajeta_;
  bool awaria_ = false;
};

class Pobranie {
 public:
  Pobranie(Bramka &bramka, const char *token) : bramka_(bramka), token_(token) {}

  // [v3.56.1 HISTORIA-BILETY] Trasa moze przyjmowac bilety z panelu.
  // nullptr = tylko naglowek Bearer (tak dzialal etap 212).
  void ustawBilety(historia213::Bilety *bilety) { bilety_ = bilety; }

  // Jedno miejsce decydujace, czy zadanie staje sie pobraniem.
  void obsluz(AsyncWebServerRequest *request, Strumien strumien) {
    if (request == nullptr) return;
    if (request->method() != HTTP_GET) {
      odpowiedzBledu(request, 405, "Metoda niedozwolona", "Allow", "GET");
      return;
    }
    if (token_ == nullptr || *token_ == '\0') {
      odpowiedzBledu(request, 503, "Pobieranie historii wylaczone (brak tokenu w secrets.h)", nullptr, nullptr);
      return;
    }
    // [v3.56.1 HISTORIA-BILETY] Dwie drogi: naglowek Bearer (jak w 212) albo
    // bilet z panelu (?k=...). Bilet sprawdzamy DOPIERO po zajetosci i magazynie,
    // zeby nie spalic linku, gdy centrala jest chwilowo zajeta.
    const AsyncWebHeader *naglowek = request->getHeader("Authorization");
    const char *podany = nullptr;
    std::size_t podanaDlugosc = 0;
    bool przezBilet = false;
    if (naglowek != nullptr) {
      if (!wyciagnijToken(naglowek->value().c_str(), naglowek->value().length(), podany, podanaDlugosc) ||
          !tokenZgodny(podany, podanaDlugosc, token_)) {
        odpowiedzBledu(request, 401, "Brak albo bledny token", "WWW-Authenticate", "Bearer");
        return;
      }
    } else if (bilety_ != nullptr && request->hasParam("k")) {
      przezBilet = true;
    } else {
      odpowiedzBledu(request, 401, "Brak poswiadczen", "WWW-Authenticate", "Bearer");
      return;
    }
    if (zajeta_) {
      odpowiedzBledu(request, 503, "Trwa inne pobranie", nullptr, nullptr);
      return;
    }
    if (!bramka_.magazynGotowy()) {
      odpowiedzBledu(request, 503, "Magazyn historii niegotowy", nullptr, nullptr);
      return;
    }

    if (przezBilet) {
      const String bilet = request->getParam("k")->value();
      if (!bilety_->zuzyj(bilet.c_str(), bilet.length(),
                          strumien == Strumien::Dluga ? historia213::kDluga : historia213::kKrotki)) {
        odpowiedzBledu(request, 401, "Link wygasl albo zostal juz uzyty - kliknij pobieranie jeszcze raz",
                       "WWW-Authenticate", "Bearer");
        return;
      }
    }
    if (!bramka_.zamknij()) {
      odpowiedzBledu(request, 503, "Zapis historii chwilowo zajety", nullptr, nullptr);
      return;
    }
    historia211::Pin *pin = new historia211::Pin();
    historia211::OpenCode otwarcie = historia211::OpenCode::Fault;
    if (pin != nullptr) {
      otwarcie = pin->open(bramka_.io(), bramka_.sciezka(strumien), bramka_.potwierdzony(strumien),
                           historia211::kMaxDocumentBytes);
    }
    bramka_.otworz();

    switch (otwarcie) {
      case historia211::OpenCode::Ok:
        break;
      case historia211::OpenCode::Missing:
      case historia211::OpenCode::NothingCommitted:
        delete pin;
        odpowiedzBledu(request, 404, "Brak historii do pobrania", nullptr, nullptr);
        return;
      case historia211::OpenCode::TooLarge:
        delete pin;
        odpowiedzBledu(request, 413, "Historia wieksza niz limit", nullptr, nullptr);
        return;
      case historia211::OpenCode::Fault:
      default:
        delete pin;
        odpowiedzBledu(request, 500, "Blad dostepu do pliku historii", nullptr, nullptr);
        return;
    }

    Odpowiedz *odp = new Odpowiedz(pin, zajeta_, "text/csv");
    if (odp == nullptr) {  // brak pamieci na obiekt odpowiedzi
      pin->close();
      delete pin;
      odpowiedzBledu(request, 503, "Brak pamieci na odpowiedz", nullptr, nullptr);
      return;
    }
    // [v3.56.3 HISTORIA-PLIK] Nazwa pliku dla przegladarki i telefonu.
    // Bez "attachment" Chrome i Safari pokazuja CSV jako tekst na
    // stronie (albo zapisuja plik bez rozszerzenia) - pobieranie konczy
    // sie widokiem na ekranie zamiast plikiem w "Pobranych".
    odp->addHeader("Content-Disposition",
                   strumien == Strumien::Dluga
                       ? "attachment; filename=\"historia-dluga.csv\""
                       : "attachment; filename=\"historia-krotka.csv\"");
    zajeta_ = true;
    request->send(odp);
  }

  // [v3.56.1 HISTORIA-BILETY] GET /history/link?stream=short|long -> 32 znaki
  // hex. Bilet jest jednorazowy i wygasa po 5 minutach. Staly token z secrets.h
  // NIGDY nie trafia do przegladarki.
  void link(AsyncWebServerRequest *request) {
    if (request == nullptr) return;
    if (request->method() != HTTP_GET) {
      odpowiedzBledu(request, 405, "Metoda niedozwolona", "Allow", "GET");
      return;
    }
    if (token_ == nullptr || *token_ == '\0') {
      odpowiedzBledu(request, 503, "Pobieranie historii wylaczone (brak tokenu w secrets.h)", nullptr, nullptr);
      return;
    }
    if (bilety_ == nullptr) {
      odpowiedzBledu(request, 503, "Bilety pobrania nie sa podpiete", nullptr, nullptr);
      return;
    }
    const bool dluga = request->hasParam("stream") && request->getParam("stream")->value() == "long";
    char bilet[historia213::kZnakow + 1];
    if (!bilety_->wydaj(dluga ? historia213::kDluga : historia213::kKrotki, bilet)) {
      odpowiedzBledu(request, 503, "Brak zrodla losowosci dla biletu", nullptr, nullptr);
      return;
    }
    AsyncWebServerResponse *r = request->beginResponse(200, "text/plain", String(bilet));
    if (r != nullptr) r->addHeader("Cache-Control", "no-store");
    request->send(r);
  }

  bool zajeta() const { return zajeta_; }

 private:
  static void odpowiedzBledu(AsyncWebServerRequest *request, int kod, const char *tekst,
                             const char *naglowek, const char *wartosc) {
    AsyncWebServerResponse *r = request->beginResponse(kod, "text/plain", String(tekst));
    if (r != nullptr && naglowek != nullptr && wartosc != nullptr) r->addHeader(naglowek, wartosc);
    request->send(r);
  }

  Bramka &bramka_;
  const char *token_;
  historia213::Bilety *bilety_ = nullptr;
  bool zajeta_ = false;
};

}  // namespace historia212
