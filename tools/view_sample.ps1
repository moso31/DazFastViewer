param(
    [ValidateSet('Character','Prop')][string]$Sample='Prop',
    [string]$File='',
    [string[]]$ContentRoot=@(),
    [switch]$Inspect,
    [ValidateRange(0,3600)][double]$PreviewSeconds=0
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if (-not $File) {
    $File=if ($Sample -eq 'Character') {
        'C:\Users\Public\Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8.1 Basic Female.duf'
    } else {
        'H:\G3\Environments\Architecture\ARK Modern Modular Cafe\Props\Tabletop\ARK MM Cafe - Food Plate with Fries.duf'
    }
}
$executable=Join-Path $projectRoot 'out/CyclesViewportBench.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {throw '请先构建：tools/build.ps1'}
if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {throw "资产不存在：$File"}
$output=Join-Path $projectRoot ('artifacts/view-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$viewerArgs=@('--file',$File,'--monitor','2','--width','1600','--height','900','--device','OPTIX','--output',$output)
foreach ($root in $ContentRoot) {$viewerArgs+=@('--content-root',$root)}
if ($Inspect) {$viewerArgs+='--inspect'}
if ($PreviewSeconds -gt 0) {$viewerArgs+=@('--preview-seconds',$PreviewSeconds.ToString([Globalization.CultureInfo]::InvariantCulture))}
Push-Location $projectRoot
try {
    & $executable @viewerArgs
    if ($LASTEXITCODE -ne 0) {throw "预览失败（$LASTEXITCODE），输出目录：$output"}
} finally {Pop-Location}
