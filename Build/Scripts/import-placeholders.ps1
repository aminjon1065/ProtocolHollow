<#
.SYNOPSIS
    Copies placeholder character content (UE5 Manny + first-person anims) from the local UE 5.8 templates into Content\.

.DESCRIPTION
    Pair of import-placeholders.sh (ADR-0011). Template /Game paths are kept so internal references stay valid.
    Placeholders are not committed (.gitignore): run once per machine after cloning; re-running is safe.
    package-win64.ps1 calls this automatically when the placeholders are missing.

.EXAMPLE
    .\Build\Scripts\import-placeholders.ps1
.EXAMPLE
    .\Build\Scripts\import-placeholders.ps1 -Check
#>
[CmdletBinding()]
param(
    # Only verify that the placeholders are present (exit 1 if not).
    [switch] $Check,

    # Engine root (folder containing "Engine"). Default: $env:UE_ROOT, then the Epic Launcher install of UE 5.8.
    [string] $EngineRoot = $env:UE_ROOT
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'HollowBuild.psm1') -Force

if ($Check) {
    if (Test-HollowPlaceholders) {
        Write-Host 'OK: placeholders present.'
        exit 0
    }
    Write-Host 'MISSING: placeholders not imported. Run Build\Scripts\import-placeholders.cmd' -ForegroundColor Red
    exit 1
}

try {
    Assert-HollowWindows
    Import-HollowPlaceholders (Resolve-HollowEngineRoot $EngineRoot)
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    exit 2
}
Write-Host 'OK: placeholders imported into Content\Characters\Mannequins and Content\FirstPerson\Anims.' -ForegroundColor Green
