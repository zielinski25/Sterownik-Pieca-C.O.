# UWAGA — te patche się NIE SKOMPILUJĄ

Ten etap (212) został dostarczony z błędem, który znalazł etap 214.

**Co jest źle:** `#include "HistoriaPobranie.h"` wstawiono PRZED
`#include <ESPAsyncWebServer.h>` (linia 5134 vs 5227). `HistoriaPobranie.h`
dziedziczy po `AsyncAbstractResponse` i woła metody `AsyncWebServerRequest`,
których w tym miejscu jeszcze nie ma. Na ESP to błąd kompilacji.

**Dlaczego testy tego nie złapały:** `Test.cpp` wciąga najpierw model serwera
(`Serwer212.h`), więc kolejność w `main_centrala.cpp` nie ma dla niego znaczenia.

**Co używać zamiast tego:** patch z etapu 214
`narzedzia/brama-przed-wgraniem214/main_centrala-v3.55.8-to-v3.56.2.patch`
(211 + 212 + 213 + poprawka, v3.55.8 → v3.56.2).

Pliki tego katalogu zostają jako zapis historyczny — nie są usuwane, ale
**nie należy ich nakładać**.
