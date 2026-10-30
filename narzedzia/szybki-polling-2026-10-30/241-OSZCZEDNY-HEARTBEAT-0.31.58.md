# 241 — 0.31.58: ekran co 8 s, heartbeat gaśnie po 10 min ciszy

Data: 2026-10-30. Decyzje użytkownika: **„1 Odświeżenia 8s wróć · 2 na 90 · 3 zgadzam się"**.

Aplikacja: **0.31.58** (`versionCode 31058`).
Firmware: **bez zmian — zostaje 3.57.1** (okno podglądu już wynosi 90 s).

## Decyzja → kod

| # | decyzja | gdzie | co dokładnie |
|---|---|---|---|
| 1 | odświeżanie ekranu **8 s** | `core/AppModel.kt` | `FOREGROUND_POLL_MS = 8_000L` (było 2 000) |
| 2 | okno podglądu **90 s** | firmware 3.57.1 | bez zmian: `FB_PODGLAD_WINDOW_MS = 90000UL` |
| 3 | heartbeat **gaśnie po 10 min bez dotknięcia** | `AppModel.kt` + `MainActivity.kt` | `PODGLAD_IDLE_STOP_MS = 10 min`; `dispatchTouchEvent` → `model.noteInteraction()` |

## Jak działa gaszenie (0.31.58)

1. `MainActivity.dispatchTouchEvent(ACTION_DOWN)` woła `noteInteraction()` przy **każdym**
   dotknięciu (także przewijanie i tło, nie tylko przyciski).
2. Heartbeat sprawdza co 50 s: jeśli od ostatniego dotknięcia minęło **> 10 min** →
   wysyła **jedno** `podglad_wyl` i przestaje (dalej sprawdza co 15 s, czy wrócił dotyk).
3. Pierwsze dotknięcie → `noteInteraction()` widzi, że podgląd stoi → **natychmiast**
   restartuje pętlę, a jej pierwsza iteracja wysyła `podglad_wl` bez czekania.
4. Każda prawdziwa komenda też liczy się jako „obecność" (`lastUserCommandMs`
   ustawia również `lastInteractionMs`) — wysłanie polecenia nie zostawia
   podglądu wygaszonego.

## Koszt danych (LTE) — przed/po

| pozycja | 0.31.57 | **0.31.58** |
|---|---|---|
| odczyt statusu przy otwartej aplikacji | 2 s ≈ 3,6 MB/h | **8 s ≈ 0,9 MB/h** |
| heartbeat „patrzę" | 50 s, stale | 50 s **tylko przy dotykaniu**; po 10 min ciszy **0** |
| sprawdzanie ACK | 0,3 s, tylko w trakcie komendy | bez zmian |
| **razem, godzina patrzenia** | ~3,7 MB | **~1,0 MB** |
| **razem, telefon leży 8 h** | ~29 MB | **~7 MB** (a po 10 min ciszy już zero) |

## Czego to NIE psuje

- Szybkość komend: bez zmian — nadal heartbeat + firmware 3.57.1 (odczyt 2 s)
 + ACK co 300 ms → **~1,3–2,3 s**.
- Status w centrali: nadal 15 s (3.57.1 nie przyspiesza statusu).
- Tło: nadal 0 zapytań z UI; alarmy 60 s.

## Efekt uboczny, o którym warto wiedzieć

Po 10 min bez dotknięcia pierwszy klik **i tak** będzie szybki, bo dotknięcie
ekranu wznawia heartbeat **zanim** komenda zostanie wysłana (dotknięcie →
`ACTION_DOWN` → heartbeat startuje; komenda idzie po `ACTION_UP`/kliknięciu).
Jeśli jednak klikniesz „z zimna" w 1. sekundzie po 10 min ciszy, centrala może
jeszcze czytać co 7 s — wtedy potwierdzenie wróci do ~7 s. To jednorazowe.

## Wgranie

- Aplikacja: ZIP → build (jedno polecenie jak w 237) → APK na pulpit.
- Firmware: **nic nie trzeba** — 3.57.1 z dokumentu 240 już ma okno 90 s.
  Jeśli jeszcze nie wgrałeś 3.57.1, zrób to; bez niej heartbeat nie przyspiesza
  odczytu komend.

## Cofnięcie

`FOREGROUND_POLL_MS` → `2_000L` (szybszy ekran), `PODGLAD_IDLE_STOP_MS` → bardzo
duża wartość (heartbeat bez gaszenia). Nic więcej nie zależy od tych stałych.

## Paczka

| co | rozmiar | SHA-256 | Dysk |
|---|---|---|---|
| Aplikacja 0.31.58 (ZIP, 144 pliki) | 1 169 766 B | `5c0ddb6ec729cce7503d8ac85fac581118bd585614dc2d57190552eea7aa3c55` | `1zQr3CBkufImz_O8ZOWxnVJKOfxpa53-d` |
| Firmware 3.57.1 (bez zmian) | 735 236 B | `a4923803f221d6e27d69a4c842ee9abd47848da71df76dff3decd09433589c57` | `1xVg04lM8nHaHHYEAJW4LyjwnDKbr4n8B` |

W repo: `android-sync/v0.31.58-oszczedny-heartbeat/`.
