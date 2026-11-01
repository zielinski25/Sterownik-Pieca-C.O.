# 240 — ustalone: 2 s, tylko komendy i potwierdzenia (firmware 3.57.1)

Data: 2026-10-30. Decyzja użytkownika: **„2s"** + wcześniejsze: **„chodzi o komendy i potwierdzenie"**.

Aplikacja bez zmian: **0.31.57** (`versionCode 31057`).
Firmware: **3.57.1** — poprawka po 3.57.0 (tam podgląd przyspieszał też status, teraz nie).

## Co dokładnie zostało ustalone

| pytanie | decyzja |
|---|---|
| tempo odczytu komend przy otwartej aplikacji | **2 s** |
| czy przyspieszać status / temperatury | **NIE** — zostaje 15 s |
| aplikacja: sprawdzanie ACK | 0,3 s (już w 0.31.57) |
| aplikacja: heartbeat „patrzę" | co 50 s (już w 0.31.57) |
| odświeżanie ekranu w aplikacji (0.31.56, 2 s) | zostaje, nie ruszane |

## Różnica 3.57.0 → 3.57.1 (jeden plik, 76 linii diffu)

| element | 3.57.0 | **3.57.1** |
|---|---|---|
| odczyt `/piec/cmd` w podglądzie | 2 s | 2 s |
| wysyłka `/piec/status` w podglądzie | 5 s | **15 s (jak norma)** |
| stała `FB_PODGLAD_SEND_INTERVAL_MS` | istniała | **usunięta** |
| reszta (heartbeat, okno 90 s, cofanie turbo, turbo 1 s/1 s po komendzie) | — | bez zmian |

## Stan końcowy taktowania

| tryb | status | komendy | krok pętli |
|---|---|---|---|
| turbo (60 s po prawdziwej komendzie) | 1 s | 1 s | 1 s |
| **podgląd (okno 90 s)** | **15 s** | **2 s** | 1 s |
| norma | 15 s | 7 s | 3 s |

## Efekt

| | przed (0.31.55 + 3.56.x) | po (0.31.57 + 3.57.1) |
|---|---|---|
| klik → centrala odczytała polecenie | do 7 s | **do 2 s** (w turbo 1 s) |
| klik → „potwierdzone" | do ~8 s | **~1,3–2,3 s** |
| świeżość temperatur | do 15 s | **do 15 s (bez zmian)** |

## Koszt

**Tylko odczyty**: ~1800 dodatkowych zapytań `/piec/cmd` na godzinę przy otwartej
aplikacji (odpowiedź `null`, kilkanaście bajtów). **Zero dodatkowych zapisów statusu**
— to jest różnica wobec 3.57.0. Po zamknięciu aplikacji (lub po 90 s bez heartbeatu)
powrót do 7 s.

## Wgranie

1. `main_centrala.cpp` z paczki → `C:\Users\Rafcio\Documents\PlatformIO\sterownik-co-esp32\src\main_centrala.cpp`
2. PlatformIO → Upload (`pio run -t upload`)
3. Weryfikacja: Ustawienia → firmware **v3.57.1**; klik w pompę → potwierdzenie ~1–2 s.

Aplikacja: **bez zmian** — zostaje 0.31.57 (ta sama paczka co w dokumencie 239).

## Cofnięcie

Podmiana `main_centrala.cpp` na wersję z paczki **3.56.9** i ponowne wgranie —
aplikacja nie wymaga wtedy żadnych zmian (koszt: wpisy `nieznana_komenda` co ~50 s
w logu centrali).

## Paczki

| co | rozmiar | SHA-256 | Dysk |
|---|---|---|---|
| Firmware 3.57.1 (ZIP, 20 plików) | 735 236 B | `a4923803f221d6e27d69a4c842ee9abd47848da71df76dff3decd09433589c57` | `1xVg04lM8nHaHHYEAJW4LyjwnDKbr4n8B` |
| Aplikacja 0.31.57 (ZIP, 144 pliki) — bez zmian | 1 168 539 B | `09d18c07214a7e94302dcd6cc14a39183eb85c173a57205d6aa38db90ff666ed` | `1lETr9MeBaFRi3ZcNU4ztb6NixYHqvAB6` |

W repo: `firmware-patch/v3.57.1-podglad-komendy/`, `android-sync/v0.31.57-podglad-panelu/`.
**3.57.0 nie wgrywaj** — zastąpiona przez 3.57.1.
