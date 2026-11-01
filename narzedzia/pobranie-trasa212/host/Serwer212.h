#pragma once
// 212: model zadan i odpowiedzi HTTP. Modelowany jest WYLACZNIE interfejs
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
