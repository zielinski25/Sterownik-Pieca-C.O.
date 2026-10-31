# 234 — wynik sprawdzenia po dobie na firmware 3.56.8

Data 2026-10-30. Źródło: `piec_log (13).txt` — 17 986 linii, 3,2 MB,
od 18.10 08:05 do 30.10 10:03. Okres po wgraniu 3.56.8: **29.10 13:22 → 30.10 10:03**.

## Wniosek w trzech zdaniach

1. **Telemetria: 1200 próbek na 1200 wysłanych — zero strat, zero luk.** Detektor
   nic nie wykrył, bo nie było czego wykrywać.
2. **Łata na „Ogrz. śr." NIE ZADZIAŁAŁA i nie miała jak** — klucz `srOstPompa`
   nigdy nie powstał, bo pompa od wgrania ani razu nie ruszyła. W logu
   **0 wystąpień** `SREDNIA po restarcie`.
3. **Wada jest większa, niż myślałem.** To nie dziura w wykresie — to
   **fałszywa liczba**. Po każdym restarcie „Ogrz. śr." pokazuje **dokładnie
   20,0 °C** przez 15–30 min, niezależnie od realnej temperatury pieca.

---

## 1. Wersja w środku: potwierdzona 3.56.8

Dowody z logu:

| dowód | wartość |
|---|---|
| pierwsza linia z licznikami `telemetry_gap_*` | `2026-10-29T13:22:28` |
| OTA potwierdzone | `13:27:35 potwierdzono_valid wynik=ESP_OK` |
| stan partycji po restarcie | `13:56:58 partycja=app0 stan=2` przy `reset=POWERON` |
| rollback | brak — urządzenie pracowało dalej 20 h |

`/api/status` **nie zawiera** tych liczników (sprawdzone w kodzie — patrz
sprostowanie w README 3.56.8). Czytane wyłącznie z linii `tag=HEALTH-A`.

## 2. Telemetria: 1200/1200 — potok szczelny

| sesja | od | do | wyprodukowano | wysłano | odrzucono | w kolejce | **zniknęło** |
|---|---|---|---|---|---|---|---|
| boot=7 (po OTA) | 29.10 13:22 | 29.10 13:52 | 30 | 30 | 0 | 0 | **0** |
| boot=1 (po POWERON) | 29.10 14:01 | 30.10 10:01 | 1200 | 1200 | 0 | 0 | **0** |

- **1200 minut = 1200 próbek** — dokładnie 1 na minutę, bez ani jednej przerwy.
- `telemetry_gap_events = 0`, `telemetry_gap_samples = 0` — detektor milczy.
- `telemetry_dropped = 0` — ring nigdy się nie przepełnił.
- `telemetry_ring = 0` na koniec — kolejka pusta, wszystko poszło.

**Był test:** 30.10 w godzinach **02:39–02:50** Firebase miał awarię
(`FB-SEND timeout t=6011ms`, `FB-CB open streak=2 backoff=30s`, kilkanaście
`skip_status_trwa`). **11 minut bez wysyłki — i ani jednej straconej próbki.**
Ring przyjął je i oddał po powrocie. To dowód, że bufor działa.

**Czego to jeszcze nie dowodzi:** audyt 29 dni wykazał 202 stracone próbki na
40 789 (0,50%), czyli ~6 na dobę. Tu wyszło 0 na 1200. Przy tempie z audytu
szansa na zero to około 0,25% — więc albo jest realnie lepiej (mniej restartów,
bo ich w tej dobie nie było wcale), albo mieliśmy po prostu spokojną dobę.
**20 godzin to za mało, żeby ogłosić wygraną.** Potrzeba kilku dób.

## 3. Łata na „Ogrz. śr." — nie zadziałała, i teraz wiem dlaczego

### Co mówi log

```
linii "SREDNIA po restarcie":  0
statusów pompa=OFF po 29.10 13:00:  238  (z 238)
statusów pompa=ON  po 29.10 13:00:    0
```

Kod zapisuje klucz `srOstPompa` **wyłącznie przy zaniku pompy** (przejście
ON→OFF). Od wgrania piec stoi zimny (12 °C), pompa nie pracowała ani raz →
klucz nie powstał → po restarcie nie ma czego odtwarzać.

To dokładnie ten przypadek, o którym pisałem wczoraj: **pierwszy restart po
wgraniu może mieć dziurę**. Ale gorzej — dopóki piec nie pali, **każdy** restart
będzie ją miał. Łata jest uśpiona do pierwszego rozpalenia.

### Co widać zamiast dziury: fałszywa liczba 20,0 °C

W kodzie jest `static int ostatni_poprawny = 20;` — wartość startowa.
Po restarcie, dopóki piec nie zostanie uznany za wygasły (a to wymaga
`Czas_OFF` = 20 minut od startu, bo licznik `millis()` startuje od zera),
kanał trzyma te **20,0 °C** bez względu na rzeczywistość.

Zmierzone — **19 takich okien w 12 dni**, praktycznie po każdym restarcie:

| kiedy | ile trwało | realna temp. pieca | pokazane „Ogrz. śr." | błąd |
|---|---|---|---|---|
| 29.10 14:01–14:11 | 15 min | 12,0 °C | **20,0 °C** | +8 K |
| 29.10 13:22–13:32 | 15 min | 12,2 °C | 20,0 °C | +7,8 K |
| 24.10 13:59–14:09 | 15 min | 11,5 °C | 20,0 °C | +8,5 K |
| 22.10 11:50–12:00 | 15 min | 14,2 °C | 20,0 °C | +5,8 K |
| **19.10 20:54–21:04** | 15 min | **52,0–53,0 °C** | **20,0 °C** | **−32 K** |
| 20.10 23:07–23:17 | 15 min | 49,2–50,8 °C | 20,0 °C | −30 K |
| 21.10 00:30–00:40 | 15 min | 37,5–38,8 °C | 20,0 °C | −17 K |
| 20.10 02:56–03:26 | 30 min | 27,5–28,5 °C | 20,0 °C | −8 K |

**To nie kosmetyka.** W sezonie grzewczym, przy piecu 52 °C, wykres pokazuje
20 °C przez 15 minut po restarcie. Błąd **−32 K** i wygląda jak prawdziwy
pomiar, bo nie jest dziurą — jest linią.

Dodatkowo: sam wykres prawdopodobnie nadal ma **dziurę**, bo kanał
`TELEMETRY_CH_OGZ_SR` nie jest oznaczany jako świeży (`telemetryOgrzSrAccepted`
= false), więc jakość leci w stan „stale". Wtedy aplikacja nie rysuje punktów —
czyli masz **jedno i drugie**: dziurę w chmurze i 20,0 °C na pulpicie.

## 4. Reszta — zdrowo

| co | wynik |
|---|---|
| restarty po 29.10 14:01 | **0** (1200 minut ciągłej pracy) |
| pamięć wewn. wolna | 84 580 B na koniec, min 76 508 B, max 88 100 B |
| PSRAM wolna | 6,61–6,70 MB — bez zmian |
| stos pętli (hwm) | 3 232–4 612 B — bezpiecznie |
| błędy czujników DS18B20 | 6, wszystkie w sekundzie startu (spurious 85 °C, odrzucone poprawnie) |
| błędy Firebase | tylko okno 02:39–02:50, bez strat danych |
| serwo / rozpalanie | 4 cykle auto, zakończone `brak_potwierdzenia_30min` — normalne, piec nie palił |

## 5. Co proponuję na 3.56.9 — DO TWOJEJ DECYZJI

Nie ruszam kodu bez Twojej zgody. Dwie opcje, obie małe:

**A) Minimum (najbezpieczniejsze).** Zamiast stałej `20` brać realny odczyt:
`static int ostatni_poprawny = 20;` → przy pierwszym poprawnym odczycie
podstawić `temperaturaOgrzewania`. Efekt: po restarcie „Ogrz. śr." pokazuje
12 °C albo 52 °C — prawdę — zamiast 20,0. **Znikają oba objawy: i dziura,
i fałszywa liczba.** Ryzyko: bardzo małe, dotyczy jednej zmiennej.

**B) Pełna.** To co wyżej **plus** odtworzenie `srOstPompa` także wtedy, gdy
klucza nie ma (piec nigdy nie palił od wgrania) — żeby łata z 3.56.8 zaczęła
działać od pierwszego restartu, a nie dopiero po pierwszym rozpaleniu.

Moja rekomendacja: **A**. Rozwiązuje zmierzony problem w 100%, nie dokłada
nowej logiki czasowej, którą trzeba by znowu testować dobę.

## 6. Czego potrzebuję od Ciebie

1. **Decyzja A czy B** (albo „nie ruszać").
2. **Zrzut wykresu** z zakładki Wykresy, kanał „Ogrz. śr.", okolice
   29.10 13:56–14:20 — żeby potwierdzić, czy jest tam dziura, czy linia 20,0.
3. Nic więcej. Log był kompletny — dzięki, to wystarczyło.

## 7. Pliki

- `sprawdz-po-dobie.py` — narzędzie, którym to policzyłem (przepisane tak, żeby
  radziło sobie z tym logiem: pomija stare wpisy sprzed 3.56.8, sortuje po
  czasie, dzieli na sesje po resecie licznika, nie po numerze `boot`, bo ten
  po wgraniu startuje od 1 i myli)
- surowy log **nie** jest w repozytorium — 3,2 MB na dobę by je rozdęło
