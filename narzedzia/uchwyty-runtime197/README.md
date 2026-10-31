# Etap 197 — zatrzymanie dostępu do plików po błędzie podczas pracy

**NIE WGRYWAĆ. release_ready=false. To osobny komponent i testy na komputerze,
nie nowa wersja programu sterownika. Program 3.55.8 pozostaje bez zmian.**

## Co powstało

`RuntimeFilesystem.h` zapewnia wspólną kontrolę operacji na plikach, również gdy
kod skopiował obiekt File albo FS. Po zgłoszeniu błędu nowe operacje nie są
wpuszczane. Otwartych plików o niepewnym stanie nie zamykamy ponownie ani nie
zwalniamy przez destruktor. Pozostają zatrzymane do końca życia programu.
Nie ma tu automatycznego ponownego uruchomienia pamięci, formatowania ani kasowania.

Operacja wpuszczona przed sygnałem zatrzymania może jeszcze zakończyć pracę i
zmienić dane. `drained()` czeka na jej wyjście. To NIE jest cofnięcie zapisu ani
natychmiastowe anulowanie. Dopiero po opróżnieniu tego wejścia nie powinny być
wykonywane kolejne operacje przez chroniony interfejs.

Miejsce na uchwyt i blok zarządzający jest rezerwowane przed otwarciem pliku.
Liczba takich miejsc jest ograniczona; ich brak nie otwiera kolejnego pliku.
Sam system plików nadal może przydzielać pamięć. Domyślne 10 miejsc i 128 bajtów
ścieżki wymagają dopasowania do rzeczywistego programu, nie są pomiarem potrzeb ESP.

`CheckedRead.h` rozróżnia dane, koniec pliku, zatrzymanie i nieprawidłowy wynik.
Nie jest implementacją pobierania HTTP. Wynik zatrzymania/błędu wymaga przerwania
odpowiedzi przez przyszłego odbiorcę, nie potwierdzenia poprawnego pobrania.

## Po czym poznajemy, że komponent działa

- 32 grupy testów: zwykłe wykonanie oraz ASan+UBSan, z włączonym wykrywaniem wycieków.
- Prawdziwy, przypięty Arduino `FS.cpp`; modelowane urządzenie i Arduino String/Stream.
- Kopie File/FS, zatrzymanie podczas operacji, brak ponownego zamknięcia po błędzie,
  odroczone zwalnianie bloku przez weak_ptr, ograniczenie puli i odczyt katalogu.
- 1000 kolejnych ponownych użyć oraz 4 wątki po 250 cykli; to nie kwalifikacja FreeRTOS.
- 14 celowo uszkodzonych wariantów kompiluje się i zostaje wykrytych przez testy.
- Sprawdzenie składni C++11 z prawdziwym nagłówkiem LittleFS i zakazem niszczenia/kopiowania Owner.
- Wyodrębniona rzeczywista metoda `_fillBuffer` potwierdza przekazywanie znacznika
  błędu File.read. Pozostały transport HTTP nie był w tym etapie uruchamiany.

Wynik bazowy: `WYNIKI/run004/wynik.json`, `MUTACJE.json`, `AUDYT.json`.
Starsze katalogi wyników dokumentują rozwój, nie są dodatkowymi testami finalnej wersji.
Rozmiar Owner na tym komputerze: 2240 B (4 miejsca), 5120 B (10 miejsc).
**To nie jest rozmiar ani zużycie pamięci na ESP.**

## Dlaczego jeszcze nie podłączono tego do sterownika

Szczegóły: `INTEGRACJA.md`. Najważniejsze:
1. Historia używa również osobnego dostępu do plików. Musi przechodzić przez tę samą kontrolę.
2. Trzeba bezpiecznie przerwać trwające pobieranie, a nie pozostawić ciągłe oczekiwanie.
3. Wysyłanie pliku nie może zgłosić sukcesu po pominięciu danych.
4. Potrzebne są prawdziwe blokady FreeRTOS, określona kolejność blokad, pomiary czasu
   i pamięci, kompilacja ESP oraz późniejsza kwalifikacja na urządzeniu.

Nie dowiedziono jeszcze ciągłego zapisu i pobierania nowej historii w całym programie.
Nie zmieniono sterowania, 8 s/60 s, SERWO%, dostępu zdalnego ani ustawień.
Zgoda A dotyczy starej historii, nie formatowania pamięci ani usuwania nowej historii po awarii.

## Odtworzenie na komputerze

W katalogu głównym rozpakowanej paczki (Python 3, g++, Linux, pthread, ASan/UBSan):

```sh
python3 -B narzedzia/uchwyty-runtime197/audit.py
python3 -B narzedzia/uchwyty-runtime197/run.py
python3 -B narzedzia/uchwyty-runtime197/mutacje.py
```

Nie są to polecenia do wgrywania ani instrukcja instalowania SDK. Audyt porównuje
SHA referencyjnego main 3.55.8 i źródeł bibliotek. Manifest paczki obejmuje wszystkie
pliki z wyjątkiem samego manifestu. Potwierdzenie odtworzenia ZIP jest osobnym plikiem.

## Licencje i granice modelu

Przypięte Arduino FS: commit b4f85baa2d4a251eb435e0f78fcdda1fb193791f;
ESPAsyncWebServer: c47917dcaba2acefb04277cd679f58a60b4fab30.
Adresy i sumy źródeł znajdują się w katalogach upstream. Kopie licencji w LICENSES;
GPL-3/LGPL-3 pochodzą z `/usr/share/common-licenses` środowiska testowego.
`WebFill.inc` jest wyodrębnioną metodą LGPL-3.0-or-later, otoczoną klasą modelową w teście.
Nie kompilowano całej biblioteki HTTP ani programu ESP.
Sanitizery nie zastępują dowodu życia obiektów w własnej puli pamięci. Backend,
blokada, Owner i jego magazyn muszą istnieć przez cały czas działania programu.
Testowe obiekty są celowo stale osiągalne z globalnej tablicy; nie wyłączono LSan.
