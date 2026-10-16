$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Chart-Inspector-Notice.diff'
$chartsPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\Charts.kt'
$notifyPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AlarmNotify.kt'

if (-not (Test-Path $patchPath)) {
    throw 'Patch file not found. Run this script from the repository checkout.'
}
foreach ($path in @($chartsPath, $notifyPath)) {
    if (-not (Test-Path $path)) {
        throw "Required Android source file is missing: $path"
    }
}

$charts = Get-Content -Raw -LiteralPath $chartsPath
$notify = Get-Content -Raw -LiteralPath $notifyPath
$chartOld = $charts.Contains('Text("INSPEKTOR"') -and $charts.Contains('private fun inspectorText(')
$chartNew = -not $charts.Contains('Text("INSPEKTOR"') -and -not $charts.Contains('private fun inspectorText(')
$noticeOld = $notify.Contains('RTDB jest aktualny.')
$noticeNew = $notify.Contains(' w tle.')

if ($chartNew -and $noticeNew) {
    Write-Host 'Already applied: the chart inspector row is removed and the service notice is updated.' -ForegroundColor Green
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
            throw 'Could not apply the chart and notification patch.'
        }
    }
    finally {
        Pop-Location
    }

    $charts = Get-Content -Raw -LiteralPath $chartsPath
    $notify = Get-Content -Raw -LiteralPath $notifyPath
    if ($charts.Contains('Text("INSPEKTOR"') -or
        $charts.Contains('private fun inspectorText(') -or
        -not $notify.Contains(' w tle.')) {
        throw 'Post-apply verification failed. Review the two source files before building.'
    }
    Write-Host 'Done: removed the chart inspector row and updated the service notice.' -ForegroundColor Green
}
else {
    throw 'A partial or unexpected source state was found. No files were changed.'
}
