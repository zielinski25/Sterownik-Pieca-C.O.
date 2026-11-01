#!/usr/bin/env python3
"""215: przygotuj katalog "prawdziwego API" z przypietych zrodel.

Nie trzymamy w gicie kopii cudzych naglowkow (LGPL). Zamiast tego bierzemy
je z etapow, ktore juz je przypielly i sprawdzily SHA-256, kopiujemy do
katalogu tymczasowego i dokladamy minimum SHIMOW (AsyncTCP, wersja,
EventSource, WebSocket, cbuf, FileOpenMode) - kazdy z komentarzem, po co.

Zrodla (wszystkie w repo, SHA sprawdzane):
  196/upstream/FS.h                - FS.h + LittleFS.h (uzywane przez biblioteke)
  197/upstream/ESPAsyncWebServer.h  - PRAWDZIWY naglowek biblioteki
  199/upstream/WebResponseImpl.h    - PRAWDZIWA definicja AsyncAbstractResponse
  206/upstream/literals.h           - literaly tekstowe HTTP
  206/upstream/WebHandlerImpl.h     - implementacje handlerow

Uruchomienie:
    python3 -B narzedzia/brama-api-esp215/przygotuj_api.py <katalog_docelowy>
"""
import hashlib
import os
import shutil
import sys

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
N = os.path.join(REPO, "narzedzia")

# plik zrodlowy -> (nazwa docelowa, sha256)
ZRODLA = {
    os.path.join(N, "uchwyty-runtime197", "upstream", "ESPAsyncWebServer.h"):
        ("ESPAsyncWebServer.h",
         "25db7c8fb30943307b28b181722187a65576f6ee778eeb99e29b1ef8db7915ba"),
    os.path.join(N, "odpowiedz-http199", "upstream", "WebResponseImpl.h"):
        ("WebResponseImpl.h",
         "b90de8322a97af18e8cd6817b7a5c27999c6a5e0097f23ecd77a4e167d9ced75"),
    os.path.join(N, "naglowki-upload206", "upstream", "literals.h"):
        ("literals.h",
         "2df08b5947a05fdb6b890a486708b046cc2bc903aa9c7d7a227c0a4a49b51fb3"),
    os.path.join(N, "naglowki-upload206", "upstream", "WebHandlerImpl.h"):
        ("WebHandlerImpl.h",
         "e2ab3f7b6af3544934bea31a06c803240a3d8a6c3bbeda9669854cf5d840a006"),
}

SHIMY = {
    # ESPAsyncWebServer.h wciaga AsyncTCP.h i uzywa dwoch typow: AsyncClient
    # (tylko wskaznik) i AsyncServer (tylko status()). Tyle wystarcza.
    "AsyncTCP.h": """#pragma once
#include <Arduino.h>
typedef enum { TCP_LISTEN, TCP_SYN_SENT, TCP_SYN_RECEIVED, TCP_ESTABLISHED,
               TCP_FIN_WAIT_1, TCP_FIN_WAIT_2, TCP_CLOSE_WAIT, TCP_CLOSING,
               TCP_LAST_ACK, TCP_TIME_WAIT, TCP_CLOSE } tcp_state;
class AsyncClient {};
class AsyncServer { public: tcp_state status() const { return TCP_CLOSE; } };
""",
    # Biblioteka sprawdza numer wersji w kilku #if.
    "AsyncWebServerVersion.h": """#pragma once
#define ASYNC_WEBSERVER_VERSION_MAJOR 3
#define ASYNC_WEBSERVER_VERSION_MINOR 9
#define ASYNC_WEBSERVER_VERSION_PATCH 1
#define ASYNC_WEBSERVER_VERSION "3.9.1"
""",
    # W naglowku biblioteki wystepuja tylko jako friend / typ.
    "AsyncEventSource.h": """#pragma once
#include <Arduino.h>
class AsyncEventSource { public: };
""",
    "AsyncWebSocket.h": """#pragma once
#include <Arduino.h>
class AsyncWebSocket { public: };
""",
    # WebResponseImpl.h trzyma unique_ptr<cbuf> (ESP8266 core).
    "cbuf.h": """#pragma once
#include <cstddef>
class cbuf {
 public:
  explicit cbuf(size_t size) : pojemnosc_(size) {}
  size_t write(const char *, size_t n) { return n; }
  size_t read(char *, size_t n) { return n; }
  size_t available() const { return 0; }
  size_t room() const { return pojemnosc_; }
  void flush() {}

 private:
  size_t pojemnosc_;
};
""",
    "FileOpenMode.h": """#pragma once
enum FileOpenMode { FILE_READ = 0, FILE_WRITE, FILE_APPEND };
""",
}

# Implementacja trzech funkcji z Arduino, ktorych naprawde potrzebuje kod.
POSIX_H = """#pragma once
#include <Arduino.h>
#include <chrono>
#include <cstdint>
inline unsigned long millis() {
  using namespace std::chrono;
  static const steady_clock::time_point start = steady_clock::now();
  return (unsigned long)duration_cast<milliseconds>(steady_clock::now() - start).count();
}
inline unsigned long micros() { return millis() * 1000UL; }
inline void delay(unsigned long ms) {
  using namespace std::chrono;
  const steady_clock::time_point do_kiedy = steady_clock::now() + milliseconds(ms);
  while (steady_clock::now() < do_kiedy) {}
}
"""

SHA_SCIEZEK = {}


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def przygotuj(cel):
    """Tworzy w <cel> katalogi api/ i shim/. Zwraca slownik z SHAs."""
    api = os.path.join(cel, "api")
    shim = os.path.join(cel, "shim")
    os.makedirs(api, exist_ok=True)
    os.makedirs(shim, exist_ok=True)
    wynik = {"api": {}, "shim": {}}

    for zrodlo, (nazwa, oczekiwany) in ZRODLA.items():
        if not os.path.isfile(zrodlo):
            raise SystemExit("brak przypietego zrodla: %s" % zrodlo)
        if sha256_sciezki(zrodlo) != oczekiwany:
            raise SystemExit("SHA-256 nie zgadza sie: %s" % zrodlo)
        shutil.copyfile(zrodlo, os.path.join(api, nazwa))
        wynik["api"][nazwa] = oczekiwany

    for nazwa, tresc in SHIMY.items():
        sciezka = os.path.join(api, nazwa)
        with open(sciezka, "w", encoding="utf-8") as f:
            f.write(tresc)
        wynik["api"][nazwa] = sha256_sciezki(sciezka)

    for nazwa in ("Arduino.h", "WString.h", "sdkconfig.h"):
        zrodlo = os.path.join(TU, "shim", nazwa)
        shutil.copyfile(zrodlo, os.path.join(shim, nazwa))
        wynik["shim"][nazwa] = sha256_sciezki(zrodlo)
    with open(os.path.join(shim, "PosixArduino.h"), "w", encoding="utf-8") as f:
        f.write(POSIX_H)
    wynik["shim"]["PosixArduino.h"] = sha256_sciezki(os.path.join(shim, "PosixArduino.h"))

    # FS.h i LittleFS.h z etapu 196 - biblioteka ich wymaga.
    for nazwa in ("FS.h", "LittleFS.h", "FSImpl.h"):
        zrodlo = os.path.join(N, "blokada-startu-fs196", "upstream", nazwa)
        shutil.copyfile(zrodlo, os.path.join(api, nazwa))
        wynik["api"][nazwa] = sha256_sciezki(zrodlo)
    return wynik


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("uzycie: przygotuj_api.py <katalog_docelowy>")
    print("PASS api215 przygotowane: %d plikow" % len(przygotuj(sys.argv[1])["api"]))
