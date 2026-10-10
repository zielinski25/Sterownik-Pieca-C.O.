$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Remove-Debug-App-Name.diff'
$stringsPath = Join-Path $repoRoot 'android\app\src\debug\res\values\strings.xml'
$oldText = '<string name="app_name">Sterownik CO (DEBUG)</string>'
$newText = '<string name="app_name">Sterownik CO</string>'

if (-not (Test-Path $patchPath) -or -not (Test-Path $stringsPath)) {
    throw 'Patch or Debug app-name resource not found. Run this script from the repository checkout.'
}

$strings = Get-Content -Raw -LiteralPath $stringsPath
if ($strings.Contains($newText) -and -not $strings.Contains($oldText)) {
    Write-Host 'Already applied: the visible DEBUG suffix is removed.' -ForegroundColor Green
}
elseif ($strings.Contains($oldText) -and -not $strings.Contains($newText)) {
    Push-Location $repoRoot
    try {
        & git apply --check $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'git apply check failed; no source files were changed.'
        }
        & git apply $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not apply the app-name patch.'
        }
    }
    finally {
        Pop-Location
    }

    $strings = Get-Content -Raw -LiteralPath $stringsPath
    if (-not $strings.Contains($newText) -or $strings.Contains($oldText)) {
        throw 'Post-apply verification failed. Review strings.xml before building.'
    }
    Write-Host 'Done: the visible DEBUG suffix is removed.' -ForegroundColor Green
}
else {
    throw 'An unexpected app-name value was found. No files were changed.'
}
