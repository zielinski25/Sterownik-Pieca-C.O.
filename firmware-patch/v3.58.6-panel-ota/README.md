# Firmware centrali 3.58.6 — aktualizacja panelu LCD przez centralę

Data: 2026-11-12 · następna po **3.58.5**
(`firmware-patch/v3.58.5-panel-wykresy-po-restarcie/`) i zawiera wszystko, co
ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp`. Pliki `.h`, `secrets.h`,
biblioteki i `platformio.ini` bez zmian. Plik pasuje do projektu, w którym
jest wersja **3.58.3**, **3.58.4** albo **3.58.5**.

Do kompletu jest firmware panelu **v1.4.0** (`LCD-Centrali/`). Panel trzeba
nim wgrać raz kablem — na płytce jest v1.2.2 ze starym kodem aktualizacji.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Co zostało sprawdzone, a co nie: rozdział na końcu. Instrukcja wgrania
> i powrotu: `CZYTAJ-NAJPIERW.txt`.

---

## Skąd ta wersja

Raport `RAPORT_OTA_PANELU_PRZEZ_CENTRALE.md` (katalog główny repozytorium)
opisał, jak `/update_panel` działało do 3.58.5. Po nim padło „Działaj”.
Cztery wady, które ta wersja usuwa:

1. **Pętla.** Centrala uznawała aktualizację za wykonaną tylko wtedy, gdy
   panel zgłosił wersję identyczną co do znaku z tagiem wydania na GitHubie.
   Panel zgłasza `v1.3.0`, wydania mają tagi w rodzaju `3.57.9`. Po udanym
   wgraniu oferta zostawała, a panel wgrywał ten sam plik przy każdym starcie.
2. **Blokada bez końca.** Dopóki oferta czekała, `/update` i `/update_panel`
   odmawiały, a nadzór zadań centrali był wyłączony. Stan wracał po restarcie
   centrali. Nie było limitu czasu ani komendy, która to kończy.
3. **Alarm.** Centrala podawała ofertę i plik także w czasie alarmu.
4. **Plik.** Pobrany z GitHuba plik był sprawdzany tylko po pierwszym bajcie —
   firmware centrali wgrany omyłkowo jako `firmware_panel.bin` przechodził.

## Co zmienia 3.58.6

1. **Potwierdzenie po pliku, nie po numerze wersji.** Każdy program ESP32 ma
   na końcu sumę SHA-256 (dopisuje ją narzędzie budujące). Centrala bierze z
   niej identyfikator pliku — 16 znaków — i podaje go panelowi. Panel v1.4.0
   zgłasza identyfikator programu, z którego pracuje. Równe = aktualizacja
   wykonana. Tag wydania i numer wersji panelu nie muszą być równe.
2. **Oferta zawsze się kończy** (tabela niżej) i o każdym zakończeniu
   przychodzi wiadomość w Telegramie.
3. **Nowa komenda `/update_panel_anuluj`** — także przycisk „Anuluj
   aktualizację panelu” w menu aktualizacji i komenda Firebase
   `update_panel_anuluj`.
4. **Czekająca oferta niczego nie blokuje.** `/update` centrali działa,
   nadzór zadań pracuje. Blokada jest tylko na czas pobierania pliku
   z GitHuba i na czas wysyłania pliku panelowi.
5. **Alarm:** centrala nie pokazuje panelowi oferty, a `/panel-ota.bin`
   odpowiada kodem 503. Po ustaniu alarmu oferta wraca.
6. **Sprawdzenie pliku z wydania:** nagłówek programu ESP32, układ ESP32
   (nie ESP32-S3 centrali), suma SHA-256 na końcu. Plik, który tego nie
   spełnia, jest odrzucany, a poprzedni plik na dysku centrali zostaje.
7. **Jedno przypomnienie.** Gdy po 5 minutach panel nie zaczął pobierać,
   przychodzi wiadomość z przyczyną: panel się nie zgłasza / panel ma starszy
   firmware i zacznie po wyłączeniu i włączeniu / trwa alarm / powód podany
   przez panel (ktoś dotyka ekranu, słaba bateria, mało pamięci…).
8. **Plik dostaje tylko panel, dla którego czeka oferta.** Do 3.58.5 każde
   zapytanie z dobrym hasłem dostawało ostatnio pobrany plik.
9. Obsługa zapytań panelu (`/api/status`) nie czeka już na wysłanie
   wiadomości Telegram — decyzję wykonuje zadanie Telegrama.
10. Komunikat po `/update_panel` nie obiecuje instalacji „przy najbliższym
    wybudzeniu panelu (przycisk)”. Gdy wydanie nie ma pliku dla panelu,
    centrala pisze to wprost (dotąd: sam kod 404).

## Jak to wygląda w Telegramie

Warunek: w najnowszym wydaniu na GitHubie jest plik `firmware_panel.bin`.
Dziś żadne wydanie go nie ma — patrz „Jak przygotować wydanie” niżej.

```
Ty:       /update_panel
Centrala: Sprawdzam najnowsza wersje panelu na GitHub...
Centrala: Wydanie 3.58.6: pobieram plik panelu na centrale...
Centrala: Plik panelu z wydania 3.58.6 jest na centrali (1234567 B). Panel z
          firmware v1.4.0 lub nowszym pobierze go sam, zwykle w ciagu minuty -
          gdy nie ma alarmu i nikt nie dotyka ekranu; na ekranie pojawi sie
          napis AKTUALIZACJA PANELU. Starszy panel zacznie dopiero po
          wylaczeniu i wlaczeniu. Wynik przyjdzie osobna wiadomoscia. Oferta
          czeka 24 h; anulowanie: /update_panel_anuluj
(panel pobiera plik, pokazuje pasek postępu, restartuje się)
Centrala: Panel zaktualizowany: pracuje na pliku z wydania 3.58.6 (zglasza
          wersje v1.4.1, przedtem v1.4.0).
```

Gdy wydanie nie ma pliku dla panelu:

```
Centrala: Pobieranie firmware panelu nieudane: w wydaniu 3.57.9 nie ma pliku
          firmware_panel.bin (kod 404). Dodaj go do wydania na GitHubie i
          wyslij /update_panel ponownie.
```

## Kiedy oferta się kończy

| Co się stało | Wiadomość (początek) |
|---|---|
| Panel v1.4.0 pracuje na oferowanym pliku | `Panel zaktualizowany: pracuje na pliku z wydania …` |
| Starszy panel zgłosił wersję równą tagowi wydania (reguła sprzed 3.58.6) | `Panel zaktualizowany do …` |
| Panel v1.4.0 odmówił: trzy nieudane pobrania, zły plik, plik za duży, złe hasło OTA, nowy firmware nie wystartował i wrócił poprzedni | `Panel nie wgral pliku z wydania …: <przyczyna>. Aktualizacja wycofana, panel pracuje na dotychczasowym firmware (…)` |
| Starszy panel (do v1.3.0) pobrał plik i zgłosił się z inną wersją niż przedtem | `Panel pobral plik z wydania … i zglasza teraz wersje … (przedtem …) - aktualizacja wykonana.` |
| Starszy panel pobierał plik i zgłasza tę samą wersję | `Panel pobieral plik z wydania …, ale nadal zglasza wersje …` — oferta wycofana, żeby panel nie wgrywał pliku przy każdym starcie |
| Panel zaczynał pobierać 4 razy i ani razu nie potwierdził | `Panel zaczynal pobierac plik z wydania … 4 razy …` |
| Minęły 24 h | `Panel nie wgral pliku z wydania … w ciagu 24 h …` |
| `/update_panel_anuluj` | `Aktualizacja panelu (plik z wydania …) anulowana.` |
| Nowe `/update_panel`, gdy poprzednia oferta jeszcze czeka | poprzednia jest zastępowana; wzmianka w wiadomości o pobieraniu |

Dodatkowo, bez kończenia oferty:

- po 5 minutach bez rozpoczęcia pobierania — jedno przypomnienie z przyczyną
  (`Minelo 5 minut, a panel nie zaczal pobierac …`);
- po restarcie centrali z czekającą ofertą — `Centrala zrestartowala sie,
  a aktualizacja panelu … jeszcze nie zostala wgrana - oferta wraca na
  kolejne 24 h.`

Każde zakończenie zostawia też linię w logu centrali:
`tag=OTA-PANEL lvl=… zdarzenie=<nazwa> nr=… wydanie=… obraz=… wersja_panelu=… pobran=… powod_panelu=…`
(nazwy: `potwierdzono_obraz`, `potwierdzono_aktualizacje`, `odmowa_panelu`,
`starszy_panel_wrocil`, `za_duzo_pobran`, `minal_czas`, `anulowano`).

## Co centrala i panel sobie mówią

Centrala → panel, w odpowiedzi `/api/status`:

| Pole | Znaczenie |
|---|---|
| `panel_ota_dostepna` | `true`, gdy czeka plik dla panelu i nie ma alarmu |
| `panel_ota_wersja` | tag wydania, z którego jest plik (jak dotąd) |
| `panel_ota_obraz` | **nowe:** identyfikator pliku, 16 znaków |
| `panel_ota_nr` | **nowe:** numer oferty; rośnie przy każdym `/update_panel` |

W czasie alarmu: `false`, pola tekstowe puste, numer 0.

Panel → centrala, w zapytaniu `/api/status?…`:

| Parametr | Znaczenie |
|---|---|
| `panel_fw` | wersja panelu (jak dotąd) |
| `reset_powod` | powód ostatniego restartu (jak dotąd) |
| `panel_obraz` | **nowe:** identyfikator programu, z którego panel pracuje |
| `panel_obraz_proba` | **nowe:** to samo przy pierwszym starcie po wgraniu, zanim panel zatwierdzi nowy program |
| `panel_ota_odmowa=<nr>-<powód>` | **nowe:** panel tego pliku nie wgra |
| `panel_ota_czeka=<powód>` | **nowe:** dlaczego panel jeszcze nie zaczął |

`GET /panel-ota.bin?token=<hasło OTA>` — kody: `200` plik; `401` złe hasło;
`404` nie ma oferty albo pliku; `503` alarm; `409` za dużo pobrań (oferta
wycofana).

## Zgodność

| Panel LCD | Centrala | Aktualizacja przez `/update_panel` |
|---|---|---|
| v1.4.0 | 3.58.6 | panel zaczyna sam w trakcie pracy, pokazuje postęp, wynik przychodzi w Telegramie |
| do v1.3.0 | 3.58.6 | panel pobiera plik dopiero po wyłączeniu i włączeniu, z pustym ekranem; po tej jednej próbie oferta znika (bez pętli); centrala pisze, co widzi |
| v1.4.0 | do 3.58.5 | panel zaczyna sam w trakcie pracy i wgrywa plik **raz**; taka centrala nie umie tego potwierdzić, więc jej oferta zostaje i blokuje `/update` — do ręcznego zdjęcia (`http://<adres centrali>/api/status?panel_fw=<tag wydania>`) |
| do v1.3.0 | do 3.58.5 | jak w raporcie: pętla, gdy tag wydania różni się od wersji panelu |

Kolejność wgrywania jest dowolna. Najprościej: centrala 3.58.6, potem panel
v1.4.0 kablem.

Oferta zapisana przez 3.58.5 (bez identyfikatora pliku) nie jest wznawiana
po wgraniu 3.58.6. Centrala pisze wtedy, żeby wysłać `/update_panel` ponownie.

## Jak przygotować wydanie z plikiem dla panelu

1. Zbuduj firmware panelu w PlatformIO (`LCD-Centrali/`). Plik:
   `LCD-Centrali/.pio/build/esp32dev/firmware.bin`.
2. Sprawdź go, zanim trafi do wydania (z katalogu głównego repozytorium):

   ```
   python3 narzedzia/ota_panelu_sprawdz.py --tag <tag wydania> --bin LCD-Centrali/.pio/build/esp32dev/firmware.bin
   ```

3. Dołącz go do wydania na GitHubie pod nazwą **`firmware_panel.bin`**.
   Centrala bierze **najnowsze** wydanie — to samo, z którego `/update`
   pobiera `firmware.bin` dla siebie.
4. `/update_panel` w Telegramie.

## Pamięć trwała centrali (NVS)

Klucze `panOtaGotowa`, `panOtaTag`, `panOtaTrwa` jak dotąd. Nowe:
`panOtaNr` (numer oferty), `panOtaObraz` (identyfikator pliku), `panOtaRozm`
(rozmiar pliku), `panOtaPrzed` (wersja panelu sprzed oferty). Zapis następuje
przy `/update_panel` i przy zakończeniu oferty — nie przy każdym zapytaniu
panelu.

## Stos i pamięć RAM centrali

Zadanie Telegrama (`tgTaskFn`) ma 8 kB stosu i pod każdą wysyłką jest jeszcze
połączenie TLS. Według logów tej centrali (opis v3.56.7 na górze pliku)
w najgorszej chwili zostaje na tym stosie 2 256–2 708 B — w logu to pole
`hwm_tg`. Nowy kod jest pod to napisany:

- teksty i kopie stanu nie leżą na stosie: na czas obsługi bierze 800 B ze
  sterty i oddaje je zaraz po wysłaniu wiadomości;
- każda wysyłka i każde pobieranie wychodzi z funkcji, która trzyma na stosie
  najwyżej kilka obiektów `String`; dłuższe teksty składają osobne funkcje,
  które wracają przed wysyłką;
- gdy nie ma oferty, obieg zadania kończy się na jednym sprawdzeniu, bez
  przydziału pamięci;
- gdy sterta odmówi tych 800 B, nic nie ginie — wynik czeka do następnego
  obiegu;
- w `pobierzFirmwarePanelaZGithub()` nowe teksty błędów są składane przez
  `+=`, żeby ramka stosu tej funkcji została taka jak w 3.58.5.

Zmienne stałe nowego kodu: ok. 330 B.

Ramki stosu policzone kompilatorem dla 32-bitowego x86 (przybliżenie): 60 B
w obiegu zadania i w anulowaniu, 156 B w `/update_panel`. **Na płytce nie
zmierzone** — po wgraniu warto porównać `hwm_tg` (rozdział „Jak sprawdzić po
wgraniu”).

## Zmiany w liczbach

Względem **3.58.5**: 22 bloki diffu, 1 128 linii dodanych, 113 usuniętych
(`main_centrala-v3.58.5-do-v3.58.6.patch`).

| Co | Linie |
|---|---|
| wpis changeloga na górze pliku | +75 (komentarz) |
| `FIRMWARE_VERSION` `"v3.58.5"` → `"v3.58.6"` | 1 zmieniona |
| blok `[v3.58.6 PANEL-OTA-BEGIN]` … `[-END]` — decyzje, bez Arduino | +467 |
| blok `[v3.58.6 PANEL-OTA-STAN-…]` — zmienne (w miejsce dwóch starych) | +24 / −2 |
| blok `[v3.58.6 PANEL-OTA-KLEJ-…]` — log, NVS, Telegram, HTTP | +450 |
| linie puste i komentarz przy blokach | +8 |
| 18 miejsc w starszym kodzie (wywołania nowych funkcji, komenda, przycisk, sprawdzenie pliku); większość usuniętych linii to kod przeniesiony do bloku KLEJ | +103 / −110 |

Względem **3.58.3**: `main_centrala-v3.58.3-do-v3.58.6.patch` (zawiera też
zmiany 3.58.4 i 3.58.5 — wykresy dla panelu).

Rozpisanie miejsce po miejscu: `WYNIKI/zakres-zmian.txt`.

## Czego NIE zmieniono

- `/update` centrali — poza tym, że odmawia, gdy plik jest właśnie wysyłany
  panelowi, i nie odmawia już z powodu samej czekającej oferty.
- Pobierania z GitHuba — poza sprawdzeniem pliku i czytelniejszym błędem 404.
- Limitów miejsca na dysku centrali, strony WWW, hasła OTA (`secrets.h`).
- Wykresów dla panelu (`/api/history/panel`): kod identyczny z 3.58.5,
  sprawdzane przy każdym uruchomieniu testów.
- Sterowania piecem, czujników, automatyki, zapisu historii, Firebase poza
  jedną nową komendą.

## Jak sprawdzić po wgraniu

1. Strona WWW centrali → Ustawienia → „Sesja operatora”: `Wersja centrali:
   v3.58.6`.
2. Telegram: `/update_panel_anuluj` → `Nie ma czego anulowac - zadna
   aktualizacja panelu nie czeka.`
3. Telegram: `/update_panel`, dopóki wydanie nie ma pliku dla panelu →
   wiadomość o braku `firmware_panel.bin` (wyżej). Nic się nie zmienia.
4. Panel v1.4.0 pracuje jak dotąd; w logu centrali po jego starcie nie ma
   nowych ostrzeżeń `tag=OTA-PANEL`.
5. **Zapas stosu.** W logu centrali co 5 minut jest linia
   `tag=HEALTH-A … hwm_tg=…B …` — najmniejszy zapas stosu zadania Telegrama
   od startu. Zapisz wartość z obecnej wersji, zanim wgrasz 3.58.6, i porównaj
   po wgraniu: po punktach 2 i 3, a potem po pełnej próbie. Spodziewane:
   podobna jak dotąd (dotąd 2 256–2 708 B). Jeśli spadnie o ponad 500 B albo
   poniżej 1 000 B — przyślij te linie, zanim użyjesz `/update_panel`
   ponownie.
6. Pełna próba — po przygotowaniu wydania (wyżej). Co warto przysłać: linie
   `tag=OTA-PANEL` z logu centrali i z logu panelu (serial 115200) oraz
   `hwm_tg` sprzed i po.

## Co zostało sprawdzone, a co nie

Sprawdzone na PC (`tests/`, wyniki z dnia wydania w `WYNIKI/`):

- **Decyzje** (blok `PANEL-OTA`) i **kod klejący** (`PANEL-OTA-KLEJ`) są
  wycinane z `main_centrala.cpp` i uruchamiane zwykłym `g++` z atrapami
  Arduino, NVS, Telegrama, sterty i serwera HTTP: 166 sprawdzeń, także pod
  AddressSanitizer i UBSan (`WYNIKI/testy-pc.txt`,
  `WYNIKI/testy-pc-sanitizer.txt`). W tym: odmowa sterty w każdym miejscu,
  w którym kod bierze pamięć roboczą, i wynik poprzedniej oferty, który
  przychodzi w chwili, gdy `/update_panel` wystawia następną.
- **Czułość testów:** 43 celowo wprowadzone błędy w tym kodzie — każdy
  wykryty (`WYNIKI/testy-czulosc.txt`).
- **Razem z panelem:** prawdziwy firmware panelu v1.4.0 uruchomiony na PC
  rozmawia z tym samym blokiem decyzji — 12 scenariuszy `ota_*` w
  `LCD-Centrali/testy_host/`.
- **Typy i nazwy:** wycięty kod oraz nowe linie
  z `pobierzFirmwarePanelaZGithub()` sprawdzone prawdziwymi nagłówkami
  arduino-esp32 (3.0.7 i 3.3.8), ESPAsyncWebServer i AsyncTCP — bez błędów
  i bez ostrzeżeń (`WYNIKI/naglowki-esp32.txt`).
- **Układ pliku `.bin`:** trzy prawdziwe obrazy zbudowane narzędziami
  Espressif (z repozytorium esptool) przeszły przez kod centrali i panelu,
  który ocenia plik. Obraz dla ESP32 z sumą na końcu — przyjęty, ten sam
  identyfikator po obu stronach; obraz bez sumy i obraz dla innego układu —
  odrzucone z właściwą przyczyną (`WYNIKI/prawdziwe-obrazy.txt`).
- **Wykresy (3.58.4/3.58.5):** kod identyczny z 3.58.5; te same 2 697
  sprawdzeń przechodzi.

Nie sprawdzone:

- **Kompilacja całego pliku dla ESP32** (PlatformIO). Pierwsza będzie Twoja.
- **13 pojedynczych miejsc w starszym kodzie** — wywołania nowych funkcji
  w trasach HTTP, `tgTaskFn()` i `setup()`, wpis w `enum`, przycisk, komendy.
  Nie są kompilowane w swoim miejscu; są w diffie (`WYNIKI/zakres-zmian.txt`).
- **Samo pobieranie w `pobierzFirmwarePanelaZGithub()`** (HTTPS, zapis na
  dysk centrali) — w testach ta funkcja jest podstawiona.
- **Stos zadania Telegrama na płytce.** PC go nie mierzy; liczby wyżej to
  przybliżenie z innego procesora. Pokaże go `hwm_tg`.
- **Wszystko, co wymaga sprzętu:** pobranie pliku z GitHuba, zapis na dysk
  centrali, wysłanie go panelowi przez WiFi, zapis w pamięci panelu, start
  nowego programu i powrót do poprzedniego, gdy nowy nie wstanie.
- **Czy panel odczyta z własnej pamięci tę samą sumę, którą centrala bierze
  z końca pliku.** Wynika to ze źródeł ESP-IDF; na płytce nie było
  sprawdzane. Da się to sprawdzić przed pierwszą aktualizacją: po wgraniu
  panelu v1.4.0 kablem linia `stan_przy_starcie obraz=…` w jego logu ma być
  równa „Identyfikator pliku” z `narzedzia/ota_panelu_sprawdz.py --bin` dla
  tego samego `firmware.bin`. Gdyby nie była, centrala nie potwierdzi
  aktualizacji, a panel po restarcie zgłosi odmowę `cofnieta` — oferta
  skończy się wiadomością, bez pętli.

## Zawartość paczki

| Plik | Co to |
|---|---|
| `src/main_centrala.cpp` | firmware centrali 3.58.6 |
| `CZYTAJ-NAJPIERW.txt` | wgranie w trzech krokach, sprawdzenie, powrót |
| `main_centrala-v3.58.5-do-v3.58.6.patch` | różnica względem 3.58.5 |
| `main_centrala-v3.58.3-do-v3.58.6.patch` | różnica względem 3.58.3 |
| `tests/` | testy na PC (`tests/README.txt`) |
| `WYNIKI/` | wyniki testów z dnia wydania, zakres zmian |
| `SHA256SUMS.txt` | sumy kontrolne plików paczki |
