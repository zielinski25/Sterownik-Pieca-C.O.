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

Rozmiar: 30 dni × ~150 B ≈ 4,5 MB na dysku (mniej niż 8,1 MB z sieci, bo bez `age`
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
