# Patch bezpieczeństwa, baterii i debugowej ikony

Pakiet zawiera:

- `Android-DirectBoot-78299c9-Audited.diff` — **pełny patch** do czystej bazy Android `78299c9`;
- `Android-DirectBoot-Security-Battery-Icon.diff` — **nakładkę** dla kopii, w której poprzedni Direct Boot/debug-branding jest już zastosowany;
- `Remove-Obsolete-Android-Default-FB-Password.ps1` — usuwa nieużywaną deklarację po nazwie, bez powielania jej wartości w patchu;
- `ANDROID-SECURITY-BATTERY-AUDIT.md` — ustalenia, dowody, ryzyko resztkowe i ograniczenia testów.

Patch zawiera wybrany debugowy artwork nr 2 jako zasób binarny. Nie zmienia produkcyjnej ikony ani firmware.

## Czysta kopia od bazy 78299c9

Z katalogu głównego klona:

```powershell
git pull --ff-only
git status --porcelain

git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-78299c9-Audited.diff
```

Jeśli kontrola przejdzie:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-78299c9-Audited.diff
powershell -NoProfile -ExecutionPolicy Bypass -File .\android-sync\Remove-Obsolete-Android-Default-FB-Password.ps1
```

## Kopia testowa z wcześniejszym Direct Boot i brandingiem

Jeśli w `piec-android-test` masz już poprzedni patch Direct Boot, poprawkę pętli alarmu, debug branding oraz poprawkę zasobu Androida, zastosuj **tylko nakładkę**:

```powershell
git pull --ff-only
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-Security-Battery-Icon.diff
```

Jeśli kontrola przejdzie:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-Security-Battery-Icon.diff
powershell -NoProfile -ExecutionPolicy Bypass -File .\android-sync\Remove-Obsolete-Android-Default-FB-Password.ps1
```

Jeżeli `--check` nie przechodzi, zatrzymaj się: nie używaj `--reject` i nie wymuszaj patcha. Nie nakładaj patcha pełnego na już zmodyfikowaną kopię.

## Build lokalny

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

To są zadania kompilacji/testów jednostkowych — nie flashują ESP32 i nie wysyłają poleceń do kotła. Nie instaluj APK ani nie wykonuj testów wyjść/alarmów na działającym piecu. Test IP jest jednostkowy i nie otwiera połączenia sieciowego.

## Ważne

- `TEST` rozróżnia wariant aplikacji, **nie** backend. Debug nadal wskazuje tę samą RTDB; traktuj go jako zdolny do wysłania poleceń, dopóki nie powstanie osobny backend albo jawny read-only guard.
- `allowBackup=false` i reguły backupu wykluczają również transfer urządzenie–urządzenie; ustawienia mogą nie przenieść się na nowy telefon.
- Nowe zmiany nie były kompilowane ani uruchamiane w tym sandboxie (brak JDK). Build na komputerze użytkownika jest wymagany przed instalacją.
