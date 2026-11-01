# Firmware 3.57.9 — limity sieci w sekundach (zawiera też poprawki 3.57.8)

Data: 2026-11-01 · baza: **3.57.8** · zastępuje **3.57.8**
Zmienia się **jeden plik**: `src/main_centrala.cpp`. Pliki `.h` bez zmian (`diff -rq`),
`src/secrets.h` nietknięty.

> **3.57.9 zawiera także poprawki z 3.57.8** (koniec „vv”, klucz NVS `nadzorWiekS`).
> 3.57.8 nie była wgrywana, więc numer przeskakuje o dwie wersje — wgranie 3.57.9
> daje oba zestawy.

---

## Skąd ta wersja

Raport nadzoru po 300 s ciszy pokazał dwa zawieszenia zadania sieciowego:

```
31.10 23:49   Ostatni znacznik: DESZCZ / BEFORE
01.11 03:35   Ostatni znacznik: FB_LOOP / BEFORE
```

Zawieszenie było rzadkie (~4 h odstępu), ale kosztowało 5 minut bez Telegrama
i Firebase przy każdym zdarzeniu. Hipoteza „kod ponawia próbę w pętli” **odpadła**:
`pobierzPrognozeDeszczu()` ma jedno `GET` i zero pętli sieciowych.

## Co było przyczyną

W arduino-esp32 2.0.x `setHandshakeTimeout()` mnoży argument przez 1000
(`WiFiClientSecure.cpp: sslclient->handshake_timeout = handshake_timeout * 1000`),
czyli przyjmuje **sekundy**. W firmware stało `4000` i `7000` — realny limit
handshake'u TLS to więc **4000 s (66 min)** i **7000 s (117 min)**, a nie 4 i 7 s.
Jedno pobranie mogło wisieć dłużej niż próg nadzoru (300 s). Dotyczyło to także
pompy `fbApp.loop()` na końcu `tgTaskFn` — czyli obu znaczników z 1.11.

Ta sama pomyłka była w `client.setTimeout()` (również sekundy) w sześciu miejscach.

## Co jest naprawione

| # | miejsce | przed | po |
|---|---|---|---|
| 1 | `setHandshakeTimeout(4000)` — 21 miejsc | 4 000 s | **8** |
| 2 | `setHandshakeTimeout(7000)` — `solarArchiveFetchGhi` | 7 000 s | **8** |
| 3 | `client.setTimeout(4000)` — DESZCZ, PROGNOZA | 4 000 s | **4** |
| 4 | `client.setTimeout(7000)` — `solarArchiveFetchGhi` | 7 000 s | **7** |
| 5 | `client.setTimeout(8000)` — `sprawdzNajnowszaWersjeGithub` | 8 000 s | **8** |
| 6 | `client.setTimeout(15000)` × 2 — OTA (firmware i panel) | 15 000 s | **15** |

**Razem 28 zmian wartości w 28 liniach** — nic więcej (brama sprawdza to automatycznie).

**Dlaczego 8 s, a nie 4:** przy limicie 4 s pobranie gubi się już przy wolnym TLS
4,5 s (całe okno 15 min bez danych). Przy 8 s taki przypadek przechodzi, a najgorszy
czas jednego `GET` to nadal **17 s** (TCP 5 s + TLS 8 s + odczyt 4 s) — 17× mniej
niż próg nadzoru.

## Bez zmian

Progi nadzoru (300 s / 3 restarty / 30 min), WDT, formaty JSON, telemetria,
Telegram, historia na flashu, archiwum solarne, wszystkie inne ustawienia.
`http.setTimeout(...)` zostaje w milisekundach — `HTTPClient` liczy w ms, to nie był błąd.
Dziewięć miejsc z już poprawnym `client.setTimeout(4)` też jest nietkniętych.

## Czego ta wersja nie robi

1. **Nie naprawia odpowiedzi, która „kapie”** (serwer wysyła 1 bajt co 3 s).
   Limit odczytu resetuje się przy każdym bajcie — potrzebny limit **całego**
   pobrania, a nie bezczynności. Osobny temat.
2. **Nie naprawia rotacji historii ani pliku `.rebuild`** — decyzja: osobno w **3.57.10**
   (gotowa lista w `narzedzia/limity-3.57.9/RAPORT-LIMITY-3.57.9.md`).

## Testy — wszystkie zielone

| zestaw | wynik |
|---|---|
| statyczne 3.57.8 (firmware + oba panele) | **43 / 43** |
| symulacja C++ (kanał A1 + bramki zapisu) | **80 / 80** |
| symulacja panelu w DOM (jsdom) | **26 / 26** |
| brama kroku 3 (diff tylko w timeoutach) | **4 / 4** |
| model czasu sieci (przed/po) | **23 / 23** |
| symulacja limitów historii | **38 / 38** + zgodność **19 / 19** |
| patch `-p0` na 3.57.8 | plik **bit w bit** jak w paczce |

Nie sprawdzono: **kompilacji** — sandbox nie ma toolchainu ESP32, to robi `pio run`.
Nie flashowano urządzenia.

## Zawartość paczki

| plik | co |
|---|---|
| `src/main_centrala.cpp` | **jedyny podmieniany plik** — 3.57.9 |
| `main_centrala-v3.57.8-to-v3.57.9.patch` | patch `-p0` z 3.57.8 (28 zmian + changelog) |
| `SHA256SUMS.txt` | sumy wszystkich plików |
| `CZYTAJ-NAJPIERW.txt` | 3 kroki + informacja o 3.57.8 |
| `WYNIKI/` | surowe wyjście wszystkich zestawów testów |
| `src/*.h` | nagłówki **bez zmian** — tylko dla testów, NIE do podmiany |

## Jak wgrać

1. Podmień `src/main_centrala.cpp` (kopia: `main_centrala.cpp.bak-3.57.8`).
2. `pio run -t upload` albo OTA z panelu.
3. Po restarcie w Telegramie: **`Wersja: v3.57.9`** (bez podwójnego `v`).
