# Panel LCD (wyświetlacz CO) — firmware v1.4.0

Źródła panelu dotykowego (ESP32 + ILI9488 + LVGL), cienkiego klienta HTTP
centrali. Punkt wyjścia: v1.1.0 z `LCD Centrali.zip`.

## Budowanie

1. Do `src/` włóż plik `secrets.h`. Najprościej **skopiować `secrets.h`
   z projektu centrali** — panel używa z niego dwóch wpisów o tych samych
   nazwach: `SECRET_WIFI_AP_PASSWORD` i `SECRET_OTA_HASLO`. Można też skopiować
   `src/secrets.h.example` jako `src/secrets.h` i wpisać te dwie wartości
   ręcznie (w v1.1.0 były w `main.cpp` jako `WIFI_PASS` i `OTA_HASLO_PANEL`).
   Bez tego pliku kompilacja zatrzymuje się z komunikatem, co zrobić.
   `secrets.h` jest w `.gitignore`.
2. Reszta bez zmian: PlatformIO → Upload (`pio run -e esp32dev -t upload`).
   `platformio.ini` i `partitions.csv` są takie same jak w v1.1.0;
   `include/lv_conf.h` zmienił się w v1.2.2 (obsługa błędu biblioteki ekranu).

**Z v1.3.0 na v1.4.0** zmienia się pięć plików w `src/`: `main.cpp`,
`diagnostyka_panelu.h`, `siec_nieblokujaca_LCD.h`, `dashboard_lvgl_poc.cpp`,
`dashboard_lvgl_poc.h`. Z v1.2.2 (ta wersja jest na płytce) — te same pięć.
Najprościej podmienić cały katalog `src/` (zostawiając swoje `secrets.h`).

**v1.4.0 trzeba wgrać kablem.** Firmware na płytce (v1.2.2) ma stary kod
aktualizacji przez centralę; nowy zaczyna działać dopiero od tej wersji.

Katalog `testy_host/` nie jest częścią firmware'u — PlatformIO go pomija.

## Co zmienia v1.4.0

**Aktualizacja panelu przez centralę (Telegram `/update_panel`): w trakcie
pracy, z postępem na ekranie, bez wgrywania tego samego pliku w kółko.**

Powód: raport `RAPORT_OTA_PANELU_PRZEZ_CENTRALE.md` (katalog główny
repozytorium) i odpowiedź „Działaj”. Do kompletu jest firmware centrali
**3.58.6** (`firmware-patch/v3.58.6-panel-ota/`).

| | do v1.3.0 | v1.4.0 |
|---|---|---|
| Kiedy panel zaczyna | tylko przy starcie — pracujący panel trzeba było wyłączyć i włączyć | sam, w trakcie pracy, zwykle w ciągu minuty od `/update_panel` |
| Ekran w trakcie | pusty | napis `AKTUALIZACJA PANELU`, pasek postępu, procenty |
| Alarm | panel zaczynał także w czasie alarmu, a w trakcie pobierania nie pytał centrali o nic | w czasie alarmu nie zaczyna; alarm w trakcie przerywa pobieranie przy pierwszej odpowiedzi centrali |
| Ten sam plik drugi raz | przy każdym starcie, gdy tag wydania różnił się od wersji panelu | nigdy — panel pamięta, co wgrał |
| Nieudane pobieranie | panel stał do końca limitu czasu biblioteki | trzy próby, potem panel zgłasza centrali odmowę z przyczyną i pracuje dalej |
| Wynik | centrala poznawała go tylko po numerze wersji | panel zgłasza: który program działa, czego nie wgra i dlaczego, na co czeka |

Zrzut z testu na PC (nie zdjęcie płytki): `ekran_aktualizacja.png`.

### Jak to działa

1. Panel zna **identyfikator programu, z którego pracuje**: początek sumy
   SHA-256, którą narzędzie budujące dopisuje na końcu każdego programu ESP32.
   Podaje go centrali w każdym zapytaniu o status.
2. Centrala 3.58.6 podaje identyfikator pliku, który ma dla panelu. Równe —
   nie ma czego wgrywać. Różne — panel zaczyna, gdy może.
3. Panel zaczyna, gdy **wszystko** z tej listy jest spełnione:
   - nie ma alarmu i dane z centrali są świeże,
   - minęło 20 s od startu panelu,
   - nikt nie dotykał ekranu od 15 s,
   - nie trwa pobieranie wykresu ani wysyłanie polecenia,
   - wolnego RAM-u jest co najmniej 30 kB (z otwartym ekranem Wykresy jest
     mniej — trzeba z niego wyjść),
   - bateria ma co najmniej 30 %, gdy panel nie jest zasilany z USB,
   - od poprzedniej próby minęło 30 s.
   Czego brakuje, panel mówi centrali, a ona po 5 minutach pisze to
   w Telegramie.
4. Pobieranie nie zatrzymuje panelu: do 4 kB na obrót pętli, prosto do pamięci
   programu. Panel dalej pyta centralę o status, watchdog pilnuje pętli.
5. Przed uruchomieniem nowego programu panel sprawdza: nagłówek pliku
   (program dla ESP32, nie dla ESP32-S3 centrali), rozmiar, identyfikator
   równy temu z oferty, a na końcu cały obraz (suma kontrolna i SHA-256 —
   robi to ESP-IDF).
6. Restart. Nowy program jest zatwierdzany dopiero po narysowaniu ekranu
   i pierwszym odpytaniu centrali. Gdyby nie wstał, bootloader wraca do
   poprzedniego; panel to rozpoznaje (działa inny program niż wgrany), zgłasza
   centrali odmowę `cofnieta` i tej oferty więcej nie próbuje.

Co panel pamięta (pamięć trwała NVS, przestrzeń `otapanel`): którą ofertę
widział ostatnio, ile razy zaczynał ją pobierać, czy plik wgrał, czy nowy
program wystartował. Jedna oferta to najwyżej **3 próby**. Próba przerwana
alarmem albo wycofaniem oferty się nie liczy.

### Co wgrać

1. **Panel:** te źródła, **kablem** (`secrets.h` bez zmian).
2. **Centrala:** `firmware-patch/v3.58.6-panel-ota/` — zmienia się jeden
   plik, `src/main_centrala.cpp`; instrukcja w `CZYTAJ-NAJPIERW.txt` tej
   paczki. Plik pasuje do projektu z wersją 3.58.3, 3.58.4 albo 3.58.5.

Kolejność jest dowolna. Zwykła praca panelu (kafelki, wykresy, alarmy) nie
zależy od wersji centrali.

### Ze starszą centralą (do 3.58.5)

Panel v1.4.0 aktualizuje się tak samo — sam, w trakcie pracy, nie w czasie
alarmu, **raz na tag wydania**. Taka centrala nie umie jednak tego
potwierdzić (porównuje numer wersji z tagiem wydania), więc jej oferta
zostaje: `/update` i `/update_panel` odmawiają, a nadzór zadań centrali jest
wyłączony — do ręcznego zdjęcia oferty adresem
`http://<adres centrali>/api/status?panel_fw=<tag wydania>`. Dlatego
`/update_panel` warto używać dopiero z centralą 3.58.6.

### Pamięć (v1.4.0)

Pomiar w modelu 32-bitowym (`testy_host/pomiar_pamieci.sh --przypadki`),
przeliczony na płytkę z logu v1.2.2 — tak samo jak tabela dla v1.3.0 niżej.

| | Firmware bierze naraz | Wolny RAM na płytce (szacunek) |
|---|---|---|
| Po starcie | — | 48,2 kB (v1.3.0: 49,2 kB) |
| Zwykła praca, alarm, pozostałe ekrany, polecenie z dotyku | ok. 6,7 kB | najmniej ok. 38 kB |
| **Aktualizacja panelu** (pobieranie i zapis pliku 1,2 MB) | 11,5 kB | najmniej ok. 25,7 kB |
| Ekran Wykresy otwarty, bez wykresu | 24,5 kB | najmniej ok. 20 kB |
| Wykres na ekranie | do 25,5 kB | najmniej ok. 11,7 kB |

- Po starcie panel ma o ok. 1 kB mniej wolnego RAM-u niż v1.3.0: 0,9 kB
  zmiennych statycznych (drugi obiekt pobierania — osobny od wykresów, żeby
  jedno nie mogło przerwać drugiego — i stan aktualizacji) oraz 80 B na
  stercie.
- W trakcie aktualizacji najwięcej bierze bufor zapisu biblioteki `Update`
  (4 kB), odpytanie centrali, które idzie w tym samym czasie (5,5 kB),
  i nakładka z paskiem postępu.
- Po przerwanej aktualizacji zostaje 16 B; bufor i nakładka są zwalniane.
- Próg „bez jednej odmowy pamięci” się nie zmienił (24,9 kB dla firmware'u
  po starcie); płytka ma nad nim ok. 12,3 kB (v1.3.0: ok. 13 kB).

### Po wgraniu

W logu panelu (serial 115200) przy każdym starcie:

```
tag=OTA-PANEL zdarzenie=stan_przy_starcie obraz=<16 znaków> zatwierdzony=1 zapis_stan=0 zapis_prob=0 zapis_klucz=-
```

`obraz=nieznany` znaczy, że panel nie odczytał sumy własnego programu —
aktualizacja przez centralę jest wtedy wyłączona (pojawi się też linia
`zdarzenie=brak_identyfikatora_obrazu`).

**Jedno sprawdzenie zaraz po wgraniu kablem**, zanim zmienisz cokolwiek
w źródłach: wartość `obraz=` z tej linii ma być równa temu, co dla pliku
z tej samej kompilacji wypisze narzędzie (z katalogu głównego repozytorium):

```
python3 narzedzia/ota_panelu_sprawdz.py --tag 3.58.6 --bin LCD-Centrali/.pio/build/esp32dev/firmware.bin
...
          Identyfikator pliku: <16 znaków>
```

Centrala bierze identyfikator z końca pliku, panel — z własnej pamięci
programu. Że to ta sama wartość, wynika ze źródeł ESP-IDF; na płytce nie było
sprawdzane, a od tego zależy potwierdzenie aktualizacji. Jeśli wartości są
różne, nie wysyłaj `/update_panel` i przyślij obie.

Przebieg aktualizacji w logu panelu:

```
tag=OTA-PANEL zdarzenie=start wersja_docelowa=3.58.6 oferta=n1:a25319045fc6a202 proba=1/3 ram=… ram_blok=…
tag=OTA-PANEL zdarzenie=pobrano_ok obraz=a25319045fc6a202 bajtow=1200000 trwalo_ms=… nvs=1 restart_za_chwile
(restart)
tag=OTA-PANEL zdarzenie=nowy_firmware_dziala obraz=a25319045fc6a202 wersja=…
```

Pozostałe linie `tag=OTA-PANEL`: rozdział „Jak czytać log”.

## Co zmienia v1.3.0

**Ekran Wykresy: punkty wykresu liczy centrala, a pobieranie danych nie
zatrzymuje panelu.**

Powód: prośba „jak wybiorę wykresy, to centrala ma pobrać dane z Firebase albo
lokalnie z pliku centrali i wysłać do panelu LCD”.

Po dotknięciu `POKAZ WYKRES` panel pyta centralę o gotowe punkty:

```
GET /api/history/panel?tryb=godz|dzien|tydz&pkt=96
```

Centrala z firmware **3.58.4 lub nowszym** (aktualna paczka:
`firmware-patch/v3.58.5-panel-wykresy-po-restarcie/` w tym repozytorium) sama
wybiera zakres — ostatnia godzina, 24 godziny albo 7 dni — dzieli go na 96
równych odcinków czasu i odsyła najwyżej 96 wierszy (18–20 kB). Bierze je ze swojej lokalnej historii: tej, z której rysuje
strona WWW centrali i którą centrala dopisuje do plików `/hist_short.csv`
i `/hist_long.csv` na swoim flashu. **Firebase nie jest używany** — centrala
ma te dane u siebie, a Firebase wymagałby internetu i pobierania ok. 270 kB
na każdą dobę.

Centrala starsza niż 3.58.4 odpowiada na ten adres przekierowaniem (HTTP 302).
Panel pobiera wtedy cały CSV historii po staremu i rysuje z niego to samo co
v1.2.2. **Kolejność wgrywania panelu i centrali jest więc dowolna**, a sam
panel v1.3.0 działa też ze starą centralą.

| | v1.2.2 | v1.3.0 + centrala 3.58.4 | v1.3.0 + starsza centrala |
|---|---|---|---|
| Co panel pobiera | cały CSV historii: do ok. 100 kB (godzina, dzień), do ok. 1,8 MB (tydzień) | gotowe punkty, 18–20 kB | cały CSV, jak v1.2.2 |
| Kto liczy punkty wykresu | panel | centrala | panel; punkty te same co w v1.2.2 |
| Ekran i dotyk w czasie pobierania | stoją | działają | działają |
| Odpytywanie `/api/status`, ekran alarmu | czekają na koniec pobierania | działają | działają |
| Limity czasu | 8 s na odczyt; czego panel nie zdążył pobrać, tego na wykresie nie ma, bez informacji | 1,5 s na połączenie, 6 s ciszy, 20 s na całość; niepełna odpowiedź daje komunikat, nie wykres | te same limity; niepełne dane dają wykres z dopiskiem `NIEPELNE` |
| Oś czasu | punkty wg numeru wiersza historii | równa: każdy punkt to taki sam odcinek czasu | jak v1.2.2 |
| Zakres „Godzinny” | 60 punktów na 62% szerokości wykresu | 96 punktów na całej szerokości | jak v1.2.2 |
| Przerwa w pracy centrali | przerwa w linii na jednym punkcie | przerwa w linii o szerokości tej przerwy | jak v1.2.2 |
| Podpis pod wykresem | etykiety pierwszego i ostatniego punktu | zakres podany przez centralę, z datą: `12.11 13:01  -  13.11 13:00` | jak v1.2.2 |
| Gdy wykresu nie ma | zawsze `Brak danych (WiFi/centrala niedostepna)` | napis z przyczyną | napis z przyczyną |
| Wpis w logu | brak | linia `tag=WYKRESY` z wynikiem | to samo i `zdarzenie=stara_centrala` |
| Wyjście z wykresu w trakcie pobierania | niemożliwe, ekran stoi | zamyka połączenie | zamyka połączenie |

Kolumna „v1.2.2” opisuje jej kod. Na PC porównane jest rysowanie: z tego
samego CSV v1.3.0 rysuje punkt w punkt to samo co v1.2.2
(`testy_host/wzorce_v122/`). Zachowania sieci v1.2.2 testy na PC nie
odtwarzają.

Poza tym:

- pod napisem `Ladowanie danych z centrali...` widać, ile już doszło
  (`47 wierszy (9 kB)`);
- czujnik, który przez część zakresu nie dawał odczytu, ma w tym miejscu
  przerwę w swojej linii; pozostałe linie rysują się dalej;
- odpowiedź, która nie jest historią (np. strona logowania obcej sieci), daje
  napis. Kod v1.2.2 rysował z takiej strony wykres;
- usunięte zostało ostatnie blokujące pobieranie w panelu
  (`wykresyFetchWykonaj()`); `HTTPClient` służy już tylko aktualizacji OTA.

Zrzuty z testów na PC (nie zdjęcia płytki): `ekran_wykres_dzien.png` — wykres
dzienny z punktów centrali, `ekran_wykres_tydzien_przerwa.png` — tydzień,
w którym centrala nie pracowała przez 1,5 doby, `ekran_wykres_brak_danych.png`
— najdłuższy z napisów pokazywanych zamiast wykresu.

### Co wgrać

1. **Panel:** te źródła, jak zwykle (`secrets.h` bez zmian).
2. **Centrala:** `firmware-patch/v3.58.5-panel-wykresy-po-restarcie/` —
   zmienia się jeden plik, `src/main_centrala.cpp`; instrukcja
   w `CZYTAJ-NAJPIERW.txt` tej paczki. Plik pasuje do projektu z wersją
   3.58.3 albo 3.58.4. Paczka 3.58.5 zastępuje 3.58.4 (poprawia wykresy po
   restarcie centrali — niżej); firmware panelu jest ten sam.

Dopóki centrala ma starszy firmware, wykresy działają po staremu (trzecia
kolumna tabeli).

### Gdy centrala ma wyłączony zapis historii na flash

Przełącznik „Zapisuj historię na flash centrali” (strona WWW centrali →
Ustawienia) wyłącza tylko zapis do plików. Historia w pamięci centrali zbiera
się dalej i to z niej centrala liczy punkty dla panelu.

| Sytuacja | Ekran Wykresy |
|---|---|
| Centrala pracuje, zapis wyłączony | wykresy takie same jak z włączonym zapisem |
| Restart centrali, zapis wyłączony, plików historii nie ma | historia od zera: napis `Centrala nie ma jeszcze danych z tego zakresu`, po pierwszym pomiarze `Za malo danych na wykres (jeden pomiar)`, od drugiego pomiaru (ok. 2 minuty po starcie) wykres, który wydłuża się z każdą minutą |
| Restart centrali, zapis wyłączony, na flashu zostały stare pliki | centrala wczytuje je przy starcie; na wykresie jest to, co z nich mieści się w zakresie, potem przerwa i nowe pomiary |

Pod oboma napisami panel dopisuje wtedy: `Zapis historii na flash jest
w centrali wylaczony: po jej restarcie historia zaczyna sie od zera`.
Firebase nie jest używany, więc starszych danych po takim restarcie na LCD
nie ma.

Trzeci wiersz tabeli działa tak od centrali **3.58.5**. Centrala 3.58.4,
mając w pamięci tylko starą historię, w pierwszej minucie po restarcie
odsyłała ją jako bieżący zakres. Potem panel pisał „jeden pomiar”: na
wykresie dziennym przez 15 minut (gdy stara historia była starsza niż doba),
na tygodniowym przez 105 minut (gdy była starsza niż tydzień). Opis i tabela
„przed i po”: `README.md` paczki 3.58.5.

### Czy RAM-u starczy

Sprawdzone 2026-11-12 na PC, przeliczone na płytkę z logu v1.2.2. Pełny
opis, także dla centrali: `RAPORT_PAMIEC_RAM_LCD_I_CENTRALA.md` w katalogu
głównym repozytorium. **Liczby w tej tabeli są dla v1.3.0**; dla v1.4.0 —
tabela „Pamięć (v1.4.0)” wyżej (różnica: ok. 1 kB mniej wolnego RAM-u).

| | Firmware bierze naraz | Wolny RAM na płytce |
|---|---|---|
| Po starcie | — | 49,2 kB (v1.2.2 zmierzone: 49,7 kB) |
| Zwykła praca, alarm, pozostałe ekrany, polecenie z dotyku | ok. 6,6 kB | najmniej ok. 39 kB (v1.2.2 zmierzone: 41,6 kB) |
| Ekran Wykresy otwarty, bez wykresu | 24,5 kB | najmniej ok. 21 kB |
| Wykres na ekranie — każdy zakres, 8 serii, także napis zamiast wykresu, błąd, starsza centrala | do 25,4 kB | najmniej ok. 13 kB |

„Firmware bierze naraz” to pomiar w modelu 32-bitowym: pamięć ponad stan po
starcie, razem z odpytaniem centrali, które wypadło w tej samej chwili.
„Wolny RAM na płytce” to **szacunek**: 49,2 kB minus to, minus pamięć sieci
płytki (odpytania do 3,5 kB — z logu v1.2.2; pobieranie wykresu do 7,5 kB —
z ustawień lwIP, nie zmierzone). Prawdziwą liczbę poda `ram_min=` w linii
`tag=WYKRESY zdarzenie=pobrano`.

Wszystko działa bez jednej odmowy pamięci, dopóki firmware ma po starcie co
najmniej 24,9 kB wolnego RAM-u. Płytka ma 49,2 kB minus do 11 kB sieci, czyli
ok. 13 kB ponad ten próg. Poniżej progu panel w modelu się nie zawiesza:
zależnie od tego, ile pamięci zostało, część odpytań centrali kończy się
napisem `BRAK PAMIECI`, wykres zastępuje napis `Brak pamieci na wykres` albo
panel restartuje się z wpisem `zdarzenie=lvgl_assert` w logu.

### Po wgraniu

Otwórz WYKRESY, wybierz serie, dotknij `POKAZ WYKRES`. W logu panelu pojawi
się jedna z dwóch par linii:

```
tag=WYKRESY zdarzenie=start tryb=dzien ram=…
tag=WYKRESY zdarzenie=pobrano tryb=dzien sposob=centrala punktow=96 przerw=0 wierszy_centrali=1440 linii=98 ucietych_linii=0 bajtow=18207 ms=… ram=… ram_min=…
```

albo, gdy centrala ma jeszcze starszy firmware:

```
tag=WYKRESY zdarzenie=stara_centrala kod=302 msg="centrala nie zna /api/history/panel (firmware starszy niz 3.58.4) - pobieram cala historie po staremu"
tag=WYKRESY zdarzenie=pobrano tryb=dzien sposob=panel punktow=… linii=1440 … bajtow=… ms=… ram=… ram_min=…
```

`sposob=centrala` znaczy, że punkty policzyła centrala. **Przyślij te linie**
z pierwszych kilku wykresów (dzień, godzina, tydzień): `ms=` to czas
pobierania na płytce, a `ram_min=` najmniejszy wolny RAM od startu. Tych dwóch
liczb testy na PC nie dają.

Centrala (3.58.4 i nowsze) pisze przy każdej takiej prośbie w swoim logu:
`tag=HISTORIA lvl=INFO msg="PANEL_WYKRES tryb=dzien pkt=96 wierszy=1440 punktow=96 przerw=0 bajtow=18207"`.

## Co zmienia v1.2.2

**Przyczyna znaleziona: panelowi brakowało pamięci RAM.** Pokazał to log
panelu z 2026-11-12 (v1.2.0):

- po starcie prawie każda odpowiedź centrali kończyła się linią
  `tag=STATUS lvl=ERR zdarzenie=odpowiedz_nieczytelna … json=NoMemory bajtow=1811`.
  Odpowiedź przyszła cała, ale panel nie miał gdzie jej rozpakować. Kafelki
  pokazywały `--` przez ok. 18 minut;
- linie `tag=PANEL` podawały `heap=47032 heap_min=39624 blok_max=38900`,
  z `blok_max` identycznym przez godzinę. Te liczby doliczały ok. 39 kB
  obszaru, z którego program nie może brać pamięci (opis w `src/main.cpp`
  przy `ramWolny()`). Naprawdę wolne było ok. 8 kB, a w najgorszej chwili
  poniżej 1 kB;
- wcześniejszy restart w tym logu (`task_wdt … loopTask`) to najprawdopodobniej
  ta sama przyczyna: biblioteka ekranu (LVGL) po nieudanym przydziale pamięci
  staje w miejscu, a po 15 s panel restartuje watchdog. Pewności nie ma —
  adresów ze śladu wywołań nie da się tu zamienić na nazwy funkcji.

v1.1.0 zajmowała tyle samo pamięci, tylko o jej braku nie pisała: nieudane
rozpakowanie odpowiedzi kończyło się po cichu, a kafelki zostawały ze starymi
wartościami. To najpewniej tłumaczy pierwsze zgłoszenie („kafelki się nie
aktualizują”) — logu panelu z tamtej chwili nie ma, więc to wniosek z kodu
i z pomiaru, nie obserwacja.

| | v1.2.0 | v1.2.2 |
|---|---|---|
| Pamięć zajęta przez firmware po starcie (pomiar na PC) | 171 kB | 129 kB |
| Wolny RAM na płytce po starcie | ok. 8 kB (z logu) | 49,7 kB (z logu; szacunek z PC: 48,6 kB) |
| Bufor rysowania ekranu | 38,4 kB | 15,4 kB |
| Bufory ekranu Wykresy (18,5 kB) | trzymane zawsze | tylko gdy ekran jest otwarty |
| Pamięć potrzebna chwilowo na jedno odpytanie centrali | 7,2 kB | 5,4 kB (odpowiedź nie jest już kopiowana) |
| Wolna pamięć w logu | `heap=` — zawyżone o ok. 39 kB | `ram=` — prawdziwa |
| Brak pamięci na odczytanie odpowiedzi | kafelki `--`, w logu `przyczyna=json` (wygląda na winę centrali), bez restartu | nagłówek `BRAK PAMIECI`, `przyczyna=pam`, restart po 45 s |
| Brak pamięci na odebranie odpowiedzi | odpowiedź po cichu ucięta, w logu wygląda jak błąd centrali | `GET nieudany: brak pamieci na odpowiedz`, `przyczyna=pam` |
| Linie logu przy braku pamięci | linie od 64 znaków mogą przepaść | najważniejsze linie wypisują się zawsze |
| LVGL nie dostaje pamięci | ekran zamiera na 15 s, restart przez watchdog | napis `BRAK PAMIECI - RESTART`, restart po 2,5 s, powód w logu centrali |

Obraz na ekranie jest taki sam (20 zrzutów z testów na PC, piksel w piksel).
Mniejszy bufor rysowania oznacza więcej, niższych pasków przy przerysowaniu
całego ekranu; czy to widać na płytce, nie zostało sprawdzone.

Od ok. 19. minuty tego logu każda próba kończyła się limitem czasu, przy
połączonym WiFi. Z logu nie da się rozstrzygnąć, czy panel gubił odpowiedzi
z braku pamięci, czy centrala przestała mu odpowiadać. W obu przypadkach
panel wychodzi z tego sam, mechanizmami z v1.2.1 (niżej).

**Po wgraniu** w logu panelu pojawi się raz linia
`tag=PAMIEC zdarzenie=po_starcie ram=… ram_blok=… ram_min=…`. `ram` powinno
wynosić kilkadziesiąt tysięcy (bajtów). To jest sprawdzian tej wersji: jeśli
będzie poniżej 20 000, przyślij tę linię.

### Wynik na płytce (2026-11-12)

Log panelu z pierwszych ok. 40 s po wgraniu v1.2.2 (zbudowanej w PlatformIO
przez użytkownika):

```
[LVGL] start lvgl_port_init(), wolny RAM=234196 B
[LVGL] bufor rysowania zaalokowany (15360 B), wolny RAM=218148 B
tag=PAMIEC zdarzenie=po_ekranach ram=105568 ram_blok=102388
tag=PAMIEC zdarzenie=po_starcie ram=49676 ram_blok=42996 ram_min=43836
tag=PANEL fw=v1.2.2 … status_ok=1 status_blad=0 ust_ok=1 ust_blad=0 … ram=50516 ram_min=43836 ram_blok=42996 … autorestartow=0
tag=PANEL fw=v1.2.2 … status_ok=15 status_blad=0 ust_ok=6 ust_blad=0 … ram=50076 ram_min=41644 ram_blok=42996 … autorestartow=0
```

- Po starcie wolne jest 49,7 kB (v1.2.0: ok. 8 kB). Najmniej w tym czasie:
  41,6 kB.
- Wszystkie odpowiedzi zostały odczytane: 16 ze statusem i 7 z ustawieniami,
  zero błędów, żadnego `json=NoMemory`, żadnego ponownego łączenia ani
  restartu.
- Panel zauważył zmianę symulacji w centrali (`czujniki=zewn` →
  `czujniki=zewn,piec`), czyli dostaje świeże dane.
- Według zgłoszenia ekran działa prawidłowo.

To krótki log. Pracy przez wiele godzin jeszcze nie pokazuje.

## Co zmienia v1.2.1

Powód: zgłoszenie z 2026-11-12 — panel z v1.2.0 nie pokazywał świeżych
danych, aż został zresetowany. Gdy powstawała v1.2.1, był tylko log centrali
i przyczyna nie była znana; pokazał ją dopiero log panelu (v1.2.2 wyżej).
Wszystko, co poniżej, jest także w v1.2.2, v1.3.0 i v1.4.0.

Gdy panel nie ma danych z centrali, sam robi to, co dotąd pomagało:

| Czas bez poprawnego statusu | Co robi panel |
|---|---|
| 10 s | kafelki `--` (jak w v1.2.0) i **powód w nagłówku**: `BRAK WIFI`, `CENTRALA MILCZY` albo `ZLA ODPOWIEDZ`, z licznikiem sekund |
| 12 s, gdy WiFi jest „połączone”, a centrala milczy | rozłącza się i łączy od nowa; po kolejnych 12 s jeszcze raz, szukając sieci centrali od zera. v1.1.0 i v1.2.0 w tym stanie nie robiły nic |
| 45 s | **restartuje się sam**; ostatnie 10 s nagłówek odlicza `RESTART ZA 7 s: …` |
| nadal bez danych | kolejne restarty po 90 s, 3 min, potem co 5 min (centrala wyłączona na dłużej nie powoduje restartów w kółko); licznik wraca do 45 s po 2 minutach poprawnych danych |

Gdy centrala odpowiada, ale nieczytelnie (`ZLA ODPOWIEDZ`), restarty są
najwyżej dwa z rzędu — trzeci nic by nie dał.

Ten sam napis co w nagłówku stoi teraz także **na ekranie alarmu**, nad
przyciskami WYCISZ / ZAMKNIJ (`ekran_alarm_bez_danych.png`). W logu centrali
z 2026-11-12 alarm przegrzania był aktywny (`piec=134.0C` na płytce bez
czujników). Przy takim statusie panel pokazuje pełnoekranowy alarm, który
zasłania kafelki i nagłówek — nie było na nim widać, czy dane jeszcze
przychodzą, ani że temperatura pochodzi z symulacji. Teraz stoi tam
`BRAK WIFI 14 s` albo `SYMULACJA: piec`.

Po takim restarcie panel **mówi centrali, dlaczego go zrobił**, a centrala
wpisuje to do swojego logu (i wysyła wiadomość na Telegram, tak jak o każdym
nietypowym restarcie panelu):

```
tag=RESET-DIAG lvl=WARN zdarzenie=nietypowy_restart_panelu powod=auto_wifi_r201_n1_t45_h61_b31_en118 poprzedni=poweron
```

Jeśli problem wróci, **ta linia z logu centrali powie, czego panelowi
brakowało** — rozpisanie pól jest niżej, w rozdziale „Jak czytać log”.

Każdy taki restart centrala ogłasza raz (po jej pierwszej odpowiedzi panel
wraca do zwykłego `reset_powod=sw`). Gdy restarty idą seriami, zgłaszane są
pierwsze trzy, a potem 6., 12., 24.… z serii — żeby łączność, która rwie się
raz za razem, nie dawała wiadomości co kilka minut. Serię kończy godzina
poprawnych danych; numer w serii jest w zgłoszeniu (`n6`).

Alarm nie ginie przy takim restarcie: stan alarmu (dym, przegrzanie pieca
lub panelu, WYCISZ, ZAMKNIJ) jest zapamiętywany przed restartem, a brzęczyk
i ekran alarmu wracają po nim, zanim przyjdą dane.

Przy okazji naprawione trzy błędy brzęczyka (były też w v1.1.0 i v1.2.0):

- WYCISZ działał dopiero po następnym poprawnym statusie — czyli bez danych
  z centrali wcale; teraz cisza zapada od razu;
- drugie naciśnięcie WYCISZ włączało dźwięk z powrotem;
- raz wyciszony rodzaj alarmu zostawał niemy przy każdym następnym
  wystąpieniu, aż do restartu panelu; teraz wyciszenie kończy się razem
  z alarmem.

## Co zmienia v1.2.0

Powód: zgłoszenie „kafelki się nie aktualizują”. **Przyczyna zgłoszenia nie
jest potwierdzona** — log v1.1.0 nie mówi, co panel odczytał ani co pokazał.
v1.2.0 usuwa błędy, które dają taki objaw, i dodaje to, czego w logu brakowało.

| Obszar | v1.1.0 | v1.2.0 |
|---|---|---|
| Odpowiedź nie do odczytania (zły kod HTTP, zły JSON) | cicho pomijana, kafelki ze starymi wartościami | linia `lvl=ERR` z przyczyną; po 10 s kafelki pokazują `--` |
| Utrata danych w trakcie pracy | kafelki i wirnik pompy bez zmian | po 10 s `--`, wirnik staje |
| Symulacja czujników w centrali | nie do zauważenia na LCD | napis `SYMULACJA: zewn` w nagłówku + wpis w logu |
| Wybór centrali | pierwsza znaleziona sieć `PiecCO_AP` | skan przy starcie, połączenie z najsilniejszą; BSSID w logu |
| Zapamiętana centrala zniknęła | panel nie łączy się z żadną inną | po 3 próbach szuka od nowa |
| Brak centrali przy włączeniu | watchdog restartuje panel co ok. 15 s | panel czeka i łączy się, gdy centrala wstanie |
| Nieudany POST | linia błędu w każdym obrocie pętli | jedna linia |
| Log przy zwykłej pracy | 3 linie `[NET-DIAG]` co 2 s | jedna linia `tag=PANEL` co 30 s |
| Wskaźnik baterii bez ogniwa | `0%` w kolorze ładowania | `USB` |
| Hasła | wpisane w `main.cpp` | w `src/secrets.h`, poza gitem |

Kolumna „v1.1.0” pochodzi z czytania kodu i z testu na PC, nie z obserwacji
na płytce — z wyjątkiem wierszy o logu i o baterii, które widać w logach
z 10 i 11.11. Pełny opis: `HISTORIA ZMIAN` na górze `src/main.cpp`.

## Logi z 2026-11-11 (panel + centrala na gołej płytce)

- **Panel na biurku ma starszy build niż te źródła.** Jeden nieudany odczyt
  ustawień daje w jego logu setki linii `[USTAWIENIA] GET nieudany: timeout
  calkowity`; źródła z `LCD Centrali.zip` (i v1.2.0) piszą jedną. W adresie
  zapytań widać też `reset_powod=task_wdt`, czyli wcześniejszy restart przez
  watchdog.
- Łączność działa: po włączeniu 54 z 56 odczytów `/api/status` udanych.
- Centrala podawała: `piec=-1.0C bojler=-1.0C panel=-1.0C pom=-1.0C
  zewn=18.5C cisnienie=0.0hPa wilgotnosc=0.0% klapa=0st syberek=16st dym=2413`
  — brak czujników (−1) i jedna wartość z symulacji (zewn).
- Tak wygląda ekran v1.2.0 dla tych danych (zrzut z testu na PC, nie zdjęcie
  płytki): `ekran_gola_plytka.png`.

## Jak czytać ekran

Środek nagłówka dashboardu (między baterią a ikonami po prawej):

- **nic** — jedna centrala, żadnej symulacji. Stan normalny.
- **`SYMULACJA: zewn, pokoj`** — centrala podaje te czujniki jako symulowane
  (włączone z jej panelu WWW). Ich wartości na kafelkach nie są pomiarem.
  Napis pojawia się i znika razem z symulacją w centrali, najdalej po kilku
  sekundach — to najprostszy znak, że panel czyta świeże dane.
- **`2 CENTRALE (XX:YY)`** — przy włączeniu panelu w zasięgu były co najmniej
  dwie sieci `PiecCO_AP`. `XX:YY` to końcówka adresu (BSSID) tej, z którą
  panel jest połączony.
- **`SYMULATOR`** — odpowiada `centrala-testowa-c3`.

Kafelki z `--` znaczą: od 10 s panel nie odczytał poprawnego statusu.
W tym samym miejscu nagłówka stoi wtedy powód (v1.2.1):

- **`BRAK WIFI 14 s`** — panel nie jest połączony z siecią centrali
  (ikona WiFi po prawej jest przekreślona).
- **`CENTRALA MILCZY 14 s`** — WiFi jest połączone, ale centrala nie
  odpowiada na zapytania.
- **`ZLA ODPOWIEDZ 14 s`** — centrala odpowiada, ale treści nie da się
  odczytać (szczegóły w logu: `tag=STATUS lvl=ERR`).
- **`BRAK PAMIECI 14 s`** (v1.2.2) — odpowiedź centrali przyszła, ale
  panelowi zabrakło pamięci RAM, żeby ją odczytać. Wina leży w panelu.
- **`RESTART ZA 7 s: BRAK WIFI`** — za tyle sekund panel zrestartuje się sam.

Napis **`AKTUALIZACJA PANELU`** z paskiem postępu, na całym ekranie (v1.4.0):
panel pobiera z centrali nowy firmware. Po pobraniu restartuje się sam.
Alarm przerywa pobieranie i ekran alarmu wraca. Opis: „Aktualizacja przez
centralę i Telegram (OTA)” niżej; zrzut z testu na PC:
`ekran_aktualizacja.png`.

Czerwony pasek **`BRAK PAMIECI - RESTART`** na górze ekranu (v1.2.2):
bibliotece ekranu zabrakło pamięci w trakcie rysowania; panel restartuje się
po 2,5 s. `BLAD EKRANU - RESTART` to ten sam restart przy innym błędzie
wewnętrznym tej biblioteki.

Tak to wygląda (zrzuty z testu na PC, nie zdjęcia płytki):
`ekran_centrala_milczy.png` — dashboard, `ekran_alarm_bez_danych.png` — ekran
alarmu (ten sam napis nad przyciskami; od v1.2.1).

Kafelek z `-1°C` znaczy: centrala nie ma odczytu z tego czujnika.

### Ekran Wykresy (v1.3.0)

Zamiast wykresu może stać jeden z napisów:

| Napis | Co się stało |
|---|---|
| `Ladowanie danych z centrali...` i pod spodem `47 wierszy (9 kB)` | pobieranie trwa. Z centralą 3.58.4 zwykle nie zdąży się pokazać; ze starszą widać, że dane płyną |
| `Centrala nie ma jeszcze danych z tego zakresu (pierwszy pomiar zapisuje minute po starcie)` | centrala odpowiedziała, ale jej historia jest pusta albo (centrala 3.58.5) w wybranym zakresie nie ma jeszcze żadnego pomiaru |
| …z dopiskiem `Zapis historii na flash jest w centrali wylaczony: po jej restarcie historia zaczyna sie od zera` | jak wyżej, a centrala trzyma historię tylko w pamięci (przełącznik na jej stronie WWW) |
| `Za malo danych na wykres (jeden pomiar)` | centrala ma dopiero jeden pomiar; linia potrzebuje dwóch |
| `Brak polaczenia z siecia centrali (WiFi)` | panel nie jest połączony z siecią centrali; zapytanie nie wyszło |
| `Centrala nie przyjela polaczenia` | WiFi jest, ale nie udało się połączyć z centralą |
| `Centrala nie odpowiedziala w czasie` | 6 s bez odpowiedzi albo bez dalszych danych |
| `Odpowiedz centrali urwala sie w polowie` | połączenie zerwane przed końcem odpowiedzi; wykres z części punktów nie jest rysowany |
| `Centrali zabraklo pamieci na wykres (HTTP 503)` | centrala 3.58.4 nie dostała pamięci na policzenie punktów |
| `Centrala odpowiedziala bledem HTTP 500` | inny błąd po stronie centrali, z kodem |
| `Centrala odpowiedziala czyms innym niz dane wykresu` | pod adresem centrali odpowiada coś, co nie zwraca historii |
| `Panelowi zabraklo pamieci na wykres`, `Brak pamieci na wykres (zamknij ekran Wykresy i otworz ponownie)` | brak RAM-u w panelu |

Przy każdym z nich w logu jest linia `tag=WYKRESY` z tą samą przyczyną
i szczegółami (niżej).

Podpis pod wykresem:

- `12.11 13:01  -  13.11 13:00` — zakres osi czasu podany przez centralę
  (lewa i prawa krawędź wykresu), w czasie lokalnym centrali;
- `czas nieznany (zegar centrali nieustawiony)` — centrala nie zna godziny;
  punkty są w dobrej kolejności, bez dat;
- czerwony dopisek `NIEPELNE` (tylko ze starszą centralą) — cały CSV nie
  doszedł w 20 s albo połączenie się urwało; wykres pokazuje najstarszą część
  danych.

## Jak czytać log

```
tag=WIFI zdarzenie=skan_centrala nr=1/1 bssid=24:6F:28:11:22:33 kanal=1 rssi=-45
tag=WIFI zdarzenie=polaczono bssid=24:6F:28:11:22:33 kanal=1 rssi=-45
tag=PANEL zdarzenie=centrala_rozpoznana typ=sterownik
tag=PANEL zdarzenie=symulacja_centrali czujniki=zewn
tag=PAMIEC zdarzenie=po_ekranach ram=... ram_blok=...
tag=PAMIEC zdarzenie=po_starcie ram=... ram_blok=... ram_min=...
tag=PANEL fw=v1.2.2 wifi=1 bssid=24:6F:28:11:22:33 kanal=1 rssi=-45 ap_przy_starcie=1 centrala=sterownik sym=zewn
          status_ok=15 status_blad=0 ust_ok=6 ust_blad=0 ekran=15 wiek_danych_s=1
          t_zewn=18.5 t_pokoj=-1.0 t_ogrz=-1.0 t_ogrz_sr=0.0 t_bojler=-1.0 t_panel=-1.0
          wilg=0 cisn=0 dym=2413 klapa=0 syberka=16 pompa=0 czas=20:41 ram=... ram_min=... ram_blok=...
          bez_danych_s=1 przyczyna=- wymuszen=0 rozl_kod=0 rozl_n=0 autorestartow=0
```

(Przykład z testu na PC; linia `tag=PANEL fw=…` jest w logu jedną linią.)

- `tag=PANEL fw=…` — 5 s po starcie i potem co 30 s:
  - `t_zewn`, `t_pokoj`, `t_ogrz`… `dym`, `klapa`, `syberka` — wartości, które
    panel ma w pamięci i podaje kafelkom. **Porównaj je z linią `tag=STATUS`
    w logu centrali** (`zewn=`, `pom=`, `piec=`…): mają być takie same;
  - `status_ok` / `status_blad` — ile odpowiedzi `/api/status` udało się
    odczytać, a ile odrzucono od poprzedniej takiej linii (zdrowo: 15 i 0);
  - `ekran` — ile razy w tym czasie dane poszły na kafelki;
  - `sym` — czujniki, które centrala podaje jako symulowane;
  - `ram`, `ram_min`, `ram_blok` (v1.2.2) — wolny RAM w bajtach, jego
    minimum od startu i największy ciągły wolny blok. Zdrowy panel ma tu
    kilkadziesiąt tysięcy; poniżej 16 000 zaczynają się kłopoty. W v1.2.0
    i v1.2.1 te pola nazywały się `heap`, `heap_min`, `blok_max` i były
    zawyżone o ok. 39 000.
- `tag=PAMIEC zdarzenie=po_ekranach` / `po_starcie` (v1.2.2) — raz, przy
  starcie: ile RAM-u zostało po zbudowaniu ekranów (przed WiFi) i po
  odebraniu pierwszych danych.
- `tag=PAMIEC lvl=WARN zdarzenie=malo_pamieci ram=… ram_blok=… ram_min=…`
  (v1.2.2) — wolnego RAM-u jest mniej niż 16 000; najwyżej raz na minutę.
- `tag=PANEL lvl=ERR zdarzenie=lvgl_assert ram=… ram_blok=… ram_min=…`
  (v1.2.2) — bibliotece ekranu zabrakło pamięci (albo wykryła inny błąd
  wewnętrzny); panel restartuje się od razu.
- `[STATUS] GET nieudany: brak pamieci na odpowiedz errno=12 ENOMEM` (v1.2.2)
  — panelowi zabrakło pamięci już przy odbieraniu odpowiedzi.
- `tag=PANEL zdarzenie=symulacja_centrali czujniki=…` — centrala włączyła
  albo wyłączyła symulację czujnika (`brak` = żadna).
- `tag=STATUS lvl=ERR zdarzenie=odpowiedz_nieczytelna przyczyna=json|kod_http|nie_http|brak_ciala …`
  — odpowiedź przyszła, ale nie dało się jej użyć; dalej w linii kod HTTP,
  błąd JSON, pamięć, początek i koniec treści. Najwyżej jedna na 10 s.
  **`json=NoMemory` znaczy: odpowiedź jest dobra, a panelowi zabrakło
  pamięci** (patrz `ram=` w tej samej linii).
- `tag=PANEL lvl=WARN zdarzenie=dane_nieaktualne` / `dane_wrocily` — chwila,
  w której kafelki przeszły na `--` i w której wróciły.
- `tag=WIFI zdarzenie=skan_centrala` — każda sieć `PiecCO_AP` widziana przy
  starcie. Tylko przy zimnym starcie, więc log warto łapać od włączenia.

Linie dodane w v1.2.1 (panel bez danych):

```
tag=PANEL lvl=WARN zdarzenie=dane_nieaktualne wiek_s=10 prog_s=10 wifi=1 przyczyna=milczy restart_za_s=34
tag=WIFI lvl=WARN zdarzenie=wymuszam_polaczenie nr=1 szukam=ten_sam_bssid bssid=24:6F:28:11:22:33 kanal=6 rssi=-45 bez_danych_s=12 msg="..."
tag=WIFI zdarzenie=rozlaczono kod=201 nazwa=NO_AP_FOUND nowych=1 lacznie=3
[STATUS] GET nieudany: connect() natychmiastowy blad errno=118 EHOSTUNREACH
tag=PANEL lvl=ERR zdarzenie=autorestart przyczyna=wifi bez_danych_s=45 nr_z_rzedu=1 w_serii=1 lacznie=1 wifi=0 rssi=0 rozl_kod=201 rozl_nazwa=NO_AP_FOUND rozl_n=14 ostatni_blad="..." ram=... ram_blok=... ram_min=... alarmy=0x00 msg="..."
tag=PANEL lvl=WARN zdarzenie=start_po_autorestarcie nr_z_rzedu=1 w_serii=1 zgloszony_nr=0 ... zglaszam_centrali=auto_wifi_r201_n1_t45_h61_b31_en118
tag=PANEL zdarzenie=autorestart_zgloszony powod=auto_wifi_r201_n1_t45_h61_b31_en118 msg="..."
```

- `przyczyna=wifi | milczy | odp | pam` — to samo, co napis w nagłówku
  (`pam` = brak pamięci w panelu, od v1.2.2).
  `restart_za_s=-1` — przy tej przyczynie restartu już nie będzie.
- `tag=PANEL lvl=WARN zdarzenie=autorestart_pominiety` — odpowiedzi centrali
  są nieczytelne mimo dwóch restartów z rzędu; panel czeka bez restartu.
- `tag=PANEL zdarzenie=autorestart_licznik_wyzerowany z_rzedu=1->0 w_serii=1->1`
  — dane poprawne od 2 minut: następny samoczynny restart znów po 45 s.
  Po godzinie taka sama linia z `w_serii=…->0`: koniec serii.
- `nr_z_rzedu` — restarty bez 2 minut poprawnych danych pomiędzy (od tego
  zależy, po ilu sekundach będzie następny); `w_serii` — restarty bez godziny
  poprawnych danych pomiędzy (ten numer idzie do centrali jako `n…`).
- `zglaszam_centrali=sw` w linii `start_po_autorestarcie` — ten restart
  z serii nie jest zgłaszany centrali (zgłoszony był nr `zgloszony_nr`).
- `autorestart_zgloszony` — centrala odpowiedziała na zapytanie z powodem
  restartu, czyli go dostała.
- `tag=WIFI zdarzenie=rozlaczono kod=…` — każde rozłączenie WiFi z kodem
  przyczyny podanym przez radio. Najczęstsze: `201 NO_AP_FOUND` (sieci
  centrali nie ma w eterze), `200 BEACON_TIMEOUT` (centrala przestała
  nadawać), `8 ASSOC_LEAVE` (panel rozłączył się sam przed kolejną próbą),
  `15 4WAY_HANDSHAKE_TIMEOUT` (typowo złe hasło).
- `errno=…` w liniach `GET nieudany` — dlaczego nie udało się nawet zacząć
  połączenia: `EHOSTUNREACH`/`ENETUNREACH` = panel nie ma połączenia WiFi
  albo adresu, `ENOMEM`/`ENOBUFS` = w panelu zabrakło pamięci.
- Linia `tag=PANEL fw=…` ma na końcu nowe pola: `bez_danych_s`, `przyczyna`
  (`-` = nic nie brakuje), `wymuszen`, `rozl_kod`, `rozl_n` (rozłączenia WiFi
  od startu), `autorestartow` (od włączenia zasilania).

Linie dodane w v1.3.0 (ekran Wykresy). Przykłady z testów na PC — `ms=`
i `ram=` na płytce będą inne:

```
tag=WYKRESY zdarzenie=start tryb=dzien ram=52000
tag=WYKRESY zdarzenie=pobrano tryb=dzien sposob=centrala punktow=96 przerw=0 wierszy_centrali=1440 linii=98 ucietych_linii=0 bajtow=18207 ms=25 ram=52000 ram_min=45000
tag=WYKRESY zdarzenie=stara_centrala kod=302 msg="centrala nie zna /api/history/panel (firmware starszy niz 3.58.4) - pobieram cala historie po staremu"
tag=WYKRESY zdarzenie=pobrano tryb=dzien sposob=panel punktow=91 przerw=0 wierszy_centrali=-1 linii=1440 ucietych_linii=0 bajtow=99860 ms=135 ram=52000 ram_min=45000
tag=WYKRESY lvl=WARN zdarzenie=pobrano_czesc tryb=tydz sposob=panel powod=limit_czasu etap=c punktow=55 linii=1701 bajtow=299837 z_zapowiedzianych=352084 ms=20010 msg="limit czasu calosci"
tag=WYKRESY zdarzenie=brak_danych tryb=dzien sposob=centrala przyczyna=pusto punktow=0 wierszy_centrali=0 linii=2 bajtow=119 ms=5
tag=WYKRESY lvl=WARN zdarzenie=blad tryb=dzien sposob=centrala przyczyna=limit_czasu kod=200 etap=p errno=0 punktow=0 linii=47 bajtow=9000 ms=6020 ram=52000 ram_blok=40000 msg="centrala przestala wysylac w trakcie"
tag=WYKRESY zdarzenie=przerwano powod=wyjscie_z_ekranu tryb=tydz po_ms=2730 linii=228
```

- `zdarzenie=start` — dotknięto `POKAZ WYKRES`; `tryb=godz|dzien|tydz`.
- `zdarzenie=pobrano` — wykres jest na ekranie.
  - `sposob=centrala` — punkty policzyła centrala (3.58.4);
    `sposob=panel` — panel pobrał cały CSV i policzył je sam (starsza centrala).
  - `punktow`, `przerw` — ile punktów z danymi i ile przerw ma wykres;
  - `wierszy_centrali` — z ilu wierszy swojej historii centrala policzyła
    punkty (`-1` przy starym sposobie);
  - `linii`, `bajtow` — ile linii i bajtów treści przyszło;
    `ucietych_linii` — linie dłuższe niż bufor panelu (ma być 0);
  - `ms` — czas od dotknięcia do wykresu; `ram`, `ram_min` — wolny RAM po
    wszystkim i jego minimum od startu panelu.
- `zdarzenie=stara_centrala kod=…` — centrala nie zna nowego adresu
  (`302` to zwykła odpowiedź firmware'u do 3.58.3); panel pobiera po staremu.
- `zdarzenie=pobrano_czesc` — tylko stary sposób: wykres z części danych,
  z dopiskiem `NIEPELNE`; `powod=limit_czasu|urwana`,
  `z_zapowiedzianych` — ile bajtów centrala zapowiedziała.
- `zdarzenie=brak_danych przyczyna=pusto|za_malo` — centrala odpowiedziała
  poprawnie, ale nie ma jeszcze z czego zrobić wykresu. To nie błąd.
- `zdarzenie=blad przyczyna=…` — wykresu nie ma. Przyczyny:
  `brak_wifi`, `brak_polaczenia`, `limit_czasu`, `http` (kod w `kod=`),
  `urwana`, `zla_odpowiedz`, `brak_pamieci`. `etap=` mówi, w którym miejscu:
  `g`, `n`, `a`, `l` nie udało się połączyć (`l` = minął limit 1,5 s),
  `s` nie udało się wysłać zapytania, `h` połączono i brak odpowiedzi,
  `p` odpowiedź zaczęła przychodzić i ustała, `c` limit 20 s na całość,
  `r` błąd odczytu, `z` i `k` to nie była odpowiedź, którą panel umie czytać,
  `-` sieć zadziałała (przyczyna jest w treści odpowiedzi albo w kodzie HTTP).
  `errno=` to numer błędu gniazda (0 = brak).
- `zdarzenie=przerwano powod=wyjscie_z_ekranu` — wyjście z wykresu przed
  końcem pobierania; panel zamknął połączenie.

**Powód samoczynnego restartu w logu centrali** (`tag=RESET-DIAG … powod=…`),
na przykładzie `auto_wifi_r201_n1_t45_h61_b31_en118`:

| Pole | Znaczenie |
|---|---|
| `auto` | panel zrestartował się sam, bo nie miał danych |
| `wifi` | czego brakowało: `wifi` (brak połączenia z siecią centrali), `milczy` (WiFi jest, centrala nie odpowiada), `odp` (odpowiedź nieczytelna), `pam` (panelowi zabrakło pamięci; v1.2.2), `lvgl` (błąd wewnętrzny biblioteki ekranu, zwykle też brak pamięci — restart od razu; v1.2.2) |
| `r201` | ostatni kod rozłączenia WiFi (`r0` = nie było żadnego) |
| `n1` | który to taki restart w serii (serię kończy godzina poprawnych danych); przeskok numeru, np. z `n3` na `n6`, to restarty, których panel nie zgłaszał |
| `t45` | ile sekund panel był bez danych |
| `h61`, `b31` | wolny RAM i największy wolny blok, w kB. Od v1.2.2 to prawdziwe liczby; zdrowy panel ma tu kilkadziesiąt, `h1_b0` znaczy brak pamięci |
| `en118` | ostatnia nieudana próba statusu — etap i numer błędu: `el0` centrala nie odpowiada na próbę połączenia, `ec0` połączono, brak odpowiedzi, `en118` połączenie odrzucone od razu (118 = brak drogi do centrali, 12 = brak pamięci), `eo500` odpowiedź nieczytelna (kod HTTP), `em200` odpowiedź przyszła, zabrakło pamięci na jej odczytanie, `em12` zabrakło pamięci już na jej odebranie, `ev0` błąd biblioteki ekranu |

Którą wersję panel ma wgraną, też widać w logu centrali: gdy panel zgłosi
się z inną wersją niż poprzednio (albo pierwszy raz po restarcie centrali),
centrala pisze `tag=OTA-PANEL lvl=INFO zdarzenie=wersja_zgloszona wersja=v1.4.0`.

**Aktualizacja przez centralę (v1.4.0)** — linie `tag=OTA-PANEL` w logu
panelu:

- `zdarzenie=stan_przy_starcie obraz=… zatwierdzony=… zapis_stan=… zapis_prob=… zapis_klucz=…`
  — raz, przy starcie. `obraz` to identyfikator programu, z którego panel
  pracuje (`nieznany` = nie udało się go odczytać). `zatwierdzony=0` to
  pierwszy start po wgraniu przez centralę. `zapis_*` to to, co panel pamięta
  o ostatniej ofercie: `zapis_stan` 0 nic, 1 próby w toku, 2 plik wgrany
  i czeka na ocenę po restarcie, 3 nowy program działa, 4 odmowa.
- `zdarzenie=decyzja akcja=… powod=… oferta=… nr=…` — przy każdej zmianie
  decyzji, nie co sekundę:
  - `akcja=nic powod=brak_oferty` — centrala nic nie oferuje;
  - `akcja=czekaj` — oferta jest, panel jeszcze nie zaczyna. `powod`:
    `alarm`, `brak_wifi`, `brak_danych`, `po_starcie` (pierwsze 20 s),
    `odstep` (30 s po nieudanej próbie), `siec_zajeta` (wykres albo
    polecenie w drodze), `dotyk` (15 s od ostatniego dotyku), `bateria`,
    `pamiec`;
  - `akcja=pomin` — nic do zrobienia: `juz_mam` (panel pracuje na tym
    pliku), `juz_wgrane` (ta oferta została już wykonana), `czeka_na_ocene`;
  - `akcja=odmow` — panel tej oferty nie wgra: `odmowa`, `proby_wyczerpane`.
- `zdarzenie=start wersja_docelowa=… oferta=… proba=1/3 ram=… ram_blok=…` —
  początek pobierania. `oferta=n<nr>:<identyfikator>` przy centrali 3.58.6,
  `oferta=t:<tag>` przy starszej.
- `zdarzenie=pobrano_ok obraz=… bajtow=… trwalo_ms=… nvs=1 restart_za_chwile`
  — plik wgrany i sprawdzony; restart. **`trwalo_ms` to liczba, której testy
  na PC nie dają.**
- `zdarzenie=ten_sam_plik` — plik od centrali okazał się programem, z którego
  panel już pracuje; nic nie zostało wgrane.
- `lvl=WARN zdarzenie=przerwana powod=alarm|oferta_wycofana bajtow=… z=…` —
  pobieranie przerwane; ta próba się nie liczy.
- `lvl=ERR zdarzenie=nieudana powod=… proba=2/3 koniec_prob=0 http=… bajtow=… z=… etap=… nvs=… blad="…"`
  — próba nieudana. `powod`: `pobieranie` (sieć albo centrala), `obraz`
  (plik nie jest poprawnym programem dla tego panelu), `za_duzy`, `haslo`
  (centrala odrzuciła hasło OTA — inne `SECRET_OTA_HASLO` w `secrets.h` panelu
  i centrali), `zapis` (zapis do pamięci programu). `koniec_prob=1` — panel
  tej oferty więcej nie próbuje i zgłasza centrali odmowę. `etap` jak
  w liniach `tag=WYKRESY`.
- `zdarzenie=nowy_firmware_dziala obraz=… wersja=…` — pierwszy start nowego
  programu, zatwierdzony.
- `lvl=ERR zdarzenie=nowy_firmware_wycofany wgrany=… dziala=…` — po wgraniu
  i restarcie działa inny program niż wgrany: nowy nie wstał i bootloader
  wrócił do poprzedniego.
- `lvl=ERR zdarzenie=brak_zapisu_nvs` — panel nie mógł zapisać stanu
  aktualizacji, więc jej nie zaczął.

Hasło OTA nie pojawia się w logu: adres pliku jest wypisywany jako
`/panel-ota.bin?token=***`.

Linie `tag=OTA-PANEL` w logu centrali 3.58.6 opisuje `README.md` jej paczki.

Stary, gęsty log `[NET-DIAG] start/wyslano/sukces` można przywrócić, ustawiając
`SiecCfg::POZIOM_LOGU = 2` w `src/siec_nieblokujaca_LCD.h`.

## Aktualizacja przez centralę i Telegram (OTA)

Od panelu **v1.4.0** i centrali **3.58.6**. Jak to działa i co się zmieniło:
rozdział „Co zmienia v1.4.0” na górze. Stan sprzed zmian:
`RAPORT_OTA_PANELU_PRZEZ_CENTRALE.md` w katalogu głównym repozytorium.

**Żadna aktualizacja tą drogą nie była jeszcze wykonana na tych
urządzeniach.** Pierwszą warto zrobić przy panelu podłączonym do komputera
(log) i z kablem pod ręką.

### Krok po kroku

1. Zbuduj firmware panelu w PlatformIO. Plik:
   `LCD-Centrali/.pio/build/esp32dev/firmware.bin`.
2. Sprawdź go (z katalogu głównego repozytorium):

   ```
   python3 narzedzia/ota_panelu_sprawdz.py --tag <tag wydania> --bin LCD-Centrali/.pio/build/esp32dev/firmware.bin
   ```

   Kod wyjścia 1 = nie wysyłaj komendy; skrypt pisze dlaczego.
3. Dołącz plik do **najnowszego** wydania na GitHubie pod nazwą
   `firmware_panel.bin`. Centrala zawsze bierze najnowsze wydanie — to samo,
   z którego `/update` bierze `firmware.bin` dla siebie. Tag wydania może być
   dowolny: od v1.4.0 nie musi być równy wersji panelu.
4. Telegram: `/update_panel`. Centrala pobiera plik na swój dysk i pisze, że
   czeka na panel.
5. Panel zaczyna sam, zwykle w ciągu minuty (warunki: „Jak to działa”,
   punkt 3). Na ekranie `AKTUALIZACJA PANELU` i pasek; potem restart.
6. Centrala pisze `Panel zaktualizowany: …` — albo czym innym się skończyło.

### Gdy coś nie idzie

| Co widać | Co to znaczy |
|---|---|
| Po 5 minutach wiadomość, że panel jeszcze nie zaczął, z przyczyną | Centrala podaje, na co panel czeka: alarm, dotyk, bateria, pamięć, brak kontaktu. Oferta czeka dalej, do 24 h. |
| Pasek postępu znika, panel pracuje dalej | Próba nieudana albo przerwana alarmem. Kolejna po 30 s; najwyżej trzy. Przyczyna: linia `zdarzenie=nieudana` albo `przerwana` w logu panelu. |
| `Panel odmówił aktualizacji … (powód: …)` | Panel tej oferty nie wgra: trzy nieudane pobrania, zły plik, plik za duży, złe hasło, nowy program nie wstał. Po usunięciu przyczyny: `/update_panel` jeszcze raz. |
| Chcę to zatrzymać | `/update_panel_anuluj`. Jeśli plik właśnie idzie, panel przerwie pobieranie przy najbliższym odpytaniu centrali (2 s). |
| Panel po aktualizacji nie wstaje | Program jest zatwierdzany dopiero po narysowaniu ekranu i odpytaniu centrali. Jeśli zawiesi się albo zrestartuje wcześniej, bootloader wraca do poprzedniego programu. **Ta droga powrotu nie była sprawdzona na płytce** — stąd kabel pod ręką. |

### Co wolno, czego nie

- Ekran prosi, żeby w trakcie nie wyłączać zasilania — bo próba przepada
  (liczy się do trzech). Samo wyłączenie nie powinno niczego zepsuć: stary
  program zostaje nietknięty, nowy trafia do drugiej połowy pamięci programu
  i jest ustawiany jako startowy dopiero po sprawdzeniu całości. Tak działa
  mechanizm ESP32; na płytce nie było to próbowane.
- Usypianie panelu jest na czas pobierania wyłączone; dotyk nie przerywa
  pobierania, a ekrany są zasłonięte nakładką.
- W czasie alarmu centrala nie podaje ani oferty, ani pliku, a panel nie
  zaczyna. Alarm ma pierwszeństwo także w trakcie: pobieranie jest
  przerywane, ekran alarmu i brzęczyk działają jak zawsze.
- Hasło (`SECRET_OTA_HASLO` w `secrets.h` panelu i centrali) musi być to samo
  po obu stronach. Idzie po sieci WiFi centrali w adresie zapytania — tak
  samo jak dotąd.

### Panel starszy niż v1.4.0

Z centralą 3.58.6 starszy panel pobiera plik po staremu — przy starcie,
z pustym ekranem — ale tylko raz: gdy zgłosi się ponownie, centrala kończy
ofertę i pisze, czy wersja się zmieniła. Z centralą do 3.58.5 starszy panel
wgrywa ten sam plik przy każdym starcie, dopóki tag wydania nie jest równy
jego wersji (opis w raporcie).

## Co zostało sprawdzone, a co nie

**v1.4.0 i v1.3.0 nie były budowane w PlatformIO ani uruchamiane na
płytce.** Na płytce sprawdzona jest v1.2.2 (niżej). v1.3.0 różni się od niej
ekranem Wykresy i sposobem pobierania jego danych; v1.4.0 dokłada
aktualizację przez centralę. Reszta kodu jest ta sama.

- **Na PC, z atrapami sprzętu:** prawdziwy `main.cpp` i dashboard uruchomione
  zwykłym `g++` przeciwko udawanym centralom — 47 scenariuszy, 486
  sprawdzeń, plus 442 sprawdzenia jednostkowe; także pod AddressSanitizer
  i UBSan. Udawana centrala wysyła ten sam JSON co firmware 3.58.3. W czterech
  scenariuszach z v1.2.2 panel nie dostaje pamięci na żądanie testu — tak, jak
  na płytce z logu z 2026-11-12.
- **Aktualizacja przez centralę (v1.4.0), na PC:** dwanaście scenariuszy
  `ota_*`. Po drugiej stronie nie stoi udawana logika, tylko **kod decyzji
  wycięty z firmware'u centrali 3.58.6** (`testy_host/kod_centrali/`; testy
  centrali pilnują, że to ten sam tekst co w jej pliku). Scenariusze:
  - cały przebieg w trakcie pracy: oferta, pobranie 1,2 MB z paskiem
    postępu, sprawdzenie pliku, restart; pierwszy start nowego programu
    i potwierdzenie przez centralę;
  - nowy program nie wstał i wrócił poprzedni — odmowa, bez ponownych prób;
  - trzy nieudane pobrania (cisza, zerwane połączenie, błąd HTTP) — odmowa,
    panel pracuje dalej;
  - plik uszkodzony, złe hasło, plik za duży, program dla innego układu,
    plik inny niż w ofercie — odmowa po jednej próbie;
  - alarm przed i w trakcie; oferta wycofana w trakcie;
  - każdy z warunków startu z osobna (czas po starcie, dotyk, pamięć,
    bateria, zapis w NVS);
  - plik, z którego panel już pracuje; centrala, która nie odpowiada na
    status, ale plik wysyła; panel, który nie zna sumy własnego programu;
  - centrala do 3.58.5: jedno wgranie, potem — mimo że jej flaga zostaje —
    żadnego kolejnego.

  Osobny skrypt (`testy_host/sprawdz_czulosc_ota.py`) 23 razy psuje jedną
  rzecz w kodzie aktualizacji i sprawdza, że testy to wykrywają (23 z 23).
  **Udawane są:** pamięć programu (zapis, sprawdzenie obrazu, wybór programu
  startowego), bootloader i jego powrót do poprzedniego programu, pamięć
  NVS, sieć. Czasy z tych testów (plik 1,2 MB w ok. 8 s, najdłuższy obrót
  pętli 35 ms) wynikają z założeń atrapy: 150 kB/s i 15 ms na 4 kB zapisu.
  Nie są pomiarem płytki.
- **Ekran Wykresy (v1.3.0), na PC:** dziewięć scenariuszy `wykresy_*` — wykres
  z punktów centrali w trzech zakresach, starsza centrala, przerwy w danych,
  brak danych, odpowiedź urwana, cisza w połowie, brak odpowiedzi, odpowiedź,
  która nie jest historią, brak WiFi, wyjście z ekranu i alarm w trakcie
  pobierania, pobieranie dłuższe niż limit, centrala 3.58.5 tuż po restarcie
  (dodany razem z paczką 3.58.5; firmware panelu bez zmian). Lista:
  `testy_host/README.md`.
  - Odpowiedzi centrali w tych testach nie są pisane ręcznie: to pliki
    wypisane przez kod centrali 3.58.5 (`testy_host/wzorce_centrali/`;
    9 z 11 centrala 3.58.4 wypisuje co do bajta tak samo), a testy centrali
    przy każdym uruchomieniu sprawdzają, że jej kod nadal daje dokładnie
    te pliki.
  - Rysowanie ze starego CSV jest porównane z v1.2.2 punkt w punkt
    (`testy_host/wzorce_v122/`: dzień, godzina, tydzień, dzień z przerwą).
  - Czasy z tych testów (np. `ms=25`) nie są pomiarem płytki — tam sieć
    jest lokalna.
- **Pamięć, na PC** (`testy_host/pomiar_pamieci.sh`): ten sam firmware
  zbudowany jako program 32-bitowy, z licznikiem każdego przydziału pamięci
  i z narzutem sterty ESP32. Po starcie trzyma 129 244 B — tyle samo co
  v1.2.2 (v1.1.0: 170 324 B, v1.2.0: 170 572 B); jedno odpytanie centrali
  bierze chwilowo 5 384 B (v1.2.0: 7 208 B); przez 10 minut pracy nie
  przybywa nic. Pobranie i narysowanie wykresu bierze chwilowo 2 452 B
  (punkty z centrali) albo 5 412 B (cały CSV), razem z odpytaniem statusu,
  które wypada w tym samym czasie; sam kod pobierania nie prosi o pamięć ze
  sterty, a pięć wykresów z rzędu nie zostawia nic. Zmiennych statycznych
  jest o 512 B więcej niż w v1.2.2 (bufor jednej linii odpowiedzi i stan
  pobierania). Skrypt pilnuje tych liczb limitami. Wszystkie scenariusze
  przechodzą także w tym modelu. Pamięci, którą na czas pobierania bierze
  stos sieciowy płytki, ten pomiar nie widzi. (Przy wydaniu v1.3.0 te
  liczby były o ok. 0,2 kB wyższe — 5 588, 2 884 i 5 616 B, tak też są
  w komentarzu w `src/main.cpp`. Model liczył wtedy każde zmniejszenie
  bloku pamięci jak nowy przydział; sterta ESP32 zmniejsza blok w miejscu.)
  **v1.4.0:** po starcie 129 324 B (80 B więcej), odpytanie centrali
  5 464 B (80 B więcej — dłuższy adres zapytania), wykres 2 452 B albo
  5 492 B, zmiennych statycznych o 898 B więcej niż w v1.3.0 (drugi obiekt
  pobierania i stan aktualizacji). Limit zmiennych statycznych w skrypcie
  podniesiony z 5 000 do 6 000 B, z uzasadnieniem w skrypcie.
- **Czy RAM-u starczy, na PC** (`testy_host/pomiar_pamieci.sh --przypadki`,
  2026-11-12): 29 przypadków pracy — każdy ekran, wykresy na wszystkie
  sposoby z 8 seriami, błędy centrali i sieci, alarm w trakcie pobierania,
  starsza centrala z całym CSV, doba pracy, 300 wykresów z rzędu, a od
  v1.4.0 także aktualizacja panelu (cały plik 1,2 MB i przerwana w połowie).
  Najwięcej pamięci naraz ponad stan po starcie: 25 496 B (v1.3.0:
  25 416 B) — przy otwartych Wykresach; aktualizacja bierze 11 484 B. Druga
  runda tych samych czynności nie zostawia nic. Osobno: sterta modelu
  z limitem — od 24 900 B wolnego RAM-u w górę nie ma ani jednej odmowy
  pamięci. Przeliczenie na płytkę i opis: „Pamięć (v1.4.0)” i „Czy RAM-u
  starczy” wyżej oraz `RAPORT_PAMIEC_RAM_LCD_I_CENTRALA.md` w katalogu
  głównym repozytorium.
- **Składnia i typy, prawdziwymi nagłówkami ESP32:** wszystkie trzy pliki
  `.cpp` bez błędów z arduino-esp32 3.0.7 i 3.3.8, ESP-IDF 5.1, TFT_eSPI,
  LVGL i ArduinoJson (`testy_host/sprawdz_naglowki_esp32.sh`; kompilator clang,
  bez linkowania).
- **Wersje bibliotek:** `platformio.ini` dopuszcza każde LVGL 8.x od 8.3.11
  i każdy ArduinoJson 7.x od 7.2.0 — którą wersję ma dany komputer, zależy od
  tego, kiedy PlatformIO je pobrał. Testy i pomiar pamięci v1.4.0 przechodzą
  z LVGL 8.3.11 i ArduinoJson 7.2.1 oraz z najnowszymi wydaniami (LVGL 8.4.0,
  ArduinoJson 7.4.3): 442 sprawdzenia jednostkowe, 47 scenariuszy,
  `pomiar_pamieci.sh --przypadki`. Z nowszymi bibliotekami pamięć po starcie
  wychodzi identyczna, a odpytanie centrali o 140 B mniejsze (5 324 B);
  aktualizacja panelu bierze 11 344 B zamiast 11 484 B.
- **Na płytce** (2026-11-12, **v1.2.2**): firmware zbudowany w PlatformIO
  i wgrany przez użytkownika. Panel startuje, zgłasza centrali `v1.2.2`, ma
  49,7 kB wolnego RAM-u po starcie i w ok. 40 s logu odczytał wszystkie
  odpowiedzi. Liczby: „Wynik na płytce” wyżej.
- **Układ pliku z programem (v1.4.0):** kod, który ocenia pobierany plik
  (nagłówek, numer układu, identyfikator z sumy na końcu), jest sprawdzony
  także na trzech prawdziwych obrazach zbudowanych narzędziami Espressif —
  razem z odpowiednim kodem centrali
  (`firmware-patch/v3.58.6-panel-ota/tests/sprawdz_prawdziwe_obrazy.sh`).
- **Nie sprawdzone na płytce, v1.4.0:** kompilacja w PlatformIO; odczyt
  sumy własnego programu (linia `stan_przy_starcie obraz=…` — sprawdzenie
  opisane w „Po wgraniu”); cała aktualizacja przez centralę — pobieranie
  pliku przez WiFi centrali, zapis do pamięci programu (biblioteka kasuje ją
  blokami po 64 kB i każde kasowanie zatrzymuje na ułamek sekundy oba
  rdzenie, więc pętla i ekran będą wtedy wolniejsze niż w modelu),
  sprawdzenie obrazu, start nowego programu, powrót bootloadera do
  poprzedniego; ile trwa (`trwalo_ms=`) i ile wolnego RAM-u wtedy zostaje;
  zapas stosu pętli; centrala 3.58.6 (jej plik też nie był kompilowany dla
  ESP32).
- **Nie sprawdzone na płytce, v1.3.0:** kompilacja w PlatformIO; ekran
  Wykresy z prawdziwą centralą — ile trwa pobieranie (`ms=`), ile wolnego
  RAM-u zostaje w jego trakcie (`ram_min=`; ekran Wykresy bierze 18,6 kB na
  bufory punktów, a otwarte połączenie swoje), jak wypada przy słabym
  sygnale; centrala 3.58.5 i 3.58.4 (ich pliki też nie były kompilowane
  dla ESP32 — opis w paczkach centrali).
- **Nadal nie sprawdzone na płytce, od v1.2.2:** praca przez wiele godzin;
  zachowanie przy prawdziwym braku pamięci i samoczynne restarty (w logu
  z 2026-11-12 nie było do nich powodu); szybkość rysowania (nie była
  mierzona). Ograniczenia testów na PC opisuje `testy_host/README.md`.
- **Szacunek a pomiar.** Pomiar na PC nie obejmuje pamięci, którą na płytce
  biorą WiFi i system; skrypt przyjmuje dla nich 55,9 kB, wyliczone z logu
  v1.2.0. Szacował 48,6 kB wolnego RAM-u po starcie, płytka pokazała 49,7 kB.
  v1.2.2 zajmuje o ok. 40 kB mniej niż v1.2.0: 41,3 kB mniej pamięci
  przydzielanej w trakcie pracy, 0,9 kB więcej zmiennych statycznych (bufor
  logu). v1.3.0 dokłada 0,5 kB zmiennych statycznych, więc szacunek dla niej
  to ok. 48,1 kB po starcie; v1.4.0 dokłada kolejny 1 kB — ok. 47,1 kB.
  (Tabele wyżej liczą od zmierzonej v1.2.2, 49,7 kB, i dają o 1,1 kB więcej:
  49,2 kB dla v1.3.0 i 48,2 kB dla v1.4.0.)
- **Zachowanie radia jest w testach założeniem.** Scenariusz
  `martwe_polaczenie` (centrala restartuje się tak szybko, że panel dalej
  uważa się za połączony) pokazuje, że od v1.2.1 panel wychodzi z takiego
  stanu sam po ok. 13 s, a v1.2.0 nie wychodzi wcale — ale czy właśnie to
  dzieje się na płytkach, nie wiadomo. Dlatego jest też restart po 45 s,
  który nie zależy od tego założenia.
- Zimny start trwa o czas skanu dłużej (szacunkowo 1,5–2 s; niezmierzone).
