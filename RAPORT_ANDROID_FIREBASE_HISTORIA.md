# Android: pobieranie danych z Firebase i historia telemetrii — research

Data: 2026-10-22. Zakres: cała aplikacja `android/` (Kotlin + Compose), ze szczególnym
naciskiem na to, **jak pobierane są dane z Firebase** i **czy historia jest dociągana
przyrostowo**, czy za każdym razem ściągana od nowa.

> **Wersja — ustalona (2026-10-22).** Twoje „31.19" to **v0.31.19**, czyli:
> baza **v0.31.18** (`C:\Users\Rafcio\Desktop\SterownikCO-Android-v0.31.18-build-20261016-094945\android`)
> plus pliki Androida z paczki **`SterownikCO-v3.50.0-ALARM-SOUND-SYNC.zip`**.
> Ta paczka dodaje `AlarmSoundSync.kt` i podnosi numer do 0.31.19 — **zmienia wyłącznie
> synchronizację dźwięków alarmów**, nie dotyka pobierania historii.
>
> Repozytorium (gałąź `arena/46f3288a…`) ma źródła **v0.31.11**. Różnicę 0.31.11 → 0.31.18
> odtworzyłem z 22 patchy w `android-sync/*.diff` (harmonogram wykresów, inspektor,
> pełnoekranowy wykres, nawigacja „Back", terminal DLOG, dźwięki alarmów, Direct Boot,
> bezpieczeństwo/bateria). **Żaden z tych patchy nie zmienia `fetchTelemetry`,
> `fetchShard` ani `loadRange`** — jedyne trafienie w tym obszarze to `Cache-Control: no-store`
> przeniesione do stałej. Wniosek: sekcje 3–6 opisują także **Twoją 0.31.19**.
>
> Jeśli chcesz mieć pewność co do bajtów, w `android-sync/Check-Android-Version.ps1`
> jest skrypt porównujący SHA256 — ale dla *drzewa 0.31.18*, nie 0.31.11.
>
> To **analiza statyczna kodu**. Nie uruchamiałem aplikacji (brak JDK/SDK w środowisku),
> nie mierzyłem ruchu sieciowego na telefonie. Liczby bajtów i żądań są policzone
> z formatu rekordów zapisywanych przez firmware (`main_centrala.cpp`, funkcja
> budująca JSON telemetrii, ~linia 39263).

---

## 0. Skąd się wzięła wersja 0.31.19

| element | skąd |
|---|---|
| baza | `SterownikCO-Android-v0.31.18-build-20261016-094945\android` (projekt lokalny, poza repo) |
| nakładka | `v3.50.0-alarm-sound-sync\android` z paczki `SterownikCO-v3.50.0-ALARM-SOUND-SYNC.zip` |
| efekt | `AlarmSoundSync.kt` + `alarm_sound …` wysyłane do centrali, `versionName = v0.31.19` |
| czego **nie** zmienia | telemetria, historia, wykresy, cache |

W repozytorium nie ma ani katalogu `v3.50.0-alarm-sound-sync`, ani `AlarmSoundSync.kt` —
ta gałąź kończy się na 0.31.11. Firmware'owa część v3.50 jest obecna (przez v3.53.1,
`grep alarm_sound` w `main_centrala.cpp` → 12 trafień), więc centrala obsługuje komendę.

## 1. Streszczenie — odpowiedź na pytanie

**Czy prawidłowo pobiera dane z Firebase?**
Tak, samo pobieranie jest poprawnie zaimplementowane: właściwe ścieżki shardów
(`/piec/telemetry/1m/v1/RRRR/MM/DD`), paginacja po `ts`, obsługa odświeżenia tokenu
przy 401/403, filtr jakości `q`, obsługa symulacji. Nie znalazłem błędu, który
gubiłby albo dublował dane w normalnych warunkach.

**Czy dociąga tylko brakujące dni, żeby nie ściągać bazy od nowa?**
**Nie. Nie ma żadnego cache'u.** Każde wejście na zakładkę „Wykresy", każde wznowienie
aplikacji i każda zmiana zakresu pobiera **cały wybrany zakres od zera** — 30 dni to
**~8,1 MB i ~150 żądań HTTP** przy każdym takim zdarzeniu.

To najpoważniejszy problem wydajnościowy aplikacji: nie jest to wyciek pamięci ani
błąd danych, tylko brak warstwy pośredniej. Szczegóły i propozycja naprawy w sekcjach 5–6.

---

## 2. Mapa przepływów danych (stan obecny)

| co | skąd | jak często | cache |
|---|---|---|---|
| status pieca (`/piec/status`) | Firebase RTDB | UI: **co 8 s** (`AppModel.kt:234`); usługa alarmowa: **co 60 s** (`AlarmMonitorService.kt:308`) | brak (tylko w pamięci) |
| **historia telemetrii** | Firebase RTDB, shardy dzienne | przy `onResume`, wejściu na Wykresy, zmianie zakresu, ręcznym odświeżeniu | **BRAK** |
| pogoda | Open-Meteo | start + co 15 min (`AppModel.kt:235`), z blokadą `weatherMutex` | brak (przetrwa tylko proces) |
| archiwum solarne | lokalne `Prefs` (`solar_real_archive_v1`) | próbka co 25 s, zapis co 5 próbek | tak, ostatnie 500 próbek |
| dzienne archiwum solarne centrali (`/solar_daily`) | **nieużywane** | — | — |

Ostatni wiersz jest istotny: całe archiwum dobowe z v3.51–v3.53 (GHI, min/max, pokrycie)
żyje **tylko w centrali i w Telegramie**. Aplikacja go nie zna — nie ma w niej odwołania
do `/piec/solar_daily` ani do Firebase'owego odpowiednika tego archiwum.

---

## 3. Jak dokładnie działa pobieranie historii

### 3.1 Ścieżka wywołań

```
MainActivity.onResume()                       MainActivity.kt:86
  └─ AppModel.onResume()                      AppModel.kt:231
       └─ loadRange(rangeSec)                 AppModel.kt:864
            └─ Rtdb.fetchTelemetry(sec)       RtdbClient.kt:475
                 └─ dla każdej doby: fetchShard(path, from, to)   RtdbClient.kt:392
                      └─ pętla: GET ?orderBy="ts"&startAt=…&endAt=…&limitToFirst=300
```

To samo `loadRange()` odpala się jeszcze:
- `AppModel.kt:434` — zaraz po zalogowaniu, jeśli jesteśmy na stronie 1,
- `AppModel.kt:2104` — `navigate(1)`, czyli **każde dotknięcie zakładki Wykresy**,
- `AppModel.kt:986-989` — `setRange()` (zmiana 6 h / 24 h / 7 dni / 30 dni),
- `Charts.kt:130` — przycisk „Odśwież historię".

### 3.2 Zapytania

Dla zakresu `N` dni kod buduje listę dób UTC (`dayRange`, `RtdbClient.kt:371`) i dla
każdej wykonuje paginację po 300 rekordów. Doba ma 1440 próbek (interwał 1 min), więc:

| zakres | dób | rekordów | bajtów | żądań (300/stronę) |
|---|---|---|---|---|
| 6 h | 1 | 360 | 0,07 MB | 2 |
| 24 h | 2 | 1440 | 0,28 MB | 5–6 |
| 7 dni | 8 | 10 080 | 1,9 MB | ~35 |
| **30 dni** | **31** | **43 200** | **8,1 MB** | **~150** |

Rekord (format firmware) ma **196 B**: `seq, ts, mono, a[11], k, s, state, q, sim, age[16]`.
Aplikacja **nie używa** pola `age` (16 liczb w każdym rekordzie), ale i tak je pobiera.

Żądania są **sekwencyjne** (kursor `startAt=cursor`, następna strona po odebraniu
poprzedniej). Przy 30 dniach i 150 okrążeniach po ~200–400 ms na LTE daje to
**30–60 sekund** oczekiwania — za każdym razem od nowa.

### 3.3 Ścieżka awaryjna

Jeśli RTDB odpowie błędem zawierającym „Index not defined", „indexOn" **albo „400"**
(`RtdbClient.kt:403`), kod przestawia `historyIndexFallback = true` i od tej chwili
pobiera **cały shard jednym żądaniem** (`fetchShardFull`, `RtdbClient.kt:426`).
Flaga jest polem `object Rtdb`, więc **nie resetuje się do końca życia procesu**.

Konsekwencja: jeśli w regułach RTDB nie ma `.indexOn: "ts"` na
`/piec/telemetry/1m/v1/...`, to każde odświeżenie pobiera pełne doby niezależnie
od tego, czy potrzebujesz 6 godzin, czy 30 dni.

---

## 4. Co jest zrobione dobrze (nie ruszać)

- Poprawne ścieżki shardów i filtrowanie `r.ts in startTs..endTs` (`RtdbClient.kt:413`).
- Ochrona przed pętlą nieskończoną: limit 100 stron i warunek `maxTs < cursor`.
- Odświeżenie tokenu przy 401/403 z ponowieniem żądania (`RtdbClient.kt:139-158`).
- Serializacja odświeżeń tokenu (`authRefreshMutex`) i komend (`commandSendMutex`).
- `toPoint()` respektuje pole jakości `q` (2 bity na kanał) i zakresy symulacji.
- Anulowanie żądań przy skasowaniu korutyny (`invokeOnCompletion`).
- Blokada duplikatu pobrania pogody (`weatherMutex`).
- Wykresy liczone w `remember(...)`, więc nie przeliczają się przy każdej rekompozycji.

---

## 5. Znaleziska

### F1 — Brak jakiegokolwiek cache'u historii (wysoki)

`loadRange()` za każdym razem woła `Rtdb.fetchTelemetry(sec)` i **podmienia** listę
(`AppModel.kt:872-876`). Nie ma Room, SQLite, plików, ani nawet ponownego użycia
poprzedniej listy w pamięci. Nie ma też warunku „jeśli zakres się nie zmienił i dane
mają mniej niż X sekund — pomiń".

Skutek: 8,1 MB i 150 żądań przy każdym wejściu na Wykresy z zakresem 30 dni.

### F2 — Pobieranie historii przy każdym wznowieniu aplikacji, niezależnie od ekranu (wysoki)

`AppModel.onResume()` (`AppModel.kt:231`) woła `loadRange(rangeSec)` **bez sprawdzenia,
czy użytkownik jest na stronie Wykresy**. Wystarczy przełączyć się na WhatsApp i wrócić.
Zakres jest zapamiętywany w `Prefs` (`chart_prefs_v2`), więc raz ustawione 30 dni
oznacza 8 MB przy każdym powrocie do aplikacji.

To samo zauważył wcześniejszy audyt (`AUDYT_ANDROID_BEZPIECZENSTWO_PAMIEC_BATERIA.md:82`),
ale bez liczb.

### F3 — Brak deduplikacji równoległych pobrań (średni)

`loadRange()` to `scope.launch { }` bez trzymania referencji do zadania. Szybkie
dotknięcie „24 h" → „7 dni" → „30 dni" uruchamia **trzy** równoległe pobrania; żadne
nie jest anulowane. Efekt: potrójny transfer i wyścig o to, które dane wylądują w UI
(wygrywa to, które skończy ostatnie — niekoniecznie ostatnio wybrane).

### F4 — Ciche gubienie dni przy błędzie (średni)

`RtdbClient.kt:484`: `catch (e: Exception) { /* pusty dzień */ }`. Jeśli w połowie
pobierania wygaśnie sesja albo zerwie się sieć na jednej dobie, wykres pokaże
„Załadowano N próbek" **bez ostrzeżenia**, a użytkownik zobaczy dziurę i pomyśli,
że centrala nie zapisywała.

### F5 — `historyIndexFallback` nigdy się nie wyłącza (średni)

Jedno niepowodzenie indeksu (albo dowolny HTTP 400 — warunek łapie też `"400"`)
i do końca życia procesu każda doba jest pobierana w całości, nawet dla zakresu 6 h.

### F6 — Brak downsamplingu przy rysowaniu (średni)

`ChartEngine` rysuje **każdy** punkt (`cubicTo` na parę sąsiednich próbek).
Dla 30 dni to 43 200 punktów × do 14 serii. Do tego `filterGlitches` liczy Hampel
na całej liście dla każdej serii. Rozwiązanie jest w `remember`, więc liczy się raz,
ale pierwsze wejście na 30 dni musi to przeliczyć i narysować.

### F7 — `Cache-Control: no-store` na każdym GET (niski, ale warto wiedzieć)

`RtdbClient.kt:145` — świadome wyłączenie cache'u HTTP. Przy Firebase REST i tak nie
dałoby to dużo, ale oznacza, że **nie ma żadnej warstwy oszczędzania** poza aplikacją.

### F8 — Aplikacja nie zna dziennego archiwum solarnego centrali (średni, funkcjonalny)

Wszystko, co powstało w v3.51–v3.53 (GHI, min/max z godziną, pokrycie, tryb
„spuszczona"), jest dostępne **tylko w Telegramie**. W aplikacji jest wyłącznie stary
`SolarAnalytics` z `Piec.html`, liczony lokalnie z próbek zbieranych **tylko wtedy,
gdy aplikacja jest otwarta** (maks. 5000 próbek ≈ 2 dni przy 25 s).

### F9 — Brak jakiegokolwiek cache'u statusu (niski)

Po zimnym starcie bez sieci pulpit jest pusty — nie ma „ostatniego znanego stanu".
Przy piecu węglowym to nie jest krytyczne, ale przy słabym zasięgu wygląda jak awaria.

### F10 — Ekran „Oprogramowanie" podaje wersję aplikacji jako wersję panelu WWW (niski)

`checkGithubRelease()` (`AppModel.kt:2012`) porównuje `releases/latest` z wersją
**firmware** (`fwLabel()`), i to jest poprawne — wydania w repozytorium to firmware.

Ale wiersz „APLIKACJA WEB PRO" (`System.kt:829`) pokazuje `S.appVersion`, a to pole
jest ustawiane raz, w `PiecState.kt:67`, na `BuildConfig.VERSION_NAME` — czyli wersję
**aplikacji Android** (np. `v0.31.11`). Z `/piec/status` nigdy nie jest nadpisywane.
Efekt: wiersz opisany „Interfejs HTML/CSS/JS PRO" pokazuje wersję aplikacji.
Paradoksalnie to jedyne miejsce w UI, gdzie wersję aplikacji w ogóle widać — dlatego
warto ją zostawić, tylko poprawnie podpisać (np. „APLIKACJA ANDROID").

---

## 6. Propozycja: cache przyrostowy historii

Cel: **z 150 żądań i 8,1 MB przy każdym wejściu → 1–2 żądania i ~0,1 MB**, przy
zachowaniu pełnej historii offline.

### 6.1 Przechowywanie

Bez nowych zależności (nie ma Room w projekcie i nie trzeba go dodawać):

```
filesDir/telemetry/v1/2026-10-21.json
filesDir/telemetry/v1/2026-10-20.json
```

Każdy plik: tablica rekordów w formacie minimalnym (to, co potrzebne do `TelemRow`:
`id, seq, ts, mono, a[], k, s, state, q, sim`). **Bez `age`** — aplikacja go nie używa,
a to 16 z 196 bajtów rekordu (8%).

Rozmiar: ~121 B/rekord ⇒ doba ≈ 170 KB, 30 dni ≈ **5 MB**, 45 dni ≈ 7,5 MB
(limit `TelemetryCache.MAX_BYTES` = 24 MB) — mniej niż 8,1 MB z sieci, bo bez `age`
i bez nazw pól można zapisać zwartą tablicą).

### 6.2 Algorytm

```
loadRange(sec):
  dni = lista dób UTC pokrywających zakres
  wynik = []
  for dzien in dni:
      zapis = cache.wczytaj(dzien)
      if zapis != null and zapis.kompletny and dzien < dzisiaj:
          wynik += zapis.rekordy            # 0 żądań
      else:
          od = if (zapis != null) zapis.maxTs + 1 else poczatekDoby
          nowe = fetchShard(od, koniecDoby)
          rekordy = scal(zapis?.rekordy, nowe)
          cache.zapisz(dzien, rekordy, kompletny = (koniecDoby < teraz))
          wynik += rekordy
  return wynik
```

Zasady odświeżania:

| doba | polityka |
|---|---|
| starsza niż wczoraj | **nigdy** nie pobieraj, jeśli plik istnieje (dane się nie zmieniają) |
| wczorajsza | dociągnij raz, po 00:10 (spool z centrali może dosłać zaległości) |
| dzisiejsza | dociągaj **tylko od `maxTs + 1`** |

Efekt dla zakresu 30 dni przy drugim wejściu: **1 żądanie** (dzisiejsza doba od
ostatniego punktu), ~0,3 MB dziennie przyrostu zamiast 8,1 MB.

### 6.3 Dodatkowo (tanie, niezależne od cache)

1. **Nie pobieraj historii, gdy nie jesteś na Wykresach.**
   `onResume()` → `if (page == 1) loadRange(rangeSec)`.
2. **Jedno zadanie na raz.** Trzymaj `Job?` i anuluj poprzednie przed nowym pobraniem.
3. **Pomiń odświeżenie, jeśli dane są świeże** (< 60 s) i zakres się nie zmienił.
4. **Pokaż częściowe błędy**: jeśli choć jedna doba padła, napisz
   „Załadowano N próbek · brak danych za 2 dni" zamiast milczeć.
5. **Resetuj `historyIndexFallback`** po 10 minutach, żeby jeden błąd nie psuł
   wydajności do końca procesu.
6. **Downsampling do rysowania**: dla zakresu ≥ 7 dni agreguj do 5-minutowych kubełków
   (min/max/średnia) — 43 200 punktów → 8 640, a wykres nie traci kształtu.

### 6.4 Oczekiwane oszczędności

| scenariusz | dziś | po zmianie |
|---|---|---|
| pierwsze wejście na 30 dni | 8,1 MB / 150 żądań | 8,1 MB / 150 żądań (bez zmian) |
| powrót do aplikacji (30 dni) | 8,1 MB / 150 żądań | **0,3 MB / 1 żądanie** |
| przełączenie zakładki Wykresy | 8,1 MB | **0** |
| zmiana 30 dni → 7 dni | 1,9 MB | **0** (7 dni jest już w cache) |
| miesięcznie, 10 wejść dziennie | ~2,5 GB | **~150 MB** |

Uwaga na limit Firebase: plan Spark ma 10 GB pobranych danych miesięcznie. Dziś
samo otwieranie wykresów 30-dniowych może ten limit wyczerpać.

---

## 7. Jak sprawdzić to na urządzeniu (bez zgadywania)

Nie mam możliwości uruchomienia aplikacji, więc te punkty trzeba potwierdzić pomiarem:

1. **Liczba żądań.** Android Studio → Profiler → Network, albo proxy (mitmproxy).
   Wejdź na Wykresy z zakresem 30 dni → policz żądania do
   `akwarium-367be-default-rtdb...`. Oczekiwane: ~150 dziś, 1 po zmianie.
2. **Bajty.** Ten sam profiler: suma transferu. Oczekiwane ~8 MB dziś.
3. **Czas do pojawienia się wykresu** na LTE. Oczekiwane 30–60 s dziś.
4. **Test indeksu.** W konsoli Firebase → Rules: czy na
   `/piec/telemetry/1m/v1` jest `.indexOn: "ts"`. Jeśli nie ma — F5 i F3 działają
   w wariancie „pełne shardy".
5. **Wersja.** W aplikacji „Więcej → Oprogramowanie" wiersz *APLIKACJA WEB PRO*
   (to `BuildConfig.VERSION_NAME`). Powinno być `v0.31.19` — jeśli jest `v0.31.18`,
   paczka v3.50 nie weszła do builda.

---

## 8. Pytania otwarte

1. ~~Czy 0.31.19 różni się w `RtdbClient.kt` / `AppModel.kt` od 0.31.11?~~
   **Zamknięte:** 0.31.19 = 0.31.18 + paczka v3.50 (tylko dźwięki alarmów).
   Kod historii jest ten sam, co w 0.31.11 — sprawdzone na 22 patchach z `android-sync/`.
2. **Czy archiwum solarne z centrali ma być widoczne w aplikacji?**
   Jeśli tak, to osobny temat: trzeba ustalić, czy czytać z Firebase'owego
   odpowiednika `/solar_daily`, czy dodać endpoint w centrali.
3. **Czy zakres 30 dni jest w ogóle potrzebny na wykresach?** Przy 14 seriach
   i braku downsamplingu to najdroższy widok w aplikacji.

## 9. Wdrożone: v0.31.20 (22.10.2026)

Paczka `SterownikCO-Android-v0.31.20-HISTORIA-CACHE.zip` (69 741 B,
sha256 `ec8b8854…`, Drive `1Y8v8p9ARgXgzyY_YNT5xzl0JHVNUpYYj`), zbudowana
**na drzewie 0.31.19**, które użytkownik przysłał jako źródła
(`android-0.31.19-zrodla.zip`, 1 111 974 B). Potwierdzone w nim:
`versionCode = 3025`, `versionName = "v0.31.19"`, 89 plików `.kt`,
`AlarmSoundSync.kt` obecny — czyli nakładka v3.50 faktycznie weszła do builda.

### 9.1 Co jest w paczce

| plik | rodzaj |
|---|---|
| `app/build.gradle.kts` | 3025 → **3026**, `v0.31.19` → **`v0.31.20`** |
| `core/TelemetryCache.kt` | **nowy** — dzienny cache JSON w `filesDir/telemetry/v1/` |
| `core/Telemetry.kt` | `data class HistoryFetch` (punkty + liczniki dni) |
| `core/RtdbClient.kt` | `fetchTelemetry` czyta/zapisuje cache, dociąga przyrostowo, raportuje błędy dni |
| `core/AppModel.kt` | `loadRange(sec, force)`, anulowanie `Job`, 60 s blokady, historia tylko na Wykresach |
| `ui/screens/Charts.kt` | przycisk odświeżania → `force = true` |
| `test/…/TelemetryCacheTest.kt` | **nowy** — 8 testów JVM (`org.json` jest już w `testImplementation`) |

### 9.2 Zasady ruchu sieciowego

1. dzień **bez pliku** → pobranie **całego** dnia (nie wycinka zakresu) i zapis,
2. dzień **bieżący (UTC)** → tylko rekordy nowsze od `maxTs` z pliku,
3. dzień **wczorajszy** → jedno pełne domknięcie na proces, **scalanie, nigdy podmiana**,
4. dzień **zamknięty** → zero sieci.

Punkt 1 jest krytyczny: pobranie skróconego dnia przy zakresie 24 h zatrułoby
pamięć i po przełączeniu na 30 dni brakowałoby próbek. Sprawdzone testem
„24 h nie zatruwa 7 dni".

### 9.3 Status znalezisk

| id | status |
|---|---|
| F1 brak cache | **naprawione** |
| F2 `onResume()` bezwarunkowo | **naprawione** |
| F3 brak anulowania pobrania | **naprawione** |
| F4 cichy `catch` per doba | **naprawione** (`daysFailed` / `daysStale`) |
| F5 `historyIndexFallback` do końca procesu | **naprawione** (TTL 10 min; koniec z `contains("400")`) |
| F6 brak downsamplingu | **otwarte** — osobny etap, zmienia wygląd serii |
| F7 `no-store` | bez zmian (brak `OkHttp.Cache`) |
| F8 `/solar_daily` nieczytane | **otwarte** — pytanie 2 w sekcji 8 |
| F9 brak informacji o źródle | **naprawione** („7 dni z pamięci”, „brak 2 dni”) |
| F10 numer wersji z `BuildConfig` | bez zmian, ale numer podbity |

### 9.4 Czego nie udało się sprawdzić przed wysłaniem

W sandboxie **nie ma JDK ani Android SDK** (`which java` → brak), więc
`assembleDebug` i `testDebugUnitTest` muszą wykonać się na laptopie użytkownika.
Wykonane zamiast tego:

- kontrola balansu nawiasów i stringów w 5 plikach (lexer obsługujący raw stringi,
  interpolację `${…}`, komentarze) — OK,
- port polityki cache do Pythona, **10 scenariuszy**: zimny start 7 dni, drugie
  wejście bez transferu, 3 nowe minuty, awaria sieci na dniu z cache, dzień bez
  cache, prune 45 dni, klucze UTC, roundtrip NaN/sim/stateKnown, 24 h → 7 dni,
  wczoraj domykane raz — **wszystko OK**,
- sprawdzenie, że użyte API istnieją w tym drzewie: `String.format(Locale, …)`,
  `lowercase(Locale)`, `catch (_: …)`, `sumOf`, `maxOfOrNull` — wszystkie już
  używane gdzie indziej w projekcie.

## 10. Wdrożone: v0.31.21 — auto-stop terminala DLOG (22.10.2026)

### 10.1 Skąd ta zmiana

Po analizie ruchu Firebase (§ 5) użytkownik wybrał **wariant A**: rytm statusu
zostaje bez zmian — `delay(8000)` w `AppModel.kt:245` (dziś 252) oraz
`BACKGROUND_STATUS_POLL_MS = 60_000` w `AlarmMonitorService.kt:398`. Z całej
piątki wariantów (A–E) do wykonania została **jedna** rzecz: otwarty arkusz
„Terminal diagnostyczny" odpytywał `/piec/dlog/<uid>/<root>/<session>`
co 5 s **bez żadnego warunku końcowego**.

Skala: 720 zapytań na godzinę to więcej niż cały polling statusu w tym samym
czasie (450 zapytań/h przy 8 s). Terminal zostawiony otwarty na noc = ~5 800
zapytań bez żadnej korzyści.

### 10.2 Co dokładnie zmieniono

| plik | linia (0.31.21) | zmiana |
|---|---|---|
| `core/AppModel.kt` | 42–48 | `private const val TERMINAL_DLOG_IDLE_STOP_MS = 5L * 60_000L` |
| `core/AppModel.kt` | 1642–1645 | pola `terminalDlogLastTouch: Long`, `terminalDlogIdleStopped: Boolean` |
| `core/AppModel.kt` | 1762 | `startTerminalDlogPolling(restart: Boolean = false)` |
| `core/AppModel.kt` | 1775–1780 | warunek bezczynności **przed** każdym pollem |
| `core/AppModel.kt` | 1798 | `fun terminalTouch()` — podtrzymanie i wznowienie |
| `core/AppModel.kt` | 1808 | `stopTerminalDlogPolling()` czyści `terminalDlogIdleStopped` |
| `ui/screens/sheets/System.kt` | 44–45, 498–512 | `Modifier.pointerInput` na korzeniu `TerminalSheet` |
| `ui/screens/Dashboard.kt` | 176–178, 191–194 | podpis kafelka SERWO w procentach (`flapLabel`, `syberkaLabel`) |
| `app/build.gradle.kts` | 15–16 | `versionCode 3027`, `versionName "v0.31.21"` |

Zegar: `SystemClock.elapsedRealtime()` (monotoniczny, odporny na zmianę czasu
w systemie). Nie używamy `System.currentTimeMillis()`.

Zdarzenia dotknięć są obserwowane w `PointerEventPass.Initial` i **nie są
konsumowane** — przewijanie konsoli DLOG działa dokładnie jak wcześniej.

### 10.3 Zachowanie po zmianie

| sytuacja | efekt |
|---|---|
| nikt nie dotyka ekranu | po 305 s ostatni poll, potem stop (61 zapytań) |
| dotknięcie / przewinięcie | licznik wznawia się; po stopie — odbiór wraca |
| zamknięcie arkusza | `stopTerminalDlogPolling()` (bez zmian) |
| powrót z tła na arkusz | start od zera (bez zmian) |
| pauza wyświetlania (`termPaused`) | **nadal** podlega auto-stopowi — pauza dotyczy tylko bufora lokalnego, sieci nie |
| stan logów na centrali | **bez zmian** — nie wołamy `terminalRemoteOff()` |

Świadoma decyzja: auto-stop jest **wyłącznie lokalny**. Wyłączenie zdalnych
logów na ESP32 zmieniłoby stan urządzenia bez kliknięcia użytkownika, a przy
tym ucina dane, które i tak trafiają do chmury.

### 10.4 Wyścig domknięty przed wysłaniem

`terminalTouch()` woła `startTerminalDlogPolling(restart = true)`. Bez flagi
`restart` dotknięcie w klatce, w której korutyna właśnie się domykała, trafiało
na `isActive == true` i kończyło się `return` — panel zostałby martwy do
zamknięcia arkusza. `restart = true` anuluje dobiegający job i startuje nowy.
Scenariusz T5 w symulacji sprawdza dokładnie ten przypadek.

### 10.5 Paczka

| | wartość |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.21-HISTORIA-CACHE.zip` |
| rozmiar | 92 568 B |
| sha256 | `491f5632e25221df6d45d6eb693850a6c1708d782376b77a75c950a9da9b5400` |
| Drive | `1fA781swQrZFQ-G5-0MAihAWuEoBbxaZc` |
| wpisy w ZIP | 11 (README + diff + 9 plików źródłowych) |
| katalog w repo | `android-sync/v0.31.21-terminal-idle-stop/` |

Paczka jest **kumulatywna względem 0.31.19** — zawiera całe pliki, więc
nadpisuje również zmiany z 0.31.20. Kto ma 0.31.20, może wgrać tę samą paczkę
bez cofania czegokolwiek.

### 10.6 Czego nie udało się sprawdzić

W sandboxie **nie ma JDK, Android SDK ani Gradle** — `assembleDebug`
i `testDebugUnitTest` muszą wykonać się na laptopie użytkownika. Wykonane
zamiast tego:

- `kcheck.py` (balans nawiasów/stringów, lexer z raw stringami i `${…}`)
  na `AppModel.kt`, `System.kt`, `RtdbClient.kt`, `Telemetry.kt`,
  `TelemetryCache.kt`, `Charts.kt` — **OK**,
- port logiki auto-stopu do Pythona, **6 scenariuszy** (T1–T6) —
  **wszystko OK**,
- kontrola zawartości ZIP: rozpakowanie do czystego katalogu i `cmp` każdego
  z 8 plików względem źródła — **identyczne**.

### 10.7 Linki

- podgląd: https://drive.google.com/file/d/1fA781swQrZFQ-G5-0MAihAWuEoBbxaZc/view
- pobranie: https://drive.google.com/uc?id=1fA781swQrZFQ-G5-0MAihAWuEoBbxaZc&export=download

Do kosza poszły: 0.31.20 (`1Y8v8p9ARgXgzyY_YNT5xzl0JHVNUpYYj`) oraz pierwsza
wersja 0.31.21 bez procentów (`1fPuWsl1nJNmT9vV1uB-uqXoXQ11n4yBz`) — żeby nie
dało się pobrać złego pliku. Wersja 0.31.21 ma **ten sam numer**, ale inną
zawartość: 92 568 B zamiast 84 769 B.

### 10.8 Kafelek SERWO w procentach (dopisane po zgłoszeniu z 22.10)

Użytkownik zakreślił na zrzucie podpis kafelka: `klapa 180° · syberek 90°`.
Duża liczba obok (`AUTO (100%)`) już była w procentach — w stopniach był tylko
podpis, czyli dwie różne jednostki w jednym kafelku.

| | przed | po |
|---|---|---|
| `Dashboard.kt` — klapa | `"klapa ${S.klapa}°"` | `"klapa $flapLabel"` → `klapa 100%` |
| `Dashboard.kt` — syberek | `"syberek ${S.syberka}°"` | `"syberek $syberkaLabel"` → `syberek 100%` |
| przelicznik | — | `Math.round(klapa * 100 / 180)`, `Math.round(syberka * 100 / 90)` |

Sprawdzone wartości brzegowe: 0°→0%, 45°→25%, 90°→50%, 179°→99%, 180°→100%;
syberek 0°→0%, 45°→50%, 89°→99%, 90°→100%. `Math.round` w Kotlinie zaokrągla
połówki w górę — dla 91° klapy wychodzi 51%, nie 50%.

**Zakres — decyzja użytkownika: tylko ten kafelek.** Arkusz „Serwo" zostaje
bez zmian (procent duży, kąt mały), a podpis „cel … °" przy wysyłaniu komendy
też zostaje w stopniach, bo tam kąt jest informacją o celu, nie o odczycie.

## 11. Bufor offline z aplikacji — firmware v3.54.0 + Android v0.31.22 (22.10.2026)

### 11.1 Punkt wyjścia

Mechanizm buforowania telemetrii przy braku internetu (runtime Spool G2) istniał
od R301–R334, ale był dostępny **wyłącznie z Telegrama**:

| komenda | działanie |
|---|---|
| `/spoolrt` | raport: ARMED/OFF, ring, strony, próbki zapisane/odtworzone |
| `/spoolrt TAK` | uzbrojenie (`g_spoolRuntimeArmed = true`) |
| `/spoolrt STOP` | wyłączenie (strony na LittleFS zostają) |

Trzy blokady dla aplikacji: brak `spoolrt` na liście `wykonajKomendePanelu()`,
brak stanu w `/piec/status`, flaga tylko w RAM (OFF po każdym restarcie).

### 11.2 Firmware v3.54.0

| miejsce | zmiana |
|---|---|
| `wykonajKomendePanelu()` | `spoolrt_wl` / `spoolrt_wyl` (ta sama funkcja co Telegram) |
| `zbudujJsonStatusPelny()` | `spool_rt`, `spool_pages`, `spool_spill`, `spool_replay` |
| `spoolRuntimePersistArmed()` | `Preferences` namespace `spoolg`, klucz `rt_on`, `putBool` |
| `spoolRuntimeRestoreFromNvs()` | `setup()` po `g_telemetryRing.init()` |
| `FIRMWARE_VERSION` | `v3.54.0` |

Zapis do NVS **tylko przy zmianie stanu** — jeden bajt na kliknięcie.
Polityka zapisu bez zmian: offline ≥120 s lub ring ≥120 próbek, cooldown 30 s,
replay 10 próbek, quota i rezerwaty LittleFS nietknięte.

Payload statusu +~90 B (limit `FB_JSON_CAP = 3072`).

### 11.3 Android v0.31.22

Nowy arkusz `Ustawienia → Bufor offline` (`ui/screens/sheets/Offline.kt`):
przełącznik + liczniki (strony, próbki zapisane, próbki odtworzone).
Stan czytany **wyłącznie** ze `spool_rt`; brak pola = stary firmware =
przełącznik nieaktywny z wyjaśnieniem.

### 11.4 Paczki

| | Android | Firmware |
|---|---|---|
| plik | `SterownikCO-Android-v0.31.22-BUFOR-OFFLINE.zip` | `SterownikCO-v3.54.0-OFFLINE-BUFFER.zip` |
| rozmiar | 108 500 B | 813 533 B |
| sha256 | `1bb4f02ec9aa575b474ef6961684b530384b807a4fb8808d1dac3f6f581bfbe8` | `77092fe0efbdc3b0d715e4434966d3b7475de03271506bddd40c0cff393d67df` |
| katalog | `android-sync/v0.31.22-offline-buffer/` | `firmware-patch/v3.54.0-offline-buffer-toggle/` |

### 11.5 Czego nie sprawdzono

Brak kompilatora ESP32 i brak JDK — build musi wykonać użytkownik.
Wykonane: balans nawiasów całego `main_centrala.cpp` identyczny z v3.53.1
(różnica 0), każda wstawka asertowana liczbą wystąpień, `kcheck.py` na
6 plikach Kotlina, kontrola obu ZIP-ów przez rozpakowanie i `cmp`.
