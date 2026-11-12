# Czy pamięci i RAM-u starczy — panel LCD i centrala

Data: 2026-11-12. Dotyczy: panel LCD **v1.3.0** (`LCD-Centrali/`) i centrala
**3.58.5** (`firmware-patch/v3.58.5-panel-wykresy-po-restarcie/`).
Firmware obu urządzeń się nie zmienia — to jest sprawdzenie.

Pytanie: „posprawdzaj, czy pamięci i RAM-u starczy w różnych przypadkach
w centrali i na LCD”.

## Wynik

RAM-u starcza w każdym sprawdzonym przypadku, na obu urządzeniach. Miejsca
w systemie plików centrali (flash na dane) nie umiem potwierdzić z samego
kodu — to osobna sprawa, bez związku z wykresami na LCD (zastrzeżenie 4).

| | Ile potrzeba | Ile jest | Zapas |
|---|---|---|---|
| **LCD, RAM** — najcięższy przypadek: wykres na ekranie w chwili odpytania centrali | 25,4 kB (firmware) + do 11 kB (sieć) | 49,2 kB po starcie | ok. 13 kB |
| **LCD, RAM** — bez otwartego ekranu Wykresy | ok. 6,6 kB (firmware) + do 3,5 kB (sieć) | 49,2 kB | ok. 39 kB |
| **Centrala, PSRAM** — historia i pozostałe bufory stałe | 1,5 MB, nie rośnie | 8 MB | do 6,9 MB (minus to, co biorą inne części firmware'u) |
| **Centrala, PSRAM** — jeden wykres dla panelu | 45 kB na chwilę | jw. | — |
| **Centrala, zwykły RAM** — jeden wykres dla panelu | ramka stosu ok. 1 kB z 16 kB; ze sterty tylko drobne obiekty serwera HTTP | nieznane (podaje log centrali) | — |
| **Centrala, flash na dane** — logi i pliki historii | logi do 8,28 MB; pliki historii do 2,23 MB przy włączonym zapisie | 10,35 MB | **nie potwierdzone** |
| **Flash na program** — LCD | nieznane (poda PlatformIO) | 2,03 MB | nieznany |
| **Flash na program** — centrala | 2,47 MB w wydaniu 3.57.9 (dopisane później, patrz zastrzeżenie 3) | 3,15 MB | ok. 0,67 MB |

Cztery zastrzeżenia:

1. **Liczby dla LCD w kolumnie „zapas” to szacunek.** v1.3.0 nie było
   uruchamiane na płytce. Punkt wyjścia pochodzi z logu płytki z v1.2.2
   (49,7 kB po starcie; v1.3.0 ma o 0,5 kB więcej zmiennych statycznych, stąd
   49,2 kB). Pamięć, którą firmware bierze w każdym przypadku, jest zmierzona
   na PC. Pamięć sieci przy pobieraniu wykresu jest policzona z ustawień
   lwIP, nie zmierzona.
2. **Wolnej pamięci centrali nie znam.** Wiem, ile bierze historia i ile
   bierze wykres dla panelu. Ile zostaje, podaje sama centrala w logu co
   5 minut (`tag=HEALTH-A`); takiego logu nie mam.
3. **Rozmiaru firmware'u we flashu nie znam** — plików nie da się tu
   skompilować dla ESP32. Wiem w przybliżeniu (z kompilacji na PC), o ile
   rosną względem poprzednich wersji: panel o ok. 8 kB względem v1.2.2,
   centrala o ok. 6 kB względem 3.58.3. Dokładną liczbę poda PlatformIO przy
   budowaniu (`Flash: [...] xx% (used ... bytes from ... bytes)`). Gdyby
   program się nie mieścił, budowanie kończy się błędem i nic nie jest
   wgrywane.
   *Dopisane przy sprawdzaniu aktualizacji panelu:* dla centrali jest liczba
   z GitHuba. Plik `firmware.bin` w wydaniu 3.57.9 ma 2 472 384 B, czyli 79 %
   partycji programu (3 145 728 B); zostaje 673 344 B. Rozmiaru 3.58.5 i obu
   wersji panelu nadal nie znam.
4. **System plików centrali: rachunek z limitów w kodzie się nie domyka.**
   Logi mogą zająć do 80 % pojemności, a bezpiecznik, który je przycina, nie
   liczy plików historii. To kod istniejący od dawna; wersje 3.58.4 i 3.58.5
   niczego do plików nie zapisują. Ile miejsca jest zajęte naprawdę, pokazuje
   adres `/api/fs-list` centrali. Szczegóły: rozdział „System plików
   centrali”.

## Skąd są liczby

| Źródło | Co z niego pochodzi |
|---|---|
| **Płytka** — log panelu z v1.2.2, 2026-11-12, ok. 40 s | wolny RAM po starcie 49 676 B, największy blok 42 996 B; w spoczynku 50 076–50 516 B; najmniej w czasie odpytywania centrali 41 644 B |
| **Model na PC** — `LCD-Centrali/testy_host/pomiar_pamieci.sh --przypadki` | ile pamięci firmware panelu trzyma i ile bierze naraz w każdym przypadku; co robi, gdy pamięci jest mniej |
| **Rachunek z kodu** — `narzedzia/pamiec_centrali.py` | rozmiary buforów centrali i pamięć jednego zapytania; rozmiary struktur od kompilatora z prawdziwymi nagłówkami ESP32; limity systemu plików |
| **Ustawienia bibliotek** — arduino-esp32 (lwIP, PSRAM), ESPAsyncWebServer 3.10.3, AsyncTCP 3.4.10 | okno TCP 5 760 B, próg 4 096 B dla PSRAM, kopia odpowiedzi w serwerze HTTP, stos zadania `async_tcp` |
| **Nie sprawdzone** | v1.3.0 i 3.58.5 na płytkach; budowanie w PlatformIO; wolna pamięć centrali; zajętość systemu plików centrali; praca przez wiele dni |

Model na PC to ten sam firmware panelu zbudowany jako program 32-bitowy.
Każdy `malloc()` firmware'u, LVGL i ArduinoJson jest liczony z narzutem
sterty ESP32 (16 B na blok). Nie ma w nim WiFi ani lwIP płytki.

---

## Panel LCD

Płytka: ESP32 bez PSRAM, 4 MB flash, dwie partycje programu po 2 031 616 B.

### Ile RAM-u ma płytka

Z logu v1.2.2:

```
tag=PAMIEC zdarzenie=po_ekranach ram=105568 ram_blok=102388
tag=PAMIEC zdarzenie=po_starcie ram=49676 ram_blok=42996 ram_min=43836
tag=PANEL fw=v1.2.2 ... ram=50516 ram_min=43836 ram_blok=42996 ...
tag=PANEL fw=v1.2.2 ... ram=50076 ram_min=41644 ram_blok=42996 ...
```

v1.3.0 trzyma po starcie tyle samo co v1.2.2 (129 244 B w modelu) i ma
o 512 B więcej zmiennych statycznych. Stąd **49 164 B**
jako wolny RAM v1.3.0 po starcie.

### Ile potrzeba w każdym przypadku

Wynik `./pomiar_pamieci.sh --przypadki`, krok [6]:

```
[6] czy RAM-u starczy: przypadki pracy (model 32-bitowy; trwa ok. 2 minut)
plytka z logu 2026-11-12: po starcie 49 164 B wolnego RAM-u (v1.2.2 zmierzone: 49 676 B; te zrodla trzymaja o 512 B wiecej)
siec plytki (model jej nie liczy): odpytania centrali do 3 500 B, pobieranie wykresu do 7 500 B

przypadek                                             trzyma B   naraz B  na plytce B
praca bez dotyku, 10 minut                                   0     5 384       40 280
praca bez dotyku, doba                                   1 248     6 632       39 032
ekran alarmu                                               188     5 572       40 092
pozostale ekrany, kazdy po kolei                           192     5 576       40 088
polecenie z dotyku (POMPA: WLACZ)                          456     5 840       39 824
ekran Wykresy otwarty, bez wykresu                      19 076    24 460       21 204
wykres godzinny, 8 serii                                19 856    25 240       12 924
wykres dzienny, 8 serii                                 19 868    25 252       12 912
wykres tygodniowy, pelna historia centrali              19 856    25 240       12 924
wykres tygodniowy z przerwa                             19 876    25 260       12 904
wykres na ekranie, 10 s                                 19 868    25 252       20 412
centrala po restarcie: napis zamiast wykresu            20 032    25 416       12 748
centrala bez historii: napis                            19 932    25 316       12 848
jeden pomiar: napis                                     19 904    25 288       12 876
centrala bez pamieci (503): napis                       19 904    25 288       12 876
odpowiedz, ktora nie jest historia: napis               19 892    25 276       12 888
odpowiedz urwana: napis                                 19 900    25 284       12 880
centrala zamilkla w polowie: napis                      19 896    25 280       12 884
wolne lacze (wykres idzie 9 s)                          19 888    25 276       12 888
bardzo wolne lacze (limit 20 s): napis                  19 896    25 280       12 884
wykres w 80 roznych chwilach wzgledem odpytan           19 868    25 272       12 892
alarm w trakcie pobierania wykresu                       1 372    25 272       12 892
starsza centrala: caly CSV doby (100 kB)                19 856    25 240       12 924
starsza centrala: caly CSV tygodnia (8000 wierszy)      19 896    25 280       12 884
starsza centrala: CSV nie zdazyl w 20 s                 19 908    25 292       12 872
30 wykresow z rzedu                                     19 856    25 252       12 912
300 wykresow z rzedu                                    19 856    25 256       12 908
  trzyma  = ile pamieci ponad stan po starcie firmware trzyma po tym przypadku (w tym jednorazowe opisy osi i legenda)
  naraz   = najwiecej pamieci ponad stan po starcie w ktorejkolwiek chwili przypadku
  plytka  = wolny RAM plytki w tej chwili: 49 164 - naraz - siec (SZACUNEK; prawdziwa liczba: "ram_min=" w logu panelu)

najwiecej naraz: 25 416 B (centrala po restarcie: napis zamiast wykresu)
na plytce zostaje wtedy ok. 12 748 B wolnego RAM-u; bez otwartych Wykresow najmniej ok. 39 032 B
po pierwszym przejsciu wszystkich przypadkow zostaje 1 248 B (tworzone raz), po drugim przybywa 0 B
```

Co z tego wynika:

- **Poza ekranem Wykresy** panel bierze chwilowo 5,4 kB — tyle, ile
  jedno odpytanie centrali. Rysowanie pozostałych ekranów, alarm i polecenie
  z dotyku dokładają do tego 0,2–0,5 kB. Po pierwszym użyciu ekranów panel
  trzyma na stałe 1,2 kB więcej (tworzone raz, m.in. opisy osi i legenda
  wykresu), stąd razem ok. 6,6 kB. Na płytce zostaje ok. 39 kB; v1.2.2
  pokazało 41,6 kB.
- **Ekran Wykresy** trzyma 18,6 kB na punkty (8 buforów po 2 304 B plus
  narzut sterty), dopóki jest otwarty. Po zamknięciu oddaje je.
- **Każdy wykres kosztuje tyle samo**: 25,2–25,4 kB naraz. Nie zależy to od
  zakresu, liczby serii (sprawdzone 8 — więcej panel nie pozwala wybrać),
  ilości historii w centrali, szybkości łącza ani od tego, czy punkty liczy
  centrala, czy panel sam z całego CSV (1,4 MB w teście). Odpowiedź jest
  czytana linia po linii do stałego bufora.
- **Szczyt to zawsze „wykres + jedno odpytanie centrali”.** W 80 próbach
  z różnym przesunięciem między pobieraniem a odpytaniem nie wyszło nic
  gorszego.
- **Nic nie przybywa**: druga runda wszystkich przypadków, doba pracy
  (43 200 odpytań) i 300 wykresów z rzędu zostawiają najwyżej kilkadziesiąt
  bajtów (limit skryptu: 256 B).
- **Błędy nie kosztują więcej**: kod 503, strona HTML zamiast danych,
  odpowiedź urwana, cisza, limit czasu — po każdym zostaje napis i tyle
  samo pamięci co po wykresie.
- **Alarm w trakcie pobierania** zamyka Wykresy i zwalnia bufory.

Pojedyncze liczby różnią się między przebiegami o kilkadziesiąt bajtów:
udawana centrala działa w osobnym wątku i raz na kilka przebiegów któreś
odpytanie nie zdąży — panel pokazuje wtedy w nagłówku powód, a ten napis
zostaje w pamięci (40 B, jednorazowo).

### Co robi panel, gdy RAM-u jest mniej

Tego na płytce nie powinno być — ale warto wiedzieć, gdzie jest próg.
Krok [7] daje stercie modelu limit i powtarza zwykły ciąg czynności:

```
[7] co robi panel, gdy wolnego RAM-u jest mniej (limit sterty w modelu)
wolny RAM dla firmware co sie dzieje (praca 30 s, wejscie na Wykresy, wykres dzienny 8 serii, 20 s ogladania, powrot)
40 000 B               wszystko dziala, zadnej odmowy pamieci
30 000 B               wszystko dziala, zadnej odmowy pamieci
26 000 B               wszystko dziala, zadnej odmowy pamieci
24 000 B               wykres jest; w trakcie 3 odpytan centrali bez pamieci (naglowek: BRAK PAMIECI), po wyjsciu z Wykresow wraca do normy
22 000 B               wykres jest; w trakcie 3 odpytan centrali bez pamieci (naglowek: BRAK PAMIECI), po wyjsciu z Wykresow wraca do normy
20 000 B               wejscie na Wykresy (8 z 8 buforow) -> LVGL bez pamieci -> restart panelu z wpisem w logu
18 000 B               napis "Brak pamieci na wykres" (7 z 8 buforow); panel pracuje dalej
16 000 B               napis "Brak pamieci na wykres" (6 z 8 buforow); panel pracuje dalej
12 000 B               wejscie na Wykresy (5 z 8 buforow) -> LVGL bez pamieci -> restart panelu z wpisem w logu
8 000 B                wejscie na Wykresy (3 z 8 buforow) -> LVGL bez pamieci -> restart panelu z wpisem w logu
4 000 B                juz na pulpicie czesc odpytan centrali bez pamieci; napis "Brak pamieci na wykres" (1 z 8 buforow); potem restart (45 s bez danych z centrali)
przedzialy (129 prob, co 250 B od 0 do 32 000 B):
  od 24 900 B          wszystko dziala, zadnej odmowy pamieci
  21 000 - 24 750 B    wykres jest; czesc odpytan centrali bez pamieci, po wyjsciu z Wykresow wraca do normy
  ponizej 21 000 B     wejscie na Wykresy konczy sie restartem panelu z wpisem w logu (61 z 84 prob) albo napisem (23 z 84)
  awarii modelu (siegniecie pod zly adres): 0 z 162 prob
prog: od 24 900 B wolnego RAM-u w gore nie ma ani jednej odmowy pamieci
plytka: 49 164 B po starcie - 11 000 B sieci = 38 164 B dla firmware'u, czyli 13 264 B ponad prog
W NORMIE: najwiecej naraz <= 27 000 B, na plytce w najgorszej chwili >= 8 000 B, bez przyrostu, bez awarii modelu
```

- **Od 24 900 B wolnego RAM-u dla firmware'u** nie ma ani jednej
  odmowy pamięci. Płytka ma 49,2 kB minus do 11 kB sieci, czyli ok. 13 kB
  ponad ten próg.
- **Od 21 000 B do progu**: wykres jest, ale część odpytań centrali
  nie dostaje pamięci (nagłówek `BRAK PAMIECI`). Po wyjściu z Wykresów wraca
  do normy.
- **Poniżej 21 000 B**: wejście na Wykresy kończy się najczęściej
  restartem panelu (61 z 84 prób) — po 2,5 s, z wpisem
  `zdarzenie=lvgl_assert` w logu. W pozostałych próbach jest napis
  `Brak pamieci na wykres`. Zależy to od tego, ile buforów punktów
  zmieściło się przed wyczerpaniem pamięci.
- W żadnej z 162 prób model nie sięgnął pod zły adres (na płytce byłby to
  wyjątek procesora). W próbach, które nie skończyły się restartem, watchdog
  nie zadziałał ani razu.

### Stos i flash

- **Stos pętli (8 192 B).** Największa nowa ramka w v1.3.0 to
  `StrumienHttpGet::poll()`, 700 B; odpowiednik w v1.2.2,
  `NonBlockingHttp::poll()`, ma 652 B (kompilacja dla 32-bitowego x86,
  `-Os` — liczby przybliżone). Pobieranie wykresu jest teraz wołane wprost
  z pętli, a w v1.2.2 szło z wnętrza obsługi dotyku LVGL, więc łańcuch
  wywołań jest krótszy niż w wersji, która działa na płytce. Zapasu stosu
  nie zmierzyłem: panel go nie loguje.
- **Flash.** Partycja programu ma 2 031 616 B. v1.2.2 się w niej mieści
  (było wgrane). v1.3.0 jest większe o ok. 8 kB (kod +5,0 kB, stałe +2,7 kB;
  przybliżenie z tej samej kompilacji). Panel nie zapisuje niczego do flasha
  w związku z wykresami.

### Czego ten pomiar nie obejmuje

- **Sieć płytki przy pobieraniu wykresu** — przyjęte „do 7,5 kB” to górna
  granica z ustawień, nie pomiar.
- **Ponowne łączenie z WiFi przy otwartym ekranie Wykresy** — skan sieci
  i logowanie biorą na płytce kilka kB, których model nie liczy.
- **Aktualizacja panelu przez centralę (OTA)** — dzieje się tylko zaraz po
  starcie, gdy ekran Wykresy jest zamknięty. Potrzebuje ok. 13 kB (bufor
  zapisu 4 kB, klient HTTP, okno TCP); policzone, nie zmierzone.
- **Rozdrobnienie sterty po wielu dniach** — model liczy bajty, nie układ
  bloków. Największy przydział w czasie pracy ma 2 304 B.

### Jak potwierdzić na płytce

Po wgraniu v1.3.0 wybierz 8 serii, pokaż wykres tygodniowy i odczytaj z logu
panelu (115200):

```
tag=PAMIEC zdarzenie=po_starcie ram=...            spodziewane: ok. 49 000
tag=WYKRESY zdarzenie=pobrano ... ram=... ram_min=...
                                                   ram: ok. 29 000; ram_min: ok. 13 000 albo więcej
tag=PANEL fw=v1.3.0 ... ram=... ram_min=... ram_blok=...
```

`ram_min` to najmniejsza ilość wolnego RAM-u od startu. Jeśli po kilku
wykresach jest poniżej 8 000, przyślij te linie.

---

## Centrala

Płytka: ESP32-S3 z 8 MB PSRAM i 16 MB flash (N16R8); partycje programu po
3 145 728 B, system plików 10 354 688 B (według `partitions.csv` z projektu
centrali w repozytorium).

Wynik `python3 narzedzia/pamiec_centrali.py`:

```
PAMIEC CENTRALI - rachunek z kodu
plik: firmware-patch/v3.58.5-panel-wykresy-po-restarcie/src/main_centrala.cpp
wersja: v3.58.5   SHA-256: f7dc097e016e2ef2b89c36ec2a34d0ce16768115c34d697751462472c40f166c
rozmiary struktur: kompilator z naglowkami arduino-esp32 3.3.8 (xtensa): HistoriaProbka 60 B, HistoriaProbkaDluga 152 B, PanelHistKubelek 232 B, PanelHistWiersz 140 B

[1] PSRAM: bufory przydzielane RAZ, przy starcie (nie rosna z czasem - najstarsze wpisy sa nadpisywane)
    historia krotka (probka co 60 s, 24 h)   1 440 x 60 B            86 400 B
    historia dluga (zdarzenia)               8 000 x 152 B        1 216 000 B
    kolejka telemetrii do Firebase           1 440 x 80 B           115 200 B
    log                                                              65 536 B
    diagnostyka zdalna                                               19 985 B
    tekst paczki telemetrii                                           8 192 B
    razem                                                         1 511 313 B  = 18 % z 8 388 608 B PSRAM
    historia dluga: najwyzej 1 wpis na probke (co 60 s), wiec 8 000 wpisow to CO NAJMNIEJ 5,6 doby;
      wpis powstaje tylko przy zmianie wartosci o prog albo raz na godzine - im spokojniej, tym dluzszy okres.
      (twardy limit w kodzie: nie czesciej niz co 30 s - HIST_DLUGA_MIN_ODSTEP_MS)
    Wylacznik zapisu na flash nie zmienia tych buforow: sa w pamieci zawsze.

[2] Jedno zapytanie GET /api/history/panel (panel LCD prosi o pkt=96; najwiecej, ile mozna: pkt=240)
                                                                     pkt=96      pkt=240
    punkty robocze: pkt x 232 B (ps_malloc, zwalniane od razu)     22 272 B     55 680 B
    tekst odpowiedzi: String::reserve(pkt x 230 + 160)             22 240 B     55 360 B
    kopia tekstu w serwerze HTTP, do konca wysylania               19 594 B     48 985 B
    NAJWIECEJ NARAZ (dwa z trzech blokow jednoczesnie)             44 512 B    111 040 B
    Wszystkie trzy bloki maja ponad 4 096 B, wiec malloc() arduino-esp32 bierze je z PSRAM
      (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096); gdy PSRAM odmowi - ze zwyklej sterty, a gdy i ta odmowi - kod 503.
    Najdluzsza odpowiedz w testach (pkt=96): 19 594 B. Zalezy od liczby punktow, NIE od ilosci historii:
      pelne bufory (8 000 + 1 440 wierszy) i pusta historia biora tyle samo albo mniej.
    Kilka zapytan naraz: handler liczy jedno po drugim; czekajace na wyslanie trzymaja tylko swoja kopie tekstu.

[3] Dla porownania - stare adresy (strona WWW; panel LCD do v1.2.2 i panel 1.3.0 ze starsza centrala)
    /api/history         String::reserve(1 440 x 60) = 86 400 B i kopia w serwerze HTTP: przy pelnym buforze ok. 172 800 B naraz
    /api/history/long    String::reserve(8 000 x 230) = 1 840 000 B i kopia w serwerze HTTP: przy pelnym buforze ok. 3 680 000 B naraz
    Panel 1.3.0 z centrala 3.58.4+ nie wywoluje juz /api/history/long: 44 512 B zamiast do ok. 3 680 000 B na wykres tygodniowy.

[4] Stos i rozmiar kodu - PRZYBLIZENIE (ten sam kod skompilowany dla 32-bitowego x86, -Os)
    ramka stosu historiaPanelZbudujCsv(): 1 116 B; stos zadania async_tcp, w ktorym dziala handler: 16 384 B
      (do tego snprintf/mktime z biblioteki C i ramki serwera HTTP - takie same jak przy pozostalych adresach)
    kod funkcji panelHist*/historiaPanel*: 5 953 B; zmienne statyczne tego kodu: 0 B

[5] System plikow centrali (flash na dane, partycja 10 354 688 B) - LIMITY Z KODU, nie pomiar
    log biezacy (log_b.txt)              rosnie do 79 % pojemnosci = 8 180 203 B; potem wysylka na Telegram i kasowanie albo rotacja
    logi + firmware panelu do OTA        logi przycinane, gdy poza tymi plikami zostaje mniej niz 20 % = 2 070 937 B
    log alarmow / log nastaw             najwyzej 1 048 576 B / 131 072 B
    historia krotka (hist_short.csv)     przycinana do 1 440 linii po przekroczeniu 2 440; rezerwa wg firmware'u 131 072 B
    historia dluga (hist_long.csv)       przycinana do 8 000 linii po przekroczeniu 8 500; rezerwa wg firmware'u 2 097 152 B
    zrzuty po awarii (coredumps)         rezerwa wg firmware'u 1 310 720 B
    wolne minimum wg firmware'u          310 641 B
    rezerwy firmware'u razem             3 849 585 B
    Przycinanie logow liczy zajetosc jako: g_logBSize + g_logASize + g_logKrytycznySize + g_panelFirmwareSize - plikow historii NIE liczy.
    Przy logach przy swoim limicie na reszte plikow zostaje 2 070 937 B, a rezerwy wyliczone w firmware to 3 849 585 B.
      Z samego kodu NIE da sie wiec potwierdzic, ze miejsca starczy, gdy logi, pliki historii i zrzuty sa naraz przy limitach.
      Firmware sprawdza to sam ("FS quota shadow", ostrzezenie fs_quota_shadow_low), ale niczym jeszcze nie steruje.
    Pliki historii rosna tylko przy wlaczonym zapisie historii. Kod wykresow dla panelu LCD niczego nie zapisuje.
    Na plytce: adres /api/fs-list (pola "used", "total" i rozmiary plikow) albo raport diagnostyczny telemetrii
      w Telegramie (linie "FS fizyczny", "FS skladniki", "FS quota shadow").

Na plytce: wolna pamiec podaje log centrali co 5 minut (tag=HEALTH-A: int_free, int_min, psram_free, psram_min).
```

### Co z tego wynika

- **Historia nie rośnie.** Bufory są przydzielane raz, przy starcie, i mają
  stały rozmiar: 1,3 MB na historię, 1,5 MB razem z kolejką telemetrii
  i logiem. Nowe wpisy nadpisują najstarsze. Wyłącznik „Zapisuj historię na
  flash centrali” tych buforów nie dotyczy.
- **Wykres dla panelu to 45 kB PSRAM na czas liczenia** i ok. 20 kB do końca
  wysyłania. Tyle samo przy pustej i przy pełnej historii.
- **Stara droga była ok. 80 razy cięższa.** Panel do v1.2.2 przy każdym
  wykresie tygodniowym kazał centrali zbudować cały CSV: do 3,7 MB PSRAM
  naraz. Panel v1.3.0 z centralą 3.58.5 tego nie robi. Strona WWW centrali
  korzysta ze starych adresów jak dotąd.
- **Zwykły RAM centrali** (ten, z którego korzystają WiFi, TLS i sterowanie)
  dostaje od tego zapytania tylko ramkę stosu ok. 1 kB i drobne obiekty
  serwera HTTP. Duże bloki idą do PSRAM.
- **Gdy pamięci zabraknie**, centrala odpowiada kodem 503, a panel pisze
  `Centrali zabraklo pamieci na wykres (HTTP 503)`. Sprawdzone w testach
  paczki centrali (odmowa w każdym z 98 miejsc dopisywania tekstu) i po
  stronie panelu.
- **Flash.** Kod wykresów dla panelu to ok. 6 kB. Niczego nie zapisuje — ani
  do plików, ani do NVS.

### Czy pamięci starczy na dobę i na tydzień historii

- **Doba: tak, zawsze.** Bufor krótki ma 1 440 miejsc, próbka co 60 s.
- **Tydzień: co najmniej 5,6 doby; ile naprawdę, zależy od tego, jak często
  zmieniają się odczyty.** Bufor długi ma 8 000 miejsc. Centrala sprawdza
  odczyty co 60 s i dopisuje wpis tylko wtedy, gdy zmieni się stan pompy,
  któraś wartość odejdzie od ostatnio zapisanej o próg (temperatury 1 °C,
  kąt 5°, wilgotność 3 %, ciśnienie 0,05) albo minie godzina. Gdyby wpis
  powstawał przy każdym sprawdzeniu, 8 000 wpisów wystarczy na 5 dni
  i 13 godzin; przy wpisie co 2 minuty — na 11 dni. Jak często wpisy powstają
  przy palącym się piecu, nie wiem: nie mam takiego logu. Gdy historii jest
  mniej niż tydzień, wykres tygodniowy pokazuje tyle, ile jest, a zakres
  jest podpisany pod wykresem.

Wyłączony zapis na flash tego nie zmienia. Zmienia tylko to, co centrala ma
po restarcie.

### System plików centrali (flash na dane)

Punkt [5] wyżej to limity przepisane z kodu, nie pomiar. Kod plików historii
jest częściowo w nagłówkach, których nie ma w repozytorium
(`CheckedHistoryIO.h` i inne), więc czytałem tylko `main_centrala.cpp`.

- **Największym plikiem są logi — z założenia.** `log_b.txt` rośnie do 79 %
  pojemności (8,18 MB). Wtedy centrala wysyła logi na Telegram i je
  kasuje, a gdy to się nie uda, `log_b.txt` zastępuje `log_a.txt`.
  Osobny bezpiecznik przycina log, gdy poza logami i firmware'em panelu
  zostaje mniej niż 20 % (2,07 MB).
- **Pliki historii mają limit.** `hist_short.csv` jest przycinany do 1 440
  linii, `hist_long.csv` do 8 000. Rosną tylko przy włączonym zapisie
  historii. Firmware przewiduje na nie w swoich rachunkach 131 kB
  i 2,10 MB.
- **Bezpiecznik logów nie liczy plików historii.** Zajętość liczy jako sumę
  trzech logów i firmware'u panelu. Komentarz przy tym rachunku mówi, że
  historia idzie przez RAM, więc się jej nie liczy — a historia jest dziś
  zapisywana do plików.
- **Według limitów z kodu rachunek się nie domyka.** 8,28 MB (logi
  i firmware panelu przy progu przycinania) + 2,23 MB (pliki historii
  według rezerw firmware'u) = 10,51 MB, a system plików ma 10,35 MB — bez
  zrzutów po awarii (do 1,31 MB) i logu nastaw. Prawdziwe pliki historii
  mogą być mniejsze niż rezerwa, więc to nie dowód, że miejsca zabraknie.
  Z kodu nie umiem jednak potwierdzić, że starczy, gdy logi są przy limicie,
  a zapis historii jest włączony.
- **Firmware już to wykrywa, ale jeszcze nie steruje.** Kontrola „FS quota
  shadow” porównuje prawdziwą ilość wolnego miejsca z rezerwami (3,85 MB)
  i pisze ostrzeżenie; komentarz w kodzie mówi, że na razie tylko raportuje.
- **Wykresów na LCD to nie dotyczy.** Liczone są z buforów w PSRAM. Wpis
  trafia najpierw do bufora, potem do kolejki zapisu na flash. Gdy przycięcie
  pliku historii się nie uda, kod zatrzymuje zapis historii do plików
  (`HISTORIA-FS … zatrzymano obie historie`). Co robi reszta firmware'u przy
  pełnym systemie plików, nie sprawdzałem.

Stan na urządzeniu:

```
http://<adres centrali>/api/fs-list
    "used": ..., "total": ..., i rozmiar każdego pliku

Telegram, przycisk „Telemetria RTDB”
    FS fizyczny: used=... B / total=... B / free=... B
    FS skladniki: logi=... B ustawienia=... B histS=... B histL=... B panel=... B coredump=... B/... plikow
    FS quota shadow: free=... B / rezerwa=... B / po_rezerwach=... B / status=OK albo LOW

log centrali (port szeregowy), gdy miejsca jest mało
    tag=LOGI-FS lvl=WARN zdarzenie=fs_quota_shadow_low free=...B reserve=...B po_rezerwach=...B
    tag=LOGI-FS lvl=ERR zdarzenie=fs_guard_critical pelny_pct=...
```

Jeśli przyślesz linie „FS fizyczny” i „FS skladniki”, policzę zapas na
prawdziwych liczbach.

### Czego nie wiem

- **Ile wolnego PSRAM i zwykłego RAM-u ma Twoja centrala.** Poza buforami
  z tabeli z PSRAM korzystają też inne części firmware'u (każdy tekst ponad
  4 kB, bufory WiFi). Centrala podaje to sama co 5 minut:

  ```
  tag=HEALTH-A lvl=INFO zdarzenie=memory_snapshot int_free=...B int_largest=...B int_min=...B psram_free=...B psram_largest=...B psram_min=...B ...
  ```

  `psram_free` powinno być liczone w megabajtach. Jeśli chcesz, przyślij
  jedną taką linię po kilku wykresach na panelu.
- **Ile miejsca jest zajęte w systemie plików centrali** — patrz wyżej.
- **Ile zajmuje firmware centrali 3.58.5 we flashu.** Wydanie 3.57.9 zajmuje
  2 472 384 B z 3 145 728 B (liczba z GitHuba, dopisana później). Nowy kod
  wykresów to ok. 6 kB, ale między 3.57.9 a 3.58.5 doszły też inne zmiany.
  Dokładną zajętość pokaże PlatformIO.
- **Stos zadania `async_tcp` na ESP32.** Ramka 1 116 B jest z kompilacji na
  PC. Log centrali podaje zapas stosu czterech innych zadań, tego nie.

---

## Co wyszło przy okazji

Żadna z tych rzeczy nie powstała w v1.3.0 ani w 3.58.5 i żadna nie wymaga
zmiany przed wgraniem. Niczego z tej listy nie zmieniałem.

1. **Panel: przy braku RAM-u wejście na Wykresy bywa restartem zamiast
   napisu.** Bufory punktów są przydzielane po kolei. Gdy wolnej pamięci jest
   mniej niż ok. 21 kB, kilka z nich zabiera resztę i następny przydział LVGL
   kończy się restartem. Na tej płytce do tego nie dochodzi (zapas ok. 13 kB).
   Można to zamienić na pewny napis: sprawdzić wolny RAM przed otwarciem
   ekranu i przydzielać wszystkie bufory albo żaden. To zmiana w firmware
   panelu — nie zrobiona.
2. **Centrala: bezpiecznik miejsca na logi nie liczy plików historii.**
   Opis w rozdziale „System plików centrali”. Z kodu wychodzi, że przy logach
   przy limicie i włączonym zapisie historii miejsca może zabraknąć; firmware
   ma już kontrolę, która to wykrywa, ale na razie tylko ostrzega. To kod
   istniejący od dawna, bez związku z wykresami na LCD. Najpierw warto
   zobaczyć prawdziwą zajętość (`/api/fs-list`).
3. **Centrala: próg ciśnienia w historii długiej.** `HIST_PROG_CISNIENIE`
   ma wartość 0,05 z komentarzem „bar”, a porównywany odczyt jest w hPa
   (BMP280). 0,05 bar to 50 hPa; 0,05 hPa to tysiąc razy mniej. Przy zmianie
   pogody o 1 hPa na godzinę taki próg jest przekraczany co 3 minuty, więc
   z podłączonym czujnikiem wpisy w historii długiej powstają częściej, niż
   wynika z komentarza, i bufor obejmuje krótszy okres (najmniej 5,6 doby).
   Jak często naprawdę — nie zmierzyłem: w Twoim logu centrali z 11.11
   ciśnienie ma stale 0,0 hPa (wygląda na niepodłączony czujnik). Widać to
   po zakresie pod wykresem tygodniowym na LCD albo po liczbie linii
   `HISTORIA_DLUGA zdarzenie=zapis` na godzinę w logu centrali (poziom
   DEBUG). To kod istniejący od dawna, nie zmieniany.
4. **Narzędzie pomiarowe panelu liczyło szczyty o ok. 0,2 kB za wysoko.**
   Każde zmniejszenie bloku przez `realloc()` traktowało jak nowy przydział.
   Sterta ESP32 zmniejsza blok w miejscu. Po poprawce: jedno odpytanie
   centrali 5 384 B (było 5 588), wykres 2 452 B (było 2 884). Liczby
   „trzyma po starcie” się nie zmieniły. W komentarzu wersji v1.3.0
   w `src/main.cpp` zostały starsze, wyższe liczby.
5. **Największa rezerwa RAM-u panelu**, gdyby kiedyś była potrzebna: wszystkie
   13 ekranów jest budowanych przy starcie i zajmuje na płytce 112,6 kB
   (`ram` spada z 218 148 do 105 568). Budowanie ich dopiero przy wejściu
   zwolniłoby większość tego.

---

## Jak powtórzyć

```
# panel (ok. 2 minut; wymaga: python3, pip install ziglang, git, binutils)
cd LCD-Centrali/testy_host
./pomiar_pamieci.sh --przypadki

# centrala (kilka sekund po pierwszym pobraniu nagłówków ESP32)
python3 narzedzia/pamiec_centrali.py
```

`pomiar_pamieci.sh --przypadki` kończy się kodem 1, gdy któraś liczba wyjdzie
poza limit: najwięcej naraz ponad 27 000 B, szacunek dla płytki poniżej
8 000 B, przyrost ponad 256 B w drugiej rundzie, w ciągu doby albo po 300
wykresach, brak wykresu tam, gdzie ma być, restart albo awaria modelu.
Sprawdzone na celowo zepsutej kopii źródeł (48 B gubione przy każdym
wykresie): skrypt zgłasza przyrost 8 128 B w drugiej rundzie i 19 188 B po
300 wykresach i kończy się kodem 1.

Opis narzędzia i jego ograniczeń: `LCD-Centrali/testy_host/README.md`,
rozdział „Czy RAM-u starczy: `--przypadki`”.
