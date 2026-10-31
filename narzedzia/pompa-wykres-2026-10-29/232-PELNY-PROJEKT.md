# 232 — koniec „kawałków": od teraz zawsze pełny projekt

Data 2026-10-29 wieczór. Wersja: **0.31.54**.

## Skąd wziął się błąd

Przejrzałem Dysk Google i wyszło, jak było naprawdę:

| wersja | co dostawałeś | rozmiar |
|---|---|---|
| 0.31.32 – 0.31.51 | **PELNY-PROJEKT** (cały projekt) | 1,15 – 1,34 MB |
| 0.31.52 | „NIEBO — 2 pliki" | 9,5 kB |
| 0.31.53 (1) | „ZBIORNIK — 12 plików" | 101 kB |
| 0.31.53 (2) | „POMPA — 3 pliki" | 22 kB |
| 0.31.54 | „STARE NAZWY — 14 plików" | 108 kB |

**Zmiana nastąpiła przy 0.31.52.** Do tego momentu każda paczka była kompletnym
projektem — rozpakowywałeś i budowałeś. Od 0.31.52 zacząłem wysyłać wycinki,
które trzeba było nakładać na stary folder. I właśnie na tym się przewróciłem:
dwa różne wycinki dostały ten sam numer 0.31.53, a w Ustawieniach oba
pokazywały `v0.31.53`.

Wniosek: **błąd nie był w kodzie, tylko w sposobie pakowania.** Wycinek nie
zawiera informacji o tym, na jakim stanie został nałożony. Pełny projekt
zawiera wszystko — nie ma czego pomylić.

## Nowa zasada

**Każda wersja = jeden kompletny ZIP całego projektu, gotowy do zbudowania.**
Wycinki („zmienione pliki") mogą iść najwyżej jako dodatek, nigdy jako jedyna
paczka.

## Paczka 0.31.54 — pełny projekt

- `SterownikCO-Android-v0.31.54-PELNY-PROJEKT.zip`
- **1 165 145 B** (1,11 MB)
- SHA-256 `f49971540c33beb6db9629e2915c61596f82c9d24bc457d4de6a2617c5ba3930`
- **144 pliki** = dokładnie tyle, ile jest w repozytorium, sprawdzone plik po pliku
  (`diff` listy z gita i listy z ZIP-a: **0 różnic**)

Co w środku:

| element | ile |
|---|---|
| pliki `.kt` | 96 |
| wpisy `app/src/main/res/` | 39 |
| `gradlew`, `gradlew.bat` | są |
| `gradle/wrapper/gradle-wrapper.jar` | jest, SHA identyczny z repo (`cb0da675…`) |
| `build.gradle.kts`, `settings.gradle.kts`, `gradle.properties` | są |
| `AndroidManifest.xml` | jest |

## Co sprawdziłem w środku (żeby nie było niespodzianki przy buildzie)

1. **Kompletność** — 144/144 pliki z gita są w ZIP-ie, zero braków.
2. **Graf zależności** — przeskanowałem wszystkie 96 plików `.kt`: 14 pakietów,
   każdy import `com.sterownikco.pro.*` ma swoje źródło. Jedyne „nieznane" to
   `R` i `BuildConfig` (generowane przy buildzie) oraz funkcje-rozszerzenia
   (`fun DrawScope.drawSvg`, `fun RowScope.QCard`, `fun <T> Seg`), których mój
   skrypt nie rozpoznał — wszystkie trzy istnieją.
3. **Wrapper** — `gradle-wrapper.jar` w ZIP-ie ma ten sam SHA-256 co w repo.
4. **Wersja** — `versionCode 31054`, `versionName "0.31.54"` — sprawdzone
   w pliku wyciągniętym z ZIP-a.
5. **Nazwy** — w tekstach UI **zero** wystąpień „Zbiornik".
6. **Pobrane z Dysku** — SHA-256 identyczny bit w bit z paczką w repo.

## Czego ten test NIE sprawdza

Nie mam tu Androida SDK, więc **nie zbudowałem APK**. Sprawdziłem kompletność
i spójność źródeł, nie kompilację. Pierwszy prawdziwy build robisz u siebie —
jeśli wywali błąd, to będzie błąd składni albo zależności, nie brak pliku.

## Czego nie zmieniono w kodzie

Ten sam kod co w paczce 14-plikowej: stare nazwy (Panel słon. / Kolektor
słoneczny próżniowy), wykres pompy na całą wysokość, model z niebem 0.31.52.
Firmware bez zmian — 3.56.7 zostaje, 3.56.8 czeka.
