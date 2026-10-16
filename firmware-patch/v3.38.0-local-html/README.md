# Wspólne rozpalanie, wykresy lokalne i Firebase — baza v3.38.0, źródło v3.46.0

Ta skumulowana łatka zachowuje lokalne archiwum wykresu ESP, używa timestampów RTDB dla wykresu Firebase i dodaje jedno wspólne sterowanie rozpalaniem serw oraz mieszadła. `Piec.html` jest osadzony w przygotowanym CPP. Głównego `Cenatrala Pieca.zip` w katalogu repozytorium nie zmieniamy.

## Jedno wspólne sterowanie „Rozpalaniem”

- `rozpalanie_serwa` uruchamia jednocześnie sekwencję serw i czasowy override mieszadła; `rozpalanie_stop` wyłącza cały tryb. `ustaw rozpalanie 1/0` pozostaje aliasem start/stop.
- Start jest odrzucany przy nieświeżej/niewiarygodnej temperaturze, aktywnym alarmie bezpieczeństwa albo temperaturze pieca powyżej 30°C. Zimne okno `<25°C` udostępnia przycisk, ale samo nie uruchamia wymuszeń.
- Rozpalenie jest potwierdzone, gdy temperatura pieca przekroczy 30°C **lub** wzrośnie o co najmniej 5°C względem odczytu zapisanego przy starcie.
- Bez potwierdzenia tryb kończy się po 30 min; po potwierdzeniu nadal obowiązuje bezwzględny limit 60 min od startu. Tryb nie jest odtwarzany po restarcie.
- Alarmy bezpieczeństwa i dymu nadal zatrzymują wymuszenia. Po normalnym zakończeniu sterowanie wraca do zwykłej automatyki serw i mieszadła.
- Status publikuje `rozpalanie`, `rozpalanie_potwierdzone` i `rozpalanie_pozostalo_s`; interfejsy blokują start na starszym firmware bez pól potwierdzających obsługę wspólnego trybu.

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

## Lokalne wgrywanie OTA przez PlatformIO

- Panel wyświetla przycisk kopiowania komendy `pio run -e esp32-s3-ota -t upload --upload-port <IP-centrali>`. Adres IP pobierany jest z bieżącego statusu centrali.
- Kliknięcie tylko kopiuje tekst do schowka — nie wybiera ani nie przesyła pliku i nie uruchamia uploadu. Użytkownik wkleja komendę i uruchamia ją samodzielnie w terminalu, w katalogu projektu PlatformIO.
- Środowisko `esp32-s3-ota` dostarcza konfigurację poświadczeń OTA; hasło nie jest wpisywane do skopiowanej komendy.

## Wersja i bezpieczeństwo zmian

Źródłowe `FIRMWARE_VERSION` przechodzi z **v3.45.0** na **v3.46.0**. Zmieniono wspólną logikę rozpalania oraz jej lokalne/Firebase API, a panel przywraca lokalny przepływ OTA przez przycisk kopiujący komendę PlatformIO; zabezpieczenia alarmowe pozostają aktywne. Nie zmieniano głównego `Cenatrala Pieca.zip`. W tej sesji nie wykonano kompilacji firmware, uploadu, flashowania ani poleceń do urządzenia.

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
