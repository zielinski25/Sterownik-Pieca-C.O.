# Pamięć pobranych dni wykresów (Android)

Do tej pory każde otwarcie wykresu pobierało z Firebase cały wybrany zakres. Dla 30 dni było to ok. 9 MB, czyli ok. 43 tys. rekordów, a działo się to także po każdym powrocie do aplikacji. Teraz aplikacja zapamiętuje pobrane doby w telefonie i z Firebase dociąga tylko nowe rekordy. Pierwsze pobranie 30 dni kosztuje tyle samo co wcześniej. Każde kolejne odświeżenie to zwykle jedno małe zapytanie o dzisiejsze dane.

**Historia w Firebase nie jest usuwana ani zmieniana.** Aplikacja tylko czyta `/piec/telemetry/1m/v1/...`. Kasuje wyłącznie własne pliki w pamięci podręcznej telefonu.

## Jak to działa

- **Jednostka pamięci:** doba UTC, czyli dokładnie jeden shard `/piec/telemetry/1m/v1/RRRR/MM/DD`.
- **Pliki:** `cache/telemetry-1m-v1/<host bazy>/RRRR-MM-DD.tday.gz`. To gzip, ok. 40 KB na dobę i ok. 1,4 MB na 35 dni.
  - Katalog cache nie trafia do kopii zapasowej (`allowBackup=false`).
  - Przy braku miejsca system może go wyczyścić. Wtedy dane zostaną po prostu pobrane ponownie.
- **Doba kompletna (zamknięta):** taka, dla której przy zapytaniu w bazie był już rekord nowszy niż jej koniec. Sterownik wysyła telemetrię po kolei, od najstarszej próbki. O dobę kompletną aplikacja już nigdy nie pyta.
- **Doba otwarta (zwykle dzisiejsza):** aplikacja pyta tylko o rekordy z `ts` od ostatniego znanego włącznie. Ewentualny duplikat na granicy usuwa deduplikacja po id rekordu.
  - Doby są odpytywane od najnowszej, więc wczorajsza zamyka się już przy pierwszym odświeżeniu po północy UTC.
  - Strony mają po 1000 rekordów i kursor włączny. Rekordy z tym samym `ts` na granicy strony nie giną.
- **Brak sieci:**
  - Doba, której nie udało się pobrać, nigdy nie jest oznaczana jako kompletna.
  - Wykres pokazuje kopię z telefonu ze znacznikiem **PAMIĘĆ TELEFONU**.
  - Pozostałe doby nie są wtedy odpytywane, więc aplikacja nie czeka N razy na timeout.
  - Gdy kopii nie ma, wykres pokazuje **BŁĄD POBRANIA**. Wcześniej wyglądało to jak „brak danych”.
- **Status pod wykresem:** np. `Załadowano 43180 próbek · z pamięci telefonu: 30 dni · nowe z RTDB: 12`. Przy pierwszym pobraniu wielu dób pokazuje postęp `doba 5/31`.
- **Wznowienie aplikacji:** historia odświeża się tylko wtedy, gdy otwarty jest ekran Wykresów. Każde nowe ładowanie anuluje poprzednie. Rozwiązuje to uwagi z `AUDYT_ANDROID_BEZPIECZENSTWO_PAMIEC_BATERIA.md` o `onResume` i o pojedynczym, anulowalnym zadaniu historii.
- **RAM:** trzymane są tylko doby bieżącego zakresu. Doby kompletne mają same punkty wykresu, bez surowych rekordów.
- **Sprzątanie:** kopie starsze niż 35 dni są usuwane z telefonu. Wylogowanie usuwa całą lokalną kopię.
- **Przycisk awaryjny:** Logi → **„Wykresy: pobierz historię od nowa”** kasuje tylko kopię w telefonie. Następne otwarcie wykresu pobierze zakres ponownie.

## Zastosowanie

Po pobraniu zmian uruchom z katalogu głównego repozytorium:

```powershell
.\android-sync\Apply-Telemetry-Day-Cache.ps1
```

Skrypt rozpozna, czy zmiany są już w źródłach (np. po `git pull` tej gałęzi). Wtedy tylko to zgłosi. Przy częściowo naniesionych zmianach zatrzyma się bez nadpisywania plików. Następnie zbuduj i zainstaluj Debug:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r ".\app\build\outputs\apk\debug\app-debug.apk"
```

## Zalecane: indeks `ts` w regułach RTDB

Bez indeksu Firebase odrzuca zapytania `orderBy="ts"` (HTTP 400). Aplikacja przechodzi wtedy na pobieranie całego shardu doby. Pamięć dób dalej działa, ale dla dzisiejszej doby pobierany jest cały jej shard (pod koniec dnia do ok. 290 KB) zamiast samych nowych rekordów. Dopisz do **istniejących** reguł bazy, bez zastępowania ich:

```json
"piec": {
  "telemetry": {
    "1m": {
      "v1": {
        "$y": { "$m": { "$d": { ".indexOn": ["ts"] } } }
      }
    }
  }
}
```

## Zakres zmian

- `core/TelemetryDayStore.kt` (nowy) zawiera czystą logikę dób, scalania, zapisu i odczytu plików oraz sprzątania, bez zależności od Androida.
- `core/RtdbClient.kt` dostał nową funkcję `fetchTelemetry(rangeSec, store, onProgress)`.
  - Pobiera rekordy doby od zadanego `ts`.
  - Błędy HTTP i sieci nie są już po cichu traktowane jak pusty dzień.
- `core/AppModel.kt`:
  - jedno anulowalne zadanie historii;
  - `onResume` tylko na ekranie Wykresów;
  - nowe statusy;
  - czyszczenie kopii przy wylogowaniu.
- `ui/screens/sheets/System.kt` (Logi) dostał przycisk „Wykresy: pobierz historię od nowa”.
- `test/.../TelemetryDayStoreTest.kt` zawiera 16 testów, m.in.:
  - kolejność od najnowszej doby i zamykanie dób;
  - przyrostowe dociąganie bez duplikatów;
  - błąd sieci;
  - odczyt kopii po restarcie;
  - uszkodzony plik;
  - sprzątanie;
  - stronicowanie z kursorem włącznym.

Firmware ESP32, `Piec.html` i dane w Firebase pozostają bez zmian. Panel WWW nadal pobiera pełny zakres przy otwarciu wykresu.
