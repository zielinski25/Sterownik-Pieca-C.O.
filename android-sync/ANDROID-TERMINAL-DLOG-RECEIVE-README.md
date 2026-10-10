# Terminal Android: odbiór DLOG i zapis do pliku

Terminal otwiera się na pełnym ekranie; jego zawartość przewija się pionowo, a lista konsoli ma własne przewijanie. Dolne skróty są pogrupowane po polsku. Po dotknięciu **Ustaw klapę…** lub **Ustaw syberek…** można wpisać pozycję od 0 do 100% i zatwierdzić ją osobno — nie ma stałej wartości 50%. Przycisk **Zapisz logi** pozostaje nieaktywny, dopóki aplikacja nie odbierze prawdziwych wpisów DLOG.

Odbiornik pobiera z Firebase RTDB ścieżkę bieżącego użytkownika:

- `/piec/devices/<Firebase UID>/telemetry/diagnostics/<sesja>/chunks/<seq>` — aktualna ścieżka,
- `/piec/telemetry/diagnostics/<sesja>/chunks/<seq>` — zgodność ze starszym firmware.

Odczyt działa wyłącznie, gdy arkusz terminala jest otwarty; aplikacja co 5 sekund sprawdza indeks sesji i nowe chunki. Przy pierwszym wejściu pobiera maksymalnie 50 ostatnich chunków bieżącej sesji. Odbiornik nie tworzy wpisów testowych ani nie wysyła poleceń do ESP32.

## Zapis logów

Dotknięcie **Zapisz logi** otwiera systemowy selektor pliku Androida. Można wybrać miejsce i nazwę pliku; domyślna nazwa to `Sterownik-CO-DLOG-<data>.txt`. Eksport jest tekstem UTF-8, zawiera krótki nagłówek sesji i tylko rzeczywiście odebrane wpisy. Uwzględnia wszystkie kategorie niezależnie od filtrów widoku, lecz eksportuje bufor aplikacji — maksymalnie 3500 najnowszych wierszy — a nie całą historię Firebase. Funkcja nie wymaga uprawnień do pamięci. Anulowanie selektora niczego nie zapisuje.

## Zbiorcza zmiana kategorii DLOG

Firmware nie obsługuje skrótu `diag remote cat ALL on/off`; przyjmuje jedną z 21 nazw kategorii w każdym poleceniu. Poprawka aplikacji wysyła więc kategorie pojedynczo, czekając na ACK ESP32 po każdej. Pasek statusu pokazuje postęp `n/21`; przy braku lub odrzuceniu ACK zatrzymuje serię i podaje kategorię oraz liczbę wykonanych zmian. Poprawka czeka też na zakończenie wcześniejszego polecenia terminala, a w czasie serii blokuje nowe kontrolki i utrzymuje blokadę kolejki, aby nie nadpisać pojedynczej komendy oczekującej w Firebase. Filtry lokalnego bufora nadal zmieniają się natychmiast. Skrypt zastosuje jedynie zmiany źródłowe — sam nie łączy się z Firebase ani piecem.

## Zastosowanie na Windows

Po pobraniu zmian, z katalogu głównego repozytorium uruchom:

```powershell
.\android-sync\Apply-Terminal-DLOG-Features.ps1
.\android-sync\Apply-Terminal-Controls.ps1
.\android-sync\Apply-Terminal-DLOG-Category-Batch.ps1
```

Skrypty można uruchamiać ponownie — rozpoznają już zastosowany stan. Skrypt kontrolek sprawdza bazę i nakłada wyłącznie UI terminala; skrypt kategorii nakłada obsługę kolejki ACK, a także aktualizuje wcześniejszą wersję poprawki kategorii, jeśli była już zastosowana. Żaden z nich nie wysyła poleceń do centrali.

Następnie zbuduj i zainstaluj **Debug APK** (to nie wgrywa firmware):

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r ".\app\build\outputs\apk\debug\app-debug.apk"
```

Otwórz **Serwis & Diagnostyka → Terminal diagnostyczny**. Logi pojawią się, jeśli bieżąca sesja ma chunki DLOG w Firebase; aplikacja nie generuje przykładowych wpisów. Sama poprawka eksportu niczego nie wysyła do kotła.
