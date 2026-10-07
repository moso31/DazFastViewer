# ASCII only: compatible with Windows PowerShell 5.1.
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param()
$ErrorActionPreference = 'Stop'
$taskWorkspace = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\','/')
$taskTargets = @()
foreach ($taskFolderName in @('build','out')) {
    $taskParent = Join-Path $taskWorkspace $taskFolderName
    if (-not (Test-Path -LiteralPath $taskParent -PathType Container)) { continue }
    $taskParentItem = Get-Item -LiteralPath $taskParent -Force
    if ($taskParentItem.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Refusing to clean a redirected folder: $taskParent"
    }
    foreach ($taskItem in Get-ChildItem -LiteralPath $taskParent -Force) {
        # Toolchain directories hold current builds and runtime packages.
        if ($taskItem.Name -match '^vs\d{4}$') { continue }
        $taskAbsolute = [IO.Path]::GetFullPath($taskItem.FullName)
        if ([IO.Path]::GetDirectoryName($taskAbsolute) -ne $taskParent) {
            throw "Target outside expected workspace folder: $taskAbsolute"
        }
        if ($taskItem.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Refusing to recursively clean a redirected target: $taskAbsolute"
        }
        $taskTargets += $taskAbsolute
    }
}
# Check every absolute target before performing any recursive deletion.
foreach ($taskAbsolute in $taskTargets) {
    if ($PSCmdlet.ShouldProcess($taskAbsolute, 'Remove legacy build/runtime artifact')) {
        Remove-Item -LiteralPath $taskAbsolute -Recurse -Force -ErrorAction Stop
    }
}
Write-Host "Legacy entries selected: $($taskTargets.Count). Toolchain directories preserved."
