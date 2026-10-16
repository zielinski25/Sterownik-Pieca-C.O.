$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Fullscreen-Chart-Layout.diff'
$chartsPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\Charts.kt'

if (-not (Test-Path $patchPath) -or -not (Test-Path $chartsPath)) {
    throw 'Patch or chart source file not found. Run this script from the repository checkout.'
}

$charts = Get-Content -Raw -LiteralPath $chartsPath
$oldLayout = $charts.Contains('Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(20.dp))') -and
    $charts.Contains('Modifier.fillMaxWidth().height(320.dp).clip(RoundedCornerShape(12.dp))') -and
    $charts.Contains('modifier = Modifier.fillMaxWidth().height(320.dp)') -and
    $charts.Contains('fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color.White')
$newLayout = $charts.Contains('Modifier.fillMaxSize().background(Pal.Surface, RoundedCornerShape(20.dp))') -and
    $charts.Contains('Modifier.fillMaxWidth().weight(1f).clip(RoundedCornerShape(12.dp))') -and
    $charts.Contains('view = view, modifier = Modifier.fillMaxSize()') -and
    $charts.Contains('maxLines = 1') -and
    $charts.Contains('overflow = TextOverflow.Ellipsis')

if ($newLayout) {
    Write-Host 'Already applied: the fullscreen chart fills the available dialog height.' -ForegroundColor Green
}
elseif ($oldLayout) {
    Push-Location $repoRoot
    try {
        & git apply --check $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'git apply check failed; no source files were changed.'
        }
        & git apply $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not apply the fullscreen chart layout patch.'
        }
    }
    finally {
        Pop-Location
    }

    $charts = Get-Content -Raw -LiteralPath $chartsPath
    if (-not $charts.Contains('Modifier.fillMaxSize().background(Pal.Surface, RoundedCornerShape(20.dp))') -or
        -not $charts.Contains('Modifier.fillMaxWidth().weight(1f).clip(RoundedCornerShape(12.dp))') -or
        -not $charts.Contains('view = view, modifier = Modifier.fillMaxSize()') -or
        -not $charts.Contains('maxLines = 1')) {
        throw 'Post-apply verification failed. Review Charts.kt before building.'
    }
    Write-Host 'Done: fullscreen chart now expands to fill available height.' -ForegroundColor Green
}
else {
    throw 'A partial or unexpected fullscreen layout was found. No files were changed.'
}
