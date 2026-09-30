# ASCII only: compatible with Windows PowerShell 5.1.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:ProjectRoot = Split-Path -Parent $PSScriptRoot
$env:PYTHONUTF8 = '1'

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed (exit $LASTEXITCODE)." }
}

function Get-VisualStudio([string]$Version = 'Auto') {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install VS 2022/2026 with Desktop development with C++. See Docs/build_windows_cn.md.' }
    $instances = & $vswhere -all -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
    $found = @($instances | Where-Object {
        $major = ([version]$_.installationVersion).Major
        ($major -eq 17 -or $major -eq 18) -and ($Version -eq 'Auto' -or ($Version -eq '2022' -and $major -eq 17) -or ($Version -eq '2026' -and $major -eq 18))
    } | Sort-Object installationVersion -Descending)
    if ($found.Count -eq 0) { throw "No VS $Version C++ x64 toolchain found. See Docs/build_windows_cn.md." }
    return $found[0]
}

function Find-Git {
    $command = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $candidates = @((Join-Path $env:ProgramFiles 'Git\cmd\git.exe'))
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $instances = & $vswhere -all -products '*' -format json | ConvertFrom-Json
        foreach ($instance in $instances) {
            $candidates += Join-Path $instance.installationPath 'Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe'
        }
    }
    foreach ($candidate in $candidates) { if (Test-Path -LiteralPath $candidate) { return $candidate } }
    throw 'Git not found. Run: winget install --id Git.Git -e, then reopen PowerShell.'
}

function Find-Python {
    $candidates = @((Join-Path $script:ProjectRoot '.deps\python\Scripts\python.exe'))
    $launcher = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($launcher) {
        $resolved = & $launcher.Source -3 -c 'import sys; print(sys.executable)'
        if ($LASTEXITCODE -eq 0) { $candidates += $resolved }
    }
    $command = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($command -and $command.Source -notlike '*\WindowsApps\*') { $candidates += $command.Source }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            & $candidate -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) and sys.maxsize > 2**32 else 1)'
            if ($LASTEXITCODE -eq 0) { return $candidate }
        }
    }
    throw 'Python 3.10+ not found. Run: winget install --id Python.Python.3.12 -e, then reopen PowerShell.'
}

function Find-CMake {
    $candidates = @((Join-Path $script:ProjectRoot '.deps\python\Scripts\cmake.exe'))
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { $candidates += $command.Source }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            $versionText = (& $candidate --version | Select-Object -First 1)
            if ($versionText -match '(\d+\.\d+\.\d+)' -and [version]$Matches[1] -ge [version]'4.2.0') { return $candidate }
        }
    }
    throw 'CMake 4.2+ not found. Run tools/bootstrap.ps1 first.'
}
