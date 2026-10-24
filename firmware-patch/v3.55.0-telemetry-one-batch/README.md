# v3.55.0 — koniec z gubieniem próbek telemetrii (jedna paczka naraz)

Data: 2026-10-24
Baza: **v3.54.0** (`firmware-patch/v3.54.0-offline-buffer-toggle/`)
Plik: `main_centrala.cpp` — **46 387 linii**
**Firmware nie był kompilowany w tym środowisku** — build i OTA robi użytkownik.
Test hostowy, który nie wymaga Arduino, jest w `tests/host/` (wynik: **OK**).

---

## 0. ZANIM SKOMPILUJESZ — `src\secrets.h`

Kompilacja **zatrzyma się celowo**, jeśli w folderze `src\` (obok `main_centrala.cpp`)
nie ma pliku `secrets.h`. To nie jest błąd tej paczki — tak było od v3.50.0.
Zgłaszane komunikaty:

```
#error "Brak pliku secrets.h — skopiuj secrets.h.example jako secrets.h i wpisz hasla."
'SECRET_WIFI_AP_SSID' was not declared in this scope
'SECRET_FIREBASE_PASS' was not declared in this scope
'SECRET_CMD_TOKEN' was not declared in this scope
```

**Najbezpieczniejsza droga:** użyj pliku `secrets.h` z poprzedniej, działającej
kompilacji (ten sam, którym zbudowałeś v3.54.0). Nie wymyślaj nowego hasła
Firebase — jeśli będzie inne niż w działającej centrali, **stracisz zdalny dostęp**.

Znajdź go na dysku:

```powershell
Get-ChildItem C:\Users\Rafcio -Filter secrets.h -Recurse -ErrorAction SilentlyContinue |
  Select-Object FullName, Length, LastWriteTime
```

Skopiuj wynik do `src\` projektu (obok `main_centrala.cpp`):

```powershell
Copy-Item "TU_WKLEJ_ZNALEZIONA_SCIEZKA\secrets.h" "TU_PROJEKT\src\secrets.h" -Force
```

Gdyby pliku nigdzie nie było — skopiuj `secrets.h.example` jako `secrets.h`
i wpisz **te same wartości, które ma wgrane urządzenie** (hasło AP, hasło OTA,
hasło konta Firebase, token komend). Cztery wpisy, każdy w cudzysłowie.

---

## 1. Co było zepsute (dowod z danych, nie domysł)

W chmurze z 24.10 była **jedna** przerwa większa niż 90 s: **10:03:05 → 10:14:05 = 10,0 min**.
Dokładnie 10 próbek — czyli `TELEMETRY_BATCH_MAX_RECORDS`.

W kolejce sterownika `seq` skoczył **2777 → 2788**, czyli o **10 numerów**, a zapisane
zostały próbki **następne** (2788…). Stare pokolenie kodu robiło tak:

| krok | co się działo |
|---|---|
| 1 | wysyłka paczki, czekanie 6 s (`TELEMETRY_REQUEST_LIMIT_MS`) |
| 2 | brak odpowiedzi → `g_telemetryRequestActive` **zostaje `true`** |
| 3 | kolejny cykl: `g_fbTelemetryResult = AsyncResult{}` i budowa **następnej** paczki |
| 4 | spóźnione ACK z kroku 1 trafia do **wspólnego** `g_fbTelemetryResult` |
| 5 | rozliczenie uznaje, że „potwierdzona" jest paczka z kroku 3 |
| 6 | `commitPopN()` zdejmuje z kolejki **niewysłane** próbki — i te 10 nigdy nie trafia do chmury |

Skutek: próbki są w lokalnym archiwum centrali (`/hist_short.csv`), ale **nie ma ich w Firebase**.

---

## 2. Co zmieniono w v3.55.0 (cztery rzeczy)

### 2.1 `[v3.55.0 ONE-BATCH-RESEND]` — jedna paczka naraz

| element | przed | po |
|---|---|---|
| stan „paczka w toku" | brak | `g_telemetryPendingValid` + `PendingCount / PendingLastSeq / PendingBytes / PendingAttempts` |
| po timeoucie | budowana **nowa** paczka | ponowienie **tych samych bajtów** (bufor JSON zamrożony) |
| liczba prób | 1 | `TELEMETRY_BATCH_MAX_ATTEMPTS = 3` |
| odstęp ponowień | 60 s | `TELEMETRY_RETRY_INTERVAL_MS = 5000` ms |
| zdejmowanie z kolejki | zawsze po „sukcesie" | tylko gdy `peekAt(batchCount-1).sampleSeq == g_telemetryPendingLastSeq` |
| rozjazd tożsamości | — | `batch_identity_mismatch`, **kolejka nietknięta** |

Nowe funkcje: `telemetryPendingAbandon()` (linia **39850**), `telemetrySendPendingBatch()` (linia **41538**),
przebudowany `telemetryTrySendOneBatch()` (za nią).

### 2.2 `[v3.55.0 SPOOL-BACKLOG]` — zapis na flash także przy krótkiej zaległości

| stała | przed | po |
|---|---|---|
| `SPOOL_RUNTIME_RING_WATERMARK` | 120 próbek (~2 h) | **15 próbek** |
| `SPOOL_RUNTIME_BACKLOG_MAX_AGE_S` | — | **600 s** (najstarsza próbka starsza niż 10 min) |

Wcześniej kilkunastominutowa zaległość wysyłki nie uruchamiała zapisu do flash — próbki
siedziały tylko w RAM i znikały przy restarcie. Teraz wyzwalacz jest **ilościowy albo czasowy**
(czas liczony z różnicy znaczników w kolejce, nie z zegara ściennego — odporny na brak RTC).

Zrzut nie rusza paczki, na którą czeka wysyłka (`g_telemetryPendingValid` blokuje spool i replay),
bo `commitPopN()` w spoolu zdjąłby dokładnie te rekordy z tailu.

### 2.3 `[v3.55.0 CLOUD-MARK]` — „ostatnia próbka w chmurze"

Nowe pola w `/piec/status` i `/api/lan/status` (razem ~70 B, limit `FB_JSON_CAP = 3072`):

| pole | znaczenie |
|---|---|
| `tel_cloud_seq` | `seq` ostatniej próbki **potwierdzonej** przez Firebase (0 = od restartu nic) |
| `tel_cloud_ts` | jej czas unix |
| `tel_cloud_ago` | ile sekund temu potwierdzono (`-1` = brak danych) |
| `tel_ring` | ile próbek czeka w kolejce |
| `tel_pending` | `1` = paczka leci / czeka na ponowienie |

Ta sama informacja w Telegramie, komenda `/spool` (raport spoola):
`Last cloud sample: seq=… ts=… ago=…s` + `Ring now: … pending=…`.

### 2.4 Wersja

```
static const char* FIRMWARE_VERSION = "v3.55.0";     // linia 5485
```

Sprawdzenie w PowerShell:

```powershell
Select-String -Path .\main_centrala.cpp -Pattern 'static const char\* FIRMWARE_VERSION'
```

---

## 3. Test przed wgraniem (zrobiony tutaj, do powtórzenia u siebie)

```
cd tests\host
python3 run_tests.py
```

Skrypt wycina z **tego samego** `main_centrala.cpp`, który jedzie na urządzenie, funkcje
`telemetryPendingAbandon`, `telemetryFinalizeCompletedRequest`, `telemetrySendPendingBatch`
i `telemetryTrySendOneBatch`, podmienia wyłącznie wywołanie wysyłki na atrapę (`FB_NET_FAIL`,
`fbSSLClient.stop()` — atrapa zachowuje się **wrogo**: odpowiedzi przychodzą po timeoucie,
część ciał ginie) i kompiluje `g++ -std=c++11`.

Wynik (u mnie):

```
spokojna siec (5% timeout)       probek=2879  w chmurze=2879  ZGUBIONE=  0  (resends=429, abandoned=12)
normalna siec (12% timeout)      probek=2879  w chmurze=2879  ZGUBIONE=  0  (resends=667, abandoned=30)
slaba siec (30% timeout)         probek=2879  w chmurze=2879  ZGUBIONE=  0  (resends=1370, abandoned=120)
bardzo slaba siec (55% timeout)  probek=2879  w chmurze=2879  ZGUBIONE=  0  (resends=2451, abandoned=527)
zerwanie WiFi na 45 min          probek=2879  w chmurze=2879  ZGUBIONE=  0  (resends=652, abandoned=27)

Sprawdzen: 20, bledow: 0
WYNIK KONCOWY: OK
```

„ZGUBIONE = 0" w każdym scenariuszu to sedno: **nie ma próbki, która zostałaby zdjęta z kolejki
bez potwierdzenia w chmurze**. „abandoned" to porzucone *kopie* paczki (WiFi padło, próby
wyczerpane) — próbki zostają w kolejce i idą ponownie.

---

## 4. Po wgraniu — po czym poznać, że działa

1. `https://akwarium-367be-default-rtdb.europe-west1.firebasedatabase.app/piec/status.json`
   — są pola `tel_cloud_seq`, `tel_cloud_ago`, `tel_ring`, `tel_pending`.
2. `tel_cloud_ago` rośnie do ~60–70 s i spada (bo wysyłka jest co 60 s) — to jest **normalne**.
3. Gdy `tel_cloud_ago` przekracza ~600 s, a `tel_ring` rośnie — internet siadł; próbki
   czekają i **pójdą później** (także z flasha po restarcie).
4. Telegram `/spool` pokazuje `Last cloud sample: …` oraz `Ring now: … pending=…`.
5. W aplikacji Android (v0.31.33) na ekranie WYKRESY, pod paskiem statusu:
   **W CHMURZE** (`3 min temu`), **KOLEJKA** (`0 szt.`), **WYSYŁKA** (`BEZ ZALEGŁOŚCI`).

---

## 5. Pliki w paczce

| plik | zawartość |
|---|---|
| `main_centrala.cpp` | pełny plik po zmianach (46 387 linii) — **to wgrywasz** |
| `main_centrala-v3.54.0-to-v3.55.0.patch` | różnica wobec v3.54.0 (402 linie), do wglądu |
| `tests/host/run_tests.py` | uruchomienie testu hostowego |
| `tests/host/TelemetryBatchHostTest.cpp` | atrapy + scenariusze symulacji |
| `README.md` | ten plik |

Kompilacja i OTA: **po Twojej stronie** (Arduino IDE / PlatformIO). Ten plik nie był tu kompilowany —
nie mam w tym środowisku toolchainu ESP32.
