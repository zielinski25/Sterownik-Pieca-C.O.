# Android v0.31.24 — kafelek SERWO w trzech liniach

Nakładka na drzewo v0.31.23. **4 pliki do nadpisania.** `versionCode = 3030`,
`versionName = "v0.31.24"`.

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.24-KAFELEK.zip` |
| rozmiar | 17 548 B (5 wpisów: 4 pliki + README-WGRANIE.txt) |
| sha256 | `5027211a36b00b89a6f7bf818d5644a4163799335a4fbdcbf5edaf40295f9d93` |

## Problem

v0.31.23 dało kafelkowi SERWO **cztery** linie, a POMPA i MIESZADŁO mają
**trzy**. Na zrzucie z telefonu SERWO było gęstsze, tekst sięgał góry kafelka
i nie miał oddechu — wyglądało to jak inny komponent, nie jak sąsiad z rzędu.

## Rozwiązanie

```
SERWO
klapa 100%           ← wartość (11 sp), pozycja KLAPY
syberek 100% · AUTO  ← podpis (7,5 sp), pozycja SYBERKA + tryb
```

Tryb przeniesiony z czwartej linii na koniec podpisu. Kafelek wraca do
`Dimens.quickH = 64.dp` — podniesienie do 70 dp było potrzebne tylko dla
czwartej linii.

| stan | wartość | podpis |
|---|---|---|
| AUTO | `klapa X%` | `syberek Y% · AUTO` |
| RĘCZNY | `klapa X%` | `syberek Y% · RĘCZNY` |
| BEZPIECZNY | `BEZPIECZNY` | `tryb bezpieczeństwa` |
| czekam na ACK | `CEL X%` | `klapa 45° · czekam` |
| brak odczytu | `klapa —` | `syberek —` |

### Kolor ramki

W v0.31.23 tryb AUTO dawał **zieloną** ramkę i zieloną wartość, więc SERWO
odcinało się od neutralnych POMPY i MIESZADŁA. Teraz:

| stan | ramka |
|---|---|
| AUTO (normalna praca) | neutralna — jak u sąsiadów |
| RĘCZNY | żółta |
| BEZPIECZNY | czerwona |
| brak danych / czekam | żółta |

Kolor niesie „coś odbiega", a nie „wszystko gra" — spójnie z resztą pulpitu.

## Szerokości (dlaczego się mieści)

Dostępne ~87 dp. `klapa 100%` 10 zn. × 11 sp ≈ 61 dp; najdłuższy podpis
`syberek 100% · RĘCZNY` 20 zn. × 7,5 sp ≈ 78 dp; `tryb bezpieczeństwa`
19 zn. ≈ 74 dp.

## Czego nie sprawdzono

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów 3 plików Kotlina (0), `diff -rq` = dokładnie 4 pliki, szerokości
tekstów w dp.
