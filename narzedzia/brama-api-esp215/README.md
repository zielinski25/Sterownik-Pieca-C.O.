# Etap 215 — brama API: kompilacja wobec PRAWDZIWEGO ESPAsyncWebServer

**Status: KANDYDAT. NIE WGRYWAĆ.** Ten etap nic nie wgrywa i nic nie zmienia
w firmware. Sprawdza istniejący kod.

## 1. Po co to jest

Etapy 211–213 były sprawdzane **modelem** serwera (`Serwer212.h`, `Serwer213.h`).
Ten model napisałem ja — więc powtarzał moje założenia. I przepuścił błąd D1
(kolejność `#include`), który na ESP jest błędem kompilacji.

Ten etap robi coś innego: bierze **prawdziwy nagłówek biblioteki**
(`ESPAsyncWebServer.h` z forka mathieucarbou, przypięty w repo z SHA-256),
kompiluje go na komputerze i kompiluje wobec niego nasz kod. To nie jest już
„test przeciwko temu, co myślę, że biblioteka robi”, tylko **przeciwko temu,
co biblioteka naprawdę deklaruje**.

## 2. Co dokładnie jest kompilowane

| # | jednostka kompilacji | po co |
|---|---|---|
| **A** | sam `ESPAsyncWebServer.h` | shim Arduino jest wystarczający; jeśli tu błąd, dalej nic nie ma sensu |
| **B-gnu++11** | `HistoriaPin.h` + `HistoriaPobranie.h` + `HistoriaBilety.h` wobec prawdziwego API | zgodność z prawdziwymi sygnaturami |
| **B-gnu++17** | to samo w C++17 | rdzeń ESP32 3.x kompiluje w C++17 |
| **C-gnu++11** | szkielet z **prawdziwym kodem wyciętym z pliku po patchu v3.56.2** (token, adaptery losowości, bramka, obiekty, trzy rejestracje tras) | kod, który realnie pójdzie na urządzenie, a nie moja kopia |
| **C-gnu++17** | to samo w C++17 | — |
| **D** | includes w kolejności **z etapu 213** | **musi się NIE kompilować** — dowód, że D1 był prawdziwy |
| **E** | celowo zepsuty podpis `_fillBuffer(uint8_t*, unsigned)` | musi być wykryty — dowód, że `override`/`static_assert` nie są puste |

Wszystko w `-fno-exceptions -fno-rtti -Wall -Wextra -Werror` (biblioteka przez
`-isystem`, żeby jej ostrzeżenia nie były naszymi).

**Wynik: 7/7 PASS.** W tym C — 57 linii prawdziwego kodu z pliku po patchu.

## 3. Co to udowadnia, a czego nie

**Udowadnia:**
* nasze klasy poprawnie dziedziczą po prawdziwym `AsyncAbstractResponse`,
* `_fillBuffer` i `_sourceValid` mają podpisy z prawdziwej biblioteki
  (sprawdzone `static_assert`, nie „na oko”),
* pola `_code`, `_contentType`, `_contentLength`, `_sendContentLength`, `_chunked`
  są w prawdziwej klasie **protected** i mamy do nich dostęp,
* `getParam`, `hasParam`, `getHeader`, `beginResponse`, `addHeader`, `send`
  istnieją z takimi argumentami, jak ich używamy,
* bramka i rejestracje wyciągnięte z pliku po patchu kompilują się w C++11 i C++17.

**Nie udowadnia:**
* że cały `main_centrala.cpp` się skompiluje — ciągnie 20+ bibliotek
  (WiFi, Firebase, ArduinoJson, DS18B20, BMP280, RTClib, SD, SPI, mbedtls…),
  których tu nie ma. To zostaje dla Twojego PlatformIO.
* że cokolwiek działa na urządzeniu.

## 4. Co jeszcze może pójść nie tak na ESP (i co wtedy robić)

Lista na podstawie tego, co już wiemy o prawdziwej bibliotece:

| # | możliwy błąd | dlaczego możliwy | co robić |
|---|---|---|---|
| 1 | `AsyncClient`/`AsyncServer` niezgodne z Twoją wersją AsyncTCP | shim zakłada tylko `status()` | nic — to nie dotyczy naszego kodu |
| 2 | `beginResponse(int, const char*, String)` niejednoznaczne | w prawdziwej klasie jest 5 przeciążeń; my trafiamy w `(int, const String&, const String&)` | jeśli błąd: dopisz `.c_str()` przy tekście |
| 3 | `String` bez `operator bool` | prawdziwy `WString` go nie ma | nasz kod nie polega na tym — sprawdzone |
| 4 | brak `esp_system.h` przed `esp_random()` | `esp_random` jest w `esp_system.h` | już jest (linia 5163), kontrola S16 z etapu 214 |
| 5 | `PROGMEM`/`R"PIECHTML(...)"` za długi dla RAM | panel ma teraz 2,6 kB więcej | jeśli zabraknie RAM: przenieść panel do LittleFS |
| 6 | ostrzeżenie `unused parameter` jako błąd | jeśli masz `-Werror` globalnie | dodać `(void)parametr;` |
| 7 | `historia213::Bilety` jako globalny obiekt | konstruktor zapisuje wskaźniki, nie alokuje | bezpieczne; brak zależności od kolejności |

## 5. Pliki

| Plik | Rola |
|---|---|
| `przygotuj_api.py` | składa katalog „prawdziwego API” z przypiętych źródeł (SHA-256) + 6 shimów |
| `shim/WString.h`, `shim/Arduino.h` | shim Arduino dla komputera (świadomie minimalny: **bez** `operator bool`) |
| `kompiluj.py` | 7 kontroli A–E |
| `Extracted215.inc` | wygenerowany: prawdziwy kod z pliku po patchu v3.56.2 |
| `run.py` | → `WYNIKI/runNNN/wynik.json` |
| `AUDYT.json`, `PLAN.md` | twierdzenia i kolejka |

## 6. Komendy

```bash
python3 -B narzedzia/brama-api-esp215/kompiluj.py   # 7 kontroli
python3 -B narzedzia/brama-api-esp215/run.py        # + raport JSON
```
