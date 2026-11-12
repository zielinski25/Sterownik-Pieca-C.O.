# Firmware centrali 3.58.4 — gotowe punkty wykresów dla panelu LCD

> **Zastąpiona przez 3.58.5** (`../v3.58.5-panel-wykresy-po-restarcie/`).
> Wgraj tamtą paczkę zamiast tej; pasuje do projektu z wersją 3.58.3 albo
> 3.58.4. 3.58.4 po restarcie centrali, mając w pamięci tylko starą historię
> (na przykład z wyłączonym zapisem historii na flash), przez pierwszą minutę
> odsyłała panelowi stare dane jako bieżący zakres, a potem panel pisał
> „Za malo danych na wykres (jeden pomiar)” — na wykresie dziennym do
> 15 minut, na tygodniowym do 105. Ten katalog zostaje jako zapis tego, co
> było wydane, i jako punkt odniesienia dla testów 3.58.5.

Data: 2026-11-12 · baza: **3.58.3** (SHA-256 `src/main_centrala.cpp`:
`65e37b14349dd37771863d702cec4d90f289aa9d8ca98a811458d8c8bcb82147`, plik
`v3.58.0-LAN-PANEL/src/main_centrala.cpp` z gałęzi
`arena/fa9f8bd9-sterownik-pieca-c-o`).

Zmienia się **jeden plik**: `src/main_centrala.cpp`. Pliki `.h`, `secrets.h`,
biblioteki i `platformio.ini` bez zmian.

Powód: prośba „jak wybiorę wykresy [na LCD], to centrala ma pobrać dane
z Firebase albo lokalnie z pliku centrali i wysłać do panelu LCD”.

Druga połowa tej zmiany to firmware panelu **v1.3.0** (`LCD-Centrali/` w tym
repozytorium). Kolejność wgrywania jest dowolna — patrz „Zgodność”.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Co zostało sprawdzone, a co nie: rozdział na końcu. Instrukcja wgrania
> i powrotu do 3.58.3: `CZYTAJ-NAJPIERW.txt`.

---

## Co ta wersja robi

Nowy adres dla panelu LCD:

```
GET /api/history/panel?tryb=godz|dzien|tydz&pkt=96
```

Centrala sama wybiera zakres, dzieli go na `pkt` równych odcinków czasu
i odsyła gotowe punkty wykresu — najwyżej 96 wierszy, 18–20 kB. Do tej pory
panel pobierał cały CSV historii (`/api/history`: do ok. 100 kB,
`/api/history/long`: do ok. 1,8 MB) i sam ściskał go do 96 punktów.

| `tryb` | Zakres | Skąd dane |
|---|---|---|
| `godz` | ostatnia godzina | bufor krótki (próbka co 60 s) |
| `dzien` | ostatnie 24 godziny | bufor krótki |
| `tydz` | ostatnie 7 dni | bufor długi (zdarzenia), a dla ostatniej doby bufor krótki — dokładniejszy |

Koniec zakresu to czas najnowszej próbki. Gdy historii jest mniej niż zakres,
zakres zaczyna się od najstarszej próbki.

**Skąd dane.** Z dwóch buforów historii w PSRAM — tych samych, z których
rysuje strona WWW centrali. Centrala odtwarza je przy starcie z plików
`/hist_short.csv` i `/hist_long.csv` i dopisuje do tych plików każdą nową
próbkę. To są więc dane „z pliku centrali”, czytane z pamięci, bez dotykania
flasha w obsłudze zapytania.

**Dlaczego nie Firebase.** Centrala tylko wysyła tam telemetrię. Odczytanie
jej z powrotem wymagałoby internetu i pobrania ok. 270 kB na każdą dobę,
a dla tych trzech zakresów nie ma tam nic, czego centrala nie miałaby
u siebie. Panel bez internetu dostaje dzięki temu wykres zawsze, gdy ma
połączenie z centralą.

**Jak liczone są punkty.**

- Oś czasu jest równa: każdy punkt to taki sam odcinek czasu
  (`krok_s` w nagłówku; dla doby ok. 15 minut, dla tygodnia 105 minut).
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

- nagłówek: `wierszy` — ile wierszy historii wzięto pod uwagę; `od`, `do` —
  początek zakresu i czas najnowszej próbki (Unix UTC; 0 = centrala nie zna
  czasu); `od_etyk`, `do_etyk` — to samo jako czas lokalny centrali, do
  podpisu osi; `fs` — 1, gdy historia jest też zapisywana na flash;
- wiersz danych ma format wiersza `/api/history/long`: etykieta,
  11 × (średnia, min, max), czas, pompa — 36 kolumn;
- `#koniec N` — ostatnia linia; N to liczba wierszy i przerw. Panel po niej
  poznaje, że odpowiedź doszła cała.

Kody HTTP: `200`; `400` — zły `tryb`; `503` — centrali zabrakło pamięci na
odpowiedź (nigdy ucięta odpowiedź z kodem 200). `pkt` jest przycinane do
8…240, domyślnie 96.

W logu centrali każda taka prośba zostawia linię:
`tag=HISTORIA lvl=INFO msg="PANEL_WYKRES tryb=dzien pkt=96 wierszy=1440 punktow=96 przerw=0 bajtow=18207"`.

## Zmiany w liczbach

4 bloki diffu, 451 linii dodanych, 1 usunięta
(`main_centrala-v3.58.3-do-v3.58.4.patch`; rozpisanie:
`WYNIKI/zakres-zmian.txt`):

| Co | Linie |
|---|---|
| wpis changeloga na górze pliku | +33 (komentarz) |
| `FIRMWARE_VERSION` `"v3.58.3"` → `"v3.58.4"` | 1 zmieniona |
| blok `[v3.58.4 PANEL-WYKRESY-BEGIN]` … `[-END]` — liczenie punktów | +404 |
| trasa `GET /api/history/panel` | +13 |

## Czego NIE zmieniono

- `/api/history`, `/api/history/long`, `/history*` — strona WWW, aplikacja
  Android i starszy panel LCD korzystają z nich jak dotąd.
- Zapisu historii, plików `/hist_short.csv` i `/hist_long.csv`, NVS,
  Firebase, Telegrama, sterowania piecem, czujników, `/api/status`,
  `/api/lan/*`, OTA.
- Nowy kod tylko **czyta** bufory historii. Niczego nie zapisuje.

## Zgodność

| Panel LCD | Centrala | Ekran Wykresy |
|---|---|---|
| v1.3.0 | 3.58.4 | gotowe punkty z centrali (nowy adres) |
| v1.3.0 | starsza | centrala odpowiada na nowy adres przekierowaniem (302); panel pobiera cały CSV po staremu |
| do v1.2.2 | 3.58.4 | panel nie zna nowego adresu; pobiera cały CSV po staremu |

Dlatego kolejność wgrywania panelu i centrali jest dowolna.

## Koszt po stronie centrali

- Odpowiedź: 18–20 kB tekstu (stara droga: do ok. 1,8 MB dla tygodnia).
- Pamięć robocza na czas jednego zapytania: ok. 22 kB na punkty (z PSRAM,
  a gdy jej brak — ze zwykłej sterty), zwalniane przed końcem obsługi
  zapytania, i ok. 22 kB na tekst odpowiedzi, który serwer HTTP trzyma do
  wysłania — jak przy każdej odpowiedzi tekstowej centrali. Bez pamięci →
  kod 503, bez wywrotki.
- Liczenie: dwa przejścia po historii (najwyżej 8000 + 1440 wierszy), bez
  operacji na flashu. Na PC pełne bufory to ok. 3 ms. **Na ESP32-S3 czas nie
  był mierzony.** Dotychczasowe `/api/history/long` przy pełnym buforze
  formatuje w tym samym zadaniu ok. 1,8 MB tekstu.
- Liczniki buforów są czytane pod tym samym spinlockiem co
  w `historiaZbudujCsv()`; same wiersze są kopiowane bez blokady. Jeśli
  w trakcie przyjdzie nowa próbka, może zmienić najwyżej najstarszy wiersz;
  czasy są przycinane do zakresu, więc żaden indeks nie wychodzi poza
  tablicę.

## Jak sprawdzić po wgraniu

1. W przeglądarce, w sieci centrali: `http://<IP>/api/history/panel?tryb=dzien`
   — ma się pokazać tekst zaczynający się od `#panel-hist 1 tryb=dzien`
   i kończący linią `#koniec …`. Tak samo `tryb=godz` i `tryb=tydz`.
2. Na panelu LCD (v1.3.0): WYKRESY → wybór serii → `POKAZ WYKRES`. W logu
   panelu: `tag=WYKRESY zdarzenie=pobrano … sposob=centrala …`; w logu
   centrali linia `PANEL_WYKRES` (wyżej).
3. Strona WWW centrali → zakładka wykresów: bez zmian (korzysta ze starych
   adresów).

## Co zostało sprawdzone, a co nie

**Sprawdzone na PC:**

- **Liczenie punktów** — kod wycięty wprost z `src/main_centrala.cpp`
  (nie kopia), skompilowany `g++` i uruchomiony z przygotowaną historią:
  1434 sprawdzenia, także pod AddressSanitizer i UBSan. W tym porównanie
  z niezależnym, prostym liczeniem „sekunda po sekundzie” na 934 losowych
  zestawach danych — tekst odpowiedzi musi być identyczny co do znaku.
  Lista przypadków: `tests/README.txt`; wyniki: `WYNIKI/testy-pc.txt`,
  `WYNIKI/testy-pc-sanitizer.txt`.
- **Składnia i typy tego bloku prawdziwymi nagłówkami arduino-esp32**
  (3.0.7 i 3.3.8: `String`, `ps_malloc`, spinlock, `localtime_r`, `DLOGF`):
  bez błędów (`tests/sprawdz_naglowki_esp32.sh`, `WYNIKI/naglowki-esp32.txt`).
  To sprawdzenie samego nowego bloku, kompilatorem clang, bez linkowania.
- **Zgodność z panelem** — testy panelu v1.3.0 używają odpowiedzi wypisanych
  przez ten kod (`LCD-Centrali/testy_host/wzorce_centrali/`), a testy centrali
  pilnują, że nadal są takie same.
- **Diff** — łatka nałożona na 3.58.3 daje dokładnie ten plik (SHA-256
  w `SHA256SUMS.txt`).

**Nie sprawdzone:**

- **Kompilacja całego pliku dla ESP32.** Plik ma ok. 49 tys. linii i wymaga
  bibliotek oraz plików `.h` projektu, których w repozytorium nie ma. Pierwszą
  kompilacją będzie Twój build w PlatformIO. Jeśli się nie powiedzie, przyślij
  komunikat błędu — wszystkie zmiany są w czterech miejscach opisanych wyżej.
- **Rejestracja trasy** (9 linii kodu) — używa tych samych wywołań
  ESPAsyncWebServer co sąsiednie trasy `/api/history*`, ale sama nie była
  kompilowana.
- **Działanie na płytce:** czas liczenia na ESP32-S3, zachowanie przy
  jednoczesnym zapisie próbki, prawdziwy brak pamięci, panel z prawdziwą
  centralą.
- **Zgodność z Twoją wersją centrali.** Plik powstał z 3.58.3 — najnowszej
  wersji w repozytorium. Jeśli na płytce jest inna wersja, nie podmieniaj
  pliku; napisz, jaka (widać ją na stronie WWW centrali i w logu przy
  starcie).
