# Usunięcie dodatkowych elementów wykresu i etykiety TEST

Patch usuwa z ekranu Wykresów zaznaczony pasek fazy pieca oraz instrukcje gestów (także w widoku pełnoekranowym). Sam wykres, legenda odcinków rzeczywistych/symulowanych, suwak zakresu i dymek wartości po dotknięciu pozostają.

Z powiadomień Debug usuwa etykietę `TEST`: nazwa aplikacji będzie `Sterownik CO (DEBUG)`, a rzeczywiste powiadomienie alarmowe nie będzie oznaczane jako `TEST`. Etykieta `DEBUG` i istniejąca ikona nadal odróżniają pakiet testowy. Tekst powiadomienia usługi pozostaje `Usługa alarmów działa w tle.`

## Zastosowanie

Po pobraniu zmian uruchom z katalogu głównego repozytorium:

```powershell
.\android-sync\Apply-Chart-Cleanup-Debug-Label.ps1
```

Skrypt sprawdza stan plików i można go uruchomić ponownie. W razie różnic lub częściowo naniesionych zmian zatrzyma się bez nadpisywania źródeł. Następnie zbuduj i zainstaluj Debug:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r ".\app\build\outputs\apk\debug\app-debug.apk"
```

Patch dotyczy wyłącznie interfejsu i etykiet aplikacji; nie zmienia RTDB, logiki alarmów ani wyjść kotła.
