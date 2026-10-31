# 201 — powiązanie uploadu z żądaniem i odroczone sprzątanie

**NIE WGRYWAĆ. release_ready=false. Main 3.55.8 niezmieniony.**
To działająca warstwa nad Pool200 w testach komputerowych, nie podłączona trasa HTTP.

## Zmiana

RequestRegistry.h przypisuje jednemu wewnętrznemu kluczowi żądania jeden token.
Nie można ponownie attach z tym samym kluczem i wyzerować istniejącego błędu.
Pierwszy plik otwiera Pool200 tylko raz; drugi plik/index==0 lub zmieniona nazwa
jest odrzucany. Brak pliku i odmowna decyzja autoryzacji nie wykonują I/O.

Disconnect tylko zapisuje informację pod krótką blokadą rejestru. Nie zamyka
pliku ani klienta TCP. Metoda service, wołana w osobnym kwalifikowanym kontekście
roboczym, zwalnia zasoby Pool200. Nie trzymamy blokady rejestru podczas pracy
nośnika. Disconnect może wejść podczas operacji; opublikowany wynik nie zgłasza
wtedy odpowiedzi do wysłania. Slot jest zajęty do końca operacji i sprzątania.

Niepewne zwolnienie pozostaje zachowane i nie jest automatycznie ponawiane.
Generacja zapobiega pomyleniu starego callbacku z nowym żądaniem nawet przy
ponownym wykorzystaniu tego samego adresu/klucza. Po wyczerpaniu generacji slot
jest wycofany. Klucz i token są wyłącznie wewnętrzne, nie stanowią autoryzacji.

## Sprawdzenie

Finalnie `WYNIKI/run004/wynik.json` i `MUTACJE.json`:
- 18 grup, w tym 24 wstrzyknięcia błędu wywołań pod nową warstwą.
- Zwykłe wykonanie i ASan+UBSan, wykrywanie wycieków włączone.
- 9 kompilujących się wadliwych wariantów, wszystkie wykryte.
- Rzeczywiste Pool200, Owner197 i Arduino FS.cpp, modelowane żądania i nośnik.
- Dwa wątki: zapis w trakcie, rozłączenie, odmowa sprzątania zajętego slotu,
  Busy dla równoległego zdarzenia i sprzątanie po zakończeniu operacji.
- Disconnect wywołany również z callbacku modelu zapisu i podczas końcowego close.
- Brak odpowiedzi drugi raz, brak dostępu starego tokenu, 255 generacji małego typu.

24 próby stanowią część 18 grup, nie dodatkowe niezależne grupy. Nie przeprowadzono
TSan ani testu FreeRTOS. Potwierdzenie ZIP zawiera też ponowne testy 200 wykonane
po rozpakowaniu, włącznie z prawdziwym POSIX i jego 15 mutacjami. Wyniki starych
etapów nie zostały nadpisane.

## Ważne granice

- Nie wykonano pełnego parsera multipart, AsyncTCP ani życia prawdziwego request.
- _tempObject nie jest używane: przypięty destruktor request wywołuje free, a nie
  destruktor przechowywanego tam obiektu C++. Audyt źródła: AUDYT.json.
- chunk/complete/service muszą pracować w kwalifikowanym kontekście plikowym.
  **Nie ma tu kolejki kopiującej data z callbacku sieciowego.** Nie zachowywać
  wskaźnika callbacku na później bez własnej kopii o właściwym czasie życia.
- Busy oznacza, że zdarzenie nie zostało wykonane. Adapter musi je uporządkować
  lub odrzucić całe żądanie; nie może po cichu zgubić porcji.
- Wynik Emit nie daje prawa do dereferencji dawnego wskaźnika request. Rozłączenie
  może nastąpić także po powrocie complete. Nadal potrzebny jest most do kontekstu
  sieciowego z ponownym sprawdzeniem życia żądania. Rejestr nie przechowuje ani
  nie zwraca wskaźnika do klienta TCP, nie wywołuje send/close.
- Brak slotu attach wymaga zachowania osobnego błędu przyjęcia dla żądania.
  Nieważny token nie może uruchomić domyślnej odpowiedzi 200.
- Cała pamięć rejestru, Pool200, Owner, blokady i backend wymagają życia do resetu.
- Ograniczenia 200 pozostają: tylko NOWE pliki, domyślnie 4096 B, bez nadpisywania
  i usuwania częściowego pliku. Wymagana długość samego pliku nie jest długością
  formularza multipart. To nie jest jeszcze zgodny zamiennik istniejącej trasy.

Ciągle potrzebne: rzeczywisty adapter żądania/kolejka/odpowiedź sieciowa,
backpressure, limity czasu i miejsca, FreeRTOS/WDT, połączenie wszystkich dróg FS,
kompilacja i kwalifikacja ESP. Nie obiecujemy cofnięcia już wykonanego zapisu ani
potwierdzenia dostarczenia odpowiedzi klientowi.

## Odtworzenie

Linux, Python 3 i g++ z ASan/UBSan, z katalogu głównego paczki:

```sh
python3 -B narzedzia/kontekst-upload201/run.py
python3 -B narzedzia/kontekst-upload201/mutacje.py
```

Bez SDK, urządzenia, OTA, formatowania i remount. Sterowanie, ustawienia, hasła,
8 s/60 s, SERWO% i zasady ochrony nowej historii pozostają bez zmian.
Paczka zawiera zależności i ich licencje. Na początku tego etapu brakujące pliki
odtworzono z dostarczonego ZIP200: zweryfikowano SHA ZIP i 121 wpisów manifestu,
przywrócono 115 brakujących plików bez nadpisania różniących się danych.
