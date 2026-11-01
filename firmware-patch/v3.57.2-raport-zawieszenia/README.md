# Firmware v3.57.2 — raport nadzoru mówi, GDZIE stanęło

Data: 2026-10-31
Baza: v3.57.1
Plik do podmiany: **`main_centrala.cpp`** (jeden plik)
Pliki `.h`: **bez zmian** (te same co 3.57.1 — są w paczce tylko dla kompletności)

---

## 1. Co jest celem

Po zdarzeniu z **31.10.2026, 04:14** przyszedł komunikat:

> Wróciłem po automatycznym restarcie.
> Zadanie "tgTask (Telegram i Firebase)" nie dało znaku życia przez 300 s.

Wiadomo było **kto** milczał. Nie było wiadomo **na czym stanął**.

Ta wersja dokłada do tego komunikatu **jedną linijkę** z ostatnim znacznikiem
kontrolnym zawieszonego zadania. Nic więcej.

## 2. Skąd te dane były brane

Firmware od v3.18.0 prowadzi `diagCheckpoint()` — każdy krok zadania zapisuje
do pamięci RTC czwórkę: **operacja, etap, uptime, wolna pamięć**. Ten sam
mechanizm obsługuje raporty po crashu.

Problem: `diagInicjalizujBoota()` w `setup()` **zeruje** ten stan przy każdym
starcie. Nadzor restartował centralę → informacja przepadała, zanim ktokolwiek
mógł ją odczytać.

## 3. Co zmieniono

| miejsce | zmiana |
|---|---|
| `nadzorOdczytajKrok()` (nowa, ~15 linii) | czysty odczyt znacznika z `rtcCrashDiag` dla wskazanego zadania |
| `nadzorZadanSieciowych()`, gałąź `NADZOR_RESTART` | odczyt **przed** `ESP.restart()`, zapis 5 kluczy do NVS, rozszerzony wpis w logu |
| `wyslijRaportNadzoru()` | doklejona linia „Ostatni znacznik: …" |
| `FIRMWARE_VERSION` | `v3.57.1` → `v3.57.2` |

Kolejność jest istotna: odczyt musi być **przed** `ESP.restart()`, bo po
restarcie `setup()` już wyzerował `rtcCrashDiag`.

## 4. Jak czytać nowy komunikat

```
Ostatni znacznik: FB_SEND / BEFORE (po 11520 s pracy, wolna pamiec 94 kB)
```

| fragment | znaczenie |
|---|---|
| `FB_SEND` | ostatnia czynność zgłoszona przez zadanie |
| `BEFORE` | przed wejściem w tę czynność (`AFTER` = po wyjściu) |
| `11520 s` | uptime w chwili tego znacznika (3 godz 12 min) |
| `94 kB` | wolny RAM w tym momencie |

Nazwy operacji (`diagOpName()`, bez zmian): `FB_SEND`, `FB_CMD`, `FB_INIT`,
`FB_LOOP`, `TG_POST`, `TG_CONNECT`, `TG_POLL`, `TG_POLL_WAIT`, `TG_ITER`,
`TG_COMMAND`, `WIFI_DNS`, `HIST_FS`, `LOG_FS`, `DESZCZ`, `PROGNOZA`.

Gdy zadanie nie zapisało **ani jednego** znacznika od startu (`seq == 0`),
komunikat mówi to wprost:

```
Ostatni znacznik: brak — zadanie nie zapisalo ani jednego znacznika od startu.
```

To też jest odpowiedź: zawieszenie nastąpiło przed pierwszą iteracją pętli.

## 5. Czego to NIE robi

- **Nie naprawia przyczyny zawieszenia.** To diagnoza, nie lek.
- Nie zmienia progów nadzoru: 5 min ciszy, 3 min rozbiegu, max 3 restarty,
  okno 30 min, zerowanie licznika po 30 min pracy.
- Nie zmienia `nadzorDecyzja()` ani testu hostowego `NadzorHostTest.cpp`.
- Nie zmienia telemetrii, Firebase, Telegrama, OTA, historii, archiwum
  solarnego, formatu `/piec/status` ani `/piec/cmd`.
- Nie zmienia rytmu 15 s (status) / 7 s (komendy) / podgląd 2 s.

**Aplikacja 0.31.63 nie wymaga żadnych zmian.**

## 6. Po co to, jeśli nie naprawia

Bo bez tego każde kolejne zawieszenie to znowu zgadywanie. Historia pokazuje
koszt zgadywania: v3.56.6 przepisała trzy zmiany naraz, po dwóch restartach
z rzędu została cofnięta w całości (v3.56.7 = funkcjonalnie v3.56.5).

Jeden konkretny znacznik (`FB_SEND / BEFORE` vs `TG_POST / BEFORE` vs
`WIFI_DNS / BEFORE`) wskazuje winowajcę od razu, bez dopasowywania pliku ELF
do zrzutu pamięci.

## 7. Co dalej z logiem z nocy

Log `piec_log (31).txt` (3,5 MB, 31.10 04:13) jest na Dysku. Zawiera też
zapisy sprzed zawieszenia — warto sprawdzić, co działo się w oknie
**03:09–03:14 UTC** (04:09–04:14 czasu lokalnego), czyli w 5 minutach
poprzedzających ciszę.

## 8. Pliki w paczce

- `main_centrala.cpp` — **pełny plik do podmiany**
- `main_centrala-v3.57.1-to-v3.57.2.patch` — sama różnica wobec 3.57.1
- `README.md` (ten plik), `CZYTAJ-NAJPIERW.txt`, `SHA256SUMS.txt`
- pozostałe `.h` + `secrets.h.example` — bez zmian
- `SterownikCO-v3.57.2-RAPORT-ZAWIESZENIA.zip` — komplet do pobrania
