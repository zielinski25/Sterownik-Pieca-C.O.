# Lokalne AI + agent sprzątający dysk — Windows 11

Dla Twojego laptopa: **AMD Ryzen 7 PRO 4750U, 16 GB RAM, Radeon zintegrowany
(496 MB VRAM)**. Wniosek na wstępie: **karta graficzna nie pomoże** — zintegrowany
Radeon nie ma pamięci, w której zmieściłby się model. Cała praca idzie na CPU
(8 rdzeni / 16 wątków). To nie jest problem, bo agent jest zbudowany tak, żeby
model był **opcjonalny**: decyzje podejmują reguły, a model ocenia tylko to, co
naprawdę sporne.

---

## 1. Co dostajesz

```
narzedzia/sprzatanie/
├── reguly.py          <- JEDYNY plik, który edytujesz (progi, wyjątki)
├── skaner.py          <- mierzy dysk, nic nie rusza
├── wykonawca.py       <- przenosi do kwarantanny po potwierdzeniu
├── agent.py           <- spina całość, pyta o zgodę
├── ollama_klient.py   <- lokalny model (czysty Python, bez pip install)
├── test_skaner.py     <- 6 testów na atrapie dysku
├── instaluj.ps1       <- instaluje Ollamę + model + skrót na pulpit
└── sprzataj.ps1       <- launcher (klik z pulpitu)
```

Zero zależności zewnętrznych. Cały kod to biblioteka standardowa Pythona.
Nic nie jest wysyłane do internetu — Ollama słucha tylko na `127.0.0.1`.

---

## 2. Pobranie kodu na laptopa (krok, który pominąłem w pierwszej wersji)

Kod leży w repozytorium na GitHubie — na Twoim laptopie go jeszcze **nie ma**.

### Wariant A (pewny): pobierz w przeglądarce

Kliknij albo wklej do Chrome/Edge:

```
https://github.com/zielinski25/Sterownik-Pieca-C.O./archive/refs/heads/arena/a6b30642-sterownik-pieca-c-o.zip
```

Zapisze się `Sterownik-Pieca-C.O.-arena-a6b30642-sterownik-pieca-c-o.zip`.
Potem: **prawy klawisz → Extract All** (albo „Wyodrębnij wszystko") i jako
miejsce wskaż Pulpit. Powstanie folder o tej samej nazwie.

### Wariant B: PowerShell (jeśli wolisz terminal)

```powershell
cd $HOME
Invoke-WebRequest -Uri "https://github.com/zielinski25/Sterownik-Pieca-C.O./archive/refs/heads/arena/a6b30642-sterownik-pieca-c-o.zip" -OutFile "$HOME\Desktop\sprzatanie.zip" -UseBasicParsing
Expand-Archive "$HOME\Desktop\sprzatanie.zip" -DestinationPath "$HOME\Desktop" -Force
```

> `codeload.github.com` u Ciebie zwrócił 404, choć z zewnątrz odpowiada 200.
> Dlatego wariant B używa `github.com/.../archive/...` — inna ścieżka, ten sam plik.

### Sprawdzenie, że masz dobre pliki

```powershell
cd "$HOME\Desktop\Sterownik-Pieca-C.O.-arena-a6b30642-sterownik-pieca-c-o\narzedzia\sprzatanie"
Get-ChildItem
```

Musisz zobaczyć **9 plików**: `agent.py`, `reguly.py`, `skaner.py`,
`wykonawca.py`, `ollama_klient.py`, `test_skaner.py`, `instaluj.ps1`,
`sprzataj.ps1`, `PRZEWODNIK.md`. Jeśli ich nie widzisz — **nie idź dalej**.

> Całe repo waży 13,7 MB, bo zawiera też dokumentację sterownika pieca.
> Agencja potrzebuje tylko folderu `narzedzia\sprzatanie` — resztę możesz
> zostawić albo skasować, na działanie nie ma wpływu.

---

## 3. Instalacja (15 minut, większość to pobieranie modelu)

Będąc w folderze `narzedzia\sprzatanie`:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\instaluj.ps1
```

Gdy PowerShell zapyta *„Do you want to change the execution policy?"* — wciśnij
**`Y`** i Enter. To zgoda **tylko na tę jedną sesję okna**, nie zmiana systemowa;
po zamknięciu PowerShell wszystko wraca do stanu wyjściowego.

Instalator sprawdzi RAM i miejsce, zainstaluje Ollamę przez winget, pobierze
`qwen2.5:7b-instruct-q4_K_M` (~4,7 GB) i zrobi skrót na pulpicie.
Jeśli nie masz Pythona: `winget install Python.Python.3.12`.

> **Wklejaj komendy pojedynczo.** PowerShell wykonuje wklejony blok linia po
> linii — gdy pierwsza się nie uda, reszta i tak idzie dalej i komunikaty
> zaczynają się mieszać. Jedna komenda → Enter → patrz na wynik → następna.

---

## 4. Kolejność uruchomień — dokładnie tak

**Krok 1. Tylko raport.** Nic nie rusza, pokazuje co leży i ile waży.

```powershell
python agent.py --tylko-raport
```

Przeczytaj wynik. Jeśli coś wygląda znajomo i potrzebnie — dopisz do `reguly.py`
(sekcja `NIGDY_FRAGMENTY`). To jest cała konfiguracja.

**Krok 2. Sprzątanie bez modelu.** Same reguły, 10–20 sekund.

```powershell
python agent.py --bez-ai
```

Zobaczysz listę grup i pytanie: *„Przenieść X GB do kwarantanny? [t/N]"*.
Dopiero po `t` cokolwiek się dzieje.

**Krok 3. Z modelem.** Gdy chcesz, żeby AI oceniło stare archiwa i duże pliki.

```powershell
python agent.py
```

Na CPU: 1–3 minuty dla ~40 pozycji. Model **nie może nic usunąć** — dostaje
tylko nazwy, rozmiary i wiek, zwraca JSON z ocenami, które widzisz w raporcie.

**Cofnięcie:**

```powershell
python agent.py --przywroc
```

---

## 5. Co jest bezpieczne, a co nie (Twoje 954 GB, 165 GB zajęte)

| Grupa | Ryzyko | Akcja | Co to jest |
|---|---|---|---|
| `cache_i_temp` | 1 | kwarantanna | Cache Chrome/Edge, `%TEMP%`, CrashDumps. Programy odtworzą same. |
| `kosz_systemowy` | 1 | **raport** | Skrypt nie ma praw do cudzych koszy — opróżnij ręcznie. |
| `katalogi_builda` | 2 | kwarantanna | `node_modules`, `.venv`, `target`, `dist` — nie ruszane od 30 dni. |
| `stare_instalki` | 2 | kwarantanna | `.exe`/`.msi` w Pobranych starsze niż 30 dni. |
| `duplikaty` | 2 | kwarantanna | Ten sam SHA-256. Zostaje **najstarszy** egzemplarz. |
| `stare_archiwa` | 3 | **raport** | ZIP/RAR/ISO starsze niż 180 dni. Mogą być jedyną kopią. |
| `duze_pliki` | 3 | **raport** | >500 MB. Wideo, obrazy dysków, VM. Decyzja Twoja. |

Ryzyko 3 **nigdy nie jest przenoszone automatycznie** — nawet z `--auto`.

### Czego agent nie dotknie nigdy

- `OneDrive`, `Google Drive`, `Dropbox`, `iCloud Drive` — usunięcie lokalne
  usuwa plik u wszystkich urządzeń
- `.git` — historia repo
- `C:\Windows`, `Program Files`, `ProgramData`, `AppData\Roaming`
- dokumenty (`.docx`, `.pdf`, `.xlsx`, zdjęcia) — to dane, nie śmieci

Każda próba jest sprawdzana **dwa razy**: raz przy skanie, raz przy przenoszeniu.

---

## 6. Gdzie lądują pliki

```
%LOCALAPPDATA%\SprzatanieDysku\
├── kwarantanna\2026-11-27_094309\cache_i_temp\...   <- pliki przeniesione
└── raporty\
    ├── plan_2026-11-27_094309.json        <- pełny raport ze skanu
    └── manifest_2026-11-27_094309.json    <- skąd dokąd poszedł każdy plik
```

Kwarantanna jest **na tym samym dysku**, więc przenoszenie jest natychmiastowe
i nic nie zajmuje dodatkowo. Miejsce zwalnia się po opróżnieniu:

```powershell
Remove-Item -Recurse -Force "$env:LOCALAPPDATA\SprzatanieDysku\kwarantanna\2026-11-27_094309"
```

Zalecenie: **tydzień karencji**. Jeśli przez tydzień nic nie zginęło — kasuj.
Manifest zostaje, więc wiesz, co zostało usunięte.

---

## 7. Model: co realnie działa na 4750U

| Model | Rozmiar w RAM | Szybkość na tym CPU | Do czego |
|---|---|---|---|
| `qwen2.5:3b-instruct-q4_K_M` | ~2,3 GB | 10–14 tok/s | szybkie decyzje, krótkie odpowiedzi |
| **`qwen2.5:7b-instruct-q4_K_M`** | **~4,9 GB** | **4–6 tok/s** | **domyślny — dobrze trzyma JSON** |
| `qwen2.5:14b-instruct-q4_K_M` | ~9 GB | 2–3 tok/s | możliwe, ale zagraza 16 GB RAM |
| `llama3.2:3b` | ~2,2 GB | 10–13 tok/s | alternatywa, słabszy polski |

Na 16 GB RAM **nie wchodź w 14B+** — Windows zje 4–6 GB, model 9 GB, zostaje
za mało na przeglądarkę i zacznie się swap (spadek do 1 tok/s).

Zmiana modelu:

```powershell
ollama pull qwen2.5:3b-instruct-q4_K_M
python agent.py --model qwen2.5:3b-instruct-q4_K_M
```

---

## 8. „Chcę agenta, który robi więcej" — jak rozszerzać

`reguly.py` to zwykłe listy. Przykład: sprzątanie cache Visual Studio (często
kilkanaście GB na laptopie programisty):

```python
CACHE_W_LOCALAPPDATA += [
    "Microsoft/VisualStudio/*/ComponentModelCache",
    "Microsoft/VisualStudio/*/Extensions",
    "NuGet/v3-cache",
    "JetBrains/*/caches",
]
```

Przykład: własny katalog, który ma być nietykalny:

```python
NIGDY_FRAGMENTY.append("/moje-faktury")
```

Po zmianie uruchom testy — sprawdzają, czy nie zepsułeś ochrony:

```powershell
python test_skaner.py
```

Testy działają na **atrapie dysku** w katalogu tymczasowym, nie na Twoich plikach.

---

## 9. Bezpieczeństwo — co ten kod robi i czego nie robi

**Robi:**

- przenosi (`shutil.move`), nigdy nie kasuje
- zapisuje manifest przed każdym ruchem
- pyta o zgodę przed każdą zmianą (bez `--auto`)
- sprawdza ścieżki chronione dwa razy
- trzyma model offline, na `127.0.0.1`

**Nie robi:**

- nie wysyła nazw plików do chmury (Ollama jest lokalna)
- nie dotyka rejestru, usług, harmonogramu zadań
- nie uruchamia się sama — nie ma żadnego zadania w tle
- nie ma uprawnień administratora (i nie potrzebuje)

**Ograniczenie, które musisz znać:** kwarantanna leży na tym samym dysku.
Jeśli zabraknie miejsca na dysku, przenoszenie też się nie uda — dlatego
najpierw skanuj z `--tylko-raport` i patrz na liczby.

---

## 10. Co dalej (jeśli chcesz więcej niż sprzątanie)

Ten sam stos (Ollama + skrypt w Pythonie) obsłuży kolejne zadania bez
instalowania czegokolwiek nowego:

- **porządki w repo** — ten projekt ma 17 plików `.md` w katalogu głównym;
  agent z `--korzen .` wskaże duplikaty i nieaktualne raporty
- **przeszukiwanie dokumentów** — Ollama ma model embeddingów
  (`ollama pull nomic-embed-text`), można zrobić lokalne „zapytaj moje PDF-y"
- **asystent w VS Code** — rozszerzenie Continue/Ollama podłącza się do
  `127.0.0.1:11434` i działa bez internetu

Każde z tych zadań korzysta z tego samego modelu — pobierasz go raz.
