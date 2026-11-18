# Audyt panelu LCD — wycieki, drenaż, błędy w kodzie

Data: 2026-11-18
Dotyczy: firmware panelu LCD **v1.4.0** (`LCD-Centrali/`), gałąź
`arena/0511811e-sterownik-pieca-c-o`, commit `3382783`.

**Po co ten plik.** To lista wszystkiego, co w panelu jest albo może być
problemem z pamięcią, z poborem prądu i z samym kodem. Przy każdej pozycji
jest dowód (plik i linia albo pomiar), propozycja poprawki i to, czego warto
poszukać w internecie, zanim się to poprawi.

**W kodzie firmware'u (`src/`) nic nie zmieniłem.** Doszło tylko narzędzie
pomiarowe w `LCD-Centrali/testy_host/` (opis: załącznik A).

Zakres to **panel LCD**. Centrala (50 tys. linii) to osobna robota — patrz
rozdział 13.

---

## 1. Najkrócej

1. **Wycieku pamięci nie znalazłem.** Sprawdziłem wszystkie miejsca, w których
   panel bierze i oddaje pamięć, i uruchomiłem 47 scenariuszy z narzędziem,
   które wykrywa błędy pamięci — zero zgłoszeń. Pamięć jest za to bez przerwy
   „mielona”: ok. 6 800 próśb o pamięć na minutę, gdy na ekranie nic się nie
   dzieje (A2).
2. **Na baterii panel nigdy nie zasypia i nigdy nie przygasza ekranu.** Kod
   usypiania jest w pliku, ale w tej wersji programu nikt go nie woła (B1).
   Przed włączeniem go trzeba zdecydować, co z alarmem: śpiący panel nie
   zapiszczy.
3. **Co 2 sekundy panel rysuje od nowa 61 % ekranu, choć nic się nie
   zmieniło.** To ok. 18 pełnych ekranów na minutę i ok. 12 GB danych na dobę
   wysyłanych do wyświetlacza. Da się zejść prawie do zera (B2).
4. **Polecenia z ekranu potrafią zginąć.** Gdy jedno polecenie jest w drodze,
   następne jest po cichu pomijane (C4).
5. **Wersja platformy nie jest przypięta** w `platformio.ini` panelu. Dziś
   budujesz tym, co akurat jest zainstalowane. Na innej wersji ten sam kod
   albo się nie skompiluje, albo ekran przygaśnie po pierwszym alarmie (C2).
6. **Jasność da się ustawić na 0 %** i ekran zostaje czarny, także po
   restarcie. Alarm ekranu nie rozjaśnia — przy jasności nocnej 0 % ekranu
   alarmu w nocy nie widać (C9).
7. **Kodowanie znaków jest w porządku** — wszystkie pliki to czysty UTF-8,
   jedyny znak spoza ASCII w napisach programu to `°`, który jest w czcionce
   (rozdział 9).
8. **Hasła** — osobny, prywatny plik (rozdział 5).

Kolejność prac, którą proponuję: rozdział 11.

---

## 2. Co sprawdziłem i jak bardzo można temu ufać

### Co było sprawdzane

| Plik | Linii | Linii kodu | Jak |
|---|---|---|---|
| `src/main.cpp` | 5 550 | 2 905 | przeczytany w całości |
| `src/siec_nieblokujaca_LCD.h` | 1 149 | 615 | przeczytany w całości |
| `src/lvgl_port.cpp` | 179 | 79 | przeczytany w całości |
| `src/dashboard_lvgl_poc.cpp` | 7 855 | 4 001 | przeczytane: odświeżanie pulpitu, liczniki czasu, animacje, bufory wykresów, nakładka aktualizacji, przyciski +/−. Budowanie ekranów sprawdzone wyszukiwaniem wzorców i narzędziami, **nie linia po linii** |
| `src/diagnostyka_panelu.h` | 1 111 | 693 | narzędzia + fragmenty (ma 442 własne sprawdzenia) |
| `platformio.ini`, `partitions.csv`, `include/lv_conf.h` | — | — | przeczytane w całości |

### Czym

- **Trzy analizatory kodu**: `g++ 12.2` z 29 dodatkowymi grupami ostrzeżeń,
  `cppcheck 2.22`, `clang-tidy` (z analizatorem statycznym clang).
- **Linker**: lista funkcji, do których w budowanej wersji nikt się nie
  odwołuje.
- **Model na PC** (`testy_host/`): prawdziwy kod panelu, prawdziwe LVGL 8.3.11
  i ArduinoJson 7.2.1, atrapy sprzętu. 442 sprawdzenia jednostkowe
  i 47 scenariuszy (486 sprawdzeń) — wszystkie przechodzą, także
  z AddressSanitizer + UBSan (zero zgłoszeń).
- **Pomiar obciążenia** na tym modelu: nowe `pomiar_obciazenia.sh`.
- **Źródła bibliotek w konkretnych wersjach**, czytane wprost z GitHuba:
  arduino-esp32 3.0.7 – 3.3.8, LVGL 8.3.11, TFT_eSPI 2.5.43, pliki
  konfiguracji bibliotek Arduino. Lista w rozdziale 10.

### Czego nie zrobiłem

- **Nie budowałem firmware'u na ESP32** i nie miałem płytki.
- **Nie mierzyłem prądu ani czasu.** Model liczy, ile pracy panel sobie
  zadaje (piksele, połączenia, prośby o pamięć) — nie ile to kosztuje baterii.
- Nie sprawdzałem centrali ani aplikacji na telefon.
- **Nie szukałem jeszcze w internecie** — poza jednym sprawdzeniem karty
  katalogowej ESP32 (tabela prądów w rozdziale 7). Tematy do wyszukania są
  w rozdziale 10.

### Skala pewności przy ustaleniach

| Słowo | Znaczenie |
|---|---|
| **pomiar** | zmierzone na modelu PC narzędziem z repozytorium — da się powtórzyć |
| **kod** | wynika wprost z kodu panelu, podaję plik i linię |
| **źródła** | przeczytane w kodzie biblioteki w podanej wersji |
| **wniosek** | moje rozumowanie; trzeba potwierdzić na płytce |
| **z pamięci** | liczba albo fakt, którego tu nie sprawdziłem — do potwierdzenia w internecie |

---

## 3. Wersje

| Co | W projekcie panelu | Co z tego wynika | Najnowsze 2026-11-18 |
|---|---|---|---|
| Platforma PlatformIO | `platform = espressif32` — **bez wersji** (`platformio.ini:29`) | brana jest ta, która jest zainstalowana. `platformio.ini` centrali przypina pioarduino `55.03.38-1` | pioarduino `55.03.313`; oficjalna `8.0.0` |
| Rdzeń arduino-esp32 | wynika z platformy | pioarduino 55.03.38 = rdzeń **3.3.8**; oficjalna 8.0.0 = rdzeń 3.3.12; oficjalna do 7.1.3 = rdzeń 2.0.17 | `3.3.13` (jest też `4.0.0-RC1`) |
| LVGL | `^8.3.11` | dopuszcza 8.4.x | linia 8 skończyła się na `8.4.0`; bieżąca to `9.6.0` (inne API) |
| ArduinoJson | `^7.2.0` | dopuszcza każde 7.x | `7.4.3` |
| TFT_eSPI | `^2.5.43` | — | `2.5.43` z 2024-03-06 — od tamtej pory bez wydań |
| Płytka | `esp32dev` | procesor 240 MHz, 4 MB flash, bez PSRAM | — |

Znak `^` znaczy „ta wersja albo nowsza z tej samej linii”. Dwa komputery mogą
więc zbudować z tych samych źródeł dwa różne programy.

---

## 4. Wszystkie ustalenia w jednej tabeli

Waga: **krytyczne** — psuje to, do czego urządzenie służy; **ważne** — realny
kłopot, warto zrobić w pierwszej kolejności; **średnie** — warto; **drobne** —
porządki.

| Nr | Co | Waga | Pewność |
|---|---|---|---|
| W | Hasła — osobny, prywatny plik | — | — |
| A1 | Wycieku pamięci nie znalazłem | wynik | pomiar + kod |
| A2 | 6 800 próśb o pamięć na minutę w spoczynku | średnie | pomiar |
| A3 | Mało wolnej pamięci, jeden wspólny „worek” dla ekranu, sieci i danych | średnie | pomiar w modelu + szacunek dla płytki (wcześniejszy raport) |
| A4 | Odpowiedź centrali nie ma górnego limitu rozmiaru | średnie | kod |
| A5 | Zapas stosu nieznany — panel go nie wypisuje | średnie | kod |
| A6 | Jedna prośba o pamięć bez sprawdzenia wyniku | drobne | kod |
| A7 | Napisy `String` składane przy każdym zapytaniu | drobne | kod |
| B1 | Na baterii panel nie zasypia, nie przygasza, nie reaguje na słabą baterię | krytyczne dla pracy na baterii | pomiar + kod |
| B2 | Co 2 s rysuje od nowa 61 % ekranu bez żadnej zmiany | ważne | pomiar |
| B3 | Procesor cały czas na 240 MHz, bez usypiania między zadaniami | średnie | źródła |
| B4 | 42 połączenia z centralą na minutę | średnie | pomiar |
| B5 | Podświetlenie: domyślnie 100 %, przygaszanie tylko według zegara centrali | średnie | kod |
| B6 | Bez centrali w zasięgu: ciągłe szukanie i restart co 5 minut | średnie | kod |
| B7 | Nadajnik WiFi na pełnej mocy | drobne | kod |
| B8 | Zapis do pamięci flash — bez zastrzeżeń; jedna uwaga | wynik | kod + źródła |
| B9 | Po alarmie animacje ekranów POMPA i SERWO chodzą dalej w tle | drobne | kod |
| C1 | Usypianie i obsługa baterii są tylko w martwej części pliku; ok. 950 linii nieużywanego kodu w `main.cpp` | ważne | kod + linker |
| C2 | Platforma nieprzypięta; na rdzeniu 3.0–3.2.0 alarm przygasza ekran | ważne | kod + źródła |
| C3 | Kalibracja dotyku nie „karmi” watchdoga — restart co 15 s | średnie | kod |
| C4 | Polecenia z ekranu giną, gdy poprzednie jest w drodze; odświeżanie nadpisuje wartość w trakcie zmiany | ważne | kod |
| C5 | 16 odwołań do tabeli ustawień bez sprawdzenia, czy nazwa istnieje | drobne | kod |
| C6 | Dwie klasy sieciowe robią to samo na dwa sposoby; jedna nie sprawdza wyniku wysyłki | drobne | kod |
| C7 | Nieużywane stałe i funkcje, zdublowany fragment, opis niezgodny z kodem | drobne | kod |
| C8 | Ostrzeżenia kompilatorów — co z nich jest szumem | wynik | narzędzia |
| C9 | Jasność można ustawić na 0 % (czarny ekran, zapamiętany); alarm nie rozjaśnia ekranu | średnie | kod |
| D | Kodowanie znaków — bez problemów | wynik | pomiar |

---

## 5. W — hasła

Ta część jest w **osobnym pliku, którego nie ma w repozytorium**:
`AUDYT_PANELU_LCD_2026-11-18_HASLA_PRYWATNE.md` na Twoim Dysku Google.
Powód: repozytorium jest publiczne, a tamten plik wskazuje konkretne miejsca.

Tematy do poszukania w internecie z tej części są w rozdziale 10 (grupa W).

---

## 6. A — pamięć

Trzy pojęcia, żeby reszta była jasna:

- **Sterta** — wspólny „worek” pamięci, z którego program bierze kawałki
  (`malloc`) i do którego je oddaje (`free`). W panelu z jednego worka biorą:
  ekran (LVGL), sieć WiFi i rozpakowywanie odpowiedzi centrali.
- **Wyciek** — kawałek wzięty i nigdy nieoddany. Po godzinach albo dniach
  worek się kończy i panel się restartuje.
- **Poszatkowanie (fragmentacja)** — pamięci jest dość, ale w małych
  kawałkach, więc większej prośby nie da się spełnić.

### A1. Wycieku pamięci nie znalazłem

**Co sprawdziłem.**

| Co panel bierze | Gdzie | Czy oddaje |
|---|---|---|
| Bufor rysowania ekranu, 15 360 B | `lvgl_port.cpp:145` | raz na starcie, na zawsze — tak ma być |
| 8 buforów wykresu po 2 304 B | `dashboard_lvgl_poc.cpp:4966–4977` | oddaje przy każdym wyjściu z ekranu Wykresy (`4987–4998`), także gdy ekran przejmuje alarm (`5002–5006`) |
| 12 B na każdy przycisk z efektem wciśnięcia | `dashboard_lvgl_poc.cpp:1278` | raz na starcie, na zawsze. Przyciski nigdy nie są usuwane, więc nic nie przybywa |
| Wszystkie ekrany LVGL | funkcje `*_zbuduj` | budowane raz na starcie (`lvgl_dashboard_poc_init`, wołane z jednego miejsca), nigdy nie budowane ponownie |
| Nakładka „AKTUALIZACJA PANELU” | `dashboard_lvgl_poc.cpp:7775–7835` | tworzona i usuwana w całości (`lv_obj_del`, linia 7777) |
| 10 liczników „brak dotyku — wróć” | funkcje `*_idle_resetuj` | stary zawsze usuwany przed utworzeniem nowego; wskaźnik zerowany. Wszystkie 10 kasowane, gdy wchodzi ekran alarmu (`3297–3307`) |
| Jednorazowe liczniki efektu wciśnięcia | `dashboard_lvgl_poc.cpp:1269–1270` | LVGL usuwa je sam po zadziałaniu |
| Gniazda sieciowe | `siec_nieblokujaca_LCD.h` | zamykane na każdej ścieżce: sukces (452), błąd (474–477), przerwanie (655–661), błąd natychmiastowy (429) |
| Odpowiedź centrali (`String`) | `siec_nieblokujaca_LCD.h:299` | bufor zostaje i jest używany ponownie; przy braku pamięci jest zwalniany (303) |
| Rozpakowany JSON | `main.cpp:2838`, `3054`, `3753` | zmienna lokalna — znika z końcem funkcji |
| Pamięć ustawień (NVS) | `main.cpp:3816`, `4645`, `4659`, `1056` | otwierana i zamykana parami; jedna przestrzeń („wyswietlacz”) celowo otwarta na stałe |
| Wyniki skanu WiFi | `main.cpp:4117`, `4148` | kasowane przed i po każdym skanie |

**Dowody z uruchomienia.**

- 47 scenariuszy pod AddressSanitizer + UBSan: **0 zgłoszeń** (bez użycia
  pamięci po zwolnieniu, bez wyjścia poza bufor, bez bloków „zgubionych” na
  końcu).
- To samo narzędzie na 11-minutowym pomiarze spoczynku: 0 zgłoszeń.
- Pomiar pamięci w modelu 32-bitowym (`pomiar_pamieci.sh --przypadki`,
  29 przypadków, powtórzony dziś na v1.4.0): po pierwszym przejściu
  wszystkich przypadków zostaje 1 248 B (rzeczy tworzone raz), po drugim
  przybywa **0 B**. Potem doba pracy bez dotyku (43 200 odczytów statusu):
  przyrost **0 B**. 300 wykresów z rzędu: przyrost −12 B.

**Czego to nie dowodzi.** Model na PC nie zawiera sterownika WiFi ani
systemu ESP32. Wyciek w nich byłby widoczny tylko na płytce.

**Jak to sprawdzić na płytce.** Panel co 30 s wypisuje linię
`tag=PANEL … ram=… ram_min=… ram_blok=…`. Zostaw go na dobę na pulpicie
i porównaj `ram` z pierwszej i ostatniej godziny. Przy wycieku `ram` maleje
jednostajnie. Zdrowy obraz: `ram` skacze wokół stałej wartości, `ram_min`
ustala się po kilku minutach i dalej nie spada.

### A2. 6 800 próśb o pamięć na minutę, gdy nic się nie dzieje

**Co widzę.** Na pulpicie, przy niezmiennych danych, firmware razem z LVGL
i ArduinoJson prosi o pamięć **6 806 razy na minutę** (113 razy na sekundę),
łącznie 547 kB na minutę. Wszystko jest oddawane, więc to nie wyciek.

**Skąd to.**

- Ok. **cztery piąte** to rozpakowywanie odpowiedzi centrali. `/api/status`
  ma 79 pól; ArduinoJson 7 kopiuje nazwę każdego pola i każdą wartość
  tekstową do własnej pamięci. Panel czyta 44 z tych pól
  (`main.cpp:2862–2950`). Dzieje się to 30 razy na minutę dla statusu
  i 12 razy dla ustawień.
- Reszta to rysowanie (B2). W pomiarze z prototypem „rysuj tylko zmiany”
  zostaje 5 806 próśb na minutę; gdy centrala milczy — 348.

**Dlaczego to ważne.** Każda prośba to chwila pracy procesora i miejsce,
w którym przy małej ilości wolnej pamięci coś może się nie udać. Już raz tak
było: log z 2026-11-12, `json=NoMemory` (naprawione w v1.2.2 mniejszym
buforem ekranu).

**Co zrobić.**

1. Najpierw B2 — znika część związana z rysowaniem.
2. Rozpakowywać tylko potrzebne pola (ArduinoJson ma do tego filtr) albo
   poprosić centralę o krótszą odpowiedź dla panelu, np.
   `/api/status?panel=1` z samymi polami, których panel używa.
3. Ustawienia pobierać rzadziej — patrz B4.

**Pewność:** pomiar. Liczba nie obejmuje napisów `String` — na płytce jest
ich więc trochę więcej (A7).

### A3. Mało wolnej pamięci i jeden wspólny worek

**Co widzę.** `lv_conf.h:26` ma `LV_MEM_CUSTOM 1` — LVGL bierze pamięć
z tego samego worka co WiFi i reszta programu. Płytka nie ma PSRAM.

Liczby dla v1.4.0 (`pomiar_pamieci.sh --przypadki`, powtórzone dziś; opis
metody w `RAPORT_PAMIEC_RAM_LCD_I_CENTRALA.md`) — to **szacunki dla płytki**:
pomiar w modelu na PC przeliczony jednym logiem z płytki.

| Stan | Wolnej pamięci |
|---|---|
| Po starcie | ok. 48 kB |
| Najmniej w zwykłej pracy, bez otwartych Wykresów | ok. 38 kB |
| Wykres na ekranie w chwili odpytania centrali | ok. 11,7 kB |
| W trakcie aktualizacji panelu | ok. 25,7 kB |

Żeby ani razu nie dostać odmowy pamięci, firmware potrzebuje 24,9 kB; płytka
ma według szacunku ok. 12,3 kB ponad to.

**Dodatkowo (źródła, arduino-esp32 3.3.8, `WiFiGeneric.cpp:273–280`):**
biblioteka WiFi domyślnie bierze bufory odbiorcze i nadawcze dynamicznie
z tego samego worka — do 32 odbiorczych i 32 nadawczych. Przy zwykłym ruchu
panelu to kilka buforów, ale nie ma tu twardej granicy po stronie panelu.

**Dlaczego to ważne.** Zapasu jest niewiele, a wszyscy biorą z jednego
miejsca. Każda nowa funkcja panelu zjada ten sam zapas.

**Co zrobić.** Na razie nic w kodzie — to tło dla pozostałych decyzji.
Do sprawdzenia w internecie: czy LVGL z własnym, stałym workiem
(`LV_MEM_CUSTOM 0`) nie byłby tu bezpieczniejszy; jak ograniczyć bufory WiFi.

### A4. Odpowiedź centrali nie ma górnego limitu

**Co widzę.** `siec_nieblokujaca_LCD.h:291–307` dokleja wszystko, co
przyjdzie, do jednego napisu, aż serwer zamknie połączenie albo minie 1,5 s.
Limitu rozmiaru nie ma.

**Dlaczego to ważne.** Gdy pod adresem centrali odpowie coś innego (inny
program, strona WWW, błąd w centrali), napis rośnie, aż zabraknie pamięci.
Kod to wykrywa i zwalnia napis (linie 299–306), ale przez chwilę worek jest
pusty — a z tego samego worka bierze WiFi.

**Co zrobić.** Dodać limit, np. 8 kB: powyżej — zamknąć połączenie z błędem
„odpowiedź za długa”. Prawdziwa odpowiedź `/api/status` ma ok. 2 kB.

### A5. Zapas stosu nieznany

**Co widzę.** Główna pętla ma 8 192 B stosu (źródła: arduino-esp32 3.3.8,
`cores/esp32/main.cpp:15–19`). Na tym stosie pracuje rysowanie LVGL,
rozpakowywanie JSON i aktualizacja. Panel nigdzie nie wypisuje, ile z niego
zostaje — w `src/` nie ma `uxTaskGetStackHighWaterMark`. Centrala swój zapas
wypisuje (`hwm_tg`).

**Dlaczego to ważne.** Przepełnienie stosu wygląda jak losowy restart bez
przyczyny. Bez liczby nie da się powiedzieć, jak daleko do tego jest.

**Co zrobić.** Dopisać jedną liczbę do linii `tag=PANEL` (np.
`stos_min=…`). To kilka linii, bez wpływu na działanie.

### A6. Jedna prośba o pamięć bez sprawdzenia wyniku

**Co widzę.** `dashboard_lvgl_poc.cpp:1278–1281` — `malloc` i od razu zapis
pod otrzymany adres, bez sprawdzenia, czy pamięć została przydzielona
(zgłasza to cppcheck). Pozostałe dwa `malloc` w panelu wynik sprawdzają.

**Dlaczego to mało groźne.** Dzieje się tylko przy starcie, gdy pamięci jest
najwięcej.

**Co zrobić.** Sprawdzić wynik; przy braku pamięci pominąć efekt wciśnięcia
dla tego przycisku.

### A7. Napisy `String` składane przy każdym zapytaniu

**Co widzę.**

- `siec_nieblokujaca_LCD.h:250–260` — zapytanie HTTP jest sklejane
  z kawałków operatorem `+`: kilka próśb o pamięć na każde zapytanie,
  42 razy na minutę.
- `main.cpp:1939`, `3681`, `3714`, `3732` — treść polecenia sklejana tak
  samo (tylko przy dotyku).
- `main.cpp:3752` — odpowiedź na polecenie jest kopiowana dwa razy.

Druga klasa w tym samym pliku (`StrumienHttpGet`, linie 606–612) robi to
samo bez pamięci ze sterty — `snprintf` do tablicy na stosie.

**Co zrobić.** Zrobić w `NonBlockingHttp` tak jak w `StrumienHttpGet`.

### Narzędzia zgłosiły, a to nie błąd

- clang-analyzer: „Potential memory leak” w ArduinoJson
  (`VariantData.hpp:526`, `:540`). To `new` w miejscu już istniejącej pamięci
  (placement new), nie przydział. **Fałszywy alarm.**
- cppcheck: pola `PamiecAutorestartu` bez wartości początkowej. To pamięć,
  która ma przetrwać restart — celowo nie jest zerowana, ma sumę kontrolną.

---

## 7. B — drenaż (bateria, radio, procesor, ekran)

Uczciwie na początek: **prądu nie mierzyłem.** Poniżej jest to, ile pracy
panel sobie zadaje, i co z kodu wynika na pewno. Ile to miliamperów, pokaże
dopiero miernik (rozdział 12).

Rząd wielkości, żeby wiedzieć, co jest duże, a co małe. Liczby dla ESP32 są
z karty katalogowej układu (ESP32 Series Datasheet v5.3,
<https://documentation.espressif.com/esp32_datasheet_en.pdf> — sprawdzone
jednym wyszukiwaniem 2026-11-18). Dotyczą samego układu, nie całej płytki.

| Co | Prąd |
|---|---|
| ESP32: procesor 240 MHz pracuje, radio wyłączone (tak jest w przerwach między ramkami WiFi) | 30–68 mA |
| to samo przy 80 MHz | 20–31 mA |
| ESP32 w chwili nadawania | do 240 mA |
| ESP32 w chwili odbioru (**z pamięci**) | ok. 100 mA |
| ESP32 w lekkim śnie | 0,8 mA |
| ESP32 w głębokim śnie | 0,01 mA |
| Panel w głębokim śnie ze sterownikiem ekranu aktywnym / uśpionym (pomiar multimetrem zapisany w komentarzu, `main.cpp:4539–4540`) | ok. 62 mA / ok. 0,11 mA |
| Podświetlenie ekranu 3,5″ na 100 % (**z pamięci**; u Ciebie może być dużo mniej — patrz B5) | zwykle kilkadziesiąt do ponad 100 mA |

Z przedostatniego wiersza wynika, że aktywny sterownik ekranu to ok. 62 mA.
Zakładam, że podświetlenie było przy tym pomiarze zgaszone (kod gasi je
przed uśpieniem, `main.cpp:4533`).

Wniosek z tabeli: **najwięcej biorą ekran z podświetleniem i sam fakt, że
panel nigdy nie śpi.** Radio przy obecnym odpytywaniu to według mojego
szacunku kilka do kilkunastu miliamperów średnio. Dlatego kolejność poniżej
zaczyna się od snu i ekranu.

### B1. Na baterii panel nie zasypia, nie przygasza i nie reaguje na słabą baterię

**Co widzę.**

- Usypianie jest napisane (`idzSpac()`, `main.cpp:4517–4576`; 5 minut bez
  dotyku, `main.cpp:4399`), ale jedyne wywołanie jest w linii 5539 — **za**
  `return;` z linii 5508. W budowanej wersji pętla kończy się na 5508.
- Mówi o tym sam komentarz w kodzie (`main.cpp:5419–5423`): logika snu nie
  jest jeszcze wpięta w gałąź LVGL.
- Linker potwierdza: `idzSpac()` i `zarejestrujAktywnosc()` są na liście
  funkcji, do których nic się nie odwołuje.
- Poziom baterii jest tylko pokazywany (`main.cpp:5424–5450`) i raz
  sprawdzany przed aktualizacją (`diagnostyka_panelu.h:815`). Nic się nie
  dzieje, gdy spada do zera.

**Pomiar** (`pomiar_obciazenia.sh`, płytka na samej baterii, 3,90 V, 673 s
bez dotyku): panel nie usnął; podświetlenie 255/255; zmian jasności: 0.

**Dlaczego to ważne.**

- Na baterii panel pracuje pełną mocą, aż ogniwo się wyczerpie.
- Ogniwo litowe rozładowane za głęboko się niszczy. Czy coś je przed tym
  chroni, zależy od płytki ładowarki (czy ma układ zabezpieczający) — tego
  z kodu nie widać.
- Przy prawie pustej baterii ESP32 potrafi wpaść w pętlę: start → włączenie
  WiFi → spadek napięcia → restart.

**Zanim to się poprawi — decyzja, której nie podejmę za Ciebie.**
Panel ma brzęczyk alarmu dymu i przegrzania. **Śpiący panel nie pyta
centrali i nie zapiszczy.** Możliwe ustawienia, od najbezpieczniejszego:

| Wariant | Oszczędność | Alarm |
|---|---|---|
| 1. Tylko ekran: po N minutach bez dotyku podświetlenie na 0 i sterownik ekranu w uśpienie; dotyk albo alarm budzi. Radio i odpytywanie bez zmian | duża (ekran to największy odbiornik) | działa jak dziś |
| 2. Jak 1, plus rzadsze odpytywanie (np. co 10 s) i wolniejszy procesor | większa | działa, z opóźnieniem do 10 s |
| 3. Głęboki sen z budzeniem co kilka minut „na sprawdzenie alarmu” | największa przy pracy ciągłej | opóźnienie równe przerwie między budzeniami |
| 4. Głęboki sen, budzenie tylko przyciskiem (tak jak w starym kodzie) | największa | **nie działa, gdy panel śpi** |

Punkt odniesienia dla wariantów 3 i 4: według pomiaru zapisanego
w komentarzu (`main.cpp:4539–4540`) w głębokim śnie z uśpionym sterownikiem
ekranu płynęło ok. 0,11 mA. Nie wiem, w którym miejscu obwodu był miernik.

Osobno, niezależnie od wariantu: **przy baterii poniżej progu** (np. 3,3 V)
panel powinien wyłączyć ekran i radio i zasnąć do podłączenia ładowania.

**Co zrobić.** Wybrać wariant. Wariant 1 jest mały i bezpieczny: licznik
czasu od ostatniego dotyku już jest (`lvgl_dashboard_poc_bez_dotyku_ms()`,
`dashboard_lvgl_poc.cpp:7841`), komenda uśpienia sterownika ekranu też
(`main.cpp:4547`).

### B2. Co 2 sekundy panel rysuje od nowa 61 % ekranu, choć nic się nie zmieniło

**Co widzę.** Po każdym udanym odczycie statusu (`main.cpp:2999`) panel woła
`lvgl_dashboard_poc_aktualizuj()` (`dashboard_lvgl_poc.cpp:7516–7756`). Ta
funkcja **bez sprawdzania, czy coś się zmieniło**, ustawia od nowa
kilkanaście napisów, kolory ramek, ikony i paski zasięgu.

LVGL 8.3.11 każde takie ustawienie traktuje jako zmianę (**źródła**):

- `lv_label_set_text()` — `src/widgets/lv_label.c:85`: zwalnia stary napis,
  przydziela nowy, oznacza obiekt do przerysowania. Nie porównuje treści.
- `lv_obj_set_style_*()` — `src/core/lv_obj_style.c:270` i `:167`: zapisuje
  wartość i oznacza obiekt do przerysowania. Nie porównuje wartości.

**Pomiar.**

| Stan | Dziś | Z prototypem „ustawiaj tylko zmiany” |
|---|---|---|
| Pulpit, dane bez zmian, pompa stoi | 18,30 ekranu/min; 93 692 px na każdy odczyt statusu | 0,01 ekranu/min |
| Pulpit, pompa pracuje | 24,30 ekranu/min | 6,00 (sam obracający się wirnik) |
| Pulpit, jedna wartość zmienia się co 10 s | 18,30 ekranu/min | 0,19 |
| Centrala milczy | 2,48 ekranu/min | 2,45 |
| Otwarty ekran POMPA | 24,39 ekranu/min | 8,86 |

Od włączenia do końca pomiaru napis został ustawiony 4 439 razy, z czego
**4 145 razy na dokładnie ten sam tekst**.

**Co to znaczy na płytce.**

- Jeden piksel to 3 bajty po SPI (**źródła**: TFT_eSPI 2.5.43,
  `User_Setup_Select.h:194–199` — ILI9488 przez SPI pracuje w kolorze
  18-bitowym). 93 692 px to 281 kB co 2 s; **8,4 MB na minutę, ok. 12 GB na
  dobę.**
- Przy zegarze SPI 27 MHz samo wysłanie 281 kB trwa co najmniej 83 ms
  (**obliczone**, nie zmierzone), a TFT_eSPI wysyła w tym trybie piksel po
  pikselu (`Processors/TFT_eSPI_ESP32.c:495–501`). Do tego czas rysowania.
- **Wniosek:** co 2 sekundy panel przez mniej więcej 0,1–0,2 s nie czyta
  dotyku. Może to być jedna z przyczyn gubionych gestów, opisanych
  w `lvgl_port.cpp:70–91`. Do sprawdzenia na płytce: czy przesunięcia palcem
  działają pewniej, gdy centrala jest wyłączona (wtedy odczytów nie ma).

**Co zrobić.** W dashboardzie ustawiać napis, kolor i ikonę tylko wtedy, gdy
nowa wartość różni się od obecnej — trzy–cztery małe funkcje pomocnicze
i podmiana wywołań w `lvgl_dashboard_poc_aktualizuj()` oraz w funkcjach,
które ona woła. Prototyp pokazuje, ile to da; **prototyp nie jest poprawką**
(podmienia funkcje biblioteki przy linkowaniu, tylko w modelu na PC).

Po poprawce `./pomiar_obciazenia.sh` powinien pokazać w stanie A liczbę
bliską zera — to gotowy sprawdzian.

### B3. Procesor cały czas na 240 MHz, bez usypiania między zadaniami

**Co widzę.**

- Płytka `esp32dev` ma `f_cpu = 240000000` (plik płytki w pioarduino
  55.03.38-1), a rdzeń ustawia tę częstotliwość przy starcie
  (arduino-esp32 3.3.8, `cores/esp32/main.cpp:96–97`).
- W `src/` nie ma ani jednego wywołania, które by to zmieniało
  (`setCpuFrequencyMhz`, `esp_pm_configure`, `esp_light_sleep_start` — brak).
- Pętla czeka `delay(5)` (`main.cpp:5507`), czyli obraca się do 200 razy na
  sekundę. W tym czasie procesor nie zasypia.
- **Źródła:** biblioteki Arduino dla ESP32 są budowane bez zarządzania
  energią: `# CONFIG_PM_ENABLE is not set` jest w konfiguracji bibliotek dla
  IDF 5.1 (ESP32) oraz w pliku `sdkconfig.defaults` wygenerowanym dla Twojej
  centrali (ESP32-S3, ESP-IDF 5.5.4). Dla zwykłego ESP32 na IDF 5.5 nie
  sprawdziłem tego wprost, ale nic nie wskazuje, żeby było inaczej. Skutek:
  automatyczny lekki sen procesora jest w zwykłym projekcie Arduino
  niedostępny.

**Co zrobić — do wypróbowania, w tej kolejności.**

1. `board_build.f_cpu = 80000000L` w `platformio.ini` i pomiar prądu oraz
   płynności ekranu. WiFi wymaga co najmniej 80 MHz (**z pamięci**).
   Rysowanie będzie wolniejsze, dlatego **najpierw B2**.
2. Po B2: wydłużyć czekanie w pętli, gdy nic się nie dzieje
   (`lv_timer_handler()` zwraca, za ile milisekund ma być wołany ponownie).
3. Lekki sen automatyczny — wymaga własnej konfiguracji bibliotek
   (w pioarduino: `custom_sdkconfig`, tzw. tryb hybrid). **Ostrożnie:**
   w centrali ten tryb był próbowany i wycofany — opisują to uwagi w jej
   `platformio.ini`. Do rozważenia na samym końcu.

### B4. 42 połączenia z centralą na minutę

**Co widzę.** Status co 2 s, ustawienia co 5 s (`main.cpp:674–675`,
`5493–5505`). Każde zapytanie to osobne połączenie TCP: otwarcie, zapytanie,
odpowiedź, zamknięcie (`siec_nieblokujaca_LCD.h:250–252`, `HTTP/1.0`,
`Connection: close`).

**Pomiar:** 42 połączenia na minutę, czyli **60 480 na dobę**.

**Dlaczego to ważne — i dlaczego nie najważniejsze.**

- Każde połączenie budzi radio. Dopóki procesor i tak nie śpi (B3), kosztuje
  to mało — szacuję kilka do kilkunastu miliamperów średnio (**wniosek**).
- Zacznie się liczyć po wprowadzeniu snu: każde połączenie to pobudka.
- Ustawienia zmieniają się rzadko (ktoś musi je zmienić). Pobieranie ich co
  5 s to 12 z 42 połączeń i część próśb o pamięć z A2.
- Po stronie centrali każde zamknięte połączenie zostawia na 2 minuty wpis
  w tablicy połączeń; tablica ma 16 miejsc (**źródła**: `sdkconfig.defaults`
  centrali — `CONFIG_LWIP_TCP_MSL=60000`, `CONFIG_LWIP_MAX_ACTIVE_TCP=16`).
  Gdy miejsca brakuje, stos sieciowy wyrzuca najstarszy taki wpis
  (**z pamięci**). To działa, ale tablica centrali jest przez panel stale
  pełna.

**Co zrobić.**

1. Ustawienia: pobierać po starcie, po każdym własnym poleceniu (już tak
   jest, `main.cpp:3777`) i np. co 30–60 s zamiast co 5 s. Lepiej: niech
   `/api/status` niesie licznik zmian ustawień, a panel pobiera ustawienia
   tylko, gdy licznik się zmieni.
2. Status co 2 s zostawić przy włączonym ekranie; przy wygaszonym — rzadziej
   (zależy od decyzji z B1).

### B5. Podświetlenie

**Co widzę.**

- Jasność domyślna 100 % w dzień i 30 % w nocy, noc 22–6
  (`main.cpp:1038–1042`).
- Przygaszanie nocne zależy od godziny z centrali i od tego, czy jej zegar
  działa (`main.cpp:1090–1103`, `2913–2915`). Bez zegara centrali — zawsze
  jasność dzienna.
- Nie ma przygaszania po czasie bez dotyku (patrz B1).
- Przy każdym starcie ekran na chwilę świeci pełną mocą, zanim panel ustawi
  swoją jasność: `platformio.ini:55` (`TFT_BACKLIGHT_ON=HIGH`) każe
  bibliotece TFT_eSPI włączyć podświetlenie w `tft.init()` (**źródła**:
  `TFT_eSPI.cpp:786–789`). Kosmetyka, widoczna w nocy.

- Według uwag w kodzie podświetlenie jest podłączone do pinu procesora
  przez rezystor 100–220 Ω (`main.cpp:1009–1011`, `platformio.ini:47–48`).
  **Wniosek:** jeśli moduł ekranu nie ma własnego tranzystora, cały prąd
  podświetlenia płynie z pinu i jest mały — wtedy samo przygaszanie da
  niewiele, a oszczędność z wygaszania ekranu to głównie uśpienie jego
  sterownika (ok. 62 mA). Rozstrzygnie pomiar z rozdziału 12.
- Jasność 0 % i zachowanie przy alarmie — osobno, C9.

**Co zrobić.** Razem z B1 (wariant 1). Błysk przy starcie: usunąć
`-DTFT_BACKLIGHT_ON=HIGH` i sterować pinem samemu — do sprawdzenia, czy
biblioteka nie potrzebuje tej flagi do czegoś jeszcze.

### B6. Bez centrali w zasięgu: ciągłe szukanie i restart co 5 minut

**Co widzę.** Gdy centrali nie ma (wyłączona, panel wyniesiony):

- ponowne łączenie co najwyżej co 30 s (`main.cpp:834`, `4318–4389`); każda
  próba to szukanie sieci;
- samoczynny restart po 45 s, potem po 90 s, 180 s i dalej **co 300 s bez
  końca** (`diagnostyka_panelu.h:489–496`);
- po każdym restarcie pełny skan i do 20 s łączenia (`main.cpp:4247–4311`).

**Dlaczego to ważne.** Na zasilaniu z USB to dobre zachowanie — panel sam
wraca. Na baterii to najdroższy stan: radio szuka prawie bez przerwy, a ekran
świeci.

**Co zrobić.** Na baterii (i tylko na baterii): po kilku nieudanych próbach
wydłużać przerwy do kilku minut i wygasić ekran. Zależy od decyzji z B1.

### B7. Nadajnik WiFi na pełnej mocy

**Co widzę.** W `src/` nie ma `WiFi.setTxPower()` — zostaje moc domyślna,
20 dBm (**źródła**: `CONFIG_ESP_PHY_MAX_WIFI_TX_POWER=20`). Panel stoi kilka
metrów od centrali.

**Dlaczego to drobne.** Panel nadaje krótko. Zysk będzie mały; widać go
najwyżej na mierniku.

**Co zrobić.** Jedna linia do wypróbowania po połączeniu, np.
`WiFi.setTxPower(WIFI_POWER_8_5dBm)`, i sprawdzenie, czy `rssi` w logu
centrali i liczba błędów statusu się nie pogarszają.

Dla porządku — rzeczy, które **są** ustawione dobrze bez Twojego udziału
(**źródła**, arduino-esp32 3.3.8): usypianie radia między ramkami jest
domyślnie włączone (`WiFiGeneric.cpp:378–382`, `WIFI_PS_MIN_MODEM`); pamięć
Bluetooth jest zwalniana, gdy Bluetooth nie jest używany
(`esp32-hal-misc.c:346–347`).

### B8. Zapis do pamięci flash — bez zastrzeżeń, jedna uwaga

**Co sprawdziłem.** Wszystkie zapisy do NVS w panelu:

| Co | Kiedy | Ocena |
|---|---|---|
| Kalibracja dotyku (`main.cpp:3890–3893`) | raz | w porządku |
| Jasność (`main.cpp:1134`) | po puszczeniu suwaka, nie w trakcie | w porządku |
| Godziny przygaszania (`main.cpp:1148`, `1151`) | przy każdym kroku +/−, także przy przytrzymaniu (10 razy na sekundę) | najwyżej 23 różne wartości na jedno przytrzymanie — bez znaczenia |
| Stan aktualizacji (`main.cpp:4658–4667`) | tylko w trakcie aktualizacji | w porządku |

**Nic nie zapisuje do flash cyklicznie.** Zużycia pamięci flash się nie
obawiam.

**Uwaga (źródła, arduino-esp32 3.3.8).** Biblioteka WiFi domyślnie trzyma
dane sieci — także hasło — w NVS (`WiFiGeneric.cpp:376`, `_persistent =
true`) i przekazuje je sterownikowi przy każdym `WiFi.begin()` (`STA.cpp`,
funkcja `connect`). Czy sterownik zapisuje je za każdym razem, czy tylko po
zmianie — nie sprawdzałem. Panel ma hasło wpisane w program i tej kopii nie
potrzebuje.

**Co zrobić.** `WiFi.persistent(false);` przed `WiFi.mode(WIFI_STA)`
(`main.cpp:4248`). Do potwierdzenia w internecie, że nie zmienia to niczego
poza zapisem.

### B9. Po alarmie animacje ekranów POMPA i SERWO chodzą dalej w tle

**Co widzę.** Animacje ekranu POMPA zatrzymuje tylko `pompa_panel_zamknij()`
(`dashboard_lvgl_poc.cpp:1165`), a ekranu SERWO tylko `serwo_panel_zamknij()`.
Gdy na otwartym ekranie POMPA pojawi się alarm, ekran alarmu kasuje liczniki
bezczynności (`3362`), ale tych animacji nie zatrzymuje. Po zamknięciu alarmu
panel wraca na pulpit, a animacje niewidocznego ekranu liczą się dalej — do
następnego otwarcia i zamknięcia ekranu POMPA.

**Dlaczego to drobne.** Niewidoczny ekran nie jest rysowany; to tylko trochę
pracy procesora. Pamięć nie przybywa.

**Co zrobić.** W `alarm_ekran_pokaz()` zatrzymać animacje obu ekranów.

---

## 8. C — błędy i słabe miejsca w kodzie

### C1. Usypianie i bateria są tylko w martwej części pliku

**Co widzę.** `main.cpp` zawiera dwa programy: nowy (LVGL) i stary (rysowany
ręcznie przez TFT_eSPI). Przełącza je `LVGL_POC_TEST 1` (`main.cpp:603`).
W `setup()` i w `loop()` nowy program kończy się na `return;` (linie 5380
i 5508), a reszta funkcji to stary program, do którego wykonanie nie dochodzi
(cppcheck: „unreachable code”, linie 5383 i 5511).

Stary program miał usypianie. Nowy go nie dostał — stąd B1.

**Ile tego jest (linker + liczenie linii).**

| Co | Rozmiar |
|---|---|
| Funkcje `main.cpp`, do których nic się nie odwołuje: stary interfejs (`rysuj*`, `obsluzDotyk`, `tresciKafelka`, `animujWcisniecie`…), `idzSpac`, `zarejestrujAktywnosc`, `wyslijUstawienie`, `touchReadChannelStabilnie` | 28 funkcji, ok. 600 linii kodu |
| Symulacja wyłączona przełącznikiem `LVGL_POC_SYMULACJA 0` (`main.cpp:2050–2399`) | ok. 350 linii |
| Komentarz na początku `main.cpp` (linie 1–544) | 544 linie |

`src/ikony.h` (2 350 linii) **nie** jest martwy: wszystkie 51 obrazków
używa nowy program przez `ikony_lvgl.h`.

Na płytkę martwy kod nie trafia (linker go usuwa), więc nie kosztuje pamięci.
Kosztuje czytanie: łatwo poprawić coś w części, która nie działa, i uznać
sprawę za załatwioną — tak jak z usypianiem.

**Co zrobić.** Po zrobieniu B1 w nowym programie usunąć stary: osobny commit,
bez żadnej innej zmiany, sprawdzony tym, że testy i rozmiar programu się nie
zmieniają.

### C2. Platforma nieprzypięta — i co się dzieje na innej wersji

**Co widzę.** `platformio.ini:29` panelu: `platform = espressif32`, bez
wersji. PlatformIO bierze wtedy to, co jest zainstalowane. `platformio.ini`
centrali ma pełny adres pioarduino `55.03.38-1`, więc na Twoim komputerze
panel najpewniej buduje się tym samym (rdzeń 3.3.8). To przypadek, nie
ustawienie.

**Co by było na innej wersji.**

| Platforma | Rdzeń | Skutek dla panelu |
|---|---|---|
| oficjalna do 7.1.3 | 2.0.17 | **nie skompiluje się**: kod używa `ledcAttach` (`main.cpp:5203`, `1305`), którego tam nie ma (sprawdzone w `esp32-hal-ledc.h` 2.0.17), i `esp_task_wdt_reconfigure` (`main.cpp:5162`) |
| każda z rdzeniem 3.0.x – 3.2.0 (starsze wydania pioarduino) | sprawdzone: 3.0.7, 3.1.0, 3.1.3, 3.2.0 | skompiluje się, ale **po pierwszym alarmie ekran przygaśnie do ok. jednej czwartej i tak zostanie do restartu** (wniosek z kodu rdzenia — nie uruchamiałem) |
| pioarduino 55.03.38-1 (ta z centrali), oficjalna 8.0.0 | 3.3.8, 3.3.12 | bez tego kłopotu (sprawdzone w kodzie 3.2.1, 3.3.0 i 3.3.8). 3.3.8 to najpewniej wersja, którą budujesz dziś; na 3.3.12 panelu nikt nie próbował |

**Skąd przygaszanie (źródła, `cores/esp32/esp32-hal-ledc.c`).** Podświetlenie
i brzęczyk to dwa wyjścia PWM, podłączane po kolei (`main.cpp:5203`, potem
`1305`). W rdzeniu do 3.2.0 dwa kolejne wyjścia dostają **ten sam
generator** (`timer = (channel / 2) % 4`). `ledcWriteTone()` brzęczyka
przestawia ten generator na inną częstotliwość i rozdzielczość 10 bitów,
a podświetlenie dalej zapisuje wartości 0–255, czyli najwyżej jedną czwartą
nowego zakresu 0–1023. Od 3.2.1 każde wyjście
o innych ustawieniach dostaje własny generator (`find_matching_timer` /
`find_free_timer`).

**Co zrobić.**

1. W `platformio.ini` panelu wpisać tę samą linię `platform = …` co
   w centrali.
2. Biblioteki przypiąć dokładnie (bez `^`): LVGL, ArduinoJson, TFT_eSPI.
3. Dopisać w README panelu, jaką platformą jest budowany.

### C3. Kalibracja dotyku nie „karmi” watchdoga

**Co widzę.** `setup()` najpierw włącza watchdog pętli na 15 s z restartem
(`main.cpp:5157–5170`), a dopiero potem woła kalibrację dotyku (`5230`).
Kalibracja czeka na palec w pętlach `while … delay(10)` (`main.cpp:3839`,
`3845`, `3856`, `3862`) i ani razu nie woła `esp_task_wdt_reset()`.

**Skutek.** Na płytce bez zapisanej kalibracji (pierwsze wgranie, wgranie
z kasowaniem pamięci) cała kalibracja musi się zmieścić w ok. 14 sekundach.
Kto czyta napisy na ekranie, nie zdąży — panel się restartuje i zaczyna od
nowa, w kółko. W logu: powód restartu `task_wdt`.

**Dlaczego dotąd nie przeszkadzało.** Twoja płytka ma kalibrację zapisaną,
a aktualizacja przez centralę jej nie kasuje.

**Co zrobić.** Wołać `esp_task_wdt_reset()` w czterech pętlach czekania albo
włączać watchdog po kalibracji.

**Pewność:** kod. Nie uruchamiałem tego — atrapa dotyku w modelu nie umie
udawać „braku palca”.

### C4. Polecenia z ekranu giną; odświeżanie nadpisuje wartość w trakcie zmiany

**Co widzę — część pierwsza.** Wszystkie polecenia idą jednym kanałem
(`komendaFetch`). Gdy poprzednie polecenie jest jeszcze w drodze, nowe jest
**pomijane** — zostaje po nim tylko linia w logu:

- `main.cpp:3676–3679` (`wyslijTrybPracy`) — tędy idą wszystkie ustawienia
  pompy, serwa, dymu i progu alarmu (`2004–2048`);
- `main.cpp:1934–1937`, `1957–1960`, `1973–1976`, `1987–1990` — pompa ręcznie,
  tryb pompy, tryb serwa, pozycja serwa.

W dwóch miejscach panel **najpierw** zapisuje nową wartość u siebie,
a dopiero potem sprawdza, czy może ją wysłać (`main.cpp:1956`, `1972`). Ekran
pokazuje wtedy nowy tryb, a centrala o nim nie wie.

**Część druga.** Przyciski + i − wysyłają polecenie przy każdym kliknięciu
**i co 100 ms przy przytrzymaniu** (`dashboard_lvgl_poc.cpp:2659`, `2677`,
`2759`, `2777`, `3043`, `3061`; 100 ms to domyślna wartość LVGL). Przy
przytrzymaniu większość poleceń trafia na zajęty kanał. Jeśli odpadnie
**ostatnie**, na ekranie jest 60, a w centrali 58.

**Część trzecia.** Po każdym poleceniu i co 5 s panel pobiera ustawienia
i **wpisuje je na otwarty ekran** (`dashboard_lvgl_poc.cpp:7085–7101`,
`7118–7129`), bez sprawdzania, czy użytkownik właśnie coś zmienia. Wartość
pod palcem może więc wrócić do starszej. Stary program miał na to
zabezpieczenie (`main.cpp:5549`: „nie nadpisuj ekranu w trakcie edycji”);
nowy go nie ma.

**Jak to zobaczyć u siebie.** Przytrzymaj + przez 3 sekundy na ekranie
ustawień pompy i patrz w log. Każda linia
`poprzednia komenda jeszcze w toku, pomijam` to jedno zgubione polecenie.
Po 5 s porównaj liczbę na ekranie z tą w centrali.

**Co zrobić.**

1. Zamiast pomijać — zapamiętać **ostatnią** wartość dla danego pola
   i wysłać ją, gdy kanał się zwolni.
2. Przy przytrzymaniu + / − zmieniać liczbę na ekranie, a polecenie wysłać
   raz, po puszczeniu.
3. Nie nadpisywać pola, które było zmieniane w ostatnich kilku sekundach
   albo ma niewysłaną wartość.
4. Gdy polecenie się nie uda — pokazać to na ekranie, nie tylko w logu.

**Pewność:** kod. Częstość zależy od czasu odpowiedzi centrali — tego model
nie oddaje.

### C5. 16 odwołań do tabeli ustawień bez sprawdzenia

**Co widzę.** `main.cpp:3084–3113`: szesnaście razy
`POLA[znajdzIndeksPola("nazwa")]`. `znajdzIndeksPola` zwraca −1, gdy nazwy
nie ma (`main.cpp:1550`), a −1 jako indeks to czytanie pamięci sprzed tabeli.

**Dziś nie ma błędu:** wszystkie 16 nazw jest w tabeli (sprawdzone skryptem).
Błąd pojawi się po literówce albo zmianie nazwy pola — i nic go nie zgłosi.

**Co zrobić.** Jedna funkcja pomocnicza, która przy −1 zwraca 0 i wypisuje
ostrzeżenie.

### C6. Dwie klasy sieciowe, dwa sposoby

`siec_nieblokujaca_LCD.h` ma dwie klasy do zapytań HTTP. Nowsza
(`StrumienHttpGet`) jest staranniejsza od starszej (`NonBlockingHttp`):

| | `NonBlockingHttp` | `StrumienHttpGet` |
|---|---|---|
| Składanie zapytania | `String` z `+` (250–260) | `snprintf` do tablicy (606–612) |
| Wysłano tylko część zapytania | niesprawdzane (261–281) | sprawdzane (619–623) |
| Limit rozmiaru odpowiedzi | brak (A4) | linia najwyżej 300 znaków |

Zapytania panelu mają po sto kilkadziesiąt bajtów, więc częściowa wysyłka
w praktyce się nie zdarza — to porządek, nie awaria.

W obu klasach jest `errno == EWOULDBLOCK || errno == EAGAIN` (linie 276, 311,
615, 643). Na ESP32 i na PC to ta sama liczba, więc kompilator zgłasza
„dwa razy to samo”. Nieszkodliwe.

**Co zrobić.** Przenieść trzy rzeczy z nowszej klasy do starszej.

### C7. Drobne porządki

- **Nieużywane stałe:** `CENTRALA_HOST`, `CENTRALA_PORT`, `HTTP_TIMEOUT_MS`
  (`main.cpp:653`, `654`, `676`). Adres centrali jest wpisany w trzech
  miejscach: tu, w `WIFI_GATEWAY` (`662`) i w
  `siec_nieblokujaca_LCD.h:70`. Używane są dwa ostatnie.
- **Zdublowany fragment:** w `aktualizujSezon()` gałęzie `main.cpp:1604–1610`
  i `1617–1623` są identyczne.
- **Nieużywane funkcje w dashboardzie:** `serwo_wybor_zbuduj` (`2228`)
  i `serwo_wybor_otworz_cb` (`2124`) — ekran, który nie jest budowany.
- **Opis niezgodny z kodem:** `LCD-Centrali/README.md` (część
  o aktualizacji) mówi, że „usypianie panelu jest na czas pobierania
  wyłączone”. Panel w tej wersji w ogóle nie usypia (B1).
- **Dwie literówki w komentarzach:** `main.cpp:475` („dogĺebny”),
  `lvgl_port.h:10` (rozsunięte „t   ą”). Bez wpływu na program.

### C8. Ostrzeżenia kompilatorów — co jest szumem

`g++` z 29 dodatkowymi grupami ostrzeżeń: 207 ostrzeżeń w plikach panelu.

| Rodzaj | Ile | Ocena |
|---|---|---|
| zamiana typów liczb (`-Wconversion`, `-Wsign-conversion`, `-Wfloat-conversion`, `-Wdouble-promotion`, `-Wuseless-cast`) | 141 | szum; przejrzane wyrywkowo |
| `snprintf` może obciąć tekst (`-Wformat-truncation`) | 44 | **nie błąd**: wszystkie 82 `snprintf` w panelu podają rozmiar bufora; obcięcie groziłoby tylko przy liczbach, których centrala nie wysyła |
| nieużywany parametr | 8 | szum |
| „dwa razy to samo” w warunku | 4 | C6 |
| pozostałe | 10 | nieużywane funkcje i stała (C7); reszta w martwym kodzie albo kosmetyczna |

W całym `src/` **nie ma** `sprintf`, `strcpy`, `strcat`, `strncpy` ani `gets`.

clang-tidy: 73 razy „pominięty wynik funkcji” (to `snprintf` — celowo);
jedno zgłoszenie wyjścia poza tabelę w `main.cpp:3550` — w martwym kodzie
starego interfejsu (ten sam wzorzec co C5).

### C9. Jasność można ustawić na 0 %; alarm nie rozjaśnia ekranu

**Co widzę.**

- Suwaki jasności dziennej i nocnej mają zakres 0–100
  (`dashboard_lvgl_poc.cpp:3677`), a kod nie pilnuje dolnej granicy
  (`main.cpp:1118–1136`). Przy 0 % podświetlenie gaśnie całkiem
  (`main.cpp:1012–1015`).
- Wartość jest zapisywana w pamięci panelu po puszczeniu suwaka
  (`main.cpp:1133–1135`) i wraca po restarcie (`main.cpp:5212`).
- Jasność zmieniają tylko: suwak, harmonogram dzień/noc i start
  (`main.cpp:1094`, `1101`, `1132`, `5212`). **Alarm jej nie zmienia.**

**Skutek.**

- Jasność dzienna na 0 %: czarny ekran, także po restarcie. Wrócić można
  tylko przesuwając suwak na ślepo.
- Jasność nocna na 0 % (ktoś chce mieć w nocy ciemno): w nocy ekranu alarmu
  dymu albo przegrzania nie widać. Brzęczyk działa.

**Co zrobić.** Dolna granica suwaków (np. 5 %) i pełna jasność na czas
alarmu. Robić razem z B1 — to ten sam fragment kodu, a wygaszanie ekranu
z B1 i tak musi umieć obudzić ekran przy alarmie.

**Pewność:** kod. Nie uruchamiałem. Zakładam, że podświetleniem steruje
tylko ten jeden pin — tak mówią uwagi w kodzie.

---

## 9. D — kodowanie znaków

Sprawdziłem wszystkie 67 plików tekstowych w `LCD-Centrali/` (źródła, testy,
opisy, konfiguracja).

| Co | Wynik |
|---|---|
| Kodowanie | wszystkie pliki to poprawny UTF-8 |
| Znacznik BOM na początku | w żadnym |
| Końce linii | LF we wszystkich plikach projektu. CRLF mają tylko dwa szablony wygenerowane przez PlatformIO (`include/README`, `lib/README`) — nie są częścią programu |
| Tabulatory, spacje na końcach linii | brak |
| Znaki spoza ASCII **w napisach programu** | tylko `°` — 7 miejsc w `dashboard_lvgl_poc.cpp` (5265, 7640–7676) |
| Polskie litery w napisach programu | brak — napisy na ekranie i w logu są pisane bez ogonków |
| Znaki „krzaczki” po złym przekodowaniu | brak |

`°` jest we wbudowanych czcionkach LVGL (Montserrat 14, 20, 28 —
`lv_conf.h:142–144`), więc wyświetla się poprawnie. W UTF-8 zajmuje 2 bajty;
bufory w tych miejscach mają 16 bajtów, a najdłuższy realny napis
(„-12.5°C”) — 8.

**Uwaga na przyszłość.** Polskich liter (ą, ę, ł…) we wbudowanych czcionkach
LVGL nie ma. Gdybyś chciał je mieć na ekranie, trzeba wygenerować własną
czcionkę — temat w rozdziale 10.

---

## 10. Czego poszukać w internecie

Zapytania podaję po angielsku, bo tak są pisane dokumentacja i zgłoszenia
błędów. Przy każdym temacie: **od czego zależy decyzja**.

### Już sprawdzone w źródłach — nie trzeba szukać

| Fakt | Gdzie |
|---|---|
| LVGL 8.3.11 nie porównuje napisu ani wartości stylu przed ustawieniem | `lvgl` v8.3.11: `src/widgets/lv_label.c:85`, `src/core/lv_obj_style.c:167`, `:270` |
| ILI9488 przez SPI w TFT_eSPI = 3 bajty na piksel, wysyłane piksel po pikselu | TFT_eSPI V2.5.43: `User_Setup_Select.h:194–199`, `Processors/TFT_eSPI_ESP32.c:418`, `:495` |
| Wspólny generator PWM dla dwóch kolejnych wyjść w rdzeniu 3.0.7–3.2.0, osobny od 3.2.1 | arduino-esp32 `cores/esp32/esp32-hal-ledc.c`, wersje 3.0.7, 3.1.0, 3.1.3, 3.2.0, 3.2.1, 3.3.0, 3.3.8 |
| Usypianie radia domyślnie włączone | arduino-esp32 3.3.8 `libraries/WiFi/src/WiFiGeneric.cpp:378–382` |
| Dane WiFi domyślnie trzymane w NVS | tamże, linia 376; `STA.cpp`, `connect()` |
| Stos pętli 8 192 B; 240 MHz z pliku płytki | arduino-esp32 3.3.8 `cores/esp32/main.cpp:15–19`, `96–97`; `boards/esp32dev.json` w pioarduino 55.03.38-1 |
| `String`: krótkie napisy (do 13 znaków) bez sterty; `= ""` nie zwalnia bufora | arduino-esp32 3.3.8 `cores/esp32/WString.cpp`, `WString.h:319–330` |
| pioarduino 55.03.38-1 = rdzeń 3.3.8; oficjalna 8.0.0 = 3.3.12; 7.1.3 = 2.0.17 | `platform.json` w obu repozytoriach platform |
| Biblioteki Arduino bez `CONFIG_PM_ENABLE`; lwIP: 16 gniazd, 16 połączeń TCP, `TCP_MSL` 60 s; zrzut awarii do flash włączony | `espressif/esp32-arduino-libs` (gałąź IDF 5.1) `esp32/sdkconfig`; `esp32-arduino-lib-builder` `configs/defconfig.common`; `sdkconfig.defaults` centrali (ESP-IDF 5.5.4, ESP32-S3) |
| Prąd ESP32: 30–68 mA przy 240 MHz i 20–31 mA przy 80 MHz (radio wyłączone), 0,8 mA w lekkim śnie, 0,01 mA w głębokim, do 240 mA przy nadawaniu | ESP32 Series Datasheet v5.3, <https://documentation.espressif.com/esp32_datasheet_en.pdf> |

### W — hasła

1. **Hasła poza plikiem programu.**
   `ESP32 store wifi password NVS instead of hardcoding`,
   `ESP32 NVS provisioning secrets nvs_partition_gen`,
   `ESP32 NVS encryption without flash encryption`.
   *Decyzja:* czy hasła mają być wpisywane raz kablem do pamięci urządzenia,
   a plik programu ma być bez haseł.
2. **Sprawdzenie pliku programu po zbudowaniu.**
   `platformio extra_scripts post action check firmware.bin strings`.
   *Decyzja:* czy budowanie ma się zatrzymać, gdy w pliku `.bin` jest hasło.
3. **Aktualizacja z prywatnego źródła.**
   `ESP32 OTA github private repository release asset token`,
   `esp_encrypted_img pre-encrypted OTA esp32`,
   `ESP32 OTA firebase storage authenticated download`.
   *Decyzja:* skąd centrala ma brać plik aktualizacji.
4. **Usuwanie haseł z historii repozytorium.**
   `github removing sensitive data from a repository git filter-repo`.

### A — pamięć

5. **ArduinoJson 7 i pamięć.**
   `ArduinoJson 7 DeserializationOption::Filter memory`,
   `ArduinoJson 7 custom allocator`,
   `ArduinoJson 7 heap fragmentation ESP32`.
   *Decyzja:* filtr w panelu czy krótsza odpowiedź z centrali (A2).
6. **LVGL: własny worek pamięci czy wspólny.**
   `LVGL 8 LV_MEM_CUSTOM 0 vs 1 ESP32 fragmentation`,
   `lv_mem_monitor LVGL 8 used_pct frag_pct`.
   *Decyzja:* czy przejść na `LV_MEM_CUSTOM 0` ze stałym rozmiarem (A3).
7. **Jak szukać wycieku na płytce.**
   `ESP32 heap_caps_get_largest_free_block fragmentation monitor`,
   `arduino-esp32 heap tracing CONFIG_HEAP_TRACING`,
   `uxTaskGetStackHighWaterMark arduino loopTask`,
   `SET_LOOP_TASK_STACK_SIZE arduino-esp32`.
   *Decyzja:* co dopisać do linii `tag=PANEL` (A5).
8. **Zrzut po awarii.** Partycja `coredump` jest w `partitions.csv`,
   a zapis zrzutu jest w bibliotekach włączony.
   `ESP32 core dump flash read esp-coredump platformio`,
   `platformio monitor_filters esp32_exception_decoder`.
   *Decyzja:* czy po losowym restarcie umiemy odczytać, co się stało.
9. **Bufory WiFi a sterta.**
   `arduino-esp32 WiFi.useStaticBuffers dynamic rx buffer heap`.

### B — drenaż

10. **Ile bierze ESP32 w każdym trybie.** Częściowo zrobione: liczby
    z tabeli w rozdziale 7 zgadzają się z kartą katalogową v5.3. Zostaje:
    `ESP32 datasheet RX current 802.11`,
    `ESP32 modem sleep average current DTIM WiFi connected`.
11. **Wolniejszy procesor przy włączonym WiFi.**
    `ESP32 setCpuFrequencyMhz 80 WiFi stable`,
    `platformio board_build.f_cpu 80000000L esp32 wifi`.
    *Decyzja:* B3, krok 1.
12. **Lekki sen automatyczny w projekcie Arduino.**
    `arduino-esp32 CONFIG_PM_ENABLE esp_pm_configure light sleep`,
    `pioarduino custom_sdkconfig CONFIG_PM_ENABLE tickless idle`,
    `ESP32 WiFi.setSleep WIFI_PS_MAX_MODEM listen interval`.
    *Decyzja:* B3, krok 3.
13. **Głęboki sen z ekranem i dotykiem.**
    `ESP32 deep sleep gpio_hold_en gpio_deep_sleep_hold_en`,
    `ILI9488 sleep in 0x10 current`,
    `ILI9488 3.5 inch SPI module LED pin backlight current transistor`,
    `XPT2046 power down mode PD0 PD1`,
    `ESP32 devkit deep sleep current AMS1117 CP2102`,
    `ESP32 deep sleep timer wakeup wifi reconnect time static ip bssid`.
    *Decyzja:* warianty 3 i 4 z B1; ile realnie bierze Twoja płytka we śnie.
14. **Bateria.**
    `TP4056 DW01 over-discharge protection cutoff voltage`,
    `LiPo 1S safe cutoff voltage 3.3V`,
    `ESP32 brownout reset loop low battery ESP_RST_BROWNOUT`.
    *Decyzja:* próg „za słaba bateria” w B1.
15. **LVGL bez zbędnego rysowania.**
    `LVGL lv_label_set_text same text redraw invalidate`,
    `LVGL 8 lv_timer_handler return value sleep`,
    `LVGL lv_disp_get_inactive_time backlight dim`,
    `LVGL LV_USE_PERF_MONITOR`.
    *Decyzja:* jak zrobić B2 i wariant 1 z B1 tak, jak zaleca dokumentacja.
16. **Szybsze wysyłanie do ILI9488.**
    `TFT_eSPI ILI9488 SPI 18 bit DMA ESP32`,
    `ILI9488 SPI max frequency 40MHz`,
    `LovyanGFX ILI9488 LVGL ESP32 vs TFT_eSPI`,
    `TFT_eSPI arduino-esp32 3.x compatibility`.
    *Decyzja:* czy zostać przy TFT_eSPI (bez wydań od 2024-03).
17. **Mniej połączeń.**
    `ESPAsyncWebServer keep-alive HTTP/1.1`,
    `ESPAsyncWebServer AsyncEventSource server sent events ESP32 client`,
    `lwIP TIME_WAIT TCP_MSL MEMP_NUM_TCP_PCB esp32`.
    *Decyzja:* B4 — rzadsze pytanie czy powiadomienia z centrali.
18. **Moc nadajnika i zapis danych WiFi.**
    `WiFi.setTxPower WIFI_POWER_8_5dBm esp32 range`,
    `WiFi.persistent(false) esp32 arduino 3.x`.

### C — kod

19. **Przypinanie wersji.**
    `platformio.ini pin platform version pioarduino`,
    `platformio lib_deps exact version without caret`.
20. **Watchdog w `setup()`.**
    `esp_task_wdt_add loopTask setup long blocking esp_task_wdt_reset`.
21. **Kolejka poleceń „wygrywa ostatnie”.**
    `embedded command queue last write wins coalesce`,
    `LVGL LV_EVENT_LONG_PRESSED_REPEAT send on release`.
22. **Analiza kodu przy każdym budowaniu.**
    `platformio check cppcheck clang-tidy`,
    `platformio build_flags -Wall -Wextra esp32`.

### D — znaki

23. **Polskie litery na ekranie.**
    `LVGL font converter polish characters range`,
    `lv_font_conv montserrat 0x104-0x17C`.

---

## 11. Kolejność, którą proponuję

Każdy krok to osobne wydanie i osobne sprawdzenie. Po każdym — testy na PC
i próba na płytce.

| Krok | Co | Ustalenia | Wielkość | Ryzyko |
|---|---|---|---|---|
| 0 | Hasła | W | — | — |
| 1 | **Rzeczy pewne i małe, bez zmiany zachowania:** przypięcie platformy i bibliotek; zapas stosu w logu; watchdog w kalibracji; limit odpowiedzi; sprawdzenie `malloc`; zabezpieczenie tabeli ustawień; `WiFi.persistent(false)` | C2, A5, C3, A4, A6, C5, B8 | mała | małe |
| 2 | **Rysuj tylko zmiany.** Sprawdzian: `pomiar_obciazenia.sh`, stan A blisko zera | B2 (i część A2) | średnia | małe — jest pomiar przed i po |
| 3 | **Ekran gaśnie po czasie bez dotyku** (wariant 1 z B1, jeśli go wybierzesz); alarm zawsze rozjaśnia ekran, jasność ma dolną granicę; rzadsze ustawienia | B1, B5, C9, B4 | średnia | średnie — wymaga prób na płytce |
| 4 | **Polecenia nie giną** | C4 | średnia | średnie |
| 5 | **Decyzja o śnie na baterii** i próg słabej baterii; zachowanie bez centrali | B1, B6 | zależy od wariantu | wymaga Twojej decyzji i pomiarów prądu |
| 6 | Wolniejszy procesor, moc nadajnika — z miernikiem w ręku | B3, B7 | mała | trzeba mierzyć |
| 7 | Krótsza odpowiedź centrali albo filtr | A2 | średnia (dotyka centrali) | średnie |
| 8 | Usunięcie starego programu z `main.cpp` | C1, C7 | duża objętościowo, prosta | małe |

Kroki 1 i 2 można zrobić od razu po sprawdzeniu tematów 15 i 19–20
z rozdziału 10. Krok 5 czeka na Twoją decyzję.

---

## 12. Co możesz zmierzyć sam

### Prąd (miernik w szereg z baterią, zakres 10 A)

Na zakresie miliamperowym miernik sam zabiera sporo napięcia i panel może
się przez to restartować — stąd zakres 10 A.

Wpisz wyniki — to rozstrzygnie, co naprawdę warto robić w krokach 3, 5 i 6.

| Stan | Jak go uzyskać | Prąd [mA] |
|---|---|---|
| Pulpit, jasność 100 % | zwykła praca | |
| Pulpit, jasność 30 % | suwak na ekranie Wyświetlacz | |
| Pulpit, jasność 0 % | w dzień: przesuń suwak jasności dziennej do zera i **nie puszczaj palca**; odczytaj miernik; wróć palcem w prawo i dopiero puść | |
| Centrala wyłączona, panel szuka | wyłącz centralę, odczekaj minutę | |
| Odłączone USB i bateria prawie pusta | — | tylko obserwacja: czy panel się restartuje w kółko |

Przy trzecim wierszu nie puszczaj palca na zerze: panel zapamiętałby 0 %
i ekran zostałby czarny (C9). Jasność zmienia się już w trakcie przesuwania,
a zapis następuje dopiero po puszczeniu.

Różnica między pierwszym a trzecim wierszem to podświetlenie. Trzeci wiersz
pomniejszony o prąd sterownika ekranu (ok. 62 mA według pomiaru zapisanego
w kodzie) to mniej więcej to, co zostanie po wariancie 1 z B1.

### Pamięć (z logu)

- Linia `tag=PANEL …` co 30 s: zapisz `ram`, `ram_min`, `ram_blok` na
  początku i po 24 godzinach.
- Po kilku wykresach: `ram_min` poniżej 8 000 — przyślij log.

### Polecenia (C4)

- Przytrzymaj + przez 3 s; policz linie
  `poprzednia komenda jeszcze w toku, pomijam`.

### Rysowanie (B2)

- Czy przesunięcia palcem działają pewniej przy wyłączonej centrali?

---

## 13. Czego ten audyt nie obejmuje

- **Centrali** (`main_centrala.cpp`, 50 063 linie). Jej ostatni audyt
  (2026-10-22) dotyczył wersji 3.54.0; od tamtej pory doszły wersje 3.58.x.
  Z panelem styka się w trzech miejscach opisanych wyżej: długość odpowiedzi
  `/api/status` (A2), liczba połączeń (B4) i plik aktualizacji (W).
- **Aplikacji na telefon** — ma własne audyty z 2026-10-10 i 2026-10-22.
- **Sprzętu**: schematu, ładowarki, zabezpieczenia ogniwa, prądu płytki we
  śnie.
- **Zachowania na prawdziwej płytce.** Wszystko, co oznaczone jako
  „wniosek”, trzeba tam potwierdzić.

---

## Załącznik A. Pomiar obciążenia

Narzędzie: `LCD-Centrali/testy_host/pomiar_obciazenia.sh` (opis:
`testy_host/README.md`, „Pomiar obciążenia w spoczynku”).

```
./pomiar_obciazenia.sh              # stan obecny
./pomiar_obciazenia.sh --prototyp   # + szacunek po zmianie „ustawiaj tylko zmiany”
```

Wynik z 2026-11-18, firmware v1.4.0 (skrócone kolumny):

```
== stan obecny
A pulpit, dane bez zmian, pompa stoi             | 120 s | 18.30 ekranu/min | 2810760 px/min |  8.43 MB SPI/min |  93692 px na odczyt statusu = 61 % | 42.0 zapytan/min | 6806 prosb o pamiec/min (547 kB)
B pulpit, dane bez zmian, pompa pracuje (wirnik) | 120 s | 24.30 ekranu/min | 3732360 px/min | 11.20 MB SPI/min | 124412 px na odczyt statusu = 81 % | 42.0 zapytan/min | 9206 prosb o pamiec/min (727 kB)
C pulpit, jedna wartosc zmienia sie co 10 s      | 120 s | 18.30 ekranu/min | 2810760 px/min |  8.43 MB SPI/min |  93692 px na odczyt statusu = 61 % | 42.0 zapytan/min | 6806 prosb o pamiec/min (547 kB)
D centrala milczy, kafelki pokazuja "--"         |  25 s |  2.48 ekranu/min |  380880 px/min |  1.14 MB SPI/min |                                    | 36.0 zapytan/min |  348 prosb o pamiec/min ( 26 kB)
E otwarty ekran POMPA (animacje ikon)            |  50 s | 24.39 ekranu/min | 3745740 px/min | 11.24 MB SPI/min | 124858 px na odczyt statusu = 81 % | 42.0 zapytan/min | 9139 prosb o pamiec/min (832 kB)
F pulpit po powrocie, dane bez zmian             | 120 s | 18.30 ekranu/min | 2810760 px/min |  8.43 MB SPI/min |  93692 px na odczyt statusu = 61 % | 42.0 zapytan/min | 6806 prosb o pamiec/min (547 kB)
na baterii, bez dotyku przez 673 s: panel nie usnal; podswietlenie 255/255, zmian jasnosci od konca startu: 0

== szacunek po zmianie "ustawiaj tylko to, co sie zmienilo" (prototyp, firmware bez zmian)
A pulpit, dane bez zmian, pompa stoi             | 120 s |  0.01 ekranu/min |     885 px/min |  0.00 MB SPI/min | 5806 prosb o pamiec/min (472 kB)
B pulpit, dane bez zmian, pompa pracuje (wirnik) | 120 s |  6.00 ekranu/min |  921600 px/min |  2.76 MB SPI/min | 8202 prosb o pamiec/min (653 kB)
C pulpit, jedna wartosc zmienia sie co 10 s      | 120 s |  0.19 ekranu/min |   29214 px/min |  0.09 MB SPI/min | 5857 prosb o pamiec/min (479 kB)
D centrala milczy, kafelki pokazuja "--"         |  25 s |  2.45 ekranu/min |  376632 px/min |  1.13 MB SPI/min |  331 prosb o pamiec/min ( 25 kB)
E otwarty ekran POMPA (animacje ikon)            |  50 s |  8.86 ekranu/min | 1360776 px/min |  4.08 MB SPI/min | 8272 prosb o pamiec/min (774 kB)
F pulpit po powrocie, dane bez zmian             | 120 s |  0.00 ekranu/min |       0 px/min |  0.00 MB SPI/min | 5802 prosb o pamiec/min (472 kB)
prototyp: lv_label_set_text            z ta sama wartoscia (pominiete)  4145, ze zmiana (wykonane)   294
prototyp: lv_obj_set_local_style_prop  z ta sama wartoscia (pominiete)  8449, ze zmiana (wykonane)  3274
prototyp: lv_img_set_src               z ta sama wartoscia (pominiete)  1280, ze zmiana (wykonane)   963
prototyp: lv_obj_clear_flag            z ta sama wartoscia (pominiete)   419, ze zmiana (wykonane)   384
```

**Co te liczby znaczą na płytce.** LVGL i `lv_conf.h` są prawdziwe; to, co
LVGL przerysowuje, zależy tylko od danych i od zegara. W stanach bez
animacji (A, C, D, F) liczba pikseli jest więc taka sama jak na płytce;
w stanach z animacją (B, E) to górna granica — płytka, która nie nadąża,
narysuje mniej klatek. Liczba połączeń jest taka sama, dopóki centrala
odpowiada na czas. Prośby o pamięć obejmują firmware, LVGL i ArduinoJson,
bez napisów `String`.

**Czego w nich nie ma.** Prądu, czasu rysowania, pracy radia i procesora.

## Załącznik B. Testy po dodaniu narzędzia pomiarowego

| Co | Wynik |
|---|---|
| Sprawdzenia jednostkowe `diagnostyka_panelu.h` | 442 z 442 |
| Scenariusze symulatora | 47 z 47 (486 sprawdzeń) |
| To samo z AddressSanitizer + UBSan | 47 z 47, 0 zgłoszeń |
| Pomiar obciążenia z AddressSanitizer + UBSan | 0 zgłoszeń |
| `pomiar_pamieci.sh --przypadki` (model 32-bitowy) | w normie; liczby takie same jak przed zmianą narzędzi (po starcie 129 324 B) |
| `sprawdz_czulosc_ota.py` (celowo psuje kod aktualizacji) | 23 z 23 błędów wykrytych |

Zmienione pliki w `testy_host/`: liczniki w trzech atrapach
(`mock/atrapa.h`, `mock/TFT_eSPI.h`, `mock/Arduino.h`), scenariusz
`pomiar_obciazenia` w `symulator_panelu.cpp`, pomijanie scenariuszy
`pomiar_*` w zwykłym przebiegu `uruchom_testy.sh`, nowe
`pomiar_obciazenia.sh` i `prototypy/ustaw_tylko_zmiany.c`, opis w `README.md`.
