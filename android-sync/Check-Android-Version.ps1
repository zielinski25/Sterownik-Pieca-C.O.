# Check-Android-Version.ps1
#
# Po co: ustalic, KTORY kod aplikacji Android masz u siebie, bez zgadywania po
# numerze na ekranie. Skrypt wypisuje versionName/versionCode oraz sumy SHA256
# czterech plikow odpowiedzialnych za pobieranie danych z Firebase.
#
# Uruchomienie (w katalogu android/):
#   powershell -ExecutionPolicy Bypass -File ..\android-sync\Check-Android-Version.ps1
# albo z podaniem sciezki:
#   powershell -ExecutionPolicy Bypass -File Check-Android-Version.ps1 -ProjectDir C:\...\android
#
# Porownaj wynik z tabela "Wersja z repozytorium" ponizej. Rozna suma SHA256
# = inny plik = inne zachowanie (np. inny cache historii).

param(
    [string]$ProjectDir = "."
)

$ErrorActionPreference = "Stop"

function Get-Sha256([string]$Path) {
    if (-not (Test-Path $Path)) { return "BRAK PLIKU" }
    return (Get-FileHash -Path $Path -Algorithm SHA256).Hash.ToLower()
}

$root = Resolve-Path $ProjectDir
Write-Host ""
Write-Host "Katalog projektu: $root"
Write-Host ""

$gradle = Join-Path $root "app/build.gradle.kts"
if (Test-Path $gradle) {
    $txt = Get-Content $gradle -Raw
    $m1 = [regex]::Match($txt, 'versionName\s*=\s*"([^"]+)"')
    $m2 = [regex]::Match($txt, 'versionCode\s*=\s*(\d+)')
    Write-Host ("versionName : " + $(if ($m1.Success) { $m1.Groups[1].Value } else { "?" }))
    Write-Host ("versionCode : " + $(if ($m2.Success) { $m2.Groups[1].Value } else { "?" }))
} else {
    Write-Host "versionName : nie znaleziono app/build.gradle.kts"
}
Write-Host ""

$files = [ordered]@{
    "app/build.gradle.kts"                                    = (Join-Path $root "app/build.gradle.kts")
    "core/RtdbClient.kt"                                      = (Join-Path $root "app/src/main/java/com/sterownikco/pro/core/RtdbClient.kt")
    "core/AppModel.kt"                                        = (Join-Path $root "app/src/main/java/com/sterownikco/pro/core/AppModel.kt")
    "core/Telemetry.kt"                                       = (Join-Path $root "app/src/main/java/com/sterownikco/pro/core/Telemetry.kt")
}

# Wersja z repozytorium (v0.31.11, commit 5fe2eb2 i wczesniejsze).
$expected = @{
    "app/build.gradle.kts" = "62d0098a3fbbcc9cc62cfa7fa7c77b28d001db50c9fd02a7cf4aa8b415d4abba"
    "core/RtdbClient.kt"   = "76fb2c890dbb005a87ff5d575c0425405ea0082e24556c33d268e8db8e8f5581"
    "core/AppModel.kt"     = "3154ea8cc193df503802d1adc1e4ce4c001948aba158efe609f6a7047ac067a6"
    "core/Telemetry.kt"    = "959e009ffe5afd63aa3a76b42379022c0ff631fefb0cc34c4f718c290ac57474"
}

Write-Host "Plik                          SHA256 (Twoj)                                                      zgodny z v0.31.11"
Write-Host "---------------------------------------------------------------------------------------------------------------"
foreach ($key in $files.Keys) {
    $hash = Get-Sha256 $files[$key]
    $same = if ($expected.ContainsKey($key) -and $hash -eq $expected[$key]) { "TAK" } else { "NIE" }
    Write-Host ("{0,-28}  {1}  {2}" -f $key, $hash, $same)
}
Write-Host ""
Write-Host "Jesli wszystko TAK: Twoj kod to v0.31.11 z repozytorium, mimo innego numeru"
Write-Host "na ekranie telefonu (numer mogl pochodzic z lokalnego builda bez zapisu do repo)."
Write-Host "Jesli NIE: podeślij te pliki albo wynik tego skryptu - analiza zostanie powtorzona."
Write-Host ""
