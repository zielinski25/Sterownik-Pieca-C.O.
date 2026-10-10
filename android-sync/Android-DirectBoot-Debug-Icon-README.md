# Debugowa ikona — wybrana opcja kolorystyczna nr 2

Pakiet zawiera tę samą wybraną grafikę w dwóch **alternatywnych sposobach zastosowania**. Wybierz tylko ścieżkę odpowiadającą stanowi źródeł; nie nakładaj obu patchy.

- `Android-DirectBoot-Debug-Icon.diff` — samodzielne dodanie zasobów ikony debug do źródeł, w których **nie ma jeszcze** debugowego `mipmap-anydpi-v26/ic_launcher.xml`.
- `Android-DirectBoot-Debug-Icon-Overlay.diff` — zalecana nakładka, jeśli wcześniejszy patch utworzył już debugową ikonę i plik `drawable-nodpi/ic_launcher_debug_art.png`. Podmienia tylko ten obraz na wybraną opcję nr 2; zachowuje istniejące XML, wariant round i monochromatyczny.

Żaden patch nie zmienia zasobów `src/main`, manifestu produkcyjnego, logiki powiadomień ani firmware. Nazwa debug i tekst powiadomienia `TEST` pozostają takie jak wcześniej.

## Którą ścieżkę wybrać?

### A. Projekt po pełnym patchu bezpieczeństwa/Direct Boot

Jeśli najpierw zastosowano `Android-DirectBoot-78299c9-Audited.diff` albo `Android-DirectBoot-Security-Battery-Icon.diff`, użyj **tylko nakładki**:

```powershell
git apply --check .\android-sync\Android-DirectBoot-Debug-Icon-Overlay.diff
if ($LASTEXITCODE -ne 0) { throw "Kontrola nie przeszła — zatrzymaj się, nie wymuszaj patcha." }
git apply .\android-sync\Android-DirectBoot-Debug-Icon-Overlay.diff
```

Oba wcześniejsze pakiety zawierają bazowy debugowy obraz o tej samej ścieżce i treści początkowej; nakładka aktualizuje go do wybranej grafiki. Nie stosuj wtedy `Android-DirectBoot-Debug-Icon.diff` — kolidowałby z istniejącym `ic_launcher.xml`.

### B. Starszy projekt bez debugowej ikony adaptive

Jeśli `android/app/src/debug/res/mipmap-anydpi-v26/ic_launcher.xml` **nie istnieje**, użyj samodzielnego patcha:

```powershell
git apply --check .\android-sync\Android-DirectBoot-Debug-Icon.diff
if ($LASTEXITCODE -ne 0) { throw "Kontrola nie przeszła — zatrzymaj się, nie wymuszaj patcha." }
git apply .\android-sync\Android-DirectBoot-Debug-Icon.diff
```

Nie nakładaj tego wariantu na pełny patch bezpieczeństwa/ikony ani na projekt, w którym ten plik już istnieje.

## Build lokalny

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

W tym środowisku nie było Java/Android SDK, więc nie uruchomiono Gradle ani testów urządzenia. Build trzeba wykonać lokalnie przed instalacją. Żaden z tych kroków nie wgrywa firmware ani nie wysyła poleceń do kotła.
