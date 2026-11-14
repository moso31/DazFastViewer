[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Scene,
    [string]$Output = ''
)
$ErrorActionPreference = 'Stop'
$previewRoot = Split-Path -Parent $PSScriptRoot
$previewExe = Join-Path $previewRoot 'out/vs2022/Release/GrowthCompositionPreview.exe'
if (-not (Test-Path -LiteralPath $previewExe)) { throw 'Build the GrowthCompositionPreview target first.' }
$previewScene = (Resolve-Path -LiteralPath $Scene).Path
if (-not $Output) { $Output = Join-Path $previewRoot 'artifacts/growth-composition-interactive' }
$previewOutput = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Path $previewOutput -Force | Out-Null
$previewProject = Join-Path $previewRoot 'DazFastViewer.project.json'
$previewArgs = @($previewScene, $previewProject, $previewOutput) | ForEach-Object { '"' + $_ + '"' }
Start-Process -FilePath $previewExe -ArgumentList $previewArgs -WorkingDirectory $previewRoot -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $previewOutput 'preview.log') `
    -RedirectStandardError (Join-Path $previewOutput 'preview-error.log') | Out-Null
