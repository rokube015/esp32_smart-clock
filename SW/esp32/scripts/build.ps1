$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$idfProfile = 'C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1'
if (-not (Test-Path -LiteralPath $idfProfile)) {
    throw "ESP-IDF v6.1 environment profile not found: $idfProfile"
}
. $idfProfile
$env:PYTHONUTF8 = '1'
$idfVersion = Get-Content -LiteralPath (Join-Path $env:IDF_PATH 'tools\cmake\version.cmake') -Raw
if ($idfVersion -notmatch 'set\(IDF_VERSION_MAJOR 6\)' -or $idfVersion -notmatch 'set\(IDF_VERSION_MINOR 1\)') {
    throw 'This project requires ESP-IDF v6.1.'
}
if ((Get-Content -LiteralPath 'sdkconfig' -Raw) -notmatch 'CONFIG_IDF_TARGET="esp32s3"') {
    throw 'This project requires the configured ESP32-S3 target.'
}
& (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe') (Join-Path $env:IDF_PATH 'tools\idf.py') build
exit $LASTEXITCODE
