$ErrorActionPreference='Stop'
$id='1OeqD1TKT5_x8EWHg2SDoywpHzSDQY1Gi'
$sha='da3d3b6d4468df85b73f49bb6aad52a079f5f389e421000c0df96edd4e06d5ed'
$proj=$null
foreach($p in (Get-ChildItem -LiteralPath "$env:USERPROFILE\Documents\PlatformIO" -Directory -ErrorAction SilentlyContinue)){if(Test-Path -LiteralPath (Join-Path $p.FullName 'src\main_centrala.cpp')){$proj=$p.FullName;break}}
if(-not $proj){foreach($p in (Get-ChildItem -LiteralPath $env:USERPROFILE -Directory -Recurse -Depth 3 -ErrorAction SilentlyContinue)){if(Test-Path -LiteralPath (Join-Path $p.FullName 'src\main_centrala.cpp')){$proj=$p.FullName;break}}}
if(-not $proj){throw "Nie znalazlem projektu PlatformIO z src\main_centrala.cpp - podaj sciezke recznie."}
Write-Host "Projekt: $proj" -ForegroundColor Cyan
$zip=Join-Path $env:TEMP 'SterownikCO-v3.57.8.zip'
$tmp=Join-Path $env:TEMP 'SterownikCO-v3.57.8-rozpakowane'
if(Test-Path -LiteralPath $tmp){Remove-Item -LiteralPath $tmp -Recurse -Force}
Invoke-WebRequest -Uri "https://drive.google.com/uc?export=download&id=$id" -OutFile $zip -UseBasicParsing
$h=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLower()
if($h -ne $sha){throw "Zly plik: SHA256=$h (oczekiwano $sha). Pobierz ZIP recznie z linku."}
Write-Host "SHA256 zgodne z paczka 3.57.8" -ForegroundColor Green
Expand-Archive -LiteralPath $zip -DestinationPath $tmp -Force
$nowy=Join-Path $tmp 'src\main_centrala.cpp'
if(-not(Test-Path -LiteralPath $nowy)){throw "W paczce nie ma src\main_centrala.cpp"}
$cel=Join-Path $proj 'src\main_centrala.cpp'
Copy-Item -LiteralPath $cel -Destination ($cel+'.bak-3.57.7') -Force
Copy-Item -LiteralPath $nowy -Destination $cel -Force
$wersja=(Select-String -LiteralPath $cel -Pattern 'FIRMWARE_VERSION = "v([0-9.]+)"').Matches.Groups[1].Value
Write-Host "Podmienione. Wersja w pliku: v$wersja" -ForegroundColor Green
Write-Host "Kopia starego pliku: $cel.bak-3.57.7"
Write-Host "Teraz: pio run -t upload" -ForegroundColor Yellow
