# Usunięcie dopisku DEBUG z nazwy aplikacji

Zmienia nazwę pakietu Debug z `Sterownik CO (DEBUG)` na `Sterownik CO`. Dzięki temu dopisek nie pojawia się już w nazwie aplikacji ani w nagłówku powiadomienia usługi. Pozostają odrębny identyfikator pakietu `.debug` i ikona Debug.

## Zastosowanie

Po pobraniu zmian uruchom z katalogu głównego repozytorium:

```powershell
.\android-sync\Apply-Remove-Debug-Name.ps1
```

Następnie zbuduj i zainstaluj Debug:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r ".\app\build\outputs\apk\debug\app-debug.apk"
```
