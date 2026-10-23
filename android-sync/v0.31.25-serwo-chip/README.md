# Android v0.31.25 — tryb serwa jako pigułka obok tytułu

Nakładka na drzewo v0.31.24. **3 pliki do nadpisania.** `versionCode = 3031`,
`versionName = "v0.31.25"`.

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.25-CHIP.zip` |
| rozmiar | 15 504 B (4 wpisy: 3 pliki + README-WGRANIE.txt) |
| sha256 | `6687884b76e7fdd6f6e752369ee828b1a73a7cc6802909384048a207e8b06cec` |

## Wymaganie użytkownika

> „obok SERWO innym stylem pisać AUTO, a na dole syberek X%, klapa X%"

Czyli: tryb nie może być zwykłą linią tekstu pod wartością — ma być
**osobnym elementem przy tytule**, a pod spodem zostają tylko dwie pozycje.

## Realizacja

```
┌──────────────────────────┐
│ SERWO   AUTO          ◉  │   ← AUTO jako pigułka (Txt.badge, tło 14% alfa)
│ klapa 100%               │   ← wartość, 11 sp
│ syberek 100%             │   ← podpis, 7,5 sp
└──────────────────────────┘
```

- `QCard` dostaje dwa nowe, domyślnie puste parametry: `tag: String = ""`
  i `tagColor: Color = Pal.Cyan`. Pigułka renderuje się tylko wtedy, gdy
  `tag` nie jest pusty — POMPA i MIESZADŁO są nietknięte.
- Pigułka: `Txt.badge` (7,2 sp, ExtraBold), tło `tagColor` z alfą 0,14,
  `RoundedCornerShape(4.dp)`, padding 4×1 dp.
- `AUTO` → cyjan, `RĘCZNY` → żółty.
- **BEZPIECZNY** nie mieści się w pigułce (10 znaków ≈ 48 dp + tytuł + ikona
  = 86 dp przy ~87 dp), więc w tym trybie pigułka znika, a tryb przejmuje
  wartość: `BEZPIECZNY` / `tryb bezpieczeństwa`, ramka czerwona.
- Kafelek dalej 3 linie / 64 dp — rytm rzędu zachowany.

## Pozostałe stany

| stan | pigułka | wartość | podpis |
|---|---|---|---|
| AUTO | `AUTO` (cyjan) | `klapa X%` | `syberek Y%` |
| RĘCZNY | `RĘCZNY` (żółty) | `klapa X%` | `syberek Y%` |
| BEZPIECZNY | — | `BEZPIECZNY` | `tryb bezpieczeństwa` |
| czekam na ACK | — | `CEL X%` | `klapa 45° · czekam` |
| brak odczytu | — | `klapa —` | `syberek —` |

## Czego nie sprawdzono

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów 2 plików Kotlina (0), `diff -rq` = dokładnie 3 pliki, szerokość
wiersza tytułu ≈ 71 dp < 87 dp.

## Build potwierdzony (2026-10-22)

```
BUILD SUCCESSFUL in 10s
37 zadań (12 wykonanych, 25 z cache)
versionCode = 3031, versionName = "v0.31.25"
```

Zero ostrzeżeń `compileDebugKotlin` w tej wersji (w v0.31.24 były dwa
o deprecated `statusBarColor` / `navigationBarColor` — te same pliki, więc
Gradle po prostu pokazał je raz; nie dotyczą SERWO).

## Jak kopiować polecenia do PowerShella

`>>` na początku linii to **znak kontynuacji**, który PowerShell sam dopisuje,
gdy polecenie nie jest jeszcze zamknięte. To nie jest część polecenia.

Gdy skopiujesz cały ekran (razem z `>>`) i wkleisz go z powrotem, PowerShell
przeczyta `>>` jako operator przekierowania i wysypie się na:

```
Missing file specification after redirection operator.
The output stream for this command is already redirected.
Unexpected token 'in' in expression or statement.
```

Nic się wtedy nie psuje — żadne polecenie się nie wykonało.

**Zasady:**
1. Wklejaj tylko jedną linię albo blok bez znaków `>>`.
2. Kopiuj z czatu przyciskiem „kopiuj" — zwykłe zaznaczenie zamienia `>`
   na `&gt;` i to też wysypuje parser.
3. Po błędzie parsera nie trzeba nic naprawiać — wystarczy wkleić polecenie
   ponownie, czyste.
