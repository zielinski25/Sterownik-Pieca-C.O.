# 0.31.53 — „zbiornik", nie „panel"

Data 2026-10-29. Wersja podbita: **0.31.52 → 0.31.53** (`versionCode` 31053).
Paczka **zastępuje** 0.31.52 — zawiera ją w całości.

## Skąd ten temat

Na zrzutach z 29.10 godz. 19:12 aplikacja pokazuje:

```
Panel teraz / powietrze       17,0 °C / 11,5 °C
Różnica panel − powietrze     +5,5 °C
```

Czytelnik rozumie to tak: „kolektor jest o 5,5 K cieplejszy od powietrza".
Tymczasem czujnik `t_panel` (BUS_3, kanał `TELEMETRY_CH_PANEL`) leży **w wodzie
zbiornika 100 l na dachu**. Potwierdza to:

- `core/SolarAnalytics.kt` — komentarz z 0.31.37 wprost tak mówi,
- `MODEL_SOLARNY_TRZEMCHA_2026-10-23.md` §19.6,
- zachowanie liczb (patrz niżej).

## Dowód z logu (nie z dokumentów)

10 dni z `piec_log (30).txt`, tylko pola `panel=` i `zewn=`:

| dzień | zbiornik min/max | powietrze min/max |
|---|---|---|
| 21.10 | 19,8 / **39,2** | 10,5 / 20,5 |
| 22.10 | 29,2 / 35,2 | 12,0 / 19,0 |
| 23.10 | 25,8 / **47,2** | 8,5 / 18,0 |
| 24.10 | −1,0 / 35,5 | −1,0 / 17,0 |
| 29.10 | 16,5 / 18,5 | 11,5 / 14,5 |

Trzy rzeczy, których kolektor próżniowy **nie zrobi**:

1. **Maksimum 47,2 °C w dziesięciu dniach.** Rura próżniowa bez odbioru ciepła
   ma 80–100 °C. Woda w 100 l owszem, 47 °C.
2. **Chłodzenie 11 godzin bez przerwy.** Noc 23/24.10: 42,5 → 27,8 °C od 19:30
   do 06:00, przy powietrzu 12 → 7,5 °C. Kolektor wyrównałby się z powietrzem
   w pół godziny. Woda stygnie godzinami.
3. **Różnica względem powietrza stała o północy** — kolejna oznaka dużej masy
   wodnej, nie cienkiej płytki.

## Co było nie tak w napisach

| plik | było | jest |
|---|---|---|
| `core/Sensors.kt` | „Panel słoneczny" / „**Kolektor glikolowy** · 1-Wire / PT1000" | „Zbiornik solarny (100 l na dachu)" / „Czujnik w wodzie zbiornika · 1-Wire / PT1000" |
| `core/TileSpecs.kt` | kafel „Panel słoneczny" + „Temperatura kolektora" | „Zbiornik solarny" + „Temperatura wody w zbiorniku 100 l" |
| `core/TileSpecs.kt` | kafel na pulpicie „Panel słon.", opis „nasłonecznienie ok" | „Zbiornik", opis „dzień" |
| `core/TileSpecs.kt` | „alarmy kotła i kolektora" | „…kotła i zbiornika solarnego" |
| `core/Telemetry.kt` | legenda wykresu „Panel" | „Zbiornik" |
| `core/AppModel.kt` | Telegram `*Panel:*`, „ALARM PANELU" | `*Zbiornik:*`, „ALARM ZBIORNIKA SOLARNEGO" |
| `ui/screens/Weather.kt` | „🌡️ PANEL − POWIETRZE" | „🌡️ ZBIORNIK − POWIETRZE" |
| `ui/screens/Overlays.kt` | „Kolektor słoneczny próżniowy" | „Zbiornik solarny 100 l (na dachu)" |
| `ui/screens/Lists.kt` | „Przegrzanie kotła & panelu", lista „…Piec, Panel, Powrót…" | „…& zbiornika", „…Piec, Zbiornik, Powrót…" |
| `ui/screens/Dashboard.kt` | „PRZEGRZANIE PANELU" | „PRZEGRZANIE ZBIORNIKA" |
| `ui/screens/sheets/Sensors.kt` | „Panel teraz", „Różnica panel − powietrze", „(czujnik kolektora…)" | „Zbiornik teraz", „Różnica zbiornik − powietrze", „(czujnik w wodzie zbiornika…)" |
| `widget/PiecWidget.kt` | „PANEL SŁONECZNY", skrót „PANEL", zakres 0–140 °C | „ZB. SOLARNY", skrót „SOLAR", zakres 0–100 °C |
| `res/values/strings.xml` | „bojler i panel słoneczny" | „bojler i zbiornik solarny" |

**Świadomie NIE zmienione:** klucze w bazie (`panel`, `t_panel`). Zmiana klucza
złamałaby historię i wykresy. Nazwa widoczna dla człowieka — tak, klucz — nie.

## Znaleziony przy okazji BŁĄD (poprawiony)

`ui/screens/Dashboard.kt`:

```kotlin
S.alarm_ogrzewanie -> "PRZEGRZANIE PIECA — " + S.fmt1(to) + " °C"
else -> "PRZEGRZANIE PANELU — " + S.fmt1(tb) + " °C"   // tb = BOJLER PIECA
```

`tb` to `t_bojler` — temperatura **bojlera pieca w kotłowni**. Przy alarmie
przegrzania kolektora aplikacja pokazywała temperaturę zupełnie innego
urządzenia. Teraz czyta `t_panel` (`tp`).

Skutek praktyczny: przy takim alarmie baner pokaże właściwą liczbę, a nie
np. 60 °C z bojlera pieca.

## Czego to NIE naprawia

Kafel nadal pokazuje **jedną** liczbę ze zbiornika. Nie ma czujnika na płytce
kolektora — więc nie da się pokazać „kolektor 78 °C, zbiornik 42 °C". To
ograniczenie sprzętu, nie napisów.

## Pakowanie

- ZIP: `SterownikCO-Android-v0.31.53-ZBIORNIK-13PLIKOW.zip`, **96 295 B**
- SHA-256: `d06b163449d994ab1c59bf4e5e8ba77a1a1fc00838c0d8afa002a30fe24d81cd`
- 13 plików, `unzip -t` bez błędów, 13/13 plików w ZIP ma **identyczny**
  SHA-256 jak w repozytorium
- Bilans nawiasów w każdym zmienionym pliku identyczny jak przed zmianą
  (zmieniano wyłącznie literały tekstowe i jedną linię `val`)
