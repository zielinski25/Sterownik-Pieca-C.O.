# AUDYT TELEMETRII I APLIKACJI — 29.10.2026

Źródła: `akwarium-367be-default-rtdb-10-export (1).json` (29 dni, 40 789 próbek),
`…-29-export.json` (29.10, 680 próbek), log `piec_log (28)` i `(29)`.

---

## CZĘŚĆ I — CO NAPRAWDĘ DZIEJE SIĘ Z DANYMI

### 1. Bilans 29 dni

| Metryka | Wartość |
|---|---|
| Próbek w chmurze | **40 789** |
| Rozruchów (bootId) | 55 |
| Zdjętych zdarzeń utraty | **78** |
| Brakujących próbek | **202** |
| Procent strat | **0,50%** |

### 2. Rozkład strat — to nie jest jeden problem, to cztery

| Ile próbek zginęło | Ile razy | Przerwa w wykresie | Prawdopodobna przyczyna |
|---|---|---|---|
| **1** | **65×** | 2 min | nieznana — wymaga instrumentacji |
| 2 | 3× | 3 min | jak wyżej, dwa razy pod rząd |
| **10** | **7×** | **11 min** | **fałszywe potwierdzenie wysyłki** (rozmiar paczki) |
| 11 | 1× | 12 min | fałszywe potwierdzenie + 1 |
| 20 | 1× | 21 min | dwa fałszywe potwierdzenia |
| 30 | 1× | 31 min | trzy fałszywe potwierdzenia |

**Wszystkie duże straty są wielokrotnością 10** = `TELEMETRY_BATCH_MAX_RECORDS`.
To nie przypadek — to odcisk palca mechanizmu paczkowego.

### 3. Gdzie strata NIE powstaje (sprawdzone i wykluczone)

- **Ring nie przepełnia się.** Pojemność 1440 próbek = 24 h; najdłuższa awaria
  w danych to ~30 min. `telemetry_dropped = 0` na urządzeniu.
- **Porzucenie paczki nie gubi próbek.** `telemetryPendingAbandon()` nie dotyka
  ringu — potwierdzone w kodzie i w symulacji.
- **Spool nie jest winny.** `Samples spilled: 0`, `Pages created: 0`.
- **Restarty** gubią tylko to, co zostało w ringu w chwili restartu — a to widać
  jako koniec rozruchu, nie jako dziurę w środku.

### 4. Mechanizm dużych strat — potwierdzony symulacją

`telemetryFinalizeCompletedRequest()` zdejmuje 10 próbek z kolejki
(`commitPopN(batchCount)`) po otrzymaniu potwierdzenia sukcesu. Jeśli
potwierdzenie dotyczy **innego, wcześniejszego żądania** (spóźniona odpowiedź
po timeoucie), próbki znikają z kolejki, choć nigdy nie trafiły do chmury.

Symulacja (`symulacja.py`, 29 dni, 41 760 próbek):

| Model | Utracone próbki | Zgodność z obserwacją |
|---|---|---|
| kod obecny (bez błędu) | 28–58 (tylko restarty) | za mało |
| fałszywe potwierdzenie 12% | 2 189–2 614 | **50× za dużo** |
| **fałszywe potwierdzenie 0,17%** | **24–65** | **zgodne** (obserwacja: 70) |

**Wniosek:** mechanizm istnieje, jest realny, ale zdarza się raz na ~4 200 paczek.
Nie jest to „gubienie danych" na skalę, ale jest to **jedyne wyjaśnienie zgodne
z rozmiarem 10**.

**Poprawka:** przed `commitPopN` zapisywać, ile rekordów faktycznie poszło
(numer ostatniego potwierdzonego `seq`), i zdejmować tylko potwierdzony
zakres. To wymaga zmiany w firmware — patrz Część III.

### 5. Pojedyncze straty (65×) — czego NIE wiem

Fakty:
- `mono` sąsiadów różni się **dokładnie o 120 000 ms** → producent działał
  normalnie, brakuje jednej minuty.
- `seq` rośnie o 2 → numer został nadany, próbka weszła do ringu
  (`telemetry_produced` = `seq` = 6734, `telemetry_dropped` = 0).
- **W kodzie nie ma żadnej ścieżki, która zdejmuje z ringu dokładnie 1 próbkę.**
  Jedyne `commitPopN` wołane jest z `batchCount` (10) albo z rozmiarem strony
  spoola (do 128).

**Uczciwie: tego nie wyjaśniłem.** Żeby to rozstrzygnąć, trzeba dodać do
firmware licznik kontrolny i log przy każdej niezgodności. Wtedy po jednym
dniu będzie wiadomo.

### 6. „Dziury" widoczne w aplikacji 29.10 — to była inna sprawa

Kanał `a[3] = tOgrzSr` („Ogrz. śr.") ma `age = 65535` (brak odczytu) przez
**~20 minut po każdym restarcie pieca**. Przyczyna opisana w §15 raportu
spadków: `ostatniRazPompaPracowalaMs` jest zerowane przy starcie, a piec jest
zimny i pompa stoi.

To **nie jest utrata danych** — to kanał, który nie ma czym się wypełnić.
Ale wizualnie wygląda identycznie jak dziura.

---

## CZĘŚĆ II — AUDYT APLIKACJI

### 7. Jak aplikacja traktuje braki

- `age == 65535` → punkt pomijany (linia się przerywa). **Poprawnie.**
- Przerwa > 6 minut (`360_000 ms`) → zamknięcie odcinka. Krótsze przerwy
  aplikacja **łączy linią prostą**. Czyli pojedyncze straty (2 min) są
  **niewidoczne** — i to jest właściwe zachowanie.
- Odcinki symulowane rysowane linią kreskowaną. **Poprawnie.**

### 8. Znalezione problemy wydajnościowe (naprawione w 0.31.49)

| Problem | Skutek | Stan |
|---|---|---|
| `Brush.verticalGradient` tworzony dla każdego odcinka | setki shaderów na klatkę | **naprawione** |
| `PathEffect.dashPathEffect` dla każdego odcinka | alokacje | **naprawione** |
| `Stroke(...)` dla każdego odcinka | alokacje | **naprawione** |
| `hexColor()` dla każdego odcinka | parsowanie stringa | **naprawione** |
| `filterNotNull()+min()+max()` w pętli po wierszach (O(n²)) | zawieszenie w 7 dni | naprawione w 0.31.47 |
| Każda próbka = osobny `SegPoint` | zawieszenie w 30 dni | naprawione w 0.31.47 (koszyki min/max) |

### 9. Co jeszcze warto sprawdzić w aplikacji

- Scalanie pamięci lokalnej z chmurą (0.31.43) — jeśli pamięć ma próbki,
  których chmura nie ma, albo odwrotnie, mogą powstawać pozorne dziury.
  **Do zbadania, wymaga danych z aplikacji.**
- `chartSampleIndexAtTimeX` przy gęstych danych — skanowanie liniowe po
  wszystkich wierszach przy każdym ruchu palca. Dla 43 000 próbek to
  odczuwalne. **Kandydat na binary search.**

---

## CZĘŚĆ III — PLAN NAPRAWY

### Priorytet 1 — dziury „Ogrz. śr." (widoczne, denerwujące)
**Firmware 3.56.8.** Po restarcie piec nie wie, kiedy pompa ostatnio
pracowała, więc czeka pełne `Czas_OFF`. Poprawka: zapisywać znacznik
w NVS (albo inicjować „dawno temu"). Efekt: kanał dostępny od pierwszej próbki.

### Priorytet 2 — fałszywe potwierdzenie (10 próbek, 7× w 29 dni)
**Firmware.** Przed `commitPopN` porównać numer ostatniego `seq` w potwierdzonym
zakresie z tym, co faktycznie wysłano. Zdejmować tylko potwierdzony zakres.

### Priorytet 3 — pojedyncze straty (65×)
**Firmware — instrumentacja, nie poprawka na ślepo.** Dodać do heartbeat:
`produced`, `sent`, `ring.size()`, `pending`. Jeśli `produced - sent - ring - pending != 0`,
logować brakujące `seq`. Po jednym dniu będzie dowód.

### Priorytet 4 — wydajność aplikacji
**0.31.49 — gotowe.** Plus `binary search` w `chartSampleIndexAtTimeX`
jako osobna zmiana.

---

## CZĘŚĆ IV — Kontrola samego detektora (przed wgraniem 3.56.8)

Detektor mierzy coś, czego wcześniej nikt nie mierzył. Zanim uznamy jego liczby
za prawdę, trzeba wykluczyć, że sam produkuje fałszywe alarmy. Sprawdzone trzy
miejsca, w których mógłby kłamać.

### IV.1 Granica restartu — BRAK fałszywego alarmu (dowód z kodu)

`g_telemetryLastCloudSeq` to `static volatile uint32_t = 0` (linia 6229) i po
restarcie nie jest odtwarzane z NVS. Pierwsza udana paczka ustawia je na swoją
ostatnią próbkę (linia 40317), a porównanie z kolejką następuje **dopiero po
tym ustawieniu**. Kolejka (`TelemetryRing`) też jest w RAM, więc po starcie
jest pusta — `peek()` zwraca `false` i warunek w linii 40326 nie ma czego
porównywać.

`g_telemetrySampleSeq` również startuje od 0 (linia 20898), ale numeracja i tak
nie koliduje między bootami, bo klucz w chmurze to `BOOTID-0000SEQ`.

Wniosek: pierwsza paczka po każdym restarcie jest **bazą odniesienia**, nie
kandydatem na lukę. Detektor nie może zgłosić przerwy na starcie.

### IV.2 Zrzut do spoola — detektor tego NIE widzi (ograniczenie, nie błąd)

`spoolRuntimeMaybeSpill()` zdejmuje próbki z kolejki przez `commitPopN(count)`
(linia 40740), ale **nie aktualizuje** `g_telemetryLastCloudSeq`. To znaczy, że
po zrzucie następna normalna paczka zaczyna się od wyższego `seq` i ustawia
bazę na swoim tailu — przerwy więc nie zgłosi.

Skutek: jeżeli próbki poszły do spoola i **nigdy** nie wrócą do chmury, detektor
milczy. To jest realna luka pomiarowa. Nie ma jej jak usunąć bez przebudowy
księgowania, a przebudowa księgowania to dokładnie to P2, które świadomie
odłożyliśmy. Zapamiętane, nie naprawiane.

Warunek, żeby to w ogóle wystąpiło: `spool_rt = 1` w `/api/status`. Przy
wyłączonym buforze offline ta ścieżka nie działa.

### IV.3 Zapis do NVS przy każdym zaniku pompy — do obserwacji

P1 zapisuje `srOstPompa` przy każdej zmianie stanu pompy (linia 32236), a
`Preferences::putUInt` kończy się realnym zapisem do flasha. To nie jest
problem przy kilku cyklach na godzinę, ale przy pompie taktującej co minutę
byłby to niepotrzebny zużyty sektor.

Do sprawdzenia po dobie: `srOstPompa` ma się zmieniać **rzadko** (kilka razy
na dobę), nie co kilka minut. Jeśli będzie inaczej — dopiszemy próg czasowy
(zapis nie częściej niż raz na 10 min).

### IV.4 Oczekiwany wynik pierwszego startu po wgraniu

Klucz `srOstPompa` powstaje w NVS dopiero **przy pierwszym zaniku pompy po
wgraniu**. Na tym jednym starcie go jeszcze nie ma, więc dziura w `Ogrz. śr.`
wystąpi ostatni raz. Od następnego restartu — znika.


## PLIKI

- `audyt.py` — skrypt audytu (uruchamiany na eksporcie RTDB)
- `symulacja.py` — symulacja potoku telemetrii i weryfikacja hipotez
