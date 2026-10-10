$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$idfEnvironmentScript = "C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1"
$expectedIdfPath = "C:\esp\v6.1\esp-idf"
$expectedToolsPath = "C:\Espressif\tools"
$expectedPythonEnvironment = "C:\Espressif\tools\python\v6.1\venv"
$idfProjectPath = Join-Path $projectRoot "SW\esp32"

if (-not (Test-Path -LiteralPath $idfEnvironmentScript -PathType Leaf)) {
    Write-Error "ESP-IDF environment initialization script not found: $idfEnvironmentScript"
    exit 1
}

try {
    . $idfEnvironmentScript
}
catch {
    Write-Error "Failed to initialize ESP-IDF environment: $($_.Exception.Message)"
    exit 1
}

if ($env:IDF_PATH -ne $expectedIdfPath) {
    Write-Error "Unexpected IDF_PATH '$env:IDF_PATH'. Expected '$expectedIdfPath'."
    exit 1
}
if ($env:IDF_TOOLS_PATH -ne $expectedToolsPath) {
    Write-Error "Unexpected IDF_TOOLS_PATH '$env:IDF_TOOLS_PATH'. Expected '$expectedToolsPath'."
    exit 1
}
if ($env:IDF_PYTHON_ENV_PATH -ne $expectedPythonEnvironment) {
    Write-Error "Unexpected IDF_PYTHON_ENV_PATH '$env:IDF_PYTHON_ENV_PATH'. Expected '$expectedPythonEnvironment'."
    exit 1
}

foreach ($commandName in @("idf.py", "cmake", "ninja")) {
    if (-not (Get-Command -Name $commandName -ErrorAction SilentlyContinue)) {
        Write-Error "Required command '$commandName' is unavailable after ESP-IDF initialization. Check the ESP-IDF installation and environment script."
        exit 1
    }
}

foreach ($requiredPath in @(
    (Join-Path $env:IDF_PATH "tools\idf.py"),
    (Join-Path $env:IDF_PATH "tools\cmake\project.cmake"),
    (Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"),
    $idfProjectPath
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        Write-Error "Required ESP-IDF path does not exist: $requiredPath"
        exit 1
    }
}

$sdkconfigPath = Join-Path $idfProjectPath "sdkconfig"
if (-not (Test-Path -LiteralPath $sdkconfigPath -PathType Leaf)) {
    Write-Error "Project sdkconfig not found: $sdkconfigPath"
    exit 1
}

$sdkconfigText = Get-Content -LiteralPath $sdkconfigPath -Raw
$targetMatch = [regex]::Match($sdkconfigText, '(?m)^CONFIG_IDF_TARGET="([^"\r\n]+)"\r?$')
if (-not $targetMatch.Success) {
    Write-Error "CONFIG_IDF_TARGET is missing from $sdkconfigPath. Configure the project target explicitly; this script will not select one."
    exit 1
}
if ($targetMatch.Groups[1].Value -ne "esp32s3") {
    Write-Error "Project target is '$($targetMatch.Groups[1].Value)', not 'esp32s3'. This script will not change the configured target."
    exit 1
}

$cachePath = Join-Path (Join-Path $idfProjectPath "build") "CMakeCache.txt"
if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
    $cacheText = Get-Content -LiteralPath $cachePath -Raw
    $cacheTargetMatch = [regex]::Match($cacheText, '(?m)^IDF_TARGET:STRING=([^\r\n]+)')
    if ($cacheTargetMatch.Success -and $cacheTargetMatch.Groups[1].Value -ne "esp32s3") {
        Write-Error "Existing build cache target is '$($cacheTargetMatch.Groups[1].Value)', not 'esp32s3'. Clean or reconfigure it explicitly; this script will not change the target."
        exit 1
    }
}

Push-Location $idfProjectPath
try {
    idf.py build
    $buildExitCode = $LASTEXITCODE
}
catch {
    Write-Error "Failed to run idf.py build: $($_.Exception.Message)"
    $buildExitCode = 1
}
finally {
    Pop-Location
}

if ($null -eq $buildExitCode) {
    $buildExitCode = 1
}
exit $buildExitCode
