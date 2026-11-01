# 204 — ograniczona pula obsługi uploadu

**NIE WGRYWAĆ. release_ready=false.** Kod komponentowy do dalszej integracji; brak zmian centrali 3.55.8 i SDK. Nie wykonano operacji na urządzeniu.

## Co zmieniono

`UploadAdapter.h` zastępuje potrzebę tworzenia osobnego `DeferredUploadReply` dla każdego połączenia. Łączy kolejkę 202 z hookami requestu 203, obsługując również okres odbierania pliku. Domyślnie dwa stałe konteksty; zero alokacji kontekstów na połączenie. Nie zastępuje callbacków AsyncClient. Nie używa `_tempObject`.

- `begin(request, expected, authorized, timeout)` rezerwuje miejsce, przyjmuje token kolejki i instaluje hook. Zwraca kod oraz wewnętrzny uchwyt adaptera (miejsce + generacja). Odmowa hooka anuluje token kolejki. Nie pauzuje odbioru.
- `chunk(handle, request, name, nameLength, index, data, length, final)` przekazuje kopię do kolejki 202. Accepted oznacza kopię w RAM, nie zapis na dysku. Uchwyty nie są poświadczeniami bezpieczeństwa ani danymi od klienta; przechowuje je integracja serwera. Obcy request lub stary numer generacji są odrzucane.
- `end(handle, request)` oznacza **koniec całego requestu**, nie koniec ostatniego callbacku pliku. Dopiero tutaj wywołuje `pause()`. Duplikat niczego nie przyjmuje; późniejsze porcje są odrzucane.
- `poll` odbiera gotowy wynik dopiero po end i przy możliwości wysyłania. Fabryka odpowiedzi działa wyłącznie w kontekście sieciowym. Własność utworzonej odpowiedzi chroniona także przy rozłączeniu i obcej odpowiedzi podczas działania fabryki.
- `disconnect` anuluje metadane kolejki, bez otwierania/zamykania plików w callbacku. Miejsce można ponownie wykorzystać dopiero po disconnect **oraz wygaśnięciu weak_ptr**. Wyczerpanie uint64 generacji wyłącza miejsce zamiast zawinięcia (warunek sprawdzony w kodzie, brak testu 2^64 cykli).

Kody begin: Accepted, Invalid, Forbidden, Full (brak miejsca adaptera), Unavailable (odmowa kolejki), Conflict (już przypisane żądanie/hook). Po odmowie adapter nie wysyła odpowiedzi i nie przejmuje requestu. Integracja musi zakończyć żądanie odpowiednim błędem w kontekście sieciowym, bez nadpisania cudzej odpowiedzi. Przy Conflict z istniejącym właścicielem decyzja należy do niego. Adapter nigdy nie formatuje pamięci, nie usuwa plików i nie ponawia niepewnego close.

## Kontrakty obowiązkowe

Adapter, Pump i Services mają trwały czas życia. Destruktor adaptera jest usunięty; produkcja potrzebuje poprawnej inicjalizacji trwałego obiektu. Wszystkie begin/chunk/end oraz callbacki sieciowe są serializowane w jednym kontekście. Tylko pracownik uruchamia `Pump::runOne()`; nie otrzymuje request/client. Nadal obowiązuje porządek blokad 197–202. Sam adapter nie jest ogólnie bezpieczny do wywołań z wielu wątków.

Services::now zwraca uint32 milisekund; timeout w 1..0x7fffffff. To limit **całego przyjętego żądania**, liczony od begin, bez odświeżania przez kolejne porcje. Sprawdzany na poll, również podczas odbioru i przy canSend=false. Nie gwarantuje terminowego przerwania, gdy zdarzenia sieciowe nie dochodzą. Żądanie po send nadal wymaga disconnect, aby zwolnić kontekst; przypięte request.abort po send nie działa. To nie naprawa anulowania aktywnego downloadu.

Services::makeResponse ma tworzyć małą odpowiedź w pamięci, bez File/I/O. Zwraca własność wskaźnika albo nullptr. Test nullptr nie kwalifikuje wszystkich awarii `new`/String na ESP. Adapter nie obsługuje clientRelease/SSE/WebSocket. `installDeferred203` nie wywołuje callbacku synchronicznie, zgodnie z badanym patchem.

Autoryzacja ma zostać zakończona **przed begin**; false nie rezerwuje żadnych zasobów. expected oznacza wiarygodną liczbę bajtów pliku, nie nagłówek Content-Length multipart. Wciąż obowiązują ograniczenia 200: tylko nowy plik O_EXCL, do 4096 bajtów, płaska nazwa ASCII do 63 znaków, brak nadpisania/wznowienia/usuwania. Przepełnienie kolejki to terminalne 503, nie backpressure. Zdrowe anulowanie może pozostawić częściowy nowy plik; niepewne I/O pozostaje objęte kwarantanną niższej warstwy.

## Dowody i ich granica

Test.cpp: 29 grup, native i ASan+UBSan, kontrola wycieków włączona. 12 celowych mutacji skompilowanych i wykrytych. Rzeczywiste Pump202/Registry201/Pool200/Owner197/FS.cpp. `Fixture203.inc` jest dokładnym prefiksem testu 203: wyodrębnione rzeczywiste metody requestu, proponowane hooki poll/disconnect, **modelowany konstruktor/pola, klient TCP i odpowiedź**. run.py weryfikuje zgodność prefiksu.

Testy obejmują 50 kolejnych żądań na tej samej puli, dwa jednoczesne żądania, przeciążenie dwóch warstw, stare/obce/nieprawidłowe uchwyty, rozłączenie przed end i w trakcie pracy zapisu, oczekiwanie na zniszczenie requestu, odmowę hooka, uprawnienia, błędy danych i I/O, timeout i zawinięcie zegara, niezależność final/end, fabrykę odpowiedzi, brak pamięci sygnalizowany nullptr, reentrancję i obcą odpowiedź. Pracownik w testach nie wykonuje wywołań sieciowych.

Host sizeof(adapter)=144 B przy dwóch miejscach nie jest pomiarem RAM na ESP. Nie obejmuje kolejki, requestów, buforów parsera i stosów. Brak kwalifikacji czasu kopiowania, fsync, blokad, WDT oraz przerw zasilania. Permanentne korzenie fixture pozostają celowo żywe; obiekty request/response są zwalniane i sprawdzane, LSan nie wyłączono.

Nie testowano rzeczywistego parsera multipart, jego callbacków i zachowania po odmowie admission. Nie ma produkcyjnego przypisania uchwytów do routingu ani planowania FreeRTOS. Biblioteka 203 zmienia layout requestu: cała musi być przebudowana, bez mieszania starych obiektów.

## Odtworzenie offline na Linux (Python 3 i C++11)

Z katalogu głównego rozpakowanej paczki:

```sh
python3 -B narzedzia/adapter-upload204/run.py
python3 -B narzedzia/adapter-upload204/mutacje.py
python3 -B narzedzia/odpowiedz-odroczona203/run.py
python3 -B narzedzia/kolejka-upload202/run.py
```

To testy komputera, nie komendy do wgrania firmware. Wynik ma być PASS; nowe wyniki run.py trafiają do nowego katalogu runNNN, nie nadpisują wcześniejszych prób.

## Co dalej

Zamknięto komponentową pulę kontekstów i jej powiązanie z etapami przyjęcia/porcji/końca requestu. Pozostają: rzeczywisty parser i routing oraz planowanie pracownika; niezmienne pobieranie podczas dopisywania/rotacji; połączenie z całym programem (surowe ścieżki I/O, blokady, RAM, WDT); pełny build ESP i kwalifikacja na sprzęcie. To obszary prac, nie liczba kolejnych paczek. Brak podstaw do uczciwego terminu zakończenia.
