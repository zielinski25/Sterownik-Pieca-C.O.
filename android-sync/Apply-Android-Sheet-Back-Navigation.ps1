$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Sheet-Back-Navigation.diff'
$chromePath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\components\Chrome.kt'

foreach ($path in @($patchPath, $chromePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required file is missing: $path"
    }
}

$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $chromePath
if ($source.Contains('BackHandler(onBack = onDismiss)')) {
    Write-Host 'System Back already closes the current app sheet.' -ForegroundColor DarkGreen
    exit 0
}
if (-not $source.Contains('fun Sheet(') -or
    -not $source.Contains('fullScreen: Boolean = false') -or
    $source.Contains('import androidx.activity.compose.BackHandler')) {
    throw 'Chrome.kt is in an unexpected state. No files were changed.'
}

Push-Location $repoRoot
try {
    & git apply --check $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw 'Back-navigation patch check failed; no source files were changed.'
    }
    & git apply $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not apply the sheet back-navigation patch.'
    }
}
finally {
    Pop-Location
}

$source = Get-Content -Raw -Encoding UTF8 -LiteralPath $chromePath
if (-not $source.Contains('BackHandler(onBack = onDismiss)')) {
    throw 'Post-apply verification failed. Review Chrome.kt before building.'
}
Write-Host 'Applied: Android Back closes the open sheet and returns to the app.' -ForegroundColor Green
