# Plan etapów 207 → 214 (stan po etapie 213)

| Etap | Co robi | Stan | Patch do firmware |
|---|---|---|---|
| 206 | nagłówki wysyłki | zrobiony | nie (wiedza) |
| 207 | zapinka na historię (`Snapshots`) | zrobiony | nie (projekt) |
| 208 | odpowiedź strumieniowa | zrobiony | propozycja |
| 209 | trasa pobrania | zrobiony | propozycja |
| 210 | audyt: kto dotyka plików historii | zrobiony | nie (audyt) |
| 211 | zapinka w wysyłaczach Telegramu | zrobiony | **v3.55.9** |
| 212 | trasa `/history` + `/history/long` | zrobiony | **v3.56.0** |
| **213** | **przycisk w panelu + bilety jednorazowe** | **zrobiony (ten etap)** | **v3.56.1** |
| 214 | kolejka pobrań i wspólny licznik | do zrobienia | v3.56.x |

## Co dalej (214) i co trzeba o tym wiedzieć

Dziś drugie pobranie w trakcie pierwszego dostaje 503. To wystarcza, dopóki
pobiera jeden człowiek z jednego telefonu. Jeśli okaże się, że aplikacja i panel
ciągną historię naraz, 214 ma sens: **kolejka dwuetapowa** (bilet rezerwuje
miejsce, plik jest wysyłany po kolei) albo **wspólny licznik bajtów**, żeby dwa
pobrania nie zjadły całej pamięci.

Nie zaczynam 214 bez danych z urządzenia: dziś nie wiem, czy 503 w praktyce
występuje. Jeśli po wgraniu 3.56.1 zobaczysz „Trwa inne pobranie” częściej niż
raz na tydzień, 214 jest potrzebne. Jeśli nie — 214 to zbędny kod.

## Kolejność sprawdzeń przed każdym wgraniem (ta sama od 209)

1. `python3 -B narzedzia/panel-pobrania213/build.py` — patch się nakłada,
   oba warianty dają ten sam plik, panel nie ma tokenu.
2. `python3 -B narzedzia/panel-pobrania213/run.py` — 3 profile × 2 zestawy.
3. `python3 -B narzedzia/panel-pobrania213/mutacje.py` — wszystkie mutanty wykryte.
4. Dopiero potem: `patch -p1`, sprawdzenie `FIRMWARE_VERSION`, kompilacja ESP,
   wgranie przez Ciebie.

## Czego ten plan świadomie NIE obejmuje

* wgrywania — to Twoja decyzja,
* OTA i samoodzyskiwania po nieudanym wgraniu,
* zmiany rytmu 8 s / 60 s, logiki pompy, alarmów, NVS,
* formatu CSV i rotacji historii,
* uwierzytelniania panelu jako całości (osobny, większy temat).
