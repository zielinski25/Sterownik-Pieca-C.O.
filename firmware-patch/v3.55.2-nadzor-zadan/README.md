# v3.55.2 — NADZÓR ZADAŃ (24.10.2026)

**Cel jednym zdaniem:** gdy centrala zamilknie w Telegramie i w aplikacji,
ma się sama podnieść, zamiast czekać na wyciągnięcie wtyczki.

---

## 1. Co pokazał zrzut pamięci (coredump) z 24.10.2026

W zrzucie `dump_seq0000000054_boot2.bin` jest zapisana jedna, konkretna przyczyna:

```
Task watchdog got triggered ... IDLE1 (CPU 1)
```

Po ludzku: **procesor ma dwa „mózgi" (rdzenie). Rdzeń numer 1 przestał na chwilę
oddychać** — przez ponad 15 sekund nie oddał sterowania zadaniu bezczynności
(`IDLE1`). Sprzętowy strażnik (watchdog) uznał to za zawieszenie i zrestartował
płytkę.

W zrzucie jest 7 wątków. Jeden z nich to `loopTask` — główna pętla urządzenia —
i on **nie był tym, który się zawiesił**. Wskazuje na to adres powrotu
`vPortExitCritical+86`, czyli miejsce, w którym główna pętla stała
**z wyłączonymi przerwaniami**. Kiedy przerwania są wyłączone, żadne inne
zadanie nie może biec — także to, które pilnuje WiFi i Telegrama.

Wniosek praktyczny: **nie da się wskazać jednej linii kodu**, która się zepsuła.
Zrzut pokazuje skutek (zagłodzenie rdzenia 1), a nie winowajcę. Dlatego ta
wersja nie „naprawia jednej linii" — dodaje **siatkę bezpieczeństwa** i lepszy
zapis miejsca, w którym urządzenie padnie następnym razem.

### Dlaczego nie wskazano linii kodu — ważne

Do zrzutu dołączony był plik `firmware.elf`, ale **to nie był plik tej
centrali**. Dowód: zrzut ma skrót `560f2edb1`, a przesłany `firmware.elf`
ma `e2787f70d`. Dodatkowo w tym ELF-ie jest opis
`arduino-lib-builder`, wersja `v5.5.4`, data 31.03.2026 — to biblioteka
Arduino, nie Wasz program.

**Na przyszłość:** przy wysyłaniu zrzutu pamięci trzeba wysłać **ten sam**
`firmware.elf`, który był wgrany na płytkę — leży w katalogu
`.pio\build\<nazwa_środowiska>\firmware.elf` **po kompilacji tej konkretnej
wersji**. Bez tego żaden program nie dopasuje zrzutu do kodu.

---

## 2. Co zmieniono w tej wersji

### 2.1 Nadzór zadań (`nadzorZadanSieciowych`)

Główna pętla urządzenia co obieg sprawdza, czy dwa zadania sieciowe dają znak
życia:

| Zadanie | Co robi | Znak życia |
|---|---|---|
| `tgTask` | wysyła do Telegrama, gada z Firebase | `g_tgTaskHeartbeatMs` |
| `tgPollTask` | odbiera komendy z Telegrama (`/status`, `/archiwum`…) | `g_tgPollHeartbeatMs` |

Jeśli któreś milczy dłużej niż **5 minut** → restart płytki, a po starcie
przychodzi na Telegram wiadomość:

> Wrocilem po automatycznym restarcie.
> Zadanie "tgTask (Telegram i Firebase)" nie dalam znaku zycia przez 612 s.
> Restart zrobil nadzor w firmware — nie bylo potrzebne wyciaganie wtyczki.
> To juz restart numer 1 w ciagu 30 minut.

Zabezpieczenia, żeby nadzór sam nie robił szkody:

| Sytuacja | Zachowanie |
|---|---|
| pierwsze 3 minuty po starcie | nadzór śpi (zadania jeszcze się rozkręcają) |
| trwa OTA albo test regresyjny (`/finalmega`, `/stabilitymega`, …) | nadzór śpi |
| oba zadania jeszcze nie wystartowały (znak = 0) | nadzór śpi |
| 3 restarty pod rząd | nadzór **przestaje restartować**, tylko zapisuje w log |
| 30 minut nieprzerwanej pracy | licznik restartów zerowany (problem minął) |

### 2.2 Ślad „na którym wywołaniu stanęło" (`diagSladWifiLoop`)

Główna pętla WiFi wykonuje po kolei kilka rzeczy. Teraz po każdej z nich
zapisuje numer kroku do pamięci podtrzymywanej przy resecie. Po następnym
zawieszeniu raport przyniesie linię typu:

```
LOOP op=WIFI_LOOP etap=START detail=3
```

i **od razu wiadomo, na którym wywołaniu stanęło** — bez dopasowywania pliku
ELF do zrzutu.

| `detail` | Ostatnie wywołanie przed zawieszeniem |
|---|---|
| 1 | `ArduinoOTA.handle()` |
| 2 | `flushWsQueue()` |
| 3 | blok `wsTerminal.cleanupClients()` (co 30 s) |
| 4 | `wifiDnsServer.processNextRequest()` |
| 5 | `wifiPolaczSTA_Krok()` |
| 6 | koniec `wifiLoop()` — czyli wyszło z niej poprawnie |

Zapis jest celowo „lekki" — bez pomiaru wolnej pamięci, żeby nie obciążać
pętli głównej.

### 2.3 Czego ta wersja **nie** naprawia

Nadzór sam siedzi w głównej pętli. Jeśli zawiesi się **dokładnie ta pętla**
(tak jak 24.10), nadzór nie zdąży zadziałać — wtedy urządzenie podnosi
sprzętowy watchdog, tak jak dotąd. Ta wersja dokłada drogę wyjścia na wypadek,
gdy główna pętla żyje, a **sieć zamilkła** — bo wtedy nie ma żadnej innej drogi
powrotu poza wtyczką.

### 2.4 Czego nie ruszono

Telemetria, Firebase, OTA i rollback, historia CSV, archiwum solarne, panel WWW,
serwo, pompa, alarmy — **bez zmian**. Format i kolejność kolumn CSV — bez zmian.

---

## 3. Testy (hostowe, bez ESP32)

```
tests/host/run_nadzor_tests.py      34 sprawdzen, 0 bledow   <- NOWY
tests/host/run_history_tests.py     48 sprawdzen, 0 bledow   <- bez zmian
tests/host/run_tests.py             20 sprawdzen, 0 bledow   <- bez zmian
```

Test nadzoru sprawdza m.in.: że zdrowa centrala nie jest restartowana, że próg
działa co do milisekundy (299 999 ms = spokój, 300 000 ms = restart), że
wskazywane jest właściwe zadanie, że nadzór śpi w rozbiegu i podczas OTA, że po
3 restartach przestaje restartować, oraz że zawinięcie licznika `millis()`
(po ~49 dniach pracy) nie robi fałszywego alarmu.

Test pobiera funkcję decyzyjną **wprost z `main_centrala.cpp`** — nie ma
drugiej, „testowej" kopii logiki, która mogłaby się rozjechać z produktem.

---

## 4. Co jest w paczce

```
main_centrala.cpp                        <- pełny plik, gotowy do podmiany
main_centrala-v3.55.1-to-v3.55.2.patch   <- alternatywa: patch -p1
secrets.h.example                        <- wzor (twoje hasla zostaja lokalnie)
tests/host/NadzorHostTest.cpp            <- test nadzoru
tests/host/run_nadzor_tests.py
tests/host/HistoryStreamHostTest.cpp     <- test historii (v3.55.1)
tests/host/run_history_tests.py
tests/host/TelemetryBatchHostTest.cpp    <- test telemetrii (v3.55.0)
tests/host/run_tests.py
README.md
SHA256SUMS.txt
```

## 5. Jak wgrać (robisz to Ty)

1. Podmień `src\main_centrala.cpp` w swoim projekcie na plik z tej paczki.
2. `secrets.h` w `src\` **musi zostać** — bez niego kompilacja się nie zacznie.
3. Zbuduj i wgraj:

```powershell
pio run -t upload
```

4. Po restarcie: sprawdź `PiecCO_AP` i wyślij na Telegram `/status`.

## 6. Po czym poznasz, że działa

- `/status` odpowiada i pokazuje `v3.55.2`.
- W logu po starcie jest linia `FIRMWARE_VERSION` z `v3.55.2`.
- Gdyby centrala kiedyś zamilkła na dłużej niż 5 minut i sama wstała —
  dostaniesz na Telegram wiadomość „Wrocilem po automatycznym restarcie".
  **To nie jest awaria — to właśnie ta funkcja.**

## 7. Historia wersji

- v3.55.0 — jedna paczka telemetrii naraz (24.10.2026, Release na GitHubie)
- v3.55.1 — bezpieczny start: historia czytana strumieniowo (24.10.2026)
- **v3.55.2 — nadzór zadań: samo-powrót po zawieszeniu + ślad kroku WiFi (24.10.2026)**
