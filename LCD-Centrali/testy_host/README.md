# Testy panelu LCD na PC

Ten katalog pozwala uruchomić **prawdziwy** `src/main.cpp` panelu (razem z
`dashboard_lvgl_poc.cpp` i `lvgl_port.cpp`) na zwykłym komputerze, bez płytki
ESP32. PlatformIO go nie widzi i nie kompiluje — nie wpływa na firmware.

```
./uruchom_testy.sh                 # wszystko: testy jednostkowe + symulator (kilka sekund; pierwszy raz dłużej)
./uruchom_testy.sh --szybko        # tylko testy jednostkowe (sam g++, 1 s)
./uruchom_testy.sh dwie_centrale   # jeden scenariusz, z pełnym logiem Seriala na ekranie
SANITIZER=1 ./uruchom_testy.sh     # to samo z AddressSanitizer + UBSan (wolniej)
./sprawdz_naglowki_esp32.sh        # składnia i typy prawdziwymi nagłówkami ESP32 (patrz niżej)
./pomiar_pamieci.sh                # ile RAM-u trzyma firmware + limity (patrz niżej; wymaga: pip install ziglang)
./pomiar_pamieci.sh --przypadki    # czy RAM-u starczy w każdym przypadku pracy (ok. 2 minut)
```

Wymaga: `g++` (C++17), `gcc`, `ar`, `git`, Linux albo WSL. Przy pierwszym
uruchomieniu skrypt pobiera z GitHuba LVGL 8.3.11 i ArduinoJson 7.2.1 do
katalogu podręcznego (`$LCD_TESTY_CACHE`, domyślnie `/tmp/lcd_testy_host`).
Logi Seriala i zrzuty ekranu (`.bmp`) z każdego scenariusza trafiają do
`<katalog podręczny>/wyniki/`.

`src/secrets.h` nie jest potrzebny — gdy go nie ma, testy dostają atrapę z
wymyślonymi hasłami (atrapa WiFi haseł nie sprawdza).

**Inne wersje bibliotek.** `platformio.ini` dopuszcza każde LVGL 8.x od 8.3.11
i każdy ArduinoJson 7.x od 7.2.0. Skrypty pobierają biblioteki tylko wtedy,
gdy nie ma ich w katalogu podręcznym, więc inną wersję wystarczy włożyć tam
wcześniej:

```
mkdir -p /tmp/lcd_nowe
git clone --depth 1 --branch v8.4.0 https://github.com/lvgl/lvgl.git /tmp/lcd_nowe/lvgl
git clone --depth 1 --branch v7.4.3 https://github.com/bblanchon/ArduinoJson.git /tmp/lcd_nowe/ArduinoJson
LCD_TESTY_CACHE=/tmp/lcd_nowe ./uruchom_testy.sh
LCD_TESTY_CACHE=/tmp/lcd_nowe ./pomiar_pamieci.sh --wszystko
```

Stan na 2026-11-12, v1.3.0: z LVGL 8.4.0 i ArduinoJson 7.4.3 przechodzą
wszystkie scenariusze (także z `SANITIZER=1` i w modelu 32-bitowym), a pomiar
pamięci daje 129 244 B po starcie, 5 244 B na jedno odpytanie i 2 452 B /
5 272 B chwilowo na wykres (punkty z centrali / cały CSV); z opcją
`--przypadki`: najwięcej naraz 25 276 B, próg 24 700 B.

## Co jest w środku

| Plik | Co robi |
|---|---|
| `test_diagnostyka.cpp` | Testy jednostkowe `src/diagnostyka_panelu.h` (kod HTTP, dławik logu, strażnik świeżości, wybór najsilniejszego AP, znacznik centrali, próg baterii; od v1.2.1: przyczyna braku danych, drabina ratunkowa, pamięć i token samoczynnego restartu; od v1.2.2: brak pamięci jako przyczyna). |
| `mock/` | Atrapy nagłówków Arduino/ESP-IDF (`Arduino.h`, `WiFi.h`, `TFT_eSPI.h`, `Preferences.h`, `esp_*.h`…). Stan „płytki” opisuje `mock/atrapa.h`. |
| `symulator_panelu.cpp` | Uruchamia `setup()` i `loop()` firmware'u, podstawia mu „centrale” (lokalny serwer HTTP) i sprawdza log oraz to, co trafiło na ekran. Odpowiedź `/api/status` ma te same pola, kolejność i format liczb co `wifiZbudujJsonStatus()` w firmware centrali 3.58.3 (ok. 1,8 kB). Udawana centrala traktuje parametr `reset_powod` tak jak 3.58.3: „pisze” o powodzie restartu panelu przy każdej zmianie wartości innej niż `sw` i `deepsleep`. Od v1.3.0 odpowiada też na `/api/history`, `/api/history/long` i `/api/history/panel` (jak centrala 3.58.4, jak starsza albo jak bardzo stara) i potrafi odpowiedź urwać, wstrzymać, opóźnić albo wysyłać z zadaną szybkością. |
| `wzorce_centrali/` | Odpowiedzi centrali na `/api/history/panel`, wypisane przez jej własny kod — 11 plików; 9 takich samych z 3.58.4 i 3.58.5, 2 tylko z 3.58.5 (opis: `CZYTAJ.txt` w tym katalogu). Z nich korzystają scenariusze `wykresy_*`. |
| `wzorce_v122/` | Punkty, które z tych samych starych CSV narysował firmware v1.2.2 (opis: `CZYTAJ.txt` w tym katalogu). Z nimi porównywane jest rysowanie po staremu. |
| `uruchom_testy.sh` | Buduje i uruchamia całość. |
| `sprawdz_naglowki_esp32.sh` | Sprawdza składnię i typy źródeł panelu prawdziwymi nagłówkami arduino-esp32 / ESP-IDF / TFT_eSPI (bez atrap). |
| `pomiar_pamieci.sh` | Mierzy, ile pamięci RAM trzyma firmware, i pilnuje limitów. Buduje ten sam program jako 32-bitowy, z `pomiar_sterty.c` zamiast zwykłego `malloc()`. Z `--przypadki`: ile pamięci trzeba w każdym przypadku pracy i co się dzieje, gdy jest jej mniej. |
| `pomiar_sterty.c` | `malloc()` z licznikiem: dla każdego bloku pamięta rozmiar i stos wywołań; liczy go tak, jak zajmuje miejsce na stercie ESP32. Potrafi też odmawiać ponad zadany limit (jak płytka bez wolnego RAM-u) i wypisać stos wywołań, gdy program modelu sięgnie pod zły adres. |
| `pomiar_rozbicie.py` | Dla `pomiar_pamieci.sh --kto`: zamienia zapisane stosy wywołań na nazwy funkcji firmware'u i sumuje pamięć według nich. |

LVGL i ArduinoJson są **prawdziwe** (te same wersje co w `platformio.ini`),
LVGL jest budowany z `include/lv_conf.h` tego projektu. Ekran to bufor
480×320, do którego LVGL rysuje tak jak na płytce.

## Scenariusze

| Nazwa | Sytuacja |
|---|---|
| `dwie_centrale` | Dwie centrale nadają tę samą sieć (prawdziwa daleko, testowa obok). Hipoteza nr 1 dla zgłoszenia „kafelki się nie aktualizują”. |
| `dwie_centrale_piec_blizej` | To samo, ale panel wisi przy piecu. |
| `zla_odpowiedz` | Centrala odpowiada, ale treści nie da się użyć: pole `nan`, HTTP 500, pusta odpowiedź, odpowiedź ucięta. |
| `brak_centrali_na_starcie` | Panel włączony bez centrali w zasięgu; centrala wstaje później. |
| `zanik_i_powrot` | Centrala znika w trakcie pracy, wraca, a potem wraca jako inna płytka (inny BSSID). |
| `nieudany_post` | Jedno nieudane polecenie z dotyku. |
| `bateria_i_log` | Minuta zwykłej pracy: objętość logu, wskaźnik baterii przy samym USB. |
| `skan_zawieszony` | Skan WiFi, który się nie kończy. |
| `ustawienia_milcza` | `/api/status` działa, `/api/ustawienia-get` nie odpowiada przez 3 minuty (sytuacja z logu panelu z 2026-11-06). |
| `gola_plytka` | Stanowisko z logów z 2026-11-11: centrala bez czujników (wszędzie −1) z symulacją jednego czujnika; zmiana i wyłączenie symulacji. |
| `martwe_polaczenie` | v1.2.1. Wartości z logu centrali z 2026-11-12 (piec 134 °C, alarm). Centrala restartuje się tak szybko, że radio panelu nie zauważa przerwy: WiFi „połączone”, centrala milczy. Panel sam łączy się od nowa. |
| `autorestart_centrala_milczy` | v1.2.1. Centrala trwale nie odpowiada mimo działającego WiFi: dwa wymuszone połączenia, odliczanie w nagłówku, restart po 45 s. |
| `autorestart_brak_wifi` | v1.2.1. Panel bez centrali w zasięgu: `BRAK WIFI`, kod rozłączenia w logu, restart po 45 s. |
| `start_po_autorestarcie` | v1.2.1. Start po samoczynnym restarcie: centrala dostaje `reset_powod=auto_…` i pisze o nim raz — także gdy sama się potem zrestartuje; licznik restartów z rzędu wraca do zera po 2 minutach poprawnych danych. |
| `seria_restartow` | v1.2.1. Piąty samoczynny restart w serii: panel zgłasza zwykłe `sw`, centrala nie pisze o nim i nie wysyła wiadomości. |
| `autorestart_kolejny` | v1.2.1. Drugi samoczynny restart z rzędu następuje dopiero po 90 s. |
| `restart_po_ota` | v1.2.1. Restart programowy z innego powodu (OTA) i przypadkowe wartości w pamięci RTC: centrala dostaje zwykłe `reset_powod=sw`. |
| `zla_odpowiedz_dlugo` | v1.2.1. Centrala stale odpowiada nieczytelnie: WiFi nie jest rozłączane, restart po 45 s z przyczyną `odp`. |
| `zla_odpowiedz_bez_konca` | v1.2.1. Odpowiedzi nieczytelne mimo dwóch restartów z rzędu: trzeciego nie ma; gdy centrala całkiem zamilknie, restart wraca. |
| `wycisz_alarm` | v1.2.1. Przycisk WYCISZ: cisza od razu; wyciszenie kończy się razem z alarmem. |
| `autorestart_przy_alarmie` | v1.2.1. Alarm przy symulowanej temperaturze pieca: ekran alarmu pokazuje `SYMULACJA: … piec`. Potem centrala znika: stan alarmu (z WYCISZ i ZAMKNIJ) jest zapisany przed restartem. |
| `start_po_autorestarcie_z_alarmem` | v1.2.1. Start po restarcie w trakcie alarmu, bez danych: brzęczyk i ekran alarmu wracają, na ekranie alarmu stoi `BRAK WIFI … s`, WYCISZ działa. |
| `brak_pamieci_json` | v1.2.2. Sytuacja z logu panelu z 2026-11-12: `malloc()` odmawia pamięci na rozpakowanie odpowiedzi (`json=NoMemory`), a `Serial.printf()` — na długie linie. Nagłówek `BRAK PAMIECI`, `przyczyna=pam`, prawdziwy wolny RAM w logu, restart po 45 s; potrzebne linie logu wypisują się mimo to. |
| `brak_pamieci_odbior` | v1.2.2. Pamięci nie starcza już na odebranie odpowiedzi (`String` przestaje rosnąć): `GET nieudany: brak pamieci na odpowiedz`, `przyczyna=pam` — a nie „zła odpowiedź centrali”, jak w v1.2.0. |
| `brak_pamieci_lvgl` | v1.2.2. `malloc()` odmawia wszystkiego: LVGL zgłasza błąd przy rysowaniu, panel pokazuje `BRAK PAMIECI - RESTART` i restartuje się od razu, z powodem `auto_lvgl_…` dla centrali. Watchdog nie zadziałał. |
| `wykresy_pamiec` | v1.2.2. Bufory punktów ekranu Wykresy istnieją tylko przy otwartym ekranie: znikają po wyjściu przez bezczynność i po zastąpieniu ekranu alarmem; brak pamięci na bufory nie wywraca panelu. |
| `wykresy_centrala_liczy` | v1.3.0. Centrala 3.58.4: jedno zapytanie `/api/history/panel`, 96 punktów takich, jak policzyła centrala, w trzech zakresach; podpis z zakresem i datą; pełne bufory centrali i najdłuższe liczby (ostatnia kolumna wiersza dochodzi cała); dotknięcie wraca od razu, `/api/status` jest odpytywany dalej. |
| `wykresy_stara_centrala` | v1.3.0. Centrala do 3.58.3 odpowiada 302: panel pobiera cały CSV i rysuje punkt w punkt to, co v1.2.2 (dzień, godzina, tydzień, dzień z przerwą). Połączenie zerwane w środku wiersza: wykres z całych wierszy, dopisek `NIEPELNE`. Bardzo stara centrala, która pod nowym adresem oddaje CSV z kodem 200. |
| `wykresy_przerwy` | v1.3.0. Tydzień, w którym centrala nie pracowała 1,5 doby: 19 z 96 pozycji bez punktu, dokładnie tam, gdzie podała centrala. Czujnik bez odczytu przez pół godziny: przerwa tylko w jego linii, zakres osi liczony z prawdziwych wartości. |
| `wykresy_brak_danych` | v1.3.0. Centrala bez historii, z wyłączonym zapisem na flash, z jednym pomiarem, bez ustawionego zegara, bez pamięci na odpowiedź (HTTP 503): każdy przypadek ma swój napis i linię w logu; przy 503 panel nie sięga po cały CSV. |
| `wykresy_bledy_sieci` | v1.3.0. Odpowiedź urwana w połowie (z `Content-Length` i bez), linie z CR LF, cisza w połowie odpowiedzi, brak odpowiedzi, strona HTML zamiast historii, brak WiFi: napis z przyczyną, wykres z kawałka nie powstaje, `/api/status` jest odpytywany przez cały czas, następna próba daje poprawny wykres. |
| `wykresy_wyjscie_w_trakcie` | v1.3.0. Starsza centrala i wolne łącze: dotknięcie ekranu w trakcie pobierania działa od razu i zamyka połączenie; pobieranie dłuższe niż 20 s kończy się wykresem z dopiskiem `NIEPELNE`; watchdog (15 s) nie zadziałał. |
| `wykresy_alarm_w_trakcie` | v1.3.0. Alarm przegrzania w trakcie pobierania: ekran alarmu pojawia się w ciągu 3 s, pobieranie jest porzucane, bufory wykresów zwolnione. |
| `wykresy_stary_csv` | v1.3.0. Stary CSV podany ekranowi wprost, bez sieci: te same punkty co w v1.2.2. Strona HTML zamiast CSV: napis (kod v1.2.2 rysował z niej wykres — zapisane we wzorcu). Ten scenariusz działa także na źródłach v1.2.2 i tak powstał `wzorce_v122/punkty.txt`. |
| `wykresy_po_restarcie_centrali` | v1.3.0 bez zmian, centrala 3.58.5 tuż po restarcie, z wyłączonym zapisem historii i starymi plikami sprzed 3 dni. Ostatnia godzina: odpowiedź to 96 linii `#przerwa` — napis `Centrala nie ma jeszcze danych z tego zakresu` z dopiskiem o wyłączonym zapisie, bez sięgania po cały CSV. Tydzień: 55 punktów starej historii, potem 41 pustych pozycji do końca osi; podpis kończy się na bieżącej chwili. |
| `ota_start` | Aktualizacja przez centralę. Centrala ma gotowy firmware panelu i zgłasza to w `/api/status`. Panel przy starcie zgłasza swoją wersję, pyta o `http://192.168.4.1:80/panel-ota.bin?token=…`, na czas pobierania wypisuje zadanie z watchdoga, po udanym pobraniu restartuje się. STAN OBECNY: przez cały ten czas ekran jest pusty. |
| `ota_po_restarcie_zgodna` | Pierwszy start po aktualizacji, zgłoszona wersja równa tagowi wydania: panel potwierdza nową partycję, centrala zdejmuje flagę, panel nie pobiera drugi raz i pracuje. |
| `ota_po_restarcie_inna_wersja` | STAN OBECNY. Zgłoszona wersja inna niż tag wydania (panel `v1.3.0`, wydanie `3.58.6`): centrala nie zdejmuje flagi, panel pobiera ten sam plik i restartuje się znowu — przy każdym starcie. |
| `ota_flaga_w_trakcie_pracy` | STAN OBECNY. Centrala zaczyna zgłaszać aktualizację, gdy panel już pracuje: przez 2 minuty panel niczego nie pobiera. Zacznie dopiero po restarcie. |
| `ota_nieudane` | Pobieranie nieudane: wpis `tag=OTA-PANEL lvl=ERR` w logu, watchdog znów pilnuje pętli, panel pracuje dalej i w tej sesji nie próbuje ponownie. |
| `ota_przy_alarmie` | STAN OBECNY. Start z czekającą aktualizacją w chwili, gdy centrala zgłasza alarm: panel i tak pobiera; wzorzec alarmu na brzęczyku w tym czasie nie gra. |

Scenariusze `ota_*` z dopiskiem „STAN OBECNY” opisują to, co firmware robi
dziś, a nie to, co powinien — po zmianie firmware'u trzeba je poprawić.
W tych scenariuszach samo pobieranie jest udawane: atrapa `HTTPUpdate`
zapisuje adres, „trwa” zadany czas i zwraca zadany wynik. Sprawdzane jest,
kiedy panel zaczyna aktualizację, o co pyta i co robi potem. Udawana
centrala zdejmuje flagę aktualizacji tak jak prawdziwa: tylko gdy tekst
z parametru `panel_fw` jest identyczny z tagiem wydania. Opis całej drogi:
`RAPORT_OTA_PANELU_PRZEZ_CENTRALE.md` w katalogu głównym repozytorium.

### Skąd odpowiedzi centrali w scenariuszach `wykresy_*`

Treść odpowiedzi `/api/history/panel` nie jest pisana ręcznie w symulatorze.
To pliki z `wzorce_centrali/`, wypisane przez kod centrali 3.58.5 wycięty
z jej `main_centrala.cpp` (testy w
`firmware-patch/v3.58.5-panel-wykresy-po-restarcie/tests/`). Dziewięć z nich
centrala 3.58.4 wypisuje co do bajta tak samo.
Te same testy centrali przy każdym uruchomieniu porównują swoje wyjście z tymi
plikami, więc panel jest sprawdzany na tym, co centrala naprawdę wysyła,
a zmiana formatu po jednej stronie wychodzi w testach drugiej.

To, co panel ma z takiej odpowiedzi narysować, symulator liczy sam, prosto
z pliku (wiersz = punkt, `#przerwa` i pole `nan` = brak punktu), niezależnie
od kodu firmware'u.

### Brak pamięci na żądanie

Program testowy jest linkowany z `--wrap=malloc,calloc,realloc,free`, więc
każdy przydział pamięci z firmware'u, LVGL i ArduinoJson przechodzi przez
funkcje w `symulator_panelu.cpp` (`namespace pamiec`). Scenariusz może kazać
im odmawiać bloków o wybranym rozmiarze — tak jak `malloc()` na płytce, której
skończył się RAM. Odmowa działa tylko wtedy, gdy wykonuje się kod firmware'u.

Dwie rzeczy mają osobne przełączniki, bo atrapy nie biorą pamięci tak jak
płytka:

- `String` (atrapa na `std::string`): `String::odmowaOdDlugosci = N` — tekst
  nie urośnie do N znaków; `concat()` zwraca wtedy `false`, a `+=` nic nie
  dopisuje. Tak zachowuje się `String` z Arduino, gdy `realloc()` odmówi.
- `Serial.printf()`: na płytce tekst od 64 znaków powstaje w pamięci
  z `malloc()` i bez niej linia przepada. Atrapa robi to samo (jej `malloc()`
  podlega odmowie); `g_atrapa.serialBezPamieci = true` wymusza to wprost.

## Czego to NIE sprawdza

To nie jest emulator ESP32. Wynik „zaliczone” znaczy: logika panelu zachowuje
się tak, jak opisują sprawdzenia, **przy założeniach z atrap**. Nie znaczy, że
firmware kompiluje się dla ESP32 ani że działa na płytce.

- **Kompilacja dla ESP32.** Atrapy mają te same sygnatury co arduino-esp32 3.x,
  ale to nie jest toolchain ESP32. Zgodność z prawdziwymi nagłówkami sprawdza
  osobny skrypt (następny rozdział); buildu PlatformIO nie zastępuje nic.
- **Radio.** Model WiFi (`mock/atrapa.cpp`) to kilka reguł:
  - `WiFi.begin()` z BSSID łączy się tylko z tym nadajnikiem;
  - bez BSSID, przy `WIFI_FAST_SCAN` (domyślne w arduino-esp32), wygrywa sieć
    na **najniższym kanale** — to *założenie* wynikające z opisu „scan will
    end after find SSID match AP” i kolejności kanałów, nie pomiar;
  - bez BSSID, przy `WIFI_ALL_CHANNEL_SCAN`, wygrywa najsilniejsza;
  - radio samo nie ponawia połączenia (na płytce rdzeń arduino-esp32 robi to
    w tle, równolegle z logiką panelu) i zgłasza kod rozłączenia tylko przy
    `WiFi.disconnect()` (kod 8) albo gdy test poda go wprost;
  - „martwe połączenie” (`AtrapaAp::wcielenie`): po szybkim restarcie
    centrali panel dalej widzi stan „połączony”, a zapytania zostają bez
    odpowiedzi, dopóki nie wywoła `WiFi.begin()` jeszcze raz. To
    **przypuszczenie** co do zachowania płytek, nie obserwacja.

  Wynik „v1.1.0 łączy się z nie tą centralą” w scenariuszu `dwie_centrale`
  wynika wprost z drugiej reguły.
- **Czasy sieci.** Zegar płytki jest sztuczny (`delay()` przesuwa `millis()`),
  a HTTP idzie po lokalnym gnieździe. Opóźnienia lwIP, zakłócenia kanału AP
  i czasy z prawdziwego logu nie są odtwarzane.
- **Sprzęt.** TFT_eSPI, dotyk, deep sleep — atrapy przyjmują wywołania,
  ale ich nie modelują. Z brzęczyka zapisywana jest tylko ostatnio ustawiona
  częstotliwość (0 = cisza).
- **Aktualizacja OTA.** Pobierania pliku, zapisu do flash, sprawdzania obrazu
  i bootloadera tu nie ma. Atrapa odnotowuje, że firmware o aktualizację
  poprosił, i zwraca wynik zadany przez scenariusz.
- **Pobieranie danych na ekran Wykresy.** Od v1.3.0 idzie tym samym kodem co
  na płytce (gołe gniazda z `siec_nieblokujaca_LCD.h`), tylko gniazda są
  linuksowe, a „centrala” siedzi w tym samym programie. Szybkość łącza,
  zwłoka i zerwanie połączenia są zadawane przez scenariusz; prawdziwego WiFi
  i lwIP płytki to nie odtwarza. `ms=` z logu takiego testu nie jest czasem
  pobierania na płytce. Źródła do v1.2.2 pobierały te dane przez
  `HTTPClient`, którego atrapa z udawaną centralą się nie łączy — na nich
  scenariusze `wykresy_*` (poza `wykresy_stary_csv`) nie pokazują nic.
- **Watchdog.** Atrapa tylko odnotowuje, że przy takim przebiegu watchdog
  zadania (15 s) by zadziałał; nie restartuje „płytki”.
- **Pamięć płytki.** W zwykłych testach „wolny RAM”, który odczytuje
  firmware, to liczby wpisane przez test (`g_atrapa.ramWolny` itd.), a brak
  pamięci to odmowa `malloc()` na żądanie scenariusza — nie prawdziwe
  wyczerpanie sterty. Ile pamięci firmware naprawdę zajmuje, mierzy
  `pomiar_pamieci.sh`.
- **Restart.** `ESP.restart()` kończy w teście „życie” firmware'u (wyjątek
  `AtrapaRestart`); program nie startuje od nowa w tym samym procesie. Start
  po restarcie sprawdzają osobne scenariusze, które przed `setup()` wypełniają
  pamięć RTC tak, jak zostawia ją firmware. Tego, że pamięć `RTC_NOINIT`
  naprawdę przetrwa restart na płytce, test nie sprawdza.

## Pomiar pamięci

```
./pomiar_pamieci.sh              # pomiar + limity (kod wyjścia 1, gdy przekroczone)
./pomiar_pamieci.sh --kto        # dodatkowo: która funkcja firmware'u trzyma ile pamięci
./pomiar_pamieci.sh --wszystko   # dodatkowo: wszystkie scenariusze w modelu 32-bitowym
./pomiar_pamieci.sh --przypadki  # dodatkowo: czy RAM-u starczy w każdym przypadku pracy (patrz niżej)
SRC=/tmp/stary/src LIMITY=0 ./pomiar_pamieci.sh   # inna wersja źródeł, same liczby
```

Po co: zwykłe testy nie widzą, że firmware zajmuje za dużo pamięci — na PC
jest jej dość. A to właśnie był powód zgłoszeń (log panelu z 2026-11-12:
`json=NoMemory`, ok. 8 kB wolnego RAM-u).

Jak: firmware jest budowany jako program **32-bitowy** (wskaźniki po 4 bajty,
jak na ESP32) kompilatorem z pakietu Pythona `ziglang`. Zwykły `malloc()`
zastępuje `pomiar_sterty.c`, który liczy każdy blok, o jaki prosi firmware
(nie atrapy i nie sam test), z narzutem sterty płytki: rozmiar zaokrąglony do
4 bajtów + 16 bajtów (arduino-esp32 3.x ma włączone lekkie „zatruwanie”
sterty, `CONFIG_HEAP_POISONING_LIGHT`).

Wynik dla v1.3.0:

```
po starcie firmware trzyma          129 244 B w 2470 blokach
jedno odpytanie centrali, chwilowo    5 384 B wiecej
przyrost po 10 minutach pracy             0 B
ekran alarmu                            188 B wiecej
ekran Wykresy otwarty                18 632 B wiecej
...i po jego zamknieciu                  24 B wiecej
wykres z centrali 3.58.4 (pobranie i rysowanie), chwilowo 2 452 B wiecej
wykres po staremu (caly CSV, 352 kB), chwilowo        5 412 B wiecej
po pierwszym wykresie zostaje           424 B (opisy osi i legenda, tworzone raz)
przyrost po 5 kolejnych wykresach       -28 B
zmienne statyczne firmware'u          4 468 B (v1.2.0: 3 092 B)
```

Pierwsze sześć liczb jest takich samych jak w v1.2.2. v1.1.0 i v1.2.0
trzymały po starcie 171 kB (170 324 B i 170 572 B), a jedno odpytanie brało
7 080 B i 7 208 B.

Liczby „chwilowo” są od 2026-11-12 o ok. 0,2 kB niższe niż przy wydaniu
v1.3.0 (wtedy: 5 588, 2 884 i 5 616 B; v1.1.0 i v1.2.0: 7 284 i 7 412 B).
Model liczył każde zmniejszenie bloku przez `realloc()` jak nowy przydział —
przez chwilę stary i nowy blok naraz. Sterta ESP-IDF zmniejsza blok
w miejscu i `pomiar_sterty.c` robi teraz tak samo. Liczby „trzyma” się nie
zmieniły.

Linie o wykresie (od v1.3.0): „chwilowo” to najwięcej pamięci naraz
w sekundach, w których panel pobiera dane i rysuje wykres — razem z odpytaniem
`/api/status`, jeśli wypadło w tym samym czasie. Limit jest ten sam co dla
jednego odpytania (7 500 B). Z opcją `--kto` skrypt sprawdza dodatkowo, że
żaden blok pamięci żywy w tej chwili nie pochodzi z kodu pobierania
(`StrumienHttpGet`, `obslugaWykresyFetch()`, `…_strumien_linia()`): ten kod
czyta do stałego bufora. Sprawdzenie zostało wypróbowane na celowo zepsutej
kopii źródeł (jeden `malloc()` w odbiorze linii) — wykrywa go.

Ostatnia linia to zmienne globalne i `static` z trzech plików `.cpp`
firmware'u (sekcje `.bss`/`.data` w plikach `.o` modelu). Licznik `malloc()`
ich nie widzi, a na płytce zabierają ten sam RAM. Sama liczba jest
przybliżona (atrapy bibliotek mają inne rozmiary obiektów niż prawdziwe);
wiarygodna jest różnica między wersjami: v1.2.2 ma ich o 864 B więcej niż
v1.2.0, głównie przez bufor `logBezPamieci()` (640 B); v1.3.0 o kolejne
512 B (bufor jednej linii pobieranej historii, 300 B, i stan pobierania).
Skrypt odejmuje tę różnicę w szacunku i pilnuje limitem (5 000 B), żeby nie
pojawiła się duża tablica statyczna.

Czego ten pomiar **nie** obejmuje: pamięci, którą na płytce biorą WiFi, lwIP
i zadania systemu. Z logu z 2026-11-12 wychodzi na nie ok. 56 kB (płytka
miała 234,6 kB przed budową ekranów i ok. 8 kB po starcie, a v1.2.0 trzyma
171 kB). Dlatego „po starcie zostaje ok. 48 060 B” w wyniku skryptu
(dla v1.2.2: 48 572 B) to **szacunek z jednego logu**. Prawdziwą liczbę
podaje panel: `tag=PAMIEC zdarzenie=po_starcie ram=…`. To samo dotyczy
pobierania wykresu: otwarte połączenie zajmuje na płytce pamięć stosu
sieciowego, której tu nie widać — pokaże ją `ram_min=` w linii
`tag=WYKRESY zdarzenie=pobrano`.

Sprawdzenie szacunku: płytka z v1.2.2 pokazała 2026-11-12
`po_starcie ram=49676`, czyli o 1,1 kB więcej, niż szacował skrypt. Z jej
logu (234 196 B przed budową ekranów, 49 676 B po starcie) na WiFi i system
wychodzi 55 276 B, a skrypt przyjmuje 55 900 B.

Atrapa `String` rośnie tak jak `String` z arduino-esp32 (dokładnie do
potrzeb, w górę do 16 bajtów; przypisanie krótszego tekstu nie zwalnia
bufora), ale pod spodem jest `std::string` — drobne teksty mogą zajmować
o kilkanaście bajtów więcej albo mniej niż na płytce.

### Czy RAM-u starczy: `--przypadki`

Krok [6] uruchamia w modelu 32-bitowym scenariusz `pomiar_przypadki`: po
kolei 27 przypadków pracy, a dla każdego, ile pamięci firmware trzyma po nim
i ile bierze **naraz** w najgorszej chwili — ponad stan po starcie:

- praca bez dotyku (10 minut i doba), ekran alarmu, każdy z pozostałych
  ekranów, polecenie z dotyku;
- ekran Wykresy: wykres godzinny, dzienny i tygodniowy z 8 seriami (więcej
  panel nie pozwala wybrać), pełna historia centrali, wykres z przerwą,
  napisy zamiast wykresu, kod 503, strona HTML, odpowiedź urwana, centrala,
  która zamilkła, wolne łącze, limit 20 s;
- wykres w 80 różnych chwilach względem odpytań `/api/status` (5 szybkości
  łącza × 16 przesunięć) — szukany jest najgorszy zbieg;
- alarm w trakcie pobierania; starsza centrala z całym CSV (8000 wierszy);
  30 i 300 wykresów z rzędu.

Całość biegnie dwa razy: druga runda pokazuje, czy powtórzenie tych samych
czynności coś zostawia. Na koniec doba pracy bez dotyku i 300 wykresów
z rzędu. Wynik dla v1.3.0 (2026-11-12):

```
najwiecej naraz: 25 416 B (centrala po restarcie: napis zamiast wykresu)
na plytce zostaje wtedy ok. 12 748 B wolnego RAM-u; bez otwartych Wykresow najmniej ok. 39 032 B
po pierwszym przejsciu wszystkich przypadkow zostaje 1 248 B (tworzone raz), po drugim przybywa 0 B
```

Pojedyncze liczby różnią się między przebiegami o kilkadziesiąt bajtów.
Udawana centrala działa w osobnym wątku i raz na kilka przebiegów któreś
odpytanie nie zdąży; panel pokazuje wtedy w nagłówku powód, a ten napis
zostaje w pamięci (40 B, jednorazowo).

Kolumna „na plytce” to **szacunek**: wolny RAM płytki po starcie minus
„naraz” minus pamięć sieci, której model nie liczy. Stałe są w nagłówku
skryptu, z opisem, skąd pochodzą:

- `RAM_PO_STARCIE_V122_B=49676` — z logu płytki z v1.2.2; dla innych źródeł
  skrypt odejmuje to, o ile więcej trzymają po starcie i w zmiennych
  statycznych (v1.3.0: 512 B);
- `SIEC_ODPYTANIA_B=3500` — z tego samego logu: w spoczynku 50,1–50,5 kB,
  najmniej 41,6 kB, a model daje na odpytanie 5,4 kB;
- `SIEC_WYKRES_B=7500` — nie zmierzone; górna granica z ustawień lwIP
  (okno odbioru TCP 5 760 B, czyli najwyżej 4 ramki czekające na odczyt).

Krok [7] powtarza zwykły ciąg czynności (praca, wejście na Wykresy, wykres
dzienny z 8 seriami, 20 s oglądania, powrót) ze stertą modelu ograniczoną
limitem: tyle, ile firmware trzyma w spoczynku, plus zadana liczba bajtów.
Przydział ponad limit dostaje odmowę, jak `malloc()` na płytce bez wolnego
RAM-u. Wynik:

```
40 000 B   wszystko dziala, zadnej odmowy pamieci
26 000 B   wszystko dziala, zadnej odmowy pamieci
24 000 B   wykres jest; w trakcie 3 odpytan centrali bez pamieci (naglowek: BRAK PAMIECI), po wyjsciu z Wykresow wraca do normy
20 000 B   wejscie na Wykresy (8 z 8 buforow) -> LVGL bez pamieci -> restart panelu z wpisem w logu
18 000 B   napis "Brak pamieci na wykres" (7 z 8 buforow); panel pracuje dalej
12 000 B   wejscie na Wykresy (5 z 8 buforow) -> LVGL bez pamieci -> restart panelu z wpisem w logu
 4 000 B   juz na pulpicie czesc odpytan centrali bez pamieci; napis "Brak pamieci na wykres" (1 z 8 buforow); potem restart (45 s bez danych z centrali)
przedzialy (129 prob, co 250 B od 0 do 32 000 B):
  od 24 900 B          wszystko dziala, zadnej odmowy pamieci
  21 000 - 24 750 B    wykres jest; czesc odpytan centrali bez pamieci, po wyjsciu z Wykresow wraca do normy
  ponizej 21 000 B     wejscie na Wykresy konczy sie restartem panelu z wpisem w logu (61 z 84 prob) albo napisem (23 z 84)
  awarii modelu (siegniecie pod zly adres): 0 z 162 prob
prog: od 24 900 B wolnego RAM-u w gore nie ma ani jednej odmowy pamieci
plytka: 49 164 B po starcie - 11 000 B sieci = 38 164 B dla firmware'u, czyli 13 264 B ponad prog
```

Poniżej 21 kB to, czy będzie napis, czy restart, zależy od tego, ile buforów
punktów (po 2 304 B) zmieści się przed wyczerpaniem pamięci: bufory są
przydzielane po kolei, a następny w kolejce jest przydział LVGL — jego brak
kończy się restartem (`panel_lvgl_assert()`).

Skrypt kończy się kodem 1, gdy: „najwięcej naraz” przekracza 27 000 B,
szacunek dla płytki spada poniżej 8 000 B, w drugiej rundzie, w ciągu doby
albo po 300 wykresach przybywa ponad 256 B, wykres albo napis nie pojawia
się tam, gdzie powinien, albo program modelu kończy się awarią (sięgnięcie
pod zły adres — na płytce byłby to wyjątek procesora i restart; skrypt
wypisuje wtedy stos wywołań). Wypróbowane na celowo zepsutej kopii źródeł
(48 B gubione przy każdym wykresie): przyrost 8 128 B w drugiej rundzie
i 19 188 B po 300 wykresach, kod wyjścia 1.

Ograniczenia kroku [7]:

- Limit jest jedną liczbą dla całej sterty. Sterta płytki bywa pofragmentowana
  — tego model nie odtwarza. Największy pojedynczy przydział firmware'u
  w czasie pracy to bufor punktów wykresu, 2 304 B.
- Odmowa nie dotyczy atrapy `String` (`std::string` skończyłby program
  wyjątkiem; prawdziwy `String` zostaje pusty). Brak pamięci na odebranie
  odpowiedzi sprawdza osobny scenariusz, `brak_pamieci_odbior`.
- Zmniejszenie bloku przez `realloc()` zawsze się udaje, jak na płytce.
  Wcześniejsza wersja modelu potrafiła go odmówić — a ArduinoJson 7.2.1
  kończy każdy odczytany tekst takim zmniejszeniem i nie sprawdza wyniku,
  więc model kończył się awarią w `StringPool::add()`. Na płytce ta droga
  nie istnieje.

## Sprawdzenie prawdziwymi nagłówkami ESP32

`./sprawdz_naglowki_esp32.sh` kompiluje `main.cpp`, `dashboard_lvgl_poc.cpp`
i `lvgl_port.cpp` w trybie „tylko sprawdź” (`-fsyntax-only`) z prawdziwymi
nagłówkami: rdzeń i biblioteki arduino-esp32 (3.0.7; `RDZEN=3.3.8` dla
nowszego), ESP-IDF 5.1.4 z konfiguracją płytki `esp32`, newlib, TFT_eSPI
2.5.43, LVGL 8.3.11, ArduinoJson 7.4.3, flagi `-D` z `platformio.ini`.
Kompilatorem jest clang z pakietu Pythona `ziglang` (ma cel xtensa); wszystko
pobiera się z PyPI i GitHuba do katalogu podręcznego (ok. 250 MB).

Wykrywa to, czego atrapy nie widzą: literówkę w nazwie funkcji, złą liczbę
albo typ argumentów, brakujący `#include`. Nie jest to build PlatformIO:
inny kompilator (clang zamiast GCC) i biblioteka C++, bez linkowania i bez
pliku `.bin`; przy `RDZEN=3.3.8` nagłówki rdzenia 3.3.8 stoją na ESP-IDF 5.1,
więc dwa typy znane tylko nowszemu IDF są podstawione zastępczo (szczegóły
w nagłówku skryptu).

`OSTRZEZENIA=1 ./sprawdz_naglowki_esp32.sh` dodatkowo liczy ostrzeżenia clang
(`-Wall -Wextra -Wformat=2 -Wshadow`) w sprawdzanych plikach i pokazuje
pierwsze z nich. Wynik skryptu zależy nadal tylko od błędów: ostrzeżenia
clang nie są ostrzeżeniami GCC z PlatformIO.

## Porównanie ze starszą wersją

Ten sam zestaw scenariuszy można puścić na innym katalogu źródeł w trybie
obserwacji (bez oceniania, każde sprawdzenie pokazuje `tak`/`NIE`):

```
mkdir -p /tmp/stary && unzip -q "../../LCD Centrali.zip" -d /tmp/stary     # v1.1.0
SRC=/tmp/stary/src OBSERWACJE=1 ./uruchom_testy.sh
```

Scenariusze `wykresy_*` na źródłach do v1.2.2 pokazują same `NIE` — tamte
wersje pobierały dane wykresów przez `HTTPClient`, którego atrapa nie łączy
się z udawaną centralą (patrz „Czego to NIE sprawdza”). Wyjątkiem jest
`wykresy_stary_csv`, który podaje CSV ekranowi wprost; uruchomiony tak na
źródłach v1.2.2 zapisuje punkty, które ona narysowała (`wzorce_v122/CZYTAJ.txt`).
