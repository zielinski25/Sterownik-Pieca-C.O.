# 0.31.53 — praca pompy na całą wysokość wykresu

Data 2026-10-29. Wersja: **0.31.52 → 0.31.53** (`versionCode` 31053).
Paczka: **3 pliki**, `android-sync/v0.31.53-pompa/`.

## Co było nie tak

Zgłoszenie użytkownika (zrzut z 29.10, 20:24, wykres 30 dni):

> *„zmieniłeś czas pracy pompy, tam wcześniej była do samej góry zaznaczona,
> a teraz nisko takie pierdki są"*

Miał rację. W 0.31.34 praca pompy w trybie mieszanym (temperatury + pompa)
została zredukowana do paska **10 dp** przy samej dolnej krawędzi:

```kotlin
// [v0.31.34 PUMP-BAND]
val pumpBase = padTop + plotH - 1.5.dp.toPx()
val pumpBandH = 10.dp.toPx()
fun pumpY(v: Double) =
    if (pumpOnly) padTop + plotH * (1 - v / 100.0)
    else pumpBase - (v / 100.0) * pumpBandH     // ← 10 dp, dolna krawędź
```

Powód był prawdziwy: przy szybkim przełączaniu 0/100 pompa rysowała pionowe
ściany na całą wysokość i **zasłaniała temperatury**. Ale rozwiązanie było
gorsze niż problem — przy zakresie 30 dni impulsy pracy mają 2–3 piksele
szerokości i po ściśnięciu do 10 dp przestają być czytelne.

## Co jest teraz

Praca pompy wraca na **całą wysokość wykresu**, ale jako **półprzezroczysta
kolumna**, nie gruba kreska:

```
krycie wypełnienia: 18%
krycie krawędzi:    55%
od:  górna krawędź pola wykresu
do:  dolna krawędź pola wykresu
kolejność rysowania: kolumny NAJPIERW, temperatury NA WIERZCHU
```

Dzięki temu problem, który 0.31.34 rozwiązywał, nie wraca — temperatury są
czytelne, bo kolumna jest przezroczysta i leży pod nimi.

W trybie **tylko POMPA** (bez temperatur) nic się nie zmienia: zostaje zwykła
linia schodkowa na pełnej wysokości, z osią PRACA / STOP.

## Dowód na prawdziwych danych

`narzedzia/pompa-wykres-2026-10-29/pompa-przed-po.png` — ten sam fragment
historii (`hist_short (5).csv`, 9–11.10, 3905 próbek, 393 próbek z pompą ON),
narysowany dwa razy: po lewej zachowanie 0.31.52, po prawej po zmianie.

Różnica jest taka, jak zgłosił użytkownik: zamiast kresek przy dolnej osi jest
pełna kolumna, a linia temperatury (Pokój) biegnie nad nią nieprzerwana —
nic nie jest zasłonięte.

## Szczegóły implementacji

- `pumpY()` obsługuje już tylko jedną geometrię (pełna wysokość) — zniknęła
  gałąź `pumpBase/pumpBandH`.
- Nowy blok „KOLUMNY PRACY POMPY" przed pętlą serii: skanuje próbki, skleja
  kolejne `ON` w jeden przedział `[i..j]`, rysuje prostokąt jako `drawLine`
  o grubości `xb - xa` plus dwie krawędzie. Minimalna szerokość **2 dp**, żeby
  pojedyncza próbka też była widoczna.
- W pętli serii dodany `if (!pumpOnly && s.group == "pump") return@forEach` —
  linia pompy nie jest już rysowana w trybie mieszanym, bo robią to kolumny.
  Bez tego mielibyśmy podwójne rysowanie.
- Kursor (crosshair) dalej liczy `pumpY()` — teraz spójnie z kolumnami,
  na pełnej wysokości.

## Koszt wydajności

Kolumny rysowane są **raz na klatkę**, nie per odcinek: jedna pętla po próbkach
plus 3 `drawLine` na każdy przedział pracy. Przy 30 dniach i 393 próbkach `ON`
to kilka tysięcy wywołań — mniej niż sama linia temperatury z wygładzaniem
Catmull–Rom, którą 0.31.49 musiała optymalizować.

## Czego ta zmiana NIE rusza

- **Nazwy czujników** — zostają „Panel słoneczny" / „Kolektor", zgodnie
  z decyzją z 29.10 o wycofaniu 0.31.53 z nazwami „zbiornik".
- **Model z niebem** z 0.31.52 — jest w tej paczce bez zmian
  (`skyRadWk = 8,20`, `reverseFlowKgS = 0`, `tankUa = 1,4`).
- **Firmware** — bez zmian, 3.56.7 zostaje, 3.56.8 czeka.

## Pakowanie

- ZIP: `SterownikCO-Android-v0.31.53-POMPA-3PLIKI.zip`, **21 965 B**
- SHA-256: `5028f279791f953bad760f07175ab7cc6d2e73c2218eddd03694b6789b76b07d`
- 3 pliki, `unzip -t` bez błędów, 3/3 plików ma **identyczny** SHA-256
  jak w repozytorium
- `ChartEngine.kt` sprawdzony kompilatorem Kotlina: **zero błędów składni**
  (brakujące odwołania `androidx.*` to brak bibliotek Androida w sandboxie,
  nie błąd kodu)
