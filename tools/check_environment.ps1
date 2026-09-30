[CmdletBinding()]
param([switch]$RequireGpu)
. (Join-Path $PSScriptRoot 'build_common.ps1')
$vs = Get-VisualStudio
$git = Find-Git
$python = Find-Python
$cmake = Find-CMake
Write-Host "Visual Studio: $($vs.displayName) [$($vs.installationVersion)]"
Write-Host "Location: $($vs.installationPath)"
Invoke-Checked $git @('--version')
Invoke-Checked $python @('--version')
Invoke-Checked $cmake @('--version')
$smi = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
$gpuReady = $false
if ($smi) {
    $rows = @(& $smi.Source --query-gpu=name,driver_version,compute_cap --format=csv,noheader)
    if ($LASTEXITCODE -eq 0) {
        foreach ($row in $rows) {
            Write-Host "GPU: $row"
            $fields = $row -split ','
            if ($fields.Count -ge 3 -and [version]$fields[1].Trim() -ge [version]'590.0' -and [version]$fields[2].Trim() -ge [version]'7.5') { $gpuReady = $true }
        }
    }
}
if (-not $gpuReady) {
    Write-Warning 'OptiX 9.1 rendering requires a Turing-or-newer NVIDIA GPU (compute capability 7.5+) and R590+ driver. No CPU/quality fallback is applied.'
    if ($RequireGpu) { throw 'GPU runtime prerequisites not met. See Docs/build_windows_cn.md.' }
}
Write-Host 'Tool discovery completed. Driver version checks do not replace a --smoke render test.'
