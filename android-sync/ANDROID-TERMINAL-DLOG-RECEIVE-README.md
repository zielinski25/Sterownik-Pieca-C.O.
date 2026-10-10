# Odbiór rzeczywistych wpisów DLOG w aplikacji Android

Obecna poprawka podłącza terminal Androida do wpisów DLOG zapisywanych przez centralę w Firebase RTDB. Wcześniej widok i przyciski były obecne, ale Android nie odczytywał strumienia.

Odczyt działa tylko wtedy, gdy arkusz terminala jest otwarty; aplikacja co 5 sekund sprawdza indeks sesji i pobiera nowe chunki. Obsługiwane są ścieżki firmware:

- `/piec/devices/<Firebase UID>/telemetry/diagnostics/<sesja>/chunks/<seq>` — aktualna,
- `/piec/telemetry/diagnostics/<sesja>/chunks/<seq>` — starsza.

Przy pierwszym wejściu do bieżącej sesji pobiera się maksymalnie 50 ostatnich chunków. Odczyt jest tylko do odczytu; sama poprawka nie wysyła poleceń do ESP32, nie testuje alarmów ani wyjść i nie wgrywa firmware. Gdy Firebase nie udostępni danych, komunikat stanu terminala pokaże błąd odczytu.

## Zastosowanie na Windows

Po pobraniu zmian w repozytorium uruchom z katalogu głównego:

```powershell
.\android-sync\Apply-Terminal-DLOG-Receive.ps1
```

Następnie zbuduj Debug i zainstaluj aplikację Android (to nie wgrywa firmware):

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r ".\app\build\outputs\apk\debug\app-debug.apk"
```

Otwórz w aplikacji **Serwis & Diagnostyka → Terminal diagnostyczny**. Odbiornik pokaże zapisane już wpisy oraz nowe wpisy z aktualnej sesji. Jeśli w Firebase nie ma żadnych chunków, terminal pozostanie pusty i pokaże odpowiedni status; nie tworzy fikcyjnych danych.
