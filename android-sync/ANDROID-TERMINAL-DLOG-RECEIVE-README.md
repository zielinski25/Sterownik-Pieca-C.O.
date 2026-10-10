# Terminal Android: odbiór DLOG i zapis do pliku

Arkusz terminala jest dostosowany do ekranu telefonu: zajmuje 75% wysokości, wypełnia szerokość i przewija się pionowo. Lista konsoli ma własne przewijanie. Przycisk **Zapisz logi** znajduje się przy akcjach terminala i pozostaje nieaktywny, dopóki aplikacja nie odbierze prawdziwych wpisów DLOG.

Odbiornik pobiera z Firebase RTDB ścieżkę bieżącego użytkownika:

- `/piec/devices/<Firebase UID>/telemetry/diagnostics/<sesja>/chunks/<seq>` — aktualna ścieżka,
- `/piec/telemetry/diagnostics/<sesja>/chunks/<seq>` — zgodność ze starszym firmware.

Odczyt działa wyłącznie, gdy arkusz terminala jest otwarty; aplikacja co 5 sekund sprawdza indeks sesji i nowe chunki. Przy pierwszym wejściu pobiera maksymalnie 50 ostatnich chunków bieżącej sesji. Odbiornik nie tworzy wpisów testowych ani nie wysyła poleceń do ESP32.

## Zapis logów

Dotknięcie **Zapisz logi** otwiera systemowy selektor pliku Androida. Można wybrać miejsce i nazwę pliku; domyślna nazwa to `Sterownik-CO-DLOG-<data>.txt`. Eksport jest tekstem UTF-8, zawiera krótki nagłówek sesji i tylko rzeczywiście odebrane wpisy. Uwzględnia wszystkie kategorie niezależnie od filtrów widoku, lecz eksportuje bufor aplikacji — maksymalnie 3500 najnowszych wierszy — a nie całą historię Firebase. Funkcja nie wymaga uprawnień do pamięci. Anulowanie selektora niczego nie zapisuje.

## Zastosowanie na Windows

Po pobraniu zmian uruchom z katalogu głównego repozytorium:

```powershell
.\android-sync\Apply-Terminal-DLOG-Features.ps1
```

Skrypt można uruchomić ponownie: rozpoznaje już zastosowane poprawki. Jeśli odbiornik DLOG był wcześniej zastosowany osobnym skryptem, doda tylko eksport.

Następnie zbuduj i zainstaluj **Debug APK** (to nie wgrywa firmware):

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r ".\app\build\outputs\apk\debug\app-debug.apk"
```

Otwórz **Serwis & Diagnostyka → Terminal diagnostyczny**. Logi pojawią się, jeśli bieżąca sesja ma chunki DLOG w Firebase; aplikacja nie generuje przykładowych wpisów. Sama poprawka eksportu niczego nie wysyła do kotła.
