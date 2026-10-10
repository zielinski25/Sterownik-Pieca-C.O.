$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Chart-Cleanup-Debug-Label.diff'
$chartsPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\Charts.kt'
$appModelPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AppModel.kt'
$notifyPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AlarmNotify.kt'
$debugStringsPath = Join-Path $repoRoot 'android\app\src\debug\res\values\strings.xml'

if (-not (Test-Path $patchPath)) {
    throw 'Patch file not found. Run this script from the repository checkout.'
}
foreach ($path in @($chartsPath, $appModelPath, $notifyPath, $debugStringsPath)) {
    if (-not (Test-Path $path)) {
        throw "Required Android source file is missing: $path"
    }
}

$charts = Get-Content -Raw -LiteralPath $chartsPath
$appModel = Get-Content -Raw -LiteralPath $appModelPath
$notify = Get-Content -Raw -LiteralPath $notifyPath
$debugStrings = Get-Content -Raw -LiteralPath $debugStringsPath

$chartOld = $charts.Contains('m.chartPhase()') -and
    $charts.Contains('dotknij = kursor') -and
    $charts.Contains('DOTKNIJ kursor') -and
    $appModel.Contains('fun chartPhase(): String')
$chartNew = -not $charts.Contains('m.chartPhase()') -and
    -not $charts.Contains('dotknij = kursor') -and
    -not $charts.Contains('DOTKNIJ kursor') -and
    -not $appModel.Contains('fun chartPhase(): String')
$noticeOld = $notify.Contains('BuildConfig.DEBUG') -and
    $debugStrings.Contains('Sterownik CO (TEST)')
$noticeNew = -not $notify.Contains('BuildConfig.DEBUG') -and
    $debugStrings.Contains('Sterownik CO (DEBUG)') -and
    -not $debugStrings.Contains('Sterownik CO (TEST)')

if ($chartNew -and $noticeNew) {
    Write-Host 'Already applied: chart status/help removed and Debug branding no longer says TEST.' -ForegroundColor Green
}
elseif ($chartOld -and $noticeOld) {
    Push-Location $repoRoot
    try {
        & git apply --check $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'git apply check failed; no source files were changed.'
        }
        & git apply $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not apply the chart cleanup and Debug-label patch.'
        }
    }
    finally {
        Pop-Location
    }

    $charts = Get-Content -Raw -LiteralPath $chartsPath
    $appModel = Get-Content -Raw -LiteralPath $appModelPath
    $notify = Get-Content -Raw -LiteralPath $notifyPath
    $debugStrings = Get-Content -Raw -LiteralPath $debugStringsPath
    if ($charts.Contains('m.chartPhase()') -or
        $charts.Contains('dotknij = kursor') -or
        $charts.Contains('DOTKNIJ kursor') -or
        $appModel.Contains('fun chartPhase(): String') -or
        $notify.Contains('BuildConfig.DEBUG') -or
        -not $debugStrings.Contains('Sterownik CO (DEBUG)') -or
        $debugStrings.Contains('Sterownik CO (TEST)')) {
        throw 'Post-apply verification failed. Review the four source files before building.'
    }
    Write-Host 'Done: chart phase/help removed; notifications no longer use the TEST label.' -ForegroundColor Green
}
else {
    throw 'A partial or unexpected source state was found. No files were changed.'
}
