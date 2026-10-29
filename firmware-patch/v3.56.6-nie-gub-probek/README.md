# v3.56.6 — koniec z gubieniem 10 próbek telemetrii

Data: 2026-10-29
Zmiana: `FIRMWARE_VERSION v3.56.5 -> v3.56.6` (PATCH — tylko naprawa, żadnych nowych funkcji)

---

## 1. Co było nie tak (dowód z danych, nie domysł)

Eksport bazy Firebase z 29.10 + archiwum centrali `hist_short.csv` pokazały:

| | 07:16 | 07:17 – 07:26 | 07:27 |
|---|---|---|---|
| **Centrala** (`/hist_short.csv`) | jest | **jest wszystkie 10** | jest |
| **Chmura** (Firebase) | jest | **BRAK 10 próbek** | jest |

- numer próbki `seq`: **6542 → 6553** — brakuje dokładnie 10 numerów,
- licznik pracy `mono`: 392 539 354 → 393 199 386 = **11 minut bez przerwy**,
- identyfikator rozruchu `4A008BE708AE217C` **ten sam** → centrala się NIE restartowała.

Czyli centrala mierzyła i zapisywała dalej, ale 10 próbek nie dotarło do chmury
i **nigdy nie zostały ponowione**.

Skala w całym eksporcie 01.10 – 29.10:

- próbek w chmurze: **40 789**
- brakujących minut: **596** = **1,44 %** osi czasu
- luk dłuższych niż 3 minuty: **46**

Wszystkie większe luki są wielokrotnością paczki wysyłkowej (10 próbek):
30, 20, 11, 10, 10, 10, 10 minut.

## 2. Dlaczego v3.55.0 tego nie naprawiła

v3.55.0 dodała ponawianie tych samych bajtów (`TELEMETRY_BATCH_MAX_ATTEMPTS = 3`,
timeout 6 s) — czyli czeka 18 sekund. To działa. Problem jest **po** wyczerpaniu prób:

1. Nieudana wysyłka ustawia `fbAppReady = false` i zamyka SSL.
2. Po 3 próbach paczka jest porzucana (`telemetryPendingAbandon`).
3. Odbudowa sesji była **warunkowa**:
   `if (g_spoolRuntimeArmed || g_telemetryPendingValid) fbInitialize();`
   Po porzuceniu **oba warunki są fałszywe** → sesja zostaje martwa,
   a zaległe próbki czekają w kolejce na przypadkową odbudowę.
4. Bufor offline (Spool) miał to łapać, ale jego progi to **15 próbek**
   i **600 s**. Paczka to dokładnie 10 próbek = 540 s różnicy znaczników,
   więc **wpadała pod oba progi jednocześnie** i nie była zapisywana.

## 3. Co zmieniono — trzy niezależne zabezpieczenia

Każde działa samo; razem się nie wykluczają.

### 3.1. Sesja Firebase odbudowywana ZAWSZE
```cpp
// BYŁO:
if (g_spoolRuntimeArmed || g_telemetryPendingValid) fbInitialize();
// JEST:
fbInitialize();
```
Po nieudanej wysyłce centrala od razu odbudowuje sesję i ponawia paczkę,
nie czekając na szczęśliwy przypadek.

### 3.2. Porzucona paczka idzie na flash
Przed `telemetryPendingAbandon("attempts_exhausted")` ustawiamy
`g_spoolRuntimeForceSpill = true`. Funkcja `spoolRuntimeMaybeSpill()` omija wtedy:
- próg ilościowy,
- próg wieku,
- cooldown 30 s.

Te 10 próbek trafia na LittleFS **natychmiast**, zanim kolejka je nadpisze,
a potem wraca do chmury przez zwykły replay (`spool_replay`).

### 3.3. Progi bufora obniżone
| Parametr | Było | Jest |
|---|---|---|
| `SPOOL_RUNTIME_RING_WATERMARK` | 15 próbek | **2 próbki** |
| `SPOOL_RUNTIME_BACKLOG_MAX_AGE_S` | 600 s | **180 s** |

## 4. Czego NIE zmieniono

- format JSON i ścieżki RTDB — chmura i aplikacja czytają to samo,
- interwał próbki 60 s,
- `TELEMETRY_BATCH_MAX_RECORDS = 10`, `MAX_ATTEMPTS = 3`, timeout 6 s,
- format stron Spool V2, quota, rezerwaty LittleFS,
- wszystkie trasy HTTP, Telegram, panel, historia, solar, alarmy.

Plik `.h` **nie został zmieniony ani jeden** — tylko `main_centrala.cpp`.

---

## 5. Jak wgrać

### Wariant A — masz dostęp do pieca (USB)

1. W projekcie `C:\Users\Rafcio\Documents\PlatformIO\sterownik-co-esp32`
   podmień **jeden plik**: `main_centrala.cpp` (ten z tej paczki).
   Nic więcej — nagłówki zostają.
2. Sprawdź, że w `secrets.h` **nie ma** słowa `ZMIEN-MNIE`.
3. Zbuduj i wgraj:
   ```
   pio run -t upload
   ```
4. Sprawdź: `/version` na Telegramie → `Obecna wersja: v3.56.6`.

### Wariant B — jesteś zdalnie (OTA przez Telegram)

1. Zbuduj u siebie **sam plik firmware**, bez wgrywania:
   ```
   pio run
   ```
2. Plik wynikowy: `.pio\build\esp32-s3-n16r8\firmware.bin`
   Zapisz jego rozmiar — piec sprawdza rozmiar przed wgraniem.
3. Wgraj go do wydania na GitHubie (tag bez litery v):
   ```
   gh release upload 3.56.6 ".pio\build\esp32-s3-n16r8\firmware.bin" --clobber
   ```
   Jeżeli wydania jeszcze nie ma:
   ```
   gh release create 3.56.6 ".pio\build\esp32-s3-n16r8\firmware.bin" --title "3.56.6"
   ```
4. Na Telegramie wyślij `/update`.
5. Po restarcie `/version` → `v3.56.6`.

**Warunek konieczny w obu wariantach:** w `secrets.h` nie może być `ZMIEN-MNIE`.
Jeżeli jest, piec ma zaślepki zamiast haseł i po wgraniu straci Telegram —
najpierw uzupełnij sekrety.

## 6. Po czym poznać, że naprawa działa

1. `/version` → `v3.56.6`.
2. `/spoolrt` → pierwsza linia `Spool G2 runtime: OFF` lub `ON`
   (informacyjnie; naprawa 3.2 działa tylko przy `ON`).
   Żeby włączyć: `/spoolrt TAK`. W aplikacji: Ustawienia → **Bufor offline**.
3. Po 24 godzinach wejdź w eksport RTDB albo w aplikacji w WYKRESY → 24 H
   i sprawdź, czy nie ma luk dłuższych niż 2 minuty.
   Przed naprawą było ich w tym okresie średnio 1–2 na dobę.
4. `/status` → pole `spool_replay` rośnie = próbki wracają z flasha do chmury.

## 7. Jeżeli coś pójdzie nie tak

Wróć do 3.56.5 tym samym sposobem (tag `3.56.5` ma już `firmware.bin`).
Ta wersja nie zmienia formatu zapisu, więc powrót jest bezpieczny —
historia, ustawienia i sekrety zostają.

---

## 8. Zawartość paczki

| Plik | Co to |
|---|---|
| `main_centrala.cpp` | **jedyne zmienione źródło** |
| `main_centrala-v3.56.5-to-v3.56.6.patch` | różnica, do wglądu (102 linie) |
| `*.h` (14 plików) | bez zmian — dla kompletności projektu |
| `secrets.h.example` | wzór, **bez prawdziwych haseł** |
| `SHA256SUMS.txt` | suma każdego pliku |

W paczce **nie ma** `secrets.h` z hasłami. Sprawdzone.
