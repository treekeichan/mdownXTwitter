param([Parameter(Mandatory = $true)][string]$Url)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$previousPath = $env:PATH
$previousUrl = $env:MDOWN_INTEGRATION_URL
$previousTools = $env:MDOWN_REAL_TOOLS
$previousOutput = $env:MDOWN_REAL_OUTPUT
try {
    $env:PATH = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.0\mingw_64\bin;' + $env:PATH
    $env:MDOWN_INTEGRATION_URL = $Url
    $env:MDOWN_REAL_TOOLS = Join-Path $projectRoot 'tools'
    $env:MDOWN_REAL_OUTPUT = Join-Path $projectRoot 'build\integration-download'
    $logPath = Join-Path $projectRoot 'build\integration-result.txt'
    & (Join-Path $projectRoot 'build\EngineTests.exe') realDownload -o "$logPath,txt"
    $testExit = $LASTEXITCODE
    if (Test-Path -LiteralPath $logPath) { Get-Content -LiteralPath $logPath }
    if ($testExit -ne 0) { throw 'Real download integration check failed' }
} finally {
    $env:PATH = $previousPath
    $env:MDOWN_INTEGRATION_URL = $previousUrl
    $env:MDOWN_REAL_TOOLS = $previousTools
    $env:MDOWN_REAL_OUTPUT = $previousOutput
}
