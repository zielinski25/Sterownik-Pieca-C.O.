# 205 — rzeczywisty parser body multipart i rygorystyczne zakończenie

**NIE WGRYWAĆ. release_ready=false.** Brak zmian centrali 3.55.8, SDK i urządzenia. To proponowany, opcjonalny tryb biblioteki, nie gotowy firmware ani pełny serwer HTTP.

## Znaleziony problem i dowód

Przypięty ESPAsyncWebServer c47917dcaba2acefb04277cd679f58a60b4fab30 zgłasza `handleUpload(..., final=true)` po rozpoznaniu tekstu boundary, **przed sprawdzeniem sufiksu**. Następnie potrafi zmniejszyć `_contentLength`. `_onData` wywołuje koniec requestu, gdy jego licznik osiąga tę długość; samo wywołanie handlera nie oznacza poprawnego formularza.

Profile original-native/original-asan wykonują niezmienione, wyodrębnione `_onData`, `_handleUploadByte`, `_parseMultipartPostByte`. W połączeniu z rzeczywistym Adapter204/Pump202/Registry201/Pool200 oraz modelowanym handlerem trzy przypadki prowadziły do 200:

1. Zakończenie `-XYZ` zamiast `--\r\n`.
2. Koniec body bez czterech ostatnich bajtów, z odpowiednio krótszą deklaracją długości.
3. Deklaracja dłuższa o 50 bajtów — parser sam skracał długość do odebranego zamknięcia.

Nie jest to twierdzenie o wyniku w aktualnej centrali: jej route nie został przełączony na ten tor. To odtworzona luka w rozumowaniu, że final + handleRequest wystarczają do potwierdzenia kompletności.

## Poprawka ograniczona do nowego trybu

- `enableStrictMultipart205(maxBody)` włącza tryb tylko po nagłówkach, przed body, dla multipart z niepustą granicą do 70 znaków, z Content-Length w dozwolonym limicie, bez chunked. Próba po odebraniu danych, po pauzie/wysłaniu/rozłączeniu albo ponownego włączenia jest odrzucana.
- Protokół celowo węższy niż ogólny multipart: **dokładnie jedna część plikowa**, z niepustymi name i filename; bez pól tekstowych, kolejnego pliku ani epilogu w zadeklarowanej długości. Wymaga końcowego `--\r\n`, również końcowego CRLF. Nie jest ogólną walidacją wszystkich nagłówków MIME.
- Final zostaje wstrzymany do zweryfikowania wszystkich czterech bajtów sufiksu i dokładnego końca zadeklarowanego body. W tym trybie nie wolno zmniejszać Content-Length.
- Błędny parser albo osiągnięcie długości body bez poprawnego zamknięcia przerywa połączenie. Po rozłączeniu/zakończeniu wysyłania nie kontynuuje callbacków parsera. Nie obiecuje odpowiedzi HTTP 400: dla błędnego framingu celowo abort.
- `StrictMultipartEnd.h` sprawdza potwierdzony koniec w whole-request callbacku przed `Adapter204::end`. Nie zastępuje kontroli uprawnień ani routingu.
- Domyślny tryb biblioteki pozostaje bez tej zmiany zachowania. Test poprawnego legacy uploadu przechodzi. Istniejące szerokie zachowania legacy nie są tu naprawiane ani w pełni regresyjnie kwalifikowane.

Pełny tekst granicy występujący w danych, ale z niewłaściwym sufiksem, jest w rygorystycznym protokole odrzucany, nie przepisywany do pliku. Krótsze, niedopasowane prefiksy pozostają danymi i są testowane. Należy dobierać granicę nieobecną w pliku.

**Bytes beyond Content-Length:** oryginalne `_onData` nadal odrzuca nadmiar bajtów dostarczonych po zadeklarowanym końcu body. Test pokazuje to jawnie. Nie kwalifikowano pipeline HTTP, przemycania żądań, zgodności proxy ani interpretacji sprzecznych nagłówków. Natomiast epilog w obrębie zadeklarowanego body jest odrzucany.

## Co rzeczywiście wykonano

`build.py` weryfikuje sumę przypiętego WebRequest.cpp i tworzy:
- `ParserOriginal.inc` — niezmienione trzy metody parsera i enum multipart;
- `Parser205.inc` — odpowiadające im metody kandydata oraz nowe sprawdzenia;
- `candidate/library.patch` — łączna poprawka 199 + 203 + 205 do czystych źródeł;
- pełne cztery pliki biblioteki do porównania bajtowego po zastosowaniu patcha.

Hostowe testy wywołują rzeczywiste `_onData` z surowym body multipart. Rzeczywiste callbacki parsera trafiają do Adapter204, następnie Pump202/Registry201/Pool200/Owner197/FS.cpp. Sprawdzono także bajty pliku w modelowanym magazynie, a nie tylko liczbę callbacków.

**Modelowane pozostają** String/Stream, otaczające pola i konstruktor requestu, klient TCP/odpowiedź, nagłówki HTTP, wybór handlera i middleware. `ParserFields.inc` celowo kończy błędem próbę wejścia do parsera nagłówków, plain post albo chunked. `Model203.inc` rozwija model 203, a nie pełną klasę biblioteki. Limity pamięci/awarie String na ESP nie są odwzorowane. `malloc` bufora parsera można celowo zawieść; poza tą jedną iniekcją korzysta z rzeczywistego malloc/free, kontrolowanego przez ASan. Bufor parsera ma przypięty rozmiar 1460 bajtów.

## Wyniki

Końcowy run004: **38 grup, 554 instancje testowe** w native i ASan+UBSan (LSan włączony). Dodatkowo trzy reprodukcje zachowania oryginału w native i ASan+UBSan. **12 skompilowanych mutacji wykrytych**.

Zakres obejmuje każdy podział krótkiego body na dwie porcje, każdy przedwczesny deklarowany koniec, pojedyncze bajty, puste i binarne pliki, 4096 bajtów, granice długości 1/70, kolejkę bez uruchomionego pracownika (503), błędne/pominięte sufiksy, dłuższą deklarację, drugi plik/pola, prefiksy boundary, timeout urwanego transportu, utratę połączenia i zniszczenie requestu wewnątrz callbacku, brak bufora i odmowę nieprawidłowego włączenia trybu. W tej serii pracownik jest wykonywany pomiędzy porcjami lub po body; rzeczywista współbieżność pracownika pozostaje testem 204, nie nowym dowodem planowania FreeRTOS.

Wcześniejszy run001 zawiera błąd kompilacji modelu (brak listy parametrów i ostrzeżenie o kopiowaniu w pętli); pozostawiony dla śladu. run002/003 to wcześniejsze przechodzące wersje testów. Końcowy wynik jest wskazany powyżej i w AUDYT.json.

## Zasady późniejszej integracji

Włączać wyłącznie dla osobnego, autoryzowanego toru pojedynczego **nowego pliku**, nie globalnie i nie dla OTA, konfiguracji, websocketów lub innych formularzy. Handler musi być rzeczywiście nietrywialny. Limit body użyty w testach to 8192 B; osobna wiarygodna długość pliku trafia do adaptera (maksimum Pool200: 4096 B). Content-Length formularza nie jest długością pliku. W razie odmowy włączenia/admission routing musi przerwać lub bezpiecznie obsłużyć błąd; nie wolno kontynuować zapisu bez kontroli.

Początek/przyjęcie requestu i przypisanie handle są wciąż modelowane. `Test.cpp::Harness` pokazuje połączenie callbacków, ale **nie jest produkcyjnym handlerem**. Brak parsera nagłówków, autoryzacji z prawdziwego serwera, rezerwacji w prawdziwym routingu i FreeRTOS dispatchera. Zmiana layoutu requestu wymaga pełnego przebudowania biblioteki i wszystkich zależnych obiektów.

Nie jest to transakcja plikowa. Porcje mogą być zapisane przed wykryciem błędnego zamknięcia; anulowanie nie usuwa częściowego nowego pliku. Nie nadpisuje istniejących danych, nie formatuje, nie ponawia niepewnego close. Wciąż obowiązują kwarantanna błędów I/O, trwały czas życia adaptera i porządek blokad poprzednich etapów. Abort po już wysłanej odpowiedzi pozostaje ograniczeniem biblioteki. Timeout zależy od kolejnych zdarzeń poll.

## Odtworzenie

Z katalogu głównego rozpakowanej paczki, Linux + Python 3 + C++11:

```sh
python3 -B narzedzia/parser-upload205/run.py
python3 -B narzedzia/parser-upload205/mutacje.py
python3 -B narzedzia/adapter-upload204/run.py
```

Nie są to komendy wgrywania. Brak pełnego buildu ESP i kwalifikacji na urządzeniu, pomiaru RAM/WDT i testów zaników zasilania. Pozostaje także niezmienność pobieranej historii podczas dopisywania/rotacji i integracja wszystkich ścieżek I/O całej centrali. Nie ma podstaw do terminu ukończenia firmware.

Źródło: https://github.com/ESP32Async/ESPAsyncWebServer/blob/c47917dcaba2acefb04277cd679f58a60b4fab30/src/WebRequest.cpp — lokalna przypięta kopia i metadane SHA w zależnościach 199. Kod biblioteki LGPL-3.0-or-later, licencje zachowane w paczce.
