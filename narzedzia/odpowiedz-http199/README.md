# 199 — kontrola błędu pobierania i końcówki odpowiedzi

**NIE WGRYWAĆ. release_ready=false. Bez zmiany centrali 3.55.8.**
To propozycja poprawki przypiętej biblioteki i osobna odpowiedź plikowa,
sprawdzone na komputerze, nie gotowa aktualizacja całego serwera/ESP.

## Co powstało

1. `CheckedFileBody.h`: odczyt tylko zadanej liczby bajtów, odróżnienie końca
   od przerwania i zbyt krótkiego pliku. Nie oddaje znacznika błędu jako liczby bajtów.
2. `CheckedFileResponse.h`: prototyp odpowiedzi o znanej długości, bez szablonów.
   Przy zatrzymaniu źródło staje się nieważne. Żaden callback błędu pamięci nie
   zamyka klienta sieciowego. Brak poprawionej deklaracji biblioteki powoduje błąd
   kompilacji tego nagłówka, zamiast cichego użycia ze starą pętlą.
3. `candidate/library.patch`: kontrola źródła także po odczycie/przed wysłaniem,
   sprawdzenie rozmiaru odczytu i poprawna obsługa końcówki czekającej w buforze.
   Patch dotyczy wyłącznie ESPAsyncWebServer commit
   `c47917dcaba2acefb04277cd679f58a60b4fab30`.

## Znaleziony dodatkowy problem

Oryginalna pętla ustawia RESPONSE_END po przygotowaniu ostatniej porcji, zanim
cała porcja trafi do bufora TCP. Jeżeli TCP przyjmie tylko część, stara metoda
_finished nadal zwraca prawdę. Rzeczywisty _onAck żądania może wtedy zamknąć
połączenie, mimo że reszta danych pozostaje w buforze odpowiedzi.

Proponowana poprawka nie oznacza końca przed opróżnieniem tego bufora i pozwala
wrócić do wysyłania końcówki. Liczy też częściowo przyjęte bajty w wyniku pętli.
Nie oznacza to potwierdzenia, że odbiorca otrzymał plik — to nadal przekazanie do TCP.

## Co rzeczywiście wykonano

- 44 grupy w każdym z czterech przebiegów: zwykły/ASan+UBSan oraz obie wartości
  ASYNCWEBSERVER_USE_CHUNK_INFLIGHT (0 i 1). Nie są to 176 różnych scenariuszy.
- Prawdziwa pętla write_send_buffs z proponowanymi zmianami; rzeczywista deklaracja
  AsyncAbstractResponse i rzeczywiste metody żądania _onAck/_onPoll/_onTimeout.
- Prawdziwy Arduino FS.cpp z właścicielem 197 i modelowanym backendem.
- Testy przyjmowania po 1 i 7 bajtów: zwykła odpowiedź i ramki chunked, także
  końcowy znacznik chunked. Odpowiedź nie zamyka się z końcówką w swoim buforze.
- Zatrzymanie przed odczytem, podczas odczytu, podczas ostatniej porcji i przy
  oczekującym buforze; przedwczesny EOF, błędny rozmiar, zamknięta kopia File.
- Sprawdzenie liczb błędu zarówno SIZE_MAX hosta, jak i 0xFFFFFFFF. To nie wykonanie
  32-bitowego ABI ESP. Zwykła wartość RESPONSE_TRY_AGAIN nadal jest dozwolona dla
  ogólnego callbacku; sprawdzany odczyt File nie używa jej do ukrywania awarii.
- 12 wadliwych wariantów kompiluje się i zostaje wykrytych. Jeden usuwa obie
  późne kontrole źródła naraz, bo pojedyncze kontrole częściowo się zastępują.
- Testy bez wyjątków i RTTI, C++11; ASan, UBSan i wykrywanie wycieków włączone.

Finalny raport: `WYNIKI/run004/wynik.json`, mutacje: `MUTACJE.json`.
Źródła i ich sumy sprawdza `build.py`; generowanie patcha jest deterministyczne.

## Granice — ważne przed użyciem

**Nie wykonano całej biblioteki, prawdziwego AsyncTCP, lwIP ani ESP.** Klient TCP,
klasa bazowa, czas życia żądania oraz składanie nagłówków są modelowane. Nagłówki
w teście zastępuje marker HEAD, a bufor ma celowo małe 64 B. Test nie dowodzi
bezpieczeństwa niszczenia obiektów przez prawdziwe onDisconnect ani poprawności
wszystkich liczników i funkcji biblioteki.

**Ścieżka szablonów nie jest naprawiona ani kwalifikowana.** Kontrola rozmiaru po
przetworzeniu szablonów nie zabezpiecza działań wykonywanych wewnątrz przetwarzania.
Nowy prototyp używa wyłącznie callback=nullptr. Nie traktować patcha jako gotowej
aktualizacji wszystkich typów odpowiedzi.

Zamknięcie po błędzie następuje w kontekście kolejnego wywołania pętli odpowiedzi.
Gdy canSend jest fałszywe, rzeczywisty _onPoll nie woła tej pętli. Test pokazuje
wprost, że nie ma wtedy natychmiastowego zamknięcia. Nadal trzeba rozwiązać
ograniczony czas przerwania/wzbudzenie w prawidłowym kontekście sieciowym. Nie ma
obietnicy natychmiastowego anulowania. Nie można odwołać już oddanych bajtów TCP,
a błąd po przekazaniu całej odpowiedzi nie zmieni jej w nieudaną z punktu widzenia klienta.

Znana długość i pozycja pliku muszą być poprawnie ustalone przed konstrukcją;
nie wolno później zmieniać długości odpowiedzi. Ograniczenie liczby bajtów nie
zapewnia niezmiennej zawartości pliku podczas równoległego zapisu/rotacji. Brak
obsługi Range, Content-Disposition, uprawnień i rejestracji tras. Wysyłania plików
DO sterownika oraz jego fałszywego potwierdzenia sukcesu nie zmieniono w 199.

## Odtworzenie

Linux, Python 3, g++, ASan/UBSan. Z katalogu głównego paczki:

```sh
python3 -B narzedzia/odpowiedz-http199/run.py
python3 -B narzedzia/odpowiedz-http199/mutacje.py
```

Nie instalują SDK i nie łączą się z urządzeniem. Katalog candidate zawiera
propozycję źródeł biblioteki, nie nowy firmware. Nie podmieniać nim bibliotek
używanych do bieżącego wgrywania. LGPL-3.0-or-later i pozostałe licencje źródeł
znajdują się w dołączonym katalogu 197/LICENSES; zachowano nagłówki źródłowe.
