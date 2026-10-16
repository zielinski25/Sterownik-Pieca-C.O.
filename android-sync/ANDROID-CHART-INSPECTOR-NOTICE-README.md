# Wykresy i powiadomienie usługi Androida

Ten patch odpowiada za dwa elementy widoczne na zrzutach:

- usuwa zaznaczony pasek `INSPEKTOR` z ekranu Wykresów i jego nieużywany renderer; wykres, dymek po dotknięciu oraz dolna instrukcja gestów pozostają;
- zmienia tekst powiadomienia usługi z technicznego przypomnienia o RTDB na krótkie, neutralne: `Usługa alarmów działa w tle.`

Znacznik `(TEST)` w tytule powiadomienia Debug zostaje — identyfikuje aplikację testową. Patch nie zmienia działania alarmów, RTDB ani sterownika kotła.

## Zastosowanie

Po pobraniu commita z gałęzi projektu, uruchom z katalogu głównego repozytorium:

```powershell
.\android-sync\Apply-Chart-Inspector-Description.ps1
```

Skrypt najpierw sprawdza stan źródeł, stosuje patch bez nadpisywania plików przy konflikcie i można go bezpiecznie uruchomić ponownie. Plik PowerShell ma BOM UTF-8 i ASCII-owy kod, aby działał także w Windows PowerShell 5.1.

Następnie zbuduj Debug:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```
