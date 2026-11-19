<#
.SYNOPSIS
    Packages Protocol: Hollow for Win64 on a memory-constrained (16 GB RAM) Windows machine.

.DESCRIPTION
    Wrapper over RunUAT BuildCookRun (ADR-0009). Limits derived from installed RAM:
      * C++ compile actions      (UBT -MaxParallelActions)                     ~2.5 GB each -> 4 at 16 GB
      * ShaderCompileWorkers     ([DevOptions.Shaders] NumUnused... threads)  ~1.25 GB each -> 6 at 16 GB
      * Cooker garbage collection when free physical memory < 3 GB            ([CookSettings])
    Incremental cook by default (-iterate). Fails if MSVC reports a warning in project sources.

    Output: <repo>\Packages\Win64\<Configuration>\Windows\ProtocolHollow.exe
    Log:    <repo>\Saved\Logs\package-win64-<Configuration>-<timestamp>.log

    Requirements: Visual Studio 2022 with MSVC 14.44.35211+ (or 14.50.35723+), Windows SDK 10.0.22621, UE 5.8.

.EXAMPLE
    .\Build\Scripts\package-win64.ps1
.EXAMPLE
    .\Build\Scripts\package-win64.ps1 -Configuration Shipping -Clean
.EXAMPLE
    .\Build\Scripts\package-win64.ps1 -MaxCompileActions 3 -ShaderWorkers 4   # when the laptop is also running the editor
#>
[CmdletBinding()]
param(
    [ValidateSet('Development', 'DebugGame', 'Shipping')]
    [string] $Configuration = 'Development',

    # Engine root (folder containing "Engine"). Default: $env:UE_ROOT, then the Epic Launcher install of UE 5.8.
    [string] $EngineRoot = $env:UE_ROOT,

    [string] $OutputDir,

    # 0 = derive from installed RAM.
    [ValidateRange(0, 256)] [int] $MaxCompileActions = 0,
    [ValidateRange(0, 256)] [int] $ShaderWorkers = 0,

    # Full rebuild and recook instead of an incremental cook.
    [switch] $Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Tuning constants - rationale in Docs/ADR/ADR-0009-cross-platform-toolchain.md.
$EngineVersion           = '5.8'
$ReservedForOsGB         = 4.0
$GBPerCompileAction      = 2.5
$ReservedForCookerGB     = 8.0
$GBPerShaderWorker       = 1.25
$ShaderCoreThreshold     = 1024    # forces the absolute NumUnusedShaderCompilingThreads instead of the 50% default
$CookerMinFreePhysicalMB = 3072
$RecommendedFreeDiskGB   = 40

if ($env:OS -ne 'Windows_NT') {
    throw 'package-win64.ps1 must run on Windows: Win64 cannot be built from macOS.'
}

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$UProject    = Join-Path $ProjectRoot 'ProtocolHollow.uproject'
if (-not (Test-Path $UProject)) { throw "Project file not found: $UProject" }

function Resolve-EngineRoot([string] $Candidate) {
    if ($Candidate) { return $Candidate }
    $Manifest = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path $Manifest) {
        $Install = (Get-Content $Manifest -Raw | ConvertFrom-Json).InstallationList |
            Where-Object { $_.AppName -eq "UE_$EngineVersion" } | Select-Object -First 1
        if ($Install) { return $Install.InstallLocation }
    }
    throw "UE $EngineVersion not found. Pass -EngineRoot or set UE_ROOT."
}

$EngineRoot = Resolve-EngineRoot $EngineRoot
$RunUAT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path $RunUAT)) { throw "RunUAT.bat not found: $RunUAT" }

# --- Memory-aware limits -------------------------------------------------------------------------
$TotalRamGB   = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
$LogicalCores = [Environment]::ProcessorCount

if ($MaxCompileActions -eq 0) {
    $MaxCompileActions = [int][math]::Floor(($TotalRamGB - $ReservedForOsGB) / $GBPerCompileAction)
}
$MaxCompileActions = [math]::Max(1, [math]::Min($MaxCompileActions, $LogicalCores))

if ($ShaderWorkers -eq 0) {
    $ShaderWorkers = [int][math]::Floor(($TotalRamGB - $ReservedForCookerGB) / $GBPerShaderWorker)
}
$ShaderWorkers = [math]::Max(1, [math]::Min($ShaderWorkers, $LogicalCores - 1))
$UnusedShaderThreads = [math]::Max(0, $LogicalCores - $ShaderWorkers)

# --- Preflight warnings ---------------------------------------------------------------------------
$Drive = (Get-Item $ProjectRoot).PSDrive
$FreeDiskGB = [math]::Round($Drive.Free / 1GB, 1)
if ($FreeDiskGB -lt $RecommendedFreeDiskGB) {
    Write-Warning "Only $FreeDiskGB GB free on $($Drive.Name): (recommended >= $RecommendedFreeDiskGB GB for DDC, cook and staging)."
}
$PageFileMB = (@(Get-CimInstance Win32_PageFileUsage -ErrorAction SilentlyContinue) |
    Measure-Object -Property AllocatedBaseSize -Sum).Sum
$PageFileGB = [math]::Round([double]$PageFileMB / 1024, 1)
if ($PageFileGB -lt $TotalRamGB) {
    Write-Warning "Page file is $PageFileGB GB. Keep it system-managed (>= RAM) so link and cook peaks do not fail with out-of-memory."
}

# --- BuildCookRun ----------------------------------------------------------------------------------
if (-not $OutputDir) { $OutputDir = Join-Path $ProjectRoot "Packages\Win64\$Configuration" }
$LogDir = Join-Path $ProjectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $LogDir, $OutputDir | Out-Null
$LogFile = Join-Path $LogDir ('package-win64-{0}-{1:yyyyMMdd-HHmmss}.log' -f $Configuration, (Get-Date))

# One -ini switch per key: comma-separated pairs are legacy-only in ConfigCacheIni.
$CookerOptions = @(
    "-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=$UnusedShaderThreads",
    "-ini:Engine:[DevOptions.Shaders]:ShaderCompilerCoreCountThreshold=$ShaderCoreThreshold",
    "-ini:Editor:[CookSettings]:MemoryMinFreePhysical=$CookerMinFreePhysicalMB"
) -join ' '

$UatArgs = @(
    'BuildCookRun',
    "-project=$UProject",
    '-target=ProtocolHollow',
    '-platform=Win64',
    "-clientconfig=$Configuration",
    '-build', '-cook', '-stage', '-pak', '-iostore', '-package', '-prereqs',
    '-archive', "-archivedirectory=$OutputDir",
    '-utf8output', '-nop4', '-unattended', '-NoXGE',
    "-ubtargs=-MaxParallelActions=$MaxCompileActions",
    "-AdditionalCookerOptions=$CookerOptions"
)
if ($Clean) { $UatArgs += '-clean' } else { $UatArgs += '-iterate' }

Write-Host "Protocol: Hollow -> Win64 $Configuration"
Write-Host ("  RAM {0} GB, {1} logical cores -> {2} compile actions, {3} shader workers" -f $TotalRamGB, $LogicalCores, $MaxCompileActions, $ShaderWorkers)
Write-Host "  Engine: $EngineRoot"
Write-Host "  Output: $OutputDir"
Write-Host "  Log:    $LogFile"

# Native stderr must not become terminating errors under 'Stop' (Windows PowerShell 5.1).
$ErrorActionPreference = 'Continue'
& $RunUAT @UatArgs 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $LogFile
$ExitCode = $LASTEXITCODE
$ErrorActionPreference = 'Stop'

if ($ExitCode -ne 0) {
    Write-Host "FAILED: BuildCookRun exit code $ExitCode. See $LogFile" -ForegroundColor Red
    exit $ExitCode
}

# Zero-warning gate (CLAUDE.md sec. 7): MSVC/UHT warnings located in project modules fail the package.
$ProjectWarnings = @(Select-String -Path $LogFile -Pattern '[\\/]Source[\\/]Hollow.*warning')
if ($ProjectWarnings.Count -gt 0) {
    Write-Host "FAILED: $($ProjectWarnings.Count) warning(s) in project sources:" -ForegroundColor Red
    $ProjectWarnings | ForEach-Object { Write-Host "  $($_.Line)" }
    exit 1
}

Write-Host "OK: packaged to $(Join-Path $OutputDir 'Windows')" -ForegroundColor Green
