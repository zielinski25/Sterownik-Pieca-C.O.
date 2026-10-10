# Android Direct Boot — bezpieczny delta-patch

`Android-DirectBoot-78299c9.diff` przenosi aktualne zmiany z katalogu `android/` na bazę Arena `78299c9` (`78299c9a5893f834cdbefd8172a6bb40bd3f6f18`). Zawiera źródła, testy jednostkowe i poprawkę odwołań do pętli alarmu. Patch nie zawiera literałów klucza API, e-maila/hasła ani tokenu.

## Użycie w czystej kopii

W czystym klonie gałęzi Arena (np. `piec-android-test`) pobierz najnowsze pliki dystrybucyjne:

```powershell
git pull --ff-only
```

Przed nałożeniem patcha sprawdź czystość kopii — oczekiwany wynik to brak wyjścia:

```powershell
git status --porcelain
```

Z katalogu głównego repozytorium najpierw wykonaj kontrolę, która niczego nie zmienia:

```powershell
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-78299c9.diff
```

Jeśli kontrola przejdzie bez błędu, zastosuj patch:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-78299c9.diff
```

Patch ma hunki bez kontekstu (`--unidiff-zero` jest wymagane), aby nie kopiować do artefaktu niezmienionych literałów konfiguracyjnych z `Prefs.kt`. Jeśli kontrola zgłosi błąd, zatrzymaj się — nie używaj `--reject` ani nie wymuszaj zastosowania.

**Jeśli w kopii testowej zastosowano już wcześniejszy patch i korektę `Android-DirectBoot-AlarmLoop-Fix.diff`, nie nakładaj ponownie patcha głównego.**

## Weryfikacja Gradle

Po zastosowaniu patcha uruchom:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

W tej sesji użytkownik potwierdził na Windows wynik `BUILD SUCCESSFUL` dla tych zadań. Pojawiły się jedynie ostrzeżenia o przestarzałych API; nie blokowały kompilacji. Nie instaluj APK ani nie testuj alarmów na piecu w ramach tej weryfikacji.

## Kontrole wykonane tutaj

- Patch sprawdzono na czystym worktree commita `78299c9`: `git apply --check --unidiff-zero` przechodzi, a po zastosowaniu katalog `android/` jest identyczny bajt w bajt z aktualnym źródłem.
- W diffie nie ma istniejących literałów konfiguracji Firebase ani hasła/tokenu.
- Parsowanie Kotlin kluczowych plików Direct Boot, spójność 19 plików XML oraz asercje tombstone przeszły.
- Pełną kompilację Gradle tutaj zastąpił potwierdzony przez użytkownika build Windows.

SHA-256 patcha: `816ae6aa85a84b71df382759bd8d871683302b0128608102157546d785cd9441`
