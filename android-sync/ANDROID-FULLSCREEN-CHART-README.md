# Naprawa widoku pełnoekranowego wykresu

W oknie pełnoekranowym karta wykresu wypełnia teraz dostępną wysokość ekranu, a sam wykres zajmuje pozostałą przestrzeń między nagłówkiem i paskiem zakresu. Tytuł pozostaje w jednym wierszu. Zachowane są kursor, powiększanie, przeciąganie zakresu, legenda i zamykanie okna.

## Zastosowanie

Po pobraniu zmian z gałęzi uruchom z katalogu głównego repozytorium:

```powershell
.\android-sync\Apply-Chart-Cleanup-Debug-Label.ps1
.\android-sync\Apply-Fullscreen-Chart-Layout.ps1
```

Pierwszy skrypt idempotentnie nanosi poprzednio zamówione usunięcie paska fazy i podpowiedzi gestów oraz etykietę `(DEBUG)`. Drugi powiększa wykres pełnoekranowy. Potem zbuduj i zainstaluj Debug:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r ".\app\build\outputs\apk\debug\app-debug.apk"
```
