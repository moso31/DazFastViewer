[CmdletBinding()]
param(
    [ValidateSet('Release','Debug','RelWithDebInfo')][string]$Configuration = 'Release',
    [ValidateSet('Auto','2022','2026')][string]$VisualStudio = 'Auto',
    [string]$BuildDir = '',
    [string]$OutputDir = '',
    [string]$CudaArchitectures = 'Common',
    [ValidateRange(1,64)][int]$Jobs = 4,
    [switch]$ConfigureOnly,
    [switch]$SkipTests,
    [switch]$SkipStage,
    [string[]]$CMakeOptions = @()
)
. (Join-Path $PSScriptRoot 'build_common.ps1')
$previousPath = $env:PATH
try {
    $vs = Get-VisualStudio $VisualStudio
    $year = if (([version]$vs.installationVersion).Major -eq 18) { '2026' } else { '2022' }
    $generator = if ($year -eq '2026') { 'Visual Studio 18 2026' } else { 'Visual Studio 17 2022' }
    $git = Find-Git
    $python = Find-Python
    $cmake = Find-CMake
    $env:PATH = (Split-Path $git) + ';' + (Split-Path $cmake) + ';' + $env:PATH
    if (-not $BuildDir) { $BuildDir = Join-Path $ProjectRoot "build\vs$year" }
    if (-not $OutputDir) { $OutputDir = Join-Path $ProjectRoot "out\vs$year\$Configuration" }
    $BuildDir = [IO.Path]::GetFullPath($BuildDir)
    $OutputDir = [IO.Path]::GetFullPath($OutputDir)
    $commonArchitectures = 'sm_75;sm_80;sm_86;sm_89;sm_90;sm_100;sm_120'
    if ($CudaArchitectures -eq 'Common') { $CudaArchitectures = $commonArchitectures }
    if ($CudaArchitectures -eq 'Auto') {
        $smi = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
        if ($smi) {
            $capabilities = @(& $smi.Source --query-gpu=compute_cap --format=csv,noheader 2>$null)
            if ($LASTEXITCODE -eq 0) {
                $arches = @($capabilities | ForEach-Object { if ($_ -match '^\s*(\d+)\.(\d+)\s*$') { "sm_$($Matches[1])$($Matches[2])" } } | Select-Object -Unique)
                if ($arches.Count -gt 0) { $CudaArchitectures = $arches -join ';' }
            }
        }
        if ($CudaArchitectures -eq 'Auto') { $CudaArchitectures = $commonArchitectures }
    }
    if ($CudaArchitectures -notmatch '^(sm_|compute_)\d+(;(sm_|compute_)\d+)*$') {
        throw 'CudaArchitectures must be Common, Auto, or a semicolon-separated list such as sm_86;sm_89.'
    }
    Write-Host "VS $year | $Configuration | CUDA: $CudaArchitectures | Jobs: $Jobs"
    $configure = @('-S', $ProjectRoot, '-B', $BuildDir, '-G', $generator, '-A', 'x64', '-T', 'v143',
        "-DCMAKE_GENERATOR_INSTANCE=$($vs.installationPath)", "-DPython3_EXECUTABLE=$python",
        "-DDFV_STAGE_EDITOR_RUNTIME=$(-not [bool]$SkipStage)",
        "-DCYCLES_CUDA_BINARIES_ARCH=$CudaArchitectures", "-DCMAKE_INSTALL_PREFIX=$OutputDir")
    $localCache = Join-Path $ProjectRoot '.deps\local.cmake'
    if (Test-Path -LiteralPath $localCache) { $configure += @('-C', $localCache) }
    $configure += $CMakeOptions
    Invoke-Checked $cmake $configure
    Invoke-Checked $python @((Join-Path $PSScriptRoot 'export_solution.py'), '--build-dir', $BuildDir)
    if ($ConfigureOnly) { return }
    Invoke-Checked $cmake @('--build', $BuildDir, '--config', $Configuration, '--parallel', "$Jobs")
    if (-not $SkipStage) {
        Invoke-Checked $python @((Join-Path $PSScriptRoot 'stage_runtime.py'), '--build-dir', $BuildDir, '--configuration', $Configuration, '--output', $OutputDir)
    }
    if (-not $SkipTests) {
        Invoke-Checked (Join-Path (Split-Path $cmake) 'ctest.exe') @('--test-dir', $BuildDir, '-C', $Configuration, '--output-on-failure')
    }
    Write-Host "Build complete: $OutputDir\DazFastViewer.exe"
} finally { $env:PATH = $previousPath }
