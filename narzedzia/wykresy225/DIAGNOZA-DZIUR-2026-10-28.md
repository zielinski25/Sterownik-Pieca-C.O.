# Temat 225 — dziury na wykresach aplikacji (v0.31.33)

Data: 2026-10-28, ok. 21:00. Materiał: 7 zrzutów z telefonu (28.10, 20:52–20:53).

## 1. Co jest na zrzutach

- Aplikacja **v0.31.33** (potwierdzone napisami „PUNKTY" i „odcinek symulowany").
- Wykres: **„TEMPERATURY + POMPA C.O."**, zakres **7 DNI**, suwakiem przybliżone okno
  od ~13 godzin (np. `24.10 03:56 → 24.10 17:07`) do ~18 godzin.
- Na wykresach widać **dwie różne rzeczy** — i to są dwa osobne problemy:

### A) Dziury (brak linii w środku zakresu)

Zmierzone programowo (szukanie kolumn bez żadnego koloru serii w obszarze wykresu):

| zrzut | okno | dziura |
|---|---|---|
| 20:53:25 | 24.10 03:56 → 17:07 | ok. **1 godzina** w środku dnia 24.10 |
| 20:53:33 | 24.10 02:51 → 14:24 | ta sama dziura, widać też drugą krótszą |
| 20:53:03 | 24.10 16:21 → 25.10 08:27 | **kilka godzin** w nocy z 24 na 25.10 |
| 20:53:16 | 24.10 10:46 → 25.10 04:15 | ta sama nocna dziura |

Czasów nie podaję co do minuty — etykiety osi są małe, a przybliżenie dałoby fałszywą
dokładność. **Dokładne godziny odczytasz sam**: przeciągnij palcem po wykresie na sam
brzeg dziury — dymek pokaże godzinę.

Ważne: **w dziurze znikają WSZYSTKIE serie naraz** (Ogrz., Bojler, Zewn., Pompa).
To nie jest błąd rysowania jednej linii — po prostu **nie ma żadnych próbek** na te minuty
w danych, które aplikacja dostała.

### B) Cztery pionowe zielone „ściany" przy prawej krawędzi

To **nie jest** linia ciśnienia (tak przypuszczałem wcześniej — pomyłka).
Kolor zmierzony z piksela: **`#4ADE80` = `74,222,128`**, a to jest dokładnie kolor serii
**„Pompa CO"** (`Telemetry.kt`, grupa `pump`).

Pompa rysowana jest schodkami, a wartość 0/100 rozciągana jest na **całą wysokość**
wykresu (`ChartEngine.kt`, `s.group == "pump" -> padTop + plotH * (1 - v/100)`).
Przy zakresie 7 dni jedna minuta to ~1 piksel, więc każde włączenie i wyłączenie pompy
to pionowa kreska na całą wysokość. Jeśli pompa przełącza się kilka razy w ciągu
kilkunastu minut — kreski zlewają się w pełną zieloną ścianę.

Czyli: **rysowanie jest poprawne, dane też, ale wygląda jak awaria.** Do tego pompa
w trybie mieszanym zasłania temperatury.

## 2. Skąd aplikacja bierze dane dla 7 dni — i dlaczego to ma znaczenie

W `AppModel.kt` (`loadRange`):

```kotlin
val useEsp = esp != null && sec <= 24 * 3600 && esp.isNotEmpty()
```

- Dla zakresu **6 h i 24 h** aplikacja łączy chmurę z archiwum centrali.
- Dla **7 dni i 30 dni** archiwum centrali jest **celowo pomijane** („dane z centrali i tak
  są podzbiorem chmury").
- Do tego z 100 km od pieca archiwum centrali jest **fizycznie nieosiągalne** — aplikacja
  czeka na nie 6 s i dostaje `null`.

**Wniosek: te wykresy to w 100 % dane z chmury Firebase.** Dziura na wykresie 7-dniowym
znaczy „tych minut nie ma w chmurze", a nie „aplikacja nie umiała narysować".

## 3. Czy piec wtedy pracował? TAK

W pliku `hist_long (7).csv` (pobrany z pieca 28.10 przez `/historia`) są wpisy przez całą
noc z 24 na 25.10:

```
19:49 19:59 20:21 20:55 21:02 21:10 21:17 21:36 21:44 21:55 22:02 22:22 22:30
22:39 22:48 22:57 23:07 23:14 23:23 23:32 23:42 00:08 00:20 00:23 00:30 ...
```

`hist_long` zapisuje próbkę **tylko przy zmianie** (temperatura ±1 °C, zmiana pompy) albo
na heartbeat **raz na 60 min**. Skoro w nocy są wpisy co 7–14 minut, to znaczy, że piec
**żył, miał dobry zegar i mierzył temperatury** przez całą tę noc.

Do tego `hist_short (7).csv` (27.10 19:00 → 28.10 20:14) ma **1515 wierszy, czyli
dokładnie 1 próbkę na minutę i ZERO luk**.

**Czyli: piec zapisywał. Do chmury te minuty nie dotarły.**

### Zastrzeżenie: czy samo pobieranie z chmury nie gubi danych?

Przejrzałem `RtdbClient.fetchTelemetry` → `fetchShard` (paginacja po 300 rekordów,
kursor = ostatni `ts` + 1, limit 100 stron na dobę, deduplikacja po `id`).

- Dziura **w środku dnia** nie ma się skąd wziąć: strona jest brana od `startAt=cursor`
  do `endAt=koniec_dnia`, więc brakujące minuty musiałyby fizycznie nie istnieć.
- Jest **jedno realne ryzyko**: `try { fetchShard(...) } catch { /* pusty dzień */ }` —
  jeśli choć jedna strona dnia rzuci wyjątek, **cały dzień** jest po cichu pomijany.
  To daje dziurę długości dnia, nie godziny, ale i tak warto to naprawić przy okazji
  (zbierać strony do skutku, a nie gubić cały dzień).

Podsumowanie: nie widzę w aplikacji mechanizmu, który zrobiłby dziurę tej długości.
Najbardziej prawdopodobne pozostaje, że tych minut nie ma w chmurze.

## 4. Gdzie więc giną dane

Kolejność po stronie firmware (v3.56.5, sprawdzone w źródle):

1. próbka co 60 s trafia do kolejki w RAM — `TelemetryRing`, pojemność **1440 próbek = 24 h**;
2. wysyłka paczkami po **10** próbek, max **3 próby**, potem paczka wraca do kolejki;
3. próbka **nie jest zdejmowana z kolejki przed potwierdzeniem** zapisu w Firebase
   (`telemetryFinalizeCompletedRequest` sprawdza jeszcze numer sekwencji);
4. bufor offline (zrzut kolejki na flash) — **włączony** (zrzut 20:53:37).

Z tego wychodzą trzy realne drogi do dziury:

- **restart centrali przy wyłączonym/nieudanym zrzucie na flash** → kolejka w RAM (24 h)
  przepada; wszystko, co nie zdążyło polecieć, ginie **na zawsze**;
- **dłuższa przerwa w internecie** → kolejka rośnie; jeśli przekroczy 1440 próbek,
  najstarsze wypadają;
- **wolne odsyłanie** po powrocie sieci (10 próbek na cykl) → dziura zasypuje się
  dopiero po godzinach, a przez ten czas aplikacja pokazuje pustkę.

### Liczniki „Bufor offline" pokazują 0 / 0 / 0

Na zrzucie 20:53:37: **Strony zapisane na flash — 0, Próbki zapisane — 0, Próbki odtworzone
w Firebase — 0**, przy przełączniku „Włączony".

To może znaczyć dwie rzeczy i trzeba je rozdzielić:
- (a) od restartu centrali naprawdę nic nie było buforowane — wtedy dziury pochodzą
  z okresów, gdy internetu nie było wcale, a nie z bufora;
- (b) aplikacja pokazuje 0, bo liczniki czyta z centrali po sieci lokalnej, a Ty jesteś
  100 km od pieca — czyli **licznik kłamie z powodu braku łączności**.

Rozstrzygnięcie jest tanie: przy piecu (albo przez VPN do domu) otworzyć ten sam ekran.
Jeśli nadal 0 — bufor nie zapisuje.

## 5. Co proponuję naprawić

### W aplikacji (mogę zrobić od razu, ~30 linii)

1. **Pompa nie na całą wysokość.** W trybie mieszanym rysować pompę jako pasek
   przy dolnej krawędzi (np. 12 dp wysokości) — włączenie = pasek wypełniony.
   Znika ściana, nie zasłania temperatur, a przybliżenie dalej pokazuje szczegóły.
   W trybie „tylko pompa" zostaje pełna wysokość.
2. **Dziura ma być opisana, nie niema.** Przy każdej przerwie > 6 min: cienka pionowa
   linia przerywana na obu końcach + etykieta `luka 1 h 12 min`, jeśli miejsce pozwala.
3. **Prawda w podpisie pod wykresem**: `luki: 2 · najdłuższa 5 h 12 min` — liczone
   z danych, więc zawsze zgodne z tym, co widać.

### W firmware (osobno, po wgraniu 3.56.5)

4. Do `/status` na Telegramie dodać jedną linię:
   `chmura: ostatnia próbka 3 min temu · w kolejce 12 · wysłane 8451 · porzucone 0`.
   Wtedy z 100 km od pieca w 5 sekund wiadomo, czy dziura właśnie się tworzy.
5. Sprawdzić, czy zrzut kolejki na flash na pewno działa (liczniki 0/0/0) — to on decyduje,
   czy restart centrali kasuje nieprzesłane próbki.

## 6. Czego NIE zmieniam

- **Progu 6 minut nie ruszam.** Podniesienie go (np. do 30 min) zasypałoby dziurę linią,
  której nie ma w danych — czyli dokładnie to, czego nie wolno robić.
- Arkuszy `hist_short` / `hist_long` i `/historia` nie ruszam — działają.
