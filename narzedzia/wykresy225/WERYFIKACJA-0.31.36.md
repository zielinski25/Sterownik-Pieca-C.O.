# Weryfikacja 0.31.36/0.31.37 — sprostowanie

> **Ten plik został sprostowany 2026-10-28 po przeczytaniu `ARCHIWUM_SOLARNE_STAN.md`.**
> Pierwotna wersja twierdziła, że −17,5 °C w 0.31.36 „jest poprawne, bo to woda
> w zbiorniku”. To było błędne — patrz niżej.

## Co mówi dokumentacja instalacji

`ARCHIWUM_SOLARNE_STAN.md` §2:

| | Układ słoneczny | Bojler pieca C.O. |
|---|---|---|
| czujnik | `czujnik_temp_Panel_Plus` | `czujnik_temp_Bojlera` |
| magistrala | `BUS_3` | `BUS_2` |
| kanał telemetrii | `TELEMETRY_CH_PANEL` → **`t_panel` (a[5])** | `TELEMETRY_CH_BOJLER` → `t_bojler` (a[1]) |

§3: kolektor próżniowy rurowy **10 rur**, zbiornik **100 l** na dachu zintegrowany
z kolektorem, obieg **termosyfonowy**, czujnik **w wodzie, z boku, w połowie wysokości
zbiornika**. W kotłowni jest **drugi** bojler, grzany wężownicą z pieca.

**Wniosek:** `t_panel` to woda w zbiorniku słonecznym na dachu, a `t_bojler` to bojler
pieca w kotłowni. Wszystkie liczby solarne muszą brać `t_panel`.

## Błąd 0.31.36

W 0.31.36 przełączyłem „Zysk/Spadek dziś” i „Pobrania ciepłej wody” na `tBojler`.
Efekt: 28.10 aplikacja pokazała −17,5 °C, czyli spadek **bojlera pieca** w kotłowni.
Zbiornik słoneczny zmienił się tego dnia o **−0,2 °C** (tak podaje `/archiwum` centrali).

Liczba −0,3 °C, którą pokazywała 0.31.35, była **prawidłowa** dla solarów. Błędne
w 0.31.35 było tylko liczenie pobrań (szum czujnika, 24 „pobrania” dziennie).

## Co robi 0.31.37

- „Zysk/Spadek dziś” z `t_panel` (jak przed 0.31.36), metoda bez zmian:
  ostatnia próbka doby − pierwsza.
- „Pobrania ciepłej wody” z `t_panel` — spadek ≥ 0,5 °C od maksimum z 5 minut,
  utrzymany 5 minut, epizody < 20 min scalane, spadek > 12 °C odrzucany jako usterka sondy.

## Zgodność z `/archiwum` centrali

| Dzień | centrala dT | aplikacja 0.31.37 | pobrania |
|---|---|---|---|
| 23.10 | +6,2 | +6,3 | 0 |
| 24.10 | −17,2 | −17,3 | 1 · −1,0 |
| 25.10 | −1,2 | −1,2 | 0 |
| 26.10 | −3,8 | −3,7 | 0 |
| 27.10 | +4,2 | +4,2 | 2 · −2,0 |
| 28.10 | −0,2 | ~0,0 (eksport do 21:07) | 0 |

Różnice 0,1–0,2 °C wynikają z granicy doby: aplikacja liczy od lokalnej północy,
eksport z chmury kończył się 21:07.

Pobrania w całym miesiącu (eksport 28 dni): **26 w 17 dniach**, po 1–2 na dobę,
każde 0,5–5 °C, głównie po południu. Poprzednia reguła (>0,3 °C/min) dawała
**1042** „pobrania” z szumu o rozdzielczości 0,1 °C.

## Co pozostaje do sprawdzenia

1. **Temat 225 — wykresy.** Brak zrzutu wykresu. Do potwierdzenia: zakres 24.10
   12:00–14:00 → podpisana przerwa „luka 77 min” (12:26 → 13:43), poza nią linie ciągłe.
2. Jedno realne pobranie wody — czy karta pokaże „1 raz · −X °C”.
