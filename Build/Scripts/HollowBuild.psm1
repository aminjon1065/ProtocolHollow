# Shared helpers for the Protocol: Hollow Windows build scripts (package-win64.ps1, run-tests-win64.ps1).
# Rationale for every limit: Docs/ADR/ADR-0009-cross-platform-toolchain.md. Keep this file ASCII-only.
Set-StrictMode -Version Latest

$script:EngineVersion        = '5.8'
$script:ReservedForOsGB      = 4.0
$script:GBPerCompileAction   = 2.5
$script:ReservedForCookerGB  = 8.0
$script:GBPerShaderWorker    = 1.25
$script:WarningPattern       = '[\\/]Source[\\/]Hollow.*warning'

function Assert-HollowWindows {
    if ($env:OS -ne 'Windows_NT') {
        throw 'This script must run on Windows: Win64 cannot be built from macOS (ADR-0009).'
    }
}

function Get-HollowProjectRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function Get-HollowUProject {
    $UProject = Join-Path (Get-HollowProjectRoot) 'ProtocolHollow.uproject'
    if (-not (Test-Path $UProject)) { throw "Project file not found: $UProject" }
    return $UProject
}

# Engine root (folder containing "Engine"): explicit value, otherwise the Epic Launcher install of UE 5.8.
function Resolve-HollowEngineRoot {
    param([string] $Candidate)
    if ($Candidate) { return $Candidate }
    $Manifest = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path $Manifest) {
        $Install = (Get-Content $Manifest -Raw | ConvertFrom-Json).InstallationList |
            Where-Object { $_.AppName -eq "UE_$script:EngineVersion" } | Select-Object -First 1
        if ($Install) { return $Install.InstallLocation }
    }
    throw "UE $script:EngineVersion not found. Pass -EngineRoot or set UE_ROOT."
}

function Get-HollowMachineInfo {
    return [pscustomobject]@{
        TotalRamGB   = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
        LogicalCores = [Environment]::ProcessorCount
    }
}

# Parallel C++ compile actions for UBT (-MaxParallelActions). 0 = derive from RAM (4 at 16 GB).
function Get-HollowCompileActions {
    param([int] $Requested, $Machine)
    $Actions = $Requested
    if ($Actions -eq 0) {
        $Actions = [int][math]::Floor(($Machine.TotalRamGB - $script:ReservedForOsGB) / $script:GBPerCompileAction)
    }
    return [math]::Max(1, [math]::Min($Actions, $Machine.LogicalCores))
}

# ShaderCompileWorker processes during cook. 0 = derive from RAM (6 at 16 GB).
function Get-HollowShaderWorkers {
    param([int] $Requested, $Machine)
    $Workers = $Requested
    if ($Workers -eq 0) {
        $Workers = [int][math]::Floor(($Machine.TotalRamGB - $script:ReservedForCookerGB) / $script:GBPerShaderWorker)
    }
    return [math]::Max(1, [math]::Min($Workers, $Machine.LogicalCores - 1))
}

# Runs a native tool, mirrors output to the console and a log file, returns the tool's exit code.
# Native stderr must not become terminating errors under 'Stop' (Windows PowerShell 5.1).
function Invoke-HollowNative {
    param([string] $FilePath, [string[]] $Arguments, [string] $LogFile)
    $Previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $FilePath @Arguments 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $LogFile | Out-Host
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $Previous
    }
}

# Zero-warning gate (CLAUDE.md sec. 7): warnings located in project modules. Returns matching lines.
function Get-HollowProjectWarnings {
    param([string] $LogFile)
    return @(Select-String -Path $LogFile -Pattern $script:WarningPattern | ForEach-Object { $_.Line })
}

# Builds ProtocolHollowEditor Win64 Development and applies the zero-warning gate. Returns $true on success.
function Invoke-HollowEditorBuild {
    param([string] $EngineRoot, [string] $UProject, [int] $MaxCompileActions, [string] $LogFile)
    $BuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    if (-not (Test-Path $BuildBat)) { throw "Build.bat not found: $BuildBat" }

    Write-Host "Building ProtocolHollowEditor Win64 Development ($MaxCompileActions compile actions) -> $LogFile"
    $ExitCode = Invoke-HollowNative $BuildBat @('ProtocolHollowEditor', 'Win64', 'Development',
        "-Project=$UProject", '-WaitMutex', "-MaxParallelActions=$MaxCompileActions") $LogFile
    if ($ExitCode -ne 0) {
        Write-Host "FAILED: UnrealBuildTool exit code $ExitCode (see $LogFile)" -ForegroundColor Red
        return $false
    }

    $Warnings = @(Get-HollowProjectWarnings $LogFile)   # @() keeps .Count valid for a single match under StrictMode
    if ($Warnings.Count -gt 0) {
        Write-Host "FAILED: $($Warnings.Count) warning(s) in project sources:" -ForegroundColor Red
        $Warnings | ForEach-Object { Write-Host "  $_" }
        return $false
    }
    return $true
}

Export-ModuleMember -Function Assert-HollowWindows, Get-HollowProjectRoot, Get-HollowUProject, Resolve-HollowEngineRoot,
    Get-HollowMachineInfo, Get-HollowCompileActions, Get-HollowShaderWorkers, Invoke-HollowNative,
    Get-HollowProjectWarnings, Invoke-HollowEditorBuild
