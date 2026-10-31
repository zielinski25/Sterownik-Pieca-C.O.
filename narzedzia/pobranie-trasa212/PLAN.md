# Plan etapów 207 → 214 (stan po etapie 212)

Kolejność jest **narzucona zależnościami**, nie wygodą: każdy etap korzysta
z kodu poprzedniego. Nie da się ich przestawić bez przepisywania.

| Etap | Co robi | Stan | Patch do firmware |
|---|---|---|---|
| 206 | nagłówki wysyłki (`AsyncWebHeader`) | zrobiony | nie (wiedza) |
| 207 | zapinka na historię (`Snapshots`) | zrobiony | nie (projekt) |
| 208 | odpowiedź strumieniowa (`SnapshotResponse`) | zrobiony | propozycja |
| 209 | trasa pobrania (`DownloadRoute`) | zrobiony | propozycja |
| 210 | audyt: kto dotyka plików historii | zrobiony | nie (audyt) |
| 211 | zapinka w wysyłaczach Telegramu | zrobiony | **v3.55.9** |
| **212** | **trasa `/history` + `/history/long`** | **zrobiony (ten etap)** | **v3.56.0** |
| 213 | strona „Pobierz historię” w panelu WWW (przycisk + token) | do zrobienia | v3.56.x |
| 214 | wspólny limit i kolejka pobrań (jedna trasa, dwa strumienie, licznik) | do zrobienia | v3.56.x |

## Zależności, które trzeba znać przed 213

* Trasa 212 nie zna HTML-a ani sesji — jest czystym HTTP z tokenem. Panel (213)
  musi albo wysłać `Authorization: Bearer`, albo dostać osobny, krótko żyjący
  link. To decyzja projektowa, nie implementacyjna — nie podejmuję jej bez
  Twojego zdania.
* Token jest **jeden na całą centralę** (`SECRET_HISTORIA_TOKEN`). Panel
  w przeglądarce nie ukryje takiego tokenu przed kimś, kto ma dostęp do
  komputera. Dlatego 213 nie powinien wklejać tokenu do HTML-a bez
  zastanowienia — lepszy jest przycisk „skopiuj link na 5 minut”.
* 214 ma sens dopiero, gdy 213 pokaże, ile pobrań naprawdę bywa naraz.
  Dziś limit jest jeden (503 na drugie pobranie) i to wystarcza.

## Czego ten plan świadomie NIE obejmuje

* wgrywania czegokolwiek na sterownik — to Twoja decyzja i Twój krok,
* OTA i samoodzyskiwania po nieudanym wgraniu (osobny wątek, nierozwiązany),
* zmiany rytmu 8 s / 60 s, logiki pompy, alarmów i NVS,
* formatu plików CSV i rotacji historii.

## Kolejność sprawdzeń przed każdym wgraniem (ta sama od 209)

1. `python3 -B narzedzia/pobranie-trasa212/build.py` — patch się nakłada,
   oba warianty dają ten sam plik.
2. `python3 -B narzedzia/pobranie-trasa212/run.py` — 3 profile, 0 błędów.
3. `python3 -B narzedzia/pobranie-trasa212/mutacje.py` — wszystkie mutanty
   wykryte (jeśli któryś przeżyje, test jest za słaby, nie kod).
4. Dopiero potem: `patch -p1`, sprawdzenie `FIRMWARE_VERSION`, kompilacja ESP,
   wgranie przez Ciebie.
