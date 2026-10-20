# Narzędzia (Windows)

## `Porzadki-Pulpitu.ps1` — porządki na pulpicie

Zbiera porozrzucane po pulpicie pliki sterownika w jednym folderze
`Pulpit\SterownikCO`. **Niczego nie usuwa**, tylko przenosi. Uruchomiony bez
parametrów pokazuje sam plan, a przenosi dopiero z `-Wykonaj`.

```powershell
$porzadki = "$env:USERPROFILE\SterownikCO\narzedzia\Porzadki-Pulpitu.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -File $porzadki            # podgląd
powershell -NoProfile -ExecutionPolicy Bypass -File $porzadki -Wykonaj   # przeniesienie
```

| Co leży na pulpicie | Dokąd trafia |
|---|---|
| najnowszy `Piec*.html`, `main_centrala*.cpp`, `firmware*.bin`, `SterownikCO*.apk` | `Pulpit\SterownikCO` |
| starsze pliki tych rodzajów (także starsze niż plik, który już był w `SterownikCO`) | `Pulpit\SterownikCO\archiwum` |
| `Cenatrala*.zip`, `SterownikCO*.zip`, `main_centrala*.zip` | `Pulpit\SterownikCO\archiwum` |
| stare kopie projektu PlatformIO centrali, czyli foldery, które **same** zawierają `platformio.ini` z `[env:esp32-s3-n16r8]` | `Pulpit\SterownikCO\archiwum` |
| wszystko inne (zdjęcia, skróty, dokumenty, inne foldery) | zostaje na miejscu, skrypt tylko to wypisuje |

O tym, który plik jest „najnowszy”, decyduje data modyfikacji. Gdy w miejscu
docelowym jest już element o tej samej nazwie, przenoszony dostaje dopisek
` (2)`, ` (3)`… i nic nie zostaje nadpisane. Ponowne uruchomienie po
porządkach wypisze `Nic do przeniesienia`. Opcja `-Pulpit <folder>` porządkuje
inny folder niż pulpit bieżącego użytkownika.

`android\Zbuduj-APK.ps1` i `firmware-patch\v3.38.0-local-html\Wgraj-Firmware.ps1`
też zapisują swoje wyniki do `Pulpit\SterownikCO`, więc bałagan nie powinien
wracać.

Workflow `.github/workflows/narzedzia-windows.yml` testuje skrypt na Windows
(Windows PowerShell 5.1 i PowerShell 7) na sztucznym pulpicie. Pulpit ma
starsze i nowsze wersje plików, kolizję nazw w archiwum, nawiasy `[]` w nazwie
pliku i prywatny folder z projektem w podfolderze, który musi zostać na miejscu.

**Wynik testu (2026-10-15, GitHub Actions `windows-latest`):** podgląd w Windows
PowerShell 5.1 niczego nie zmienił. `-Wykonaj` przeniósł 12 elementów, w tym dwa
foldery projektu (v3.36.1 i v3.11.4). Przy kolizji nazwy w archiwum plik dostał
nazwę `Piec (1) (2).html`, a pięć elementów niezwiązanych ze sterownikiem zostało
na miejscu. Drugie uruchomienie w PowerShell 7 wypisało `Nic do przeniesienia`.
W tym samym przebiegu CI `Zbuduj-APK.ps1` zapisał APK i `Piec.html` do
`Pulpit\SterownikCO`, a `Wgraj-Firmware.ps1` zapisał tam `firmware_v3.47.1.bin`,
przenosząc starszy `firmware_v3.45.1.bin` do `archiwum`.


### Drugi etap: dodatkowe paczki i kopie

Po pierwszych porządkach można uruchomić **opcjonalny** szerszy podgląd:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File $porzadki -DodatkoweKopie
# Dopiero po sprawdzeniu planu i zamknięciu projektów w IDE/terminalach:
powershell -NoProfile -ExecutionPolicy Bypass -File $porzadki -DodatkoweKopie -Wykonaj
```

Do `SterownikCO\archiwum` trafią także:
- `Sterownik-*.apk` i `SterownikPieca*.apk` — wszystkie dodatkowe APK, **bez**
  wybierania najlepszej wersji i bez zastępowania `SterownikCO-debug.apk`;
- `Android-Chart-Fix-*.zip` i `Piec.html.*.bak`;
- foldery `piec-android-chartfix-*`, `piec-android-test`, `piec-co-final`,
  `piec-fw-v<cyfra>*`, `sterownik-co-android_v<cyfra>*` oraz
  `Sterownik-Pieca-C.O.-fresh-*`.

Nazwy tych folderów wskazują kopie sterownika, ale nie dowodzą, że projekt nie
jest obecnie używany. **Sprawdź plan przed wykonaniem.** Przeniesienie projektu
może wymagać ponownego otwarcia go w IDE z nowego miejsca (skróty i ścieżki nie
są przepisywane). Dodatkowe reguły pomijają junctiony i dowiązania symboliczne.
Foldery o niejasnych nazwach (`app`, `files`, `piec`, `Nowy folder`, `natan`) oraz
prywatne dokumenty, modele STL i skróty pozostają na miejscu, o ile nie pasują
do wcześniej opisanej reguły projektu PlatformIO centrali.

### Ostatni etap: pozostałe rozpoznane elementy

Opcja `-ResztaPulpitu` grupuje elementy potwierdzone podczas przeglądu pulpitu.
Nie usuwa niczego i nie traktuje projektów jako nieaktualnych:

| Element | Folder na pulpicie |
|---|---|
| foldery `app`, `piec` | `Projekty` |
| foldery `files`, `Marlin_Ender3V2S1_drukarka3D`, pliki `obj_*.stl` | `Druk3D` |
| folder `OFERTA`, pliki `TR.docx`, `Produkty promowane.txt`, `Nowy Dokument tekstowy.txt` | `Dokumenty` |
| folder `Nowy folder` (mieszane pliki) | `Do-przejrzenia` |

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File $porzadki -ResztaPulpitu
# Sprawdź plan i zamknij projekty oraz terminale korzystające z tych folderów:
powershell -NoProfile -ExecutionPolicy Bypass -File $porzadki -ResztaPulpitu -Wykonaj
```

Projekty są przenoszone **w całości**, w tym ukryte `.git` i pliki IDE. Nie są
zmieniane ich pliki konfiguracyjne ani absolutne ścieżki. Po przeniesieniu otwórz
projekt z nowej lokalizacji; GitHub Desktop może wymagać wskazania nowej ścieżki
repozytorium. Jeśli chcesz zachować ścieżkę któregoś projektu, nie wykonuj planu.
Skróty, folder `natan` oraz inne nierozpoznane elementy pozostają na miejscu.
Wyniki kolejnych kompilacji nadal trafiają do `SterownikCO`. Dotychczasowych
folderów docelowych nie przenosimy; przy kolizji nazwy używamy dopisku ` (2)`.
