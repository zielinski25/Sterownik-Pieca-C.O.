# 231 — dwie różne paczki o numerze 0.31.53 (i jak to naprawione)

Data 2026-10-29 wieczór. Wersja: **0.31.53 → 0.31.54**.

## Co się stało

Dziś powstały **dwie różne paczki z tym samym numerem 0.31.53**:

| kolejność | zawartość | nazwy czujnika | status |
|---|---|---|---|
| 1. | `v0.31.53-zbiornik` (12 plików) | „Zbiornik solarny" | **wycofana** — użytkownik jej nie chciał |
| 2. | `v0.31.53-pompa` (3 pliki) | „Panel słoneczny" (stare) | poprawna, ale numer ten sam |

Skutek: w Ustawieniach obie pokazują `v0.31.53`. Użytkownik zbudował pierwszą
i nie miał jak odróżnić, że ma złą wersję — na kafelku widział „Zbiornik".
**To błąd w numerowaniu, nie w kodzie.** Ten sam numer nigdy nie może
oznaczać dwóch różnych rzeczy.

## Naprawa

Numer podbity na **0.31.54**. Paczka zawiera **14 plików**: 12 plików, które
ruszyła zła paczka (żeby je nadpisać), plus `ChartEngine.kt` (wykres pompy)
i `SolarThermalModel.kt` (model z niebem).

Gdyby paczka miała tylko 3 pliki „pompy", złe nazwy **zostałyby** w projekcie —
i o to właśnie chodziło w tym błędzie.

Sprawdzone programowo: w 11 plikach z tekstami UI w ZIP-ie **zero wystąpień**
„Zbiornik". Słowo to zostaje wyłącznie w komentarzu fizyki
(`SolarThermalModel.kt`, „zbiornik stoi na dachu") — to nie jest tekst na ekranie.

### Przywrócone napisy (dokładnie)

| miejsce | tekst |
|---|---|
| kafelek pulpitu | `Panel słon.` |
| lista Ustawienia | `Panel słoneczny` + opis `Temperatura kolektora` |
| czujnik | `Panel słoneczny (solary)` / `Kolektor glikolowy · 1-Wire / PT1000` |
| arkusz po kliknięciu | `Kolektor słoneczny próżniowy` |
| wiersze w arkuszu | `Panel teraz / powietrze`, `Różnica panel − powietrze` |
| legenda wykresu | `Panel` |
| ekran Pogoda | `PANEL − POWIETRZE` |
| widget | `PANEL SŁONECZNY`, skrót `PANEL` |
| Telegram | `*Panel:*`, `ALARM PANELU` |
| Ustawienia → alarmy | `Przegrzanie kotła & panelu` |
| baner alarmu | `PRZEGRZANIE PANELU` |

## Dowód, że to naprawdę „stare" nazwy

Sprawdzone w historii gita — `title = "Panel słon."` w **każdym** commicie
dotykającym `TileSpecs.kt`, od pierwszego (`530fc8b`) po dziś. Nigdy nie było
innej nazwy kafelka. Dlatego „dawniej" = dokładnie to, co jest w tej paczce.

## Znaleziony przy okazji błąd w repozytorium

Polecenie `git add -A` przy cofaniu 0.31.53 **usunęło z repo 25 plików**, które
istniały tylko w drzewie roboczym (m.in. `AlarmSoundSync.kt`, `ChartArchive.kt`,
`TelemetryCache.kt`, `Offline.kt`, 8 testów). Drzewo robocze było kompletne,
HEAD — nie. Dodatkowo sandbox cofnął się do commita `dc2a49d`, czyli stanu
sprzed 0.31.33.

Naprawione: `git reset --hard origin/arena/8d4e1b71-sterownik-pieca-c-o`
przywrócił `7b388b2` z kompletnym drzewem; sprawdzenie
`git ls-files -- android/` oraz obecność `AlarmSoundSync.kt` i `Offline.kt`
potwierdzone.

## Pakowanie

- ZIP: `SterownikCO-Android-v0.31.54-STARE-NAZWY-14PLIKOW.zip`, **107 875 B**
- SHA-256: `21aabeca14b7a00ff8573f4e580bcd74a4dfb75021aa35f4bd57377de5c26de5`
- 14 plików, `unzip -t` bez błędów, **14/14** ma identyczny SHA-256 jak w repo
- `versionCode 31054`, `versionName "0.31.54"` — sprawdzone w środku ZIP-a

## Czego NIE zmieniono

- Model z niebem (0.31.52) — bez zmian: `skyRadWk 8,20`, `tankUa 1,4`.
- Wykres pompy (0.31.53-pompa) — bez zmian, potwierdzone `pumpY` w `ChartEngine.kt`.
- Firmware — bez zmian. 3.56.7 zostaje, 3.56.8 czeka, nie wgrane.

## Znany, nienaprawiony defekt w tej samej okolicy

`ui/screens/Dashboard.kt` przy alarmie przegrzania panelu czyta `t_bojler`
(bojler pieca) zamiast `t_panel`. Naprawa to jedna linia, ale **nie jest
zrobiona** — użytkownik prosił o powrót do stanu poprzedniego, a to zmiana
zachowania, nie nazw. Do decyzji.
