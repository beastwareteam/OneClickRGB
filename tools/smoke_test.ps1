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

# Reads the text of a child control of the main window. The status log is an
# EDIT control, and "the window comes up but the log stays empty" is a failure
# no check that only looks at files or exit codes can see.
Add-Type -Namespace Win32 -Name Ui -MemberDefinition @'
[DllImport("user32.dll", SetLastError=true)]
public static extern IntPtr GetDlgItem(IntPtr hDlg, int nIDDlgItem);
[DllImport("user32.dll", CharSet=CharSet.Unicode)]
public static extern IntPtr SendMessageW(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
[DllImport("user32.dll", CharSet=CharSet.Unicode)]
public static extern IntPtr SendMessageW(IntPtr hWnd, uint msg, IntPtr wParam, System.Text.StringBuilder lParam);
[DllImport("user32.dll")]
public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
public struct RECT { public int Left, Top, Right, Bottom; }
'@

# ID_STATIC_STATUS in src/oneclick_rgb_complete.cpp
$ID_STATIC_STATUS = 1060

function Get-StatusLogText {
    param([IntPtr]$MainWindow)
    $ctrl = [Win32.Ui]::GetDlgItem($MainWindow, $ID_STATIC_STATUS)
    if ($ctrl -eq [IntPtr]::Zero) { return $null }

    # WM_GETTEXT, not GetWindowText: for a control owned by another process
    # GetWindowText returns the window caption and an EDIT control has none, so
    # it hands back an empty string no matter what the control contains. That
    # is by design, and reading it the wrong way made a working log look empty.
    $WM_GETTEXTLENGTH = 0x000E
    $WM_GETTEXT       = 0x000D

    $len = [int][Win32.Ui]::SendMessageW($ctrl, $WM_GETTEXTLENGTH, [IntPtr]::Zero, [IntPtr]::Zero)
    if ($len -le 0) { return "" }
    $sb = New-Object System.Text.StringBuilder ($len + 1)
    [void][Win32.Ui]::SendMessageW($ctrl, $WM_GETTEXT, [IntPtr]$sb.Capacity, $sb)
    return $sb.ToString()
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

# An instance already running holds the single-instance mutex, so the one this
# script starts would exit immediately and every check below would report a
# failure that says nothing about the build under test.
$running = @(Get-Process OneClickRGB -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    Write-Host "OneClickRGB is already running - close it before running the smoke test:" -ForegroundColor Red
    $running | ForEach-Object { Write-Host "  PID $($_.Id)  $($_.Path)" }
    exit 1
}
try {
    $existing = [System.Threading.Mutex]::OpenExisting('Local\OneClickRGB.SingleInstance')
    $existing.Dispose()
    Write-Host "The single-instance mutex is held by another process - close it first." -ForegroundColor Red
    exit 1
} catch [System.Threading.WaitHandleCannotBeOpenedException] {
    # Nothing holds it - this is the expected path.
}

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
    if ($app.HasExited) {
        # Everything after this inspects a live window; without one the checks
        # would fail with binding errors that hide the actual cause.
        Write-Host "  the process exited early with code $($app.ExitCode) - remaining checks skipped" -ForegroundColor Red
        Write-Host ""
        if (Test-Path $logPath) {
            Write-Host "  debug log:"
            Get-Content $logPath | ForEach-Object { Write-Host "    $_" }
        }
        exit 1
    }
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

    # --- Status log --------------------------------------------------------
    # The log is what tells the user why a device did not respond. It has been
    # empty, doubled and repainted per line; each of those reached a user.
    Write-Host "[status log]"
    $app.Refresh()
    $log = Get-StatusLogText $app.MainWindowHandle
    Check "the status control exists" { $null -ne $log }
    Check "it is not empty" { $log -and $log.Trim().Length -gt 0 }
    Check "it shows the startup line" { $log -match 'OneClickRGB started' }
    Check "no line appears twice in a row" {
        if (-not $log) { return $false }
        $lines = @($log -split "`r`n" | Where-Object { $_.Trim() -ne '' })
        $dupes = 0
        for ($i = 1; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -eq $lines[$i-1]) { $dupes++ }
        }
        $dupes -eq 0
    }

    # Press Apply for real and watch what the log does. In --dry-run this runs
    # every protocol against the simulated backend, so it is safe here.
    $before = $log
    $WM_COMMAND = 0x0111
    $ID_BTN_APPLY = 1001
    $BN_CLICKED = 0
    $wParam = [IntPtr](($BN_CLICKED -shl 16) -bor $ID_BTN_APPLY)
    [void][Win32.Ui]::SendMessageW($app.MainWindowHandle, $WM_COMMAND, $wParam, [IntPtr]::Zero)
    Start-Sleep -Seconds 2
    $after = Get-StatusLogText $app.MainWindowHandle

    Check "applying writes to the log" { $after -and $after -match 'Applying RGB Settings' }
    Check "applying appends rather than clearing the history" {
        $after -and $before -and $after.Length -gt $before.Length
    }
    Check "one apply produces one run, not several" {
        if (-not $after) { return $false }
        ([regex]::Matches($after, 'Applying RGB Settings')).Count -eq 1
    }

    # A picture of the log after scrolling. Painting defects - text drawn over
    # text because the control never erased its background - are invisible to
    # every check above, which only ever reads the control's text. No assertion
    # is made on the pixels; the image is there to be looked at, and CI keeps it
    # as an artifact.
    Write-Host "[appearance]"
    $log = [Win32.Ui]::GetDlgItem($app.MainWindowHandle, $ID_STATIC_STATUS)
    $WM_VSCROLL = 0x0115
    foreach ($cmd in 2,2,3) {      # SB_PAGEUP, SB_PAGEUP, SB_PAGEDOWN
        [void][Win32.Ui]::SendMessageW($log, $WM_VSCROLL, [IntPtr]$cmd, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 200
    }
    Check "the log control could be captured" {
        Add-Type -AssemblyName System.Drawing
        $rect = New-Object Win32.Ui+RECT
        if (-not [Win32.Ui]::GetWindowRect($log, [ref]$rect)) { return $false }
        $w = $rect.Right - $rect.Left
        $h = $rect.Bottom - $rect.Top
        if ($w -le 0 -or $h -le 0) { return $false }
        $bmp = New-Object System.Drawing.Bitmap $w, $h
        $gfx = [System.Drawing.Graphics]::FromImage($bmp)
        $gfx.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
        $shot = Join-Path (Split-Path $Exe -Parent) 'smoke_status_log.png'
        $bmp.Save($shot, [System.Drawing.Imaging.ImageFormat]::Png)
        $gfx.Dispose(); $bmp.Dispose()
        Write-Host "    saved: $shot"
        Test-Path $shot
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
