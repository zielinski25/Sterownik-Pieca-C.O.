# Firmware centrali 3.58.5 — wykresy dla panelu LCD po restarcie centrali

Data: 2026-11-12 · zastępuje paczkę **3.58.4** (`firmware-patch/v3.58.4-panel-wykresy/`).

Zmienia się **jeden plik**: `src/main_centrala.cpp`. Pliki `.h`, `secrets.h`,
biblioteki i `platformio.ini` bez zmian. Plik pasuje do projektu, w którym
jest wersja **3.58.3** albo **3.58.4** (3.58.5 = 3.58.4 + poprawka opisana
niżej; 3.58.4 = 3.58.3 + adres `/api/history/panel`).

Panel LCD zostaje na **v1.3.0** — ta wersja nie wymaga nowego firmware'u panelu.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Co zostało sprawdzone, a co nie: rozdział na końcu. Instrukcja wgrania
> i powrotu: `CZYTAJ-NAJPIERW.txt`.

---

## Skąd ta wersja

Pytanie: „a jak centrala ma wyłączone zapisywanie wykresów lokalnie?”
(przełącznik „Zapisuj historię na flash centrali” na stronie WWW centrali,
Ustawienia → „Zapis historii na flash”).

Odpowiedź jest w następnym rozdziale. Przy jej sprawdzaniu kod 3.58.4 został
uruchomiony na PC w sytuacji „centrala po restarcie” i wyszły dwie wady,
które 3.58.5 usuwa. Nie zależą od przełącznika zapisu, ale z wyłączonym
zapisem i starymi plikami na flashu powtarzały się po każdym restarcie.

## Wyłączony zapis historii — co widać na LCD

Przełącznik blokuje tylko dopisywanie do plików `/hist_short.csv`
i `/hist_long.csv`. Historia w pamięci centrali (PSRAM) zbiera się dalej,
a wykresy dla panelu są liczone właśnie z niej.

| Sytuacja | Ekran Wykresy na LCD |
|---|---|
| Centrala pracuje, zapis wyłączony | wykresy takie same jak z włączonym zapisem: godzina, doba, tydzień |
| Restart centrali, zapis wyłączony, plików historii nie ma (wyczyszczone) | historia zaczyna się od zera: przez pierwszą minutę napis `Centrala nie ma jeszcze danych z tego zakresu`, po pierwszym pomiarze `Za malo danych na wykres (jeden pomiar)`, od drugiego pomiaru (ok. 2 minuty po starcie) wykres, który wydłuża się z każdą minutą |
| Restart centrali, zapis wyłączony, na flashu zostały stare pliki | centrala wczytuje je przy starcie. To, co z nich mieści się w zakresie wykresu, jest na wykresie, potem przerwa i nowe pomiary. Czego w zakresie nie ma, tego nie widać |
| Restart centrali, zapis włączony | wykres jak przed restartem; gdy centrala nie pracowała dłużej niż 6 minut, w tym miejscu linia ma przerwę |

Pod napisami „brak danych” i „jeden pomiar” panel dopisuje przy wyłączonym
zapisie: `Zapis historii na flash jest w centrali wylaczony: po jej restarcie
historia zaczyna sie od zera`.

**Firebase nie jest używany.** Centrala wysyła tam telemetrię, ale jej nie
czyta, więc po restarcie z wyłączonym zapisem starszych danych na LCD nie ma.
Żeby historia przetrwała restart, zapis na flash musi być włączony (tak jest
domyślnie).

## Co zmienia 3.58.5

Wyłącznie liczenie okna czasu w kodzie `[v3.58.4 PANEL-WYKRESY]`
(`GET /api/history/panel`):

1. **Początek okna** to pierwsza sekunda zakresu, z której jest pomiar.
   W 3.58.4 okno zaczynało się od najstarszej próbki w buforach, także gdy
   leżała daleko przed zakresem.
2. **Koniec okna** to bieżący czas centrali, gdy od najnowszej próbki minęło
   więcej niż próg przerwy (6 minut; 90 minut, gdy najnowszy wpis pochodzi
   z bufora długiego). Czas bez pomiarów wychodzi jako przerwa. W 3.58.4
   okno zawsze kończyło się na najnowszej próbce.
3. Gdy w zakresie nie ma żadnego pomiaru, wszystkie pozycje to `#przerwa`
   (panel pisze wtedy `Centrala nie ma jeszcze danych z tego zakresu`).
4. Przebiegi, które ustalają okno, czytają tylko czas wiersza. Pełne
   przeliczenie wartości jest jedno (w 3.58.4 były dwa).

Bieżący czas to pola `aktualnaData*` / `aktualnyCzas*`, które `RTC()`
odświeża w pętli głównej co 2 s — z tego samego zegara, z którego czas
dostają próbki historii. Obsługa zapytania HTTP nie dotyka magistrali I2C.
Gdy centrala nie zna czasu albo zegar pokazuje czas wcześniejszy niż
najnowsza próbka, okno kończy się na najnowszej próbce, jak w 3.58.4.

### Przed i po (kod obu wersji uruchomiony na PC)

Pełna tabela: `WYNIKI/po-restarcie-przed-i-po.txt`
(skrypt `tests/porownaj_z_3.58.4.sh`).

| Po restarcie centrali | 3.58.4 | 3.58.5 |
|---|---|---|
| Pierwsza minuta, w buforach tylko stara historia (sprzed 2, 3, 40 dni) | wykres ze starych danych, kończący się na ostatnim starym pomiarze; w trybie godzinnym podpis osi to same godziny, więc nie widać, że dane są stare | godzina i doba: napis „brak danych z tego zakresu”; tydzień: gdy stara historia sięga ostatnich 7 dni — oś do bieżącej chwili, stara historia i przerwa do końca; gdy jest starsza — ten sam napis |
| 2 minuty, stara historia starsza niż zakres | godzina: 2 punkty i 94 przerwy; doba i tydzień: napis „jeden pomiar” | wykres z nowych pomiarów, oś od pierwszego z nich |
| Tryb dzienny, stara historia starsza niż doba | napis „jeden pomiar” przez pierwsze 15 minut | wykres od drugiego pomiaru |
| Tryb tygodniowy, stara historia starsza niż tydzień | napis „jeden pomiar” przez pierwsze 105 minut | wykres od drugiego pomiaru |
| Plików historii nie ma | napis do drugiego pomiaru, potem wykres | bez zmian |
| Zwykły restart z włączonym zapisem (ostatni wpis sprzed 4 minut) | pełny wykres | bez zmian |

Zrzuty ekranu panelu v1.3.0 z testu na PC (nie zdjęcia płytki), centrala
tuż po restarcie, zapis wyłączony, stare pliki sprzed 3 dni:
`WYNIKI/panel-po-restarcie-godzina.png` i `WYNIKI/panel-po-restarcie-tydzien.png`.

### Czego 3.58.5 nie zmienia w odpowiedziach

Gdy na początku zakresu jest pomiar albo cała historia jest od zakresu
krótsza, a historia nie kończy się w przeszłości, odpowiedź jest co do bajta
taka jak w 3.58.4. Sprawdzone na 9 wzorcach, z których korzystają testy
panelu, i na 1834 losowych zestawach danych: 892 mają być bez zmian i są
identyczne, 942 spełniają warunek zmiany i się różnią
(`tests/porownaj_z_3.58.4.sh`).

Różnica widoczna także poza restartem: gdy zakres zaczyna się w przerwie
(centrala nie pracowała akurat godzinę albo dobę temu), 3.58.4 zaczynała
wykres od pustego miejsca, a 3.58.5 zaczyna go od pierwszego pomiaru po tej
przerwie. Podpis osi podaje czas tego pomiaru.

---

## Co robi adres `/api/history/panel`

```
GET /api/history/panel?tryb=godz|dzien|tydz&pkt=96
```

Centrala sama wybiera zakres, dzieli go na `pkt` równych odcinków czasu
i odsyła gotowe punkty wykresu — najwyżej 96 wierszy, 18–20 kB. Panel do
v1.2.2 pobierał cały CSV historii (`/api/history`: do ok. 100 kB,
`/api/history/long`: do ok. 1,8 MB) i sam ściskał go do 96 punktów.

| `tryb` | Zakres | Skąd dane |
|---|---|---|
| `godz` | ostatnia godzina | bufor krótki (próbka co 60 s) |
| `dzien` | ostatnie 24 godziny | bufor krótki |
| `tydz` | ostatnie 7 dni | bufor długi (zdarzenia), a dla ostatniej doby bufor krótki — dokładniejszy |

Okno czasu: koniec to czas najnowszej próbki albo bieżący czas centrali
(punkt 2 wyżej); początek to pierwsza sekunda zakresu, z której jest pomiar.
Gdy historii jest mniej niż zakres, oś obejmuje tylko to, co jest.

**Skąd dane.** Z dwóch buforów historii w PSRAM — tych samych, z których
rysuje strona WWW centrali. Centrala odtwarza je przy starcie z plików
`/hist_short.csv` i `/hist_long.csv` i, gdy zapis jest włączony, dopisuje do
tych plików każdą nową próbkę. Obsługa zapytania czyta tylko pamięć.

**Jak liczone są punkty.**

- Oś czasu jest równa: każdy punkt to taki sam odcinek czasu
  (`krok_s` w nagłówku; dla pełnej doby ok. 15 minut, dla tygodnia 105 minut).
- Wartość próbki obowiązuje do następnej próbki. Bufor długi jest
  zdarzeniowy — brak wpisu znaczy „bez zmian” — więc punkt jest średnią
  ważoną czasem, a nie średnią z wpisów. Każdy punkt niesie też minimum
  i maksimum.
- Gdy odstęp między próbkami przekracza próg (6 minut w buforze krótkim,
  90 minut w długim — te same progi co na stronie WWW), odcinki bez danych
  wychodzą jako linie `#przerwa`. Panel rysuje tam przerwę w linii.
- Czujnik bez odczytu (NaN) daje w swoich kolumnach `nan`; pozostałe kolumny
  tego punktu są wypełnione.
- Rachunki są całkowitoliczbowe (dziesiąte części jednostki × sekundy), więc
  wynik nie zależy od zaokrągleń zmiennoprzecinkowych i jest taki sam w teście
  na PC i na ESP32.

**Odpowiedź** (`text/plain`):

```
#panel-hist 1 tryb=dzien pkt=96 wierszy=1440 od=1794484860 do=1794571200 krok_s=899 zrodlo=lokalne fs=1 od_etyk=12.11_13:01 do_etyk=13.11_13:00
13:15,-4.9,-5.0,-4.9,45.0,45.0,45.0,…,1794485758,0
#przerwa
…
#koniec 96
```

- nagłówek: `wierszy` — ile wierszy historii wzięto pod uwagę (całe bufory,
  nie tylko okno); `od`, `do` — początek i koniec okna (Unix UTC;
  0 = centrala nie zna czasu); `od_etyk`, `do_etyk` — to samo jako czas
  lokalny centrali, do podpisu osi; `fs` — 1, gdy historia jest też
  zapisywana na flash, 0, gdy zapis jest wyłączony;
- wiersz danych ma format wiersza `/api/history/long`: etykieta,
  11 × (średnia, min, max), czas, pompa — 36 kolumn;
- `#koniec N` — ostatnia linia; N to liczba wierszy i przerw. Panel po niej
  poznaje, że odpowiedź doszła cała.

Format jest ten sam co w 3.58.4.

Kody HTTP: `200`; `400` — zły `tryb`; `503` — centrali zabrakło pamięci na
odpowiedź (nigdy ucięta odpowiedź z kodem 200). `pkt` jest przycinane do
8…240, domyślnie 96.

W logu centrali każda taka prośba zostawia linię:
`tag=HISTORIA lvl=INFO msg="PANEL_WYKRES tryb=dzien pkt=96 wierszy=1440 punktow=96 przerw=0 bajtow=18207"`.

## Zmiany w liczbach

Względem **3.58.4**: 14 bloków diffu, 215 linii dodanych, 43 usunięte
(`main_centrala-v3.58.4-do-v3.58.5.patch`).

| Co | Linie |
|---|---|
| wpis changeloga na górze pliku | +50 (komentarz) |
| `FIRMWARE_VERSION` `"v3.58.4"` → `"v3.58.5"` | 1 zmieniona |
| wewnątrz bloku `[v3.58.4 PANEL-WYKRESY-BEGIN]` … `[-END]` | +164 / −42 |

Względem **3.58.3**: 4 bloki diffu, 623 linie dodane, 1 usunięta
(`main_centrala-v3.58.3-do-v3.58.5.patch`).

| Co | Linie |
|---|---|
| wpisy changeloga (3.58.5 i 3.58.4) | +83 (komentarz) |
| `FIRMWARE_VERSION` `"v3.58.3"` → `"v3.58.5"` | 1 zmieniona |
| blok `[v3.58.4 PANEL-WYKRESY-BEGIN]` … `[-END]` — liczenie punktów | +526 |
| trasa `GET /api/history/panel` | +13 |

Rozpisanie: `WYNIKI/zakres-zmian.txt`.

## Czego NIE zmieniono

- Formatu odpowiedzi `/api/history/panel` i jej trasy HTTP.
- `/api/history`, `/api/history/long`, `/history*` — strona WWW, aplikacja
  Android i starszy panel LCD korzystają z nich jak dotąd.
- Zapisu historii, plików `/hist_short.csv` i `/hist_long.csv`, przełącznika
  zapisu, NVS, Firebase, Telegrama, sterowania piecem, czujników,
  `/api/status`, `/api/lan/*`, OTA.
- Kod `PANEL-WYKRESY` tylko **czyta** bufory historii i pola zegara. Niczego
  nie zapisuje.

## Zgodność

| Panel LCD | Centrala | Ekran Wykresy |
|---|---|---|
| v1.3.0 | 3.58.5 | gotowe punkty z centrali; po restarcie centrali jak w tabeli „Przed i po” |
| v1.3.0 | 3.58.4 | gotowe punkty z centrali; po restarcie wady opisane wyżej |
| v1.3.0 | do 3.58.3 | centrala odpowiada na nowy adres przekierowaniem (302); panel pobiera cały CSV po staremu |
| do v1.2.2 | 3.58.4, 3.58.5 | panel nie zna nowego adresu; pobiera cały CSV po staremu |

Kolejność wgrywania panelu i centrali jest dowolna.

## Koszt po stronie centrali

- Odpowiedź: 18–20 kB tekstu (stara droga: do ok. 1,8 MB dla tygodnia).
- Pamięć robocza na czas jednego zapytania: ok. 22 kB na punkty (z PSRAM,
  a gdy jej brak — ze zwykłej sterty), zwalniane przed końcem obsługi
  zapytania, i ok. 22 kB na tekst odpowiedzi, który serwer HTTP trzyma do
  wysłania — jak przy każdej odpowiedzi tekstowej centrali. Bez pamięci →
  kod 503, bez wywrotki. Bez zmian względem 3.58.4.
- Liczenie: dwa przejścia po samych czasach wierszy i jedno pełne (najwyżej
  8000 + 1440 wierszy), bez operacji na flashu. Na PC pełne bufory to
  ok. 2 ms. **Na ESP32-S3 czas nie był mierzony.** Dotychczasowe
  `/api/history/long` przy pełnym buforze formatuje w tym samym zadaniu
  ok. 1,8 MB tekstu.
- Liczniki buforów są czytane pod tym samym spinlockiem co
  w `historiaZbudujCsv()`; same wiersze są czytane bez blokady. Jeśli
  w trakcie przyjdzie nowa próbka, może zmienić najwyżej najstarszy wiersz;
  czasy są przycinane do zakresu, więc żaden indeks nie wychodzi poza
  tablicę. Odczyt zegara (`mktime()`) jest poza spinlockiem.

## Jak sprawdzić po wgraniu

1. Strona WWW centrali → Ustawienia → „Sesja operatora”: `Wersja centrali:
   v3.58.5`.
2. W przeglądarce, w sieci centrali: `http://<IP>/api/history/panel?tryb=dzien`
   — tekst zaczynający się od `#panel-hist 1 tryb=dzien` i kończący linią
   `#koniec …`. Tak samo `tryb=godz` i `tryb=tydz`.
3. Na panelu LCD (v1.3.0): WYKRESY → wybór serii → `POKAZ WYKRES`. W logu
   panelu: `tag=WYKRESY zdarzenie=pobrano … sposob=centrala …`; w logu
   centrali linia `PANEL_WYKRES` (wyżej).
4. To, co zmienia ta wersja: zrestartuj centralę i po ok. 2–3 minutach otwórz
   na panelu wykres dzienny i tygodniowy. Ma być wykres — z pomiarów od
   restartu (zapis wyłączony) albo dotychczasowy (zapis włączony) — a nie
   napis „jeden pomiar”.
5. Strona WWW centrali → zakładka wykresów: bez zmian (korzysta ze starych
   adresów).

## Co zostało sprawdzone, a co nie

**Sprawdzone na PC:**

- **Liczenie punktów** — kod wycięty wprost z `src/main_centrala.cpp`
  (nie kopia), skompilowany `g++` i uruchomiony z przygotowaną historią:
  2697 sprawdzeń, także pod AddressSanitizer i UBSan. W tym porównanie
  z niezależnym, prostym liczeniem „sekunda po sekundzie” na 1834 losowych
  zestawach danych (934 jak w 3.58.4 i 900 nowych: stara historia, nowe
  pomiary, zegar centrali) — tekst odpowiedzi musi być identyczny co do
  znaku. Lista przypadków: `tests/README.txt`; wyniki: `WYNIKI/testy-pc.txt`,
  `WYNIKI/testy-pc-sanitizer.txt`.
- **Czułość testów** — 14 celowo wprowadzonych błędów w nowym kodzie (zły
  próg, odwrócony warunek, pominięta reguła); testy wykryły wszystkie 14
  (`WYNIKI/testy-czulosc.txt`).
- **Porównanie z 3.58.4** — `tests/porownaj_z_3.58.4.sh`: ten sam test
  skompilowany z kodem obu wersji; tabela sytuacji po restarcie i losowe
  zestawy (`WYNIKI/po-restarcie-przed-i-po.txt`).
- **Składnia i typy tego bloku prawdziwymi nagłówkami arduino-esp32**
  (3.0.7 i 3.3.8: `String`, `ps_malloc`, spinlock, `localtime_r`, `mktime`,
  `DLOGF`, pola zegara zadeklarowane tak jak w pliku centrali): bez błędów
  i bez ostrzeżeń clang (`OSTRZEZENIA=1 tests/sprawdz_naglowki_esp32.sh`,
  `WYNIKI/naglowki-esp32.txt`). To sprawdzenie samego bloku, kompilatorem
  clang, bez linkowania.
- **Zgodność z panelem v1.3.0** — firmware panelu bez zmian, uruchomiony
  na PC z odpowiedziami tej centrali: 11 wzorców
  (`LCD-Centrali/testy_host/wzorce_centrali/`, w tym dwa nowe: same przerwy
  i przerwa do końca osi) i scenariusz `wykresy_po_restarcie_centrali`.
  Do tego jednorazowo, poza skryptami paczki: 90 odpowiedzi z tabeli „po
  restarcie” podanych symulatorowi panelu — 70 razy wykres z tą liczbą
  punktów i tym podpisem osi, które podała centrala, 10 razy napis „brak
  danych”, 10 razy „jeden pomiar”.
- **Diff** — łatki nałożone na 3.58.3 i na 3.58.4 dają dokładnie ten plik
  (SHA-256 w `SHA256SUMS.txt`).

**Nie sprawdzone:**

- **Kompilacja całego pliku dla ESP32.** Plik ma ok. 49 tys. linii i wymaga
  bibliotek oraz plików `.h` projektu, których w repozytorium nie ma. Pierwszą
  kompilacją będzie Twój build w PlatformIO. Jeśli się nie powiedzie, przyślij
  komunikat błędu — zmiany względem 3.58.3 są w czterech miejscach pliku.
- **Rejestracja trasy** (9 linii kodu z 3.58.4) — używa tych samych wywołań
  ESPAsyncWebServer co sąsiednie trasy `/api/history*`, ale sama nie była
  kompilowana.
- **Działanie na płytce:** czas liczenia na ESP32-S3, zachowanie przy
  jednoczesnym zapisie próbki, prawdziwy brak pamięci, panel z prawdziwą
  centralą, zachowanie po prawdziwym restarcie (kiedy `RTC()` pierwszy raz
  ustawia pola zegara; do tej chwili okno kończy się na najnowszej próbce,
  jak w 3.58.4).
- **Zegar centrali przestawiony.** Gdy zegar skoczy do przodu o więcej niż
  próg przerwy, wykres kończy się na nowym czasie, a starsza historia jest
  przed przerwą albo poza zakresem. Sprawdzone tylko rachunkowo (testy 11 i 15).
- **Zgodność z Twoją wersją centrali.** Plik powstał z 3.58.3 — najnowszej
  wersji w repozytorium — przez 3.58.4. Jeśli na płytce jest inna wersja, nie
  podmieniaj pliku; napisz, jaka (widać ją na stronie WWW centrali i w logu
  przy starcie).
