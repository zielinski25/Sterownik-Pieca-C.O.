# Testy panelu LCD na PC

Ten katalog pozwala uruchomić **prawdziwy** `src/main.cpp` panelu (razem z
`dashboard_lvgl_poc.cpp` i `lvgl_port.cpp`) na zwykłym komputerze, bez płytki
ESP32. PlatformIO go nie widzi i nie kompiluje — nie wpływa na firmware.

```
./uruchom_testy.sh                 # wszystko: testy jednostkowe + symulator (kilka sekund; pierwszy raz dłużej)
./uruchom_testy.sh --szybko        # tylko testy jednostkowe (sam g++, 1 s)
./uruchom_testy.sh dwie_centrale   # jeden scenariusz, z pełnym logiem Seriala na ekranie
TYLKO='^ota_' ./uruchom_testy.sh   # tylko scenariusze o nazwach pasujących do wyrażenia (tu: aktualizacja OTA)
SANITIZER=1 ./uruchom_testy.sh     # to samo z AddressSanitizer + UBSan (wolniej)
python3 sprawdz_pola_statusu.py    # czy prawdziwa centrala wysyła w /api/status każde pole, które panel czyta (krok [1b]; v1.4.6 erratum)
python3 sprawdz_czulosc_ota.py     # czy scenariusze ota_* wykrywają celowo wprowadzone błędy (ok. 10 minut)
./sprawdz_naglowki_esp32.sh        # składnia i typy prawdziwymi nagłówkami ESP32 (patrz niżej)
./pomiar_pamieci.sh                # ile RAM-u trzyma firmware + limity (patrz niżej; wymaga: pip install ziglang)
./pomiar_pamieci.sh --przypadki    # czy RAM-u starczy w każdym przypadku pracy (ok. 4 minut)
./pomiar_obciazenia.sh            # ile panel rysuje, pyta centralę i prosi o pamięć, gdy nic się nie zmienia (patrz niżej)
```

Wymaga: `g++` (C++17), `gcc`, `ar`, `git`, Linux albo WSL. Przy pierwszym
uruchomieniu skrypt pobiera z GitHuba LVGL 8.4.0 i ArduinoJson 7.4.3 (te
same wersje, które od v1.4.1 przypina `platformio.ini`) do
katalogu podręcznego (`$LCD_TESTY_CACHE`, domyślnie `/tmp/lcd_testy_host`).
Logi Seriala i zrzuty ekranu (`.bmp`) z każdego scenariusza trafiają do
`<katalog podręczny>/wyniki/`.

`src/secrets.h` nie jest potrzebny — gdy go nie ma, testy dostają atrapę z
wymyślonymi hasłami (atrapa WiFi haseł nie sprawdza).

**Inne wersje bibliotek.** Do v1.4.0 `platformio.ini` dopuszczał każde LVGL
8.x od 8.3.11 i każdy ArduinoJson 7.x od 7.2.0, a skrypty budowały na
8.3.11 i 7.2.1; od v1.4.1 jedno i drugie to 8.4.0 i 7.4.3. Skrypty pobierają
biblioteki tylko wtedy, gdy nie ma ich w katalogu podręcznym, więc inną
wersję (np. do porównania ze starą) wystarczy włożyć tam wcześniej:

```
mkdir -p /tmp/lcd_stare
git clone --depth 1 --branch v8.3.11 https://github.com/lvgl/lvgl.git /tmp/lcd_stare/lvgl
git clone --depth 1 --branch v7.2.1 https://github.com/bblanchon/ArduinoJson.git /tmp/lcd_stare/ArduinoJson
LCD_TESTY_CACHE=/tmp/lcd_stare ./uruchom_testy.sh
LCD_TESTY_CACHE=/tmp/lcd_stare ./pomiar_pamieci.sh --wszystko
```

Stan na 2026-11-12, v1.4.0: z LVGL 8.4.0 i ArduinoJson 7.4.3 przechodzą
wszystkie scenariusze (także w modelu 32-bitowym), a pomiar pamięci daje
129 324 B po starcie, 5 324 B na jedno odpytanie i 2 452 B / 5 352 B chwilowo
na wykres (punkty z centrali / cały CSV); z opcją `--przypadki`: najwięcej
naraz 25 356 B, aktualizacja panelu 11 344 B, próg 24 800 B. Ze starymi
wersjami (8.3.11 i 7.2.1) te same liczby to 129 324, 5 464, 2 452 /
5 492, 25 496, 11 484 i 24 900 B.

## Co jest w środku

| Plik | Co robi |
|---|---|
| `test_diagnostyka.cpp` | Testy jednostkowe `src/diagnostyka_panelu.h` (kod HTTP, dławik logu, strażnik świeżości, wybór najsilniejszego AP, znacznik centrali, próg baterii; od v1.5.0: decyzja o przyciemnieniu i śnie na baterii — `senDecyzja()`; od v1.5.1: tabele czasów „Przyciemnij po / Uśpij po”, „nigdy”, filtr pierwszego dotknięcia przyciemnionego ekranu `dotykPrzepusc()`; od v1.5.2: obrót ekranu a kalibracja dotyku `dotykTrzebaOdwrocic()`/`dotykObroc180()`; od v1.2.1: przyczyna braku danych, drabina ratunkowa, pamięć i token samoczynnego restartu; od v1.2.2: brak pamięci jako przyczyna; od v1.4.0: aktualizacja przez centralę — identyfikator programu, klucz oferty, zapis stanu w NVS, decyzja „zacząć czy czekać”, sprawdzenie nagłówka pliku, adres zapytania o status). 588 sprawdzeń. |
| `mock/` | Atrapy nagłówków Arduino/ESP-IDF (`Arduino.h`, `WiFi.h`, `TFT_eSPI.h`, `Preferences.h`, `esp_*.h`…). Stan „płytki” opisuje `mock/atrapa.h`. Od v1.4.0: `Update.h` (zapis nowego programu), `esp_partition.h` (suma własnego programu), `esp_ota_ops.h` (zatwierdzenie programu po starcie), `atrapa_sha256.h` (SHA-256 do sprawdzenia pliku). `HTTPUpdate.h` i `HTTPClient.h` zostały dla źródeł starszych niż v1.4.0. |
| `symulator_panelu.cpp` | Uruchamia `setup()` i `loop()` firmware'u, podstawia mu „centrale” (lokalny serwer HTTP) i sprawdza log oraz to, co trafiło na ekran. Odpowiedź `/api/status` ma te same pola, kolejność i format liczb co `wifiZbudujJsonStatus()` w firmware centrali 3.58.3 (ok. 1,8 kB). Udawana centrala traktuje parametr `reset_powod` tak jak 3.58.3: „pisze” o powodzie restartu panelu przy każdej zmianie wartości innej niż `sw` i `deepsleep`. Od v1.3.0 odpowiada też na `/api/history`, `/api/history/long` i `/api/history/panel` (jak centrala 3.58.4, jak starsza albo jak bardzo stara) i potrafi odpowiedź urwać, wstrzymać, opóźnić albo wysyłać z zadaną szybkością. Od v1.4.0 wydaje też plik `/panel-ota.bin`, a o ofercie aktualizacji decyduje kodem wyciętym z centrali 3.58.6 (`kod_centrali/`). |
| `kod_centrali/` | Blok decyzji o aktualizacji panelu wycięty z firmware'u centrali 3.58.6 (opis: `CZYTAJ.txt` w tym katalogu). Symulator włącza go przez `#include`; testy centrali sprawdzają przy każdym uruchomieniu, że kopia jest identyczna z jej plikiem. |
| `sprawdz_czulosc_ota.py` | 23 razy psuje jedną rzecz w kopii źródeł (pominięte sprawdzenie, brak przerwania przy alarmie, brak zapisu stanu…) i sprawdza, że scenariusze `ota_*` albo testy jednostkowe to wykrywają. To sprawdzenie testów, nie kodu. |
| `wzorce_centrali/` | Odpowiedzi centrali na `/api/history/panel`, wypisane przez jej własny kod — 11 plików; 9 takich samych z 3.58.4 i 3.58.5, 2 tylko z 3.58.5 (opis: `CZYTAJ.txt` w tym katalogu). Z nich korzystają scenariusze `wykresy_*`. |
| `wzorce_v122/` | Punkty, które z tych samych starych CSV narysował firmware v1.2.2 (opis: `CZYTAJ.txt` w tym katalogu). Z nimi porównywane jest rysowanie po staremu. |
| `uruchom_testy.sh` | Buduje i uruchamia całość: 588 sprawdzeń jednostkowych i 69 scenariuszy (772 sprawdzenia). Pliki firmware'u kompiluje z `-Wall -Wextra` — nowe ostrzeżenie to coś do poprawienia. |
| `sprawdz_naglowki_esp32.sh` | Sprawdza składnię i typy źródeł panelu prawdziwymi nagłówkami arduino-esp32 / ESP-IDF / TFT_eSPI (bez atrap). |
| `sprawdz_pola_statusu.py` | Krok `[1b]` `uruchom_testy.sh`: czy każde pole, które panel czyta z `/api/status` (`diag::POLA_STATUSU_CZYTANE[]` + `symulacja_*`), **prawdziwa centrala** wystawia w `wifiZbudujJsonStatus()` (najnowszy `firmware-patch/v3.*/src/main_centrala.cpp`; inny plik: `CENTRALA_CPP=…`). Atrapa centrali podaje to, co jej każemy — v1.4.6 dostał pole `fw`, którego centrala do 3.58.7 w `/api/status` nie miała, i testy były zielone, a na płytce „Centrala -”. Na 3.58.6/3.58.7 skrypt zgłasza brak `fw`, na 3.58.8 przechodzi. Wyjątek: `symulator` (wysyła tylko centrala testowa). |
| `pomiar_pamieci.sh` | Mierzy, ile pamięci RAM trzyma firmware, i pilnuje limitów. Buduje ten sam program jako 32-bitowy, z `pomiar_sterty.c` zamiast zwykłego `malloc()`. Z `--przypadki`: ile pamięci trzeba w każdym przypadku pracy i co się dzieje, gdy jest jej mniej. |
| `pomiar_sterty.c` | `malloc()` z licznikiem: dla każdego bloku pamięta rozmiar i stos wywołań; liczy go tak, jak zajmuje miejsce na stercie ESP32. Potrafi też odmawiać ponad zadany limit (jak płytka bez wolnego RAM-u) i wypisać stos wywołań, gdy program modelu sięgnie pod zły adres. |
| `pomiar_rozbicie.py` | Dla `pomiar_pamieci.sh --kto`: zamienia zapisane stosy wywołań na nazwy funkcji firmware'u i sumuje pamięć według nich. |
| `pomiar_obciazenia.sh` | Mierzy obciążenie w spoczynku: ile pikseli idzie do ekranu, ile połączeń z centralą i ile próśb o pamięć na minutę; czy panel na baterii usypia albo przygasza ekran. Same liczby, bez oceny. Z `--prototyp` dodatkowo szacunek po zmianie „ustawiaj tylko to, co się zmieniło”. |
| `prototypy/` | `ustaw_tylko_zmiany.c` — cztery funkcje LVGL podmienione przy linkowaniu na wersje, które najpierw porównują nową wartość ze starą. Tylko dla `pomiar_obciazenia.sh --prototyp`. To nie jest poprawka firmware'u i nie trafia na płytkę. |

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
| `ota_w_trakcie_pracy` | v1.4.0, centrala 3.58.6. Pracujący panel dostaje ofertę i w ciągu kilku sekund zaczyna sam: pobiera plik 1,2 MB z paskiem postępu (ekran `AKTUALIZACJA PANELU`), zapisuje stan w NVS i restartuje się. Do pamięci programu trafia plik z centrali bajt w bajt. W trakcie panel dalej pyta centralę o status, pętla nie staje (watchdog nie działa), hasła OTA nie ma w logu. |
| `ota_po_restarcie` | v1.4.0. Pierwszy start nowego programu: panel zgłasza `panel_obraz_proba`, zatwierdza program i odtąd zgłasza `panel_obraz`; centrala potwierdza po identyfikatorze pliku, choć panel zgłasza `v1.4.0`, a tag wydania to `3.58.6`. Drugiego pobierania nie ma. |
| `ota_wycofany_firmware` | v1.4.0. Po wgraniu działa inny program niż wgrany (nowy nie wstał, bootloader wrócił do poprzedniego): panel zgłasza odmowę `cofnieta` z numerem oferty, centrala kończy ofertę, panel tego pliku więcej nie pobiera. |
| `ota_stara_centrala` | v1.4.0 z centralą do 3.58.5 (flaga bez identyfikatora pliku; panel rozpoznaje ofertę po tagu wydania). W czasie alarmu panel nie zaczyna, choć taka centrala flagi nie chowa; alarm w trakcie przerywa pobieranie najdalej po 2,5 s. Po alarmie: pobranie od początku, wgranie, restart. |
| `ota_stara_centrala_po_restarcie` | v1.4.0 z centralą do 3.58.5 po wgraniu: jej flaga zostaje (zgłoszona wersja to nie tag wydania), a panel **nie** wgrywa pliku drugi raz i się nie restartuje — pamięta, że plik z tego wydania już działa. |
| `ota_nieudane_pobieranie` | v1.4.0. Trzy próby, każda z innym błędem: centrala milknie w połowie pliku (panel czeka 15 s), zrywa połączenie, odpowiada kodem 500. Kolejna próba nie wcześniej niż po 30 s. Po trzeciej panel zgłasza odmowę `pobieranie`; między próbami pracuje normalnie, rozpoczęty zapis jest porzucany, w logu błędu sieci jest `token=***` zamiast hasła. |
| `ota_alarm` | v1.4.0. W czasie alarmu centrala 3.58.6 nie pokazuje oferty i panel nie zaczyna. Alarm w trakcie pobierania: przerwane przy pierwszej odpowiedzi centrali z alarmem (najdalej po 2,5 s), ekran alarmu i brzęczyk działają, próba nie liczy się do trzech; po alarmie panel pobiera od początku i kończy. |
| `ota_zly_plik` | v1.4.0. Plik uszkodzony w środku (odrzuca go sprawdzenie obrazu przed uruchomieniem), złe hasło (401), plik większy niż partycja (odrzucony po nagłówku), program dla ESP32-S3 (centrala 3.58.6 odrzuca go sama; od starszej centrali panel poznaje go po nagłówku), plik inny niż w ofercie: za każdym razem jedna próba i odmowa z właściwą przyczyną; nic nie jest uruchamiane. |
| `ota_warunki` | v1.4.0. Warunki startu po kolei: 20 s po włączeniu, 15 s bez dotyku, za mało wolnego RAM-u, słabe ogniwo bez zasilania z USB, nieudany zapis do NVS. Panel podaje centrali, na co czeka (`panel_ota_czeka=…`). Oferta wycofana w trakcie pobierania: przerwane najdalej po 2,5 s, próba się nie liczy. |
| `ota_ten_sam_plik` | v1.4.0. Centrala oferuje plik, z którego panel już pracuje. 3.58.6: panel nic nie pobiera, centrala potwierdza przy pierwszym zapytaniu. Centrala do 3.58.5: panel pobiera plik raz, poznaje go po identyfikatorze z końca pliku, nie wgrywa i się nie restartuje, a potem już go nie pobiera. |
| `ota_status_milczy` | v1.4.0. Centrala przestaje odpowiadać na `/api/status`, ale plik wysyła (wolne łącze, ok. 57 s): panel nie wymusza ponownego połączenia WiFi i nie restartuje się z braku danych; plik dochodzi do końca. |
| `ota_bez_identyfikatora` | v1.4.0. Panel nie odczytał sumy własnego programu: w logu `brak_identyfikatora_obrazu`, zapytania o status bez `panel_obraz` (jak przed v1.4.0), a oferty centrali panel nie rusza. |
| `kafelki_szerokosc` | v1.5.2. Najszersze wartości każdego kafelka (`-25.5°C`, `-10.5°C`, `100 %`, `1013 hPa`, `100°C`, `9999`, `Klapa 100%`): każda etykieta kończy się ≥ 4 px przed prawą krawędzią kafelka; `1013 hPa` czcionką 20 z tym samym dołem co sąsiad czcionką 28; deszcz: 105 min → „za 2 h” (28), 100 → „za 1,5 h”, 99 → „za 99 min”, 5 → „za 5 min” (20), pada 30 → „jeszcze / 30 min” (dwie linie, 14), 120 → „jeszcze 2 h”, -1 → „pada” (28); po deszczu „3.5°C” wraca czcionką 28 na wysokość POKÓJ; log `tag=DOTYK … odwrocony=1` (kalibracja z obrotu 1 na ekranie w obrocie 3). |
| `polskie_litery` | v1.4.5. Czcionki `lv_font_pl_14/20/28`: glif dla każdego znaku użytego na ekranach, napisy z ogonkami na pulpicie i panelach. |
| `wersje_na_ekranie` | v1.4.6. Wersje panelu i centrali na ekranie startowym i na WYSWIETLACZ, linia `tag=PANEL` z `centrala_fw=`, napis kafelka WYŚWIETLACZ mieści się w karcie. |
| `zasilanie` | v1.4.7. Ekran ZASILANIE: po starcie bez ustawień 240 MHz i 19,5 dBm (jak do v1.4.6); `WiFi.setTxPower()` nigdy przed `WiFi.mode()`; taktowanie ustawiane na końcu `setup()`; przyciski 240→160→80→240 MHz i 19,5→15 dBm zmieniają układ od razu i zapisują indeks w NVS; odczyt pod przyciskami (RSSI panelu, MHz, dBm); przy −80 dBm ostrzeżenie, przy −70 dBm znika; X i zamknięcie samoczynne po 13,5 s; `tag=PANEL` z `cpu_mhz=80 tx_dbm=15.0 tx_ok=1`; po zerwaniu i ponownym `WiFi.begin()` moc ustawiona od nowa bez dodatkowych linii logu. |
| `zasilanie_po_restarcie` | v1.4.7. NVS z `cpuIdx=2`, `txIdx=3`: po starcie 80 MHz i 8,5 dBm, kafelki z danymi, przyciski pokazują zapisane wartości, 8,5 → 19,5 dBm (lista zawija się). |
| `zasilanie_zly_indeks` | v1.4.7. Nieznane indeksy w NVS (200, 7): domyślne 240 MHz i 19,5 dBm, żadnej złej wartości do `setCpuFrequencyMhz()`. |
| `sen_na_baterii` | v1.5.0. Ogniwo 3,90 V, USB odłączone: pierwszy `tag=BATERIA` od razu po starcie; 59 s bez dotyku jasno, 62 s — 5 % (wypełnienie 12/255, `tag=SEN zdarzenie=przyciemnienie`), panel dalej pyta centralę; nic nie rozjaśnia po cichu; dotyk (`lv_disp_trig_activity`) i przycisk GPIO14 przywracają jasność w 100 ms; 5 min od ostatniej aktywności (±2 s) `esp_deep_sleep_start()`: podświetlenie 0, `gpio_hold_en` na 21 i 13, brzęczyk cicho i nisko, ext0 GPIO14/LOW, ext1 GPIO35/ALL_LOW, bez napisu o baterii. |
| `sen_usb_nie_usypia` | v1.5.0. Ogniwo + ładowarka (GPIO35 nisko): 6,5 min bez dotyku — jasno, bez snu, żadnej linii `tag=SEN`; v1.5.3: wskaźnik „USB 64%”, 2 s po wyjęciu ładowarki „64%” + log `usb_odlaczone`, 1,5 s po włożeniu „USB 64%” + `usb_podlaczone`; ładowarka wyjęta po dotyku — po minucie 5 %; ładowarka włożona — jasność wraca sama (`powod=usb`); sam USB bez ogniwa (wskaźnik „USB”) — nigdy. |
| `sen_alarm` | v1.5.0. Alarm przegrzania 7 min bez dotyku na baterii: jasno, bez snu, `sen_wstrzymany powod=alarm` raz; alarm ustaje — nikt nie dotykał od 7 min, więc sen w ciągu sekund. |
| `sen_ota` | v1.5.0. Ogniwo 4,10 V, pobieranie 1,2 MB po 6 kB/s: po 1,5 min bez dotyku jasno, bez snu, `sen_wstrzymany powod=ota`, bez przyciemnienia. |
| `sen_rozladowana` | v1.5.0. Ogniwo 3,25 V bez ładowarki, ktoś właśnie dotknął: napisy TFT „BATERIA ROZLADOWANA” / „Podlacz ladowarke. Panel usypia.”, sen 2,5 s po końcu `setup()` (`powod=bateria_rozladowana napiecie=3.25V`), hold na 21/13. |
| `sen_rozladowana_alarm` | v1.5.0. 3,25 V w alarmie: 30 s bez snu, ekran alarmu; 3,05 V: przy następnym pomiarze sen (`powod=bateria_krytyczna`), brzęczyk cicho. |
| `sen_po_wybudzeniu` | v1.5.1. Start po `ESP_SLEEP_WAKEUP_EXT0` (przycisk) na baterii, BSSID/kanał centrali w pamięci RTC: `zrodlo=przycisk`; **żadnego** napisu TFT ekranu startowego („WYSWIETLACZ CO”, „Panel v…”, „Laczenie z WiFi”, „Szukam centrali”, „Centrala v…”); `tag=SEN zdarzenie=pulpit_po_wybudzeniu` **przed** `zdarzenie=polaczono`; co najmniej 153 600 px poszło do wyświetlacza w `setup()`; `setup()` < 2 s (bez pauzy 1,5 s), bez skanu; linia `tag=PANEL zdarzenie=ekran_startowy` jest w logu; po 3 s kafelki BOJLER/ZEWN z danymi, pełna jasność; minuta bez dotyku → 5 %. |
| `sen_po_wybudzeniu_ladowarka` | v1.5.1. Start po `EXT1` (ładowarka): `zrodlo=usb`, pulpit od razu, 90 s na ładowarce bez przyciemnienia. |
| `sen_ustawienia` | v1.5.1. Drugi pulpit → WYŚWIETLACZ: wiersz „Na baterii” obok dotychczasowych; przyciski „Przyciemnij po: 1 min” / „Uśpij po: 5 min”; przycisk „Uśpij po” kończy się ≥ 10 px nad dołem i prawą krawędzią, wiersz harmonogramu nad nim; dotknięcie → „2 min”, NVS `wyswietlacz/senPrzyIdx=2`, log `tag=SEN zdarzenie=ustawienie przyciemnij_ms=120000 uspij_ms=300000`; „Uśpij po” po kolei 10 min → 30 min → 1 godz. → nigdy → 2 min → 5 min (zawija), NVS `senSpijIdx=1`; po zamknięciu ekranu: 65 s jasno, 125 s → 5 %, sen 5 min po dotyku. |
| `sen_ustawienia_nigdy` | v1.5.1. NVS `senPrzyIdx=5` (nigdy), `senSpijIdx=0` (2 min): przyciski pokazują zapisane wartości; 100 s bez dotyku jasno, sen 120 s (±2 s) po dotyku bez wcześniejszego przyciemnienia. |
| `sen_ustawienia_nigdy_sen` | v1.5.1. NVS `senPrzyIdx=77` (nieznany → domyślna minuta), `senSpijIdx=5` (nigdy): po 65 s 5 %, po 16 min nadal nie śpi; 3,25 V → sen `powod=bateria_rozladowana` mimo „nigdy”. |
| `poprawki_v141` | v1.4.1. Zrzut po awarii w logu i skasowany; jasność 0 % z NVS podniesiona do 5 %; `stos_min=`, `alok_nieudane=` w linii `tag=PANEL`; odpowiedź dłuższa niż 8 kB kończy się błędem `odpowiedz za dluga`, panel pyta dalej. |
| `polecenia_nie_gina` | v1.4.2. Centrala odpowiada na POST po 300 ms. Pięć kliknięć `+` co 100 ms: centrala dostaje końcową wartość (10), dwa POST-y zamiast pięciu, nic nie jest pominięte. Przytrzymany `+` (20 kroków co 100 ms): najwyżej 3 POST-y, końcowa wartość po obu stronach. Odczyt ustawień wystartowany w trakcie polecenia nie cofa ekranu (`stara_wartosc_pominieta`). Polecenie odrzucone przez centralę: pasek `ODRZUCONE: tryb serwa nie jest reczny`, ekran wraca do wartości centrali, pasek znika po 3 s. Centrala milczy na POST: pasek `POLECENIE NIE DOSZLO DO CENTRALI`, jedna linia błędu. |
| `bez_centrali_dlugo` | v1.4.2. Po trzech samoczynnych restartach sieci centrali nadal nie ma: przez 20 min żadnego restartu (do v1.4.1 co 5 min), próby połączenia dalej co ≤ 30 s, nagłówek `BRAK WIFI 20 min`. Gdy centrala wraca — dane w ciągu minuty, bez restartu. |
| `centrala_milczy_dlugo` | v1.4.2. To samo, gdy WiFi jest, a centrala milczy: bez restartu, wymuszone łączenie od nowa co 30 s (11 w 5 min), nagłówek `CENTRALA MILCZY 5 min`; gdy centrala znów odpowiada — dane bez restartu. |
| `ustawienia_na_zadanie` | v1.4.3 (B4). 2 min na pulpicie: 60 odczytów statusu i **2** odczyty ustawień (do v1.4.2: 24); zmiana trybu pompy i serwa „z telefonu” znana panelowi w 3 s (ze statusu); otwarcie ekranu USTAWIENIA POMPY = odczyt w 300 ms, potem co 5 s (12 na minutę), zmiana liczby z telefonu widoczna w 5 s; po własnym poleceniu odczyt od razu; po powrocie na pulpit znów 2 na 2 min. |

W scenariuszach `ota_*` firmware idzie tą samą drogą co na płytce: własnym
gniazdem pobiera plik od udawanej centrali (lokalny serwer HTTP) i oddaje go
do `Update.write()`. Udawana jest pamięć programu i wszystko pod nią — lista
w rozdziale „Czego to NIE sprawdza”. Restart panelu kończy scenariusz, więc
„po restarcie” to osobne scenariusze, które zaczynają od zapisu w NVS takiego,
jaki zostawia firmware.

Po stronie centrali 3.58.6 decyduje **jej własny kod** (`kod_centrali/`):
kiedy pokazać ofertę, kiedy wydać plik, kiedy uznać aktualizację za wykonaną.
Centrala do 3.58.5 jest modelem jej reguły (oferta znika tylko wtedy, gdy
zgłoszona wersja jest identyczna z tagiem wydania), nie jej kodem. Limit 24 h,
przypomnienie po 5 minutach i treść wiadomości Telegrama sprawdzają testy
centrali (`firmware-patch/v3.58.6-panel-ota/tests/`).

Jak było do v1.3.0: `RAPORT_OTA_PANELU_PRZEZ_CENTRALE.md` w katalogu głównym
repozytorium. Sześć scenariuszy z tamtego sprawdzenia (`ota_start`,
`ota_po_restarcie_zgodna`, `ota_po_restarcie_inna_wersja`,
`ota_flaga_w_trakcie_pracy`, `ota_nieudane`, `ota_przy_alarmie`) opisywało
stary kod i zostało zastąpione powyższymi.

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
- **Aktualizacja OTA (od v1.4.0).** Pobieranie pliku idzie prawdziwym kodem
  firmware'u, po lokalnym gnieździe. Wszystko pod `Update.write()` jest
  atrapą:
  - `mock/Update.h` — zapis nowego programu. Trzyma dane w pamięci, bierze
    ze sterty bufor 4 kB (tak jak biblioteka, żeby pomiar pamięci go liczył),
    każde zapisane 4 kB „trwa” zadany czas (domyślnie 15 ms), a na końcu
    sprawdza obraz w uproszczeniu: pierwszy bajt, układ ESP32, SHA-256 treści
    równe ostatnim 32 bajtom pliku. Nie ma prawdziwej pamięci flash,
    segmentów programu, sumy kontrolnej nagłówka ani błędów kasowania.
  - `mock/esp_ota_ops.h` (`mock/esp_partition.h` tylko go włącza) — suma
    własnego programu (`esp_partition_get_sha256()`): wartość wpisana przez
    scenariusz albo błąd. Tego, że na płytce ta funkcja zwraca właśnie
    ostatnie 32 bajty wgranego pliku, test nie sprawdza — wynika to ze źródeł
    ESP-IDF, a pokaże pierwszy start v1.4.0 (linia `stan_przy_starcie obraz=…`
    ma być równa identyfikatorowi z `narzedzia/ota_panelu_sprawdz.py --bin`).
    Sam układ pliku (suma na końcu, numer układu w nagłówku) jest sprawdzony
    na prawdziwych obrazach z repozytorium esptool:
    `firmware-patch/v3.58.6-panel-ota/tests/sprawdz_prawdziwe_obrazy.sh`.
  - W tym samym pliku: stan programu po starcie („do zatwierdzenia”) i samo
    zatwierdzenie. Bootloadera, który wraca do poprzedniego programu, nie
    ma: scenariusz `ota_wycofany_firmware` zaczyna od stanu, jaki taki powrót
    zostawia.
  - NVS (`mock/Preferences.h`) — mapa w pamięci, z możliwością wymuszenia
    błędu zapisu.

  Czasy z tych scenariuszy (1,2 MB w ok. 8 s, najdłuższy obrót pętli 35 ms)
  wynikają z założeń: łącze 150 kB/s i 15 ms na każde 4 kB. Na płytce
  rozkład jest inny: biblioteka `Update` kasuje pamięć programu blokami po
  64 kB, a kasowanie bloku zatrzymuje oba rdzenie na ułamek sekundy. Co
  64 kB pobranego pliku jeden obrót pętli będzie więc wyraźnie dłuższy niż
  w modelu. Ile to trwa i jak wygląda wtedy ekran, pokaże dopiero płytka.
  Stosu zadania pętli (8 kB) testy na PC nie mierzą wcale.
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

Wynik dla v1.4.0:

```
po starcie firmware trzyma          129 324 B w 2470 blokach
jedno odpytanie centrali, chwilowo    5 464 B wiecej
przyrost po 10 minutach pracy             0 B
ekran alarmu                            188 B wiecej
ekran Wykresy otwarty                18 632 B wiecej
...i po jego zamknieciu                  24 B wiecej
wykres z centrali 3.58.4 (pobranie i rysowanie), chwilowo 2 452 B wiecej
wykres po staremu (caly CSV, 352 kB), chwilowo        5 492 B wiecej
po pierwszym wykresie zostaje           424 B (opisy osi i legenda, tworzone raz)
przyrost po 5 kolejnych wykresach       -28 B
zmienne statyczne firmware'u          5 366 B (v1.2.0: 3 092 B)
```

v1.3.0 i v1.2.2: 129 244 B po starcie i 5 384 B na odpytanie — v1.4.0 ma po
80 B więcej (odpytanie: dłuższy adres, bo doszedł identyfikator programu).
v1.1.0 i v1.2.0 trzymały po starcie 171 kB (170 324 B i 170 572 B), a jedno
odpytanie brało 7 080 B i 7 208 B.

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
512 B (bufor jednej linii pobieranej historii, 300 B, i stan pobierania);
v1.4.0 o kolejne 898 B (drugi obiekt pobierania — osobny dla aktualizacji,
żeby wykres i aktualizacja nie mogły sobie przerwać — i stan aktualizacji).
Skrypt odejmuje tę różnicę w szacunku i pilnuje limitem, żeby nie pojawiła
się duża tablica statyczna. Limit był 5 000 B; dla v1.4.0 jest 6 000 B,
z uzasadnieniem w nagłówku skryptu.

Czego ten pomiar **nie** obejmuje: pamięci, którą na płytce biorą WiFi, lwIP
i zadania systemu. Z logu z 2026-11-12 wychodzi na nie ok. 56 kB (płytka
miała 234,6 kB przed budową ekranów i ok. 8 kB po starcie, a v1.2.0 trzyma
171 kB). Dlatego „po starcie zostaje ok. 47 082 B” w wyniku skryptu
(dla v1.3.0: 48 060 B, dla v1.2.2: 48 572 B) to **szacunek z jednego logu**. Prawdziwą liczbę
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

Krok [6] uruchamia w modelu 32-bitowym scenariusze `pomiar_przypadki`
i (od v1.4.0) `pomiar_ota`: po kolei 29 przypadków pracy, a dla każdego, ile
pamięci firmware trzyma po nim i ile bierze **naraz** w najgorszej chwili —
ponad stan po starcie:

- praca bez dotyku (10 minut i doba), ekran alarmu, każdy z pozostałych
  ekranów, polecenie z dotyku;
- ekran Wykresy: wykres godzinny, dzienny i tygodniowy z 8 seriami (więcej
  panel nie pozwala wybrać), pełna historia centrali, wykres z przerwą,
  napisy zamiast wykresu, kod 503, strona HTML, odpowiedź urwana, centrala,
  która zamilkła, wolne łącze, limit 20 s;
- wykres w 80 różnych chwilach względem odpytań `/api/status` (5 szybkości
  łącza × 16 przesunięć) — szukany jest najgorszy zbieg;
- alarm w trakcie pobierania; starsza centrala z całym CSV (8000 wierszy);
  30 i 300 wykresów z rzędu;
- od v1.4.0: aktualizacja panelu — cały plik 1,2 MB aż do restartu oraz
  aktualizacja przerwana w połowie (wycofana oferta), z pomiarem tego, co po
  niej zostaje.

Całość biegnie dwa razy: druga runda pokazuje, czy powtórzenie tych samych
czynności coś zostawia. Na koniec doba pracy bez dotyku i 300 wykresów
z rzędu. Wynik dla v1.4.0 (2026-11-12):

```
aktualizacja panelu przerwana w polowie                     16    11 404       25 782
aktualizacja panelu: caly plik 1,2 MB                  restart    11 484       25 702
...
najwiecej naraz: 25 496 B (centrala po restarcie: napis zamiast wykresu)
na plytce zostaje wtedy ok. 11 690 B wolnego RAM-u; bez otwartych Wykresow najmniej ok. 25 702 B (aktualizacja panelu: caly plik 1,2 MB)
po przerwanej aktualizacji zostaje 16 B ponad stan sprzed niej (bufor zapisu i nakladka z paskiem postepu sa zwalniane)
po pierwszym przejsciu wszystkich przypadkow zostaje 1 248 B (tworzone raz), po drugim przybywa 0 B
```

Kolumny dwóch pierwszych linii: ile zostaje po przypadku, ile naraz, ile
wolnego RAM-u na płytce (szacunek). Dla v1.3.0 było: najwięcej naraz
25 416 B, na płytce ok. 12 748 B; bez otwartych Wykresów najmniej ok.
39 032 B (od v1.4.0 najcięższym przypadkiem poza Wykresami jest
aktualizacja).

Pojedyncze liczby różnią się między przebiegami o kilkadziesiąt bajtów.
Udawana centrala działa w osobnym wątku i raz na kilka przebiegów któreś
odpytanie nie zdąży; panel pokazuje wtedy w nagłówku powód, a ten napis
zostaje w pamięci (40 B, jednorazowo).

Kolumna „na plytce” to **szacunek**: wolny RAM płytki po starcie minus
„naraz” minus pamięć sieci, której model nie liczy. Stałe są w nagłówku
skryptu, z opisem, skąd pochodzą:

- `RAM_PO_STARCIE_V122_B=49676` — z logu płytki z v1.2.2; dla innych źródeł
  skrypt odejmuje to, o ile więcej trzymają po starcie i w zmiennych
  statycznych (v1.3.0: 512 B; v1.4.0: 1 490 B);
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
30 000 B   wszystko dziala, zadnej odmowy pamieci
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
plytka: 48 186 B po starcie - 11 000 B sieci = 37 186 B dla firmware'u, czyli 12 286 B ponad prog
```

(Wynik dla v1.4.0. v1.3.0: te same progi; płytka 49 164 B po starcie, czyli
13 264 B ponad próg.)

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

## Pomiar obciążenia w spoczynku

```
./pomiar_obciazenia.sh              # stan firmware'u z ../src
./pomiar_obciazenia.sh --prototyp   # + szacunek po zmianie „ustawiaj tylko to, co się zmieniło”
SRC=/ścieżka/do/src ./pomiar_obciazenia.sh   # inne źródła, np. starsza wersja (do porównania przed/po)
```

Scenariusz `pomiar_obciazenia` (w zwykłym `./uruchom_testy.sh` pomijany, jak
wszystkie `pomiar_*`) uruchamia panel **na ładowarce** (od v1.5.0: na samej
baterii panel po minucie przygasza ekran, a po pięciu usypia — stany A–F
mierzą jasny ekran), ogniwo 3,90 V, nie dotyka ekranu i przez ok. 11 minut
liczy, a na końcu wyjmuje ładowarkę, dotyka raz i podaje, po ilu sekundach
ekran przygasł (stan G/H: pobór przy 5 %) i kiedy panel usnął:

- **piksele oddane do ekranu** — atrapa `TFT_eSPI.h` liczy to, co LVGL
  przekazuje do `pushColors()`. Ekran ma 153 600 pikseli; jeden piksel to
  3 bajty po SPI (ILI9488 w TFT_eSPI 2.5.43 pracuje w kolorze 18-bitowym);
- **połączenia z centralą** — zapytania, które dotarły do udawanej centrali;
- **prośby o pamięć** — wywołania `malloc()`, `calloc()` i `realloc()`
  z firmware'u, LVGL i ArduinoJson (bez napisów `String`: atrapa `String`
  to `std::string`);
- **obroty pętli** `loop()`;
- **zmiany jasności podświetlenia** i to, czy panel usnął
  (`esp_deep_sleep_start()` w atrapie kończy program kodem 42).

Wynik z 2026-11-19, firmware **v1.4.3** (LVGL 8.4.0, ArduinoJson 7.4.3;
ustawienia w tle co 60 s, filtr pól JSON — „krok 5” po audycie):

| Stan | Ekranów na minutę | Pikseli na jeden odczyt statusu | Połączeń na minutę | Próśb o pamięć na minutę |
|---|---|---|---|---|
| A pulpit, dane bez zmian, pompa stoi | 0,01 | 30 (0 %) | **31** | **2 760** (312 kB) |
| B pulpit, dane bez zmian, pompa pracuje | 6,00 | 30 720 (20 %) | 31 | 5 156 |
| C pulpit, jedna wartość zmienia się co 10 s | 0,19 | 974 (1 %) | 31 | 2 811 |
| D centrala milczy, kafelki pokazują „--” | 2,45 | 14 266 (9 %) | 28,8 | 319 |
| E otwarty ekran POMPA | 8,86 | 45 359 (30 %) | 31,2 | 5 237 |
| F pulpit po powrocie, dane bez zmian | 0,00 | 0 | 31 | 2 756 |

Połączenia: 30 odczytów statusu + 1 odczyt ustawień na minutę (ekran POMPA
to nie ekran ustawień, więc E też ma 31). Próśb o pamięć o połowę mniej niż
w v1.4.1 — to filtr ArduinoJson (pomijane pola nie są kopiowane) i 11
odczytów ustawień mniej na minutę. Reszta (ok. 2 700 na minutę w A) to
rozpakowywanie 46 pól statusu 30 razy na minutę plus LVGL.
`pomiar_pamieci.sh` dla tej wersji: po starcie 131 680 B (v1.4.2: 129 324 B —
filtr statusu, ok. 2,4 kB, budowany raz), jedno odpytanie chwilowo
2 664 B (v1.4.2: 5 324 B), zmienne statyczne 6 646 B (v1.4.2: 6 566 B;
v1.4.0: 5 366 B — kolejka poleceń z v1.4.2), przyrost po 10 min 0 B.

**v1.4.5** (2026-11-20, polskie litery — punkt D): nowy scenariusz
`polskie_litery` (32 sprawdzenia: czcionki `lv_font_pl_14/20/28` mają 18 polskich
liter, ° i •; każdy znak każdej etykiety na każdym ekranie ma glif w swojej
czcionce; napisy składane w trakcie pracy też; żadnego napisu bez ogonków;
zrzuty pulpitu, paska błędu, POMPA, SERWO, drugiego pulpitu, WYŚWIETLACZ).
Oczekiwania scenariuszy, które porównują napisy, dostały polskie litery
(„CIŚNIENIE”, „POKAŻ WYKRES”, „BRAK PAMIĘCI”, „Spróbuj ponownie”…).
`uruchom_testy.sh` i `pomiar_pamieci.sh` kompilują dodatkowo
`src/czcionki/lv_font_pl_*.c`. `pomiar_obciazenia.sh`: połączenia i prośby o
pamięć jak w v1.4.4 (E: 5 238 zamiast 5 237 — dłuższy napis „WŁĄCZ”), pikseli
o 2–3 % więcej w A i C (etykiety o 1–2 px wyższe przez nową wysokość linii
czcionek). `pomiar_pamieci.sh`: po starcie 131 736 B (v1.4.4: 131 680 B;
+56 B to dłuższe napisy UTF-8 w etykietach), reszta bez zmian. Czcionki same
w sobie to tablice `const` we flashu — zero sterty.

**v1.4.4** (2026-11-20, porządki w kodzie — „krok 7”): `pomiar_obciazenia.sh`
daje **te same liczby co v1.4.3** we wszystkich sześciu stanach (31,0 / 31,0 /
31,0 / 28,8 / 31,2 / 31,0 połączeń; 2 760 / 5 156 / 2 811 / 319 / 5 237 / 2 756
próśb o pamięć); `pomiar_pamieci.sh`: po starcie 131 680 B i odpytanie
2 664 B bez zmian, zmienne statyczne **6 198 B** (v1.4.3: 6 646 B — znikły
tablice przycisków i sprite kafelka starego pulpitu TFT). Od tej wersji
`uruchom_testy.sh` kompiluje pliki firmware'u z `-Wall -Wextra` (do v1.4.3:
`-w`) — oczekiwany wynik to zero ostrzeżeń; biblioteki (LVGL, ArduinoJson)
są włączane przez `-isystem`, więc ich ostrzeżenia nie są liczone.

Wynik z 2026-11-18, firmware **v1.4.1** (LVGL 8.4.0, ArduinoJson 7.4.3;
pulpit ustawia napis, kolor, ikonę i widoczność tylko przy zmianie —
„krok 2” po audycie):

| Stan | Ekranów na minutę | Pikseli na jeden odczyt statusu | Połączeń na minutę | Próśb o pamięć na minutę |
|---|---|---|---|---|
| A pulpit, dane bez zmian, pompa stoi | 0,01 | 30 (0 %) | 42 | 5 506 |
| B pulpit, dane bez zmian, pompa pracuje | 6,00 (sam wirnik pompy) | 30 720 (20 %) | 42 | 7 902 |
| C pulpit, jedna wartość zmienia się co 10 s | 0,19 | 974 (1 %) | 42 | 5 557 |
| D centrala milczy, kafelki pokazują „--” | 2,45 (licznik sekund w nagłówku) | 14 266 (9 %) | 36 | 331 |
| E otwarty ekran POMPA | 8,86 (animacje ikon) | 45 359 (30 %) | 42 | 7 972 |
| F pulpit po powrocie, dane bez zmian | 0,00 | 0 | 42 | 5 502 |

To, co zostało, to animacje (wirnik pompy w B, ikony na ekranie POMPA w E),
licznik sekund „BRAK DANYCH … s” w nagłówku (D) i prawdziwe zmiany wartości
(C). Liczby zgadzają się z szacunkiem `--prototyp` dla v1.4.0 (niżej).
Prośby o pamięć (ok. 5 500 na minutę; to ArduinoJson przy każdym odczycie
statusu i ustawień) zostały na krok 5 — wynik wyżej (v1.4.3).

Ten sam pomiar dla **v1.4.0** (LVGL 8.3.11, ArduinoJson 7.2.1; v1.4.0 z LVGL
8.4.0 i ArduinoJson 7.4.3 dawała te same liczby) — tak było przed krokiem 2:

| Stan | Ekranów na minutę | Pikseli na jeden odczyt statusu | Połączeń na minutę | Próśb o pamięć na minutę |
|---|---|---|---|---|
| A pulpit, dane bez zmian, pompa stoi | 18,30 | 93 692 (61 % ekranu) | 42 | 6 806 |
| B pulpit, dane bez zmian, pompa pracuje | 24,30 | 124 412 (81 %) | 42 | 9 206 |
| C pulpit, jedna wartość zmienia się co 10 s | 18,30 | 93 692 (61 %) | 42 | 6 806 |
| D centrala milczy, kafelki pokazują „--” | 2,48 | — | 36 | 348 |
| E otwarty ekran POMPA | 24,39 | 124 858 (81 %) | 42 | 9 139 |

Na baterii, 673 s bez dotyku: panel nie usnął, podświetlenie 255/255, zero
zmian jasności.

To samo dla v1.4.0 z `--prototyp` (firmware v1.4.0 bez zmian, cztery funkcje
LVGL podmienione przy linkowaniu na wersje, które porównują przed
ustawieniem) — szacunek, który krok 2 w v1.4.1 potwierdził co do setnych:

| Stan | Ekranów na minutę | Próśb o pamięć na minutę |
|---|---|---|
| A pulpit, dane bez zmian, pompa stoi | 0,01 | 5 806 |
| B pulpit, dane bez zmian, pompa pracuje | 6,00 (sam wirnik pompy) | 8 202 |
| C pulpit, jedna wartość zmienia się co 10 s | 0,19 | 5 857 |
| D centrala milczy | 2,45 | 331 |
| E otwarty ekran POMPA | 8,86 | 8 272 |

Od włączenia panelu do końca pomiaru `lv_label_set_text()` zostało wywołane
4 439 razy, z czego 4 145 razy z napisem, który już był na ekranie; dla
`lv_obj_set_local_style_prop()` (kolory, ramki) to 8 449 z 11 723. Liczby
obejmują też jednorazowe budowanie ekranów przy starcie.

**Co te liczby znaczą na płytce.** LVGL i `lv_conf.h` są prawdziwe, a to, co
LVGL przerysowuje, zależy tylko od danych i od zegara. W stanach bez
animacji (A, C, D) liczba pikseli jest więc taka sama jak na płytce;
w stanach z animacją (B, E) to górna granica — płytka, która nie nadąża,
narysuje mniej klatek. Liczba połączeń jest taka sama, dopóki centrala
odpowiada na czas. Liczba obrotów pętli to górna granica: tutaj rysowanie
i wysyłka po SPI nie zajmują czasu, na płytce zajmują.

**Czego tu nie ma:** prądu, czasu rysowania, pracy radia i procesora. Pomiar
mówi, ile pracy panel sobie zadaje, a nie ile to kosztuje baterii — to trzeba
zmierzyć na płytce.

## Sprawdzenie prawdziwymi nagłówkami ESP32

`./sprawdz_naglowki_esp32.sh` kompiluje `main.cpp`, `dashboard_lvgl_poc.cpp`
i `lvgl_port.cpp` w trybie „tylko sprawdź” (`-fsyntax-only`) z prawdziwymi
nagłówkami: rdzeń i biblioteki arduino-esp32 (3.0.7; `RDZEN=3.3.8` dla
nowszego), ESP-IDF 5.1.4 z konfiguracją płytki `esp32`, newlib, TFT_eSPI
2.5.43, LVGL 8.4.0, ArduinoJson 7.4.3, flagi `-D` z `platformio.ini`.
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
mkdir -p /tmp/stary && unzip -q ~/Pobrane/"LCD Centrali.zip" -d /tmp/stary  # v1.1.0, archiwum z Dysku Google
SRC=/tmp/stary/src OBSERWACJE=1 ./uruchom_testy.sh
```

Scenariusze `ota_*` na źródłach starszych niż v1.4.0 nie są uruchamiane:
każdy kończy się jedną linią `NIE` z wyjaśnieniem (symulator poznaje wersję
po `DIAG_MA_OTA` w `diagnostyka_panelu.h`). Sprawdzone 2026-11-12 na
źródłach v1.3.0: całość się buduje i przechodzi w trybie obserwacji.

Scenariusze `wykresy_*` na źródłach do v1.2.2 pokazują same `NIE` — tamte
wersje pobierały dane wykresów przez `HTTPClient`, którego atrapa nie łączy
się z udawaną centralą (patrz „Czego to NIE sprawdza”). Wyjątkiem jest
`wykresy_stary_csv`, który podaje CSV ekranowi wprost; uruchomiony tak na
źródłach v1.2.2 zapisuje punkty, które ona narysowała (`wzorce_v122/CZYTAJ.txt`).
