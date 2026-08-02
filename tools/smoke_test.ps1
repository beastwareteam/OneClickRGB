<#
.SYNOPSIS
    Starts the built application and checks that it actually works.

.DESCRIPTION
    The unit tests cover the protocols; nothing covered whether the program
    starts, keeps a single instance, or writes its files where it claims to.
    Every defect that reached the user so far was of that kind, so this runs the
    real binary in --dry-run (no hardware is touched) and asserts on observable
    behaviour.

    Exits non-zero on the first failed check, so it can gate a build.
#>
[CmdletBinding()]
param(
    # Resolved in the body: $PSScriptRoot is not reliably populated while
    # parameter defaults are bound under Windows PowerShell.
    [string]$Exe
)

$ErrorActionPreference = 'Stop'

if (-not $Exe) {
    $root = Split-Path -Parent $MyInvocation.MyCommand.Path
    $Exe = Join-Path $root '..\build_app\OneClickRGB.exe'
}
$script:Failures = 0

function Check {
    param([string]$What, [scriptblock]$Condition)
    $ok = $false
    try { $ok = [bool](& $Condition) } catch { $ok = $false }
    if ($ok) {
        Write-Host "  $What ... ok"
    } else {
        Write-Host "  $What ... FAILED" -ForegroundColor Red
        $script:Failures++
    }
}

function Stop-App {
    param($Process)
    if ($Process -and -not $Process.HasExited) {
        # The window minimises to tray on WM_CLOSE, so end the process outright.
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit(5000) | Out-Null
    }
}

if (-not (Test-Path $Exe)) {
    Write-Host "Executable not found: $Exe" -ForegroundColor Red
    Write-Host "Build it first: build_app.bat"
    exit 1
}
$Exe = (Resolve-Path $Exe).Path
$exeDir = Split-Path $Exe -Parent
$appData = Join-Path $env:APPDATA 'OneClickRGB'

Write-Host ""
Write-Host "Smoke test: $Exe"
Write-Host ""

# --- The build output must be runnable as it stands ------------------------
# hidapi.dll is load-time linked: if it is missing the process dies before
# reaching WinMain, with a Windows error that mentions nothing about RGB.
Write-Host "[build output]"
Check "hidapi.dll sits next to the executable" { Test-Path (Join-Path $exeDir 'hidapi.dll') }

$logPath = Join-Path $appData 'debug.log'
Remove-Item $logPath -Force -ErrorAction SilentlyContinue

$app = $null
$second = $null
try {
    # --- Startup -----------------------------------------------------------
    Write-Host "[startup]"
    $app = Start-Process $Exe -ArgumentList '--dry-run','--no-apply','--debug' -PassThru
    Start-Sleep -Seconds 4

    Check "process is still alive after four seconds" { -not $app.HasExited }
    Check "a main window exists" { $app.Refresh(); $app.MainWindowHandle -ne 0 }

    # --- Debug log placement ----------------------------------------------
    # It used to be written relative to the current directory, which for the
    # elevated autostart task is not the install folder.
    Write-Host "[logging]"
    Check "--debug writes the log to %APPDATA%" { Test-Path $logPath }
    Check "no debug.log lands next to the executable" {
        -not (Test-Path (Join-Path $exeDir 'debug.log'))
    }
    Check "the log records exactly one startup" {
        (Select-String -Path $logPath -Pattern 'WinMain started' -ErrorAction SilentlyContinue).Count -eq 1
    }
    Check "startup reached the message loop" {
        (Select-String -Path $logPath -Pattern 'Entering Message Loop' -ErrorAction SilentlyContinue).Count -eq 1
    }

    # --- Single instance ---------------------------------------------------
    # Two instances would drive the same devices and race on config.json.
    Write-Host "[single instance]"
    $second = Start-Process $Exe -ArgumentList '--dry-run','--no-apply' -PassThru
    $exited = $second.WaitForExit(10000)
    Check "a second launch exits on its own" { $exited }
    Check "it exits successfully rather than crashing" { $exited -and $second.ExitCode -eq 0 }
    Check "the first instance is untouched" { -not $app.HasExited }
    Check "the second launch did not log a startup of its own" {
        (Select-String -Path $logPath -Pattern 'WinMain started' -ErrorAction SilentlyContinue).Count -eq 1
    }

    # --- Settings ----------------------------------------------------------
    Write-Host "[settings]"
    $configPath = Join-Path $appData 'config.json'
    Check "config.json exists" { Test-Path $configPath }
    if (Test-Path $configPath) {
        $cfg = Get-Content $configPath -Raw | ConvertFrom-Json
        # 0 is not a keyboard mode at all, and as an edge mode it means FREEZE
        # while the UI shows Static.
        Check "kbMode is a mode the keyboard implements" {
            $cfg.kbMode -in @(1,2,3,4,5,6,7,8,10,12,13)
        }
        Check "edgeMode is within the protocol range and not FREEZE" {
            $cfg.edgeMode -ge 1 -and $cfg.edgeMode -le 5
        }
    }

    # --- Profile storage ---------------------------------------------------
    # L"\profiles" lost its separator and wrote to a sibling folder.
    Write-Host "[paths]"
    Check "no stray folder beside the app data directory" {
        -not (Test-Path (Join-Path $env:APPDATA 'OneClickRGBprofiles'))
    }
    Check "no file with a control character in its name" {
        $bad = Get-ChildItem $appData -Force -ErrorAction SilentlyContinue |
               Where-Object { $_.Name -match '[\x00-\x1f]' }
        $null -eq $bad -or $bad.Count -eq 0
    }
}
finally {
    Stop-App $app
    Stop-App $second
}

Write-Host ""
if ($script:Failures -gt 0) {
    Write-Host "SMOKE TEST FAILED: $($script:Failures) check(s)" -ForegroundColor Red
    exit 1
}
Write-Host "SMOKE TEST PASSED" -ForegroundColor Green
exit 0
