# Co jest zrobione, a co nie — stan na 2026-10-28

Jeden plik zamiast siedmiu. Odpowiedź na pytanie „czy wszystko z plików MD
jest już zrobione": **nie**. Kod i testy offline — tak. Sprawdzenie na
urządzeniu — nie, i część z tego jest fizycznie niemożliwa z odległości.

## ZROBIONE I POTWIERDZONE

| co | dowód |
|---|---|
| v3.56.5 kompiluje się w prawdziwym PlatformIO | zgłoszenie użytkownika „sukces" — pierwszy pełny build tego kodu |
| Blok historii wyniesiony z ciała `wifiZarejestrujTrasy()` | 1 naruszenie → 0, diff = czyste przeniesienie + wersja |
| Pobranie historii przez Telegram działa na piecu | 2 pliki CSV + `log_ustawien.txt` przyszły na Telegram, wgrane na Dysk |
| Dane z pieca są poprawne | 1 515 wierszy, wszystkie 14 kolumn, 1 próbka/min, zero luk |
| Format `hist_long` = 36 kolumn | policzone z pliku (poprzednio błędnie podawano 35) |
| Zmiana czasu letniego obsłużona poprawnie | etykiety CEST→CET, `czas_unix` ciągły |
| Oba CSV kończą się `\n` (warunek startu v3.56.5) | sprawdzone bajt po bajcie na oryginałach z Dysku |
| Wgranie nie kasuje historii | `Transition::run()`: kasowanie tylko przy pliku A1, którego nie ma |
| Awaria LittleFS nie odcina WiFi/Telegrama | `BootFilesystem` zwraca pusty FS, sieć działa dalej |
| Nadzorca nie restartuje pieca przy braku internetu | heartbeat odświeżany co iterację, niezależnie od sieci |
| Testy hostowe etapów 209–222 | `PASS brama222`, `brama221`, `brama220`, 219 — wszystkie zielone |

## NIE ZROBIONE — czeka na urządzenie

| co | dlaczego nie zrobione | kiedy |
|---|---|---|
| **Wgranie v3.56.5** | decyzja użytkownika: „dzisiaj nie będę wgrywał, jutro spróbuję" | jutro albo przy wizycie |
| **Trasa `/history` i `/history/long` na prawdziwym piecu** | działa tylko w sieci domowej; 100 km = brak dostępu | po wgraniu, na miejscu |
| **Przyciski „Krótka (24 h)" / „Długa (archiwum)" na telefonie** | to jest właściwe kryterium odbioru — i jest niesprawdzone | na miejscu |
| **Wygląd panelu w przeglądarce** | brak dostępu do panelu | na miejscu |

## NIE ZROBIONE — świadomie odłożone

| co | powód |
|---|---|
| Kolejka pobrań / wspólny licznik bajtów | potrzebna tylko gdy na piecu pojawi się 503 „Trwa inne pobranie" — jeszcze nie wystąpiło |
| Rotacja haseł (`CMD_TOKEN`, AP, OTA, Firebase) | hasła są w ZIP-ach trackowanych w gicie; rotacja wymaga zmiany w centrali, aplikacji i wyświetlaczu naraz |
| Nagłówek kolumn w CSV | pliki go nie mają; kolumny zna kod panelu. Kosmetyka, do decyzji |

## RYZYKA ZAPISANE DO OBSERWACJI

1. **Piec nie grzeje** — przez 25 h pompa = 0, bojler 48,5 → 21,5 °C, pokój do 15 °C,
   na zewnątrz 15 °C. Wygląda na wyłączone ogrzewanie, nie na awarię, ale wymaga
   potwierdzenia. Nie jest to problem tej funkcji — to obserwacja z danych.
2. **Korekta zegara w tył o 51 min** — 25.10, wiersz 447→448 `hist_long`.
   Najpewniej NTP po starcie. Sprawdzić, czy się powtarza.
3. **Nadmiarowy wiersz w `hist_long`** — zapis z 28.10 20:07 w pliku wysłanym
   19:22. Albo brak wpisu w raporcie, albo błąd. Do sprawdzenia przy następnym `/historia`.

## JEDNO ZDANIE PODSUMOWANIA

Funkcja jest **napisana, skompilowana i działa w warstwie danych** — historia
z pieca schodzi na Telegram. Brakuje wyłącznie sprawdzenia trasy HTTP i przycisków
w panelu, co wymaga wgrania firmware i dostępu do sieci domowej.
