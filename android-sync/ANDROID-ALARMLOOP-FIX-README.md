# Korekta kompilacji: pętla dźwięku alarmu

Ten mały patch jest przeznaczony dla kopii, w której zastosowano już `Android-DirectBoot-78299c9.diff`. Naprawia cztery błędne odwołania do `AlarmNotify.startAlarmLoop/stopAlarmLoop`; metody są zadeklarowane w `AlarmMonitorService`.

## Zastosowanie w `piec-android-test`

W kopii, w której poprzedni build zgłosił `Unresolved reference startAlarmLoop/stopAlarmLoop`, wykonaj z katalogu głównego repozytorium:

```powershell
git pull --ff-only
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-AlarmLoop-Fix.diff
```

Jeśli kontrola przejdzie bez błędu, zastosuj poprawkę i ponów build:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-AlarmLoop-Fix.diff
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

Commit dystrybucyjny dodaje tylko pliki w `android-sync/`; nie zmienia lokalnie zmodyfikowanych źródeł Androida. Nie używaj `git reset`, `git checkout --` ani `git restore`.

SHA-256: `a93f3c810ec0b43ff6d9a11b341d60cc73b6f962fa19e8762ff10eb12d9ed54e`
