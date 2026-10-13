# Wykresy lokalne i Firebase — baza v3.38.0, źródło v3.45.0

Ta łatka zachowuje wykres ESP korzystający wyłącznie z lokalnego archiwum centrali i naprawia osobną ścieżkę wykresu Firebase. `Piec.html` jest osadzony w przygotowanym CPP. Głównego `Cenatrala Pieca.zip` w katalogu repozytorium nie zmieniamy.

## Zachowanie wykresu ESP (bez zmiany źródła lokalnego)

- **6 h / 24 h:** pobieranie `GET /api/history` (lokalny bufor minutowy zapisany w `/hist_short.csv`).
- **7 dni / 30 dni:** pobieranie `GET /api/history/long` oraz łączenie z ostatnimi 24 h z `/api/history`.
- Na stronie serwowanej przez ESP wykres, jego próbki, pompa i odświeżanie nadal pochodzą wyłącznie z lokalnych endpointów. Błąd lokalnego API nie uruchamia fallbacku Firebase.
- Istniejące lokalne archiwum nie zapisuje `t_trociny` ani `dym`; te opcjonalne serie pozostają bez lokalnej historii.
- Lokalna ścieżka i jej rozmieszczanie próbek pozostają wzorcem i nie korzystają z danych statusowych Firebase.

## Naprawiona ścieżka wykresu Firebase

- Wykres dodaje wyłącznie rekordy zapisane w archiwum RTDB, używając ich `ts`. Odpytanie `/piec/status` co 4 s aktualizuje bieżący panel, ale nie tworzy historycznego punktu wykresu.
- Oś X, ticki, powiększanie, przewijanie i crosshair po stronie Firebase odnoszą się do czasu próbek, a nie do ich indeksu w tablicy.
- Zakres `od–do` jest ustawiany na granicach minutowych. Wyświetlany opis pokazuje granice aktywnego okna.
- Co minutę strona doczytuje tylko końcówkę archiwum z krótkim nakładaniem, a rekordy łączy i deduplikuje po ID (z fallbackiem `ts + seq`). Nie pobiera ponownie całego zakresu 7/30 dni.
- Konwersja serw do procentów zachowuje wartości ułamkowe jak parser lokalnego CSV, a dolne granice `t_panel` i `t_trociny` odpowiadają zakresom wspólnego filtra wykresu.

Dla porównania 6 h / 24 h wybierz te same serie i ustaw ten sam zakres. Lokalne i Firebase-owe archiwa minutowe mogą różnić się dostępnością pojedynczych zapisów. Dla 7/30 dni dokładna zgodność rozdzielczości nie jest gwarantowana: lokalne długie archiwum jest zdarzeniowe, a RTDB zapisuje próbki minutowe. Porównanie wymaga też takich samych filtrów i ustawień osi.

## Wersja i bezpieczeństwo zmian

Źródłowe `FIRMWARE_VERSION` przechodzi z **v3.44.0** na **v3.45.0**. Nie zmieniano algorytmu sterowania piecem, serw, dymu ani pompy. Nie wykonano kompilacji firmware, flashowania ani poleceń do urządzenia. Główny ZIP firmware w checkoutcie nie jest częścią tej łatki.

## Zastosowanie

Łatka skumulowana jest względem bazowego `src/main_centrala.cpp` v3.38.0. W osobnej kopii tej bazy sprawdź i zastosuj:

```sh
patch --dry-run -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
patch -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
```

Nie stosuj jej do zmodyfikowanego `Cenatrala Pieca.zip` w głównym checkoutcie. Budowanie, instalacja i ewentualne OTA są osobnymi działaniami.

## Weryfikacja

- JavaScript strony sprawdzony parserem Node.js.
- Testy regresyjne sprawdzają: brak tworzenia punktów z pollingu statusu, deduplikację archiwum, przyrostowe doczytywanie, pozycję X opartą o `ts` oraz niezależną ścieżkę lokalną.
- `WIFI_LOCAL_PANEL_HTML` jest porównywany bajt w bajt z aktualnym `Piec.html`.
- Skumulowaną łatkę sprawdzono przez dry-run i zastosowanie do śledzonej bazy v3.38.0; wynik odtwarza przygotowany CPP.
- Nie wykonano `pio run`, uploadu, flashowania ani komend sterujących piecem.
