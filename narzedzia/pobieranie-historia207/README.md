# 207 — pobieranie historii podczas dopisywania i rotacji

**NIE WGRYWAĆ. release_ready=false.** Bez zmian centrali 3.55.8, SDK i urządzenia. To komponent offline,
nie gotowe pobieranie przez HTTP.

## Problem, którego 199 nie zamykał

199 sprawdził odczyt i końcówkę odpowiedzi, ale plik historii mógł się w tym czasie zmieniać. Odczyt po
nazwie przy jednoczesnej zmianie nazwy albo skróceniu pliku daje klientowi zmieszaną lub urwaną treść,
a `CheckedFileBody` nie ma jak tego rozpoznać — dla niego to nadal poprawnie zakończony odczyt.

## Rozwiązanie

`Snapshots.h`: przyjęcie pobrania w kontekście sieciowym, pod **tym samym zamkiem**, którego używają
dopisywanie i rotacja:

- długość (`committed()`) jest odczytywana razem z nazwą pliku pod zamkiem — pin obejmuje dokładny,
  potwierdzony prefiks, a nie „plik, jaki będzie później”;
- plik jest otwierany **raz**, na stałej nazwie, i czytany tylko do zapamiętanej długości;
- dopisywanie może trwać dalej — nowe bajty nie wchodzą do trwającego pobrania;
- **rotacja jest odroczona**, dopóki trwa choć jedno pobranie (`beginRotation()` odmawia); stary plik nie
  jest zmieniany, zmieniany nazwą ani usuwany pod czytającym. Autor rotacji musi spróbować później;
- podczas rotacji nowe pobrania są odrzucane (`Unavailable`);
- pula jest ograniczona (2 pobrania), uchwyty mają pokolenie — stary uchwyt po zwolnieniu miejsca jest
  odrzucany;
- termin ważności pobrania (`timeout`) i `expire(now)` uwalniają miejsce, żeby zawieszony klient nie
  blokował zapisu historii na zawsze. Po terminie odczyt zwraca `Stale`, nigdy „koniec”.

Wyniki odczytu: `Data`, `End`, `Stopped`, `Invalid`, `Stale`. **Błąd ani zatrzymanie nigdy nie są zwracane
jako poprawny koniec**, więc wywołujący nie może wysłać krótszego pliku jako sukcesu.

Decyzja użytkownika A obowiązuje: stara historia może zostać odrzucona przy rotacji, ale nowe pomiary muszą
dalej być zapisywane i pobieralne. Test tego pilnuje — po rotacji powstaje nowy plik, dopisywanie zaczyna
się od zera, a nowe pobranie widzi wyłącznie nowe dane.

## Co rzeczywiście wykonano

- Rzeczywisty `RuntimeFilesystem.h` z 197 (właściciel plików, `Lease`, kontrola zatrzymania) i rzeczywisty
  Arduino `FS.cpp`. Otwarty plik, `seek`, `read`, `rename`, `remove`, `close` to prawdziwe wywołania.
- Backend plików jest modelowany (mapa nazwa→zawartość), tak jak w 197/198/199. **Model nie realizuje
  `O_APPEND`** — test jawnie ustawia pozycję na końcu przed zapisem. To ograniczenie modelu.
- Sprawdzenie z `CheckedFileBody` z 199: dla znanej długości zwraca dokładnie zapamiętany prefiks, mimo
  dopisywania w trakcie.
- 23 grupy testów w native i ASan+UBSan z LSan. **13 skompilowanych mutacji wykrytych.**
- Zakres: pusty plik, prefiks przy dopisywaniu, porcje 1/7/4096 B, rotacja odroczona przy jednym i dwóch
  pobraniach, rotacja bez czytelników, odmowa w trakcie rotacji, limit puli i pokolenia, uchwyty obce/zerowe/
  po zwolnieniu, termin i `expire`, skrócony plik (koniec≠sukces), zatrzymanie magazynu, 50 kolejnych pobrań,
  dopisywanie między odczytami, zerowy bufor, przycięcie do długości pinu, powtórzony `End`, dwukrotne wejście
  w rotację, błąd krótkiego zapisu, źródło niegotowe.

## Granice — ważne przed użyciem

- Ochrona dotyczy **wyłącznie pobrań przyjętych przez `Snapshots`**. Każde inne miejsce, które otwiera plik
  historii bezpośrednio, nie jest chronione i może dostać zmieszaną treść.
- Nie ma tu warstwy HTTP: ktoś musi zamienić `Stopped`/`Stale` na przerwanie połączenia, a `Data`/`End` na
  wysyłkę. Podłączenie do 199/203 jest pokazane tylko w teście.
- Rotacja jest odroczona, nie kolejkowana: wywołujący musi ponowić próbę. Jeśli klient nigdy nie zamknie
  pobrania, miejsce zwolni dopiero termin (`timeout`). Wartość terminu musi dobrać integrator.
- Modelowany backend nie ma inode'ów: nie wolno wyciągać wniosku, że otwarty uchwyt przeżyje `rename`.
  Dlatego rotacja **czeka**, zamiast przenosić nazwę pod czytającym.
- Brak: całej biblioteki i serwera HTTP, AsyncTCP, FreeRTOS, ESP, RAM/WDT, braku zasilania, usuwania plików
  tymczasowych, zakresów HTTP (`Range`), kompresji, szablonów i niezmienności przy zewnętrznym uszkodzeniu
  pliku przez inne zadanie.

## Odtworzenie

Linux, Python 3, C++11:

```sh
python3 -B narzedzia/pobieranie-historia207/run.py
python3 -B narzedzia/pobieranie-historia207/mutacje.py
```

Finalny raport: `WYNIKI/run001/wynik.json`; mutacje: `MUTACJE.json`. Nie są to komendy wgrywania.
