# Android v0.31.23 — kafelek SERWO i natychmiastowy kwadracik

Nakładka na drzewo v0.31.22 (`C:\Users\Rafcio\Desktop\SterownikCO-0.31.22`).
**7 plików do nadpisania, żadnego nowego.** `versionCode = 3029`,
`versionName = "v0.31.23"`.

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.23-POPRAWKI.zip` |
| rozmiar | 31 298 B (8 wpisów: 7 plików + README-WGRANIE.txt) |
| sha256 | `27957b011774298431ab1cbad8335c06c7fbf65510f6086825e29053991deade` |

## Co naprawia

### 1. Kafelek SERWO — dwie pozycje zamiast jednej

Było: `AUTO (100%)` + podpis `klapa 100% · syberek 10…` (ucięty).
Jedna liczba przy trybie sugerowała, że serwo ma **jedną** pozycję — a to są
dwa niezależne serwa, a tryb to trzecia informacja.

Jest:

```
SERWO
klapa 100%      ← wartość (11 sp), pozycja klapy
syberek 10%     ← podpis (7,5 sp), pozycja syberka
tryb AUTO       ← druga linia podpisu, AUTO / RĘCZNY / BEZPIECZNY
```

Kąt z centrali (klapa 0–180°, syberek 0–90°) → procent. Tylko procent jest
porównywalny między dwoma serwami o różnym zakresie.

Szerokości: „klapa 100%" 10 znaków × 11 sp ≈ 61 dp; „syberek 10%" i
„tryb BEZPIECZNY" przy 7,5 sp ≈ 47 i 59 dp — wszystkie poniżej ~87 dp
dostępnych w kafelku, więc żadnego wielokropka.

Wymagało to **nowego parametru `sub2`** w `QCard` (domyślnie pusty — pozostałe
kafelki bez zmian) i podniesienia `Dimens.quickH` z 64 do 70 dp (czwarta
linia tekstu; przy 64 dp było 2 dp do przycięcia).

### 2. Kwadracik „Bufor offline" — bez 8-sekundowej zwłoki

Było: klik → toast „Potwierdzone przez sterownik (ACK)" → kwadracik **pusty**
przez kilka sekund, bo czytał wyłącznie status, a ten leci co 8 s. Zaznaczał
się dopiero po wyjściu z arkusza i powrocie.

Jest: `CheckRowEcho` w `Kit.kt` — po kliknięciu kwadracik pokazuje stan
docelowy od razu, podpis „Wysyłanie… czekam na potwierdzenie centrali".
Świeży status zgodny z echem zdejmuje echo; brak potwierdzenia w **12 s**
też je zdejmuje (UI wraca do prawdy z centrali). **Rytm statusu 8 s / 60 s
nietknięty** — decyzja „A" z 2026-10-22 nadal obowiązuje.

### 3. Ten sam błąd w dwóch innych miejscach

| miejsce | stan |
|---|---|
| Kontrola → „Włączony antystop" | `CheckRowEcho` |
| Mieszadło → „Automatyka włączona" | `CheckRowEcho` |

Nie dotyczyło i nie zmienia: wykresów, kafelków Dashboardu, Telegrama —
te zapisują się lokalnie i reagowały natychmiast.

## Czego nie sprawdzono

Brak JDK/Kotlinc/Gradle w środowisku autora paczki → **brak dowodu
„kompiluje się"**. Sprawdzone: bilans nawiasów 5 plików Kotlina (0 różnicy),
asercje kotwic (dokładnie 1 wystąpienie), `diff -rq` całego drzewa
(dokładnie 7 zmienionych plików), szerokości tekstów w dp.
