# Panel LCD — wyniki szukania w internecie

Data: 2026-11-18. Ciąg dalszy pliku
`AUDYT_PANELU_LCD_2026-11-18_WYCIEKI_DRENAZ_KOD.md`: tematy z jego
rozdziału 10, po kolei, z tymi samymi numerami. Programu panelu nie
zmieniałem.

Przy każdej odpowiedzi jest napisane, skąd pochodzi:

| Oznaczenie | Co znaczy |
|---|---|
| **źródła** | przeczytałem kod biblioteki w konkretnej wersji — najpewniejsze |
| **dokumentacja** | strona producenta (Espressif, LVGL, PlatformIO, GitHub, ArduinoJson) |
| **pomiar** | uruchomiłem model panelu na PC |
| **inne** | forum, blog, zgłoszenie błędu — najmniej pewne |

---

## 1. Najkrócej

1. **Usypianie można przywrócić.** Napisałeś, że alarm gra z głośnika
   centrali. Kod centrali to potwierdza: ma własny brzęczyk i steruje nim
   bez udziału panelu. Śpiący panel alarmu więc nie wyciszy. Według liczb
   Espressif żadna inna poprawka nie da na baterii tyle, co sen (temat 10).
2. **Zbędnego rysowania nie naprawi nowsza biblioteka.** LVGL 8.4.0 i 9.6.0
   też rysują od nowa napis, który się nie zmienił. Poprawka musi być
   w programie panelu (temat 15).
3. **Wersje bibliotek trzeba przypiąć.** Zapis z `^` w `platformio.ini`
   bierze przy świeżej instalacji LVGL 8.4.0 i ArduinoJson 7.4.3, a testy na
   PC domyślnie używają 8.3.11 i 7.2.1. ArduinoJson 7.4.3 ma poprawkę błędu
   pamięci z 2026-03-02 (tematy 5 i 19).
4. **Po losowym restarcie da się odczytać, co się stało.** Biblioteki
   Arduino same zapisują zrzut awarii do pamięci flash. Panel może go
   wypisać w logu po następnym starcie — dziś tego nie robi (temat 8).
5. **Pięciu rzeczy nie warto ruszać:** szybszej wysyłki do ekranu, połączeń
   „podtrzymywanych”, automatycznego lekkiego snu, osobnej puli pamięci dla
   LVGL i stałych buforów WiFi. Powody są przy tematach 16, 17, 12, 6 i 9.
6. **Polskie litery na ekranie da się zrobić** skryptem, który jest w LVGL
   (temat 23).
7. **Hasła (tematy 1–4)** — wyniki są w prywatnym pliku na Twoim Dysku
   Google, nie tutaj.

Zmieniona kolejność prac jest w rozdziale 4.

---

## 2. Twoja odpowiedź o alarmie — co sprawdziłem

Napisałeś: alarm jest pociągnięty do głośnika z centrali.

W kodzie centrali (wersja 3.58.6 z tej gałęzi,
`firmware-patch/v3.58.6-panel-ota/src/main_centrala.cpp`):

- brzęczyk jest na wyjściu 12 (`pinBuzer`, linia 23161),
- steruje nim funkcja `buzzerAlarmuCentrali()` (linia 34713),
- gra, gdy trwa alarm dymu albo przegrzania i nie jest wyciszony,
- panel nie bierze w tym udziału.

Wniosek: uśpiony panel nie wyłącza dźwięku alarmu. Tracisz wtedy tylko
brzęczyk samego panelu i ekran alarmu na panelu. Zakładam, że w wersji,
która jest w piecu, ten fragment jest taki sam.

**Co proponuję.** Przywrócić to, co panel robił przed przejściem na nowy
ekran — ten kod jest w pliku, tylko nikt go nie woła (`main.cpp:4392–4576`,
`5538–5539`):

| Co | Jak było i jak proponuję |
|---|---|
| Kiedy zasypia | na baterii, po 5 minutach bez dotyku |
| Kiedy nie zasypia | gdy podłączone jest ładowanie USB |
| Jak się budzi | przyciskiem na wyjściu 14 |
| Co robi przed snem | gasi podświetlenie, usypia sterownik ekranu, trzyma pin podświetlenia w stanie niskim |
| Ile bierze we śnie | ok. 0,11 mA — pomiar zapisany w kodzie (`main.cpp:4539–4540`) |

Trzy rzeczy bym dodał:

- nie zasypiać, gdy trwa alarm (ekran alarmu zostaje widoczny),
- nie zasypiać w trakcie aktualizacji panelu,
- przy baterii poniżej progu zasnąć od razu (temat 14).

Tego jeszcze nie zrobiłem. To krok 3 w rozdziale 4.

---

## 3. Tematy po kolei

### W — hasła (tematy 1–4)

Wyniki są w pliku `AUDYT_PANELU_LCD_2026-11-18_HASLA_PRYWATNE.md` na Twoim
Dysku Google.

### A — pamięć

#### 5. ArduinoJson 7 i pamięć

**Co znalazłem.**

- Od wersji 7.0.0 przy rozpakowywaniu biblioteka zawsze kopiuje napisy do
  własnej pamięci na stercie. Tryb „bez kopiowania” został usunięty. Autor
  pisze, że stertę rozdrabnia właściwie tylko rozpakowywanie
  (`deserializeJson()`).
  **dokumentacja**:
  <https://arduinojson.org/v7/revisions/>,
  <https://arduinojson.org/news/2024/01/03/arduinojson-7/>
- Jest filtr: podaje się listę pól, które mają zostać, reszta jest pomijana
  już w trakcie czytania. **dokumentacja**:
  <https://arduinojson.org/v7/how-to/deserialize-a-very-large-document/>
- Można podać własny sposób przydzielania pamięci (klasa `Allocator`,
  konstruktor `JsonDocument(Allocator*)`). **źródła**: ArduinoJson v7.4.3,
  `src/ArduinoJson/Memory/Allocator.hpp:13–17`,
  `src/ArduinoJson/Document/JsonDocument.hpp:24`.
- **Wersje 7.4.3, 7.3.2 i 7.2.2 z 2026-03-02 naprawiają wyjście poza bufor**
  przy zamianie napisu z bardzo długą liczbą na liczbę (zgłoszenie #2220).
  Wersja 7.2.1, której używają testy na PC, tej poprawki nie ma.
  **dokumentacja**: <https://arduinojson.org/v7/revisions/>
- Wersja 7.4.0 oszczędza pamięć przy napisach do 3 znaków.

**Pomiar** (model na PC, stan A — pulpit, dane bez zmian):

| Biblioteki | Próśb o pamięć na minutę |
|---|---|
| LVGL 8.3.11 + ArduinoJson 7.2.1 | 6 806 |
| LVGL 8.4.0 + ArduinoJson 7.4.3 | 6 506 |

Różnica (300 na minutę) to najpewniej ArduinoJson — liczby rysowania są
w obu wierszach identyczne (temat 15).

**Co z tego wynika.**

- Przypiąć ArduinoJson 7.4.3 (krok 1).
- Ustalenie A2 zostaje: filtr w panelu zrobi to bez ruszania centrali.
  Panel czyta 44 z 79 pól.
- Własny przydział pamięci dla JSON-a jest możliwy, ale na razie
  niepotrzebny.

#### 6. LVGL: własna pula pamięci czy wspólna

**Co znalazłem.**

- Przy obecnym ustawieniu (`LV_MEM_CUSTOM 1`) funkcja `lv_mem_monitor()`
  zwraca same zera. Liczy cokolwiek tylko przy własnej puli LVGL.
  **źródła**: LVGL v8.3.11, `src/misc/lv_mem.c`, funkcja `lv_mem_monitor`.
- Fragmentację wspólnej sterty pokazują funkcje systemu:
  największy wolny blok w porównaniu z całą wolną pamięcią.
  **dokumentacja**:
  <https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/system/heap_debug.html>

**Co z tego wynika.** Zostać przy wspólnej stercie. Osobna pula musiałaby
być stale zarezerwowana na największe chwilowe zużycie (wykresy), a wolnej
pamięci jest za mało, żeby ją tak blokować. Panel już loguje wolną pamięć,
najmniejszą od startu i największy blok — to wystarcza.

#### 7. Jak szukać wycieku na płytce

**Co znalazłem.**

- System może wołać Twoją funkcję przy **każdej nieudanej prośbie o pamięć**
  (`heap_caps_register_failed_alloc_callback`). Dostaje rozmiar i nazwę
  funkcji, która prosiła. Nie wymaga żadnych ustawień budowania.
  **dokumentacja**: adres jak w temacie 6.
- Śledzenie każdej alokacji (`heap tracing`) jest w bibliotekach Arduino
  wyłączone (`CONFIG_HEAP_TRACING_OFF=y`). **źródła**: `sdkconfig` bibliotek
  Arduino dla ESP32 (gałąź IDF 5.1) i `sdkconfig.defaults` Twojej centrali
  (ESP-IDF 5.5.4).
- Włączone jest za to lekkie wykrywanie uszkodzeń sterty
  (`CONFIG_HEAP_POISONING_LIGHT=y`). **źródła**: te same pliki.
- Zapas stosu pętli: `uxTaskGetStackHighWaterMark(NULL)`. Rozmiar stosu
  zmienia się jedną linią `SET_LOOP_TASK_STACK_SIZE(16 * 1024);`.
  **źródła**: przykład `ArduinoStackSize.ino` w arduino-esp32,
  <https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/ArduinoStackSize/ArduinoStackSize.ino>

**Co z tego wynika** (krok 1): dopisać do logu zapas stosu i licznik
nieudanych próśb o pamięć z rozmiarem ostatniej. Funkcja wołana przy
nieudanej prośbie ma tylko zapisać liczby; wypisze je pętla.

#### 8. Zrzut po awarii

**Co znalazłem.**

- Biblioteki Arduino zapisują zrzut awarii do flash, w formacie ELF, ze
  sprawdzeniem przy starcie. **źródła**: `sdkconfig` jak w temacie 7
  (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`,
  `CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y`).
- Panel ma na to miejsce: partycja `coredump`, 64 kB (`partitions.csv`).
- Przy takich ustawieniach program może po starcie sam odczytać skrót
  zrzutu: nazwę zadania, adres błędu i ślad wywołań
  (`esp_core_dump_image_check()`, `esp_core_dump_get_summary()`,
  `esp_core_dump_get_panic_reason()`). **źródła**: ESP-IDF `release/v5.5`,
  `components/espcoredump/include/esp_core_dump.h:21–37`, `104–172`.
- Cały zrzut czyta się z komputera kablem:
  `esp-coredump -p PORT info_corefile .pio/build/esp32dev/firmware.elf`.
  **inne**: <https://hpsaturn.com/espcoredump-platformio-arduino/>;
  **dokumentacja** samego mechanizmu:
  <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/core_dump.html>
- Do odczytu potrzebny jest plik `firmware.elf` z **tego samego** budowania,
  które jest na płytce.

**Co z tego wynika** (krok 1): po starcie panel sprawdza, czy jest zrzut,
wypisuje w logu jedną linię ze skrótem i kasuje zrzut. Wtedy po losowym
restarcie wystarczy log.

**Nie sprawdzone:** czy zrzut mieści się w 64 kB. Do sprawdzenia na płytce
przez celowe wywołanie błędu.

#### 9. Bufory WiFi a sterta

**Co znalazłem.** Domyślnie bufory WiFi są przydzielane w miarę potrzeb.
Dokumentacja zaleca to ustawienie, gdy liczy się pamięć; stałe bufory są
trochę szybsze. **dokumentacja**:
<https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html>
(`useStaticBuffers`).

**Co z tego wynika.** Nic nie zmieniać. Stałe bufory zabrałyby na stałe
kilka kilobajtów.

### B — drenaż

#### 10. Ile bierze ESP32

**Co znalazłem.** Liczby producenta dla samego układu ESP32, połączonego
z WiFi i bezczynnego (procesor 160 MHz):

| Tryb | Średni prąd |
|---|---|
| Radio usypiane między ramkami — tak pracuje panel | ok. 29–31 mA |
| To samo z automatycznym zwalnianiem procesora | ok. 20–23 mA |
| Automatyczny lekki sen | ok. 2–3 mA |
| Głęboki sen | 0,005 mA |

**dokumentacja**:
<https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/low-power-mode/low-power-mode-wifi.html>

Chwilowo: odbiór 95–100 mA, nadawanie 180–240 mA. **dokumentacja**: karta
katalogowa ESP32, tabela „RF Power-Consumption Specifications” (kopia:
<https://cdn.sparkfun.com/assets/e/b/6/b/0/esp32_datasheet_en-1223853.pdf>).
To potwierdza liczbę odbioru, która w audycie była oznaczona „z pamięci”.

Typowy moduł ekranu 3,5″ z ILI9488 (MSP3520) bierze według instrukcji
ok. 90 mA i ma własny układ sterowania podświetleniem. **inne**: instrukcja
LCDwiki, <https://manuals.plus/elecrow/msp3520-3-5inch-spi-module-manual>.
Nie wiem, czy masz dokładnie ten moduł.

**Co z tego wynika.**

- Panel włączony: mniej więcej 120–160 mA (układ plus ekran). To szacunek,
  nie pomiar.
- Panel w głębokim śnie: ok. 0,11 mA według Twojego wcześniejszego pomiaru.
- Przykład dla ogniwa 2000 mAh (nie znam Twojego): 130 mA to ok. 15 godzin,
  0,11 mA to ponad rok.
- Dlatego sen jest ważniejszy niż wszystkie inne oszczędności razem.
- Poprawka do audytu (B5): jeśli moduł ma własny układ podświetlenia, to
  podświetlenie jest dużą częścią prądu ekranu i przygaszanie ma sens.
  Rozstrzygnie pomiar z rozdziału 12 audytu.

#### 11. Wolniejszy procesor przy włączonym WiFi

**Co znalazłem.** Dozwolone są 240, 160 i 80 MHz. **źródła**: arduino-esp32
3.3.8, `cores/esp32/esp32-hal-cpu.h:102–106`. Poniżej 80 MHz WiFi nie
działa. **inne**: forum Espressif, <https://www.esp32.com/viewtopic.php?t=1299>

**Co z tego wynika.** 80 MHz można spróbować, ale dopiero po poprawce
rysowania (krok 2) i z miernikiem. Przy głębokim śnie na baterii zysk jest
mały.

#### 12. Automatyczny lekki sen w projekcie Arduino

**Co znalazłem.**

- Wymaga opcji `CONFIG_PM_ENABLE` i „tickless idle”. W gotowych bibliotekach
  Arduino obie są wyłączone, a samo dopisanie flag nic nie daje, bo
  biblioteki są już zbudowane. **inne**:
  <https://community.platformio.org/t/esp32-auto-light-sleep-arduino-idf/13676>
- pioarduino umie przebudować biblioteki z własnymi opcjami
  (`custom_sdkconfig`). W tym mechanizmie był błąd: po przebudowie nie
  kopiował jednego z plików potrzebnych do złożenia programu. Zgłoszenie
  #464 zamknięto 2026-04-13. **źródła**:
  <https://github.com/pioarduino/platform-espressif32/issues/464>.
  Jeden z projektów trafił na ten błąd właśnie przy włączaniu tych dwóch
  opcji. **inne**: <https://github.com/prokrypt/CrossInk/pull/13>

**Co z tego wynika.** Nie ruszać. Dużo ryzyka, a głęboki sen na baterii
daje więcej.

#### 13. Głęboki sen z ekranem i dotykiem

**Co znalazłem.**

- Zwykłe wyjście (takie jak 21 od podświetlenia) trzyma stan w głębokim śnie
  tylko wtedy, gdy wywołane są obie funkcje: `gpio_hold_en()`
  i `gpio_deep_sleep_hold_en()`. **dokumentacja**:
  <https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-reference/peripherals/gpio.html>.
  Stary kod usypiania robi obie (`main.cpp:4557–4558`).
- Układ dotyku XPT2046 sam przechodzi w oszczędzanie po każdym odczycie
  i bierze wtedy do 3 µA. **źródła**: TFT_eSPI V2.5.43,
  `Extensions/Touch.cpp:66–111` (bajty sterujące `0xD0`, `0x90`, `0xB0`,
  `0xC0`); **dokumentacja**: karta katalogowa XPT2046,
  <https://grobotronics.com/images/datasheets/xpt2046-datasheet.pdf>
- W tym trybie XPT2046 daje sygnał dotknięcia na wyjściu `T_IRQ`. Gdyby je
  podłączyć do ESP32, panel mógłby się budzić dotknięciem ekranu. To zmiana
  w kablach, nie w programie — tylko jako możliwość.

**Co z tego wynika.** Stary kod usypiania robi to, co trzeba. Wystarczy go
wpiąć w nową pętlę (krok 3).

#### 14. Bateria

**Co znalazłem.**

- Zabezpieczenie na typowej płytce ładowarki TP4056 (układ DW01A) odcina
  ogniwo dopiero przy 2,4 V i puszcza przy 3,0 V. **inne**:
  <https://www.best-microcontroller-projects.com/dw01a.html>,
  <https://mischianti.org/tp4056-lipo-battery-charger-high-resolution-pinout-datasheet-and-specs/>
- ESP32 sam się restartuje, gdy napięcie zasilania spadnie do ok. 2,43 V
  (najniższy z ośmiu progów; ten jest ustawiony w bibliotekach Arduino dla
  ESP32). **źródła**: `sdkconfig` bibliotek Arduino (gałąź IDF 5.1),
  `CONFIG_ESP_BROWNOUT_DET_LVL_SEL_0=y`; **inne** (lista progów):
  <https://stackoverflow.com/questions/75819429/saving-some-data-in-power-failure-using-brown-out-interrupt>

**Co z tego wynika** (krok 3).

- Sprzęt nie ochroni ogniwa przed zbyt głębokim rozładowaniem — 2,4 V to za
  nisko. Potrzebny jest próg w programie, np. 3,3 V: poniżej panel zasypia
  i nie budzi się sam.
- Po restarcie z powodu spadku napięcia (`ESP_RST_BROWNOUT`) na baterii
  panel powinien od razu zasnąć, zamiast startować WiFi. To przerywa pętlę
  restartów.

#### 15. LVGL bez zbędnego rysowania

**Co znalazłem.**

- `lv_label_set_text()` i `lv_obj_set_local_style_prop()` nie porównują
  nowej wartości ze starą także w LVGL 8.4.0 i w najnowszym 9.6.0.
  **źródła**: LVGL v8.4.0 `src/widgets/lv_label.c`, `src/core/lv_obj_style.c`
  (obie funkcje identyczne jak w 8.3.11); v9.6.0
  `src/widgets/label/lv_label.c` (`set_text_internal`),
  `src/core/lv_obj_style.c`.
- Dokumentacja LVGL podaje gotowy wzór usypiania: czas bez dotyku to
  `lv_disp_get_inactive_time(NULL)`, a `lv_anim_count_running()` mówi, czy
  animacje się skończyły. **dokumentacja**:
  <https://docs.lvgl.io/8.3/porting/sleep.html>
- `lv_timer_handler()` zwraca, za ile milisekund trzeba je wywołać znowu.
  Pętla może tyle poczekać, zamiast kręcić się co 5 ms. **źródła**: LVGL
  v8.4.0, `src/misc/lv_timer.h:64–68`.

**Pomiar** powtórzony na LVGL 8.4.0 i ArduinoJson 7.4.3
(`LCD_TESTY_CACHE=… ./pomiar_obciazenia.sh --prototyp`):

| Stan | Ekranów na minutę dziś | Z prototypem „ustawiaj tylko zmiany” |
|---|---|---|
| A pulpit, dane bez zmian, pompa stoi | 18,30 | 0,01 |
| B pulpit, pompa pracuje | 24,30 | 6,00 |
| C jedna wartość zmienia się co 10 s | 18,30 | 0,19 |
| D centrala milczy | 2,48 | 2,45 |
| E otwarty ekran POMPA | 24,39 | 8,86 |

Liczby rysowania są takie same jak na LVGL 8.3.11. Na baterii, 673 s bez
dotyku: bez zmian — panel nie usnął, podświetlenie 255/255.

**Co z tego wynika** (krok 2): poprawka w panelu — ustawiać napis, kolor
i obrazek tylko wtedy, gdy się zmieniły. Przejście na LVGL 9 tego nie
załatwi.

#### 16. Szybsze wysyłanie do ILI9488

**Co znalazłem.**

- TFT_eSPI nie ma szybkiego trybu (DMA) dla ekranów z kolorem 18-bitowym po
  SPI, czyli dla ILI9488. **źródła**: TFT_eSPI V2.5.43,
  `Processors/TFT_eSPI_ESP32.h:120–121`.
- 27 MHz, które masz ustawione, to wartość z przykładu TFT_eSPI dla ILI9488.
  **źródła**: `User_Setups/Setup21_ILI9488.h:28`.
- Karta katalogowa ILI9488 ma podawać dla zapisu najkrótszy cykl zegara
  50 ns, czyli 20 MHz. **inne** — dwa opisy powołują się na stronę 332
  karty, np. <https://github.com/viktor-nikolov/ILI9488-Xilinx>; trzeci
  podaje 40 MHz. Samej karty nie czytałem.
- TFT_eSPI: ostatnie wydanie 2024-03-06, ale w repozytorium są jeszcze
  zmiany z 2026 roku (ostatnia 2026-04-03); 360 otwartych zgłoszeń
  i propozycji zmian.
  LovyanGFX jest rozwijane (wydanie 1.3.0 z 2026-11-06). **źródła**:
  GitHub, stan na 2026-11-18.

**Co z tego wynika.**

- Nie da się rozsądnie przyspieszyć wysyłki. Trzeba mniej rysować (krok 2).
- Nie podnosić `SPI_FREQUENCY`. Gdyby na ekranie pojawiały się śmieci,
  pierwsza rzecz do sprawdzenia to 20 MHz.
- Zostać przy TFT_eSPI 2.5.43 i przypiąć tę wersję. Zmiana biblioteki ekranu
  to duża robota bez pewnego zysku.

#### 17. Mniej połączeń

**Co znalazłem.**

- Serwer WWW centrali (ESPAsyncWebServer) do każdej odpowiedzi dodaje
  `Connection: close` i zamyka połączenie. Podtrzymywania połączeń nie ma.
  **źródła**: ESPAsyncWebServer v3.10.3 (najstarsza dopuszczona
  w `platformio.ini` centrali) i v3.12.1 (najnowsza),
  `src/WebResponses.cpp:270`, `:340`.
- U tego, kto zamyka pierwszy (tutaj: centrala), zamknięte połączenie
  zajmuje miejsce w tablicy jeszcze przez 2 minuty. Gdy tablica jest pełna,
  system usuwa najstarsze takie wpisy. **źródła**: esp-lwip,
  `src/core/tcp.c`, funkcja `tcp_alloc`. To potwierdza zdanie z audytu
  (B4), które było oznaczone „z pamięci”.

**Co z tego wynika.** Liczbę połączeń zmniejsza się tylko rzadszym pytaniem.
Najprościej: ustawienia pobierać wtedy, gdy są potrzebne (otwarty ekran
ustawień albo zmiana zgłoszona w statusie), a nie co 5 sekund. To 12 z 42
połączeń na minutę.

#### 18. Moc nadajnika i zapis danych WiFi

**Co znalazłem.**

- `WiFi.setTxPower()` działa dopiero po uruchomieniu WiFi; wcześniej zwraca
  błąd. **źródła**: arduino-esp32 3.3.8,
  `libraries/WiFi/src/WiFiGeneric.cpp`, funkcja `setTxPower`.
- `WiFi.persistent(false)` trzeba wywołać przed `WiFi.mode()`. **źródła**:
  ten sam plik, linie 293, 376, 479, 602.

**Co z tego wynika.** `WiFi.persistent(false)` — krok 1. Moc nadajnika —
z miernikiem, mały zysk (panel nadaje rzadko).

### C — kod

#### 19. Przypinanie wersji

**Co znalazłem.**

- `^8.3.11` znaczy „dowolna 8.x od 8.3.11 w górę”. Dokładna wersja to zapis
  bez `^`. **inne**:
  <https://community.platformio.org/t/effect-of-in-lib-deps/45562>
- W rejestrze PlatformIO jest LVGL 8.4.0 (2024-03-20), więc świeża
  instalacja weźmie 8.4.0, nie 8.3.11. **źródła**: rejestr PlatformIO,
  <https://api.registry.platformio.org/v3/packages/lvgl/library/lvgl>
- Najnowsze wersje na 2026-11-18: arduino-esp32 3.3.13 i pioarduino
  55.03.313 (oba z 2026-11-17); oficjalna platforma PlatformIO 8.0.0
  (2026-10-19). **źródła**: GitHub.
- Dokumentacja arduino-esp32 od wersji 3.3.12 opisuje wprost: dwa wyjścia
  PWM z tą samą częstotliwością i rozdzielczością dostają wspólny generator,
  a zmiana częstotliwości jednego zmienia oba. **źródła**: zmiana #12793,
  <https://github.com/espressif/arduino-esp32/pull/12793>
- W panelu podświetlenie ma 5000 Hz, a brzęczyk startuje z 3000 Hz
  (`main.cpp:5203`, `1305`). Są różne, więc na rdzeniu 3.2.1 i nowszych
  każde ma swój generator. To potwierdza ustalenie C2.

**Co z tego wynika** (krok 1). Propozycja zapisu w `platformio.ini` panelu:

```
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.38-1/platform-espressif32.zip
lib_deps =
    bodmer/TFT_eSPI @ 2.5.43
    bblanchon/ArduinoJson @ 7.4.3
    lvgl/lvgl @ 8.4.0
```

Platforma jest ta sama co w centrali. Testy na PC przestawię na te same
wersje. Do kodu dojdzie uwaga: nie ustawiać podświetleniu i brzęczykowi tej
samej częstotliwości.

**Nie wiem**, co masz dziś zainstalowane. Pokazuje to początek wydruku
z budowania panelu: linia `PLATFORM:` i lista bibliotek pod
`Dependency Graph`. Jeśli jest tam coś innego, przypniemy to, co jest.

#### 20. Watchdog w `setup()`

**Co znalazłem.** `esp_task_wdt_reset()` „karmi” watchdoga;
`esp_task_wdt_delete(NULL)` wypisuje bieżące zadanie spod nadzoru,
`esp_task_wdt_add(NULL)` zapisuje je z powrotem. **dokumentacja**:
<https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html>

**Co z tego wynika** (krok 1): w pętlach kalibracji dotyku wołać
`esp_task_wdt_reset()`.

#### 21. Kolejka poleceń

Tu nie było czego szukać — to decyzja projektowa. Proponuję (krok 4):

- dla każdego ustawienia panel pamięta jedną, ostatnią wartość do wysłania,
- wysyła ją, gdy poprzednie polecenie się skończy,
- przy przytrzymaniu „+” albo „−” wysyła wartość końcową, nie każdy krok,
- odświeżanie nie nadpisuje pola, które właśnie zmieniasz.

#### 22. Analiza kodu przy każdym budowaniu

**Co znalazłem.** PlatformIO ma osobną opcję na flagi tylko dla plików
projektu (`build_src_flags`) i polecenie `pio check` z ustawieniami
`check_tool`, `check_flags`, `check_severity`, `check_skip_packages`.
**dokumentacja**:
<https://docs.platformio.org/en/latest/projectconf/sections/env/index.html>

**Co z tego wynika.** `build_src_flags = -Wall -Wextra` pokaże ostrzeżenia
z kodu panelu bez zalewu z bibliotek. Dopiero po sprzątnięciu starego kodu
(krok 7) — dziś większość ostrzeżeń jest właśnie stamtąd.

### D — znaki

#### 23. Polskie litery na ekranie

**Co znalazłem.**

- LVGL ma skrypt do budowania czcionki takiej jak wbudowana (Montserrat
  razem z ikonami), w innym rozmiarze albo z innym zestawem znaków:
  `scripts/built_in_font/built_in_font_gen.py`. W tym samym katalogu są
  potrzebne pliki czcionek. **źródła**: LVGL v8.4.0;
  **dokumentacja**: <https://docs.lvgl.io/8.2/overview/font.html>
- Czcionkę zgłasza się w kodzie przez `LV_FONT_DECLARE(nazwa)`.

Polecenie dla rozmiaru 14 (tak samo dla 20 i 28):

```
python3 built_in_font_gen.py --size 14 --bpp 4 -o lv_font_montserrat_14_pl.c \
  -r "0x20-0x7F,0xB0,0x2022,0xD3,0xF3,0x104-0x107,0x118-0x119,0x141-0x144,0x15A-0x15B,0x179-0x17C"
```

**Co z tego wynika.** Jeśli chcesz polskie litery na ekranie, mogę te trzy
pliki wygenerować i podmienić czcionki. To osobny, mały krok. Nie
sprawdzałem jeszcze, o ile urośnie program.

---

## 4. Kolejność prac po szukaniu

Zastępuje tabelę z rozdziału 11 audytu. Każdy krok to osobna wersja panelu,
testy na PC i próba na płytce.

| Krok | Co | Ustalenia |
|---|---|---|
| 0 | Hasła | prywatny plik |
| 1 | **Małe i pewne:** przypięcie wersji; zrzut awarii, nieudane prośby o pamięć i zapas stosu w logu; watchdog w kalibracji; limit odpowiedzi; sprawdzenie `malloc`; zabezpieczenie tabeli ustawień; `WiFi.persistent(false)`; dolna granica jasności | C2, A5, C3, A4, A6, C5, B8, C9 |
| 2 | **Rysuj tylko zmiany.** Sprawdzian: `pomiar_obciazenia.sh`, stan A blisko zera | B2 |
| 3 | **Usypianie na baterii jak dawniej** + próg słabej baterii + przerwanie pętli restartów | B1, B5, B6 |
| 4 | **Polecenia nie giną** | C4 |
| 5 | Ustawienia pobierane wtedy, gdy potrzebne; filtr JSON | B4, A2 |
| 6 | 80 MHz i moc nadajnika — z miernikiem | B3, B7 |
| 7 | Usunięcie starego programu z `main.cpp`; ostrzeżenia kompilatora włączone | C1, C7, C8 |
| — | Polskie litery — jeśli chcesz | D |

Kroki 1 i 2 nie wymagają od Ciebie żadnej decyzji.

---

## 5. Czego nie udało się potwierdzić

- Jakie wersje platformy i bibliotek masz zainstalowane (temat 19).
- Czy Twój moduł ekranu to MSP3520 i ile bierze samo podświetlenie
  (temat 10). Rozstrzygnie pomiar.
- Największa dozwolona częstotliwość SPI dla ILI9488 — źródła się różnią,
  karty katalogowej nie czytałem (temat 16).
- Czy zrzut awarii mieści się w 64 kB (temat 8).
- Czy opcja `CONFIG_PM_ENABLE` jest wyłączona także dla zwykłego ESP32 na
  ESP-IDF 5.5. Sprawdziłem ESP32 na IDF 5.1 i ESP32-S3 na IDF 5.5.4.
- Czy LovyanGFX ma szybki tryb dla ILI9488 — nie sprawdzałem.
- Nic z tego pliku nie było uruchamiane na płytce. Prądu nie mierzyłem.

---

## Załącznik. Poprawki w arduino-esp32 po wersji 3.3.8

Z opisów wydań 3.3.9–3.3.13. Panel używa własnego kodu sieci na gniazdach
systemu, więc poprawki klas `HTTPClient` i `NetworkClient` go nie dotyczą.
Dotyczą centrali, która pobiera nimi aktualizacje.

| Wersja | Co | Kogo dotyczy |
|---|---|---|
| 3.3.9 | poprawki PWM (`ledc`); poprawki `NetworkClient`; awarie przy wielokrotnym włączaniu i wyłączaniu WiFi | panel (PWM), centrala |
| 3.3.11 | PWM: wartość równa albo większa od największej traktowana jako „stale włączone” | panel |
| 3.3.12 | dokumentacja wspólnych generatorów PWM; poprawka największego rozmiaru pliku w `HTTPUpdate`; poprawka ponownego użycia połączenia w `HTTPClient` | panel (opis), centrala |
| 3.3.13 | `HTTPClient` przerywa czytanie, gdy serwer zamilknie; aktualizacja poddaje się po 30 s ciszy | centrala |

Źródło: <https://github.com/espressif/arduino-esp32/releases>. Nie
sprawdzałem, czy któryś z tych błędów występuje u Ciebie. Centrala jest
poza zakresem tego audytu — zostawiam to jako notatkę.
