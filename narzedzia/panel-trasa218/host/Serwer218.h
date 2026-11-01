#pragma once
// 218: model zadan, odpowiedzi HTTP I TABELI TRAS.
// Punkt wyjscia: host/Serwer213.h z etapu 213 (skopiowany bajt w bajt,
// dopisana tylko tabela tras na koncu - patrz znacznik [218] nizej). Modelowany jest WYLACZNIE interfejs
// AsyncWebServer, ktorego uzywa trasa (metoda, naglowki, beginResponse,
// send) oraz petla wypelniania bufora odpowiedzi. Sama trasa i bramka sa
// PRAWDZIWE - wyciagniete z main_centrala.cpp po patchu (Extracted212.inc).
//
// Model NIE jest biblioteka: nie ma gniazd, TLS, szablonow, chunked, Range.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Arduino.h"

enum WebRequestMethodComposite {
  HTTP_GET = 1,
  HTTP_POST = 2,
  HTTP_PUT = 4,
  HTTP_DELETE = 8,
  HTTP_ANY = 0xFF,
};

class AsyncWebHeader {
 public:
  AsyncWebHeader(const char *n, const char *v) : nazwa_(n), wartosc_(v) {}
  const String &name() const { return nazwa_; }
  const String &value() const { return wartosc_; }

 private:
  String nazwa_;
  String wartosc_;
};

class AsyncAbstractResponse;

class AsyncWebServerResponse {
 public:
  virtual ~AsyncWebServerResponse() {}
  // Tylko w modelu: odpowiedz strumieniowa albo nullptr. W prawdziwej
  // bibliotece robi sie to rzutowaniem; tu wystarczy, ze test nie potrzebuje RTTI.
  virtual AsyncAbstractResponse *jakoStrumien() { return nullptr; }
  void addHeader(const String &n, const String &v) { naglowki.emplace_back(n.std_str(), v.std_str()); }
  std::vector<std::pair<std::string, std::string>> naglowki;
  int _code = 200;
  String _contentType;
  std::size_t _contentLength = 0;
  bool _sendContentLength = true;
  bool _chunked = false;
};

using AwsTemplateProcessor = std::function<String(const String &)>;

class AsyncAbstractResponse : public AsyncWebServerResponse {
 public:
  explicit AsyncAbstractResponse(AwsTemplateProcessor = nullptr) {}
  AsyncAbstractResponse *jakoStrumien() override { return this; }
  virtual bool _sourceValid() const { return false; }
  virtual std::size_t _fillBuffer(uint8_t *, std::size_t) { return 0; }
};

// To, co biblioteka tworzy dla request->beginResponse(kod, typ, tresc).
class AsyncBasicResponse : public AsyncWebServerResponse {
 public:
  AsyncBasicResponse(int kod, const char *typ, const std::string &tresc) {
    _code = kod;
    _contentType = typ;
    cialo = tresc;
    _contentLength = tresc.size();
  }
  std::string cialo;
};

// [v3.56.1 HISTORIA-BILETY] Parametr zapytania (?k=..., &stream=...).
// W prawdziwej bibliotece to AsyncWebParameter z WebRequest.cpp.
class AsyncWebParameter {
 public:
  AsyncWebParameter(const char *n, const char *v) : nazwa_(n), wartosc_(v) {}
  const String &name() const { return nazwa_; }
  const String &value() const { return wartosc_; }

 private:
  String nazwa_;
  String wartosc_;
};

class AsyncWebServerRequest {
 public:
  int metoda = HTTP_GET;
  std::vector<AsyncWebHeader> naglowki;
  AsyncWebServerResponse *wyslana = nullptr;
  int kod = 0;
  bool wyslano = false;
  int beginKod = 0;
  std::string beginCialo;

  int method() const { return metoda; }

  // [v3.56.1 HISTORIA-BILETY] Parametry zapytania. Biblioteka trzyma je
  // w _params (GET) i _postParams; trasa uzywa tylko GET-owych.
  std::vector<AsyncWebParameter> parametry;

  bool hasParam(const char *n) const { return getParam(n) != nullptr; }

  const AsyncWebParameter *getParam(const char *n) const {
    for (const auto &p : parametry) {
      const std::string &a = p.name().std_str();
      if (a.size() != std::strlen(n)) continue;
      bool takie = true;
      for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = n[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
        if (x != y) {
          takie = false;
          break;
        }
      }
      if (takie) return &p;
    }
    return nullptr;
  }

  void dodajParametr(const char *n, const char *v) { parametry.emplace_back(n, v); }


  // Biblioteka porownuje nazwy naglowkow bez wzgledu na wielkosc liter
  // (AsyncWebHeader::name().equalsIgnoreCase w WebRequest.cpp, ref c47917d).
  const AsyncWebHeader *getHeader(const char *n) const {
    for (const auto &h : naglowki) {
      const std::string &a = h.name().std_str();
      const char *b = n;
      if (a.size() != std::strlen(b)) continue;
      bool takie = true;
      for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
        if (x != y) {
          takie = false;
          break;
        }
      }
      if (takie) return &h;
    }
    return nullptr;
  }

  void dodajNaglowek(const char *n, const char *v) { naglowki.emplace_back(n, v); }

  AsyncWebServerResponse *beginResponse(int kod_, const char *typ, const String &tresc) {
    beginKod = kod_;
    beginCialo = tresc.std_str();
    return new AsyncBasicResponse(kod_, typ, tresc.std_str());
  }

  void send(AsyncWebServerResponse *r) {
    wyslana = r;
    wyslano = true;
    kod = r ? r->_code : 0;
  }
};

// Petla, ktora biblioteka wypelnia bufor odpowiedzi (uproszczona: bez okna
// TCP i bez potwierdzen - to bylo przedmiotem etapow 199/208).
struct Wysylka {
  std::string cialo;
  std::size_t porcji = 0;
  bool zrodloByloWazneNaKoncu = false;
};

inline Wysylka wyslij(AsyncAbstractResponse *r, std::size_t bufor = 97) {
  Wysylka w;
  std::vector<uint8_t> b(bufor);
  for (;;) {
    const std::size_t n = r->_fillBuffer(b.data(), b.size());
    if (n == 0) break;
    w.cialo.append(reinterpret_cast<const char *>(b.data()), n);
    ++w.porcji;
    if (w.porcji > 1000000) break;  // zabezpieczenie przed petla w mutancie
  }
  w.zrodloByloWazneNaKoncu = r->_sourceValid();
  return w;
}

// ═══════════════════════════════════════════════════════════════════════════
// [218] TABELA TRAS - to, czego etap 213 NIE mial i dlatego przepuscil blad
// "przycisk nie dziala w ogole".
//
// Port zachowania z ESPAsyncWebServer (ref c47917d, plik WebServer.cpp):
//   * konstruktor AsyncURIMatcher(const char *uri) -> goly napis to
//     BackwardCompatible: pasuje `_value == path` ALBO
//     `path.startsWith(_value + "/")`,
//   * AsyncURIMatcher::exact(uri) -> pasuje wylacznie `_value == path`,
//   * _attachHandler() (WebServer.cpp:142) bierze PIERWSZY pasujacy handler
//     w kolejnosci rejestracji i sprawdza metody przez `_method & method`.
// Model NIE zna wyrazen regularnych ani parametrow sciezki - plik ich nie
// uzywa (sprawdza to build.py: zadnej rejestracji regex() nie ma).
// ═══════════════════════════════════════════════════════════════════════════

class AsyncURIMatcher {
 public:
  enum Typ { NONE, ALL, EXACT, PREFIX, BACKWARD };

  AsyncURIMatcher() : typ_(NONE) {}
  // Goly napis - dokladnie ta sama droga, ktora szedl firmware do v3.56.3.
  AsyncURIMatcher(const char *uri) : typ_(BACKWARD) { ustaw(String(uri ? uri : "")); }

  static AsyncURIMatcher exact(const char *uri) {
    AsyncURIMatcher m;
    m.typ_ = EXACT;
    m.ustaw(String(uri ? uri : ""));
    return m;
  }
  static AsyncURIMatcher prefix(const char *uri) {
    AsyncURIMatcher m;
    m.typ_ = PREFIX;
    m.ustaw(String(uri ? uri : ""));
    return m;
  }
  static AsyncURIMatcher all() {
    AsyncURIMatcher m;
    m.typ_ = ALL;
    return m;
  }

  // AsyncURIMatcher::matches(request) w bibliotece czyta request->url()
  // (bez zapytania). Tu podajemy sama sciezke - ten sam efekt.
  bool matches(const String &sciezka) const {
    const std::string &p = sciezka.std_str();
    switch (typ_) {
      case ALL:
        return true;
      case EXACT:
        return p == wartosc_;
      case PREFIX:
        return p.compare(0, wartosc_.size(), wartosc_) == 0;
      case BACKWARD:
        return p == wartosc_ || (p.size() > wartosc_.size() &&
                                 p.compare(0, wartosc_.size(), wartosc_) == 0 &&
                                 p[wartosc_.size()] == '/');
      case NONE:
      default:
        return false;
    }
  }

  Typ typ() const { return typ_; }
  const std::string &wartosc() const { return wartosc_; }
  const char *nazwaTypu() const {
    switch (typ_) {
      case ALL: return "all";
      case EXACT: return "exact";
      case PREFIX: return "prefix";
      case BACKWARD: return "backward(goly napis)";
      case NONE:
      default: return "none";
    }
  }

 private:
  void ustaw(const String &u) { wartosc_ = u.std_str(); }
  Typ typ_ = NONE;
  std::string wartosc_;
};

struct Trasa218 {
  AsyncURIMatcher matcher;
  int metoda = 0;
  std::function<void(AsyncWebServerRequest *)> handler;
};

class AsyncWebServer {
 public:
  Trasa218 &on(AsyncURIMatcher matcher, int metoda,
               std::function<void(AsyncWebServerRequest *)> handler) {
    Trasa218 t;
    t.matcher = std::move(matcher);
    t.metoda = metoda;
    t.handler = std::move(handler);
    trasy_.push_back(std::move(t));
    return trasy_.back();
  }

  void onNotFound(std::function<void(AsyncWebServerRequest *)> handler) {
    notFound_ = std::move(handler);
  }

  // _attachHandler(): filter() && canHandle(), pierwszy wygrywa.
  // canHandle() wymaga ustawionego handlera i zgodnosci metody
  // (`_method & request->method()`), wiec trasa HTTP_ANY lapie wszystko.
  int dopasuj(const String &sciezka, int metoda) const {
    for (std::size_t i = 0; i < trasy_.size(); ++i) {
      const Trasa218 &t = trasy_[i];
      if (!t.handler) continue;
      if ((t.metoda & metoda) == 0) continue;
      if (t.matcher.matches(sciezka)) return static_cast<int>(i);
    }
    return -1;
  }

  // Zwraca indeks trasy, ktora obsluzyla zadanie (-1 = onNotFound).
  int obsluz(AsyncWebServerRequest *request, const String &sciezka) {
    const int i = dopasuj(sciezka, request->method());
    if (i < 0) {
      if (notFound_) notFound_(request);
      return -1;
    }
    trasy_[static_cast<std::size_t>(i)].handler(request);
    return i;
  }

  std::size_t liczbaTras() const { return trasy_.size(); }
  const Trasa218 &trasa(std::size_t i) const { return trasy_[i]; }
  bool maNotFound() const { return static_cast<bool>(notFound_); }

  void wyczysc() {
    trasy_.clear();
    notFound_ = nullptr;
  }

 private:
  std::vector<Trasa218> trasy_;
  std::function<void(AsyncWebServerRequest *)> notFound_;
};

// Szybkie sprawdzenie statyczne (uzywane przez build.py i Test.cpp):
// czy dana sciezka trafia do trasy o podanym typie matchera.
inline bool trasaMaTyp(const AsyncWebServer &s, std::size_t i, AsyncURIMatcher::Typ typ) {
  return s.trasa(i).matcher.typ() == typ;
}
