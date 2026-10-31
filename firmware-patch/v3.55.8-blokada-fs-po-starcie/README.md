# Kandydat v3.55.8 — NIE WGRYWAĆ

324 miejsca zwykłego dostępu Arduino LittleFS w głównym CPP przechodzą przez bramkę. Po nieudanym starcie195 pozostaje ona zamknięta. To **nie** pełna izolacja awarii podczas późniejszej pracy ani dowód poprawności na ESP.

A1 nadal może usunąć dwa stare CSV tylko na istniejące poprawne żądanie. Program nie tworzy żądania i nie czyści staging. Nie aktywuj A1 na urządzeniu na podstawie tej paczki.

Wymagane jest wszystkich siedem nagłówków obok CPP, w tym nowy BootFilesystem.h. Przykład secrets.h.example nie zawiera prywatnych sekretów. Pełen zakres i ograniczenia: `narzedzia/blokada-startu-fs196/README.md`.

Pierwsza linia: zmiana v3.55.7 → v3.55.8. Stała wersji: linia 5556.

## PowerShell5.1 — tylko kontrola kopii, bez kompilacji/wgrywania
Rozpakuj ZIP w Pobranych do ETAP-196-BLOKADA-FS-PO-BLEDZIE-STARTU.

```powershell
$p = Join-Path $env:USERPROFILE 'Downloads\ETAP-196-BLOKADA-FS-PO-BLEDZIE-STARTU\firmware-patch\v3.55.8-blokada-fs-po-starcie\main_centrala.cpp'
if (!(Test-Path -LiteralPath $p)) { throw "Najpierw rozpakuj ZIP do wskazanego katalogu." }
$hash = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()
if ($hash -ne '3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31') { throw "Niezgodna suma pliku" }
Select-String -LiteralPath $p -Pattern 'static const char\* FIRMWARE_VERSION ='
Write-Host "SUMA ZGODNA — kandydat v3.55.8; NIE WGRYWAC"
```
