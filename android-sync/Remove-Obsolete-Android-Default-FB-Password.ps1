$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$prefsPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\Prefs.kt'
if (-not (Test-Path -LiteralPath $prefsPath)) {
    throw "Nie znaleziono Androidowego pliku Prefs.kt: $prefsPath"
}

$text = [System.IO.File]::ReadAllText($prefsPath)
$pattern = '(?m)^[ \t]*const val DEFAULT_FB_PASS[ \t]*=[^\r\n]*(?:\r?\n|$)'
$matches = [System.Text.RegularExpressions.Regex]::Matches($text, $pattern)
if ($matches.Count -gt 1) {
    throw 'Znaleziono więcej niż jedną deklarację DEFAULT_FB_PASS; przerwano bez zmian.'
}
if ($matches.Count -eq 1) {
    $updated = [System.Text.RegularExpressions.Regex]::Replace($text, $pattern, '', 1)
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($prefsPath, $updated, $utf8NoBom)
    Write-Host 'Usunięto nieużywaną deklarację DEFAULT_FB_PASS.'
} else {
    Write-Host 'Nieużywana deklaracja DEFAULT_FB_PASS nie występuje; bez zmian.'
}
