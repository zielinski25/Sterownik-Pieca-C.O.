# Archiwum solarne w centrali ESP32 — stan i ustalenia

Dokument odtworzony 2026-10-20 po resecie środowiska (poprzednia kopia nie była w repozytorium).
Ma być **żywy**: każda zmiana zachowania archiwum dopisuje sekcję „Historia wersji”, a ustalenia
sprzętowe poprawia się w miejscu. Wersja opisywana: **v3.52.7**.

---

## 1. Po co to jest

Centrala ESP32 sama, bez otwartego `Piec.html` i bez aplikacji na telefonie, zbiera:

1. **snapshot prognozy GHI** na kolejną dobę (Open-Meteo, `shortwave_radiation`),
2. **podsumowanie rzeczywistych odczytów temperatury zbiornika kolektora słonecznego**.

Dane leżą lokalnie w LittleFS (`/solar_daily/RRRR-MM-DD.json`), potem są wysyłane do Firebase.
Dostęp do nich jest **wyłącznie przez Telegram albo GitHuba** — w raporcie nie ma już odnośnika
do endpointu w sieci lokalnej.

## 2. Kanał pomiarowy — to NIE jest bojler pieca

| | Układ słoneczny (archiwum) | Bojler pieca C.O. |
|---|---|---|
| czujnik | `czujnik_temp_Panel_Plus` | `czujnik_temp_Bojlera` |
| bieguna | `BUS_3` | `BUS_2` |
| kanał telemetrii | `TELEMETRY_CH_PANEL` | `TELEMETRY_CH_BOJLER` |
| zakres | **−30…160 °C** | 0…160 °C |

Rekord archiwum **nie może być hybrydowy**. Rekordy sprzed poprawki kanału mają
`schemaVersion = 1` i są oznaczane `[dane CO]`; usuwa się je przez `/archiwum_czysc TAK`.

## 3. Instalacja (ustalenia użytkownika)

- kolektor **próżniowy rurowy, 10 rur** (nie 12),
- zbiornik **100 l**, zintegrowany z kolektorem, na dachu,
- obieg **termosyfonowy — bez pompy**, więc bez przepływomierza,
- czujnik w **wodzie, z boku, w połowie wysokości** zbiornika,
- **są dwa boilery**: (1) na dachu — zbiornik kolektora, (2) w kotłowni — bojler C.O. grzany
  wężownicą z pieca. Zimna woda z wodociągu idzie **bezpośrednio** do boilera, potem ciśnieniowo
  do kranu; użytkownik zaworami wybiera ten, w którym woda jest cieplejsza.

## 4. Tryby pracy instalacji

- **LATO / JESIEŃ / WIOSNA** — wodociąg → zbiornik na dachu → kran (obsługiwany jest boiler z dachu).
- **ZIMA** — kolektor nieużywany, **woda spuszczona**, używany bojler pieca.
- **spuszczona (poza sezonem, wiosna–jesień)** — sytuacja wyjątkowa: zbiornik bez wody
  (np. po spuszczeniu na czas mrozów albo przed napełnieniem).

Znacznik trybu ustawia się z Telegrama:

```
/solar_tryb                 # pokazuje stan
/solar_tryb spuszczona      # ZIMA: woda spuszczona — bez GHI, rekordy offSeason
/solar_tryb sezon           # woda w zbiorniku — GHI wraca przy najbliższym oknie
```

Tryb jest zapisany w NVS (`solarOffSeason`), więc **przeżywa restart i OTA**.

**Dlaczego to jest potrzebne:** czujnik w pustym zbiorniku mierzy powietrze. Bez znacznika taki
dzień wygląda dokładnie jak zwykły pochmurny dzień z zerowym przyrostem. Poza sezonem:
prognoza GHI **nie jest pobierana**, rekord ma `offSeason = true`, a w raporcie dopisek
`(poza sezonem)`.

## 5. Co dokładnie zbiera firmware

- **Próbka telemetrii** — co 60 s, wpięta w istniejący zapis historii (`historiaZapiszProbke`).
- **Checkpoint rekordu** — co 5 min, tylko gdy jest co zapisać (próbka albo prognoza dla tej daty).
- **Granice doby** — zawsze `Europe/Warsaw`, liczone przez `mktime`/`localtime` z `TZ`,
  więc doba ma 24, 23 albo 25 godzin (zmiana czasu rozliczana poprawnie).
- **Okna prognozy GHI:**
  - wieczór **23:30–23:59** → prognoza na **jutro** (podstawowe źródło), do 8 prób co 2 min,
  - dostęp **00:00–04:59** → prognoza na **dziś**, gdy wieczorna się nie udała
    (`capturedLate`, do 20 prób co 15 min, timeout 7 s).

## 6. Format rekordu (`/solar_daily/RRRR-MM-DD.json`)

- `schemaVersion = 2`, `localDate`, `timezone = Europe/Warsaw`, `source`, `updatedAtUtc`,
- `seasonMode` / `offSeason` — tryb instalacji w chwili zapisu,
- `forecast`: `source`, `capturedAtUtc`, `expectedHours`, `hourCount`, `complete`,
  `sumGhiKwhM2`, `peakWm2`, tablica `hours[] {t, ghi}` (czas UTC punktu),
- `actual`: `channel = t_panel`, opis systemu, `minC`, `maxC`, `deltaC`, `firstUtc`, `lastUtc`,
  `coverageRatio`, `validSamples`, `expectedSampleCount`, `simulatedSamples`,
  `technicalCandidate`, `finalized`.

## 7. Komendy Telegrama

| Komenda | Działanie |
|---|---|
| `/archiwum [N]`, `/solar [N]` | raport z N ostatnich dni (1–30, domyślnie 7) |
| `/archiwum_usun RRRR-MM-DD` | usuwa **jeden** dzień |
| `/archiwum_czysc` / `TAK` | pokazuje / usuwa rekordy sprzed poprawki kanału |
| `/solar_tryb [spuszczona\|sezon]` | znacznik trybu instalacji |

## 8. Jak czytać raport

```
2026-10-20  GHI 0.55 kWh/m2  dT +0.2C  amplituda 2.5K  (min 22.5 / max 25.0)  pokrycie 74.0% (98.6% czasu)  1065/1440 prob. 05:20-23:30 (w toku)
```

- **GHI** — suma nasłonecznienia z prognozy, kWh/m². `(niepelna)` = brakuje godzin.
- **dT** — pierwsza próbka doby → ostatnia. **Słaby wskaźnik**: porównuje północ z wieczorem.
- **amplituda** — `max − min` w ciągu doby. To ona pokazuje, ile zbiornik realnie się zmienił.
- **pokrycie** — udział minut doby, w których jest próbka. W dobie w toku drugi nawias to pokrycie
  **czasu, który już upłynął** (mianownik = minuty od północy).
- `(w toku)`, ` sym`, `TECH`, `[dane CO]`, `(poza sezonem)` — znaczniki stanu rekordu.

## 9. OTA i rollback — wymóg „żadna aktualizacja nie może wymagać kabla”

Od v3.52.6 partycja jest potwierdzana **w `loop()`**, nie na końcu `setup()`:

| warunek | czas |
|---|---|
| stabilna praca z WiFi | 10 min |
| awaryjnie, bez WiFi | 60 min |

Firmware resetujący się co 15 s nigdy nie potwierdzi obrazu → bootloader wraca do poprzedniej
wersji. Znacznik `/ota_pending.txt` pozwala wykryć cichy rollback i wysłać o nim Telegram
(`tag=OTA-ROLLBACK`). **Potwierdzone na urządzeniu:** v3.52.6 wgrane OTA, komunikat
„Aktualizacja zakończona pomyślnie” przyszedł ~10 minut po starcie.

Czego to **nie** chroni: awarii po potwierdzeniu, zawieszenia bez resetu, zaniku prądu w pierwszym
starcie.

## 10. Czego świadomie NIE twierdzimy

- `ΔT × 0,1163 kWh/K` to **równoważnik rzędu wielkości** dla 100 l, a nie zmierzony uzysk kolektora
  (brak przepływomierza, termosyfon).
- Czujnik w połowie wysokości **nie widzi stratyfikacji**, więc licznik poborów
  (>0,3 °C/min w ≤5 min) będzie **zaniżał** — nie udajemy, że policzy wylaną wodę.
- Dzień bez GHI nie jest „błędny” — po prostu nie ma czym porównać temperatury.
- Poza sezonem temperatura z pustego zbiornika **nie jest** danymi słonecznymi.

## 11. Historia wersji

| wersja | co zmieniła |
|---|---|
| v3.51.0 | archiwum w centrali (nie w przeglądarce), raport `/archiwum`, przycisk w Telegramie |
| v3.51.1 | przyczyna braku GHI w rekordzie i w raporcie (`http_400`, `json`, `brak_pol`) |
| v3.52.0 | kanał `t_panel` (układ słoneczny), rekordy CO oznaczane i usuwalne |
| v3.52.1 | `/archiwum_usun` — pojedynczy dzień; koniec rekordów hybrydowych |
| v3.52.2 | pusty rekord nie wraca po usunięciu dnia |
| v3.52.3 | przyczyna `http_400`: końcowe `Z` w `start_hour`/`end_hour` |
| v3.52.4 | twarda bramka `SOLAR_DAILY_ARCHIVE_API` — nie da się zbudować mieszanej paczki |
| v3.52.5 | odpowiedź GHI czytana w całości (`getString`) przed `deserializeJson` |
| v3.52.6 | potwierdzenie OTA w `loop()` (10 min / 60 min) + wykrywanie cichego rollbacku |
| **v3.52.7** | amplituda, pokrycie od upływu czasu, `/solar_tryb`, tablica GHI bez przesunięcia |

## 12. Co dalej (plan etapowy)

1. **Najbliższe dni** — po wgraniu v3.52.7 sprawdzić w `/archiwum`, czy linia ma `amplituda`
   i czy w dobie w toku pojawia się `(x% czasu)`.
2. **Przed zimą** — wpisać `/solar_tryb spuszczona` **w dniu spuszczania wody**, nie później;
   inaczej rekordy z pustego zbiornika wejdą do statystyk jako „pochmurne dni”.
3. **Wiosną** — `/solar_tryb sezon`; prognoza GHI wróci przy najbliższym oknie.
4. **Opcjonalnie** — po odzyskaniu źródeł odtworzyć `PLAN_ARCHIWUM_SOLARNE_CENTRALA.md`
   (plan wdrożenia i uzasadnienia decyzji) — obecnie nie istnieje.
