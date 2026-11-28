# Firmware centrali 3.58.21 — Zew 2: odniesienie do DS18B20 dla przeliczonych ramek klona

Data: 2026-11-27 · następna po **3.58.20** (`firmware-patch/v3.58.20-dht22-ujemne/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 542 linie, SHA-256 w `SHA256SUMS.txt`).
Patch względem 3.58.20: **10 hunków, ok. 45 linii kodu** (reszta to komentarze i wpis w historii), wyłącznie C++
w bloku `ZEW2-FILTR`, nowej `zew2OdczytDs()` i w `wilgotnosc()`. HTML/JS strony WWW, `/api/status`, Firebase,
telemetria minutowa, `zewnZrodlo` — bajt w bajt / pole w pole jak w 3.58.20 (sprawdzane testem).
**Panel LCD bez zmian** (v1.5.18).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Pomysł (27.11)
„Zew 1 (DS18B20) i Zew 2 (DHT22) są podobne, z przesunięciem — niech DS będzie odniesieniem dla przeliczanych
wartości z klona.”

## Co mówią dane (eksport Firebase 26.11, 778 par Zew 1 / Zew 2)

| | Zew 1 − Zew 2 |
|---|---|
| średnia / mediana | **3,16 / 3,1 °C** |
| min / max | **2,2 / 4,5 °C** |
| godzinami (UTC) | 2,8–3,9; najwięcej 05–07 h |

Wnioski:
1. Przesunięcie jest stabilne, ale **pływa o ±1,2 °C** w ciągu doby (miejsce montażu, słońce, wiatr). Próg rzędu 2–3 °C
   odrzucałby prawdziwe odczyty rano — DS **nie może być sędzią**, tylko **siatką bezpieczeństwa** z szerokim progiem.
2. Gdyby DS18B20 padł, DHT22 ma dalej być samodzielnym źródłem Zew (`zewnZrodlo`) — więc zwykłe ramki nie mogą
   zależeć od DS. Porównanie dotyczy **wyłącznie ramek przeliczonych** z kodowania klona (3.58.20), bo tylko tam jest
   nowy, „obliczony” element, który warto dodatkowo pilnować.
3. Próg **8 °C**: normalna różnica 2–4,5 °C mieści się z dużym zapasem; łapie tylko bzdury (np. zakłócenie z poprawną
   sumą kontrolną, które trafiło w przedział klona, gdy na dworze jest +10 °C).

## Zmiana
- Blok `ZEW2-FILTR` (testowalny na PC): `ZEW2_DS_PROG_C = 8.0f` i `zew2DsOdbiega(tDht, tDs, dsOk)` — `false` gdy brak DS
  albo NaN, inaczej `|tDht − tDs| > 8`. `zew2Filtruj()` dostaje parametr `wymusPodejrzany` (domyślnie `false` =
  zachowanie 3.58.20); jedyna zmiana w filtrze to `podejrzany = wymusPodejrzany || zew2Podejrzany(...)`.
- Nowa `zew2OdczytDs(float*)` tuż przed `wilgotnosc()`: zwraca `temperaturaNaZewnatrz` (DS18B20) tylko gdy
  `telemetryQuality(TELEMETRY_CH_ZEWN) == 0` (świeży, ≤ 135 s) i **nie symulowany** (`lastGoodSimulated`,
  `trybSymulacji`).
- `wilgotnosc()`: po dekoderze klona → `dsOdbiega = dhtPrzeliczona && zew2DsOdbiega(t, dsTemp, dsOk)` → WARN
  `DHT22 Zew2 zdarzenie=ds_odniesienie t=-0.3 ds=10.0 prog=8 msg="przeliczona ramka daleko od DS18B20, powtorka za 3 s"`
  i `zew2Filtruj(..., dsOdbiega)`. Ramka idzie **tą samą ścieżką, co skok > 3 °C**: nie trafia na wykres, filtr czeka 3 s
  na powtórkę; dwa zgodne odczyty (≤ 0,5 °C) → potwierdzone i przyjęte **mimo DS** (siatka, nie sędzia); powtórka pasująca
  do historii → przyjęta, kandydat odrzucony; nic do niczego nie pasuje → odrzucona.
- Linia INFO `DHT22 diagnostyka`: nowe pola `ds_roznica=<DS − DHT>` (`nan` bez DS) i `ds_odbiega=<licznik>`.

Bez DS (brak czujnika, nieświeży, symulacja ZEWN, tryb symulacji) — zachowanie identyczne z 3.58.20. Wilgotność,
zwykłe ramki, `/api/status`, Firebase, LCD, WWW — bez zmian. Koszt fałszywego alarmu: 3 s opóźnienia jednej próbki.

## Testy na PC (`tests/uruchom_testy.sh`, wynik w `WYNIKI/testy-pc.txt`)
- **A** blok `ZEW2-FILTR` wycięty z pliku: `[1]`–`[16]` z 3.58.10/3.58.20 (regresja) + `[17]` `zew2DsOdbiega()` na progu
  (3,1 / 4,5 / 8,0 nie; 8,1 / 10,3 / −9 tak; bez DS i NaN nigdy), `[18]` `zew2Filtruj()` z wymuszeniem (powtórka,
  dwa zgodne odczyty wygrywają mimo DS, domyślny parametr = 3.58.20) — **125 sprawdzeń, 0 błędów** (także ASan+UBSan).
- **B** prawdziwa `wilgotnosc()` z atrapami (DS = atrapa `zew2OdczytDs()`): `[1]`–`[12]` z 3.58.10/3.58.20 (regresja, bez
  DS) + `[13]` DS 2,6 + ramka klona −0,5 → przyjęta od razu, `ds_roznica=3.1`; zwykła ramka 12,0 przy DS 2,6 **nie**
  jest porównywana z DS (tylko filtr skoków); `[14]` DS +10 + ramka klona −0,3 → powtórka, WARN `ds_odniesienie`,
  `ds_odbiega=1`, RH z ramki przyjęta; dwa zgodne −0,3/−0,4 → potwierdzone mimo DS; bez DS → jak 3.58.20 —
  **78 sprawdzeń, 0 błędów**.
- **C** znaczniki 3.58.21, porównanie tylko dla `dhtPrzeliczona`, kolejność `dekoder → DS → zew2Filtruj(..., dsOdbiega)`,
  `wymusPodejrzany` użyty tylko do flagi, plik = 3.58.20 + patch (SHA), HTML i kod poza blokiem/`zew2OdczytDs()`/
  `wilgotnosc()` identyczny z 3.58.20.
- **D** pełne testy paczki 3.58.20 (i przez nią 3.58.19…3.58.14) na tym pliku (do harnessu 3.58.20 dopisana atrapa
  `zew2OdczytDs()`, bo 3.58.21 wymaga jej od testu; logika testów bez zmian).

## Pliki
`src/main_centrala.cpp` · `main_centrala-v3.58.20-do-v3.58.21.patch` · `tests/` (`uruchom_testy.sh`,
`wytnij_zew2_filtr.py`, `wytnij_wilgotnosc.py`, `test_zew2_filtr.cpp`, `test_wilgotnosc.cpp`) ·
`WYNIKI/zakres-zmian.txt` (numery linii) · `WYNIKI/testy-pc.txt` · `SHA256SUMS.txt` · `CZYTAJ-NAJPIERW.txt`.
