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
