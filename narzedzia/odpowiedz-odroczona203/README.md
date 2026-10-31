# 203 — wynik zapisu odbierany w kontekście żądania

**NIE WGRYWAĆ. release_ready=false. Centrala 3.55.8 bez zmian.**
To połączenie komponentów w teście i propozycja poprawki biblioteki, nie działająca
trasa na ESP ani pełna kwalifikacja HTTP/AsyncTCP.

## Co przybyło

`DeferredUploadReply.h` odbiera wynik kolejki 202 dopiero w wywołaniu należącym
do żądania. Pracownik plikowy nie dostaje request/client i nie wysyła odpowiedzi.
Żądanie jest wstrzymane przez rzeczywisty kod pause. Po otrzymaniu wyniku tworzymy
odpowiedź i przekazujemy ją do rzeczywistych send/_send. Kod sieciowy nie wykonuje
sprzątania plików — disconnect tylko unieważnia token; robi to później konsument.

Proponowany hook jest wewnątrz istniejącego request::_onPoll, przed canSend.
**Nie podmieniamy callbacku onPoll należącego do AsyncClient.** Hook jest pojedynczy,
rejestracja nie nadpisuje poprzedniego. Zabezpieczenie przed ponownym wejściem
wyklucza rekurencyjne wykonanie. Podczas callbacku istnieje mocna referencja request,
więc synchroniczne rozłączenie nie usuwa obiektu przed końcem używającej go funkcji.

Hook disconnect zachowuje istniejący callback użytkownika i jego możliwość
shared_from_this. Nie korzystamy z _tempObject. Sprawdzamy też sytuację, gdy
fabryka odpowiedzi rozłącza klienta lub wysyła inną odpowiedź: nasz nieprzekazany
obiekt zostaje zwolniony, zamiast wyciec po odrzuceniu przez send.

Stan oczekiwania ma limit czasu sprawdzany także przy canSend=false. **To limit
sprawdzany na zdarzeniu poll, nie niezależny zegar gwarantujący przerwanie w terminie.**
Jeżeli pętla sieciowa nie dostarcza zdarzeń, ten kod nie może sam się wykonać.

Kontekst można ponownie uzbroić metodą rearm, ale dopiero po disconnect i faktycznym
zniszczeniu starego request. Sama informacja o rozłączeniu nie wystarcza, jeżeli
ktoś nadal trzyma mocną referencję. Słaba referencja nie przedłuża życia request.
Sprawdzono ponowne użycie jednego kontekstu dla kolejnych 21 żądań.

## Co naprawdę testowano

Finalny raport: `WYNIKI/run004/wynik.json`, mutacje: `MUTACJE.json`.
- 25 grup: zwykłe wykonanie oraz ASan+UBSan, LSan włączony.
- 13 kompilujących się uszkodzonych wariantów wykrytych w profilu ASan+UBSan.
  Usunięcie mocnej referencji w poll rzeczywiście wywołało wykryty use-after-free.
- Oryginalne ciała metod fabryki request, pause, send, _send, onDisconnect oraz
  destruktora. Dwie metody (poll/disconnect) mają proponowane zmiany.
- Oryginalny destruktor naprawdę usuwa modelowanego klienta i odpowiedź, nie tylko
  zwiększa licznik. Testy sprawdzają wygaśnięcie weak_ptr i bilans odpowiedzi.
- Realne 197/200/201/202 i FS.cpp; pracownik zapisuje, a poll na innym wątku
  wykonuje odpowiedź. Asercje wykrywają wywołanie modelu sieci z pracownika.
- Odmowa uprawnień i błąd I/O pozostają odpowiednio 403/500; brak odpowiedzi z fabryki
  prowadzi do abort, nie do „OK”. Fabryka jest kontrolowanym modelem, patrz niżej.
- Czekanie bez możliwości wysyłania, granica czasu i zawinięcie uint32_t.
- Zniszczenie request przy trwającym zapisie, reentrancja, zastana odpowiedź,
  wcześniejsze pause, odmowa ponownej rejestracji i kontrola ponownego użycia kontekstu.

**Nie kompilowano całej klasy i całej biblioteki dla ESP.** Pola klasy i konstruktor
w teście są modelem otaczającym wyodrębnione rzeczywiste ciała metod. AsyncClient,
wejściowa pętla zdarzeń i sama odpowiedź HTTP są modelowane. Nie ma rzeczywistej
serializacji nagłówków/TCP. Wywołujemy metody poll bezpośrednio w kontrolowanym
kontekście. To silniejszy test życia obiektu niż sam licznik close, ale nie dowód
poprawności całego stosu ESP ani wszystkich jego callbacków.

## Ważne odkrycie: to nie jest naprawa aktywnego pobierania

Przypięte request.abort wykonuje abort klienta tylko, gdy _sent=false. Po wysłaniu
odpowiedzi jest bezczynne. Jest to osobno sprawdzone testem. Dlatego 203 służy
oczekiwaniu na odpowiedź uploadu, a nie odwołaniu już trwającego downloadu z 199.
Nie można odwołać bajtów już oddanych TCP ani cofnąć zapisu wpuszczonego przed timeout.

## Warunki przyszłego użycia

- Wszystkie metody pomocnika i hooki tylko w serializowanym kontekście sieciowym.
  _this i pola stanu nie są ogólnym interfejsem thread-safe. Pracownik używa wyłącznie 202.
- Bind dopiero dla końca całego żądania, które nie ma wcześniejszej odpowiedzi,
  nie zostało wysłane ani wstrzymane przez innego właściciela. Token i klucz są
  serwerowe i muszą należeć do tego samego request. Nie są uprawnieniami klienta.
- Kontekst, Services i Pump muszą przetrwać callback disconnect. Aplikacja musi
  zapewnić ograniczoną pulę kontekstów; test rearm nie tworzy kompletnego adaptera trasy.
  Gdy rearm/bind zawiedzie po przyjęciu tokenu, adapter musi ten token anulować.
- Services.now to monotoniczne uint32_t milisekund; timeout w zakresie 1..0x7fffffff.
- Services.makeResponse ma tworzyć małą odpowiedź pamięciową, bez File/I/O.
  Test obsługuje jej jawny wynik nullptr; nie dowodzi bezpieczeństwa wszystkich
  alokacji String/new w prawdziwej fabryce ESP. Nie wolno wywoływać clientRelease
  ani przekazywać połączenia SSE/WebSocket w tym protokole. Te ścieżki nie są kwalifikowane.
- Nie wywoływać kodu sieciowego pod Owner/logMutex. Oczekiwanie nie dodaje retry
  zapisu, remount ani kasowania częściowego pliku.

Nadal potrzebne: podłączenie rzeczywistego parsera i pełnej trasy, budzenie
konsumenta, rezerwacje pamięci/miejsca, polityka dużych plików i nadpisywania,
pełny audyt pozostałych callbacków, FreeRTOS/WDT i testy całej biblioteki/ESP.
Ograniczenia 200 pozostają: tylko nowe pliki, domyślny limit 4096 B.

## Patch i odtworzenie

`candidate/library.patch` łączy zmiany 199 i 203 dla jednego przypiętego commitu
ESPAsyncWebServer `c47917dcaba2acefb04277cd679f58a60b4fab30`. Dotyka czterech plików.
To **nie** gotowa aktualizacja bibliotek używanych do wgrywania. Zmienia układ klasy
request, więc nie wolno mieszać nowych nagłówków ze starymi obiektami skompilowanymi.
Ścieżka szablonów z 199 nadal nie jest kwalifikowana.

Linux, Python 3, g++ z ASan/UBSan, z głównego katalogu paczki:

```sh
python3 -B narzedzia/odpowiedz-odroczona203/run.py
python3 -B narzedzia/odpowiedz-odroczona203/mutacje.py
```

build.py sprawdza sumy przypiętych źródeł i odtwarza propozycję patcha. Weryfikację
zastosowania patcha do czystych źródeł i powtórzenia testów zawiera potwierdzenie ZIP.
Wyodrębniony kod biblioteki ma LGPL-3.0-or-later; źródła i licencje są dołączone.
Starsze raporty dokumentują rozwój, nie kolejne dowody finalnej wersji.
Nie zmieniono urządzenia, SDK, main 3.55.8, ustawień, 8 s/60 s, SERWO% i zgody A.
