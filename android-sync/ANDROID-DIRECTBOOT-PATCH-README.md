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

## Poprawka dla branding patcha już zastosowanego

Poprzednia wersja zmiany ikony miała błędny atrybut w `<aapt:attr>` i mogła zakończyć `:app:mergeDebugResources` komunikatem `attr tag requires the 'name' attribute`. Jeśli branding patch jest już zastosowany, nie nakładaj go ponownie. Z katalogu głównego repozytorium pobierz aktualizację i zastosuj jednowierszową poprawkę:

```powershell
git pull --ff-only
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-Debug-Branding-Resource-Fix.diff
git apply --unidiff-zero .\android-sync\Android-DirectBoot-Debug-Branding-Resource-Fix.diff
```

Poprawka zmienia `android:name` na `name` w `<aapt:attr>`, zgodnie z wymaganiem Android Resource Compiler. Następnie uruchom ponownie:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

## Weryfikacja

Użytkownik potwierdził `BUILD SUCCESSFUL` dla `:app:testDebugUnitTest :app:assembleDebug` przed zmianami brandingowymi oraz że po wylogowaniu debugowa aplikacja pozostała wylogowana po restarcie. Pierwszy build z brandingiem zatrzymał się na błędnym atrybucie ikony; nowa dystrybucja zawiera poprawioną składnię, która wymaga ponownego builda na Windows.

Weryfikacja na piecu nie wymaga symulacji alarmu ani zmiany wyjść. Nie testuj, gdy występuje aktywny alarm.

SHA-256 pełnego patcha: `422cdce7c490b1022fa74e0b0a64a54225355d4b3ef501134eae4a08de5a8a03`
SHA-256 poprawki debug dla już zmodyfikowanej kopii: `1fbbc363c9649c2a5c6438e7bda9895e8521de878c76f5c7623b57f45e22079e`
SHA-256 jednowierszowej poprawki zasobu: `86d4d0c9e24c2dcfcab9d06d0871f5f58ea9c367428647d60146c276b81a631b`
