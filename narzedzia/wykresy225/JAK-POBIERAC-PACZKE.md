# Jak pobierać paczki Androida (i jak nie dać się oszukać)

Zapisane po zdarzeniu z 29.10.2026, kiedy pobranie z Dysku Google urwało się
w połowie, a `zbuduj.ps1` po cichu zbudował **starą wersję**.

## Zasada numer jeden

**Sprawdź rozmiar i sumę SHA256, ZANIM rozpakujesz.** Nie po rozpakowaniu,
nie po zbudowaniu — przed.

```powershell
(Get-Item $z).Length
(Get-FileHash $z -Algorithm SHA256).Hash
```

Rozmiar musi się zgadzać co do bajtu. Suma musi się zgadzać co do znaku.
Jeśli nie — pobierz jeszcze raz. Nie „spróbuj może zadziała".

## Co poszło nie tak 29.10.2026

Objaw w konsoli:

```
100  904k    0  904k    0     0   452k      0  --:--:--  0:00:02
New-Object : Exception calling ".ctor" with "3" argument(s):
  "Nie można odnaleźć rekordu końca katalogu centralnego."
```

Co to znaczy po ludzku:

1. Plik ma 1 327 341 B, a przyszło 926 000 B — **Drive zamknął połączenie**.
   curl napisał „100%", bo serwer nie podał rozmiaru w nagłówku, więc curl
   nie miał z czym porównać. Procent kompletności nic tu nie znaczy.
2. „Nie można odnaleźć rekordu końca katalogu centralnego" = ZIP nie ma
   końca. To nie jest uszkodzony ZIP — to jest **ucięty** ZIP.
3. `Expand-Archive` padł, więc folder `...v0.31.45` nie powstał.
4. `cd` do nieistniejącego folderu nie zmienił katalogu — PowerShell został
   w starym folderze `...v0.31.43`.
5. `zbuduj.ps1` uczciwie wypisał `== Wersja projektu: 0.31.43` i zbudował
   starą wersję. Wszystkie zadania `UP-TO-DATE`, 14 sekund.

Wniosek: **`UP-TO-DATE` przy budowaniu paczki, którą właśnie pobrałeś, jest
czerwonym światłem.** Nowa paczka nigdy nie jest „up-to-date" — Gradle nie ma
skąd wziąć wyniku.

## Kolejność, która nie zawodzi

Wklejaj **po jednej linii**, każdą z Enterem. Wklejenie czterech naraz
w jednym bloku sklei je (w konsoli widać `>>`) i druga linia wykona się mimo
błędu pierwszej.

```powershell
$u = "https://raw.githubusercontent.com/zielinski25/Sterownik-Pieca-C.O./arena/8d4e1b71-sterownik-pieca-c-o/android-sync/v0.31.45-ostateczna/SterownikCO-Android-v0.31.45-PELNY-PROJEKT.zip"
$z = "$env:USERPROFILE\Downloads\SterownikCO-0.31.45.zip"
curl.exe -L --retry 5 --retry-delay 2 -o $z $u
(Get-Item $z).Length
(Get-FileHash $z -Algorithm SHA256).Hash -eq "A02D8D603FE8DC55B970472B54D92CF5DDC49BAE8A964E5D6A34B8462BAB48BA"
```

Oczekiwane: `1327341` i `True`. Cokolwiek innego — powtórz `curl.exe`.

Dopiero potem:

```powershell
Expand-Archive $z -DestinationPath "$env:USERPROFILE\Downloads\SterownikCO-ANDROID" -Force
cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.45"
(Select-String -Path app\build.gradle.kts -Pattern 'versionName').Line
powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1
```

Przed budowaniem `Select-String` musi wypisać `versionName = "0.31.45"`.

## Skąd brać pliki

| źródło | kiedy | uwagi |
|---|---|---|
| `raw.githubusercontent.com/.../<gałąź>/...` | **domyślnie** | zwykły plik statyczny, bez strony pośredniej, wznawialny |
| Dysk Google (`uc?id=...&export=download`) | awaryjnie | potrafi urwać połączenie i pokazać „100%" |
| `github.com/.../releases` | **nie dla Androida** | ESP sprawdza najnowsze wydanie i szuka `firmware.bin` — wydanie bez firmware'u psuje OTA |

## Jak sprawdzić, że APK jest właściwy

- nazwa pliku z numerem: `SterownikCO-v0.31.45-debug.apk`
- rozmiar ok. 12,8–12,9 MB
- w konsoli `zbuduj.ps1`: `== Wersja projektu: 0.31.45 (versionCode 31045)`
- w aplikacji: Ustawienia → jest „Bufor offline"; Pulpit → SERWO w procentach

Jeśli którakolwiek z tych rzeczy się nie zgadza — nie instaluj, tylko napisz.
