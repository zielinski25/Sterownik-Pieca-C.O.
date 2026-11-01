# Pion w poziomie — dlaczego ChartScrollbar jest ucięty (stan na 0.31.76)

Data: 2026-11-01. Źródło liczb: zrzut użytkownika (`image-7.png`, panel 1:1
1280 × 576 px, gęstość 1,5607 px/dp) + stałe z `ui/theme/Theme.kt`.

## 1. Ile jest miejsca

| pas | px | dp |
|---|---|---|
| pasek statusu (system) | 56 | 36 |
| górny pasek aplikacji (`Dimens.topBarH`) | 97 | 62 |
| **treść (to, co widzi użytkownik)** | **419** | **271** |
| razem okno | 576 | 369 |

Szerokość okna: 1280 px = 820 dp.

## 2. Ile potrzebuje strona WYKRESY

Liczby z kodu: `Dimens.pagePad` 12, `Dimens.gap` 8, `Dimens.chartH` **220**,
`ChartScrollbar` = 30 dp toru + 3 dp odstępu + ~12 dp opisu.

| blok | dp |
|---|---|
| górny padding strony | 12 |
| nagłówek strony (tytuł + podtytuł) | ~50 |
| odstęp | 8 |
| nagłówek karty wykresu (tytuł, opis, plakietka, 2 ikony) | ~46 |
| odstęp | 8 |
| pasek serii (chipy, `horizontalScroll`) | ~26 |
| odstęp | 8 |
| **wykres (`Dimens.chartH`)** | **220** |
| odstęp | 8 |
| **`ChartScrollbar`** | **45** |
| legenda + KPI (jeszcze niżej) | 24 |
| **razem do końca scrollbara** | **~455** |

**Wniosek: 455 dp treści na 271 dp ekranu → scrollbar leży ~184 dp pod dolną
krawędzią.** To nie jest błąd rysowania ani ucięta karta: `verticalScroll`
działa, treść po prostu nie mieści się w poziomie. Ten sam objaw był
w 0.31.69 (okno 734 × 339 dp, treść 277 dp) — przyczyna identyczna.

Sam wykres to 220 dp z 455 dp, czyli **48 % całej potrzebnej wysokości**.
Dlatego „nie ruszać wykresów" i „scrollbar ma być widoczny" są w konflikcie,
którego nie da się rozwiązać samym paddingiem.

## 3. Warianty

| # | zmiana | zysk wysokości | koszt |
|---|---|---|---|
| A | górny pasek 62 → 46 dp **tylko w poziomie** | +16 dp | nagłówek ciaśniejszy (mniejszy tytuł); wykres bez zmian |
| B | `chartH` liczone z okna: `min(220, maxHeight - 190)` → ~150 dp | +70 dp | wykres niższy — **łamie „nie ruszać wykresów"** |
| C | `ChartScrollbar` przybity do dolnej krawędzi obszaru treści (rysowany jako nakładka, nie w przewijanym stosie) | 0 dp, ale **zawsze widoczny** | wykres i kolejność bez zmian; suwak „pływa" nad treścią |
| D | zostawić przewijanie | 0 dp | scrollbar nadal ~130–184 dp pod ekranem |

Rekomendacja: **A + C**. A zabiera 16 dp z paska, który w poziomie i tak jest
przewymiarowany (62 dp to wysokość z CSS panelu desktopowego), a C rozwiązuje
sedno — suwak zakresu jest sterowaniem, więc musi być widoczny bez przewijania.
B odkładam: dotyka geometrii wykresu, którego użytkownik prosił nie ruszać.

## 4. Co dokładnie zmieniłoby się w kodzie (do akceptacji)

A: `SterownikApp.kt` — przy `rail == true` wysokość `TopBar` z `Dimens.topBarH`
na `48.dp` (albo nowa stała `topBarHCompact`), bez zmian w środku paska.
C: `Charts.kt` — `ChartScrollbar(m)` wyniesiony z `Column(verticalScroll)`
do `Box`-a nad przewijaną treścią, wyrównany do dołu, z tłem i `padding`
równym `pagePadH`; `ChartsPage` dostaje dodatkowy `bottom` padding, żeby
ostatnie kafelki nie wchodziły pod suwak.

Zero zmian w: rysowaniu wykresu (`ChartCanvas`), danych, SERWO, historii,
kolorach, kolejności pozycji.

## 5. Jak to potwierdzić po wdrożeniu

Zrzut w poziomie, ten sam telefon: suwak widoczny bez przewijania, górny pasek
niższy o ~25 px, wykres nadal 220 dp (343 px).
