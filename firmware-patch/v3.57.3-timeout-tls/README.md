# Firmware v3.57.3 — timeout TLS uzbrajany przed każdym `fbApp.loop()`

Data: 2026-10-31
Baza: v3.57.2 (zawiera całość 3.57.2)
Plik do podmiany: **`main_centrala.cpp`** (jeden plik)
Pliki `.h`: **bez zmian**

---

## 1. Zdarzenie, które to wywołało

**31.10.2026** — centrala zamilkła o **04:09:00**, nadzór zrestartował ją ok. **04:14**.

Ostatnia linia w logu przed ciszą:

```
t=2026-10-31T04:09:00 boot=3 seq=1194 tag=FB-CMD lvl=ERR zdarzenie=get_error blad="TIMEOUT" t=5005ms
```

Po restarcie:

```
t=2026-10-31T04:14:11 boot=4 seq=11 tag=NADZOR lvl=WARN zdarzenie=raport_po_restarcie nr=1
t=CZAS NIEZNANY boot=4 seq=1  tag=BOOT lvl=INFO zdarzenie=restart powod=SW streak=0
```

Pamięć: `int_free` 76–85 kB (bez trendu), `hwm_tg=1020B`, brak panic,
`powod=SW` — restart programowy z nadzoru, nie crash.

## 2. Mechanizm — trzy fakty z dokumentacji

**a) `stop()` zeruje `handshake_timeout`.**
arduino-esp32, issue #6077 i PR #5945: *„clears `ssl_client` in several places, which sets `handshake_timeout` to 0"*. Po każdym `stop()` timeout TLS trzeba uzbroić ponownie.

**b) Domyślny timeout handshake to 120 000 ms.**
Potwierdzone w konstruktorze `NetworkClientSecure` (PR #24 w HexLions/RackTemp: *„default handshake_timeout is 120000ms … a handshake that stalls would stall the whole device for up to two minutes on a single send"*).

**c) Czas blokady zależy wyłącznie od ustawień klienta SSL.**
mobizt, autor `FirebaseClient`, dyskusja #203: *„There is no way to exit from this process to run your other code if you are using the WiFiClientSecure. The connection state, the blocking (waiting) time for connection and SSL handshake depends on the SSL client timeout setting."*

Dodatkowo issue #333 tej samej biblioteki opisuje ten sam wzorzec: po śmierci gniazda w połowie odpowiedzi `available()` zwraca 0, a wewnętrzna pętla odczytu nie kończy się — bo timer jest uzbrajany wewnątrz pętli, którą ma ograniczać.

## 3. Gdzie w naszym kodzie brakowało uzbrojenia

Każda ścieżka błędu `get()`/`set()` robi:

```cpp
fbAppReady = false;
fbFirebaseSocketStopSafe();   // fbSSLClient.stop() -> handshake_timeout = 0
FB_NET_FAIL();
```

Potem dwa miejsca wołały `fbApp.loop()` **bez** ponownego uzbrojenia:

| miejsce | kiedy | przed 3.57.3 |
|---|---|---|
| pompa na końcu iteracji `tgTaskFn` | co 100 ms | `fbApp.loop()` bez timeoutu |
| krótka ścieżka `fbInitialize()` (`g_fbAppInitialized`) | przy każdym `get`/`set` po błędzie | `fbApp.loop()` bez timeoutu |

Pętle oczekiwania (`fbCzekajImpl`) robiły to poprawnie — `applyFbSockTimeouts` co 10 ms — ale pompa nie.

Własny komentarz przy pompie ostrzega wprost: *„bez tego, po `stop()` na martwym gniazdku, `fbApp.loop()` może zablokować"*. Warunek `if (fbAppReady)` z tego komentarza nie został jednak użyty — kod sprawdza `g_fbAppInitialized`, które po `stop()` **zostaje `true`**.

## 4. Co zmieniono

Dwie identyczne wstawki — po dwie linie kodu:

```cpp
fbSSLClient.setHandshakeTimeout(4000);
applyFbSockTimeouts(fbSSLClient, 8);
```

przed `fbApp.loop()` w pompie i w krótkiej ścieżce `fbInitialize()`.
Plus znaczniki `FB_INIT / BEFORE`–`AFTER` wokół tego drugiego wywołania.

Zbilansowanie: **+2 `(` / +2 `)` / 0 klamer** — dokładnie dwie instrukcje.
Zero zmian w logice sterowania.

## 5. Czego świadomie NIE zmieniono

Rozważałem dodanie `fbAppReady` do warunku pompy (tak jak sugeruje komentarz). **Odrzucone po researchu**: mobizt wielokrotnie podkreśla, że `FirebaseApp::loop()` musi chodzić „swobodnie" — bez tego zadanie auth nie postępuje i `fbAppReady` nigdy nie wróciłoby do `true`, więc centrala straciłaby Firebase do najbliższego cyklu 15 s/7 s. Uzbrojenie timeoutu rozwiązuje problem bez odbierania bibliotece jej pętli.

Nie ruszono też: progów nadzoru, circuit breakera, telemetrii, Telegrama, OTA, historii, archiwum solarnego, formatów `/piec/status` i `/piec/cmd`, rytmu 15/7/2 s.

## 6. Ryzyko

| obszar | ocena |
|---|---|
| zmiana logiki | brak — tylko stan gniazda |
| koszt | dwa `setsockopt` na iterację (już tak jest w `fbCzekajImpl`) |
| regresja połączenia | timeout 4 s to ta sama wartość, której używa reszta pliku od v3.7.0.2 |
| co jeśli nie pomoże | raport nadzoru wskaże `FB_LOOP` / `FB_INIT` / `FB_CMD` — dalsza diagnoza bez zgadywania |

## 7. Czego to nie gwarantuje

Nie ma 100% pewności, że to była **jedyna** przyczyna. Gniazdo mogło też zawisnąć w samym `stop()`. Ale:
- poprawka adresuje udokumentowany mechanizm, nie hipotezę,
- jest nieszkodliwa, jeśli przyczyną było coś innego,
- a raport nadzoru z 3.57.2 i tak wskaże ścieżkę przy następnym zdarzeniu.

## 8. Pliki

- `main_centrala.cpp` — pełny plik do podmiany
- `main_centrala-v3.57.2-to-v3.57.3.patch` — sama różnica wobec 3.57.2
- `CZYTAJ-NAJPIERW.txt`, `README.md`, `SHA256SUMS.txt`
- `SterownikCO-v3.57.3-TIMEOUT-TLS.zip`
