<#
.SYNOPSIS
    Runs Protocol: Hollow automation tests headless on Windows and returns a non-zero exit code on any problem.

.DESCRIPTION
    Pair of run-tests-mac.sh (same options and exit codes, CLAUDE.md sec. 7, ADR-0009):
      1. incremental build of ProtocolHollowEditor Win64 Development + zero-warning gate (skip with -NoBuild);
      2. UnrealEditor-Cmd -nullrhi "Automation RunTests <Filter>" with a timeout;
      3. summary of Saved\Automation\<timestamp>\Report\index.json (index.html is next to it).

    Exit codes: 0 all passed | 1 test failed / not run | 2 usage or environment | 3 no report produced
                4 no test matched the filter | 5 editor exited non-zero | 6 timeout | 7 build failed

.EXAMPLE
    .\Build\Scripts\run-tests-win64.ps1
.EXAMPLE
    .\Build\Scripts\run-tests-win64.ps1 -Filter Hollow.Core -NoBuild -Strict
#>
[CmdletBinding()]
param(
    # Test name filter passed to "Automation RunTests".
    [string] $Filter = 'Hollow.',

    # Skip the editor build (default: incremental build + zero-warning gate first).
    [switch] $NoBuild,

    # Tests that pass with warnings count as failures.
    [switch] $Strict,

    [ValidateRange(60, 86400)] [int] $TimeoutSec = 1800,

    # Engine root (folder containing "Engine"). Default: $env:UE_ROOT, then the Epic Launcher install of UE 5.8.
    [string] $EngineRoot = $env:UE_ROOT,

    # 0 = derive from installed RAM.
    [ValidateRange(0, 256)] [int] $MaxCompileActions = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'HollowBuild.psm1') -Force

$ExitTestsFailed = 1; $ExitEnvironment = 2; $ExitNoReport = 3; $ExitNoTests = 4
$ExitEditorFailed = 5; $ExitTimeout = 6; $ExitBuildFailed = 7

try {
    Assert-HollowWindows
    $ProjectRoot = Get-HollowProjectRoot
    $UProject    = Get-HollowUProject
    $EngineRoot  = Resolve-HollowEngineRoot $EngineRoot
    $EditorCmd   = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    if (-not (Test-Path $EditorCmd)) { throw "UnrealEditor-Cmd.exe not found: $EditorCmd" }
}
catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    exit $ExitEnvironment
}

$RunDir    = Join-Path $ProjectRoot ('Saved\Automation\{0:yyyyMMdd-HHmmss}' -f (Get-Date))
$ReportDir = Join-Path $RunDir 'Report'
$EditorLog = Join-Path $RunDir 'Editor.log'
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

if (-not $NoBuild) {
    $Machine = Get-HollowMachineInfo
    $Actions = Get-HollowCompileActions $MaxCompileActions $Machine
    if (-not (Invoke-HollowEditorBuild $EngineRoot $UProject $Actions (Join-Path $RunDir 'Build.log'))) {
        Write-Host 'FAILED: editor build (or zero-warning gate) failed, tests not run.' -ForegroundColor Red
        exit $ExitBuildFailed
    }
}

# Start-Process joins arguments verbatim, so values with spaces are quoted here.
$EditorArgs = @(
    "`"$UProject`"",
    "-ExecCmds=`"Automation RunTests $Filter; Quit`"",
    '-TestExit="Automation Test Queue Empty"',
    '-unattended', '-nullrhi', '-nosplash', '-nosound', '-nop4',
    "-abslog=`"$EditorLog`"",
    "-ReportExportPath=`"$ReportDir`""
) -join ' '

Write-Host "Running automation tests '$Filter' (timeout ${TimeoutSec}s) -> $RunDir"
$Editor = Start-Process -FilePath $EditorCmd -ArgumentList $EditorArgs -NoNewWindow -PassThru `
    -RedirectStandardOutput (Join-Path $RunDir 'Console.txt') -RedirectStandardError (Join-Path $RunDir 'Console.err.txt')
$null = $Editor.Handle   # caches the process handle so ExitCode stays readable after exit
if (-not $Editor.WaitForExit($TimeoutSec * 1000)) {
    Write-Host "FAILED: timeout after ${TimeoutSec}s, killing editor (pid $($Editor.Id)). Log: $EditorLog" -ForegroundColor Red
    Stop-Process -Id $Editor.Id -Force -ErrorAction SilentlyContinue
    exit $ExitTimeout
}
$EditorExit = $Editor.ExitCode

# --- Report --------------------------------------------------------------------------------------
$ReportJson = Join-Path $ReportDir 'index.json'
if (-not (Test-Path $ReportJson)) {
    # UE exports no report at all when the filter matches nothing; it logs this line instead.
    if ((Test-Path $EditorLog) -and (Select-String -Path $EditorLog -Pattern 'No automation tests matched' -SimpleMatch -Quiet)) {
        Write-Host 'FAILED: no test matched the filter.' -ForegroundColor Red
        exit $ExitNoTests
    }
    Write-Host "FAILED: no report produced (editor crashed or failed to start). Log: $EditorLog" -ForegroundColor Red
    exit $ExitNoReport
}

function Get-Field($Object, [string] $Name) {
    $Property = $Object.PSObject.Properties[$Name]
    if ($Property) { return $Property.Value }
    return $null
}

function Show-Tests([string] $Title, $Tests, [string[]] $Kinds) {
    foreach ($Test in $Tests) {
        Write-Host "  $Title $(Get-Field $Test 'fullTestPath')"
        foreach ($Entry in @(Get-Field $Test 'entries')) {
            if ($null -eq $Entry) { continue }
            $LogEvent = Get-Field $Entry 'event'
            $Type = Get-Field $LogEvent 'type'
            if ($Kinds -notcontains $Type) { continue }
            $File = Get-Field $Entry 'filename'
            $Location = ''
            if ($File -and $File -ne 'Unknown') { $Location = " ($(Split-Path $File -Leaf):$(Get-Field $Entry 'lineNumber'))" }
            Write-Host "      ${Type}: $(Get-Field $LogEvent 'message')$Location"
        }
    }
}

$Report = Get-Content $ReportJson -Raw -Encoding UTF8 | ConvertFrom-Json
$Tests  = @(Get-Field $Report 'tests' | Where-Object { $null -ne $_ })
$Passed = @($Tests | Where-Object { $_.state -eq 'Success' -and $_.warnings -eq 0 })
$Warned = @($Tests | Where-Object { $_.state -eq 'Success' -and $_.warnings -gt 0 })
$Failed = @($Tests | Where-Object { $_.state -eq 'Fail' })
$NotRun = @($Tests | Where-Object { $_.state -ne 'Success' -and $_.state -ne 'Fail' })

Write-Host ''
Write-Host ('Automation report: {0} test(s) | passed {1} | passed with warnings {2} | failed {3} | not run {4}' -f `
    $Tests.Count, $Passed.Count, $Warned.Count, $Failed.Count, $NotRun.Count)
Show-Tests 'FAIL   ' $Failed @('Error')
Show-Tests 'WARN   ' $Warned @('Warning')
Show-Tests 'NOT RUN' $NotRun @('Error', 'Warning')
Write-Host "Report: $(Join-Path $ReportDir 'index.html') | Editor log: $EditorLog"

if ($Tests.Count -eq 0) {
    Write-Host 'FAILED: no test matched the filter.' -ForegroundColor Red
    exit $ExitNoTests
}
if ($Failed.Count -gt 0 -or $NotRun.Count -gt 0 -or ($Strict -and $Warned.Count -gt 0)) {
    exit $ExitTestsFailed
}
if ($EditorExit -ne 0) {
    Write-Host "FAILED: editor exited with code $EditorExit although all tests passed (see $EditorLog)." -ForegroundColor Red
    exit $ExitEditorFailed
}
Write-Host 'OK: all tests passed.' -ForegroundColor Green
exit 0
