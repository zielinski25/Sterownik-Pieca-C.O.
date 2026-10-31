# Etap 223 — v3.56.5: prawdziwy błąd kompilacji naprawiony

## Co się stało

Pierwsze prawdziwe `pio run` u użytkownika (2026-10-28) zatrzymało się na:

```
src/main_centrala.cpp:28745: error: a function-definition is not allowed here before '{' token
  static void historiaLosuj213(std::uint8_t *out, std::size_t n) {
src/main_centrala.cpp:28748: error: a function-definition is not allowed here before '{' token
src/main_centrala.cpp:28749: error: 'historiaLosuj213' was not declared in this scope
```

**Przyczyna (prawdziwa, w firmware od v3.56.0/v3.56.1, nie w testach):**
blok historii — `HistoriaBramka212`, obiekt `Pobranie`, `historiaLosuj213`,
`historiaZegar213`, `historiaBilety`, `HistoriaBiletyPodpiecie213` — został
wpisany **w środku** funkcji `wifiZarejestrujTrasy()`. W C++ wewnątrz funkcji
wolno zdefiniować lokalną klasę i lokalny obiekt statyczny, ale **nie wolno
zdefiniować funkcji**. Dlatego kompilator padał na `historiaLosuj213`.

## Dlaczego etapy 219–222 tego nie złapały

| etap | co kompilował | czy mógł to złapać |
|---|---|---|
| 219 | analiza tekstu (Python) | nie |
| 220 | zgodność wywołań z nagłówkami biblioteki | nie |
| 221 | **wycięte fragmenty** pliku (nie cały plik) | nie — fragment wycięty zaczynał się na `struct`, bez otaczającej funkcji |
| 222 | to samo + panel | nie |

Wniosek na przyszłość: każda zmiana w `main_centrala.cpp` musi przejść
**kompilację całego pliku**, nie tylko wycinka.

## Poprawka (v3.56.5)

Cały blok przeniesiony **przed** `void wifiZarejestrujTrasy() {`, na zakres
globalny. Treść bloku bez zmian — porównanie linia w linii:
`main_centrala-v3.56.4-to-v3.56.5.patch` (6 615 B: 7 linii komentarza,
1 linia numeru wersji, 41 przeniesionych linii).

Trasy `/history`, `/history/long`, `/history/link` **zostają w funkcji**
(były tam poprawne).

## Weryfikacja

Skrypt `napraw223.py`:

1. znajduje wszystkie definicje funkcji w ciałach funkcji, z pominięciem
   surowych napisów C++ (`R"…(…)…"`), komentarzy i metod klas,
2. przed poprawką: **1** trafienie (linia 28745),
3. po poprawce: **0**,
4. głębokość nawiasów na początku przeniesionego bloku: **0** (zakres globalny),
5. nawiasy w całym pliku: 3449 `{` = 3449 `}`.

Kontrola zależności: wszystko, czego blok używa (`historiaSys`,
`historiaDostepGotowy`, `littlefsReady`, `g_histShortSize/LongSize`,
`logMutex`, `historia211::kShortVfsPath`, `historia212`) jest zadeklarowane
**wyżej** niż nowe miejsce (linie 22793–25047).

## secrets.h — drugi, osobny problem

Kompilator zgłosił też brak `SECRET_WIFI_AP_SSID`, `SECRET_WIFI_AP_PASSWORD`,
`SECRET_OTA_HASLO`, `SECRET_FIREBASE_PASS`, `SECRET_CMD_TOKEN`. To nie błąd
firmware: plik `src\secrets.h` w projekcie użytkownika powstał w tej sesji
i miał **tylko** linię z tokenem historii. Brakuje pozostałych wartości —
a są to hasła użytkownika, których nie znamy i których nie wolno wymyślać.

## Pliki

* `napraw223.py` — skrypt poprawki (z walidacją)
* `main_centrala-v3.56.4-to-v3.56.5.patch` — pełna różnica
* `paczka/PROJEKT-v3.56.5-POPRAWIONY.zip` — 725 012 B, 18 wpisów,
  sha256 `81bf40f8…49c2`, Drive id `1s9GB4cyYmFygO5wThMOvyjUoNevGLAyc`

## Stan

v3.56.5 **nie jest** sprawdzony kompilatorem — to robi użytkownik.
Poprawka usunęła jedyne wykryte naruszenie zakresu; następny błąd (jeśli
będzie) pokaże kolejne `pio run`.
