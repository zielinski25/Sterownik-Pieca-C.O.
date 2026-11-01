# 239 — komendy docierają w ~2 s, potwierdzenia w ~1,5 s (0.31.57 + firmware 3.57.0)

Data: 2026-10-30. Aplikacja **0.31.57** (`versionCode 31057`). Firmware **3.57.0**.
To domknięcie 238: tam chodziło o *odczyt statusu*, tutaj o **drogę komendy w obie strony**.

## Co powiedziałeś

> „komendy które wydaję z aplikacji docierały [do centrali], aplikacja jest otwarta,
> i szybciej potwierdzenia przychodziły"

## Gdzie był czas (sprawdzone w kodzie)

| etap | ile trwało | plik / linia |
|---|---|---|
| app → `/piec/cmd` (PATCH) | ~0,2 s | `RtdbClient.kt` `sendCommand` |
| **centrala czyta `/piec/cmd`** | **do 7 s** | `main_centrala.cpp` `FB_CMD_INTERVAL_MS = 7000UL` |
| centrala wykonuje + pisze ACK | ~0,2 s | `fbSendCommandAck` |
| **app zauważa ACK** | **do 1 s** | `RtdbClient.kt` `delay(1000)` w pętli ACK |
| **razem** | **do ~8,4 s** | |

Dlaczego turbo nie pomagało: `fbTurboUntilMs` przesuwa się **po odczytaniu payloadu**
(`checkFirebaseCommands`), więc pierwsza komenda po ciszy i tak czekała pełne 7 s.

## Co zmieniłem — dwie strony

### Aplikacja 0.31.57

1. **Heartbeat `podglad_wl`** — od razu po wejściu na pierwszy plan i potem co 50 s
   (tylko na pierwszym planie). Po zejściu w tło `podglad_wyl`.
   Wstrzymuje się, jeśli w ostatnich 30 s szła prawdziwa komenda (żeby nie nadpisać
   payloadu w `/piec/cmd`). Nowe: `Rtdb.sendCommandQuiet()` — bez czekania na ACK.
2. **Sprawdzanie ACK co 300 ms** zamiast 1000 ms (`ACK_POLL_MS`).
3. (z 0.31.56) polling statusu 2 s.

### Firmware 3.57.0

| tryb | status | komendy | krok pętli |
|---|---|---|---|
| turbo (60 s po prawdziwej komendzie) | 1 s | 1 s | 1 s |
| **podgląd (okno 90 s od `podglad_wl`)** | **5 s** | **2 s** | 1 s |
| norma (nikt nie patrzy) | 15 s | 7 s | 3 s |

Nowe komendy `podglad_wl` / `podglad_wyl` nic nie przełączają — tylko przesuwają
okno podglądu. Heartbeat **nie** wrzuca centrali w pełne turbo: jeśli turbo uruchomił
właśnie ten odczyt, jest cofane. Okno 90 s > heartbeat 50 s, więc jeden zgubiony
heartbeat nic nie psuje; po zabiciu aplikacji tryb sam wygasa.

## Efekt

| | przed | po |
|---|---|---|
| klik → centrala odczytała komendę | do 7 s | **do 2 s** (w turbo 1 s) |
| klik → „potwierdzone" w aplikacji | **do ~8,4 s** | **~1,3–2,5 s** |
| świeżość statusu przy otwartej aplikacji | do 15 s | 5 s (turbo 1 s) |
| aplikacja w tle | 0 zapytań | 0 zapytań (alarmy 60 s) |

Koszt przy otwartej aplikacji: +~1800 odczytów `/piec/cmd` i +~720 zapisów
`/piec/status` na godzinę. Po zamknięciu — zero różnicy wobec 3.56.9.

## Zgodność

| aplikacja | firmware 3.56.x i starszy | firmware 3.57.0 |
|---|---|---|
| 0.31.57 | działa (heartbeat uruchamia turbo), ale w logu centrali leci `nieznana_komenda` co ~50 s | pełny efekt, zero śmieci w logu |
| 0.31.56 i starsza | jak dotąd | działa, bez heartbeatu (tylko szybszy polling statusu) |

Kolejność wgrywania nie ma znaczenia. Nic nie jest zablokowane wzajemnie.

## Jak sprawdzić

1. Firmware: Ustawienia → wersja firmware **v3.57.0**.
2. Otwórz aplikację, kliknij pompę → potwierdzenie w **~1–2 s**.
3. Log centrali: po otwarciu aplikacji **jedna** linia `tag=FB-PODGLAD ... aktywacja`,
   po zamknięciu `... deaktywacja`. Żadnych powtórzeń, żadnych `nieznana_komenda`.

## Cofnięcie

- Firmware: podmiana `main_centrala.cpp` z paczki 3.56.9 i ponowne wgranie.
- Aplikacja: `PODGLAD_HEARTBEAT_MS` w `core/AppModel.kt` — usunięcie `startPodgladHeartbeat()`
  z `onResume()` wyłącza heartbeat w całości; `ACK_POLL_MS` w `RtdbClient.kt` wraca do `1000L`.

## Paczki

| co | rozmiar | SHA-256 | Dysk |
|---|---|---|---|
| Aplikacja 0.31.57 (ZIP, 144 pliki) | 1 168 539 B | `09d18c07214a7e94302dcd6cc14a39183eb85c173a57205d6aa38db90ff666ed` | `1lETr9MeBaFRi3ZcNU4ztb6NixYHqvAB6` |
| Firmware 3.57.0 (ZIP, 20 plików) | 736 820 B | `f2339570b291f1f6e180854f7464cb55392d1674a206bd1bae9d2a328edbd743` | `1AbXNzIqokL2IYSQWgsXQN8RcgNrwIa8L` |

W repo: `android-sync/v0.31.57-podglad-panelu/`, `firmware-patch/v3.57.0-podglad-panelu/`.
