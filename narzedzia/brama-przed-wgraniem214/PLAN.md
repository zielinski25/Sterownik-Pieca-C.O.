# Plan 207 → 216 (stan po etapie 214)

| Etap | Co robi | Stan | Patch |
|---|---|---|---|
| 207 | zapinka na historię | zrobiony | nie (projekt) |
| 208 | odpowiedź strumieniowa | zrobiony | propozycja |
| 209 | trasa pobrania | zrobiony | propozycja |
| 210 | audyt dostępu do plików historii | zrobiony | nie (audyt) |
| 211 | zapinka w wysyłaczach Telegramu | zrobiony | v3.55.9 |
| 212 | trasa `/history`, `/history/long` | zrobiony, **patch przestarzały (D1)** | v3.56.0 |
| 213 | przycisk w panelu + bilety | zrobiony, **patch przestarzały (D1)** | v3.56.1 |
| **214** | **brama przed wgraniem + poprawka D1** | **zrobiony (ten etap)** | **v3.56.2** |
| 215 | kompilacja ESP (Twoja) — pierwszy prawdziwy kompilator | czeka na Ciebie | — |
| 216 | (po kompilacji) poprawki błędów kompilacji, jeśli będą | nie zaczęty | v3.56.x |

## Dlaczego 215 jest po Twojej stronie

W tym środowisku nie ma toolchainu ESP32 i nie ma dostępu do serwerów, z których
się go pobiera. Brama 214 maksymalnie zmniejsza ryzyko, ale **nie zastąpi
kompilatora**. Najtańszy scenariusz: nakładasz patch, uruchamiasz kompilację,
wysyłasz mi pierwszą linię błędu (albo „przeszło”). Wtedy wiem, czy 216
w ogóle jest potrzebne.

## Kolejność sprawdzeń (nie zmienia się od 209)

1. `python3 -B narzedzia/brama-przed-wgraniem214/build.py` — patche, odwracalność, 18 kontroli.
2. `python3 -B narzedzia/brama-przed-wgraniem214/selftest.py` — 16 przypadków, że brama łapie.
3. `patch -p1`, `grep FIRMWARE_VERSION` (linia 5591), `grep -A2 ESPAsyncWebServer.h`.
4. Kompilacja ESP — u Ciebie.
5. Wgranie — u Ciebie.

## Czego ten plan nie obejmuje

* wgrywania, OTA i samoodzyskiwania,
* zmiany rytmu 8 s / 60 s, pompy, alarmów, NVS,
* formatu CSV i rotacji historii,
* kolejki pobrań (etap 214 z pierwotnego planu) — **odłożony**, bo nie ma
  pomiaru, że dwa pobrania naraz w ogóle się zdarzają.
