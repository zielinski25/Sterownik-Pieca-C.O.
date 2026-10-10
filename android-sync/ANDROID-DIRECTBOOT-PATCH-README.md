# Android Direct Boot — zweryfikowany delta-patch

`Android-DirectBoot-78299c9.diff` przenosi aktualne zmiany z katalogu `android/` na bazę Arena `78299c9` (`78299c9a5893f834cdbefd8172a6bb40bd3f6f18`). Zawiera Direct Boot, tombstone wylogowania, poprawkę wywołań pętli alarmu oraz czytelne oznaczenie wariantu debug. Patch nie zawiera literałów klucza API, e-maila/hasła ani tokenu.

## Czysta kopia

W czystym klonie gałęzi Arena pobierz najnowsze pliki dystrybucyjne:

```powershell
git pull --ff-only
```

Sprawdź, że kopia jest czysta (brak wyjścia):

```powershell
git status --porcelain
```

Z katalogu głównego repozytorium najpierw sprawdź patch:

```powershell
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-78299c9.diff
```

Jeśli nie ma błędu, zastosuj:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-78299c9.diff
```

## Kopia testowa z poprzednimi patchami

Jeśli w `piec-android-test` masz już zastosowany wcześniejszy patch Direct Boot oraz `Android-DirectBoot-AlarmLoop-Fix.diff`, **nie nakładaj ponownie pełnego patcha**. Zrób tylko:

```powershell
git pull --ff-only
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-Debug-Branding.diff
```

Jeśli kontrola przejdzie, zastosuj małą zmianę rozróżniającą debug:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-Debug-Branding.diff
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

Wariant debug otrzyma nazwę `Sterownik CO (TEST)`, fioletowe tło ikony oraz prefiks `TEST` w alarmowym powiadomieniu; powiadomienie usługi będzie zawierało nazwę wariantu. Wariant release pozostaje bez tych oznaczeń. Po buildzie zainstaluj ponownie debug APK, aby zobaczyć nową ikonę i powiadomienia.

Patch ma hunki bez kontekstu (`--unidiff-zero` jest wymagane), aby nie kopiować do artefaktu niezmienionych literałów konfiguracyjnych z `Prefs.kt`. Jeśli `--check` zgłosi błąd, zatrzymaj się — nie używaj `--reject` ani nie wymuszaj zastosowania.

## Weryfikacja

Użytkownik potwierdził `BUILD SUCCESSFUL` dla `:app:testDebugUnitTest :app:assembleDebug` oraz że po wylogowaniu debugowa aplikacja pozostała wylogowana po restarcie. Zmiana oznaczeń debug jest nowa i wymaga ponownego builda debug przed instalacją.

Weryfikacja na piecu nie wymaga symulacji alarmu ani zmiany wyjść. Nie testuj, gdy występuje aktywny alarm.

SHA-256 pełnego patcha: `902c59fb73f6f81e10b840756b6a8c58e95d085120fa348f8ae4457683e24cbe`
SHA-256 poprawki debug dla już zmodyfikowanej kopii: `b5ebdafb96c9f89c97e43c46eed6807df826517b82920db6bfce1604b86375c5`
