[CmdletBinding()]
param(
    [string]$Preset = "windows-msvc-direct2d-capture",
    [switch]$SkipFullSuite
)

$ErrorActionPreference = "Stop"
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $repo
try {
    Write-Host "[AveMotion Part 19] Configure: $Preset"
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

    Write-Host "[AveMotion Part 19] Build: $Preset"
    cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

    Write-Host "[AveMotion Part 19] Direct2D capture validation"
    ctest --preset $Preset `
        -R "avemotion\.(capture\.metrics|direct2d\.(header|contract|smoke|capture_preflight|capture))" `
        -V --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Direct2D capture validation failed." }

    if (-not $SkipFullSuite) {
        Write-Host "[AveMotion Part 19] Full Telegram + Direct2D suite"
        ctest --preset $Preset --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "Full CTest suite failed." }
    }

    $artifactRoot = Join-Path $repo "out/build/$Preset/direct2d-capture-artifacts"
    $manifest = Join-Path $artifactRoot "capture_manifest.tsv"
    $summary = Join-Path $artifactRoot "README.txt"
    if (-not (Test-Path $manifest)) {
        throw "Expected capture manifest was not produced: $manifest"
    }
    if (-not (Test-Path $summary)) {
        throw "Expected capture summary was not produced: $summary"
    }

    $manifestRows = (Get-Content -LiteralPath $manifest -Encoding UTF8).Count - 1
    if ($manifestRows -ne 75) {
        throw "Expected 75 capture rows, found $manifestRows."
    }
    $bitmapCount = (Get-ChildItem -LiteralPath $artifactRoot -Recurse -Filter *.bmp).Count
    if ($bitmapCount -ne 225) {
        throw "Expected 225 capture bitmaps, found $bitmapCount."
    }

    $failedRows = Select-String -LiteralPath $manifest -Pattern "`tFAIL$"
    if ($failedRows) {
        throw "Capture manifest contains failed rows."
    }

    Write-Host "[AveMotion Part 19] PASS"
    Write-Host "Capture manifest: $manifest"
    Write-Host "Capture images:   $bitmapCount BMP files"
}
finally {
    Pop-Location
}
