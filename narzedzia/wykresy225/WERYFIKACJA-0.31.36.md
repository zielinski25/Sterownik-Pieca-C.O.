# Weryfikacja 0.31.36 na telefonie — 2026-10-28, 23:47

Wersja **0.31.36** zainstalowana (wersja sprawdzona po zrzutach: nowy podpis pod
tabelą bilansu, nowe liczby). Temat **226 (panel słoneczny) uznaję za domknięty w kodzie
i potwierdzony na żywo**.

## Co pokazują zrzuty z 23:47

| Ekran | Przed (0.31.35, 23:09) | Po (0.31.36, 23:47) |
|---|---|---|
| Karta „SPADEK DZIŚ” | −0,3 °C (kolektor 17,5 → 17,2) | **−17,5 °C** (zbiornik) |
| Karta „POBRANIA DZIŚ” | „25 razy · −7,5 °C” | **—** (brak) |
| Arkusz „Spadek dziś · zmierzone” | −0,3 °C | **−17,5 °C** |
| Arkusz „Pobrania ciepłej wody” | „24 razy · −7,2 °C” | **brak** |
| Podpis pod tabelą | kończył się na „…nie pomiar.” | + zdanie o progu 0,5 °C / 5 min |

Pulpit i arkusz pokazują **tę samą liczbę** — to był jeden z objawów błędu w 0.31.35
(arkusz „brak”, pulpit „25 razy”).

## Dlaczego −17,5, a nie −16,5

Eksport z chmury, na którym liczyłem, kończy się **28.10 o 20:07 UTC (21:07 czasu
polskiego)**. Do tej chwili zbiornik zszedł z 37,5 do 21,0 °C, czyli −16,5.
Do 23:47 zbiornik stygł dalej (przez pętlę termosyfonu ok. 0,5 °C/h) i zszedł do
około 20,0 °C — stąd **−17,5**. To są dane na żywo, nie rozjazd.

Liczba rośnie więc dalej przez noc i **o 00:00 (lokalna północ) licznik startuje od
zera** — 29.10 pokaże własną dobę.

## Co jeszcze nie jest sprawdzone

1. **Temat 225 — wykresy.** Wciąż brak zrzutu wykresu. Do potwierdzenia:
   - zakres **24.10, 12:00–14:00** → podpisana przerwa **„luka 77 min”** (12:26 → 13:43),
   - poza luką linie **ciągłe** (brak cichych przerw),
   - pompa nie zasłania linii temperatur.
2. **Pierwsze prawdziwe pobranie wody.** Dziś wyszło „brak”, bo zbiornik tylko stygł —
   to jest poprawne, ale nie sprawdza dodatniej strony reguły. Przy następnym poborze
   karta powinna pokazać „1 raz · −X °C” (X zależnie od ilości wody).

## Jak to policzono (skrót)

- „Zysk / Spadek dziś” = ostatnia minus pierwsza próbka **zbiornika** od lokalnej północy.
- Pobranie = spadek ≥ 0,5 °C od maksimum z 5 minut, utrzymany przez następne 5 minut;
  epizody bliższe niż 20 min scalane; spadek > 15 °C obcinany (odczepiona sonda).
- Sprawdzone offline na eksporcie 28 dni (39 602 próbki): 32 pobrania w 16 dniach,
  28.10 — zero. Poprzednia reguła dawała 1042 „pobrania” z szumu czujnika.
