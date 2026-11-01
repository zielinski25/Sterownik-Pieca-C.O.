# Firmware 3.57.8 — raport nadzoru mówi wreszcie kiedy i gdzie

Data: 2026-11-01 · baza: **3.57.7** · zastępuje **3.57.7**
Zmienia się **jeden plik**: `src/main_centrala.cpp`. Pliki `.h` bez zmian (`diff -rq`), `src/secrets.h` nietknięty.

---

## Skąd ta wersja

Po wgraniu 3.57.7 Telegram pokazał:

```
Wersja: vv3.57.7
...
Ostatni znacznik: DESZCZ / BEFORE (po 0 s pracy, wolna pamiec 81 kB)
Ostatni znacznik: FB_LOOP / BEFORE (po 0 s pracy, wolna pamiec 85 kB)
```

Pierwsze to literówka, drugie — **cichy błąd zapisu do NVS**.

## Co jest naprawione

| # | objaw | przyczyna | poprawka |
|---|---|---|---|
| 1 | `vv3.57.7` | komunikat doklejał własne `v` do `FIRMWARE_VERSION`, które już je zawiera | wypisujemy stałą bez dodatkowego prefiksu → `v3.57.8` |
| 2 | zawsze `po 0 s pracy` | klucz NVS `nadzorKrokUptimeS` ma **17 znaków**, a NVS przyjmuje **15** → zapis kończył się cicho, odczyt dawał 0 (ten sam błąd, co `[FIX NVS-15]` przy ustawieniach) | klucz skrócony do `nadzorKrokS` (11). Stary nigdy nie został zapisany, więc nic nie tracimy |
| 3 | brak informacji „jak dawno” | był tylko uptime znacznika | nowy klucz `nadzorWiekS` (12): ile sekund minęło od znacznika **do zawieszenia**; raport pokazuje to jako pierwszą liczbę |

Nowy komunikat:

```
Ostatni znacznik: DESZCZ / BEFORE — 302 s przed restartem (po 25800 s pracy, wolna pamiec 81 kB)
```

**Jak to czytać:** nadzór restartuje centralę po ok. **300 s ciszy**. Więc:

| wiek znacznika | co to znaczy |
|---|---|
| ok. **300 s** (295–310) | znacznik powstał **tuż przed zatrzymaniem** — to jest dokładnie miejsce zawieszenia, np. wejście w `http.GET()` |
| **wyraźnie więcej** (np. 640 s) | po znaczniku zadanie pracowało jeszcze (wiek − 300) s i stanęło **później**; znacznik to tylko dolna granica |
| **`brak`** | zadanie nie zapisało ani jednego znacznika od startu |

Czyli: `302 s` = „stanęło w tym miejscu”, `640 s` = „to miejsce było dawno, stanęło gdzieś dalej”.

## Czego ta wersja NIE robi

**Nie naprawia samego zawieszania.** Oba restarty z 1.11 (23:49 `DESZCZ`, 03:35 `FB_LOOP`)
to to samo, co wcześniej: `tgTask` wchodzi w wywołanie sieciowe (TLS/HTTP) i nie wraca.
Nadzór zadziałał poprawnie — po 300 s zrestartował centralę, piec przez ten czas
pracował normalnie, pauzowały tylko Telegram i Firebase. Ta wersja sprawia, że
**następny taki raport powie, gdzie dokładnie i po jakim czasie** — dopiero na tej
podstawie można celować w przyczynę.

## Testy (wszystkie zielone)

| zestaw | wynik |
|---|---|
| statyczne — w tym **nowa sekcja F: klucze NVS ≤ 15 znaków**, brak `vv`, wiek znacznika | **43 / 43** |
| symulacja C++ (kanał A1 + bramki zapisu) | **80 / 80** |
| symulacja panelu w DOM | **26 / 26** |
| patch `-p0` na 3.57.7 | plik **bit w bit** jak w paczce |

## Jak wgrać

1. Podmień `src/main_centrala.cpp` (kopia: `main_centrala.cpp.bak-3.57.7`).
2. `pio run -t upload` albo OTA z panelu.
3. Po restarcie w Telegramie: **`Wersja: v3.57.8`** (bez podwójnego `v`).
