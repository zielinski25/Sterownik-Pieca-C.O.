# Temat 225 — dziury na wykresach aplikacji (v0.31.33 → poprawka v0.31.34)

Data: 2026-10-28, wieczór. Materiał: 7 zrzutów z telefonu (20:52–20:53),
**eksport bazy Firebase** (`akwarium-367be-default-rtdb-10-export.json`, 20 MB,
39 978 próbek, 01–28.10) oraz pliki `hist_long (7).csv` i `hist_short (7).csv` z pieca.

## 1. Co jest na zrzutach

- Aplikacja **v0.31.33**, wykres **„TEMPERATURY + POMPA C.O."**, zakres **7 DNI**,
  suwakiem przybliżone okno 13–18 godzin.
- Dwie różne rzeczy, które wyglądają podobnie:

### A) Dziury — brak linii w środku zakresu

Mierzone programowo (kolumny bez żadnego piksela serii w obszarze wykresu).
Czasów nie podaję co do minuty — etykiety osi są małe. **Dokładną godzinę odczytasz
sam**: przeciągnij palcem po wykresie na brzeg dziury, dymek pokaże godzinę.

| zrzut | okno | dziura |
|---|---|---|
| 20:53:25 | 24.10 03:56 → 17:07 | ok. 1 godzina w środku dnia |
| 20:53:33 | 24.10 02:51 → 14:24 | ta sama + krótsza |
| 20:53:03 | 24.10 16:21 → 25.10 08:27 | kilka godzin w nocy |
| 20:53:16 | 24.10 10:46 → 25.10 04:15 | ta sama nocna |

W dziurze znikają **wszystkie serie naraz** — to nie jest błąd rysowania jednej linii,
tylko brak wierszy danych.

### B) Cztery pionowe zielone „ściany" przy prawej krawędzi

To **nie** ciśnienie (tak przypuszczałem wcześniej — pomyłka). Kolor z piksela to
**`#4ADE80`**, czyli dokładnie kolor serii **„Pompa CO"**. Pompa rysowana jest
schodkami, a 0/100 rozciągane na **całą wysokość** wykresu. Przy zakresie 7 dni
minuta to ~1 piksel, więc seria szybkich przełączeń zlewa się w pełną zieloną ścianę.
Rysowanie poprawne, wygląd — nie.

## 2. Co pokazał eksport z chmury (rozstrzygnięcie)

Eksport obejmuje 28 dni października, ~1430 próbek na dobę (czyli ~1/min).

| dzień | próbek | przerwy > 6 min |
|---|---|---|
| 24.10 | 1352 | **10:03–10:14 (11 min)**, **12:26–13:43 (77 min)**, 13:47–13:55 (8 min) |
| 25.10 | 1432 | **brak** |
| 26–28.10 | 1204–1437 | brak |

**Wniosek 1:** dziura w środku dnia 24.10 jest **prawdziwa** — w chmurze naprawdę
nie ma 77 minut. Aplikacja pokazała prawdę.

**Wniosek 2 — ważniejszy:** w nocy z 24 na 25.10 chmura ma **każdą minutę**,
z poprawnymi wartościami (sprawdzone co 30 min od 19:00 do 01:30). A wykres pokazał
w tym miejscu kilkugodzinną pustkę. **Czyli tę część danych zgubiła aplikacja,
a nie piec i nie chmura.**

Sprawdziłem też, czy piec wtedy pracował: `hist_long (7).csv` ma wpisy przez całą
noc (19:49, 20:21, 21:02, 22:22, 23:07, 00:08, 01:33…), a `hist_short (7).csv`
ma 1515 wierszy na 25 godzin i **zero luk**. Piec zapisywał normalnie.

## 3. Dlaczego aplikacja mogła zgubić

Przeanalizowałem `RtdbClient.fetchTelemetry` → `fetchShard` i przesymulowałem ich
działanie na eksporcie: dla dni 23–28 zwracają **100 % rekordów**. Sama logika jest
poprawna — ale ma dwie dziury na błędy sieci:

1. **`try { fetchShard(dzień) } catch { /* pusty dzień */ }`** — jeden nieudany
   request kasuje **cały dzień**.
2. **`if (page.size < 300) break`** — krótsza odpowiedź serwera jest brana za
   „koniec dnia" i reszta dnia przepada bez śladu. To jedyne miejsce, które potrafi
   zostawić dziurę w środku dnia bez żadnego komunikatu.

Do tego dochodzi brak jakiejkolwiek kontroli: aplikacja nie sprawdza, czy pobrała
komplet, i nie mówi, ile próbek brakuje.

## 4. Poprawka: v0.31.34 „dziury i pompa"

Plik: `android-sync/v0.31.34-dziury-i-pompa/SterownikCO-Android-v0.31.34-DZIURY.zip`
(plus 5 plików `.kt`, `ZMIANY-0.31.33-do-0.31.34.diff`, `SHA256SUMS.txt`,
`README-WGRANIE.txt`). Zmienione są trzy pliki, 102 linie dodane, 7 usuniętych.

1. **`RtdbClient.kt` — `repairGaps()`**: po pobraniu całego zakresu aplikacja szuka
   przerw > 3 min i **dopytuje dokładnie ten przedział**. Prawdziwa przerwa zwróci
   0 wierszy — wtedy zostaje. Zgubiona strona — zostaje uzupełniona.
2. **`RtdbClient.kt` — błąd strony nie kasuje dnia**: zachowujemy to, co przyszło.
3. **`ChartEngine.kt` — pasek pompy**: w trybie mieszanym pompa zajmuje 10 dp przy
   dolnej krawędzi (jak kod paskowy), zamiast malować ściany na całą wysokość.
4. **`ChartEngine.kt` — znaczniki luk**: dwie przerywane pionowe kreski + napis
   `luka 1 h 12 min` nad przerwą.
5. **`AppModel.kt` — podpis pod wykresem**: `· luk: N (najdłuższa X h Y min)`
   oraz jawne „tylko chmura — archiwum centrali jest w domu" dla zakresów > 24 h.

Czego **nie** zmieniam: progu 6 minut (podniesienie go zasypałoby linią, której
nie ma w danych), archiwum ESP, plików `hist_*`, firmware'u.

## 5. Do sprawdzenia przy piecu (nie teraz, nie zdalnie)

Zrzut 20:53:37 „Bufor offline": przełącznik **Włączony**, ale liczniki
**0 / 0 / 0**. Albo od restartu nic nie buforował, albo aplikacja pokazuje zera,
bo czyta je z centrali po sieci domowej (a Ty jesteś 100 km dalej).
Rozstrzygnięcie: otworzyć ten sam ekran przy piecu albo przez VPN.

## 6. Otwarte

- Czy dziura nocna zniknie po v0.31.34 — to sprawdzi `repairGaps()`. Jeśli nie
  zniknie, znaczy, że problem jest głębiej (np. RTDB zwraca krótszą stronę niż
  deklaruje limit) i wtedy trzeba dorzucić logowanie liczby stron na dzień.
- Skąd 77 minut przerwy 24.10 12:26–13:43: to jedyna **prawdziwa** przerwa w całym
  miesiącu razem z 10:03–10:14 i 13:47–13:55. Wygląda na zanik internetu przy piecu.
