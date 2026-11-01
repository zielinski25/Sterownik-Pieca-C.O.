# Spadki temperatury w zbiorniku wieczorami — pobór wody czy wychładzanie?

Data: 2026-10-29. Dane: eksport RTDB `akwarium-367be-default-rtdb-10-export (1).json`
(01.10 01:00 → 29.10 10:51), 40 789 próbek minutowych.

Pytanie użytkownika: *„powtarzalność spadku temperatury w tym zbiorniku wieczorami
w zależności od temperatury zewnętrznej — to może nie być pobór, tylko ochłodzenie"*.

---

## 1. Skąd wiadomo, która kolumna to woda w zbiorniku

Nie zgadywane. Sprawdzone na archiwum solarnym z centrali (`/archiwum_solarne`):

| dzień | archiwum Telegram | kolumna 7 `hist_short` |
|---|---|---|
| 28.10 | min 16.8 @ 07:20 | 16.8 @ 07:20 ✔ |
| 28.10 | max 20.5 @ 11:49 | 20.5 @ 11:49 ✔ |

Kolumna 7 = `a[5]` = `czujnik_temp_Panel_Plus` = woda w zbiorniku 100 l.
Kolumna 2 = `a[0]` = temperatura powietrza.

## 2. Jakość danych — 376 próbek odrzuconych

- **376 próbek** z `a[5]` = 0,0 °C. To brak odczytu, nie temperatura. Odrzucone.
- **03.10 godz. 06:00** pokazywało 0,0 °C — bez czyszczenia wyszłoby „spadek 34,3 °C",
  czyli fałszywy rekord. Po oczyszczeniu: 21,8 °C.
- Rozdzielczość czujnika **0,1 °C** przy próbce co minutę ⇒ **regresja minutowa jest
  bezużyteczna** (dT/dt to szum kwantyzacji). Dlatego wszystkie wnioski liczone na
  agregatach 4- i 8-godzinnych, nie na minutach.

## 3. Wynik główny: to NIE jest pobór wody

Test: skok ≥ 0,4 °C między kolejnymi próbkami (≤3 min) = sygnatura poboru.

| | liczba nocy |
|---|---|
| wszystkich nocy z danymi | 28 |
| **bez ani jednego skoku** | **26** |
| ze skokami (pobór) | 2 (03.10 — 241 skoków, 25.10 — 35 skoków) |

**26 z 28 nocy nie ma śladu poboru wody.** A spadki tam są — mediana **2,3 °C
w godzinach 18–22** i **3,3 °C w 22–06**. Czyli hipoteza użytkownika jest trafna:
to przeważnie **wychładzanie**, nie pobór.

Dowód dodatkowy — noc 16.10, największy spadek (12,2 °C w 4 h), **zero skoków**.
Co minutę, gładko:

```
18:00 38.5   18:10 36.8   18:20 35.0   18:30 33.5   18:40 32.3
```
Tempo **0,25 °C/min = 15 °C/h**, powietrze stałe 11,5 °C. Żadnego schodka.
Taki przebieg wyklucza pobór wody — pobór daje schodek, nie równię pochyłą.

## 4. Ale temperatura powietrza tego NIE tłumaczy

Regresja po 26 nocach bez poboru: `spadek/h = k · (T_woda − T_powietrze)`

```
k  = 0,0250 /h        R² = −0,047
```

**R² ujemne** = różnica temperatur nie ma żadnej mocy tłumaczącej. Przykłady wprost:

| noc | śr. różnica T−Ta | tempo spadku |
|---|---|---|
| 02.10 | 26,2 K | 1,49 °C/h |
| 11.10 | 24,1 K | 0,54 °C/h |
| 16.10 | 14,9 K | **1,85 °C/h** |
| 18.10 | 16,1 K | 0,18 °C/h |

Większa różnica temperatur potrafi dać **mniejszy** spadek. Gdyby to było zwykłe
przewodzenie przez ścianki zbiornika, tak być nie może.

## 5. Ile strat wynika z samych ścianek — a ile z reszty

Pojemność: 100 l × 4,186 kJ/(kg·K) = **418,6 kJ/K** (0,1163 kWh/K).

Model z `MODEL_SOLARNY_TRZEMCHA` przyjmuje `UA = 1,4 W/K`. Wtedy:

```
przy różnicy 10 K  -> 0,12 °C/h
przy różnicy 20 K  -> 0,24 °C/h
przy różnicy 30 K  -> 0,36 °C/h
```

Pomiar (mediana nocy bez poboru):

| faza | tempo | UA wyliczone z pomiaru |
|---|---|---|
| 18–22 (pętla ciepła) | 0,57 °C/h | 3,6 W/K |
| 22–06 (pętla zimna) | 0,42 °C/h | 2,9 W/K |

**Nawet w „zimnej" fazie nocy pomiar daje 2,1× więcej strat niż `UA = 1,4 W/K`.**
Wniosek: albo izolacja zbiornika jest gorsza niż katalog, albo dochodzi druga droga
ucieczki ciepła — pętla kolektora.

## 6. Rozkład dwumodalny — pętla „włącza się" albo „nie"

`UA` policzone osobno dla każdej nocy, faza 18–22:

```
1,1  1,5  1,7  1,7  1,8  2,0  2,1  2,2  2,2  2,5  2,9  2,9  3,3  3,3  3,8
5,7  5,8  6,3  6,3  7,3  8,8  9,7  9,8
13,9  18,5  18,8  19,4  27,6
```

Nie rozkład ciągły — **dwie grupy**. Większość nocy: 1–4 W/K. Kilka nocy: 14–28 W/K,
czyli **5–9× więcej**. To sygnatura mechanizmu włącz/wyłącz, nie płynnego przewodzenia.

Sprawdzenie na 7 dniach, dla których jest GHI z archiwum centrali:

| noc | GHI dnia | UA wieczorne |
|---|---|---|
| 25.10 | 2,13 | 19,4 |
| 27.10 | 2,00 | 13,9 |
| 24.10 | 1,83 | 9,7 |
| 23.10 | 2,65 | 6,3 |
| 26.10 | 0,53 | 5,7 |
| 28.10 | 0,48 | 2,2 |
| 29.10 | 0,27 | 2,2 |

Słoneczny dzień → następna noc z dużymi stratami. Pochmurny dzień → małe.
**7 punktów to za mało na dowód**, ale kierunek jest jednoznaczny i zgadza się
z fizyką: po słonecznym dniu kolektor jest gorący i pętla termosyfonowa ma czym
„pompować" ciepło w nocne niebo; po pochmurnym dniu kolektor jest zimny i nic nie krąży.

## 7. Kontrola źródeł z plików MD — co się potwierdza, a co nie

### 7.1 OSTI 116242 — POTWIERDZONE, ale z zastrzeżeniem

Źródło istnieje: *Performance analysis of a thermosyphon solar water heating system.
Part 2: Nighttime performance*, Ghoneim & Nassr, ASME 1995, OSTI ID 116242.
Cytat: „Deduced nighttime mass flow rates are in the order of **0.005 kg/sec**.
The effect of reverse flow was found to increase the system thermal losses by
**nearly fourfold**."

**Zastrzeżenie, którego MD nie podaje:** to instalacja z kolektorem **płaskim**
(1,8 × 0,8 m) i zbiornikiem **100 l**. Nasza instalacja ma kolektor **heat-pipe**
(10 rur próżniowych). MD przenosi „czterokrotnie" z płaskiego na heat-pipe bez
zaznaczenia tego — a to nie to samo, patrz 7.2.

### 7.2 Int. J. Low-Carbon Tech. 10(4) 2015, 430–437 — POTWIERDZONE i pasuje do naszych danych

Ong i in., *Reverse flow in natural convection heat pipe solar water heater*,
DOI 10.1093/ijlct/ctu017. Kluczowe zdania:

- „reverse flow always occurred in the evenings and was about **5–11 times less than
  forward flow**"
- „overnight mean water temperature drop was independent of the height... and ranged
  between **2 and 10 °C**"
- „The temperature drop was greater when the night was cooler."
- **„Nocturnal reverse flow in heat pipe collectors is LESS than that occurring in flat
  plate ones because the condenser section of the former is well insulated"**

Nasza mediana spadku nocnego: **5,6 °C** (2,3 + 3,3). Mieści się dokładnie w 2–10 °C
podanych przez to źródło. To mocne potwierdzenie, że instalacja zachowuje się normalnie.

Ale to samo źródło **osłabia** tezę o „czterokrotności": dla heat-pipe przepływ wsteczny
jest mniejszy niż dla płaskiego, więc 4× z OSTI nie przenosi się wprost.

### 7.3 Michaelides (EuroSun 2010) — MD nazywa to „sprzeczne", i ma rację

Źródło istnieje. Tezy: straty nocne są **liniowo** zależne od różnicy
temperatur zbiornik–otoczenie, współczynnik korelacji **0,93–0,95**, a przepływ
wsteczny ma wpływ „minimalny albo żaden". Dodatkowo: „energy losses of flat-plate
collectors during night time are mainly due to **conduction** rather than
recirculation".

**Nasze dane nie potwierdzają tej liniowości** (R² ≈ 0). Możliwe przyczyny rozjazdu:
- Michaelides miesza wodę w zbiorniku („thoroughly mixing") — u nas jest
  **stratyfikacja**, a czujnik siedzi w połowie wysokości,
- on liczy **energię** w kJ za całą noc, nie tempo w °C/h,
- u niego nie ma kolektora heat-pipe na dachu w Trzemchy.

Uczciwie: **nasz pomiar nie rozstrzyga sporu w literaturze**, ale jest z nim niesprzeczny —
pokazuje, że raz pętla działa, raz nie, a to właśnie wyjaśnia, dlaczego jedni widzą
liniowość, a inni nie.

### 7.4 Morrison 1986 — POTWIERDZONE pośrednio

Wymieniany przez oba nowsze źródła jako autor badania przepływu wstecznego. Nie
sprawdzane w oryginale.

### 7.5 Czego w literaturze nie ma

Nie znalazłem źródła, które podawałoby **UA zbiornika 1,4 W/K dla 100 l**.
Nasz pomiar mówi 2,9–3,6 W/K. To jest pozycja do sprawdzenia, bo bezpośrednio
wpływa na model.

## 8. Wnioski

1. **Hipoteza użytkownika jest trafna.** Spadki wieczorne to przeważnie wychładzanie,
   nie pobór wody — 26 z 28 nocy bez śladu poboru.
2. **Ale to nie jest wychładzanie „od temperatury powietrza".** Różnica temperatur
   zbiornik–powietrze nie tłumaczy spadków (R² = −0,047). Samo przewodzenie przez
   ścianki daje 2,1× mniej, niż widać.
3. **Brakujące ciepło ucieka przez pętlę kolektora**, i to skokowo: rozkład `UA`
   jest dwumodalny (1–4 W/K albo 14–28 W/K). Pętla albo pracuje, albo stoi.
4. **Model dwuwęzłowy z `MODEL_SOLARNY_TRZEMCHA` §18 jest właściwym podejściem** —
   jedyny, który w ogóle przewiduje spadek temperatury. Ale jego `UA = 1,4 W/K`
   jest **2× za małe** wobec pomiaru.
5. **Spadek nocny 5,6 °C mieści się w normie literaturowej** (2–10 °C, Ong 2015).
   Instalacja nie jest zepsuta.

## 9. Czego ten raport NIE twierdzi

- Nie twierdzi, że 4× z OSTI stosuje się do naszej instalacji (7.1, 7.2).
- Nie rozstrzyga sporu OSTI ↔ Michaelides (7.3).
- Nie ma danych o wietrze, zachmurzeniu nocnym ani nasłonecznieniu w płaszczyźnie
  kolektora dla większości nocy — korelacja z GHI opiera się na **7 nocach**.
- Jeden czujnik w połowie zbiornika nie widzi stratyfikacji ani rozkładu temperatur.

## 10. Co z tego wynika dla aplikacji (do decyzji, nie zrobione)

Nie zmieniono żadnego pliku `.kt`. Pozycje do rozważenia:

| pozycja | dziś | pomiar | skutek |
|---|---|---|---|
| `tankUa` | 1,4 W/K | 2,9–3,6 W/K | model zaniża straty nocne ~2× |
| `reverseFlowKgS` | 0,004 kg/s | niezmierzony | zgadza się z Ong 2015 (mniej niż 0,005 dla płaskiego) |
| karty 7-dniowe | `4,43·GTI − 5,57` | R² modelu nieznane | stała strata 5,57 °C/dobę nie zna temperatury wody |

---

## 11. Korekta po uściśleniu użytkownika (2026-10-29): instalacja BEZPOŚREDNIA, bez wężownicy

Użytkownik: *„instalacja na dachu to nie woda z wodociągów rurką idzie do zbiornika na dachu,
a druga rurka podłączona do zbiornika wylatuje już ciepła woda, tam nie ma żadnej wężownicy,
tam jest spychająca woda i wypychająca"*.

### 11.1 Potwierdzenie z literatury — to układ termosyfonowy BEZPOŚREDNI

| źródło | co mówi |
|---|---|
| instsani.pl, *Budowa i schematy instalacji solarnych* | „W instalacji z termosyfonem woda zimna doprowadzona jest **bezpośrednio** do zasobnika na dachu, a stąd po ogrzaniu przepływa do instalacji w budynku" |
| instsani.pl, *Zasobniki i podgrzewacze solarne* | „Z uwagi na sposób wymiany ciepła — **systemy bezpośrednie i pośrednie**. W pierwszym przypadku **woda użytkowa krąży z zasobnika do kolektora i z powrotem**" |
| pvsky.pl | „Zimna woda z sieci wpływa do dolnej części zbiornika, a stamtąd do szklanych rur próżniowych. Podgrzana woda, jako lżejsza, unosi się do górnej części zbiornika" |
| ecozone.solar (LP-100) | „W rurach próżniowych oraz zasobniku kolektora bezciśnieniowego **znajduje się woda**... cyrkulacja powoduje stały wzrost temperatury" |

**Wniosek: użytkownik ma rację.** To nie jest układ z wężownicą i glikolem. Woda użytkowa
krąży ze zbiornika do kolektora iz powrotem, grawitacyjnie, bez pompy.

### 11.2 Co to wyjaśnia w naszych danych

Rozkład `UA` był **dwumodalny** (1–4 W/K albo 14–28 W/K). W układzie **bezpośrednim**
to naturalne: nie ma wężownicy ani zaworu, który zatrzyma obieg wsteczny. Gdy różnica
gęstości wystarczy, cała woda ze zbiornika płynie przez kolektor i oddaje ciepło w niebo.
Gdy nie wystarczy — zostaje tylko przewodzenie przez ścianki.

W układzie **pośrednim** (glikol + wężownica) ten mechanizm jest odcięty od wody użytkowej.
Dlatego literatura (Michaelides, OSTI) daje różne wyniki — badano różne układy.

### 11.3 ROZSTRZYGNIĘTE: rury heat-pipe (wersja B), zbiornik 100 l bez zmian

Użytkownik potwierdził 2026-10-29: **„Na 100% to jest ta wersja B"** — rury heat-pipe:
sucha rura szklana, w środku miedziany pręt z oczkiem wchodzącym w górną belkę.

Skutki:

| pozycja | wniosek |
|---|---|
| pojemność cieplna | **100 l, 0,1163 kWh/K — BEZ ZMIAN** |
| woda w rurach | **nie ma** — w rurach krąży czynnik w zamkniętym pręcie |
| wariant „131 l / 0,152 kWh/K" | **odpada** |

To potwierdza też aperturę z MD §5.1: 10 rur 58/1800 daje aperturę ~0,95 m²,
a kod aplikacji liczy `apertureOptical = 0,678` (0,95 × η₀ 0,714) — zgadza się.

### 11.4 SPROSTOWANIE: nie ma ryzyka zamarznięcia rur

**Poprzednia wersja tego raportu (i moja odpowiedź) twierdziła, że woda w rurach
zamarznie zimą i rozsadzi rury. To było BŁĘDNE dla tej instalacji.**

Rura heat-pipe jest **sucha** — zawiera zamknięty miedziany pręt z niewielką ilością
czynnika, nie wodę użytkową. Woda jest tylko w zbiorniku, manifoldzie i rurach
zasilanie/powrót. Zamarznięcie grozi więc **wyłącznie obwodowi wodnemu**
(manifold + rury), a nie szklanym rurom.

Dodatkowo rury heat-pipe mają **właściwość diody cieplnej**: ciepło płynie tylko
od absorbera do manifolda, nigdy w drugą stronę (Alwaer & Gryzagoridis). To ogranicza
nocne straty przez same rury — ale **nie zatrzymuje obiegu wstecznego wody
w manifoldzie i rurach**, i to on odpowiada za straty zmierzone w §5–6.

Literatura potwierdza: Ong 2015 — „nocturnal reverse flow in heat pipe collectors
is LESS than that occurring in flat plate ones because the condenser section
of the former is well insulated".

### 11.5 Co zostaje do zrobienia zimą

Nie „spuść wodę z rur", a: **sprawdź, czy obwód wodny (manifold + rury) jest
opróżniany albo zabezpieczony przed mrozem.** Jeśli jest zawór spustowy — dobrze.
Jeśli nie ma, to jedyne zabezpieczenie to ruch wody (nie zamarznie, gdy krąży),
a w bezsłoneczny mróz krążenia nie ma.

---

## §12. SPROSTOWANIE (29.10, po wgraniu 3.56.7) — MOJA DIAGNOZA LUK BYŁA BŁĘDNA

### 12.1 Co twierdziłem wcześniej
Twierdziłem, że przy `TELEMETRY_BATCH_MAX_ATTEMPTS = 3` porzucona paczka 10 próbek
**przepada**, bo `telemetryPendingAbandon("attempts_exhausted")` kasuje ją z pamięci.
Na tym opierałem zmianę w v3.56.6 (progi 15→2 / 600→180 s + `g_spoolRuntimeForceSpill`).

### 12.2 Jak jest naprawdę — cytat z kodu (v3.56.7, funkcja `telemetryPendingAbandon`, ~l. 40178)

```cpp
// [v3.55.0] Zwolnienie bufora serializacji BEZ ruszania kolejki. Wolno to
// zrobic zawsze i tylko dlatego, ze probki NIGDY nie sa zdejmowane z ringu
// przed potwierdzonym sukcesem — porzucenie kopii JSON nie gubi danych,
// najwyzej opoznia wysylke do nastepnego cyklu.
static void telemetryPendingAbandon(const char *why) {
  if (!g_telemetryPendingValid) return;
  g_telemetryPendingValid = false;
  g_telemetryPendingCount = 0;
  ...
}
```

`telemetryPendingAbandon()` **nie dotyka ringu**. Zeruje tylko licznik i zwalnia bufor JSON.

Jedyne miejsca, które zdejmują próbki z ringu (`commitPopN`), to:

| linia | funkcja | warunek |
|---|---|---|
| 40224 | `telemetryFinalizeCompletedRequest()` | tylko po `ok == true` **i** zgodności `sampleSeq` ostatniej próbki |
| 40640 | `spoolRuntimeMaybeSpill()` | dopiero po zapisaniu strony, reopen i weryfikacji CRC |
| 36802 | `spoolGTest...` | tryb testowy |

Wszystkie trzy są zabezpieczone. **Nie ma ścieżki, w której porzucenie paczki gubi próbki.**

### 12.3 Wniosek dla v3.56.6
Punkty 2 i 3 zmiany v3.56.6 (progi spoola) oraz `g_spoolRuntimeForceSpill` **nie mogły
naprawić luk**, bo luki nie powstawały w tym miejscu. Zmiana była ryzykiem bez zysku.
To dodatkowo uzasadnia cofnięcie do zachowania v3.56.5 (wydanie v3.56.7).

### 12.4 Co nadal jest nieznane
Nagłówek pliku (l. 137–144) dokumentuje realnie zaobserwowaną lukę 24.10
10:03:05 → 10:14:05 = dokładnie 10 próbek, i 5 takich „równo 10,0 min" przerw
w 28 dni. Komentarz przypisuje to temu, że „tail ringu przesunął się nad nimi" —
ale to opis stanu **przed** v3.55.0, bo v3.55.0 wprowadziła kontrolę tożsamości.

**Hipotezy do sprawdzenia (żadna niepotwierdzona):**
1. Spóźnione ACK po timeoucie — `telemetryFinalizeCompletedRequest()` dostaje
   `success = true` od **porzuconego** żądania, `commitPopN(10)` zdejmuje próbki,
   których RTDB nigdy nie zapisał. Jedyna ścieżka, która pasuje do „dokładnie 10".
2. Spill → strona na LittleFS bez udanego replay → próbki są lokalnie, nie ma ich
   w chmurze. Wykluczone, jeśli `Pages created = 0`.
3. Filtrowanie po stronie odbiorcy (`tPanel = 0.0`, odrzucone 376 próbek) — nie
   daje przerw równych 10.

### 12.5 Czego potrzeba, żeby rozstrzygnąć
- `/spoolrt` **pełny** (`Samples spilled`, `Pages created`, `Ring N/1440`) — rozstrzyga hipotezę 2
- eksport RTDB + `/hist_short.csv` za **ten sam** przedział — porównanie trzech źródeł
- log z `DLOGF(DC_TELEMETRY, ...)` w oknie luki — `batch_ok` / `batch_fail` / `pending_abandon`

**Zasada na przyszłość:** nie zmieniać nic w spoolu, dopóki nie mamy dowodu z logu,
że luka powstaje w spoolu.

---

## §13. ZAMKNIĘCIE ŚLEDZTWA „DLACZEGO PIEC SIĘ RESTARTUJE" (29.10, po 12:40)

### 13.1 Ustalenie użytkownika
Użytkownik potwierdził: **„Dzisiaj dwa albo trzy razy wgrywałem"** firmware.

### 13.2 Dopasowanie do logu — pełna zgodność
W logu `piec_log (29).txt` są DOKŁADNIE trzy starty z partycji w stanie
`ESP_OTA_IMG_PENDING_VERIFY` (`zdarzenie=potwierdzenie_odroczone`):
10:30:34, 11:30:01, 12:22:07. Każdy z nich to świeżo zapisany obraz firmware.

| boot | start | koniec | czas | jak się skończył | zdrowie przed końcem |
|---|---|---|---|---|---|
| 3 (3.56.6) | 10:30:33 | 11:27:52 | 57 min | **wgranie nr 2** (użytkownik) | PING 11:20 i 11:25, heap 87 576→87 688, **0 błędów FB** |
| 4 (3.56.6) | 11:30:00 | 12:03:00 | 33 min | **NADZOR nr=1** o 12:08:15 | od 11:59:34 lawina timeoutów FB |
| 5 (3.56.6) | 12:08:15 | 12:20:40 | 12 min | **wgranie nr 3** (3.56.7) | PING 12:13 i 12:18, heap 87 964→87 492, **0 błędów FB** |
| 6 (3.56.7) | 12:22:06 | — | działa | — | obraz potwierdzony 12:32:07 |

### 13.3 Wniosek — sprostowanie Drugie
**Nie ma dowodu, że v3.56.6 była wadliwa.** Dwa z trzech jej rozruchów były
całkowicie zdrowe do ostatniej sekundy i zostały przerwane przez użytkownika,
który wgrywał firmware. Jedyny prawdziwy restart (boot=4) to zadziałanie
nadzoru po 300 s milczenia `tgTask`, a nie awaria oprogramowania jako takiego.

### 13.4 Co pozostaje realnym problemem
1. **NADZOR o 12:08:15.** `tgTask` milczał 300 s. Circuit breaker Firebase był
   otwarty, `FB-CMD`/`FB-SEND` dostawały TIMEOUT 5–6 s. To zdarzyło się RAZ.
   v3.56.5 miał 30 otwarć breakera w 4,5 dnia i nigdy nie osiągnął 300 s ciszy —
   więc próg jest blisko i przy gorszej sieci może zadziałać ponownie.
2. **Luki 10 próbek na wykresie.** Nierozwiązane. v3.56.7 = zachowanie v3.56.5,
   więc luki będą wracać. Patrz §12.
3. Literówka `vv3.56.7` w raporcie startowym (linia 34500:
   `F(" • 📦 Wersja: <b>v")` + `FIRMWARE_VERSION` = `"v3.56.7"`). Kosmetyka,
   obecna od dawna, do poprawy przy następnej paczce.

### 13.5 Decyzja
**Zostajemy na v3.56.7.** Powrót do v3.56.6 nie ma sensu: jego jedyna zmiana
funkcjonalna (progi spoola + force-spill) nie mogła naprawić luk (§12), a OTA
odrzuci downgrade — powrót wymagałby wydania v3.56.8. v3.56.7 jest
funkcjonalnie v3.56.5, który chodził 4 dni 15 h bez restartu.

**Zasada operacyjna: nie wgrywać firmware bez potrzeby.** Każde wgranie
zeruje uptime i niszczy dowody z logu.

---

## §14. WERYFIKACJA NA ŚWIEŻEJ BAZIE RTDB (29.10, eksport z 12:42)

Źródło: `google_drive/akwarium-367be-default-rtdb-29-export.json`
(295 928 B, 680 rekordów, węzeł „29" = cały dzień 29.10, 01:00:09 → 12:41:59).

### 14.1 Kompletność liczników seq w obrębie każdego rozruchu

| bootId | wersja | seq | próbek | **brakuje** | zakres czasu |
|---|---|---|---|---|---|
| `4A008BE708AE217C` | 3.56.5 | 6166–6734 | 559 | **10** (6543–6552) | 01:00:09 → 10:28:12 |
| `FC764B1F9A59366C` | 3.56.6 (boot=3) | 1–57 | 57 | **0** | 10:31:22 → 11:27:23 |
| `77A3CAC8F5BB8032` | 3.56.6 (boot=4) | 1–32 | 32 | **0** | 11:30:54 → 12:01:54 |
| `BCA88359DE792844` | 3.56.6 (boot=5) | 1–12 | 12 | **0** | 12:09:05 → 12:20:05 |
| `48B6D6073BC34CD4` | 3.56.7 (boot=6) | 1–20 | 20 | **0** | 12:22:58 → 12:41:59 |

Odstęp między próbkami wewnątrz rozruchu: **mediana 60,0 s, min 60 s, max 61 s.**
Czyli **ani jednej brakującej próbki** poza jedną luką w starym rozruchu.

### 14.2 Wszystkie przerwy czasowe > 90 s w dniu 29.10

| od | do | przerwa | przyczyna |
|---|---|---|---|
| 07:16:11 | 07:27:11 | **11 min 00 s** | **TEN SAM boot** `4A008BE7` — jedyna prawdziwa utrata danych |
| 10:28:12 | 10:31:22 | 3 min 10 s | przejście boot `4A008BE7` → `FC764B1F` (wgranie nr 1) |
| 11:27:23 | 11:30:54 | 3 min 31 s | przejście boot `FC764B1F` → `77A3CAC8` (wgranie nr 2) |
| 12:01:54 | 12:09:05 | 7 min 11 s | przejście boot `77A3CAC8` → `BCA88359` (**restart nadzoru**) |
| 12:20:05 | 12:22:58 | 2 min 53 s | przejście boot `BCA88359` → `48B6D607` (wgranie nr 3) |

**Razem 27 min 45 s przerw, z czego 16 min 45 s to wyłącznie restarty urządzenia**
(2–7 min bez zasilania aplikacji). Prawdziwa utrata danych: **11 minut, raz**.

### 14.3 Luka 07:16 — szczegół
seq 6542 = 07:16:11, seq 6553 = 07:27:11. Brakuje 6543–6552 = **10 próbek**.
To rozmiar jednej paczki (`TELEMETRY_BATCH_MAX_RECORDS = 10`). Rozruch się NIE zmienił
(ten sam bootId), więc nie był to restart. To jedyne zdarzenie tego typu w całym dniu.

### 14.4 Wnioski — trzeci i najważniejszy
1. **v3.56.6 nie zgubiła ani jednej próbki.** Trzy rozruchy, 101 próbek, kompletne seq.
2. **v3.56.7 też nie gubi** (20 próbek, komplet).
3. Luka, od której zaczęło się całe śledztwo (07:16), zdarzyła się na **v3.56.5**.
4. „Dziury" widoczne w aplikacji 29.10 to w 16/27 minuty **okresy restartów**, a nie
   gubienie próbek. Wykres nie ma jak pokazać danych z urządzenia, które nie działa.
5. Moje wcześniejsze pomiary pikselowe ze zrzutów („dziura 11:50–12:00", „12:30–12:41")
   były **błędne** — rozróżnienie kolorów żółty/pomarańcz na JPEG jest niewiarygodne.
   Dane z RTDB są źródłem prawdy i one mówią: ciągłość co 60 s.

### 14.5 Co pozostaje
Ryzyko „spóźnionego ACK" (§12.4, hipoteza 1) jest nadal nieobalone, ale jego częstość
to **1 zdarzenie na ~28 dni**. Nie uzasadnia kolejnej zmiany w firmware.
Priorytet: zero zmian, obserwacja.

---

## §15. PRAWDZIWA PRZYCZYNA „DZIUR" W WYKRESIE — ZNALEZIONA

### 15.1 Metoda
Rozbiór `akwarium-367be-default-rtdb-29-export.json` (680 rekordów, 5 rozruchów).
Każdy rekord ma tablice `a[11]` (wartości ×10) i `age[11]` (wiek odczytu w s).
`age == 65535` znaczy „ten kanał NIGDY nie miał poprawnego odczytu".

### 15.2 Mapowanie kanałów (z firmware, `telemetryJsonAppend`, l. 39754)
| idx | pole | opis |
|---|---|---|
| 0 | tZewnX10 | zewnątrz |
| 1 | tBojlerX10 | bojler CWU |
| 2 | tOgrzX10 | kocioł C.O. |
| **3** | **tOgrzSrX10** | **„Ogrz. śr." — średnia** |
| 4 | tPowrotX10 | powrót |
| 5 | tPanelX10 | panel solarny |
| 6 | tPokojX10 | pokój |
| 7 | tTrocinyX10 | trociny |
| 8 | rhX10 | wilgotność |
| 9 | pressureX10 | ciśnienie |
| 10 | dym | dym |

### 15.3 Co pokazały dane
`age[3] == 65535` występuje w **trzech** przebiegach i **każdy zaczyna się w chwili startu pieca**:

| boot | przebieg braku | długość |
|---|---|---|
| `FC764B1F` (boot=3) | 10:31:22 → 10:50:22 | 20 próbek |
| `77A3CAC8` (boot=4) | 11:30:54 → 11:49:54 | 20 próbek |
| `BCA88359` + `48B6D607` | 12:09:05 → 12:41:59 (koniec danych) | 32 próbki |

Pierwszy poprawny odczyt pojawia się **~20 min 50 s po starcie** (boot=3: start 10:30:33,
pierwszy dobry 10:51:22; boot=4: start 11:30:00, pierwszy dobry ~11:50:54).
Żaden inny kanał nie ma tego problemu (poza kanałem 10 = dym, który nie jest podłączony,
i pojedynczymi 8 próbkami na kanale 8).

### 15.4 Mechanizm w firmware
`temperaturaOgrzewaniaSrednia` (l. 21773) jest liczone w funkcji `obliczanie_srednia()`:
- tryb AUTO (Wybor != 1) → gałąź `else`: średnia krocząca z 4 próbek, ale
  `telemetryOgrzSrAccepted = true` **tylko gdy `pompaWlSrednia`** (pompa pracuje).
- **Pompa stoi** (STOP), więc ta ścieżka nigdy nie oznacza kanału jako dobrego.
- Ratunkowa gałąź `piecUznanyZaWygasly` ustawia `telemetryOgrzSrAccepted = true`,
  ale wymaga `millis() - ostatniRazPompaPracowalaMs >= Czas_OFF * 60000`.
- `ostatniRazPompaPracowalaMs` to `static` **zerowany przy każdym starcie**.

**Skutek:** po każdym restarcie pieca kanał „Ogrz. śr." jest pusty przez pełne
`Czas_OFF` minut — mimo że piec jest zimny od wielu godzin i pompa nie pracowała
dużo dłużej niż `Czas_OFF`. Dopiero po tym czasie nadzór „zauważa", że piec wygasł.
Zaobserwowane ~20,8 min ⇒ ustawione `Czas_OFF` = 20 min (domyślne w kodzie to 30).

### 15.5 Wniosek
**„Dziury" w wykresie to nie gubienie próbek.** To kanał pochodny, który po restarcie
nie ma czym się wypełnić. Aplikacja rysuje go poprawnie — po prostu nie ma wartości.
Dodatkowo: przy każdym restarcie użytkownik „zyskuje" kolejne 20 minut dziury.

### 15.6 Możliwe poprawki (do decyzji)
- **A) Firmware:** zapisywać `ostatniRazPompaPracowalaMs` (albo sam znacznik „piec wygasł")
  w NVS/RTC, żeby po restarcie od razu wiadomo było, że piec jest zimny.
  Poprawka mała, ale dotyka logiki sterowania — wymaga wydania 3.56.8.
- **B) Firmware, minimalna:** w `setup()` ustawić `ostatniRazPompaPracowalaMs = -Czas_OFF*60000`
  (czyli „dawno temu"), skoro po restarcie i tak nie wiemy, kiedy pompa ostatnio pracowała.
  Efekt: kanał dostępny od pierwszej próbki. Ryzyko: jeśli piec faktycznie pracował
  tuż przed restartem, średnia ruszy od surowego odczytu zamiast od wygładzonej wartości.
- **C) Aplikacja:** traktować `age == 65535` jako „brak danych" i nie rysować —
  już tak robi. Nic do zmiany.

**Rekomendacja:** B, jako osobne wydanie 3.56.8, po obserwacji obecnej 3.56.7.
