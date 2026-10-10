# Android Direct Boot — delta-patch

Patch `Android-DirectBoot-78299c9.diff` przenosi aktualne zmiany z katalogu `android/` na bazę Arena `78299c9` (`78299c9a5893f834cdbefd8172a6bb40bd3f6f18`). Zawiera zmiany źródeł Androida i testy jednostkowe; nie jest wynikiem kompilacji Gradle.

## Pobranie patcha przez istniejący klon testowy

W czystym klonie `C:\Users\Rafcio\Desktop\piec-android-test` wykonaj:

```powershell
git pull --ff-only
```

Ten commit dystrybucyjny dodaje tylko pliki patcha i instrukcję w `android-sync/`; nie zmienia źródeł `android/`. Nie wykonuj tych poleceń w zmodyfikowanym folderze `piec`.

## Zastosowanie

Z katalogu głównego `piec-android-test` sprawdź najpierw, czy kopia jest czysta:

```powershell
git status --porcelain
```

Oczekiwane: brak wyjścia. Następnie wykonaj kontrolę, która nie zmienia plików:

```powershell
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-78299c9.diff
```

Jeśli kontrola przejdzie bez błędu, zastosuj patch:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-78299c9.diff
```

Patch ma hunki bez kontekstu (`--unidiff-zero` jest wymagane), aby nie kopiować do artefaktu niezmienionych literałów konfiguracyjnych z `Prefs.kt`. Pozostawia istniejące wartości konfiguracyjne w pliku bazowym bez zmian. Jeśli kontrola zgłosi błąd, zatrzymaj się — nie używaj `--reject` ani nie wymuszaj zastosowania.

## Weryfikacja Gradle

Po zastosowaniu patcha uruchom:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

Przekaż końcowy komunikat Gradle (`BUILD SUCCESSFUL` albo pełny błąd). Nie instaluj APK ani nie testuj alarmów na piecu w ramach tej weryfikacji.

## Kontrole wykonane tutaj

- Patch sprawdzono na czystym worktree commita `78299c9`: `git apply --check --unidiff-zero` przechodzi, a po zastosowaniu katalog `android/` jest identyczny bajt w bajt z aktualnym źródłem.
- W diffie nie ma istniejących literałów API key, e-maila/hasła ani tokenu.
- Parsowanie Kotlin dla kluczowych plików Direct Boot, spójność 19 plików XML oraz asercje kolejności tombstone przeszły.
- Pełna kompilacja Gradle nie została wykonana w tym środowisku.

SHA-256 patcha: `95fc38ad0e54d2d9980d3617034a8dfc69baf45a8967c0355eec7e5933fc698f`
