[CmdletBinding()]
param([switch]$Offline, [string]$QtRoot = '', [string]$CudaRoot = '', [string]$Proxy = '')
. (Join-Path $PSScriptRoot 'build_common.ps1')
$previousPath = $env:PATH
$previousHttpProxy = $env:HTTP_PROXY
$previousHttpsProxy = $env:HTTPS_PROXY
try {
    $vs = Get-VisualStudio
    Write-Host "C++ toolchain: $($vs.installationPath)"
    $git = Find-Git
    $env:PATH = (Split-Path $git) + ';' + $env:PATH
    if ($Proxy) { $env:HTTP_PROXY = $Proxy; $env:HTTPS_PROXY = $Proxy }
    $python = Find-Python
    $venv = Join-Path $ProjectRoot '.deps\python'
    $venvPython = Join-Path $venv 'Scripts\python.exe'
    if (-not (Test-Path -LiteralPath $venvPython)) {
        if ($Offline) { throw 'Missing .deps/python. Prepare dependencies online on this machine first.' }
        Invoke-Checked $python @('-m', 'venv', $venv)
    }
    $lock = Get-Content (Join-Path $PSScriptRoot 'dependencies.lock.json') -Raw | ConvertFrom-Json
    if (-not $Offline) {
        Invoke-Checked $venvPython @('-m', 'pip', 'install', '--disable-pip-version-check', "cmake==$($lock.tools.cmake)", "aqtinstall==$($lock.tools.aqtinstall)")
    }
    $arguments = @((Join-Path $PSScriptRoot 'bootstrap.py'))
    if ($Offline) { $arguments += '--offline' }
    if ($QtRoot) { $arguments += @('--qt-root', $QtRoot) }
    if ($CudaRoot) { $arguments += @('--cuda-root', $CudaRoot) }
    Invoke-Checked $venvPython $arguments
} finally {
    $env:PATH = $previousPath
    $env:HTTP_PROXY = $previousHttpProxy
    $env:HTTPS_PROXY = $previousHttpsProxy
}
