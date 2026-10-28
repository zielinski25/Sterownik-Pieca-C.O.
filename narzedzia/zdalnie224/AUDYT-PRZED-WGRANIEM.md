# Audyt przed wgraniem v3.56.5 na pracujący piec (v3.55.1)

Cel audytu: skok v3.55.1 → v3.56.5 to siedem wersji, których nikt nie uruchomił
na sprzęcie, a piec jest 100 km od użytkownika. Sprawdzone pod kątem tego, co
dzieje się **przy starcie**, bo tam błąd odcina zdalny kontakt.

Zakres różnicy: **1 682 zmienione linie, 199 hunków**
(`diff v3.55.1/main_centrala.cpp v3.56.5/main_centrala.cpp`).

## Co się zmieniło — pięć obszarów

| obszar | wersja | charakter |
|---|---|---|
| `systemFS196` (BootFilesystem) zamiast `LittleFS` | v3.55.8 | zamiana nazwy w ~100 miejscach, semantyka ta sama |
| `history193::Engine` + `CheckedHistoryIO.h` | v3.55.5–v3.55.9 | zapis historii przez jedną warstwę z potwierdzaniem |
| `history194::Startup` + `history195::Boot` + A1 | v3.55.6 | kontrola startu plików CSV, protokół resetu |
| nadzorca zadań (`NadzorDecyzja`) | v3.55.2 | restart po 5 min milczenia zadania |
| trasa `/history*` + panel + bilety | v3.56.0–v3.56.5 | nowa funkcja pobrania |

## Sprawdzone — i bezpieczne

### 1. Historia NIE zostanie skasowana przy pierwszym starcie

`Transition.h`, funkcja `run()`:

* marker ukończenia A1 **jest** → `release()`, komentarz w kodzie:
  `// NEVER delete history on this branch`
* marker **nie ma**, plik żądania **nie ma** → `Result::Existing`, **zero kasowania**
* kasowanie **tylko** gdy istnieje `/littlefs/history_reset_a1.req`
  o dokładnej treści `RESET-HISTORY-A1\n`

Na piecu użytkownika nie ma ani markera, ani żądania → pierwszy start v3.56.5
wpada w gałąź `Existing`. Nic nie jest usuwane. To potwierdza wcześniejszą
odpowiedź o partycjach, ale teraz na poziomie kodu, nie rozumowania o flashu.

### 2. Awaria LittleFS nie odcina WiFi ani Telegrama

`BootFilesystem::finish(false)` → `phase_ = Blocked` → `selected()` zwraca
**pusty** `fs::FS`. Każda operacja plikowa kończy się bezpiecznie błędem, ale
sieć, RTC, czujniki i Telegram działają dalej. Brak tu ścieżki do paniki
ani do restartu w pętli.

### 3. Nadzorca zadań nie zrestartuje pieca przy awarii internetu

`g_tgTaskHeartbeatMs` / `g_tgPollHeartbeatMs` są ustawiane **na początku każdej
iteracji pętli zadania**, niezależnie od wyniku sieci:

```cpp
void tgTaskFn(void*) { tgTaskBeat(0); for (;;) { tgTaskBeat(1); ... } }
void tgPollTaskFn(void*) { tgPollTaskBeat(0); for (;;) { tgPollTaskBeat(0); ...
```

Brak WiFi → pętla się kręci → znacznik odświeżany → nadzór milczy.
Restart polega na **zablokowaniu samego zadania** (zawieszone TLS/TCP),
a nie na braku internetu. Do tego 3 min rozbiegu po starcie i limit
3 restartów pod rząd (`NADZOR_MAX_RESTARTOW`), licznik zerowany po 30 min pracy.

### 4. Nowy klucz w `secrets.h` nie blokuje kompilacji

```cpp
#ifndef SECRET_HISTORIA_TOKEN
  #define SECRET_HISTORIA_TOKEN ""
#endif
```

Brak klucza = pusty token = trasa odpowiada 503. Użytkownik ma ten klucz,
więc dostanie 200.

## JEDNO realne ryzyko — i jak je sprawdzić w 20 sekund

`HistoryStartup.h` przy każdym starcie wymaga, żeby **oba pliki CSV kończyły się
znakiem nowej linii**:

```cpp
if(ok && s.st_size){ char last=0; ... read(fd,&last,1) ...
  else if(last!='\n') ok=fail(Reason::Boundary); }
```

Jeśli którykolwiek plik kończy się w połowie wiersza → `State::Blocked`.
Skutek: `/history` odpowiada 503, `/historia` na Telegramie mówi
„historia zablokowana; plik zachowany". **Stanu nie da się cofnąć zdalnie** —
trzeba poprawić plik przy komputerze albo wykonać reset A1 (kasuje historię).

Jak prawdopodobne: mało. Zapis w v3.55.1 szedł całymi wierszami
(`f.print(linia)`, a `historiaLiniaCsv()` zawsze kończy `csv += '\n'`).
Połowa wiersza może zostać tylko po utracie zasilania w trakcie zapisu
albo po zapełnieniu dysku.

**Sprawdzenie przed wgraniem** (na pliku CSV, który użytkownik ma z Telegrama):

```powershell
$f = "$env:USERPROFILE\Downloads\hist_short (7).csv"
$b = [System.IO.File]::ReadAllBytes($f)
"ostatni bajt: {0} (0x{0:X2})" -f $b[-1]
if ($b[-1] -eq 10) { "OK - konczy sie znakiem nowej linii" } else { "UWAGA - NIE konczy sie; nie wgrywac bez poprawki" }
```

To samo dla `hist_long (7).csv`. `10` = `\n` = wszystko w porządku.

## Co obserwować po wgraniu

| moment | co powinno przyjść na Telegram |
|---|---|
| po restarcie | komunikat potwierdzenia partycji OTA |
| `/version` | `v3.56.5` |
| `/historia` | 3 dokumenty: `hist_short.csv`, `hist_long.csv`, `log_ustawien.txt` |
| po 5–10 min | cisza. Gdyby przyszło „restart z nadzoru" — zgłosić |

Jeśli po wgraniu **nie ma żadnej wiadomości** — to jeszcze nie wyrok: piec mógł
wrócić na v3.55.1 przez rollback (wtedy `/version` pokaże v3.55.1) albo
utknąć w pętli restartów. W obu przypadkach potrzebny jest ktoś przy urządzeniu.

## Wniosek

Firmware jest **bezpieczniejszy niż wyglądał**: historia nie ginie, sieć nie
zależy od LittleFS, nadzorca nie reaguje na brak internetu. Zostaje jedno
sprawdzenie przed wgraniem — znak nowej linii na końcu CSV.

Ale: **zysk z wgrania teraz jest zerowy** dla osoby 100 km od pieca. Nowa funkcja
to przyciski w panelu WWW, dostępne tylko w sieci domowej. Pobieranie historii
przez Telegram działa już na v3.55.1.

Rozsądna kolejność: sprawdzić końcówkę CSV → jeśli OK, wgrać przy okazji
(na miejscu albo zdalnie, gdy jest ktoś z USB pod ręką) → **nigdy nie wgrywać
zdalnie bez planu B przy urządzeniu**.

---

## Poprawione polecenie sprawdzające (PowerShell 5.1)

Pierwsza wersja nie zadziałała z dwóch powodów:

1. pliki leżą na Dysku Google, nie w `Pobrane`,
2. czat zamienia `[System.IO.File]` na link markdown — wklejony kod był
   niepoprawny składniowo (`[[System.IO](http://System.IO).File]`).

Wersja poniżej nie zawiera **ani jednego nawiasu kwadratowego** (typ bajtowy
odczytujemy przez `Get-Content -Encoding Byte`, ostatni bajt przez `.GetValue()`),
więc czat nie ma czego zepsuć, i szuka plików w całym profilu użytkownika:

```powershell
$pliki = Get-ChildItem "$env:USERPROFILE" -Filter "hist_*.csv" -Depth 4 -ErrorAction SilentlyContinue |
         Sort-Object LastWriteTime -Descending
if (-not $pliki) {
  Write-Host "Brak hist_*.csv na tym komputerze." -ForegroundColor Red
  Write-Host "Pobierz oba pliki z Dysku Google (przycisk Pobierz) i uruchom jeszcze raz." -ForegroundColor Yellow
} else {
  $pliki | Select-Object -First 5 Name, Length, LastWriteTime, DirectoryName | Format-List
  foreach ($g in "hist_short","hist_long") {
    $f = $pliki | Where-Object { $_.Name -like "$g*" } | Select-Object -First 1
    if (-not $f) { Write-Host ("brak " + $g) -ForegroundColor Yellow; continue }
    $b = Get-Content -Path $f.FullName -Encoding Byte -ReadCount 0
    $ost = $b.GetValue($b.Length - 1)
    if ($ost -eq 10) { Write-Host ("OK    " + $f.Name) -ForegroundColor Green }
    else { Write-Host ("UWAGA " + $f.Name + " - ostatni bajt " + $ost) -ForegroundColor Red }
  }
}
```

`10` = `\n` = plik kompletny. Każda inna wartość = nie wgrywać zdalnie.
