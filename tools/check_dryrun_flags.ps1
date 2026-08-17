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
       Report = 'rendercheck_edge.txt'; Exit = 0; MaxSec = 15
       ReportMatch = 'no question was asked'
       LogMatch = '\[dry-run\].*rendercheck skipped' }

    @{ Name = '--rendercheck=kb (dry-run, kein Dialog)'
       Args = @('--dry-run','--rendercheck=kb')
       Report = 'rendercheck_kb.txt'; Exit = 0; MaxSec = 15
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

    # --- Per-Tasten-Pfade ---------------------------------------------------
    # --keyidentify oeffnet im Echtbetrieb pro Offset einen Dialog und schreibt
    # ein Tripel in die Farbtabelle. Unter --dry-run darf beides nicht passieren,
    # und der Ausstieg muss VOR der ersten MessageBox liegen - sonst haengt
    # dieser unbeaufsichtigte Lauf bis MaxSec und tarnt den Fehler als Timeout.
    @{ Name = '--keyidentify (dry-run, kein Dialog, kein Write)'
       Args = @('--dry-run','--keyidentify=0x2C0,0x2C3')
       Report = 'keyidentify.txt'; Exit = 0; MaxSec = 15
       ReportMatch = 'no question was asked'
       LogMatch = '\[dry-run\].*keyidentify skipped' }

    # Ein Offset mitten in einem Tripel wuerde jede folgende Tastenfarbe um
    # einen Kanal verschieben. Argumentpruefung vor allem anderen: Exit 2, und
    # der Report sagt, was gefordert ist.
    @{ Name = '--keyidentify mit Offset in der Tripel-Mitte -> Abbruch'
       Args = @('--dry-run','--keyidentify=0x2C1')
       Report = 'keyidentify.txt'; Exit = 2; MaxSec = 15
       ReportMatch = 'multiple of 3'
       LogMatch = '\[keyidentify\].*invalid or missing offset' }

    @{ Name = '--keyidentify ausserhalb der Farbtabelle -> Abbruch'
       Args = @('--dry-run','--keyidentify=0x100')
       Report = 'keyidentify.txt'; Exit = 2; MaxSec = 15
       ReportMatch = 'Nothing was written'
       LogMatch = '\[keyidentify\].*nothing was written' }

    @{ Name = '--keypattern (dry-run, schreibt die Farbtabelle nicht)'
       Args = @('--dry-run','--keypattern')
       Report = 'keypattern.txt'; Exit = 0; MaxSec = 15
       LogMatch = '\[dry-run\].*keypattern skipped' }

    @{ Name = '--keypattern mit unbekanntem Wert -> Abbruch'
       Args = @('--dry-run','--keypattern=off')
       Report = 'keypattern.txt'; Exit = 2; MaxSec = 15
       ReportMatch = 'Nothing was written'
       LogMatch = '\[keypattern\].*unknown value' }

    # --- Modifier, die ohne ihre Aktion nichts tun duerfen -------------------
    # Ein Modifier, der stillschweigend ignoriert wird, ist schlimmer als ein
    # Fehler: --kbmode-only=0x00,0x04 wuerde dann alle 21 Modi laufen lassen,
    # waehrend der Bediener zwei erwartet und der Report zwei nennt.
    @{ Name = '--kbmode-only ohne Sweep -> Abbruch'
       Args = @('--dry-run','--kbmode-only=0x00,0x04')
       Report = $null; Exit = 2; MaxSec = 15
       LogMatch = '\[kbmode\].*kbmode-only only applies' }

    @{ Name = '--kbmode-only mit Muell -> Abbruch'
       Args = @('--dry-run','--kbmode-sweep=5','--kbmode-only=0x00,banane')
       Report = $null; Exit = 2; MaxSec = 15
       LogMatch = '\[kbmode\].*comma-separated byte list' }

    @{ Name = '--ask mit unbekanntem Wert -> Abbruch'
       Args = @('--dry-run','--kbmode-sweep=5','--confirm','--ask=farbe')
       Report = $null; Exit = 2; MaxSec = 15
       LogMatch = '\[kbmode\].*ask needs motion' }

    @{ Name = '--kbcolor ohne Modus-Aktion -> Abbruch'
       Args = @('--dry-run','--kbcolor=FF0000')
       Report = $null; Exit = 2; MaxSec = 15
       LogMatch = '\[kbmode\].*kbcolor only applies' }

    @{ Name = '--kbcolor mit ungueltigem Wert -> Abbruch'
       Args = @('--dry-run','--kbmode=0x06','--kbcolor=#FF0000')
       Report = $null; Exit = 2; MaxSec = 15
       LogMatch = '\[kbmode\].*six hex digits' }

    # --kbdump ist rein lesend, deswegen laeuft dieser Fall ohne --dry-run: die
    # Argumentpruefung muss greifen, bevor ueberhaupt ein Handle geoeffnet wird.
    @{ Name = '--kbdump-range mit verdrehtem Bereich -> Abbruch'
       Args = @('--kbdump','--kbdump-range=0x300-0x200')
       Report = 'kbdump.txt'; Exit = 2; MaxSec = 15
       ReportMatch = 'Nothing was read'
       LogMatch = '\[kbdump\].*invalid range' }

    @{ Name = '--kbdump-range ohne Wert -> Abbruch'
       Args = @('--kbdump','--kbdump-range')
       Report = 'kbdump.txt'; Exit = 2; MaxSec = 15
       ReportMatch = 'Nothing was read'
       LogMatch = '\[kbdump\].*invalid range' }
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

# --- Instanzsperre ----------------------------------------------------------
# Zwei Prozesse auf derselben HID-Collection lesen die Antworten des jeweils
# anderen. Am 17.08.2026 liefen --rendercheck=edge und --rendercheck=kb
# gleichzeitig: die Reports schrieben sich ineinander, ein Snapshot kam als 18
# Nullbytes zurueck und wurde als "verified" ins Flash restauriert - die
# Tastatur ging aus, waehrend jede Zeile "verified" meldete.
#
# Hier wird der Mutex von aussen gehalten, damit der Fall deterministisch
# pruefbar ist, ohne zwei echte Probes zu starten. Erwartet: sofortiger Abbruch
# mit Exit 3 und ein Report, der sagt, dass nichts gemessen wurde.
Write-Host ""
Write-Host "Instanzsperre (zweite Instanz darf die Hardware nicht anfassen):"
$dump = Join-Path $docsDir 'kbdump.txt'
if (Test-Path $dump) { Remove-Item $dump -Force }

$held = New-Object System.Threading.Mutex($true, 'Local\OneClickRGB_HidProbe')
try {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $p2 = Start-Process -FilePath $Exe -ArgumentList @('--kbdump') `
                        -WorkingDirectory $workDir -PassThru -Wait
    $sw.Stop()
    $probs = @()
    if ($p2.ExitCode -ne 3)  { $probs += "Exitcode $($p2.ExitCode) statt 3" }
    if ($sw.Elapsed.TotalSeconds -gt 15) { $probs += "hat $([int]$sw.Elapsed.TotalSeconds)s gebraucht" }
    if (-not (Test-Path $dump)) {
        $probs += "kein Report geschrieben"
    } elseif ((Get-Content $dump -Raw) -notmatch 'another OneClickRGB instance') {
        $probs += "Report nennt den Grund nicht"
    } elseif ((Get-Content $dump -Raw) -match '^\s*0000:') {
        $probs += "Report enthaelt Dumpzeilen - es wurde doch gelesen"
    }
    if ($probs.Count -eq 0) {
        Write-Host ("  OK   zweite Instanz bricht ab ({0:N1}s)" -f $sw.Elapsed.TotalSeconds) -ForegroundColor Green
        $pass++
    } else {
        Write-Host "  FAIL zweite Instanz wurde nicht sauber abgewiesen" -ForegroundColor Red
        $probs | ForEach-Object { Write-Host "         - $_" -ForegroundColor Red }
        $fail++
    }

    # Derselbe Test fuer den Pfad mit Bereichsargument: die Argumentpruefung
    # laeuft VOR der Sperre (ein kaputtes Argument soll auch dann benannt
    # werden, wenn gerade jemand anders misst), die Sperre aber vor jedem
    # HID-Zugriff. Beide Reihenfolgen zusammen sind das, was hier geprueft wird.
    #
    # Warum --keyidentify und --keypattern hier NICHT einzeln vorkommen: sie
    # wuerden im Fehlerfall - also genau dann, wenn die Sperre nicht greift -
    # auf die Hardware schreiben und einen Dialog oeffnen, in einem Skript, das
    # unbeaufsichtigt laeuft und laut eigener Zusage nichts schreibt. Sie nehmen
    # dieselbe Sperre ueber dieselben zwei Funktionen (AcquireProbeLock /
    # ProbeLockBusy) wie --kbdump; ein eigener Fall wuerde denselben Code
    # nochmal pruefen und dafuer das Risiko eintauschen.
    if (Test-Path $dump) { Remove-Item $dump -Force }
    $sw2 = [System.Diagnostics.Stopwatch]::StartNew()
    $p3 = Start-Process -FilePath $Exe -ArgumentList @('--kbdump','--kbdump-range=0x2C0-0x43F') `
                        -WorkingDirectory $workDir -PassThru -Wait
    $sw2.Stop()
    $probs2 = @()
    if ($p3.ExitCode -ne 3) { $probs2 += "Exitcode $($p3.ExitCode) statt 3" }
    if ($sw2.Elapsed.TotalSeconds -gt 15) { $probs2 += "hat $([int]$sw2.Elapsed.TotalSeconds)s gebraucht" }
    if (-not (Test-Path $dump)) {
        $probs2 += "kein Report geschrieben"
    } elseif ((Get-Content $dump -Raw) -match '^\s*02C0:') {
        $probs2 += "Report enthaelt Dumpzeilen - es wurde doch gelesen"
    }
    if ($probs2.Count -eq 0) {
        Write-Host ("  OK   --kbdump-range respektiert die Sperre ({0:N1}s)" -f $sw2.Elapsed.TotalSeconds) -ForegroundColor Green
        $pass++
    } else {
        Write-Host "  FAIL --kbdump-range hat die Sperre ignoriert" -ForegroundColor Red
        $probs2 | ForEach-Object { Write-Host "         - $_" -ForegroundColor Red }
        $fail++
    }
} finally {
    $held.ReleaseMutex()
    $held.Dispose()
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
