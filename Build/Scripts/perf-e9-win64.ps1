<#
.SYNOPSIS
    Measures criterion E9 on this Windows machine (ADR-0023): >= 60 FPS at 1080p (High, TSR) for a host with 4 players
    in the hero room, with an Unreal Insights trace.
.DESCRIPTION
    1. Packages the game (package-win64.ps1, Development: the launcher engine cannot build Test targets) unless -SkipPackage.
    2. Checks the machine: GPU, driver, mains power, power plan.
    3. Runs the benchmark scenario on this machine: a rendered host (the measured one, traced) and 3 headless clients
       (-nullrhi, 30 FPS cap, Null online subsystem, local IP), seed 7. AI removed, everyone at the core in the CoreVault,
       the host lifts it and the squad Resonance goes Critical (the emergency strobe), two clients weld, the host's view
       turns evenly while Hollow.Perf.Capture records WarmupSeconds + Seconds.
    4. Prints the verdict; the folder holds the trace (.utrace), the CSV, the JSON report, the logs and summary.txt.
    -ClientTrace adds a run measuring a rendered client (headless host); -Hunt adds a CPU run with the AI alive (no verdict).
    Exit code: 0 E9 passed, 1 failed, 2 the run did not complete.
.EXAMPLE
    .\Build\Scripts\perf-e9-win64.cmd
.EXAMPLE
    .\Build\Scripts\perf-e9-win64.cmd -SkipPackage -Preset Low -ClientTrace -Hunt
#>
[CmdletBinding()]
param(
    # Development: the closest measurable build with the dev commands and Insights (Test needs a source-built engine).
    [ValidateSet('Development')]
    [string] $Configuration = 'Development',
    [ValidateSet('High', 'Low')]
    [string] $Preset = 'High',
    [ValidateRange(10, 600)] [int] $Seconds = 60,
    [ValidateRange(0, 120)] [int] $WarmupSeconds = 10,
    [int] $Seed = 7,
    [int] $ResX = 1920,
    [int] $ResY = 1080,
    # Windowed instead of fullscreen (the measurement turns vsync off either way).
    [switch] $Windowed,
    # Use the build already in -BuildDir (default Packages\Win64\<Configuration>\Windows).
    [switch] $SkipPackage,
    [string] $BuildDir,
    [switch] $ClientTrace,
    [switch] $Hunt,
    [switch] $NoTrace
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'HollowBuild.psm1') -Force
Assert-HollowWindows

$StartupTimeoutSec = 300      # first launch of a cooked build compiles shaders (PSO cache)
$ReportGraceSec    = 180
$ClientFpsCap      = 30       # headless clients only have to play
$RaidStartSec      = 45       # the host starts the raid this long after hosting (clients join meanwhile)
$Clients           = 3
$Welders           = 2

$ProjectRoot = Get-HollowProjectRoot
if (-not $BuildDir) { $BuildDir = Join-Path $ProjectRoot "Packages\Win64\$Configuration\Windows" }

# --- 1. Build -----------------------------------------------------------------------------------------------------------
if (-not $SkipPackage) {
    & (Join-Path $PSScriptRoot 'package-win64.ps1') -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED: packaging (exit $LASTEXITCODE)" -ForegroundColor Red; exit 2 }
}
$Exe = Get-ChildItem -Path (Join-Path $BuildDir 'ProtocolHollow\Binaries\Win64') -Filter 'ProtocolHollow*.exe' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $Exe) { Write-Host "FAILED: no game executable under $BuildDir (package first, or pass -BuildDir)" -ForegroundColor Red; exit 2 }
$GameSaved = Join-Path $BuildDir 'ProtocolHollow\Saved'

# --- 2. Machine ---------------------------------------------------------------------------------------------------------
$Gpus = @(Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Where-Object { $_.Name -notmatch 'Basic Display|Remote' })
$Cpu = Get-CimInstance Win32_Processor -ErrorAction SilentlyContinue | Select-Object -First 1
$Battery = @(Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue)
$OnMains = ($Battery.Count -eq 0) -or ($Battery | Where-Object { $_.BatteryStatus -eq 2 })
$Plan = (& powercfg /getactivescheme) -join ' '
Write-Host "Protocol: Hollow - E9 on this machine ($Configuration, preset $Preset, ${ResX}x${ResY})"
foreach ($Gpu in $Gpus) { Write-Host ("  GPU:   {0} (driver {1})" -f $Gpu.Name, $Gpu.DriverVersion) }
if ($Cpu) { Write-Host ("  CPU:   {0}, {1} cores / {2} threads" -f $Cpu.Name.Trim(), $Cpu.NumberOfCores, $Cpu.NumberOfLogicalProcessors) }
Write-Host "  Power: $Plan"
Write-Host "  Build: $($Exe.FullName)"
if (-not $OnMains) { Write-Warning 'Running on battery: plug the laptop in (the GPU is throttled on battery).' }
if ($Plan -notmatch 'High performance|Ultimate|8c5e7fda|e9a42b02') {
    Write-Warning 'The power plan is not High performance (Control Panel > Power Options, or the laptop vendor tool).'
}
if ($Gpus.Count -gt 1) {
    Write-Warning 'Several GPUs: make sure the game runs on the discrete one (Windows Settings > Display > Graphics > ProtocolHollow).'
}

$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$Out = Join-Path $ProjectRoot "Saved\PerfCapture\$Stamp-$Preset"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Common = @("-HollowPreset=$Preset", '-nosplash', '-unattended',
    '-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null', '-ini:Engine:[OnlineSubsystemSteam]:bEnabled=false')
$Headless = @('-nullrhi', '-nosound')
$Total = $WarmupSeconds + $Seconds + 5

function Get-Slot([string] $Name) { return "-ini:Game:[/Script/HollowEconomy.HollowEconomySettings]:SaveSlotName=$Name" }

function Get-View([bool] $Rendered) {
    if (-not $Rendered) { return $Headless }
    $View = @("-ResX=$ResX", "-ResY=$ResY")
    if ($Windowed) { $View += '-windowed' } else { $View += '-fullscreen' }
    return $View
}

function Get-Trace([string] $Name) {
    if ($NoTrace) { return @() }
    return @('-trace=default,gpu,frame,net,physics,audio', "-tracefile=`"$(Join-Path $Out "$Name.utrace")`"", '-statnamedevents')
}

function Start-Game([string] $Name, [string[]] $Arguments, [string] $Commands) {
    # Start-Process joins the arguments with spaces: paths and the command list are quoted here.
    $All = $Common + $Arguments + @((Get-Slot $Name), "-abslog=`"$(Join-Path $Out "$Name.log")`"", "-ExecCmds=`"$Commands`"")
    return Start-Process -FilePath $Exe.FullName -ArgumentList $All -PassThru
}

# The log of one process: its -abslog file, or (if the build ignores -abslog) the log in the game's Saved\Logs whose
# command line names its own save slot.
function Resolve-Log([string] $Log) {
    if (Test-Path $Log) { return $Log }
    $Slot = "SaveSlotName=" + [IO.Path]::GetFileNameWithoutExtension($Log)
    $Found = Get-ChildItem (Join-Path $GameSaved 'Logs') -Filter '*.log' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending |
        Where-Object { Select-String -Path $_.FullName -Pattern ([regex]::Escape($Slot) + '\b') -Quiet -List } | Select-Object -First 1
    if ($Found) { return $Found.FullName }
    return $null
}

function Wait-ForLog([string] $Log, [string] $Pattern, [int] $TimeoutSec) {
    $Deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $Deadline) {
        $Path = Resolve-Log $Log
        if ($Path -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

function Get-HostScenario([string] $Capture, [bool] $KillAi, [bool] $Measured) {
    $C = @('r.VSync 0', 't.MaxFPS 0', 'Hollow.Session.Host InviteOnly', "Hollow.Dev.RunAfter $RaidStartSec Hollow.Raid.Start $Seed")
    if ($KillAi) {
        $C += @('Hollow.Dev.RunInRaid 2 Hollow.AI.Kill', 'Hollow.Dev.RunInRaid 3 Hollow.AI.Director Spawn', 'Hollow.Dev.RunInRaid 4 Hollow.AI.Kill')
    } else {
        $C += @('Hollow.Dev.RunInRaid 3 Hollow.AI.Director Spawn', 'Hollow.Dev.RunInRaid 6 Hollow.AI.Director Hunt')
    }
    $C += @('Hollow.Dev.RunInRaid 5 Hollow.Dev.GoToNearestCore All', 'Hollow.Dev.RunInRaid 6 Hollow.Interact', 'Hollow.Dev.RunInRaid 7 Hollow.Resonance.Add 90')
    if ($Measured) {
        $C += @('Hollow.Dev.RunInRaid 8 Hollow.Graphics.Dump', "Hollow.Dev.RunInRaid 9 Hollow.Dev.LookAround $Total 2",
            "Hollow.Dev.RunInRaid 9 Hollow.Perf.Capture $Capture $Seconds $WarmupSeconds")
    }
    return $C -join ','
}

function Get-ClientScenario([bool] $Weld, [string] $Capture, [bool] $Rendered) {
    $C = @()
    if (-not $Rendered) { $C += "t.MaxFPS $ClientFpsCap" } else { $C += @('r.VSync 0', 't.MaxFPS 0') }
    $C += 'open 127.0.0.1'
    if ($Weld) { $C += "Hollow.Dev.RunInRaid 8 Hollow.Tool.Hold Hollow.Input.SecondaryUse $Total" }
    if ($Capture) {
        $C += @('Hollow.Dev.RunInRaid 8 Hollow.Graphics.Dump', "Hollow.Dev.RunInRaid 9 Hollow.Dev.LookAround $Total 2",
            "Hollow.Dev.RunInRaid 9 Hollow.Perf.Capture $Capture $Seconds $WarmupSeconds")
    }
    return $C -join ','
}

# One run: the host and three clients; $Measured is 'host' or 'client' (that one renders and is traced); waits for its report.
function Invoke-Run([string] $Capture, [string] $Measured, [bool] $KillAi) {
    Write-Host ''
    Write-Host "== $Capture ($Measured measured)" -ForegroundColor Cyan
    $HostLog = Join-Path $Out "$Capture-host.log"
    $Started = Get-Date
    $Processes = @()
    $HostArgs = (Get-View ($Measured -eq 'host'))
    if ($Measured -eq 'host') { $HostArgs += (Get-Trace "$Capture-host") }
    $Processes += Start-Game "$Capture-host" $HostArgs (Get-HostScenario $Capture $KillAi ($Measured -eq 'host'))
    if (-not (Wait-ForLog $HostLog 'listening on port' $StartupTimeoutSec)) {
        Write-Host "FAILED: the host never listened (see $HostLog)" -ForegroundColor Red
        $Processes | Stop-Process -Force -ErrorAction SilentlyContinue
        return $null
    }
    $MeasuredLog = $HostLog
    for ($Index = 1; $Index -le $Clients; $Index++) {
        $Rendered = ($Measured -eq 'client') -and ($Index -eq $Clients)
        $Weld = $Index -le $Welders
        $ClientArgs = (Get-View $Rendered)
        $ClientCapture = ''
        if ($Rendered) { $ClientArgs += (Get-Trace "$Capture-client"); $ClientCapture = $Capture; $MeasuredLog = Join-Path $Out "$Capture-client$Index.log" }
        $Processes += Start-Game "$Capture-client$Index" $ClientArgs (Get-ClientScenario $Weld $ClientCapture $Rendered)
    }
    $Done = Wait-ForLog $MeasuredLog 'Hollow\.Perf: report ' ($StartupTimeoutSec + $RaidStartSec + $Total + $ReportGraceSec)
    Start-Sleep -Seconds 3
    $Processes | Stop-Process -Force -ErrorAction SilentlyContinue
    if (-not $Done) { Write-Host "FAILED: no capture report (see $MeasuredLog)" -ForegroundColor Red; return $null }
    $MeasuredLog = Resolve-Log $MeasuredLog
    Get-ChildItem (Join-Path $GameSaved 'Logs') -Filter '*.log' -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $Started } |
        Copy-Item -Destination $Out -ErrorAction SilentlyContinue   # the logs written to the game's folder, if -abslog was ignored
    $ReportLine = (Select-String -Path $MeasuredLog -Pattern 'Hollow\.Perf: report (.+\.json)' | Select-Object -Last 1).Matches[0].Groups[1].Value.Trim()
    $Json = Get-Item $ReportLine -ErrorAction SilentlyContinue
    if (-not $Json) { $Json = Get-ChildItem (Join-Path $GameSaved 'Profiling\Hollow') -Filter "$Capture-*.json" | Sort-Object LastWriteTime -Descending | Select-Object -First 1 }
    Copy-Item $Json.FullName $Out
    Copy-Item ([IO.Path]::ChangeExtension($Json.FullName, 'csv')) $Out -ErrorAction SilentlyContinue
    $Report = Get-Content $Json.FullName -Raw | ConvertFrom-Json
    $Colour = 'Red'
    if ($Report.verdict -eq 'PASS') { $Colour = 'Green' }
    Write-Host ("  {0}" -f $Report.summary) -ForegroundColor $Colour
    Write-Host ("  {0} on {1}, {2:N0} MB VRAM, {3} players, preset {4}, {5}" -f $Report.machine.rhi, $Report.machine.gpu, $Report.machine.vramMB,
        $Report.scene.players, $Report.scene.preset, $Report.scene.resolution)
    return $Report
}

# --- 3. Runs ------------------------------------------------------------------------------------------------------------
$E9 = Invoke-Run "E9-$Preset" 'host' $true
$Extra = @()
if ($ClientTrace) { $Extra += Invoke-Run "Client-$Preset" 'client' $true }
if ($Hunt) { $Extra += Invoke-Run "Hunt-$Preset" 'host' $false }

# --- 4. Summary ---------------------------------------------------------------------------------------------------------
$Summary = @("Protocol: Hollow E9 run $Stamp ($Configuration, preset $Preset, ${ResX}x${ResY})")
foreach ($Gpu in $Gpus) { $Summary += "GPU: $($Gpu.Name) (driver $($Gpu.DriverVersion))" }
foreach ($Report in @($E9) + $Extra) {
    if ($Report) { $Summary += "$($Report.name): $($Report.summary)" }
}
$Summary | Set-Content -Path (Join-Path $Out 'summary.txt') -Encoding ASCII
Write-Host ''
Write-Host "Results: $Out  (send this folder: logs, .utrace, .csv, .json, summary.txt)"
if (-not $E9) { exit 2 }
if ($E9.verdict -eq 'PASS') { Write-Host 'E9: PASS' -ForegroundColor Green; exit 0 }
Write-Host 'E9: FAIL' -ForegroundColor Red
exit 1
