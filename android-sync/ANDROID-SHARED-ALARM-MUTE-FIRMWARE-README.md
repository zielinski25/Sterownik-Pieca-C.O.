# Wspólne wyciszenie alarmów — poprawka firmware centrali

Plik `Android-Shared-Alarm-Mute-Firmware.patch` jest **osobnym patchem źródłowym** dla `src/main_centrala.cpp`. Nie zmienia ani nie zastępuje `Cenatrala Pieca.zip`.

## Co dodaje

- Komendy Firebase: `alarm_mute dym <sekundy|off>`, `alarm_mute ogrz <sekundy|off>` oraz atomowe `alarm_mute oba <sekundy|off>`.
- `0` wycisza dany aktywny alarm do jego ustąpienia; `1..3600` ustawia timer w sekundach; `off` natychmiast kończy wyciszenie. Każdy telefon, który ma także wyciszać własny alarm, wymaga zaktualizowanej aplikacji Android.
- Wyciszenie czasowe po upływie timera automatycznie wznawia dźwięk, jeśli alarm nadal trwa. Komenda `oba` ustawia obie aktywne klasy alarmu w ramach jednego polecenia.
- Cztery pola w JSON statusu centrali (`/piec/status`): `alarm_dym_wyciszony`, `alarm_dym_wyciszenie_ms`, `alarm_ogrz_wyciszony`, `alarm_ogrz_wyciszenie_ms`. Pozostały czas jest raportowany w ms; `-1` oznacza wyciszenie do ustąpienia alarmu, `0` — brak aktywnego wyciszenia. Android wykorzystuje istniejące `updatedAt` tylko do rozpoznania powtórzonej/starej migawki, by jej ponowny odczyt nie przedłużał lokalnego timera.

## Zastosowanie bez naruszania ZIP-a

1. Zrób kopię ZIP-a i wypakuj kopię roboczą do osobnego katalogu.
2. W katalogu wypakowanym (tam, gdzie jest `src/main_centrala.cpp`) najpierw sprawdź patch:

   ```powershell
   git apply --check "C:\ścieżka\do\Android-Shared-Alarm-Mute-Firmware.patch"
   ```

3. Jeśli kontrola przejdzie, zastosuj patch w tej kopii:

   ```powershell
   git apply "C:\ścieżka\do\Android-Shared-Alarm-Mute-Firmware.patch"
   ```

4. W razie potrzeby można uruchomić wyłącznie kompilację PlatformIO (`pio run`). **Nie uruchamiaj `pio run -t upload` ani `pio run -e esp32-s3-ota -t upload`; nie flashuj centrali.**

Patch powstał dla źródła wyodrębnionego z lokalnego ZIP-a użytkownika i celowo pozostaje oddzielony od archiwum. Nie wysyłano komend do działającego pieca ani nie testowano firmware na urządzeniu. Kompilacja firmware i Androida nie została jeszcze wykonana.
