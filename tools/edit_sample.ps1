param(
    [ValidateSet('Character','Genesis8','Prop')][string]$Sample='Character',
    [string]$File='',
    [string]$Pose='',
    [string[]]$ContentRoot=@(),
    [string]$RuntimeDir='out/vs2022/Release'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if (-not $File) {
    if ($Sample -in @('Character','Genesis8')) {
        $File=Join-Path $env:PUBLIC 'Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8.1 Basic Female.duf'
        if ($Sample -eq 'Genesis8') {$File=Join-Path $env:PUBLIC 'Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8 Basic Female.duf'}
    } else {
        throw 'Use -File to select a prop from your content library.'
    }
}
if (-not [IO.Path]::IsPathRooted($RuntimeDir)) {$RuntimeDir=Join-Path $projectRoot $RuntimeDir}
$editor=Join-Path $RuntimeDir 'DazFastViewer.exe'
if (-not (Test-Path -LiteralPath $editor)) {throw '缺少编辑器，请先运行 tools/build.ps1'}
$editorArguments=@('--file',$File,'--project',(Join-Path $projectRoot 'DazFastViewer.project.json'))
if ($Pose) {$editorArguments+=@('--pose',$Pose)}
foreach ($root in $ContentRoot) {$editorArguments+=@('--content-root',$root)}
Push-Location $projectRoot
try { & $editor @editorArguments } finally {Pop-Location}
