# UWAGA — te patche się NIE SKOMPILUJĄ

Ten etap (213) odziedziczył błąd z etapu 212 i go nie naprawił.

**Co jest źle:** `#include "HistoriaPobranie.h"` i `#include "HistoriaBilety.h"`
są wciągnięte PRZED `#include <ESPAsyncWebServer.h>` (linie 5140–5141 vs 5227).
Trasa dziedziczy po `AsyncAbstractResponse`, a `AsyncWebServerRequest` jest
używany w ciele metod — na ESP to błąd kompilacji.

**Co używać zamiast tego:** patch z etapu 214
`narzedzia/brama-przed-wgraniem214/main_centrala-v3.55.8-to-v3.56.2.patch`
(211 + 212 + 213 + poprawka, v3.55.8 → v3.56.2), albo — jeśli masz już
v3.56.1 — mały patch `main_centrala-v3.56.1-to-v3.56.2.patch`.

Testy tego etapu (20 grup / 126 sprawdzeń + regresja 212) są nadal ważne:
sprawdzają logikę trasy i biletów. Nie sprawdzają kolejności include'ów —
to robi brama z etapu 214.

Pliki tego katalogu zostają jako zapis historyczny — **nie należy ich nakładać**.
