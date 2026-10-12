# Wspólne wyciszenie alarmów — poprawka firmware centrali

`Android-Shared-Alarm-Mute-Firmware.patch` zawiera zmianę kodu dla `src/main_centrala.cpp`. Patch jest osobny — nie zmienia ani nie zastępuje `Cenatrala Pieca.zip`.

## Co dodaje

- Komendy Firebase: `alarm_mute dym <sekundy|off>`, `alarm_mute ogrz <sekundy|off>` oraz atomowe `alarm_mute oba <sekundy|off>`.
- `0` wycisza aktywny alarm do jego ustąpienia; `1..3600` ustawia timer w sekundach; `off` natychmiast kończy wyciszenie.
- Po upływie timera dźwięk automatycznie wraca, jeśli alarm nadal trwa. `oba` wycisza obie aktywne klasy alarmu jednym poleceniem.
- Status `/piec/status` zawiera pola `alarm_dym_wyciszony`, `alarm_dym_wyciszenie_ms`, `alarm_ogrz_wyciszony` i `alarm_ogrz_wyciszenie_ms`, dzięki czemu stan może być synchronizowany pomiędzy telefonami.

## Przygotowanie pliku na Windows

Użyj `Prepare-Shared-Alarm-Mute-Firmware.ps1`. Skrypt:

1. Odczytuje lokalny `Cenatrala Pieca.zip` z głównego checkoutu i rozpakowuje go do katalogu tymczasowego.
2. Sprawdza i nakłada patch (obsługuje różnice CRLF/LF).
3. Weryfikuje wejściową wersję `v3.36.2`, dodaje wpis changeloga i ustawia `FIRMWARE_VERSION` na `v3.37.0`.
4. Kopiuje gotowy plik na Pulpit jako `main_centrala-v3.37.0-shared-alarm-mute.cpp`.

Uruchom skrypt z katalogu głównego build worktree:

```powershell
.\android-sync\Prepare-Shared-Alarm-Mute-Firmware.ps1
```

Jeśli bieżący katalog to `android`, użyj:

```powershell
..\android-sync\Prepare-Shared-Alarm-Mute-Firmware.ps1
```

Skrypt nie modyfikuje ZIP-a. Przerywa działanie, jeżeli lokalny plik nie jest w wersji `v3.36.2` albo patch nie pasuje. Nie kompiluje ani nie flashuje firmware. Nie uruchamiaj `pio run -t upload` ani `pio run -e esp32-s3-ota -t upload`.

## Ograniczenia testów

Nie wysyłano komend do działającego pieca, nie symulowano alarmów i nie testowano firmware na urządzeniu. Firmware i APK wymagają lokalnej kompilacji; brak potwierdzenia kompilacji firmware w tym środowisku.
