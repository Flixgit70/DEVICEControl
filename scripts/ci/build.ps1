param(
    [string]$Preset = "windows-release"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $Root

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset $Preset --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
