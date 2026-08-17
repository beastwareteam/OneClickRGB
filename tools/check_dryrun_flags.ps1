<#
    check_dryrun_flags.ps1 - Prueft die --dry-run-Wache aller Probe-Flags.

    Schreibt selbst nichts auf Hardware: jeder Aufruf laeuft mit --dry-run, und
    genau das ist der Pruefgegenstand. Das Skript darf daher auch mit
    angeschlossener Tastatur laufen.

    Geprueft wird pro Flag:
      1. Exitcode
      2. Der Report existiert und enthaelt den DRY-RUN-Hinweis, aber KEINE
         Messzeilen - ein Dry-Run darf nichts behaupten, was er nicht gemessen
         hat (Regel 1).
      3. Der Prozess bricht SOFORT ab, statt die Haltezeiten abzuwarten. Ein
         --edgemode-sweep=5 wuerde 11x5s = 55s laufen; unter --dry-run darf es
         nur Sekunden dauern. Genau das war der alte Fehler des kbmode-Blocks:
         21 Schritte schlafen und den Report mit "READ FAILED" fuellen fuer
         Writes, die nie versucht wurden.
      4. debug.log enthaelt die passende Skip-Zeile.

    Der letzte Fall prueft zusaetzlich den Tokenizer am fertigen Binary:
    "--kbmode-sweep5" (das '=' vergessen) darf KEINEN Sweep starten. Mit dem
    alten strstr-Matching hat es das getan.

    Aufruf:  .\tools\check_dryrun_flags.ps1
             .\tools\check_dryrun_flags.ps1 -Exe .\build\OneClickRGB.exe
#>

[CmdletBinding()]
param(
    # Leer = Standardpfad relativ zu diesem Skript. Nicht als Parameter-Default,
    # weil $PSScriptRoot dort je nach Aufrufart noch leer ist.
    [string] $Exe = ""
)

$ErrorActionPreference = 'Stop'

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if ([string]::IsNullOrWhiteSpace($Exe)) {
    $Exe = Join-Path $scriptDir "..\build\OneClickRGB.exe"
}

if (-not (Test-Path $Exe)) {
    Write-Host "Executable nicht gefunden: $Exe" -ForegroundColor Red
    Write-Host "Vorher build_native.bat laufen lassen."
    exit 1
}
$Exe = (Resolve-Path $Exe).Path
$docsDir = Join-Path $env:APPDATA "OneClickRGB\docs"

# LogDebug() schreibt "debug.log" relativ zum Arbeitsverzeichnis des Prozesses.
# Jeder Testfall bekommt deshalb ein eigenes, leeres Arbeitsverzeichnis: dann
# gehoert jede Zeile in dieser debug.log genau einem Aufruf, und es muss nichts
# ueber Byte-Offsets aus einer wachsenden Sammel-Logdatei herausgerechnet werden.
# (Das war der erste Versuch und er war falsch: Windows liefert die Groesse einer
# Datei, die ein anderer Prozess gerade geschrieben hat, aus einem gecachten
# Verzeichniseintrag - der Offset zeigte am Ende hinter das Dateiende.)
$workDir = Join-Path ([IO.Path]::GetTempPath()) ("OneClickRGB_dryrun_" + [Guid]::NewGuid().ToString("N").Substring(0,8))
New-Item -ItemType Directory -Force -Path $workDir | Out-Null
$logPath = Join-Path $workDir "debug.log"

Write-Host "Exe      : $Exe"
Write-Host "Reports  : $docsDir"
Write-Host "Arbeitsdir/Log: $workDir"
Write-Host ""

# Eine Messzeile im Report beginnt mit Zeit und Modusbyte ("  0  0x00 ...").
# Taucht so etwas nach einem Dry-Run auf, behauptet der Report eine Messung.
$measurementRow = '^\s+\d+\s+0x[0-9A-Fa-f]{2}\s'

$cases = @(
    @{ Name = '--edgemode (dry-run)'
       Args = @('--dry-run','--edgemode=3')
       Report = 'edgemode_probe.txt'; Exit = 0; MaxSec = 15
       LogMatch = '\[dry-run\].*edgemode.*skipped' }

    @{ Name = '--edgemode-sweep (dry-run, wuerde 55s laufen)'
       Args = @('--dry-run','--edgemode-sweep=5')
       Report = 'edgemode_probe.txt'; Exit = 0; MaxSec = 15
       LogMatch = '\[dry-run\].*edgemode.*skipped' }

    @{ Name = '--edgespeed-sweep (dry-run, wuerde 30s laufen)'
       Args = @('--dry-run','--edgespeed-sweep=5')
       Report = 'edgemode_probe.txt'; Exit = 0; MaxSec = 15
       LogMatch = '\[dry-run\].*edgemode.*skipped' }

    @{ Name = 'zwei Edge-Aktionen gleichzeitig -> Abbruch'
       Args = @('--dry-run','--edgemode=3','--edgemode-sweep')
       Report = 'edgemode_probe.txt'; Exit = 2; MaxSec = 15
       ReportMatch = 'mutually exclusive'
       LogMatch = '\[edgeprobe\] more than one action flag' }

    @{ Name = '--kbmode-sweep (dry-run, wuerde 105s laufen)'
       Args = @('--dry-run','--kbmode-sweep=5')
       Report = 'kbmode_probe.txt'; Exit = 0; MaxSec = 15
       LogMatch = '\[dry-run\].*kbmode.*skipped' }

    @{ Name = 'zwei Keyboard-Aktionen gleichzeitig -> Abbruch'
       Args = @('--dry-run','--kbmode=3','--kbmode-sweep')
       Report = $null; Exit = 2; MaxSec = 15
       LogMatch = '\[kbmode\].*mutually exclusive' }

    @{ Name = '--unlock-winkey (dry-run)'
       Args = @('--dry-run','--unlock-winkey')
       Report = 'unlock_winkey.txt'; Exit = 0; MaxSec = 15
       LogMatch = '\[dry-run\].*unlock-winkey skipped' }

    # Ab hier die Pfade, die im Echtbetrieb einen MessageBox oeffnen. Genau die
    # duerfen unter --dry-run KEINEN Dialog zeigen: dieses Skript laeuft
    # unbeaufsichtigt, ein wartender Dialog wuerde bis MaxSec haengen und den
    # Fehler als Timeout tarnen statt ihn zu benennen. Die MaxSec-Schranke ist
    # hier also die eigentliche Pruefung.
    @{ Name = '--rendercheck=edge (dry-run, kein Dialog)'
       Args = @('--dry-run','--rendercheck=edge')
       Report = 'rendercheck.txt'; Exit = 0; MaxSec = 15
       ReportMatch = 'no question was asked'
       LogMatch = '\[dry-run\].*rendercheck skipped' }

    @{ Name = '--rendercheck=kb (dry-run, kein Dialog)'
       Args = @('--dry-run','--rendercheck=kb')
       Report = 'rendercheck.txt'; Exit = 0; MaxSec = 15
       ReportMatch = 'no question was asked'
       LogMatch = '\[dry-run\].*rendercheck skipped' }

    @{ Name = '--rendercheck ohne Ziel -> Abbruch'
       Args = @('--rendercheck')
       Report = 'rendercheck.txt'; Exit = 2; MaxSec = 15
       ReportMatch = 'needs a target'
       LogMatch = '\[rendercheck\].*target' }

    @{ Name = '--edgemode-sweep --confirm (dry-run, kein Dialog)'
       Args = @('--dry-run','--edgemode-sweep=5','--confirm')
       Report = 'edgemode_probe.txt'; Exit = 0; MaxSec = 15
       LogMatch = '\[dry-run\].*edgemode.*skipped' }

    @{ Name = '--kbmode-sweep --confirm (dry-run, kein Dialog)'
       Args = @('--dry-run','--kbmode-sweep=5','--confirm')
       Report = 'kbmode_probe.txt'; Exit = 0; MaxSec = 15
       LogMatch = '\[dry-run\].*kbmode.*skipped' }
)

$fail = 0
$pass = 0

function Test-Case($c) {
    $reportPath = if ($c.Report) { Join-Path $docsDir $c.Report } else { $null }
    if ($reportPath -and (Test-Path $reportPath)) { Remove-Item $reportPath -Force }

    if (Test-Path $script:logPath) { Remove-Item $script:logPath -Force }

    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $script:Exe -ArgumentList $c.Args `
                       -WorkingDirectory $script:workDir -PassThru -Wait
    $sw.Stop()
    $elapsed = [Math]::Round($sw.Elapsed.TotalSeconds, 1)

    $problems = @()

    if ($p.ExitCode -ne $c.Exit) {
        $problems += "Exitcode $($p.ExitCode), erwartet $($c.Exit)"
    }
    if ($elapsed -gt $c.MaxSec) {
        $problems += "Laufzeit ${elapsed}s > $($c.MaxSec)s - die Wache greift zu spaet oder gar nicht"
    }

    if ($reportPath) {
        if (-not (Test-Path $reportPath)) {
            $problems += "Report fehlt: $($c.Report)"
        } else {
            $text = Get-Content $reportPath -Raw
            $lines = Get-Content $reportPath
            if ($c.ReportMatch) {
                if ($text -notmatch $c.ReportMatch) { $problems += "Report enthaelt '$($c.ReportMatch)' nicht" }
            } elseif ($text -notmatch 'DRY RUN') {
                $problems += "Report nennt den Dry-Run nicht"
            }
            $rows = @($lines | Where-Object { $_ -match $script:measurementRow })
            if ($rows.Count -gt 0) {
                $problems += "Report enthaelt $($rows.Count) Messzeile(n), obwohl nichts gemessen wurde"
            }
        }
    }

    if ($c.LogMatch) {
        $log = if (Test-Path $script:logPath) { Get-Content $script:logPath -Raw } else { "" }
        if ([string]::IsNullOrEmpty($log)) {
            $problems += "debug.log wurde nicht geschrieben"
        } elseif ($log -notmatch $c.LogMatch) {
            $problems += "keine Skip-Zeile in debug.log (erwartet: $($c.LogMatch))"
        }
    }

    if ($problems.Count -eq 0) {
        Write-Host ("  OK   {0}  ({1}s)" -f $c.Name, $elapsed) -ForegroundColor Green
        return $true
    }
    Write-Host ("  FAIL {0}  ({1}s)" -f $c.Name, $elapsed) -ForegroundColor Red
    $problems | ForEach-Object { Write-Host "         - $_" -ForegroundColor Red }
    return $false
}

Write-Host "Dry-Run-Wache:"
foreach ($c in $cases) {
    if (Test-Case $c) { $pass++ } else { $fail++ }
}

# --- Tokenizer am fertigen Binary -------------------------------------------
# "--kbmode-sweep5" ist kein Flag. Es darf keinen Sweep starten - stattdessen
# faellt WinMain durch zur GUI. Der Prozess wird nach kurzer Zeit beendet; da
# --dry-run gesetzt ist, geht dabei nichts an die Hardware.
Write-Host ""
Write-Host "Tokenizer (kein Sweep bei vertipptem Flag):"
$probe = Join-Path $docsDir 'kbmode_probe.txt'
if (Test-Path $probe) { Remove-Item $probe -Force }

$p = Start-Process -FilePath $Exe -ArgumentList @('--dry-run','--kbmode-sweep5') `
                   -WorkingDirectory $workDir -PassThru
Start-Sleep -Seconds 4
$stillRunning = -not $p.HasExited
if ($stillRunning) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }

if (Test-Path $probe) {
    Write-Host "  FAIL '--kbmode-sweep5' hat den kbmode-Probe gestartet (Report wurde geschrieben)" -ForegroundColor Red
    $fail++
} else {
    $note = if ($stillRunning) { "GUI gestartet und beendet - kein Probe-Report" } else { "kein Probe-Report" }
    Write-Host "  OK   '--kbmode-sweep5' startet keinen Sweep ($note)" -ForegroundColor Green
    $pass++
}

Remove-Item $workDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "------------------------------------------------"
if ($fail -eq 0) {
    Write-Host "ALLE $pass PRUEFUNGEN OK" -ForegroundColor Green
    exit 0
}
Write-Host "$fail von $($pass + $fail) Pruefungen fehlgeschlagen" -ForegroundColor Red
exit 1
