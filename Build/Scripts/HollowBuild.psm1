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

# True when <Root>\Engine\Build\Build.version reports the locked engine version (ADR-0008).
function Test-HollowEngineVersion {
    param([string] $Root)
    $VersionFile = Join-Path $Root 'Engine\Build\Build.version'
    if (-not (Test-Path $VersionFile)) { return $false }
    $Version = Get-Content $VersionFile -Raw | ConvertFrom-Json
    return ('{0}.{1}' -f $Version.MajorVersion, $Version.MinorVersion) -eq $script:EngineVersion
}

# Engine root (folder containing "Engine"): explicit value, otherwise UE 5.8 from the Epic Launcher manifest, the
# launcher registry key, or an engine registered by UnrealVersionSelector (HKCU Builds). The launcher manifest can be
# empty while the engine is installed (seen on the Windows test laptop), so every candidate is checked by Build.version.
function Resolve-HollowEngineRoot {
    param([string] $Candidate)
    if ($Candidate) { return $Candidate }

    $Candidates = @()
    $Manifest = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path $Manifest) {
        $Candidates += @((Get-Content $Manifest -Raw | ConvertFrom-Json).InstallationList |
            Where-Object { $_.AppName -eq "UE_$script:EngineVersion" } | ForEach-Object { $_.InstallLocation })
    }
    $Launcher = Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$script:EngineVersion" -Name 'InstalledDirectory' `
        -ErrorAction SilentlyContinue
    if ($Launcher) { $Candidates += $Launcher.InstalledDirectory }
    $BuildsKey = 'HKCU:\Software\Epic Games\Unreal Engine\Builds'
    if (Test-Path $BuildsKey) {
        $Builds = Get-Item $BuildsKey
        $Candidates += @($Builds.GetValueNames() | ForEach-Object { $Builds.GetValue($_) })
    }

    foreach ($Root in $Candidates) {
        if ($Root -and (Test-HollowEngineVersion $Root)) { return (Resolve-Path $Root).Path }
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

# --- Placeholder content (ADR-0011): copied from the local UE templates, never committed ---------------------

function Get-HollowPlaceholderPaths {
    $Root = Get-HollowProjectRoot
    return [pscustomobject]@{
        Characters  = Join-Path $Root 'Content\Characters\Mannequins'
        FirstPerson = Join-Path $Root 'Content\FirstPerson\Anims'
    }
}

# True when the assets the project references directly are present (same list as import-placeholders.sh).
function Test-HollowPlaceholders {
    $Paths = Get-HollowPlaceholderPaths
    $Required = @(
        (Join-Path $Paths.Characters 'Meshes\SKM_Manny_Simple.uasset'),
        (Join-Path $Paths.Characters 'Anims\Unarmed\ABP_Unarmed.uasset'),
        (Join-Path $Paths.FirstPerson 'ABP_FP_Copy.uasset')
    )
    foreach ($File in $Required) {
        if (-not (Test-Path $File)) { return $false }
    }
    return $true
}

# robocopy wrapper: exit codes 0-7 are success, 8+ are failures.
function Copy-HollowTree {
    param([string] $Source, [string] $Destination, [string] $File = '*.*', [string[]] $ExcludeDirs = @(), [switch] $TopLevelOnly)
    $RoboArgs = @($Source, $Destination, $File, '/NFL', '/NDL', '/NJH', '/NJS', '/NP')
    if (-not $TopLevelOnly) { $RoboArgs += '/E' }
    if ($ExcludeDirs.Count -gt 0) { $RoboArgs += '/XD'; $RoboArgs += $ExcludeDirs }
    & robocopy @RoboArgs | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed with code $LASTEXITCODE`: $Source -> $Destination" }
}

# Subset closed over its references: Quinn materials and melee attack anims are left out on purpose.
function Import-HollowPlaceholders {
    param([string] $EngineRoot)
    $CharactersSrc  = Join-Path $EngineRoot 'Templates\TemplateResources\High\Characters\Content\Mannequins'
    $FirstPersonSrc = Join-Path $EngineRoot 'Templates\TP_FirstPerson\Content\FirstPerson\Anims'
    foreach ($Src in @($CharactersSrc, $FirstPersonSrc)) {
        if (-not (Test-Path $Src)) { throw "Template content not found: $Src" }
    }

    $Paths = Get-HollowPlaceholderPaths
    Copy-HollowTree (Join-Path $CharactersSrc 'Meshes') (Join-Path $Paths.Characters 'Meshes')
    Copy-HollowTree (Join-Path $CharactersSrc 'Materials') (Join-Path $Paths.Characters 'Materials') -File 'M_Mannequin.uasset' -TopLevelOnly
    Copy-HollowTree (Join-Path $CharactersSrc 'Materials\Manny') (Join-Path $Paths.Characters 'Materials\Manny')
    Copy-HollowTree (Join-Path $CharactersSrc 'Textures\Manny') (Join-Path $Paths.Characters 'Textures\Manny')
    Copy-HollowTree (Join-Path $CharactersSrc 'Textures\Shared') (Join-Path $Paths.Characters 'Textures\Shared')
    Copy-HollowTree (Join-Path $CharactersSrc 'Rigs') (Join-Path $Paths.Characters 'Rigs')
    Copy-HollowTree (Join-Path $CharactersSrc 'Anims\Unarmed') (Join-Path $Paths.Characters 'Anims\Unarmed') -ExcludeDirs @('Attack')
    Copy-HollowTree $FirstPersonSrc $Paths.FirstPerson

    if (-not (Test-HollowPlaceholders)) { throw 'Import finished but required assets are missing.' }
}

Export-ModuleMember -Function Assert-HollowWindows, Get-HollowProjectRoot, Get-HollowUProject, Resolve-HollowEngineRoot,
    Get-HollowMachineInfo, Get-HollowCompileActions, Get-HollowShaderWorkers, Invoke-HollowNative,
    Get-HollowProjectWarnings, Invoke-HollowEditorBuild, Test-HollowPlaceholders, Import-HollowPlaceholders
