# Raport: aplikacja nie rozciąga się po obrocie — research, decyzje, poprawka

Data: 2026-11-01 · wersja: **0.31.69** (baza 0.31.68) · zakres: **krok 1 + krok 2**

---

## 1. Objaw

Po obrocie telefonu na poziomo aplikacja nie zajmuje całego ekranu: siedzi
w wąskiej kolumnie przyklejonej do **lewego górnego rogu**, resztę wypełnia
czarne tło, a zakładki dolnej nawigacji są ucięte („Pane…”).

Pomiar zrzutu z 1.11 (skrypt, nie „na oko”):

| co | piksele | dp (przy 2,94 px/dp) |
|---|---|---|
| szerokość aplikacji | 1210 px | **412 dp** ← dokładnie `frameWidth` |
| czarne pole po prawej | 938 px | 319 dp |
| szerokość okna | 2158 px | **735 dp** |
| martwa powierzchnia okna | — | **≈ 45 %** |

## 2. Przyczyna (jedna linia)

`app/src/main/java/com/sterownikco/pro/ui/SterownikApp.kt:63-66` (0.31.68):

```kotlin
val wide = maxWidth > 560.dp
Column(
    Modifier.then(
        if (wide) Modifier.width(m.frameWidth.dp).fillMaxHeight(0.98f)   // 412 dp
        else Modifier.fillMaxSize()
    )
)
```

Decyzję podejmowała **sama szerokość**. Telefon ma w pionie ~331–412 dp (ramka
się nie włącza), ale po obrocie ta sama wartość staje się 735–915 dp i próg
`> 560 dp` puszcza. Ramka jest przyklejona do `TopStart`, bo `Box` domyślnie
tak wyrównuje, a `fillMaxHeight(0.98f)` zostawia 2 % pionu.

Skąd ta reguła: z `Piec.html`, gdzie ramka to **ręczny tryb symulatora na PC**
(`body.mode-phone-frame`, komentarz w pliku: „wymuszony przyciskiem na PC”).
W HTML `#phone` ma domyślnie `width: 100%`, a `@media (max-width: 768px)`
wymusza pełny ekran — port do Androida zgubił ten warunek.

## 3. Research — co mówią źródła (7 języków)

**Oficjalne (EN):**
- Wysokość jest klasyfikowana **osobno** od szerokości. Telefon w poziomie to
  *Medium/Expanded szerokość × **Compact wysokość** (< 480 dp)* — „99,78 %
  telefonów w poziomie”. Google ostrzega: *„consider the height window size class
  for scenarios such as phones in landscape… **two pane layouts are not practical**”*.
  Źródło: developer.android.com/develop/ui/compose/layouts/adaptive/use-window-size-classes
- **Nie wolno wnioskować z orientacji ani z „czy tablet”** — to samo okno zmienia
  rozmiar przy obrocie, w split-screen i po rozłożeniu foldable.
  Źródło: developer.android.com/develop/ui/views/layout/use-window-size-classes
- **Tier 3 (podstawowy) wymaga pełnego okna i braku letterboxingu**; Tier 2 mówi
  o ograniczaniu szerokości *treści*, nie o udawaniu ramki telefonu.
  Źródło: developer.android.com/docs/quality-guidelines/adaptive-app-quality(/tier-2)
- **Blokada orientacji nie jest rozwiązaniem**: od Androida 16 (targetSdk 36)
  system ignoruje `screenOrientation`, `resizeableActivity`, `min/maxAspectRatio`
  na dużych ekranach; obejście znika w Androidzie 17.
  Źródło: developer.android.com/about/versions/16/behavior-changes-16
- `BoxWithConstraints` to `SubcomposeLayout` — „unikaj w gorących ścieżkach i na
  liście”, ale pojedyncze użycie u korzenia przy rzadko zmieniającym się progu
  (boolean) jest tanie.
  Źródło: developer.android.com/agents/skills/jetpack-compose/…/references/layouts

**Chiński (dokumentacja Xiaomi / HyperOS):** ta sama tabela — *较小的高度 · 高度 < 480dp
→ 99.78% 的手机处于横屏模式* („najmniejsza wysokość < 480 dp → 99,78 % telefonów
w trybie poziomym”) i zasada *感知窗口大小，而非屏幕* („rozmiar okna, nie ekranu”).
Źródło: dev.mi.com/xiaomihyperos/documentation/detail?pId=2047

**Hiszpański:** *„un móvil puede tener compact para el ancho y medium/expanded
para el alto, **que se invierte al cambiar la orientación**”* — czyli klasa
szerokości i wysokości zamieniają się miejscami przy obrocie.

**Rosyjski:** `BoxWithConstraints` daje `maxWidth`/`maxHeight` **rodzica**;
decyzje ekranowe podejmuje się z klas rozmiaru okna, nie z widoku.

**Japoński i niemiecki:** zapytania zwróciły materiał angielski — nie znalazłem
rodzimych źródeł, więc ich nie cytuję.

**Praktyka (najbliższy analog):** plan naprawy w projekcie Shuttle2 formułuje to
wprost: *„Layout is decided by window size class, never by orientation or
»is tablet«. Landscape phone = Medium/Expanded width × Compact height.
**No orientation locks, ever**.”*

## 4. Symulacja (dlaczego krok 1 to za mało)

Model odwzorowuje kod: chrome = TopBar 62 dp + NavBar 74 dp, wykres `chartH` = 220 dp.

| wariant (telefon 735×331, poziom) | wysokość treści | wykres 220 dp |
|---|---|---|
| 0.31.68: ramka 412 dp | 188 dp | **nie mieści się** |
| krok 1: pełne okno, dolny pasek | 195 dp | **nie mieści się** |
| **krok 1+2: pełne okno + szyna 80 dp** | **269 dp** | **mieści się** |

Zysk powierzchni treści z szyny: **+23 %** (telefon użytkownika, mały telefon),
**+16 %** (Pixel 7).

Martwe pole okna w 0.31.68 → po poprawce:

| okno | 0.31.68 | 0.31.69 |
|---|---|---|
| 735×331 (telefon, poziom) | 45,1 % | 0 % |
| 915×412 (Pixel 7, poziom) | 55,9 % | 0 % |
| 851×400 (split-screen) | 52,6 % | 0 % |
| 673×841 (foldable, pion) | 40,0 % | 0 % |
| 1280×800 (tablet) | 68,5 % | 0 % |

## 5. Decyzje (podjęte samodzielnie, na polecenie użytkownika)

1. **Zakres = krok 1 + krok 2.** Samo zdjęcie ramki nie naprawia wykresu
   w poziomie (195 dp < 220 dp), więc obie zmiany idą razem.
2. **Reguła szyny: `wysokość < 480 dp I szerokość >= 600 dp`.** Granice klas
   rozmiaru okna Androida. Nie „orientacja”, nie „czy tablet”, nie „czy telefon”.
3. **Tablet 1280×800 (dużo pionu) zostaje przy dolnym pasku** — 664 dp treści,
   wykres się mieści. Szyna jest odpowiedzią na **brak pionu**, nie na szerokość.
   Zmiana nawigacji na szerokich tabletach to osobna decyzja (krok 3).
   *(Ten punkt złapała symulacja: pierwsza wersja testu oczekiwała szyny na
   1280×800 i była sprzeczna z regułą — poprawiono test, nie kod.)*
4. **Krok 3 (ograniczenie szerokości treści) odłożony.** Nie był objawem;
   wymaga decyzji o wartościach granicznych na desktopie.
5. **Żadnej blokady orientacji** — od Androida 16 i tak ignorowana.
6. **Wersja podbita do 0.31.69** (versionCode 31069) — zasada „każda paczka =
   nowy numer” od 0.31.54.
7. **Martwy kod po ramce usunięty**: `AppModel.frameWidth`, `Dimens.frameW`,
   `Dimens.frameH` — jedno użycie, nic innego z nich nie korzystało.

## 6. Zmiany w kodzie

| plik | zmiana |
|---|---|
| `ui/SterownikApp.kt` | pełne okno zawsze; `Row` z opcjonalną szyną; `BoxWithConstraints` zostaje (mierzy okno) |
| `core/ScreenLayout.kt` **(nowy)** | `useNavRail()`, `contentHeightDp()` — czysta logika, testowalna bez Androida |
| `ui/components/Chrome.kt` | nowa `NavRail` (80 dp, te same 5 zakładek, pozycje z `weight(1f)`) |
| `ui/theme/Theme.kt` | `Dimens.railW = 80.dp`, usunięte `frameW`/`frameH` |
| `core/AppModel.kt` | usunięte `frameWidth` |
| `app/build.gradle.kts` | 0.31.69 / 31069 |
| `app/src/test/.../ScreenLayoutTest.kt` **(nowy)** | 6 testów na realnych wymiarach okien |

## 7. Weryfikacja

| sprawdzenie | wynik |
|---|---|
| `narzedzia/sprawdz_uklad.py` na 0.31.69 | **23 OK / 0 BŁĄD** |
| ten sam skrypt na 0.31.68 (test negatywny) | **17 BŁĄD** — brama potrafi zawieść |
| spójność testu z kodem (model w Pythonie) | **9/9 przypadków** |
| ZIP rozpakowany = katalog roboczy | **bit w bit, 149 plików** |
| balans nawiasów w 4 zmienionych plikach | OK |

**CZEGO NIE SPRAWDZONO:** kompilacji i testów JUnit — w środowisku nie ma Javy
ani Gradle. Pierwsze prawdziwe uruchomienie `:app:testDebugUnitTest` następuje
przy budowaniu APK. Ryzyko: literówka w API Compose, której nie widzi analiza
tekstowa. Wszystkie użyte API (`Row`, `weight`, `fillMaxHeight`, `BoxWithConstraints`,
`Modifier.width`, `Brush.linearGradient`) są już w projekcie używane w tym samym
pliku lub w `NavBar`.

## 8. Do sprawdzenia po wgraniu

1. Pion: wygląd bez zmian.
2. Poziom: pełne okno, nawigacja po lewej, wykres bez przewijania.
3. Obrót **w trakcie pracy** (Pulpit i Wykresy): układ przelicza się bez restartu
   i bez utraty stanu — `configChanges` w manifeście to gwarantuje, ale trzeba to
   zobaczyć.
4. Split-screen: bez martwego pola.
