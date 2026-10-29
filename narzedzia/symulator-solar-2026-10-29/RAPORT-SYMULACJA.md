# Symulator logiki solarnej — 29.10.2026

Po co to jest: żeby przestać pisać „będzie dobrze". Ten symulator **kompiluje prawdziwy kod
z 0.31.49** (te same pliki, które idą na telefon) i uruchamia na nim tysiąc dni pogody.

## Czego NIE sprawdza

- Nie kompiluje całej aplikacji — brak Android SDK i bibliotek Compose/AndroidX.
- Nie sprawdza wyglądu, animacji, rysowania.
- Nie sprawdza `Weather.kt` w całości — tylko decyzję karty (`SolarDayCard`, wiersze 455–462),
  przepisaną do funkcji `kartaWartosc()`.

## Co sprawdza NAPRAWDĘ

`SolarAnalytics.kt` i `SolarThermalModel.kt` są wklejone do kompilacji **bez jednej zmiany**.
Zastąpione są tylko zależności zewnętrzne:

| podmieniony plik | po co |
|---|---|
| `stub/Compose.kt` | `mutableStateOf` / `mutableDoubleStateOf` — zachowują się jak w Compose |
| `stub/Json.kt` | `org.json` — używane tylko przez `save()`/`load()`, których test nie rusza |
| `stub/Core.kt` | `Prefs`, `Weather.startOfHour`, `WHour`, `TelemPoint` — skopiowane 1:1 |

`zbuduj-i-uruchom.sh` bierze pliki z `android/...` w chwili uruchomienia. Nie ma kopii
w repo, więc symulator nie może testować nieaktualnego kodu.

## Wynik

Pełne wyjście: `WYNIK.txt`.

| test | co robi | wynik |
|---|---|---|
| A | ekran z 29.10: zysk +1,5 i „do końca dnia" +2,1 | karta DZIŚ = **+3,6** ✅ |
| B | 1000 losowych dób, 25 niezmienników | **24 748 sprawdzeń, 0 błędów** |
| C | 17 przypadków brzegowych (NaN, puste, −40 °C, 1200 W/m²) | **130 sprawdzeń, 0 błędów**, żadnego NaN na ekranie |
| D | ten sam dzień policzony 100× | **identyczny co do 0,1 °C** |
| E | czy ekran z 29.10 jest fizycznie spójny | model daje +1,9 °C przy +2,1 z ekranu |
| F | czas wykonania | 0,40 ms na recompozycję |
| G | **poprawka 0.31.50** — zgodność kart | stara: 128/1000 dni źle, nowa: **0/1000** |

## Błąd, który sam popełniłem w teście

Pierwsze uruchomienie dało **55 błędów** typu „DZIŚ = −2,8 mimo zysku ≥ 0 i prognozy ≥ 0".
Po analizie: to był **zły niezmiennik**, nie błąd kodu. Gdy model nie znajdzie szczytu
(woda stygnie), karta „DO KOŃCA DNIA" pokazuje `+0,0 °C`, ale karta DZIŚ dodaje
`forecastEndDeltaC` — czyli realny spadek. Przy wodzie 46 °C i powietrzu −6 °C zbiornik
naprawdę stygnie o kilka stopni do północy. Karta DZIŚ = −2,8 jest wtedy **poprawna**.

Niezmiennik poprawiono na: „karta DZIŚ = ZYSK DZIŚ + to, co model naprawdę dodał".
Po poprawce: 0 błędów.

## Co symulator znalazł naprawdę

### 1. Karta „DO KOŃCA DNIA" pokazuje `+0,0 °C`, gdy model liczy wychładzanie

`SolarAnalytics.forecastGain = maxOf(0.0, res.deltaPeakC)` obcina wynik do zera.
`Weather.endOfDayGain()` drukuje `forecastGain`, więc przy chłodzeniu wychodzi **„+0,0 °C"**,
a podpis mówi „słońce nie przebije strat ciepła".

Problem: karta obok (DZIŚ) w tym samym momencie pokazuje np. **−2,8 °C**, bo dodaje
`forecastEndDeltaC`. Użytkownik widzi „+0,0" i „−2,8" i nie ma jak zgadnąć, skąd różnica.

Częstość w symulacji: **126 z 1000 dób = 12,6%**. W realu rzadsze (mój generator dopuszcza
wodę 46 °C przy powietrzu −6 °C), ale w lecie po zachodzie słońca to typowa sytuacja.
W nocy karta pokazuje „—", więc problem dotyczy tylko dnia.

**Poprawka wprowadzona w 0.31.50** (`Weather.kt`, `endOfDayGain`): gdy
`forecastPeakReal == false`, bierzemy `forecastEndDeltaC` zamiast obciętego zera.

Dowód poprawki — test G, 1000 dób:

| wersja | dni, gdzie „ZYSK DZIŚ" + „DO KOŃCA DNIA" ≠ karta DZIŚ |
|---|---|
| 0.31.49 (stara) | **128 / 1000 = 12,8%** |
| 0.31.50 (nowa) | **0 / 1000 = 0,0%** |

Poprawka zmienia liczbę na ekranie w 12,8% dób — dokładnie w tych, w których
stara wersja się rozjeżdżała. W pozostałych 87,2% nic się nie zmienia.

### 2. `forecastGain` nigdy nie jest ujemny, a `forecastEndDeltaC` może być bardzo ujemny

To nie błąd sam w sobie — `forecastGain` jest z definicji „o ile wzrośnie do szczytu".
Ale obie wartości idą na ekran bez rozróżnienia, co opisano w punkcie 1.

## Czego symulacja nie rozstrzyga

- **Czy projekt się skompiluje.** Nie mam Android SDK. Sprawdzone: `SolarAnalytics.kt`
  i `SolarThermalModel.kt` kompilują się bezbłędnie. `Weather.kt` i `ChartEngine.kt`
  są sprawdzone tylko przeglądem różnic (diff jest mały i typowo poprawny).
- **Czy model fizyczny zgadza się z rzeczywistością.** Symulacja sprawdza spójność
  wewnętrzną, nie zgodność z prawdziwym słońcem. To było robione wcześniej na 4 dniach
  pomiarów (błąd średni 1,3 °C).

## 0.31.50 — co zmieniono i czego to dotyczy

| plik | zmiana |
|---|---|
| `build.gradle.kts` | versionCode 31050, versionName 0.31.50 |
| `SolarAnalytics.kt` | bez zmian względem 0.31.49 (model fizyczny z 0.31.48) |
| `Weather.kt` | `endOfDayGain()` pokazuje realną zmianę, nie `+0,0` przy chłodzeniu |
| `TileSpecs.kt` | podpis kafelka pokoju z progami zamiast stałego „komfort cieplny" |
| `ChartEngine.kt` | bez zmian względem 0.31.49 (szybsze rysowanie z 0.31.49) |

Podpis kafelka pokoju:

| temperatura | podpis |
|---|---|
| < 16 °C | zimno |
| 16–18,9 °C | chłodno |
| 19–24 °C | komfort cieplny |
| > 24 °C | ciepło |

Przy 14,5 °C (stan z 29.10.2026) kafelek pokaże **„zimno"** zamiast „komfort cieplny".
