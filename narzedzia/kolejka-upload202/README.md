# 202 — kolejka własnych kopii danych uploadu

**NIE WGRYWAĆ. release_ready=false. Main 3.55.8 bez zmian.**
To kolejny element połączony z 197/200/201 w testach, nie gotowy adapter AsyncTCP.

## Co zamyka ten pakiet

`UploadPump.h` przejmuje własną kopię nazwy i danych, zanim metoda enqueue wróci.
Nie przechowuje wskaźnika do danych callbacku. Dzieli porcję na ograniczone ramki,
zachowując offset i final tylko na ostatniej ramce. Miejsce dla CAŁEJ porcji
sprawdzane jest przed kopiowaniem: przyjęcie połowy bez zgłoszenia błędu jest zabronione.

Brak miejsca kończy dane żądanie błędem 503; nie udajemy przyjęcia i nie obiecujemy
ponowienia nietrwałego wskaźnika. Pozostałe żądanie zachowuje swoje dane. Oczekujący
koniec całego żądania i jego wynik mają miejsce we własnym rekordzie, więc pełna
kolejka danych nie blokuje ich zgłoszenia.

attach/enqueue/end/disconnect nie wykonują operacji plikowych. runOne, wołane
przez jednego konsumenta, wykonuje Registry201/Pool200 poza blokadą kolejki.
Drugi równoczesny konsument otrzymuje Busy, bez wykonania zdarzenia.
Disconnect usuwa oczekujące ramki i odpowiedź, nie zamyka pliku. Sprzątanie robi
konsument. Operacja już wpuszczona może dokończyć zapis; nie cofamy jej skutków.

Wynik jest komendą z tokenem, statusem i liczbą bajtów. Nie zawiera request*,
client* ani callbacku send. Może być odebrany tylko raz. Nie można zresetować
błędu ponownym attach z tym samym kluczem, nawet po sprzątnięciu niższej warstwy.

## Testy

`WYNIKI/run002/wynik.json`, `MUTACJE.json`:
- 23 grupy, w tym 24 przypadki awarii operacji plikowych przez całą nową warstwę.
- Zwykłe wykonanie i ASan+UBSan z włączonym wykrywaniem wycieków; C++11,
  bez wyjątków/RTTI, ostrzeżenia traktowane jako błędy.
- 12 kompilujących się uszkodzonych wariantów, wszystkie wykryte.
- Zmiana nazwy i danych natychmiast po enqueue nie zmienia zapisanego pliku.
- Podział na ramki, porcja 4096 B w domyślnej konfiguracji, pełna kolejka,
  brak przyjęcia części porcji, równoległe żądanie i stary token.
- Dwa wątki: zapis w trakcie, nowe dane drugiego żądania, odmowa drugiemu
  konsumentowi, disconnect i brak późniejszego wyniku dla przerwanego żądania.
- Rzeczywiste komponenty 197/200/201 oraz FS.cpp; żądania/TCP i nośnik modelowane.

24 przypadki są częścią grup, nie dodatkowymi grupami. Testy na komputerze nie
są testem FreeRTOS, AsyncTCP ani ESP. Sanitizery nie zastępują kontroli wyścigów
TSan ani dowodu czasu życia prawdziwego request. Potwierdzenie ZIP zawiera również
ponowne testy etapów 200 i 201 w czystym katalogu.

Domyślnie: 2 rekordy żądań, 16 ramek po 256 B. sizeof samego Pump na hoście:
6048 B. **To nie zużycie RAM na ESP i nie suma wszystkich komponentów.**

## Kontrakt przyszłego adaptera

- Jeden serwerowy token przypisany do jednego żądania. Nie pochodzi od klienta.
- Dane i nazwa muszą być dostępne podczas enqueue. Po powrocie Accepted mogą
  zniknąć. Accepted oznacza przyjęcie do RAM, NIE zapisane dane ani HTTP 200.
- Rejected kończy przyjmowanie tego żądania. Żadnego cichego ignorowania błędu.
- end wołać dopiero dla końca CAŁEGO żądania. Pole final oznacza tylko koniec pliku.
- takeReply wykonywać w prawidłowym kontekście sieciowym z niezależną ochroną
  życia request. Disconnect może nastąpić także po pobraniu wyniku; nie ma tu
  gwarancji, że późniejsze send na dawnym wskaźniku byłoby bezpieczne.
- Konsument musi być rzeczywiście uruchamiany i regularnie wykonywać runOne.
  Ta biblioteka sama nie tworzy zadania, nie budzi go i nie gwarantuje terminu.
- Registry201 i Pool200 nie mogą być równolegle obsługiwane przez innego adaptera.
- Rejestr/kolejka, blokady, backend i magazyny muszą żyć do resetu. Nie niszczyć
  ani rekonstruować obiektów, aby ominąć zatrzymanie lub kwarantannę.

## Nadal niegotowe

Nie podłączono prawdziwych callbacków parsera, budzenia konsumenta, zegarów
ani wysyłania odpowiedzi. Nie jest to implementacja pause/wznawiania TCP.
Przepełnienie oznacza odrzucenie, nie automatyczne wstrzymanie nadawcy.
Nie ma jeszcze deadline dla powolnego klienta i rezerwacji miejsca na nośniku.

Mutex kolejki chroni kopiowanie ograniczonej ilości danych i krótkie operacje
rejestru, nigdy I/O. Czas jego trzymania i opóźnienia konsumenta wymagają pomiarów
na ESP. Limit 256 B nie oznacza ograniczonego czasu fsync/close. Nie wywoływać
metod Pump z posiadaną Lease Owner ani w odwrotnej kolejności blokad.

Pozostają ograniczenia 200: wyłącznie nowe pliki, domyślnie do 4096 B, bez
nadpisania, bez kasowania częściowego pliku. To nie zgodny zamiennik istniejącej
trasy. Długość pliku nie może być wzięta z Content-Length całego multipart.
Nie zmieniono zasad A, ustawień, sterowania, 8 s/60 s, SERWO% ani dostępu zdalnego.

## Odtworzenie

Linux, Python 3, g++ z ASan/UBSan. Z głównego katalogu paczki:

```sh
python3 -B narzedzia/kolejka-upload202/run.py
python3 -B narzedzia/kolejka-upload202/mutacje.py
```

Bez SDK i połączenia z urządzeniem. Licencje zależności dołączone w wcześniejszych
katalogach paczki. Manifest i oddzielne potwierdzenie ZIP pozwalają sprawdzić dane.
