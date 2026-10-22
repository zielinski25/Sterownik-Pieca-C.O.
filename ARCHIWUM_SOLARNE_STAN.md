# Archiwum solarne w centrali ESP32 — stan i ustalenia

Dokument odtworzony 2026-10-20 po resecie środowiska (poprzednia kopia nie była w repozytorium).
Ma być **żywy**: każda zmiana zachowania archiwum dopisuje sekcję „Historia wersji”, a ustalenia
sprzętowe poprawia się w miejscu. Wersja opisywana: **v3.53.1**.

> **Zanim policzysz energię:** zbiornik ma **100 l**. Przelicznik to
> **0,1163 kWh/K** — nigdy 200 kg. Patrz §14.

---

## 1. Po co to jest

Centrala ESP32 sama, bez otwartego `Piec.html` i bez aplikacji na telefonie, zbiera:

1. **snapshot prognozy GHI** na kolejną dobę (Open-Meteo, `shortwave_radiation`),
2. **podsumowanie rzeczywistych odczytów temperatury zbiornika kolektora słonecznego**.

Dane leżą lokalnie w LittleFS (`/solar_daily/RRRR-MM-DD.json`), potem są wysyłane do Firebase.
Dostęp do nich jest **wyłącznie przez Telegram albo GitHuba** — w raporcie nie ma już odnośnika
do endpointu w sieci lokalnej.

## 2. Kanał pomiarowy — to NIE jest bojler pieca

| | Układ słoneczny (archiwum) | Bojler pieca C.O. |
|---|---|---|
| czujnik | `czujnik_temp_Panel_Plus` | `czujnik_temp_Bojlera` |
| bieguna | `BUS_3` | `BUS_2` |
| kanał telemetrii | `TELEMETRY_CH_PANEL` | `TELEMETRY_CH_BOJLER` |
| zakres | **−30…160 °C** | 0…160 °C |

Rekord archiwum **nie może być hybrydowy**. Rekordy sprzed poprawki kanału mają
`schemaVersion = 1` i są oznaczane `[dane CO]`; usuwa się je przez `/archiwum_czysc TAK`.

## 3. Instalacja (ustalenia użytkownika)

- kolektor **próżniowy rurowy, 10 rur** (nie 12),
- zbiornik **100 l**, zintegrowany z kolektorem, na dachu,
- obieg **termosyfonowy — bez pompy**, więc bez przepływomierza,
- czujnik w **wodzie, z boku, w połowie wysokości** zbiornika,
- **są dwa boilery**: (1) na dachu — zbiornik kolektora, (2) w kotłowni — bojler C.O. grzany
  wężownicą z pieca. Zimna woda z wodociągu idzie **bezpośrednio** do boilera, potem ciśnieniowo
  do kranu; użytkownik zaworami wybiera ten, w którym woda jest cieplejsza.

## 4. Tryby pracy instalacji

- **LATO / JESIEŃ / WIOSNA** — wodociąg → zbiornik na dachu → kran (obsługiwany jest boiler z dachu).
- **ZIMA** — kolektor nieużywany, **woda spuszczona**, używany bojler pieca.
- **spuszczona (poza sezonem, wiosna–jesień)** — sytuacja wyjątkowa: zbiornik bez wody
  (np. po spuszczeniu na czas mrozów albo przed napełnieniem).

Znacznik trybu ustawia się z Telegrama:

```
/solar_tryb                 # pokazuje stan
/solar_tryb spuszczona      # ZIMA: woda spuszczona — bez GHI, rekordy offSeason
/solar_tryb sezon           # woda w zbiorniku — GHI wraca przy najbliższym oknie
```

Tryb jest zapisany w NVS (`solarOffSeason`), więc **przeżywa restart i OTA**.

**Dlaczego to jest potrzebne:** czujnik w pustym zbiorniku mierzy powietrze. Bez znacznika taki
dzień wygląda dokładnie jak zwykły pochmurny dzień z zerowym przyrostem. Poza sezonem:
prognoza GHI **nie jest pobierana**, rekord ma `offSeason = true`, a w raporcie dopisek
`(poza sezonem)`.

## 5. Co dokładnie zbiera firmware

- **Próbka telemetrii** — co 60 s, wpięta w istniejący zapis historii (`historiaZapiszProbke`).
- **Checkpoint rekordu** — co 5 min, tylko gdy jest co zapisać (próbka albo prognoza dla tej daty).
- **Granice doby** — zawsze `Europe/Warsaw`, liczone przez `mktime`/`localtime` z `TZ`,
  więc doba ma 24, 23 albo 25 godzin (zmiana czasu rozliczana poprawnie).
- **Okna prognozy GHI:**
  - wieczór **23:30–23:59** → prognoza na **jutro** (podstawowe źródło), do 8 prób co 2 min,
  - dostęp **00:00–04:59** → prognoza na **dziś**, gdy wieczorna się nie udała
    (`capturedLate`, do 20 prób co 15 min, timeout 7 s).

## 6. Format rekordu (`/solar_daily/RRRR-MM-DD.json`)

- `schemaVersion = 2`, `localDate`, `timezone = Europe/Warsaw`, `source`, `updatedAtUtc`,
- `seasonMode` / `offSeason` — tryb instalacji w chwili zapisu,
- `forecast`: `source`, `capturedAtUtc`, `expectedHours`, `hourCount`, `complete`,
  `sumGhiKwhM2`, `peakWm2`, tablica `hours[] {t, ghi}` (czas UTC punktu),
- `actual`: `channel = t_panel`, opis systemu, `minC`, `maxC`, `deltaC`, `firstUtc`, `lastUtc`,
  `coverageRatio`, `validSamples`, `expectedSampleCount`, `simulatedSamples`,
  `technicalCandidate`, `finalized`.

## 7. Komendy Telegrama

| Komenda | Działanie |
|---|---|
| `/archiwum [N]`, `/solar [N]` | raport z N ostatnich dni (1–30, domyślnie 7) |
| `/archiwum_usun RRRR-MM-DD` | usuwa **jeden** dzień |
| `/archiwum_czysc` / `TAK` | pokazuje / usuwa rekordy sprzed poprawki kanału |
| `/solar_tryb [spuszczona\|sezon]` | znacznik trybu instalacji |

## 8. Jak czytać raport

```
2026-10-20  GHI 0.55 kWh/m2  dT -1.5°C  amplituda 3.2°C  (min 21.8 @ 05:40 / max 25.0 @ 14:20)  pokrycie 94.6% (98.6% czasu)  1362/1440 prob. 00:00-22:58 (w toku)
```

- **GHI** — suma nasłonecznienia z prognozy, kWh/m². `(niepelna)` = brakuje godzin.
- **dT** — pierwsza próbka doby → ostatnia. **Słaby wskaźnik**: porównuje północ z wieczorem.
- **amplituda** — `max − min` w ciągu doby, w °C. Pokazuje, ile zbiornik realnie się zmienił.
  (W fizyce różnicę temperatur podaje się w kelwinach — 2,5 K to dokładnie 2,5 °C. W raporcie
  jest °C, bo tak się u nas czyta temperaturę.)
- **`prognoza na jutro (doba sie nie zaczela)`** — rekord zapisany wieczorem na następną dobę
  (okno 23:30–23:59). Zero odczytów jest tu **stanem oczekiwanym**, nie utratą danych.
  Taki dzień nie wchodzi do sumy GHI ani do mianownika „odczyty”; prognoza jest pokazana
  osobno w linii `Jutro (prognoza): GHI …`.
- **`brak probek (doba w toku)`** — dzisiejsza doba, jeszcze bez próbek (np. tuż po północy).
- **`brak odczytow`** — doba miniona bez danych. Tylko tu to komunikat o realnej luce.
- **`TECH`** — doba zamknięta, kompletna, bez symulacji i bez podejrzanych poborów; kandydat
  do ręcznej oceny modelu. **Nie pojawia się przed domknięciem doby.**
- **`(za malo probek: N)`** — mniej niż 30 próbek (< pół godziny). Wtedy `dT` i amplituda są
  bezużyteczne, więc ich nie podajemy; liczba próbek mówi, ile jest.
- **Pokrycie od upływu czasu** pojawia się dopiero po godzinie doby. Wcześniej jedna minuta
  restartu to kilkanaście punktów procentowych — liczba prawdziwa, ale myląca.
- **`@ HH:MM`** przy min/max — kiedy padło ekstremum (czas lokalny). Dopiero z tym amplituda
  się tłumaczy: minimum nad ranem + maksimum po południu = grzanie słońcem; minimum wieczorem
  = nic nie grzało i amplituda jest tylko dobową huśtawką.
  **Brak `@`** = godzina ekstremum jest nieznana. Dwa przypadki: (1) rekord sprzed v3.52.9,
  (2) doba, która **trwała w chwili wgrywania** v3.52.9+ — skrajne temperatury zostały
  zapisane, zanim firmware umiał zapisać ich czas, a późniejsze próbki ich nie przebijają,
  więc znacznik nigdy się nie pojawi. To jednorazowy koszt OTA w środku dnia; doba startująca
  od zera ma godziny od pierwszej próbki.
- **pokrycie** — udział minut doby, w których jest próbka. W dobie w toku drugi nawias to pokrycie
  **czasu, który już upłynął** (mianownik = minuty od północy).
- `(w toku)`, ` sym`, `TECH`, `[dane CO]`, `(poza sezonem)` — znaczniki stanu rekordu.

## 9. OTA i rollback — wymóg „żadna aktualizacja nie może wymagać kabla”

Od v3.52.6 partycja jest potwierdzana **w `loop()`**, nie na końcu `setup()`:

| warunek | czas |
|---|---|
| stabilna praca z WiFi | 10 min |
| awaryjnie, bez WiFi | 60 min |

Firmware resetujący się co 15 s nigdy nie potwierdzi obrazu → bootloader wraca do poprzedniej
wersji. Znacznik `/ota_pending.txt` pozwala wykryć cichy rollback i wysłać o nim Telegram
(`tag=OTA-ROLLBACK`). **Potwierdzone na urządzeniu:** v3.52.6 wgrane OTA, komunikat
„Aktualizacja zakończona pomyślnie” przyszedł ~10 minut po starcie.

Czego to **nie** chroni: awarii po potwierdzeniu, zawieszenia bez resetu, zaniku prądu w pierwszym
starcie.

## 10. Czego świadomie NIE twierdzimy

- `ΔT × 0,1163 kWh/K` to **równoważnik rzędu wielkości** dla 100 l, a nie zmierzony uzysk kolektora
  (brak przepływomierza, termosyfon).
- Czujnik w połowie wysokości **nie widzi stratyfikacji**, więc licznik poborów
  (>0,3 °C/min w ≤5 min) będzie **zaniżał** — nie udajemy, że policzy wylaną wodę.
- Dzień bez GHI nie jest „błędny” — po prostu nie ma czym porównać temperatury.
- Poza sezonem temperatura z pustego zbiornika **nie jest** danymi słonecznymi.

## 11. Historia wersji

| wersja | co zmieniła |
|---|---|
| v3.51.0 | archiwum w centrali (nie w przeglądarce), raport `/archiwum`, przycisk w Telegramie |
| v3.51.1 | przyczyna braku GHI w rekordzie i w raporcie (`http_400`, `json`, `brak_pol`) |
| v3.52.0 | kanał `t_panel` (układ słoneczny), rekordy CO oznaczane i usuwalne |
| v3.52.1 | `/archiwum_usun` — pojedynczy dzień; koniec rekordów hybrydowych |
| v3.52.2 | pusty rekord nie wraca po usunięciu dnia |
| v3.52.3 | przyczyna `http_400`: końcowe `Z` w `start_hour`/`end_hour` |
| v3.52.4 | twarda bramka `SOLAR_DAILY_ARCHIVE_API` — nie da się zbudować mieszanej paczki |
| v3.52.5 | odpowiedź GHI czytana w całości (`getString`) przed `deserializeJson` |
| v3.52.6 | potwierdzenie OTA w `loop()` (10 min / 60 min) + wykrywanie cichego rollbacku |
| v3.52.7 | amplituda, pokrycie od upływu czasu, `/solar_tryb`, tablica GHI bez przesunięcia |
| v3.52.8 | jednostka w raporcie: °C zamiast K (zmiana kosmetyczna) |
| v3.52.9 | godzina minimum i maksimum (`@ HH:MM`) — amplituda przestaje być zagadką |
| v3.52.10 | „prognoza na jutro” zamiast mylącego „brak odczytów”; `TECH` tylko dla doby zamkniętej |
| v3.52.11 | początek doby nie udaje pomiaru: `(za malo probek)`, pokrycie czasu dopiero po godzinie |
| v3.53.0 | całe składanie raportu w `SolarDailyArchive.h` + symulator; podział wiadomości (limit Telegrama) |
| **v3.53.1** | `/archiwum_test` — dowód podziału na urządzeniu, zanim archiwum uzbiera 30 dni; odpowiedź na nieznaną komendę |

## 11a. Jak sprawdzić raport BEZ wgrywania firmware

Od v3.53.0 cała treść wiadomości (nagłówek, linie dni, stopka, podział na porcje)
jest w `SolarDailyArchive.h`, a nie w `main_centrala.cpp`. Firmware tylko czyta
pliki i wypełnia tablicę rekordów.

```sh
cd firmware-patch/v3.53.0-solar-daily-archive/tests
g++ -std=c++11 -Wall -Wextra -Werror -I.. ReportSimulation.cpp -o /tmp/reportsim
/tmp/reportsim              # 3665 sprawdzeń, 0 błędów
/tmp/reportsim --verbose    # pełna treść każdej wiadomości
```

**Zasada:** żadna zmiana w raporcie nie idzie na urządzenie, dopóki symulator
nie przejdzie. `--verbose` pokazuje też granice wiadomości i ich rozmiar w bajtach —
raport 30-dniowy to 4585 B, czyli **dwie wiadomości** (3777 B i 822 B), nie jedna. Wersje v3.52.7–v3.52.11 łamały tę zasadę i kosztowały pięć
aktualizacji OTA — każda naprawiała coś, co widać było tylko w gotowym tekście.

## 11b. Paczka v3.53.1

| | |
|---|---|
| plik | `v3.53.1-solar-daily-archive.zip` |
| rozmiar | 853 182 B |
| sha256 | `07a01257a509e29e70a7e1ffee1e5abff816e50531fb4a8c973f27904bb8389b` |
| Google Drive | `1X9wYcTUtuyh9pm4Qrrr_s8aU3O0W0zs5` |
| katalog w repo | `firmware-patch/v3.53.1-solar-daily-archive/` |

Paczka jest **jedna** i zastępuje v3.52.7–v3.52.11 (każda z nich była osobnym,
niepełnym krokiem). Zawiera `Piec.html` bez zmian oraz dwa patche v3.49.x.

### Dowód podziału wiadomości bez czekania miesiąc

Podział na porcje włącza się dopiero przy ~25 zapisanych dniach. Archiwum miało **2**,
więc po wgraniu v3.53.0 nie było jak sprawdzić, czy druga wiadomość w ogóle dojdzie —
a błąd ujawniłby się po miesiącu jako **ciche zatrzymanie raportu**.

```
/archiwum_test
```

Buduje 30 dni **wymyślonych** w RAM (`buildSelfTestDays()`), bez czytania i zmieniania
LittleFS, i wysyła je **drogą prawdziwego raportu** (`tgWyslijRaportTekst`). Pierwsza
linia zawsze mówi `TEST podzialu — liczby wymyslone, archiwum nietkniete`. Oczekiwany
wynik: **dwie wiadomości**, druga zaczyna się od `(ciag dalszy)`.

**Wynik 2026-10-21 00:56 (v3.53.1):** dwie wiadomości, druga z `(ciag dalszy)` —
podział działa na urządzeniu, zgodnie z symulatorem (szczegóły w sekcji 13).

## 12. Co dalej (plan etapowy)

1. **Po wgraniu v3.53.1** — `/archiwum_test`: mają przyjść **dwie** wiadomości,
   druga z `(ciag dalszy)` i z danymi oznaczonymi `TEST`. To jedyny sposób, żeby
   zobaczyć podział na porcje teraz, a nie po miesiącu zbierania dni.
2. **Przed zimą** — wpisać `/solar_tryb spuszczona` **w dniu spuszczania wody**, nie później;
   inaczej rekordy z pustego zbiornika wejdą do statystyk jako „pochmurne dni”.
3. **Wiosną** — `/solar_tryb sezon`; prognoza GHI wróci przy najbliższym oknie.
4. **Opcjonalnie** — po odzyskaniu źródeł odtworzyć `PLAN_ARCHIWUM_SOLARNE_CENTRALA.md`
   (plan wdrożenia i uzasadnienia decyzji) — obecnie nie istnieje.

## 13. Dziennik wyników z urządzenia

Zapis rzeczywistych raportów, żeby dało się porównać dni i wychwycić anomalie.

### 2026-10-21 po północy — wgrana v3.53.0 (pierwsze wgranie po testach)

Użytkownik zgłosił instalację **v3.53.0**. To pierwsza paczka od v3.52.7, której treść
raportu została sprawdzona **przed** wgraniem (339 PASS + 3665 sprawdzeń symulatora
w wersji v3.53.1; w chwili wgrywania było 3559).

Wynik na urządzeniu (zrzut Telegrama, 00:24–00:33):

| sprawdzenie | oczekiwane | wynik |
|---|---|---|
| `/update` | pobranie z GitHuba i restart | `Nowa wersja: 3.53.0 (obecna: v3.52.10) … Aktualizacja pobrana OK` — **na urządzeniu była v3.52.10**, czyli v3.52.11 nigdy nie weszła |
| `/archiwum` po restarcie (00:26) | licznik próbek wraca z pliku, bez utraty doby | `20/1440 prob. 00:00-00:19` — dokładnie ten sam stan, co przed restartem |
| `/archiwum` o 00:32 | licznik rośnie dalej | `25/1440 prob. 00:00-00:30` (restart zjadł ~6 minut: 00:20–00:25) |
| `/archiwum 30` | **dwie** wiadomości | **nie udało się sprawdzić** — archiwum ma tylko **2 dni**, więc nie ma czego dzielić |
| `/archiwum30` (bez spacji) | — | **cisza**; do v3.53.1 nieznana komenda nie dostawała żadnej odpowiedzi |
| rollback OTA | brak komunikatu | brak — aktualizacja się potwierdziła |

**Wniosek:** podziału wiadomości nie da się sprawdzić na prawdziwych danych przez
najbliższy miesiąc (~25 dni potrzebnych linii). Stąd v3.53.1 z `/archiwum_test`.

### 2026-10-21, 00:56 — `/archiwum_test` potwierdził podział (v3.53.1)

Zgłoszone przez użytkownika: przyszły **dwie wiadomości**.

```
wiadomosc 1/2: TEST podzialu — ... + naglowek + 2026-09-22 ... 2026-10-15
wiadomosc 2/2: (ciag dalszy) + 2026-10-16 ... 2026-10-21 + Suma GHI 54.45 (30/30)
```

Treść zgadza się linia w linię z wydrukiem symulatora (S24, wariant `2026-10-21`):
te same dni, te same pokrycia (87.5%, 95.9%, … 96.5%), ta sama suma **54,45 kWh/m²**.
Czyli droga „String → kolejka → `sendTelegramMessage()` → API Telegrama” nie zmienia
tego, co przewidział test hostowy.

**To zamyka ostatnią rzecz, której nie dało się sprawdzić na komputerze.**
Prawdziwy raport zacznie przychodzić w dwóch wiadomościach, gdy archiwum uzbiera
**25 dni** (przy obecnych ~150 B na linię 24 dni to jeszcze 3656 B, 25 dni to 3809 B).
Archiwum ma 2 dni, więc nastąpi to około **13.11.2026** — i wtedy `(ciag dalszy)`
w raporcie nie jest awarią, tylko tym, co właśnie sprawdziliśmy.

Sprawdzenie podziału przenosi się na `/archiwum_test` (v3.53.1). Jeżeli przyjdzie
jedna wiadomość albo żadna — podział nie działa, patrzeć na `/logi`. Jeżeli druga
wiadomość przyjdzie **sama z nagłówkiem** — wrócił błąd z v3.53.0 (prefiks wysyłany
osobno), ale ten kod jest pokryty testem (`checkSplit`, punkt 3).

### 2026-10-22, 07:00 — pierwsza doba zamknięta po słonecznym dniu

```
2026-10-22  GHI 0.19 kWh/m2  dT -1.2°C  amplituda 1.5°C  (min 33.8 @ 06:41 / max 35.2 @ 00:14)
            pokrycie 29.2% (100.0% czasu)  421/1440 prob. 00:00-07:00 (w toku)
2026-10-21  GHI 2.47 kWh/m2  dT +13.8°C  amplituda 19.5°C  (min 19.8 @ 02:34 / max 39.2 @ 15:23)
            pokrycie 99.0%  1425/1440 prob. 00:00-23:59 TECH
```

**Odpowiedź na pytanie z 18:06: zbiornik przetrwał noc ze stratą zaledwie 1,4 K**
(35,2 °C o 00:14 → 33,8 °C o 06:41, czyli 0,22 K/h). To scenariusz lepszy, nie gorszy —
termosyfon + 100 l trzyma ciepło. Wcześniejszy wariant „spadnie do ~22 °C” się nie zdarzył,
bo tamten zbiornik był po prostu zimny po pochmurnym 20.10, nie dlatego, że izolacja jest słaba.

**21.10 zamknięta: 99,0% (1425/1440) — 15 brakujących minut na całą dobę** i znacznik `TECH`.

**`dT` spadło z +15,8 do +13,8 przy zamknięciu doby — i tak ma być.** `dT` to
„koniec dnia minus początek”, a nie „ile się nagrzało”:

| godzina | temperatura | `dT` |
|---|---|---|
| 00:00 | 21,2 °C | — |
| 15:23 | 39,2 °C (maksimum) | — |
| 18:06 | 37,0 °C | **+15,8** |
| 23:59 | 35,0 °C | **+13,8** |

Przez ostatnie 6 godzin doby zbiornik stracił 2,0 K i `dT` zmalało o te same 2,0.
**`amplituda 19,5` ani drgnęła** — i to jest dowód, po co w raporcie są obie liczby.
Kto patrzyłby tylko na `dT`, pomyślałby, że dzień się „popsuł”.

**Rozkład strat ciepła (dwa dni, ten sam wzorzec):**

| okres | strata | tempo |
|---|---|---|
| 20.10: 25,0 → 21,2 (14:20 → 23:51) | 3,8 K | 0,40 K/h |
| 21.10: 39,2 → 35,0 (15:23 → 23:59) | 4,2 K | 0,49 K/h |
| noc 21→22: 35,2 → 33,8 (00:14 → 06:41) | 1,4 K | **0,22 K/h** |
| noc 20→21: 21,2 → 19,8 (23:51 → 02:34) | 1,4 K | 0,52 K/h |

Wieczorem **zawsze ~4 K** w 8–9 godzinach — to powtarzalne. Noc jest nieregularna:
21→22 bardzo spokojna (0,22 K/h), a 20→21 miała 1,4 K w 2,7 h, po czym o **03:35**
czujnik pokazał **21,8 °C**, czyli wzrost o 2 K w środku nocy bez słońca.

**Możliwe wyjaśnienia wzrostu o 03:35** (nie rozstrzygam — brak danych):
- stratyfikacja: chłodniejsza woda przy ściance opada, cieplejsza z góry przechodzi
  obok czujnika w połowie wysokości (ten czujnik nie widzi rozkładu, patrz sekcja 3),
- chwilowy zanik cyrkulacji / cofnięcie termosyfonu.
Nie wpływa to na liczby dobowe (minimum 19,8 i tak jest prawdziwe), ale pokazuje,
czego ten czujnik nie potrafi rozstrzygnąć.

**Dziś (22.10) prognoza to 0,19 kWh/m² — 13× mniej niż wczoraj (2,47).** Przy takim
nasłonecznieniu zbiornik **będzie tracił** ciepło przez cały dzień. Realnie: zejście
o kilka K w ciągu dnia, a `dT` dla 22.10 pozostanie **ujemne** — to nie awaria, tylko
pochmurny dzień. Wzrost temperatury zacznie się dopiero przy pierwszej pogodnej dobie.

**`(100.0% czasu)` tym razem jest prawdziwe:** 421 próbek w 420 upłyniętych minutach
(przekroczenie 0,2%, w granicach tolerancji `r <= 1.02` z v3.53.0). Inaczej niż 21.10,
gdzie restart OTA zjadł 6 minut i pokrycie czasu spadło do 95,3%.

### 2026-10-21, 18:06 — najlepsza doba w dotychczasowej historii

```
2026-10-21  GHI 2.47 kWh/m2  dT +15.8°C  amplituda 19.5°C
            (min 19.8 @ 02:34 / max 39.2 @ 15:23)  pokrycie 74.4% (98.4% czasu)
            1072/1440 prob. 00:00-18:06 (w toku)
```

| godzina | temperatura | co się działo |
|---|---|---|
| 00:00 | 21,2 °C | poziom z końca 20.10 |
| 02:34 | **19,8 °C** | minimum nocne (wychłodzenie 1,4 K) |
| 11:33 | 29,0 °C | +7,8 K od północy (raport z 11:35) |
| **15:23** | **39,2 °C** | **maksimum doby** |
| 18:06 | 37,0 °C | stygnięcie **0,81 K/h** po zachodzie słońca |

**Odtworzone w symulatorze znak w znak** z tymi samymi liczbami.

**Amplituda 19,5 przy min 19,8 i max 39,2 (różnica 19,4) to nie błąd.** W pliku są
pełne wartości — najprawdopodobniej max 39,25 i min 19,75. `printf("%.1f")` zaokrągla
je do 39,2 i 19,8 (połówka do liczby parzystej), a amplituda liczona jest z wartości
pełnych: 39,25 − 19,75 = 19,50 → `19.5`. Dlatego trzy liczby w jednym wierszu nie
muszą się „dodawać” co do dziesiątej.

**Energia — szacunek, nie pomiar:** 19,5 K × 0,1163 kWh/K ≈ **2,27 kWh** w 100 l wody.
Przy aperturze ~1,4 m² i prognozie 2,47 kWh/m² (≈3,46 kWh padającego) daje to **~66%**.
To **za dużo na realną sprawność**, więc jedno z założeń jest zbyt ostrożne:

1. **GHI w rekordzie to PROGNOZA, nie pomiar.** Jeśli dziś było realnie więcej niż
   2,47 kWh/m², mianownik jest zaniżony — i to najbardziej prawdopodobne wyjaśnienie.
2. **Apertura mogła być zaniżona.** Wcześniej (20.10) z 3,2 K wyszło ~1,4 m² przy
   założeniu 45–50% sprawności. Dziś ten sam rachunek daje 66% — czyli albo apertura
   jest większa (10 rur próżniowych to więcej niż 1,4 m² powierzchni czynnej),
   albo sprawność jest wyższa.
3. **Czujnik w połowie wysokości nie widzi stratyfikacji.** W słoneczny dzień góra
   zbiornika jest cieplejsza od środka; średnia temperatura wody rośnie mniej niż
   wskazanie czujnika, więc „energia z 19,5 K” jest raczej **zawyżona**.

Wniosek: **do kalibracji aperatury potrzebny jest pomiar nasłonecznienia, nie prognoza.**
Do tego czasu każdą liczbę kWh traktować jako rząd wielkości, nie wynik.

**Do sprawdzenia rano (22.10):** jaką temperaturę pokaże minimum. Jeśli zbiornik
przetrwa noc z 37,0 °C do ~30 °C, to termosyfon + 100 l trzyma ciepło lepiej, niż
zakładaliśmy; jeśli spadnie do ~22 °C, to 15 K ucieka w ciągu nocy i wieczorny
odczyt 37,0 °C jest mniej wart niż wygląda.

### 2026-10-21, 06:19 i 11:35 — pierwsze raporty z v3.53.1 (prawdziwe dane)

```
06:19  GHI 2.47 kWh/m2  dT +0.2°C  amplituda 2.0°C  (min 19.8 @ 02:34 / max 21.8 @ 03:35)
       pokrycie 25.1% (95.3% czasu)  361/1440 prob. 00:00-06:15 (w toku)
11:35  GHI 2.47 kWh/m2  dT +7.8°C  amplituda 9.2°C  (min 19.8 @ 02:34 / max 29.0 @ 11:33)
       pokrycie 47.2% (97.8% czasu)  680/1440 prob. 00:00-11:34 (w toku)
```

Oba wiersze odtworzone **znak w znak** przez symulator z tymi samymi liczbami —
urządzenie liczy to samo, co test hostowy.

**Rachunek się zgadza co do dziesiątej:**
- pierwsza próbka doby 00:00 = **21,2 °C** (dokładnie tyle, ile koniec 20.10),
- `dT +7.8` = 29,0 − 21,2 ✓ (o 11:34 ostatnia próbka = 29,0, czyli maksimum z 11:33),
- `amplituda 9.2` = 29,0 − 19,8 ✓ (19,8 to minimum nocne o 02:34),
- `dT +0.2` o 06:19 = 21,4 − 21,2 ✓ (po nocnym spadku zbiornik wrócił nad poziom północy).

**Dlaczego `dT` i `amplituda` się różnią:** `dT` liczy od północy (21,2 °C), a amplituda
od nocnego minimum (19,8 °C). Różnica 1,4 °C to wychłodzenie nocne — dlatego obie liczby
są w raporcie obok siebie.

**Brakujące minuty:** o 06:19 brakowało 18 z 379 minut (95,3% czasu), o 11:35 — 15 z 695
(97,8%). Z tego ~6 minut to restart przy OTA (~00:20–00:25), reszta to pojedyncze minuty
gubione przez zadanie próbkujące co 60 s, gdy akurat trwa wysyłka do Telegrama albo zapis
checkpointu. **100% nigdy nie będzie** i tak ma być — dlatego pokazujemy dwie liczby:
udział w całej dobie (25,1%) i udział w czasie, który upłynął (95,3%).

**Szacunek energii (nie pomiar):** 9,2 K × 0,1163 kWh/K ≈ **1,07 kWh** w 100 l wody.
Czujnik jest w połowie wysokości i nie widzi stratyfikacji, więc góra zbiornika jest
prawdopodobnie cieplejsza — wartość raczej zaniża niż zawyża.

### 2026-10-21, raport o ~00:05, wersja v3.52.10

```
Archiwum solarne (2 dni, z centrali)
2026-10-21  GHI 2.47 kWh/m2  dT +0.0°C  amplituda 0.0°C  (min 21.2 @ 00:00 / max 21.2 @ 00:00)
            pokrycie 0.3% (83.3% czasu)  5/1440 prob. 00:00-00:04 (w toku)
2026-10-20  GHI 0.55 kWh/m2  dT -2.0°C  amplituda 3.8°C  (min 21.2 @ 23:51 / max 25.0)
            pokrycie 98.0%  1411/1440 prob. 00:00-23:59 TECH
Suma GHI: 3.02 kWh/m2 (2/2 dni z prognoza) | odczyty: 2/2
```

**Co jest poprawne:**
- 21.10 wskoczył do sumy i do „odczyty 2/2”, bo **dobę zaczęto zbierać** (5 próbek).
  Suma 3.02 = prognoza 20.10 (0.55) + prognoza 21.10 (2.47).
- 20.10 domknął się z **TECH** — pełne pokrycie 98.0%, 1411/1440, bez symulacji.
- **`@` wreszcie działa**: `min 21.2 @ 23:51` — nowe minimum padło już po wgraniu v3.52.10,
  więc dostało godzinę. `max 25.0` zostało odtworzone ze starego rekordu, więc godziny
  nie ma i mieć nie będzie. To potwierdza mechanizm z sekcji o `@`.
- 21.10 pokazuje `@ 00:00` przy obu ekstremach, bo pierwsza próbka doby **jest** i minimum,
  i maksimum (wszystkie 5 odczytów to 21,2 °C).

**Co było mylące (poprawione w v3.52.11):** `dT +0.0°C / amplituda 0.0°C` po pięciu próbkach
oraz `(83.3% czasu)` policzone z sześciu minut.

### 2026-10-20/21, raport o ~23:45, wersja v3.52.10

```
Archiwum solarne (2 dni, z centrali)
2026-10-21  GHI 2.47 kWh/m2  prognoza na jutro (doba sie nie zaczela)
2026-10-20  GHI 0.55 kWh/m2  dT -1.8°C  amplituda 3.5°C  (min 21.5 / max 25.0)
            pokrycie 97.0% (98.0% czasu)  1397/1440 prob. 00:00-23:43 (w toku)
Suma GHI: 0.55 kWh/m2 (1/1 dni z prognoza) | odczyty: 1/1
Jutro (prognoza): GHI 2.47 kWh/m2 — doba sie jeszcze nie zaczela.
```

**Obie poprawki v3.52.10 działają:** przyszła doba nie udaje braku odczytów, nie wchodzi
do sumy GHI ani do mianownika, a `TECH` zniknął z doby w toku.

**`@` nadal nie ma — i tak ma być.** Doba 20.10 trwała w chwili wgrywania v3.52.10.
Restart odtworzył z pliku `minC = 21.5`, `maxC = 25.0`, ale godziny tych ekstremów nie
istniały w starym rekordzie (`minUtc = 0`). Późniejsze próbki (~23,4 °C) nie biją żadnej
z granic, więc znaczniki zostaną zerami do końca doby. Sprawdzone symulacją:
po restarcie starych granic 16 nowych próbek nie ustawia godziny; doba zaczęta od zera
ustawia ją od pierwszej próbki. **21.10 będzie miał `@`.**

Próbki: 1387 → 1397 w ciągu 10 minut = dokładnie 1/min, bez luk.

### 2026-10-21 → raport o ~23:35, wersja v3.52.8

```
Archiwum solarne (2 dni, z centrali)
2026-10-21  GHI 2.47 kWh/m2  brak odczytow
2026-10-20  GHI 0.55 kWh/m2  dT -1.8°C  amplituda 3.5°C  (min 21.5 / max 25.0)
            pokrycie 96.3% (98.1% czasu)  1387/1440 prob. 00:00-23:33 (w toku) TECH
Suma GHI: 3.02 kWh/m2 (2/2 dni z prognoza) | odczyty: 1/2
```

**To nie była awaria.** O 23:30 firmware zapisał z góry plik jutra z prognozą GHI
(celowo, żeby restart o 23:59 nie zgubił prognozy). Doba 21.10 jeszcze się nie zaczęła,
więc odczytów być nie mogło. `TECH` wziął się z tolerancji 30 minut w `completeCoverage()`.
Oba komunikaty poprawione w v3.52.10. Brak `@` przy min/max = zapis sprzed v3.52.9.

### 2026-10-20 (v3.52.8, raport o ~23:01)

```
2026-10-20  GHI 0.55 kWh/m2  dT -1.5°C  amplituda 3.2°C  (min 21.8 / max 25.0)
            pokrycie 94.6% (98.6% czasu)  1362/1440 prob.  00:00-22:58 (w toku)
```

- **`dT -1.5°C` to dokładnie ta pomyłka, dla której dodano amplitudę.** Zbiornik zaczął dobę
  ciepły (25,0 °C po słonecznym 19.10), wystygł nocą do 21,8 °C, w dzień wrócił do 25,0 °C
  i wieczorem zszedł do ~23,5 °C. `dT` widzi tylko „start 25,0 → teraz 23,5” = **−1,5 °C**,
  czyli „woda się wychłodziła”. Amplituda **3,2 °C** pokazuje realny zakres doby.
- **Amplituda też nie jest czystym uzyskiem.** Miesza wychładzanie nocne (25,0 → 21,8)
  z grzaniem dziennym (21,8 → 25,0). W tym dniu oba ruchy mają prawie tę samą wartość,
  więc 3,2 °C przypadkiem odpowiada grzaniu — ale to zbieg okoliczności, nie reguła.
  Bez godzin min/max nie da się tego rozdzielić.
- **Pokrycie:** 1362/1440 = 94,6% doby, ale 1362 z ~1381 upłyniętych minut = **98,6%**.
  Brakuje ~19 minut — tyle, ile trwa kilka restartów przy wgrywaniu firmware'u.
- **Kalibracja instalacji:** 3,2 °C × 0,1163 kWh/K = **0,372 kWh** ciepła w wodzie przy
  GHI 0,547 kWh/m². Przy sprawności 45–50% odpowiada to aperaturze **~1,4 m²**,
  a nie ~1 m², jak zakładałem w szacunkach z 19–20.10. 10 rur próżniowych ma większą
  powierzchnię czynną, niż przyjąłem — **poprawka na przyszłość**.
- Konsekwencja: prognoza „~9 K z 2,449 kWh/m²” na 21.10 była policzona dla zbyt małej
  apertury. Realistyczniej **11–13 °C**, nadal jako szacunek, nie pomiar.

---

## 14. Przelicznik energii — 100 l, nigdy 200 kg

**Zgłoszenie użytkownika 2026-10-22:** „JAKIE 200 KG WODY". Słusznie.

W instalacji jest **zbiornik 100 l** (§3). Tymczasem aplikacja Android liczy
energię dla **200 kg wody** — liczba przejęta z `Piec.html` i nigdy nie
dopasowana do tej instalacji.

### 14.1 Co pokazuje aplikacja, a co jest w instalacji

| wielkość | instalacja (ustalenie) | kod APK v0.31.25 | błąd |
|---|---|---|---|
| masa wody w przeliczniku | **100 l** | `200` — `SolarAnalytics.kt:146` | **2× za dużo** |
| równoważnik energii | **0,1163 kWh/K** | 0,2326 kWh/K | **2× za dużo** |
| powierzchnia kolektora (prognoza) | 10 rur, ~1,4 m² czynnej (§13, kalibracja 20.10) | `2.0` m² — `SolarAnalytics.kt:137` | ~1,4× za dużo |
| model zysku w °C | apertura 1,0 m² × sprawność 0,49 | `4.2 °C/(kWh/m²)` — `SolarAnalytics` | **poprawne** (patrz §15.1) |

Rachunek kontrolny:

```
100 l → 100 × 4,186 / 3600 = 0,1163 kWh/K     ← tyle ma być
200 kg → 200 × 4,186 / 3600 = 0,2326 kWh/K    ← tyle liczy APK
                                              stosunek = dokładnie 2,00
```

**Skutek:** „~X kWh" pod „ZYSKIEM BRUTTO SŁOŃCA" jest zawyżone **dwa razy**.
Przy 20 °C przyrostu APK pokaże ~4,65 kWh, a w 100 l jest ~2,33 kWh.

### 14.2 Czego nie wiadomo

- ~~`4.2` nie ma zapisu, czym jest~~ — **sprawdzone 2026-10-22 (§15.1).**
  `4.2` to **°C na 1 kWh/m²** padającego słońca: apertura 1,0 m² × sprawność
  dzienna 0,49 ÷ 0,1163 kWh/K = 4,21. Zgodne z katalogami kolektorów
  10 rur (0,90–1,12 m²). Mnożenie przez `* 10` to tylko sposób na jedno
  miejsce po przecinku (`round(x*42)/10`), nie dodatkowy mnożnik.
- Prognoza liczona z zachmurzenia (`(100 − cloud) × 8,5`) była osobnym,
  gorszym modelem — usunięta w v0.31.27 (§15.2).

### 14.3 Reguła

> **Liczby o instalacji bierze się z tego dokumentu, nigdy z kodu aplikacji.**

Kod może być źródłem tego, *co* się liczy — ale nie tego, *ile* jest wody,
rura, metrów czy litrów. 200 kg w kodzie nie jest ustaleniem, jest
pozostałością.

### 14.4 Decyzja: wariant **B** (2026-10-22)

| wariant | zmiana | efekt |
|---|---|---|
| A | `200` → `100` w `SolarAnalytics.kt` | kWh zgodne z 100 l, reszta bez zmian |
| **B — wybrany** | A + usunięcie `~X kWh` z kafelków | zostaje tylko °C |
| C | A + jedna stała `KWH_PER_K = 0.1163` | jedna liczba na całą aplikację |

**Uzasadnienie B:** z §10 i §13 wynika, że w liczbie kWh siedzą **trzy**
niepewności naraz — GHI jest prognozą, a nie pomiarem, apertura i sprawność
nie były mierzone, a czujnik w połowie wysokości nie widzi stratyfikacji.
Taka liczba wygląda na pomiar, którym nie jest. Stopnie Celsjusza są
policzalne z czujnika — kWh nie były. Zostaje to, co da się obronić.

### 14.5 Co zmieniono w kodzie (v0.31.26, `versionCode 3032`)

| plik | zmiana |
|---|---|
| `core/SolarAnalytics.kt` | `estKwh()`: `* 200` → `* 100` (0,1163 kWh/K) |
| `ui/screens/Weather.kt` | „⚡ ZYSK BRUTTO SŁOŃCA” i „🔮 PROGNOZA ZYSKU” bez podpisu kWh; karty 7 dni bez linii kWh |
| `ui/screens/sheets/Sensors.kt` | „Zysk brutto słońca” i „Prognoza zysku” bez `· ~X kWh` |
| `app/build.gradle.kts` | 3032 / `v0.31.26` |

**Nietknięte:** ΔT, pokrycie, pobory wody, suma GHI, eksport CSV, tryb serwa.
`forecastKwh` zostaje w kodzie jako wartość, ale **nie jest nigdzie wyświetlana** —
gdy pojawi się pomiar nasłonecznienia, wróci bez zgadywania.

---

## 15. DZIŚ ≠ suma bufora — naprawa modelu (v0.31.27, APK 3033)

**Zgłoszenie 2026-10-22 (zrzut ekranu):** karta „DZIŚ" pokazywała
**+93,1 °C**, gdy model na dziś dawał **+9,1 °C** (289 W/m² → 2,17 kWh/m² × 4,2).
Ta sama liczba 93,1 widniała w „Zysku brutto słońca".

### 15.1 Stała 4,2 °C/(kWh/m²) — skąd się bierze

Dane katalogowe kolektorów **10 rur 58/1800**:

| kolektor | apertura | sprawność optyczna |
|---|---|---|
| SOLEKO APS 10 | 0,90 m² | 68,7% |
| Projprzem PE10-58 | 1,00 m² | 81,5% (absorber) |
| IB-Sol 10 | 1,12 m² | 65,8% (apertura) |
| Hewalex KSR10 | 1,01 m² | 78% |

Uzysk roczny kolektora próżniowego: **450–600 kWh/m²**
(`powietrze.malopolska.pl`, `darmoweogrzewanie.com`).

```
apertura 1,0 m² × sprawność dzienna 0,49 ÷ 0,1163 kWh/K = 4,21 °C/(kWh/m²)
```

**Wniosek:** model dzienny był poprawny od początku. Błąd nie leżał
w `4.2` — leżał w tym, że karta „DZIŚ" podstawiała pod niego sumę
narastającą z bufora. Poprzedni wpis „nieodtwarzalny" w §14 był błędny
i został poprawiony.

### 15.2 Co było zepsute

| defekt | kod | skutek na zrzucie |
|---|---|---|
| „DZIŚ" brało sumę z bufora | `estGainC = maxOf(accumulatedGrossGain, modelGain)` | +93,1 zamiast +9,1 |
| prognoza liczona innym wzorem | `(100 − cloud) × 8,5` vs `pik × 7,5 × 4,2` | +0,0 °C obok +19,0 °C |
| brak sufitu dobowego | mnożenie GHI bez ograniczenia | możliwe +93 °C dla 100 l |

### 15.3 Jak jest teraz

- **DZIŚ** = suma przyrostów z próbek **od lokalnej północy**; gdy brak
  próbek — model. Suma narastająca **nigdy** nie wchodzi na to miejsce.
- **Jeden wzór** dla DZIŚ, JUTRO i kart 7 dni:
  `ΔT = min(GHI_kWh/m² × 4,2 ; 45 °C)`. Wejście: prognoza
  `shortwave_radiation`, a gdy jej brak dla godziny — szacunek z zachmurzenia.
- **Sufit 45 °C** na dobę: 100 l od ~20 °C nie zagotuje się w jeden dzień.
- **Arkusz Czujniki** rozdziela „dziś" i „narastająco" — dwie osobne linie,
  każda z własną etykietą.

### 15.4 Czego to nadal nie jest

Model nie zna: temperatury startowej wody, strat zbiornika, kąta padania,
zacienienia, sprawności zależnej od różnicy temperatur. To **rząd
wielkości** — zgodnie z §10. Model nie zastępuje pomiaru; on tylko mówi,
czego się spodziewać, żeby porównać z tym, co pokaże czujnik.

### 15.5 Paczka

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.27-DZIS.zip` — 25 558 B, 5 wpisów |
| sha256 | `a8edd4e5729c85a1fa7f8a8184a162058a706047610745389a107d3e86500ee6` |
| pliki | 4: `build.gradle.kts`, `SolarAnalytics.kt`, `Weather.kt`, `Sensors.kt` |

### 15.6 Błąd kompilacji i poprawka (ten sam numer 3033)

Pierwsze wydanie v0.31.27 nie skompilowało się:

```
e: SolarAnalytics.kt:188:49 None of the following candidates is applicable:
   fun times(other: Byte): Double … fun times(other: Short): Double
```

Przyczyna — `if` z gałęziami **różnych typów liczbowych**:

```kotlin
((100.0 - h.cloud) * 8.5 * (if (h.isDay) 1 else 0.0))
//                                    ↑ Int   ↑ Double  → wynik: Number, nie Double
```

Kotlin nie wybiera `Double` po cichu. Typ wspólny `Int` i `Double` to
`Number & Comparable<...>`, a `Double * Number` nie istnieje.

Poprawka: `(if (h.isDay) 1.0 else 0.0)`.

**Reguła:** w wyrażeniu arytmetycznym obie gałęzie `if` muszą mieć ten sam
typ liczbowy. `1 else 0.0` wygląda niewinnie, a wywala cały build.

Paczka poprawiona pod tym samym numerem (3033), nowy sha256
`a8edd4e5…` zastępuje `41bc30e2…`.

### 15.7 Build po poprawce

```
BUILD SUCCESSFUL in 22s
37 zadań (6 wykonanych, 31 z cache)
versionCode = 3033, versionName = "v0.31.27"
```

Zero błędów. 10 ostrzeżeń, wszystkie sprzed tej zmiany (`VIBRATOR_SERVICE`,
`quadraticBezierTo` w `Weather.kt` i `SvgPath.kt`, `statusBarColor`, trzy
`Condition is always 'true'` w `ChartEngine.kt`).

---

## 16. Panel solarny po ludzku (v0.31.28, APK 3034)

Zgłoszenie: „Narastającą jest zbędna, resztę zrób tak aby była łatwa do
zrozumienia dla nowej osoby".

### 16.1 Dlaczego suma narastająca musiała zniknąć

`gain` liczy **każdy** wzrost, a **każdy spadek pomija**:

```kotlin
val deltaP = maxOf(0.0, tPanel  - prev.tPanel)
val deltaB = maxOf(0.0, tBojler - prev.tBojler)
gain = maxOf(deltaP, deltaB) + (if (isDraw) drawDrop else 0.0)
```

To prostownik szumu: drobne wahania czujnika o 0,3 °C w tę i z powrotem
dodają się jak realne grzanie. Na zrzucie: **+93,1 °C z 206 próbek**, gdy
woda w zbiorniku miała 33 °C. Liczba nie miała odpowiednika w rzeczywistości
— więc została **usunięta z ekranu**, nie przemianowana.

To samo dotyczyło „Poborów wody narastająco".

### 16.2 Zasada: każda liczba mówi, skąd jest

| wiersz | źródło |
|---|---|
| Panel teraz / powietrze | czujnik |
| Różnica panel − powietrze | czujnik (różnica) |
| **Zysk dziś · zmierzone** | czujnik, próbki od północy |
| **Prognoza na dziś · szacunek** | model z prognozy GHI |
| Pobrania ciepłej wody | czujnik, wykryte spadki |

Pod tabelą jest jedno zdanie wyjaśnienia dla kogoś, kto widzi to pierwszy raz:

> Zysk = o ile °C ogrzała się woda w zbiorniku 100 l. „Zmierzone" to odczyt
> czujnika w zbiorniku, „szacunek" to wyliczenie z prognozy słońca — nie pomiar.

### 16.3 Karta „DZIŚ" w siatce prognozy

Wcześniej pokazywała **pomiar**, a pozostałe dni **prognozę** — w jednej
siatce pod tytułem „PROGNOZA POTENCJAŁU". Teraz wszystkie siedem kart to
prognoza, łącznie z DZIŚ. Pomiar dzisiejszy jest tam, gdzie jego miejsce:
w „Zysku dziś · zmierzonym".

Pod nagłówkiem sekcji doszedł podpis:
`o ile °C może ogrzać się woda w zbiorniku 100 l · szacunek, nie pomiar`.

### 16.4 Zmiany nazw (bez skrótów)

| było | jest |
|---|---|
| Panel teraz / zewnątrz | Panel teraz / powietrze |
| ΔT panel − zewnątrz | Różnica panel − powietrze |
| Zysk dziś | Zysk dziś · zmierzone |
| Prognoza zysku · dziś | Prognoza na dziś · szacunek |
| Pobory wody dziś | Pobrania ciepłej wody |
| ΔT PANEL - ZEWN. (kafelek) | PANEL − POWIETRZE |
| POBORY DZIŚ (kafelek) | POBRANIA DZIŚ |

### 16.5 Paczka

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.28-CZYTELNE.zip` — 21 222 B, 4 wpisy |
| sha256 | `0263c4fb28fa46870410ec180d5bb25e6bb73d2e44476b9ccccca26fb0e4fe6e` |
| pliki | 3: `build.gradle.kts`, `Weather.kt`, `Sensors.kt` |

Przy okazji: `SolarDayCard` stracił nieużywany parametr `m` (karta nie czyta
już stanu aplikacji, dostaje tylko godziny).
