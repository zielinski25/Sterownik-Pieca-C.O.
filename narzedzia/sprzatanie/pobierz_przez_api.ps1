# =============================================================================
#  POBIERANIE PRZEZ api.github.com  (omija codeload.github.com, ktory jest u Ciebie blokowany)
#
#  Dlaczego tak: github.com/.../archive/....zip PRZEKIEROWUJE na codeload.github.com,
#  a to jest dokladnie ten adres, ktory zwraca u Ciebie 404. api.github.com
#  odpowiada bezposrednio (HTTP 200, zero przekierowan) i zwraca pliki w base64.
#
#  Uzycie:
#    .\pobierz_przez_api.ps1                 # pobiera do .\sprzatanie
#    .\pobierz_przez_api.ps1 -Cel C:\Agent   # inny katalog
#    .\pobierz_przez_api.ps1 -Testuj         # sprawdza API, nic nie zapisuje
# =============================================================================
[CmdletBinding()]
param(
    [string]$Cel = "$PWD\sprzatanie",
    [switch]$Testuj
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$repo = "zielinski25/Sterownik-Pieca-C.O."
$galaz = "arena/a6b30642-sterownik-pieca-c-o"
$katalog = "narzedzia/sprzatanie"
$api = "https://api.github.com/repos/$repo/contents/$katalog?ref=$galaz"

function Pobierz-Json($url) {
    $naglowki = @{ "User-Agent" = "SprzatanieAgenta"; "Accept" = "application/vnd.github+json" }
    return Invoke-RestMethod -Uri $url -Headers $naglowki -UseBasicParsing -TimeoutSec 60
}

Write-Host "`nTestuje api.github.com..." -ForegroundColor Cyan
try {
    $lista = Pobierz-Json $api
} catch {
    Write-Host "API nie odpowiada: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Siec blokuje tez api.github.com - uzyj wariantu z Dyskiem Google." -ForegroundColor Yellow
    exit 1
}

$pliki = @($lista | Where-Object { $_.type -eq 'file' })
Write-Host ("  API dziala. Plikow w katalogu: " + $pliki.Count) -ForegroundColor Green
$pliki | ForEach-Object { Write-Host ("    - " + $_.name + "  (" + $_.size + " B)") }

if ($Testuj) {
    Write-Host "`n-Testuj: nic nie zapisano. Gdyby wszystko gra, uruchom bez -Testuj." -ForegroundColor Gray
    exit 0
}

if ($pliki.Count -lt 9) {
    Write-Host "  Oczekiwalem 10 plikow, jest $($pliki.Count). Przerywam." -ForegroundColor Red
    exit 1
}

Write-Host "`nPobieram do: $Cel" -ForegroundColor Cyan
New-Item -ItemType Directory -Path $Cel -Force | Out-Null

$ok = 0
foreach ($p in $pliki) {
    try {
        # pobieramy pojedynczy plik - API zwraca base64, wiec polskie znaki przechodza nietkniete
        $d = Pobierz-Json $p.url
        $bajty = [Convert]::FromBase64String(($d.content -replace "`n", ""))
        $sciezka = Join-Path $Cel $p.name
        [IO.File]::WriteAllBytes($sciezka, $bajty)

        # kontrola: rozmiar musi sie zgadzac z deklarowanym
        $rozmiar = (Get-Item $sciezka).Length
        if ($rozmiar -ne $p.size) {
            Write-Host ("    [!] " + $p.name + ": $rozmiar B zamiast " + $p.size) -ForegroundColor Yellow
        } else {
            Write-Host ("    [ok] " + $p.name + "  ($rozmiar B)") -ForegroundColor Green
            $ok++
        }
    } catch {
        Write-Host ("    [x] " + $p.name + ": " + $_.Exception.Message) -ForegroundColor Red
    }
}

Write-Host "`nPobrano $ok z $($pliki.Count) plikow." -ForegroundColor Cyan

if ($ok -lt 9) {
    Write-Host "Za malo plikow - nie uruchamiaj agenta. Wyslij mi ten komunikat." -ForegroundColor Red
    exit 1
}

Write-Host @"

Gotowe. Teraz:
  cd "$Cel"
  python agent.py --tylko-raport     # nic nie rusza
  python agent.py --bez-ai           # sprzatanie po Twoim 't'

"@ -ForegroundColor Gray

if ((Read-Host "Uruchomic teraz raport? [t/N]") -match '^(t|tak|y)$') {
    Set-Location $Cel
    & python agent.py --tylko-raport
}
