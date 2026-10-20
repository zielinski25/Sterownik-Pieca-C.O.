# v3.53.0 — autonomiczne dzienne archiwum solarne w centrali

Paczka robocza oparta na źródłach v3.50.0. Katalogi wcześniejszych wersji pozostają bez zmian.
**Nie zmieniano Androida, reguł Firebase ani plików historii. Firmware nie był kompilowany w tym środowisku.**

## Co wnosi v3.53.0 — raport testowany PRZED wgraniem

Pięć poprzednich wersji (3.52.7–3.52.11) to były poprawki rzeczy, które widać
**dopiero w gotowej wiadomości** — a testy sprawdzały tylko **pojedynczą linię dnia**.
Efekt: pięć aktualizacji pod rząd, każda wymagająca wgrania.

To się zmienia. Składanie wiadomości przeniesione do `SolarDailyArchive.h`:

| element | gdzie |
|---|---|
| czytanie plików z LittleFS | `main_centrala.cpp` (zostaje) |
| wypełnienie `DayReport` | `main_centrala.cpp` (zostaje) |
| **nagłówek, linie dni, stopka** | **`SolarDailyArchive.h` → `buildReport()`** |
| **podział na wiadomości** | **`SolarDailyArchive.h` → `splitForTelegramParts()`** |

Dzięki temu `tests/ReportSimulation.cpp` uruchamia **ten sam kod**, który jest
w firmware, i drukuje dokładnie to, co dostajesz w Telegramie.

```sh
cd tests
g++ -std=c++11 -Wall -Wextra -Werror -I.. ReportSimulation.cpp -o /tmp/reportsim
/tmp/reportsim            # scenariusze + wyniki
/tmp/reportsim --verbose  # pełna treść każdej wiadomości
```

### Co symulator znalazł w tej wersji

Sześć realnych błędów, których testy jednostkowe nie widziały:

| # | problem | skutek dla użytkownika |
|---|---|---|
| 1 | `/archiwum 30` dawało **4585 znaków**, a Telegram odrzuca powyżej 4096 | raport **nie docierał w ogóle** |
| 2 | znacznik `(ciag dalszy)` leciał **osobnym wywołaniem** wysyłki | druga wiadomość miała **14 bajtów** i sam nagłówek kontynuacji |
| 3 | świeży start (tylko prognoza na jutro) pokazywał `(0/1 dni z prognoza)` | czyta się jak „brak prognozy”, choć była linię wyżej |
| 4 | pokrycie od upływu czasu przy niespójnych danych dawało `100.0% czasu` przy `pokrycie 62.5%` | fałszywe „wszystko pokryte” |
| 5 | `strncpy` bez terminatora przy zbyt długim źródle | czytanie poza buforem (mogło wywrócić raport) |
| 6 | symulator dzielił na **3900** znaków, a firmware na **3800** | test przechodził, ale sprawdzał **inny podział** niż wysyłany |

Naprawione: podział na porcje ≤ 3800 znaków (`kTelegramSplitLen` — **jedna stała**
dla firmware i symulatora), prefiks kontynuacji **w tej samej wiadomości** co treść,
stopka bez mylącego mianownika, pokrycie czasu tylko przy spójnych danych,
jawne zerowanie terminatorów, zabezpieczenie przed ucięciem raportu
(ostatnia porcja mówi `(raport uciety — za duzo dni naraz)`).

### Zakres symulacji — 3559 sprawdzeń

| grupa | scenariusze |
|---|---|
| początek doby | 00:05 (5 próbek), 00:31 (próg 30), 01:59 |
| doba w toku | południe, wieczór, pełne 1440/1440 |
| doba zamknięta | pełna (TECH), z dziurą (bez TECH), z symulacjami, z rekordem CO |
| tryby | `spuszczona` (poza sezonem), brak czasu lokalnego (brak SNTP) |
| skrajne | doba 25 h, doba 23 h, 30 dni, 1 dzień, pusty katalog, awaria GHI |
| **przemiatanie** | **co 10 minut przez całą dobę (145 wariantów)** |
| **przemiatanie** | **każda liczba dni 1–30** |

Każdy wyrenderowany raport jest sprawdzany automatycznie na: długość linii,
brak `nan`/`inf`/`0/0`, potrójne spacje, zbalansowane `<b>…</b>`, zakończenie
nową linią, oraz — dla **każdej** z 175 wyrenderowanych wiadomości — podział bez
utraty treści, porcje ≤ 4096 znaków, brak pustych porcji, prefiks kontynuacji
tylko na porcjach 2..N, brak rozerwanego `<b>`.

`--verbose` pokazuje **granice wiadomości** i ich rozmiar w bajtach, czyli
dokładnie to, co przyjdzie do Telegrama:

```
---- wiadomosc 1/2 (3777 B) ----
☀️ <b>Archiwum solarne</b> (30 dni, z centrali)
...
---- wiadomosc 2/2 (822 B) [ciag dalszy] ----
(ciag dalszy)
...
---- razem 4585 B tresci, 4599 B po podziale ----
```

## Co wnosi v3.52.11 — początek doby nie udaje pomiaru

Pięć minut po północy raport pokazał:

```
2026-10-21  GHI 2.47 kWh/m2  dT +0.0°C  amplituda 0.0°C
            (min 21.2 @ 00:00 / max 21.2 @ 00:00)  pokrycie 0.3% (83.3% czasu)  5/1440 prob.
```

Wszystko prawda — ale `dT +0.0°C / amplituda 0.0°C` czyta się jak **„zbiornik nic nie
zrobił przez cały dzień"**, a to tylko 5 próbek z pierwszej minuty. Podobnie `(83.3% czasu)`:
sześć upłyniętych minut, z których restart przy wgrywaniu zjadł jedną → procent skacze
o 17 punktów przy każdym braku.

| było | jest |
|---|---|
| `dT +0.0°C  amplituda 0.0°C  (min 21.2 @ 00:00 / max 21.2 @ 00:00)` | `dT —  amplituda —  (za malo probek: 5)` |
| `pokrycie 0.3% (83.3% czasu)` już po 6 minutach doby | pokrycie od upływu czasu **dopiero po godzinie** |

Próg: **30 próbek** (`kMinSamplesForDelta`, pół godziny). Poniżej — brak podsumowania
temperatury, jest tylko uczciwa informacja, ile próbek jest. Powyżej — pełna linia jak dotąd.

Po zmianie ta sama doba wygląda tak:

```
2026-10-21  GHI 2.47 kWh/m2  dT —  amplituda —  (za malo probek: 5)  pokrycie 0.3%  5/1440 prob. (w toku)
```

Po pół godzinie zbierania linia sama wraca do normalnego formatu.

## Co wnosi v3.52.10 — „brak odczytów”, które nie jest brakiem danych

Raport o 23:35 na urządzeniu pokazał:

```
2026-10-21  GHI 2.47 kWh/m2  brak odczytow
2026-10-20  GHI 0.55 kWh/m2  dT -1.8°C  amplituda 3.5°C ... (w toku) TECH
Suma GHI: 3.02 kWh/m2 (2/2 dni z prognoza) | odczyty: 1/2
```

Czytelnik widzi „21 października — brak odczytów” i wygląda to jak **utrata danych**.
Tymczasem to normalny, zaprojektowany stan: o **23:30** firmware zapisuje z góry plik
**jutra** z prognozą GHI (celowo — restart o 23:59 nie może zgubić prognozy). Doba 21.10
jeszcze się nie zaczęła, więc odczytów być nie może.

**Poprawki:**

| było | jest |
|---|---|
| `brak odczytow` (także dla przyszłej daty) | `prognoza na jutro (doba sie nie zaczela)` |
| `brak odczytow` dla dzisiejszej doby bez próbek | `brak probek (doba w toku)` |
| `brak odczytow` tylko dla minionych dni | bez zmian — tu to uczciwy komunikat |
| `odczyty: 1/2` (jutro w mianowniku) | `odczyty: 1/1` — liczą się doby **zaczęte** |
| suma GHI wciągała prognozę na jutro | osobna linia `Jutro (prognoza): GHI 2.47 kWh/m2` |

**Drugi problem — `TECH` na niedomkniętej dobie.** `completeCoverage()` ma tolerancję
30 minut do końca doby, więc doba w toku dostawała znacznik „kandydat techniczny”
już od **23:30** — na pół godziny przed końcem, zanim było wiadomo, czy się domknie.
Teraz `TECH` pojawia się **wyłącznie dla doby zamkniętej**.

Po zmianie ten sam raport wygląda tak:

```
☀️ Archiwum solarne (2 dni, z centrali)
2026-10-21  GHI 2.47 kWh/m2  prognoza na jutro (doba sie nie zaczela)
2026-10-20  GHI 0.55 kWh/m2  dT -1.8°C  amplituda 3.5°C  (min 21.5 / max 25.0)
            pokrycie 96.3% (98.1% czasu)  1387/1440 prob. 00:00-23:33 (w toku)
Suma GHI: 0.55 kWh/m2 (1/1 dni z prognoza) | odczyty: 1/1
Jutro (prognoza): GHI 2.47 kWh/m2 — doba sie jeszcze nie zaczela.
```

Niczego nie ukrywam: gdy w **minionym** dniu naprawdę nie ma odczytów, nadal będzie
`brak odczytow`.

## Co wnosi v3.52.9 — godzina minimum i maksimum

Amplituda z v3.52.7 mówi **ile** stopni zbiornik się zmienił, ale nie mówi **skąd**.
Doba 20.10 wyglądała tak:

| godzina | temp. | co się działo |
|---|---|---|
| 00:00 | 25,0 °C | woda ciepła po słonecznym 19.10 |
| ~05:40 | 21,8 °C | wychładzanie nocne |
| popołudnie | 25,0 °C | grzanie słońcem |
| 22:58 | 23,5 °C | wieczorne stygnięcie |

`amplituda 3.2°C` jest poprawna, ale **dokładnie tę samą liczbę dałoby samo wychłodzenie
nocne bez ani minuty słońca**. Teraz linia pokazuje, kiedy padły oba ekstrema:

```
2026-10-20  GHI 0.55 kWh/m2  dT -1.5°C  amplituda 3.2°C
            (min 21.8 @ 05:40 / max 25.0 @ 14:20)
            pokrycie 94.6% (98.6% czasu)  1362/1440 prob. 00:00-22:58 (w toku)
```

Jak to czytać:

- **minimum przed wschodem słońca + maksimum po południu** → amplituda to grzanie słońcem,
- **maksimum wieczorem, minimum rano** → to samo (grzanie w ciągu dnia),
- **minimum wieczorem** → tego dnia nic nie grzało, amplituda to sama dobowa huśtawka,
- **`@` w ogóle nie ma** → rekord zapisany przed v3.52.9; nie zmyślamy godziny.

Zmiana dotyczy tylko raportu i zapisu rekordu (`actual.minUtc` / `actual.maxUtc`).
Próbki, progi i sposób liczenia pokrycia — bez zmian.

## Co wnosi v3.52.8 — stopnie Celsjusza zamiast kelwinów

Raport pokazywał `amplituda 2.5K`. To było poprawne fizycznie (różnicę temperatur
podaje się w kelwinach), ale w Polsce nikt tak nie czyta temperatury — wyglądało jak
literówka. Ta sama wartość, zapis po ludzku:

```
2026-10-20  GHI 0.55 kWh/m2  dT +0.2°C  amplituda 2.5°C  (min 22.5 / max 25.0) ...
```

Zmiana **wyłącznie kosmetyczna**: liczby, progi, zapis rekordu i znaczniki bez zmian.
Stare rekordy nie wymagają niczego — jednostka jest tylko w nagłówku raportu.

## Co wnosi v3.52.7 — raport, który mówi prawdę, i tryb „spuszczona”

Sześć poprawek czytelności raportu `/archiwum` plus jeden nowy znacznik. Żadna z nich nie zmienia
sposobu liczenia próbek ani zapisu rekordu „od środka” — poprawiają to, co widać w Telegramie.

### 1. Amplituda obok `dT`

`dT` porównuje **pierwszą próbkę doby z ostatnią**. Przy dobowym cyklu grzania i stygnięcia
wychodziło `dT +0.2°C`, mimo że zbiornik realnie zmienił temperaturę o **2,5 K**. Obok `dT` jest
teraz `amplituda = max − min`, która tej pomyłki nie popełnia:

```
2026-10-20  GHI 0.55 kWh/m2  dT +0.2°C  amplituda 2.5°C  (min 22.5 / max 25.0) ...
```

### 2. Pokrycie względem czasu, który upłynął

Raport o 18:00 pokazywał `pokrycie 74%`, choć brakowało 15 minut z 1080 (czyli **98,6%**).
Mianownikiem była cała doba, a nie czas od północy do teraz. Dla dnia w toku linia ma teraz
**oba** wskazniki:

```
pokrycie 74.0% (98.6% czasu)
```

Dzień zamknięty ma nadal tylko jeden — wtedy oba mianowniki są identyczne.

### 3. Koniec linii o sieci lokalnej

Z raportu zniknęła linia `Pelny JSON: GET /api/solar-archive (siec lokalna).`
Dostęp do archiwum jest **wyłącznie przez Telegram i GitHuba** — raport nie powinien
reklamować endpointu, którego użytkownik nie używa i nie chce używać.

### 4. Pokrycie z jednym miejscem po przecinku

`0%` przy 5 próbkach wyglądało jak brak danych, a było `0,4%`. Teraz `0.4%`.

### 5. Nowa komenda `/solar_tryb spuszczona | sezon`

Jedyny sposób, żeby odróżnić „zbiornik z wodą, tylko pochmurno” od „woda spuszczona na zimę”.
Bez tego zimowe odczyty — czujnik w **pustym** zbiorniku mierzy powietrze — wyglądają jak dane
słoneczne z zerowym przyrostem.

| Komenda | Efekt |
|---|---|
| `/solar_tryb` | pokazuje aktualny tryb i co on oznacza |
| `/solar_tryb spuszczona` | ZIMA: **nie pobiera prognozy GHI**, nowe rekordy dostają `offSeason` |
| `/solar_tryb sezon` | woda w zbiorniku, prognoza GHI wraca przy najbliższym oknie |

Tryb jest zapisany w NVS (`solarOffSeason`), więc **przeżywa restart i OTA** — po aktualizacji
nie wraca po cichu do „sezon”. Poza sezonem działa wszystko poza GHI: próbki, checkpoint co 5 min,
domknięcie doby i synchronizacja z Firebase. W raporcie taki dzień ma dopisek `(poza sezonem)`,
a nagłówek całego raportu — linię `Tryb: spuszczona (ZIMA — zbiornik pusty)`.

### 6. Tablica godzinowa GHI przesunięta o godzinę

Open-Meteo podaje `shortwave_radiation` jako **średnią z poprzedniej godziny**: punkt o 00:00
opisuje dobę **poprzednią** (23:00–24:00), a prawdziwa ostatnia godzina wypadała poza limit
tablicy. Sumy i szczyt były poprawne (na obu granicach GHI = 0), więc błąd było widać tylko przy
porównywaniu profilu godzinowego. Teraz do doby wchodzą punkty z przedziału `(start, end]` —
dokładnie tyle, ile doba ma godzin (24, 23 albo 25).

**Wymagany jest też nowy `SolarDailyArchive.h`** — `main_centrala.cpp` nie skompiluje się ze
starym plikiem (`SOLAR_DAILY_ARCHIVE_API` podniesione do `20261027`).

## Co wnosi v3.52.6 — rollback, który naprawdę działa (krytyczne)

Zdarzył się realny przypadek: po wgraniu nowego firmware'u **watchdog resetował ESP co ~15 s**.
Urządzenie było nieosiągalne dla OTA i trzeba było jechać do niego z kablem.

**Dlaczego rollback wtedy nie pomógł:** obraz był potwierdzany na **końcu `setup()`**. Pętla resetów
zaczynała się później — w `loop()` i taskach — a w chwili pierwszego resetu partycja była już
`VALID`. Bootloader nie miał do czego wracać.

**Naprawa:** potwierdzenie przeniesione z `setup()` do `loop()`:

| Warunek potwierdzenia | Wartość |
|---|---|
| stabilna praca z podłączonym WiFi | **10 minut** (`OTA_CONFIRM_UPTIME_MS`) |
| awaryjnie, gdy WiFi nie wstanie | **60 minut** (`OTA_CONFIRM_MAX_MS`) |

Firmware resetujący się co 15 s **nigdy nie potwierdzi partycji** → przy pierwszym resecie
bootloader wraca do poprzedniej wersji → urządzenie jest znów dostępne przez OTA. Dokładnie ten
scenariusz, który wcześniej kończył się wizytą z kablem.

**Zmiana zachowania:** komunikat „Aktualizacja zakończona pomyślnie" przychodzi teraz **~10 minut
po starcie**, a nie od razu. To celowe — wcześniej był wysyłany, zanim cokolwiek zdążyło się
zepsuć.

**Dodatkowo — koniec cichego rollbacku.** Przy odroczonym potwierdzeniu zapisywany jest znacznik
`/ota_pending.txt` z numerem wersji. Jeśli po restarcie wystartuje **inna** wersja niż w znaczniku,
oznacza to, że poprzednia aktualizacja się nie potwierdziła i ESP32 sam wrócił. Wtedy przychodzi:

```
UWAGA: wersja v3.52.6 nie potwierdzila sie po aktualizacji - ESP32 sam wrocil do v3.52.5.
Szczegoly: log OTA-ROLLBACK.
```

## Co wnosi v3.52.5 — odpowiedź czytana w całości przed parsowaniem (krytyczne)

Po naprawie URL-a status zmienił się z `http_400` na **`json`**. To był postęp: zapytanie
przechodziło (HTTP 200), ale `deserializeJson()` kończył się błędem.

Przyczyna: parsowanie szło **wprost ze strumienia** (`http.getStream()`). Przy odpowiedzi
z `Transfer-Encoding: chunked` strumień podaje surowe znaczniki długości fragmentów, więc parser
widzi śmieci zamiast `{`. Pobieranie pogody działa, bo jego odpowiedź jest inna — mniejsza i bez
chunked.

Naprawa: `http.getString()` składa całą odpowiedź (obsługuje chunked i `Content-Length`),
a dopiero potem `deserializeJson()` pracuje na gotowym `String`. Przy niepowodzeniu zapisujemy
`err.c_str()` oraz pierwsze 90 bajtów treści — raport pokaże konkretną przyczynę, np.:

```
Ostatnia proba GHI: json - InvalidInput | <pierwsze 90 bajtow odpowiedzi>
```

## Co wnosi v3.52.4 — koniec zgadywania przy `http_400` (krytyczne)

Po wgraniu v3.52.3 raport **nadal** pokazywał `GHI brak (http_400)`, mimo że `buildGhiUrl()`
w nagłówku było już poprawione. Przyczyną okazał się nie kod, a **sposób raportowania**: `/archiwum`
czyta `forecast.captureStatus` **z pliku rekordu**, nie z żywej próby. Wpis `http_400` pochodził
z próby wykonanej **przed restartem**, a komenda poszła kilka sekund po OTA — zanim centrala
zdążyła wykonać pierwszą próbę na nowym kodzie. Nagłówek był poprawny przez cały czas.

Wniosek: status w raporcie bywa historyczny; po wgraniu trzeba odczekać jeden cykl (15 min).

Dwie niezależne warstwy, żeby to się nie powtórzyło:

1. **Blokada na etapie kompilacji.** `SolarDailyArchive.h` ma teraz `SOLAR_DAILY_ARCHIVE_API`.
   `main_centrala.cpp` zawiera `#error`, jeśli ta wartość jest starsza niż `20261020`:
   ```
   Podmien TAKZE plik SolarDailyArchive.h — stara wersja buduje URL z 'Z' i Open-Meteo zwraca HTTP 400.
   ```
   Build **nie przejdzie**, dopóki oba pliki nie będą zgodne.
2. **URL budowany lokalnie** w `main_centrala.cpp` (`solarArchiveBuildGhiUrlLocal()`), bez
   zależności od nagłówka i bez końcowego `Z`. Sprawdzono hostowo, że dla 5 dat (m.in. 25.10
   i 29.03, przejścia czasu) daje URL **identyczny** z poprawnym `buildGhiUrl()` z nagłówka.

Dodatkowo raport Telegrama pokazuje teraz **dowód ostatniej próby**, gdy się nie udała:

```
Ostatnia proba GHI: http_400 - <poczatek odpowiedzi serwera>
URL: https://api.open-meteo.com/v1/forecast?...&start_hour=...&end_hour=...
```

Koniec ze zgadywaniem: jeśli `http_400` powróci, w Telegramie będzie widać **dokładny URL**
wysłany przez centralę i **treść odpowiedzi Open-Meteo**.

## Co wnosi v3.52.3 — PRZYCZYNA `http_400` (krytyczne)

Raport pokazał `2026-10-20 GHI brak (http_400)`. To była **prawdziwa przyczyna** wszystkich
wcześniejszych braków prognozy.

`buildGhiUrl()` budował `start_hour`/`end_hour` przez `formatIsoUtc()`, które dopisuje końcowe `Z`
(ISO-8601). **Open-Meteo nie przyjmuje `Z` w tych parametrach.** Sprawdzone bezpośrednio:

```
...&start_hour=2026-10-19T22:00:00Z  ->  {"reason":"Bad Request","error":true}   (HTTP 400)
...&start_hour=2026-10-19T22:00:00   ->  200 OK, 25 punktów dla doby 20.10
```

Strefa jest już podana parametrem `timezone=GMT`, więc `Z` było zbędne — i blokowało wszystko.
Teraz `Z` jest ucinane przed zbudowaniem URL, a test hostowy pilnuje, żeby w zapytaniu nie było
ani jednego `Z` w granicach godzin.

Sprawdzone ręcznie na API (poza testem), dla dat z URL-a produkowanego przez firmware:

| Doba | Wynik |
|---|---|
| 2026-10-20 (24 h) | 200 OK, 25 punktów |
| 2026-10-25 (25 h, koniec DST) | 200 OK, 26 punktów |

(Uwaga: zapytanie o datę spoza zakresu API, np. pół roku wstecz, zwraca
`Parameter 'start_hour' is out of allowed range` — to normalne i nie dotyczy pracy na bieżąco.)

## Co wnosi v3.52.2 — pusty rekord nie wraca

Po `/archiwum_usun 2026-10-19` dzień zniknął, ale natychmiast pojawił się ponownie jako
`2026-10-19 GHI brak brak odczytow`. Powód: `solarArchiveBeginDay()` zapisywał pusty „szkielet"
rekordu zaraz po starcie doby — także po usunięciu. Wyglądało to jakby usuwanie nie działało.

- `beginDay()` **nie zapisuje już pustego pliku**.
- Checkpoint zapisuje dzień dopiero, gdy ma **co najmniej jedną próbkę albo prognozę GHI** dla tej daty.
- Skutek: usunięty dzień zostaje usunięty. Pusty plik nie zaśmieca `/archiwum` ani `/api/solar-archive`.

## Co wnosi v3.52.1 — dzień hybrydowy

Po wgraniu v3.52.0 rekord 19.10 wyglądał tak:

```
2026-10-19  GHI brak  dT +38.8C (min 11.5 / max 62.0)  pokrycie 86%  1242/1440 prob. 00:00-20:46
```

Pokrycie i próbki są już poprawne (1242/1440 = 86%), ale `dT +38.8 °C` **nie jest prawdziwe**:
agregat odtworzył `firstC = 11.8 °C` z pliku zapisanego jeszcze dla **bojlera pieca C.O.**,
a potem doszły próbki ze zbiornika (do 62 °C). Powstał dzień hybrydowy — dwa różne czujniki
w jednym rekordzie. Co gorsza, nowy firmware nadpisał plik jako `schemaVersion 2`, więc rekord
przestał być oznaczany `[dane CO]`, choć nadal był zmieszany.

- **Odtwarzanie liczników tylko z tego samego kanału.** `schemaSupportsCounterRestore()`
  przepuszcza wyłącznie `schemaVersion >= 2`. Rekord starego kanału jest pomijany —
  doba startuje od zera od chwili wgrania, zamiast sklejać dwa czujniki.
- **Nowa komenda `/archiwum_usun RRRR-MM-DD`** usuwa jedną dobę. Jeżeli to doba liczona teraz,
  zerwany jest także agregat w RAM — inaczej checkpoint po 5 minutach odtworzyłby plik
  z tymi samymi danymi.
- `/archiwum_czysc TAK` nadal usuwa wszystkie rekordy sprzed zmiany kanału.

Kolejność porządków po wgraniu:

```
/version                        -> v3.52.1
/archiwum_usun 2026-10-19       -> usuwa dzień hybrydowy (dziś liczy się od nowa)
/archiwum                       -> zostaje tylko czysty dzień
```

## Co wnosi v3.52.0 — najważniejsza poprawka

**Archiwum czytało niewłaściwy czujnik.** Przez trzy doby zapisywało temperaturę **bojlera pieca C.O.**
(`TELEMETRY_CH_BOJLER`, `czujnik_temp_Bojlera`, magistrala BUS_2) — czyli wodę układu grzewczego,
która z kolektorem słonecznym nie ma nic wspólnego.

Układ słoneczny ma **własny zbiornik**, a jego temperaturę mierzy **czujnik „panel”**
(`czujnik_temp_Panel_Plus`, magistrala BUS_3) = `TELEMETRY_CH_PANEL`. To ten kanał jest teraz
źródłem archiwum.

- Powód fałszywego `dT +46.0C` był podwójny: zły kanał **i** próbka `0.0 °C` z DS18B20 po restarcie.
  Oba są wyeliminowane — kanał jest właściwy, a o wiarygodności decyduje bit jakości odczytu.
- **Zakres `-30..160 °C` zostaje bez zmian.** Czujnik panelu zimą legalnie pokazuje wartości ujemne,
  więc **nie ma** odrzucania `<= 0` (to dotyczyło bojlera CWU). Wartość `85.0 °C` (power-on DS18B20)
  jest odrzucana wcześniej, w `odczytajTemperature()`.
- Rekord ma teraz `schemaVersion: 2` oraz jawne `actual.channel = "t_panel"`,
  `channelLabel = "czujnik panelu (bojler solarny)"`, `channelPin = "BUS_3 / czujnik_temp_Panel_Plus"`.
- Stare rekordy (`schemaVersion < 2`) są w Telegramie oznaczone **`[dane CO]`** i można je usunąć:
  `/archiwum_czysc` pokazuje ile, `/archiwum_czysc TAK` usuwa **tylko** te stare.
- W panelu `Piec.html` karta archiwum mówi teraz `Δ t_panel`, a CSV ma kolumny
  `t_panel_first_c`, `t_panel_last_c`, `observed_delta_t_panel_c`.

### Układ na dachu i w kotłowni (potwierdzony zdjęciem i opisem, 2026-10-19/20)

Są **dwa osobne zasobniki** — to najważniejsze rozróżnienie w całym projekcie:

| Zasobnik | Gdzie | Co to jest |
|---|---|---|
| słoneczny | dach | kolektor próżniowy rurowy **10 rur** z **integrowanym zbiornikiem poziomym 100 l** |
| CWU pieca | kotłownia | bojler C.O. grzany wężownicą z pieca |

Obieg słoneczny jest **grawitacyjny (termosyfon), bez pompy**. Czujnik
`czujnik_temp_Panel_Plus` siedzi **w zbiorniku na dachu**, z boku, w **połowie wysokości**.

Tryby pracy (przełączane zaworami):

- **letni** — wodociąg → zbiornik na dachu → kran. Zbiornik jest jednocześnie zasobnikiem CWU,
  więc czujnik mierzy wodę, która realnie leci do kranu.
- **zimowy** — wodociąg → bojler pieca → kran. Zbiornik na dachu jest **spuszczony i nieużywany**.
- **przejściowy** (wiosna/jesień) — użytkownik wybiera zaworami ten zasobnik, w którym woda jest cieplejsza.

Co to znaczy dla archiwum:

- Dopóki w zbiorniku **jest woda**, pomiar ma sens — również wtedy, gdy woda nie zasila kranu:
  zbiornik nadal się nagrzewa od słońca, a brak poborów daje **najczystszy sygnał** przyrostu.
- **Zima ze spuszczoną wodą** to jedyny okres bezwartościowy: czujnik mierzy powietrze w pustym
  zbiorniku. Takie dni trzeba oznaczyć i wyłączyć z sum (plan: v3.53, znacznik trybu).
- Czujnik w połowie wysokości **nie widzi pełnej stratyfikacji**: pobór CWU objawia się słabiej niż
  przy czujniku na dole, a `ΔT` nie jest średnią temperatury całych 100 l.
- `ΔT × 0,1163 kWh/K` to równoważnik dla 100 l **jednorodnie ogrzanej wody**. Przy czujniku
  w połowie wysokości pozostaje **szacunkiem rzędu wielkości**, nigdy pomiarem energii.

## Co wnosi v3.51.2 (po drugiej dobie na urządzeniu)

Rekord 19.10 pokazał `1201/1440 prob.`, ale `pokrycie 27%` (~389 minut) oraz `dT +46.0C`.
Dwie osobne wady:

- **Pokrycie stało.** Licznik „różnych minut" był sprzężony z bitem maski minut. Gdy znaczniki
  czasu się powtarzają (albo maska jest niepełna po restarcie), licznik staje, mimo że próbki
  dalej napływają — dokładnie 1201 próbek i 389 minut. Teraz licznik jest **monotoniczny**
  (nowa minuta względem poprzedniej próbki) i nie zależy od maski. Maska nadal jest zapisywana,
  ale wyłącznie do odtworzenia pokrycia po restarcie.
- **Fałszywe dT +46°C.** DS18B20 po restarcie zwraca `0.0 °C`, co przechodziło przez zakres
  `0..160` jako poprawna próbka i stawało się pierwszą wartością doby. Teraz próbka bojlera
  `<= 0.0 °C` jest liczona jako błąd odczytu (woda CWU nie ma 0 °C).

Raport Telegrama pokazuje teraz **min/max oraz zakres godzin próbek**, np.:

```
2026-10-19  GHI brak  dT +0.4C (min 41.2 / max 46.0)  pokrycie 83%  1201/1440 prob. 00:00-20:01 (w toku)
```

Jeśli `GHI brak` nie ma w nawiasie przyczyny, znaczy to, że okno snapshotu jeszcze nie nadeszło
(albo dana doba była przed wdrożeniem v3.51.1).

## Co wnosi v3.51.1 (po pierwszym uruchomieniu na urządzeniu)

v3.51.0 zbierał próbki poprawnie, ale **prognoza GHI nie została ani raz pobrana** — przez dwie doby
rekordy miały `GHI brak`. Przyczyna: zbyt wąskie okno (23:45–23:59), tylko 3 próby co 5 minut
i timeout TLS 4 s. Trzy nieudane połączenia pod rząd kosztowały całą dobę.

- **Szersze okno:** 23:30–23:59, próba co 2 minuty, maks. 8 prób, timeout 7 s.
- **Dosieg poranny:** 00:00–04:59 wolno jeszcze pobrać prognozę na **bieżącą** dobę, jeśli wieczór
  zawiódł. Przed wschodem słońca to nadal prognoza, nie retrospekcja — rekord dostaje
  `forecast.capturedLate = true`, żeby dało się to odfiltrować. Próby co 15 min, maks. 20.
- **Widoczna przyczyna:** każda nieudana próba zapisuje `forecast.captureStatus`
  (`http_begin`, `http_<kod>`, `json`, `brak_pol`) i jest pokazywana w Telegramie jako
  `GHI brak (http_-1)` — zamiast gołego „brak”.
- **Pokrycie przeżywa restart:** maska minut trafia do rekordu (`actual.minuteMaskHex`, 376 znaków),
  więc po OTA/restarcie pokrycie doby nie spada do zera ani się nie zawyża.
- **Poprawiona odmiana:** „1 dzień” zamiast „1 dni”.
- Poprawka schematu: rekord z prognozą ma teraz jawne `forecast.available = true`.

## Co się zmieniło

Archiwum dnia nie jest już liczbowane w przeglądarce. Powstaje w centrali ESP32, w istniejącym
zadaniu sieciowym `tgTaskFn`, więc działa także przy zamkniętym panelu `Piec.html` i zamkniętej
aplikacji Android.

- **Wieczorny snapshot GHI.** Raz na dobę, w oknie 23:45–23:59 czasu lokalnego, centrala wykonuje
  jedno małe zapytanie do Open-Meteo: tylko `hourly=shortwave_radiation`, `timezone=GMT`,
  `timeformat=unixtime` i granice `start_hour`/`end_hour` podane w UTC. Dzięki temu doba ma
  poprawnie 23, 24 albo 25 godzin i nie gubi się ostatnia godzina (GHI jest średnią z **poprzedniej**
  godziny). To nie jest nowy poller — to maksymalnie 3 próby w jednym wieczornym oknie.
- **Wynik rzeczywisty bez kopiowania telemetrii.** Istniejąca próbka minutowa przekazuje do agregatu
  tylko temperaturę bojlera, znacznik UTC, jakość i flagę symulacji. Agregat trzyma w RAM pierwszą
  i ostatnią wartość, min/max, liczbę próbek poprawnych/symulowanych/błędnych, pokrycie minutowe
  i najdłuższą przerwę. Symulacje nie wchodzą do wyniku fizycznego, ale są liczone i raportowane.
- **Zapis lokalny.** Rekord dnia trafia do `/solar_daily/YYYY-MM-DD.json` (plik tymczasowy + rename,
  bez ruszania `/hist_short.csv` i `/hist_long.csv`). Checkpoint jest rzadki — co 5 minut, a nie przy
  każdej próbce. Retencja: po jednym najstarszym pliku na cykl, limit 400 dni.
- **Synchronizacja z Firebase.** Po powrocie sieci rekord jest wysyłany tym samym, istniejącym
  klientem Firebase przez `update` (PATCH) na `/piec/solarArchive/v1/days/YYYY-MM-DD.json`.
  Porażka (np. brak reguły) **nie kasuje** rekordu — zostaje lokalnie i jest ponawiany co 15 minut.
- **Nowy endpoint tylko do odczytu:** `GET /api/solar-archive` oraz `GET /api/solar-archive?date=RRRR-MM-DD`
  zwraca gotowe rekordy z LittleFS. `Piec.html` i Android mają z niego czytać, a nie zbierać.
- **Odczyt archiwum przez Telegram.** Centrala bywa dostępna tylko przez Telegram (bez LAN-u pod ręką),
  więc doszła komenda `/archiwum` (alias `/solar`): czyta gotowe rekordy i wysyła jedną wiadomość —
  jedną linię na dobę. `/archiwum 30` pokazuje do 30 dni (domyślnie 7). Ten sam raport jest pod
  przyciskiem „☀️ Archiwum solarne” w menu „📁 Logi i dane”. To wyłącznie odczyt: nic nie zbiera,
  nie dopisuje brakujących dni i nie zmienia rekordów.
- **Panel `Piec.html` jest teraz czytnikiem.** Usunięto z niego kolekcję IndexedDB i zapisywanie
  archiwum do Firebase przez przeglądarkę (`captureForecasts`/`syncPending` zostały jako puste
  funkcje zgodności). Karta pokazuje rekord z centrali albo uczciwie „brak rekordu”.

## Pliki

- `main_centrala.cpp` — firmware v3.51.0 z sekcją `[v3.51.0 SOLAR-DAILY-ARCHIVE]`, hookiem w
  `historiaZapiszProbke()`, bramką w `tgTaskFn`, endpointem `/api/solar-archive` oraz osadzonym
  na nowo `Piec.html`.
- `SolarDailyArchive.h` — czysta logika: granice doby Europe/Warsaw (23/24/25 h), agregat `t_bojler`,
  snapshot GHI, budowa URL. Bez Arduino, bez sieci, testowalna hostowo.
- `Piec.html` — panel w wersji czytającej archiwum z centrali.
- `tests/SolarDailyArchiveTest.cpp` — testy hostowe logiki.
- `fix-wdt-idle1-v3.49.1.patch`, `main_centrala-v3.49.0-features.patch` — materiały historyczne;
  pełne źródło w tej paczce ma je już zastosowane. Nie nakładać ponownie.

## Testy

Test hostowy logiki (w tym katalogu):

```sh
cd tests
g++ -std=c++11 -Wall -Wextra -Werror SolarDailyArchiveTest.cpp -o /tmp/solar-daily-test
/tmp/solar-daily-test
```

Wynik w tej sesji: `SolarDailyArchiveTest: PASS (339 sprawdzeń)` — asercje na format linii
(amplituda, pokrycie od upływu czasu, godziny min/max), znacznik `(poza sezonem)`,
okno godzinowe GHI `(start, end]`, zachowanie `minUtc`/`maxUtc` po restarcie,
a także rozróżnienie „prognoza na jutro” / „brak próbek (doba w toku)” /
„brak odczytów” oraz to, że `TECH` nie pojawia się przed domknięciem doby, a linia z mniej niż
30 próbkami nie podaje `dT` ani amplitudy.

Testy panelu (z katalogu głównego repozytorium):

```sh
node --test tests/*.test.js
```

Wynik w tej sesji: 17/17 testów przeszło, w tym nowy test sprawdzający, że `Piec.html`
czyta `/api/solar-archive` i nie zawiera już IndexedDB ani zapisu archiwum przez przeglądarkę.

### Poprawki kompilacji (2026-10-18, po pierwszym buildzie użytkownika)

- **`now.hour()` → `now.hour`** w `solarArchiveLoop()`. `solar_daily::LocalFields` to zwykła struktura
  z polami `hour`/`minute`, a nie `DateTime` z Arduino — wołanie jak funkcji nie kompilowało się
  (`expression cannot be used as a function`). Pozostałe `now.hour()` w pliku dotyczą `DateTime`
  i są poprawne.
- **Retencja:** `entry.name()` na ESP32 może zwracać nazwę z prefiksem katalogu. Nazwa jest teraz
  ucinana po ostatnim `/` przed sprawdzeniem daty — wcześniej stare pliki nie byłyby usuwane.

Uwaga do VS Code: czerwone „cannot open source file SolarDailyArchive.h” w zakładce *Problems*
pochodzi z IntelliSense (brak ścieżki w `includePath`), a nie z kompilatora. Jeśli `pio run` nie
zgłasza `fatal error: SolarDailyArchive.h: No such file or directory`, plik jest na miejscu i błąd
jest kosmetyczny.

**Czego nie sprawdzono:** pełnego builda PlatformIO (brak toolchainu w tym środowisku) i działania
na urządzeniu. Przed wgraniem trzeba zbudować firmware u siebie.

## Jak to odczytać

Najprościej z telefonu, bez sieci lokalnej:

- `/archiwum` — ostatnie 7 dni, jedna linia na dobę,
- `/archiwum 30` — do 30 dni,
- `/solar` — alias tego samego,
- przycisk „☀️ Archiwum solarne” w menu „📁 Logi i dane”.

Przykładowa linia:

```
2026-10-18  GHI 2.84 kWh/m2  dT +3.1C  pokrycie 97%  1378/1440 prob.
2026-10-17  GHI brak (http_-1)  dT +0.4C  pokrycie 92%  1325/1440 prob.
```

Znaczniki: `(w toku)` — doba jeszcze się liczy, `(niepelna)` — prognoza nie ma kompletu godzin,
`sym` — w dobie były próbki symulowane, `TECH` — dzień spełnia techniczne kryteria oceny modelu
(ale nadal nie jest pomiarem energii kolektora). W nawiasie po `GHI brak` jest powód: `http_begin`
(nie udało się otworzyć połączenia), `http_<kod>` (serwer odpowiedział kodem błędu), `json`
(odpowiedź nie była poprawnym JSON-em), `brak_pol` (brak pól GHI w odpowiedzi).

## Wgranie (OTA, terminal PlatformIO)

Przykład dla Windows PowerShell. Ustaw ścieżki do rozpakowanego projektu i do tej paczki; wykonaj
komendy w kopii projektu, zostawiając jego `platformio.ini` bez zmian.

```powershell
$project = "C:\SCIEZKA\DO\Cenatrala Pieca"
$package = "C:\SCIEZKA\DO\v3.51.0-solar-daily-archive"

foreach ($file in @("main_centrala.cpp", "AutoIgnitionColdTimer.h", "MieszadloTrocinyAuto.h", "AlarmBuzzerProfiles.h", "SolarDailyArchive.h")) {
  $current = Join-Path "$project\src" $file
  if (Test-Path $current) { Copy-Item $current "$current.pre-v351.bak" -NoClobber }
}
Copy-Item "$package\main_centrala.cpp" "$project\src\main_centrala.cpp" -Force
Copy-Item "$package\AutoIgnitionColdTimer.h" "$project\src\AutoIgnitionColdTimer.h" -Force
Copy-Item "$package\MieszadloTrocinyAuto.h" "$project\src\MieszadloTrocinyAuto.h" -Force
Copy-Item "$package\AlarmBuzzerProfiles.h" "$project\src\AlarmBuzzerProfiles.h" -Force
Copy-Item "$package\SolarDailyArchive.h" "$project\src\SolarDailyArchive.h" -Force

Set-Location $project
pio run -e esp32-s3-ota
# Uruchom upload dopiero po udanym buildzie:
pio run -e esp32-s3-ota -t upload
```

`esp32-s3-ota` dziedziczy konfigurację `esp32-s3-n16r8` i używa OTA skonfigurowanego lokalnie
w `platformio.ini`; nie wpisuj ani nie publikuj hasła OTA. Przed uploadem sprawdź, czy `upload_port`
wskazuje aktualny adres centrali i czy komputer oraz centrala są w tej samej sieci. Paczka nie
zawiera `platformio.ini` celowo.

## Zanim włączysz wysyłkę do Firebase

Domyślnie `SOLAR_ARCHIVE_FB_SYNC_ENABLED` ma wartość `1`, a ścieżka to
`/piec/solarArchive/v1/days/YYYY-MM-DD.json`. **Reguły Firebase nie zostały zmienione w tej paczce.**
Jeśli wdrożone reguły nie zezwalają na zapis w tej ścieżce, rekordy pozostaną w LittleFS,
a w logu pojawi się `SOLAR-ARCH zdarzenie=sync_blad`. Wtedy:

1. sprawdź w konsoli Firebase, czy reguła dopuszcza tę ścieżkę dla zalogowanego użytkownika centrali,
2. albo ustaw `SOLAR_ARCHIVE_FB_SYNC_ENABLED 0` i korzystaj wyłącznie z `/api/solar-archive`.

Nie zmieniaj reguł „na ślepo” i nie otwieraj całego drzewa `/piec` publicznie.

## Znane ograniczenia

- To archiwum **faktu**, nie pomiar energii kolektora. `Δ t_bojler` zależy też od poboru CWU, dogrzewania
  innym źródłem i strat zbiornika, dlatego rekord ma flagę `technicalCandidate` i nie jest automatycznie
  „kalibracją”.
- Snapshot GHI jest dla płaszczyzny poziomej (GHI), nie dla pochyłego kolektora (GTI).
- Zapytanie GHI używa `setInsecure()`, tak jak dotychczasowe pobieranie Open-Meteo w tym firmware;
  weryfikacja certyfikatu to osobne zadanie.
- Godzina 23:45 jest lokalna. Przy braku wiarygodnego czasu (brak RTC i brak NTP) archiwum nic nie
  zapisuje — świadomie, zamiast tworzyć rekord z fałszywą datą.
- Ponowne wysłanie rekordu jest idempotentne dzięki kluczowi `YYYY-MM-DD`.
