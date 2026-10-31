# 246 — 0.31.63: naprawa kompilacji 0.31.62 (stan wykresów poza blokiem `init`)

Data: 2026-10-30 · Dotyczy: aplikacja Android · Firmware: bez zmian (3.57.1)

## Co się stało

Budowanie **0.31.62** padło:

```
e: AppModel.kt:282:17 Variable cannot be initialized before declaration.
e: AppModel.kt:284:13 Variable cannot be initialized before declaration.
BUILD FAILED in 1m 10s
```

To mój błąd. Odtwarzanie stanu wykresów wpisałem bezpośrednio w blok `init`
klasy `AppModel`, a użyłem tam dwóch pól:

| linia | pole | gdzie zadeklarowane |
|---|---|---|
| 282 | `historyLoadedRange` | linia 1284 (po `init`) |
| 284 | `historyLoadedElapsed` | linia 1283 (po `init`) |

Kotlin nie pozwala inicjalizować pola przed jego deklaracją w tekście klasy.
Pozostałe pola (`telemetry`, `chartLive`, `chartPoints`, `rangeSec`, `page`)
są zadeklarowane **przed** `init` (linie 205–245), więc one nie powodowały błędu.

## Poprawka (0.31.63 / versionCode 31063)

1. Cały blok odtwarzania stanu przeniesiony do nowej funkcji
   `private fun restoreRuntimeState()`, umieszczonej tuż przed `onResume()`.
   W ciele funkcji kolejność deklaracji pól nie ma znaczenia.
2. Funkcja jest wołana na początku `onResume()` — a więc **przed**
   `if (page == 1) loadRange(rangeSec)`, dzięki czemu odtworzony znacznik
   `historyLoadedElapsed` od razu działa (brak ponownego pobierania).
3. Zabezpieczenie `runtimeRestored`: odtworzenie wykonywane jest raz na model.
4. W bloku `init` zostało wyłącznie `page = prefs.getInt(Prefs.K_LAST_PAGE, 0)`
   — pole zadeklarowane w linii 205, czyli przed `init`.

## Kontrola wykonana przed spakowaniem

Skrypt przeanalizował plik `AppModel.kt`:

- znalazł blok `init` (linie 255–291) i wszystkie deklaracje pól klasy,
- sprawdził, czy w `init` jest przypisanie do pola zadeklarowanego **później**:
  **BRAK** (wynik pusty),
- bilans nawiasów po usunięciu komentarzy i napisów: klamry 538/538,
  nawiasy okrągłe 1595/1595.

To ta sama klasa błędu, którą dał 0.31.59 (`statusStampBefore`) — za każdym
razem winna jest zmiana robiona „w locie” bez kompilatora pod ręką. Sandbox nie
ma Android SDK, więc jedyne, co mogę zrobić, to wyłapać takie rzeczy statycznie
— i tym razem zrobiłem to jawnie, nie „na oko”.

## Co ta wersja daje (funkcjonalnie to samo, co miała dać 0.31.62)

- zakładka zapamiętana (`Prefs.K_LAST_PAGE`) — po powrocie jesteś na Wykresach,
- wykres z pamięci **procesu** (`AppRuntime.lastTelemetry` itd.) rysuje się od razu,
- okno „parę minut” bez pytania chmury: `HISTORY_MIN_INTERVAL_MS` = 5 min,
- `launchMode="singleTop"` + `FLAG_ACTIVITY_SINGLE_TOP` — alarm nie odtwarza panelu,
- wpis diagnostyczny w dzienniku:
  `Start panelu: pid=1234 wiek=37s · zakładka 1 · wykres z pamięci (1450 pkt)`
  (inny PID = proces zabity przez system; ten sam PID = tylko odtworzenie okna).

## Weryfikacja paczki

| Co | Wynik |
|---|---|
| `versionCode` / `versionName` | 31063 / „0.31.63” |
| `AppRuntime.kt` w ZIP | jest |
| `restoreRuntimeState` w ZIP | jest (funkcja + wywołanie w `onResume`) |
| `unzip -tq` | bez błędów, 190 plików |
| SHA-256 | `d0ce2cd18eb0283efb82f110f0066fdf813d8ebd308e7ad6b451a7f6a5c5537e` (1 176 191 B) |
| Pobranie z Dysku i porównanie SHA | zgodne |

## Paczka

`SterownikCO-Android-v0.31.63-PELNY-PROJEKT.zip`
ID `1LFrgH_AwQsie6NDH-KVmD_gqUWtyewoB`
SHA-256 `d0ce2cd18eb0283efb82f110f0066fdf813d8ebd308e7ad6b451a7f6a5c5537e`

0.31.62 → **NIE BUDOWAĆ** (błąd kompilacji opisany wyżej).
