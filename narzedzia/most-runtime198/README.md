# 198 — jedna kontrola dla zapisu historii i otwartych plików

**NIE WGRYWAĆ. release_ready=false.** To działające połączenie komponentów w testach
na komputerze, a nie nowa wersja centrali. Program 3.55.8 pozostaje niezmieniony.

## Co konkretnie zmienia ten pakiet

197 kontrolował zwykłe obiekty File/FS. Historia miała jednak osobny sposób dostępu
do plików. Nowy `RuntimeHistory.h` prowadzi jej dopisywanie i przycinanie przez
**tego samego właściciela i tę samą blokadę**, przez całą transakcję.
Zwykły zapis File nie może wejść pomiędzy sprawdzenie, zapis, synchronizację,
weryfikację i zamknięcie wykonywane przez historię.

Zamknięta została również luka w sprzątaniu po błędzie: silnik 193 zawsze zgłaszał
próbę close. Bramka 198 po zatrzymaniu nie przekazuje takiej próby do pamięci.
Dotyczy to zarówno błędów operacji, jak i wykrycia niepoprawnej końcówki pliku lub
niezgodności odczytanych danych. Niepewnego zamknięcia nie ponawiamy. Deskryptor
zwrócony równocześnie z błędem też pozostaje zapisany w ograniczonej puli.

Nie ma zmiany algorytmu CSV/rotacji: `Engine198.h` jest wyodrębniony z 193, z trzema
mechanicznie sprawdzanymi zmianami potrzebnymi do przekazania kontekstu obserwatora
błędu. Bramka zastępuje dostęp do wywołań systemowych, nie obchodzi ich wyników.

## Wyniki — nowo wykonane w 198

| Sprawdzenie | Wynik i zakres |
|---|---|
| 36 grup | Zapis obu historii, przycinanie, odczyt przez prawdziwy Arduino FS.cpp, pule, błędy semantyczne, współbieżność |
| 153 wstrzyknięcia | Każda pozycja wywołania dopisywania/przycinania: wynik ujemny, pozorny sukces z errno, zatrzymanie podczas operacji |
| 220 błędów nośnika | Prawdziwy przypięty rdzeń LittleFS, model pamięci w RAM; wszystkie spowodowały zatrzymanie dalszych operacji |
| 1000 dopisań | Ponowne użycie ograniczonej puli bez narastania liczby deskryptorów |
| 14 mutacji | Wszystkie wadliwe warianty kompilowały się; wszystkie wykryte |
| C++11 | Bez wyjątków i RTTI, ostrzeżenia traktowane jako błędy w zwykłych testach |
| ASan + UBSan | Testy modelu i rdzenia powtórzone, LSan włączony |

153 przypadki są częścią grup testowych, nie należy dodawać tych liczb jako liczby
niezależnych scenariuszy. Mutacje wykonywane w profilu zwykłym. Próby współbieżności
obejmują dopisywanie i rotację, z zatrzymaniem oraz bez niego. Test potwierdza,
że oczekujący zapis File i oczekiwanie `drained()` rzeczywiście blokują się do
wyjścia transakcji, a nie tylko zakłada taką kolejność na podstawie opóźnienia.

Finalne wyniki wskazuje potwierdzenie paczki. Starsze katalogi WYNIKI dokumentują
rozwój; nie są dodatkowymi wykonaniami finalnego kodu.

## Ważne rozróżnienie: zapis potwierdzony a późniejszy błąd

`Result.confirmed` oznacza sukces sprawdzonego protokołu konkretnej operacji.
`Result.ready` jest tylko obserwacją stanu przy tworzeniu wyniku, nie pozwoleniem
na następną operację. Zatrzymanie może nastąpić zaraz po tej obserwacji.

Test wymusza zatrzymanie **po ostatnim sprawdzonym close, ale przed pobraniem
stanu ready**. Potwierdzenie pozostaje prawdziwe. Tak samo późniejsza nieudana
rotacja nie unieważnia wcześniejszego potwierdzonego dopisania. W przyszłej
integracji trzeba wycofać z kolejki już zapisany fragment, a nie dopisać go drugi raz.
Niepewny zapis nie otrzymuje potwierdzenia i nie jest automatycznie ponawiany.

## Czego wynik NIE dowodzi

- Nie podłączono komponentu do centrali, kolejki pomiarów ani transportu HTTP.
- Rdzeń LittleFS korzysta z bezpośredniego adaptera testowego, nie z ESP VFS.
  W tych próbach Arduino File ma modelowany backend. W osobnym teście modelowym
  File i deskryptory historii operują na tych samych danych.
- Pamięć rdzenia jest natychmiast zapisywanym modelem RAM. Nie przeprowadzono w 198
  prób odcięcia zasilania, buforowanego sprzętu ani kwalifikacji trwałości na ESP.
- Operacja wpuszczona przed zatrzymaniem może jeszcze zmienić dane. Nie cofamy jej.
  Brak dalszych wywołań obowiązuje po zamknięciu wejścia i wyjściu trwającej operacji.
- Nie czytamy, nie zamykamy, nie odmontowujemy ani nie montujemy ponownie
  niepewnego stanu, nawet w celu jego oględzin w testach.
- `unresolved` liczy zachowane wpisy własnej puli, w tym niepewne próby close;
  nie oznacza liczby faktycznie otwartych plików ani wszystkich zasobów backendu.
- Nadal potrzebne są obsługa przerwania HTTP, prawdziwe blokady FreeRTOS, pomiary
  czasu/RAM/WDT, sprawdzenie wszystkich dróg dostępu i kompilacja ESP.

## Odtworzenie

Linux, Python 3, g++ i gcc z ASan/UBSan. Z katalogu głównego rozpakowanej paczki:

```sh
python3 -B narzedzia/most-runtime198/audit.py
python3 -B narzedzia/most-runtime198/run.py
python3 -B narzedzia/most-runtime198/mutacje.py
```

Skrypty nie pobierają SDK ani nie łączą się z urządzeniem. Formatowany jest wyłącznie
nowy, pusty model RAM wewnątrz procesu testowego. Nie są to polecenia do wgrywania.

Zależności i licencje są w paczce: FS z 196, właściciel z 197, referencyjny nagłówek
historii z 3.55.8 oraz rdzeń littlefs z etapu 133. Hashami sprawdzamy źródła i brak
zmian w zamrożonych etapach. Wszystkie celowo zachowane obiekty testowe pozostają
osiągalne z globalnych tablic; nie wyłączono wykrywania wycieków.

Ustawienia, hasła, sterowanie, 8 s/60 s i SERWO% pozostają bez zmian. Zgoda A nie
jest zgodą na kasowanie nowych pomiarów po awarii ani formatowanie pamięci urządzenia.
