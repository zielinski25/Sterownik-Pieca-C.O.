# 248 — Firmware 3.57.3: timeout TLS przed każdym `fbApp.loop()`

Data: 2026-10-31
Paczka: `firmware-patch/v3.57.3-timeout-tls/`
ZIP: **`1EEbu2gIWfqm54qtzoXevMJGjm4ZLQgOC`** · 738 541 B ·
SHA-256 `e148f4f4ed086b61c2a19f6a3e9986838d340549829a48ee4aae0c56ed6876b9`

**Zastępuje 3.57.2** — zawiera całość 3.57.2 plus poprawkę.

---

## 1. Dowód z logu (31.10.2026)

```
t=2026-10-31T04:06:21 boot=3 seq=1190 tag=HEALTH  heap=77752B  hwm_tg=1020B
t=2026-10-31T04:07:32 boot=3 seq=1193 tag=STATUS  piec=17.0C pompa=OFF
t=2026-10-31T04:09:00 boot=3 seq=1194 tag=FB-CMD lvl=ERR zdarzenie=get_error blad="TIMEOUT" t=5005ms
                                 ^^^ OSTATNIA LINIA PRZED CISZĄ
t=2026-10-31T04:14:11 boot=4 seq=11   tag=NADZOR lvl=WARN zdarzenie=raport_po_restarcie nr=1
t=CZAS NIEZNANY   boot=4 seq=1    tag=BOOT lvl=INFO zdarzenie=restart powod=SW streak=0
```

Wykluczone: pamięć (76–85 kB stabilnie), stos (`hwm_tg` 1020 B), panic/WDT
(`powod=SW`, `streak=0`), WiFi (DNS `ok=1 dt_ms=40`, `wifi=1`).

**Dlaczego brakuje `PING`/`HEALTH` z 04:11:21** (i wpisu `NADZOR zdarzenie=zawieszenie`):
`zapiszLog()` odkłada linię do bufora, a na LittleFS zrzuca ją `logFlushDoFS()`
wołane **wyłącznie z `tgTaskFn` co 5 s** (`LOG_FLUSH_INTERVAL_MS = 5000`).
Zadanie padło → zrzut ustał → wszystko po 04:09:00 zostało w RAM i przepadło.
Linia `FB-CMD` z 04:09:00 **jest**, bo zrzut zdążył się wykonać — potem stanął.

**Wniosek dla przyszłych wersji:** dane diagnostyczne trzeba trzymać w NVS
albo wysyłać Telegramem, nie tylko w logu. Tak działa raport nadzoru z 3.57.2.

## 2. Mechanizm (z dokumentacji, nie z hipotezy)

| fakt | źródło |
|---|---|
| `stop()` zeruje `handshake_timeout` — trzeba uzbrajać przed każdym połączeniem | arduino-esp32 issue #6077, PR #5945 |
| domyślny `handshake_timeout` = **120 000 ms** | konstruktor `NetworkClientSecure` (PR HexLions/RackTemp #24) |
| czas blokady `connect()`/handshake zależy **wyłącznie** od ustawień klienta SSL | mobizt (autor FirebaseClient), dyskusja #203 |
| po śmierci gniazda `available()` zwraca 0 i wewnętrzna pętla odczytu nie kończy się | mobizt/FirebaseClient issue #333 |

Sekwencja w naszym kodzie:

```cpp
// ścieżka błędu get()/set()
fbAppReady = false;
fbFirebaseSocketStopSafe();   // fbSSLClient.stop()  -> handshake_timeout = 0
FB_NET_FAIL();
```

Potem dwa miejsca wołały `fbApp.loop()` **bez ponownego uzbrojenia**:

| miejsce | wywołanie | stan przed 3.57.3 |
|---|---|---|
| pompa końca iteracji `tgTaskFn` | co 100 ms | bez timeoutu |
| krótka ścieżka `fbInitialize()` (`g_fbAppInitialized`) | przy każdym `get/set` po błędzie | bez timeoutu |

Pętle oczekiwania (`fbCzekajImpl`) robiły to poprawnie — `applyFbSockTimeouts`
co 10 ms. Pompa nie.

## 3. Zmiana

Dwie wstawki, po dwie linie kodu:

```cpp
fbSSLClient.setHandshakeTimeout(4000);
applyFbSockTimeouts(fbSSLClient, 8);
```

przed `fbApp.loop()` w pompie i w krótkiej ścieżce `fbInitialize()`,
plus znaczniki `FB_INIT / BEFORE`–`AFTER` wokół drugiego wywołania.

| kontrola | wynik |
|---|---|
| bilans nawiasów vs 3.57.2 | **+2 `(` / +2 `)` / 0 klamer** — dokładnie dwie instrukcje |
| deklaracje przed użyciem | `applyFbSockTimeouts` (32916), `diagIdBiezacegoTasku` (32959) — oba przed `fbInitialize()` (~42200) |
| rozmiar ZIP na Dysku | 738 541 B — zgodny z lokalnym |

## 4. Odrzucone po researchu

**Warunek `fbAppReady` w pompie** (tak sugerował własny komentarz w kodzie).
mobizt wielokrotnie: `FirebaseApp::loop()` musi chodzić swobodnie — bez tego
zadanie auth nie postępuje, `fbAppReady` nigdy nie wraca do `true` i centrala
traci Firebase do najbliższego cyklu 15 s / 7 s. Uzbrojenie timeoutu rozwiązuje
problem bez odbierania bibliotece pętli.

## 5. Bez zmian

Progi nadzoru (5 min / 3 min / 3 restarty / 30 min), circuit breaker, telemetria,
Telegram, OTA, historia, archiwum solarne, format `/piec/status` i `/piec/cmd`,
rytm 15 s / 7 s / 2 s, logika pieca. Aplikacja **0.31.63 bez zmian**.

## 6. Otwarte

1. Czy 3.57.3 wgrane — wersja w Ustawieniach: `v3.57.3`.
2. Pola zegara RTC w `/piec/status` — **odłożone przez użytkownika**;
   arkusz „Data i czas" nadal pokaże „oczekiwanie na odczyt".
3. `dym=231` przed restartem → `dym=0` po — do obserwacji, nie teraz.
4. Czy 0.31.63 zbudowało się i czy panel nie wraca na pulpit.
