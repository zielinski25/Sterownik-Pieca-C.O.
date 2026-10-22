# v3.54.0 — przełącznik „bufor offline" z aplikacji Android

Data: 2026-10-22
Baza: v3.53.1 (ten sam `main_centrala.cpp` + nagłówki + panel)
**Firmware nie był kompilowany w tym środowisku** — build i OTA robi użytkownik.

## Po co to jest

Do tej pory bufor offline (runtime Spool G2) dało się włączyć **tylko z Telegrama**
(`/spoolrt TAK`). Były trzy blokady:

| problem | skutek |
|---|---|
| `spoolrt` nie było na liście komend `/piec/cmd` | aplikacja nie mogła tego włączyć |
| `/piec/status` nie publikował stanu | aplikacja nie miała czego pokazać |
| flaga żyła **tylko w RAM** | po zaniku prądu bufor był OFF dokładnie wtedy, gdy był potrzebny |

v3.54.0 usuwa wszystkie trzy.

## Co dokładnie zmieniono

| miejsce | zmiana |
|---|---|
| `wykonajKomendePanelu()` | nowe komendy **`spoolrt_wl`** i **`spoolrt_wyl`** (ta sama funkcja co `/spoolrt TAK` / `/spoolrt STOP`) |
| `zbudujJsonStatusPelny()` | 4 nowe pola: `spool_rt`, `spool_pages`, `spool_spill`, `spool_replay` |
| `spoolRuntimePersistArmed()` | **jeden bajt** w NVS: namespace `spoolg`, klucz `rt_on` |
| `spoolRuntimeRestoreFromNvs()` | wywołane w `setup()` po `g_telemetryRing.init()` |
| `FIRMWARE_VERSION` | `v3.53.1` → `v3.54.0` |

Zapis do NVS następuje **wyłącznie przy zmianie stanu** (klik w aplikacji albo
`/spoolrt`), nigdy przy próbce telemetrii. Jeden bajt na kliknięcie.

### Pola w `/piec/status` (i `/api/lan/status`)

| pole | znaczenie |
|---|---|
| `spool_rt` | `1` = bufor włączony, `0` = wyłączony |
| `spool_pages` | ile stron zapisano na flash od startu centrali |
| `spool_spill` | ile próbek zapisano na flash |
| `spool_spill` / `spool_replay` | ile próbek wróciło do Firebase |

Payload urósł o ~90 B (limit `FB_JSON_CAP = 3072`, dotychczasowy JSON ~1,9 KB).

## Czego NIE zmieniono

- polityka zapisu: offline ≥ **120 s** albo ring ≥ **120** próbek,
- cooldown sprawdzania **30 s**,
- odtwarzanie po **10** próbek na raz,
- format stron Spool V2, quota i rezerwaty LittleFS,
- Telegram: `/spoolrt`, `/spoolrt TAK`, `/spoolrt STOP` i przyciski menu działają jak wcześniej,
- panel HTML (`Piec.html`) i reguły Firebase.

## Jak to sprawdzić po wgraniu

1. **Telegram:** `/spoolrt` → pierwsza linia `Spool G2 runtime: OFF`.
2. **Aplikacja:** Ustawienia → **Bufor offline** → przełącznik. Po kliknięciu
   aplikacja pokaże ACK, a po chwili status `Włączony`.
3. **Telegram ponownie:** `/spoolrt` → `ARMED`, w wiadomości po `TAK` dopisek
   `; NVS ON`.
4. **Restart centrali** (albo zanik prądu). Po starcie `/spoolrt` → nadal `ARMED`,
   a w logu DLOG `SPOOL-G2 zdarzenie=restored_from_nvs`.
5. **Wyłączenie:** przełącznik w aplikacji → `/spoolrt` → `OFF`, w logu
   `SPOOL-G2 zdarzenie=stopped`, w NVS zero.

## Wgranie (OTA, terminal PlatformIO)

```powershell
$project = "C:\SCIEZKA\DO\Cenatrala Pieca"
$package = "C:\SCIEZKA\DO\v3.54.0-offline-buffer-toggle"

foreach ($file in @("main_centrala.cpp", "SolarDailyArchive.h", "AutoIgnitionColdTimer.h", "MieszadloTrocinyAuto.h", "AlarmBuzzerProfiles.h")) {
  $current = Join-Path "$project\src" $file
  if (Test-Path $current) { Copy-Item -LiteralPath $current -Destination "$current.pre-v354.bak" }
}
Get-ChildItem "$package\*.h" | Copy-Item -Destination "$project\src" -Force
Copy-Item -LiteralPath "$package\main_centrala.cpp" -Destination "$project\src\main_centrala.cpp" -Force

Set-Location $project
pio run -e esp32-s3-ota
pio run -e esp32-s3-ota -t upload
```

`pio run` musi się udać **przed** `-t upload`. `platformio.ini` zostaje bez zmian
(paczka go nie zawiera). Hasła OTA nie wpisuj do czatu ani do repo.

## Czego nie sprawdzono

W tym środowisku **nie ma kompilatora ESP32**. Wykonane zamiast tego:

- kontrola balansu nawiasów całego pliku (lekser C++ z komentarzami, stringami
  i znakami) — wynik **identyczny** z v3.53.1 (różnica 0),
- każda wstawka asertowana liczbą wystąpień kotwicy (dokładnie 1),
- sprawdzenie typów: `putBool()` zwraca `size_t`, `getBool()` — `bool`,
  liczniki `g_spoolRuntime*` są `volatile uint32_t` i są zadeklarowane
  **przed** builderem statusu.

Jeśli build zgłosi błąd — nie wgrywaj, odeślij mi log.
