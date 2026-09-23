[CmdletBinding()]
param(
    [string]$Preset = "windows-msvc-direct2d",
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
    Write-Host "[AveMotion Part 18.1] Configure: $Preset"
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

    Write-Host "[AveMotion Part 18.1] Build: $Preset"
    cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

    Write-Host "[AveMotion Part 18.1] Focused Direct2D validation"
    ctest --preset $Preset -R "avemotion\.direct2d\.(header|contract|smoke)" -V --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Direct2D validation failed." }

    if (-not $SkipFullSuite) {
        Write-Host "[AveMotion Part 18.1] Full standalone suite"
        ctest --preset $Preset --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "Full CTest suite failed." }
    }

    $artifact = Join-Path $repo "out/build/$Preset/direct2d-artifacts/avemotion-direct2d-warp.ppm"
    if (-not (Test-Path $artifact)) {
        throw "Expected WARP artifact was not produced: $artifact"
    }
    Write-Host "[AveMotion Part 18.1] PASS"
    Write-Host "WARP reference image: $artifact"
}
finally {
    Pop-Location
}
