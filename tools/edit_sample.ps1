param(
    [ValidateSet('Character','Genesis8','Prop')][string]$Sample='Character',
    [string]$File='',
    [string[]]$ContentRoot=@()
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if (-not $File) {
    if ($Sample -in @('Character','Genesis8')) {
        $File='C:\Users\Public\Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8.1 Basic Female.duf'
        if ($Sample -eq 'Genesis8') {$File='C:\Users\Public\Documents\My DAZ 3D Library\People\Genesis 8 Female\Genesis 8 Basic Female.duf'}
    } else {
        $File='H:\G3\Environments\Architecture\ARK Modern Modular Cafe\Props\Tabletop\ARK MM Cafe - Food Plate with Fries.duf'
    }
}
$editor=Join-Path $projectRoot 'out\DazFastViewer.exe'
if (-not (Test-Path -LiteralPath $editor)) {throw '缺少编辑器，请先运行 tools/build.ps1'}
$editorArguments=@('--file',$File,'--project',(Join-Path $projectRoot 'DazFastViewer.project.json'))
foreach ($root in $ContentRoot) {$editorArguments+=@('--content-root',$root)}
Push-Location $projectRoot
try { & $editor @editorArguments } finally {Pop-Location}
