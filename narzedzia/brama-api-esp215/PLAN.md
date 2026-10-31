# Plan 207 → 216 (stan po etapie 215)

| Etap | Co robi | Stan | Patch |
|---|---|---|---|
| 207–210 | zapinka, odpowiedź strumieniowa, trasa, audyt | zrobione | nie |
| 211 | zapinka w wysyłaczach Telegramu | zrobiony | v3.55.9 |
| 212 | trasa `/history`, `/history/long` | zrobiony (patch przestarzały, D1) | v3.56.0 |
| 213 | przycisk w panelu + bilety | zrobiony (patch przestarzały, D1) | v3.56.1 |
| 214 | brama przed wgraniem + poprawka D1 | zrobiony | **v3.56.2** |
| **215** | **brama API wobec prawdziwego ESPAsyncWebServer** | **zrobiony (ten etap)** | **bez zmian** |
| 216 | Twoja kompilacja ESP + poprawki, jeśli będą | czeka na Ciebie | v3.56.x |

## Co się zmieniło w planie po 215

Wcześniej planowałem „216 = lista prawdopodobnych błędów kompilacji”. Po 215
ta lista jest **krótsza i konkretniejsza**, bo połowę potencjalnych błędów
wykluczył prawdziwy kompilator (sekcja 4 w `README.md`). Zostało 7 pozycji,
z których tylko dwie (5 i 6) mogą realnie wystąpić na Twoim sprzęcie.

Dlatego 216 to już nie „zgadywanie błędów”, tylko:

1. Ty nakładasz `main_centrala-v3.55.8-to-v3.56.2.patch`,
2. kompilujesz PlatformIO,
3. wysyłasz mi wynik („przeszło” albo pierwszy błąd).

Jeśli przejdzie — następny sensowny krok to **test na urządzeniu**: pobranie
historii z telefonu i sprawdzenie, czy PDF/CSV ma te same wiersze co
`/api/history`. Jeśli nie przejdzie — poprawka będzie mała, bo wiemy już,
że API się zgadza.

## Kolejność sprawdzeń przed wgraniem

1. `python3 -B narzedzia/brama-przed-wgraniem214/build.py` — 18 kontroli pliku.
2. `python3 -B narzedzia/brama-przed-wgraniem214/selftest.py` — 16 przypadków.
3. `python3 -B narzedzia/brama-api-esp215/kompiluj.py` — 7 kontroli API.
4. `patch -p1`, `grep FIRMWARE_VERSION` (linia 5591).
5. Kompilacja ESP i wgranie — u Ciebie.

## Czego plan nie obejmuje

* wgrywania, OTA i samoodzyskiwania,
* rytmu 8 s / 60 s, pompy, alarmów, NVS, CSV, rotacji,
* kolejki pobrań (odłożona — brak pomiaru, że dwa pobrania naraz się zdarzają),
* uwierzytelniania panelu jako całości.
