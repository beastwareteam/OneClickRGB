<#
    edge_probe_session.ps1 - Fuehrt eine vollstaendige Edge-Messung am Geraet
    durch und sichert sie ab.

    ACHTUNG: Dieses Skript schreibt auf die Tastatur. Es ist der einzige Teil der
    Testabdeckung, der das tut, und es braucht einen Menschen davor - der Beleg
    fuer "der Effekt laeuft" ist eine Leiste, die sich bewegt, und den kann kein
    Read-Back ersetzen (Regel 1).

    Ablauf, in dieser Reihenfolge:
      1. --kbdump  -> Referenzzustand des gesamten Konfigspeichers VORHER
      2. der Sweep -> schreibt ausschliesslich den Payload-Bereich des jeweiligen
         Geraetepfads: Edge profile_base+0x1E..0x27, Tastatur +0x01..0x12
      3. --kbdump  -> Zustand NACHHER
      4. kbdump_diff.ps1 -> belegt, dass sich nur diese Bytes bewegt haben. Das
         ist die Gegenprobe zu Regel 2, und sie laeuft automatisch, damit sie
         nicht "beim naechsten Mal" gemacht wird.

    Alle Dateien landen gesammelt in einem Sitzungsordner unter
    %APPDATA%\OneClickRGB\docs\sessions\, damit eine Messung spaeter noch
    zuordenbar ist.

    Aufruf:
        .\tools\edge_probe_session.ps1                      # Edge-Modus-Sweep 0x00..0x0A, 5s
        .\tools\edge_probe_session.ps1 -Hold 8
        .\tools\edge_probe_session.ps1 -What speed          # Tempo 0..5 bei Mode 0x03
        .\tools\edge_probe_session.ps1 -What speed -SpeedMode 0x01
        .\tools\edge_probe_session.ps1 -What kbmode         # Tastatur-Modus-Sweep 0x00..0x14
#>

[CmdletBinding()]
param(
    [ValidateSet('mode','speed','kbmode')] [string] $What = 'mode',
    [ValidateRange(1,60)]                  [int]    $Hold = 5,
    # Welcher Modus beim Tempo-Sweep gehalten wird (nur fuer -What speed).
    [string] $SpeedMode = "0x03",
    [string] $Exe = "",
    # Ueberspringt die Rueckfrage. Nur benutzen, wenn schon jemand hinsieht.
    [switch] $Yes
)

$ErrorActionPreference = 'Stop'

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if ([string]::IsNullOrWhiteSpace($Exe)) { $Exe = Join-Path $scriptDir "..\build\OneClickRGB.exe" }
if (-not (Test-Path $Exe)) {
    Write-Host "Executable nicht gefunden: $Exe" -ForegroundColor Red
    exit 1
}
$Exe = (Resolve-Path $Exe).Path
$diffTool = Join-Path $scriptDir "kbdump_diff.ps1"
$docsDir  = Join-Path $env:APPDATA "OneClickRGB\docs"

# Eine laufende Instanz wendet bei jeder Aenderung Einstellungen an und wuerde
# mitten in die Messung schreiben.
$running = Get-Process OneClickRGB -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "Es laeuft noch eine OneClickRGB-Instanz (PID $($running.Id -join ', '))." -ForegroundColor Yellow
    Write-Host "Sie wuerde in die Messung hineinschreiben. Bitte erst beenden." -ForegroundColor Yellow
    exit 1
}

$stamp      = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDir = Join-Path $docsDir "sessions\edge_$stamp"
New-Item -ItemType Directory -Force -Path $sessionDir | Out-Null

switch ($What) {
    'mode' {
        $probeArgs  = @("--edgemode-sweep=$Hold")
        $steps      = 11                     # 0x00..0x0A
        $label      = "Edge-Modus-Sweep 0x00..0x0A"
        $reportName = "edgemode_probe.txt"
        # Erlaubter Schreibbereich relativ zum Profilblock: der Edge-Payload.
        $slotLo, $slotHi = 0x1E, 0x27
        $target     = "Randbeleuchtung"
    }
    'speed' {
        $probeArgs  = @("--edgespeed-sweep=$Hold", "--edgespeed-mode=$SpeedMode")
        $steps      = 6                      # Tempo 0..5
        $label      = "Edge-Tempo-Sweep 0..5 bei Mode $SpeedMode"
        $reportName = "edgemode_probe.txt"
        $slotLo, $slotHi = 0x1E, 0x27
        $target     = "Randbeleuchtung"
    }
    'kbmode' {
        $probeArgs  = @("--kbmode-sweep=$Hold")
        $steps      = 21                     # 0x00..0x14
        $label      = "Tastatur-Modus-Sweep 0x00..0x14"
        $reportName = "kbmode_probe.txt"
        # SetEVisionKeyboard liest und schreibt 18 Bytes ab +0x01, also +0x01..+0x12.
        $slotLo, $slotHi = 0x01, 0x12
        $target     = "Tastaturbeleuchtung"
    }
}
$estSec = $steps * $Hold

Write-Host ""
Write-Host "=== Edge-Messung: $label ===" -ForegroundColor Cyan
Write-Host "Haltezeit pro Schritt: ${Hold}s   Schritte: $steps   Dauer: ca. ${estSec}s"
Write-Host "Sitzungsordner: $sessionDir"
Write-Host ""
Write-Host ("Was hier geschrieben wird: ausschliesslich profile_base+0x{0:X2}..0x{1:X2}," -f $slotLo, $slotHi)
Write-Host "ueber den Produktionspfad. Nur die Werte variieren, nie die Offsets."
Write-Host "Der Zustand von vorher wird am Ende zurueckgeschrieben."
Write-Host ""
Write-Host "WICHTIG: Die $target im Blick behalten und pro Schritt notieren," -ForegroundColor Yellow
Write-Host "         ob sich etwas bewegt. Der Report laesst dafuer eine Spalte frei." -ForegroundColor Yellow
Write-Host ""

if (-not $Yes) {
    $answer = Read-Host "Bereit? [j/N]"
    if ($answer -notmatch '^(j|J|y|Y)') { Write-Host "Abgebrochen - nichts geschrieben."; exit 130 }
}

function Invoke-Exe([string[]]$exeArgs, [string]$what) {
    Write-Host "-> $what" -ForegroundColor Gray
    $p = Start-Process -FilePath $script:Exe -ArgumentList $exeArgs -PassThru -Wait
    return $p.ExitCode
}

function Save-Report([string]$name, [string]$target) {
    $src = Join-Path $script:docsDir $name
    if (Test-Path $src) { Copy-Item $src (Join-Path $script:sessionDir $target) -Force; return $true }
    Write-Host "   WARNUNG: $name wurde nicht geschrieben" -ForegroundColor Yellow
    return $false
}

# --- 1. Referenzdump ---------------------------------------------------------
$rc = Invoke-Exe @("--kbdump") "kbdump (vorher)"
if (-not (Save-Report "kbdump.txt" "01_kbdump_before.txt")) { exit 1 }

$activeProfile = 0
foreach ($line in (Get-Content (Join-Path $sessionDir "01_kbdump_before.txt"))) {
    if ($line -match '^\s*activeProfile\s*=\s*(\d+)') { $activeProfile = [int]$Matches[1]; break }
}
$writeLo = $activeProfile * 0x40 + $slotLo
$writeHi = $activeProfile * 0x40 + $slotHi
$allowed = "0x{0:X2}-0x{1:X2}" -f $writeLo, $writeHi
Write-Host ("   aktives Profil P{0} -> erlaubter Schreibbereich {1}" -f $activeProfile, $allowed)

# --- 2. Sweep ----------------------------------------------------------------
Write-Host ""
$sweepRc = Invoke-Exe $probeArgs "$label (ca. ${estSec}s - jetzt hinsehen)"
Save-Report $reportName "03_$reportName" | Out-Null
Write-Host ("   Exitcode {0}{1}" -f $sweepRc, $(if ($sweepRc -ne 0) { "  <- Report lesen, etwas hat nicht verifiziert" } else { "" }))

# --- 3. Dump danach ----------------------------------------------------------
Write-Host ""
$rc = Invoke-Exe @("--kbdump") "kbdump (nachher)"
if (-not (Save-Report "kbdump.txt" "04_kbdump_after.txt")) { exit 1 }

# --- 4. Kollateral-Gegenprobe ------------------------------------------------
Write-Host ""
Write-Host "-> Kollateral-Gegenprobe (Regel 2)" -ForegroundColor Gray
$diffOut = & powershell -NoProfile -ExecutionPolicy Bypass -File $diffTool `
    -Before (Join-Path $sessionDir "01_kbdump_before.txt") `
    -After  (Join-Path $sessionDir "04_kbdump_after.txt") `
    -Allowed $allowed 2>&1
$diffRc = $LASTEXITCODE
$diffOut | Tee-Object -FilePath (Join-Path $sessionDir "05_collateral_diff.txt") | Out-Host

# --- Zusammenfassung ---------------------------------------------------------
Write-Host ""
Write-Host "=== Zusammenfassung ===" -ForegroundColor Cyan
Write-Host "Sitzungsordner : $sessionDir"
Write-Host "Messreport     : 03_$reportName"
Write-Host "Kollateral     : 05_collateral_diff.txt (Exitcode $diffRc)"
Write-Host ""
if ($diffRc -ne 0) {
    Write-Host "Es wurde ausserhalb von $allowed geschrieben. Das ist ein Verstoss gegen" -ForegroundColor Red
    Write-Host "Regel 2 und wichtiger als jedes Messergebnis - erst das klaeren." -ForegroundColor Red
} else {
    Write-Host "Nur der erlaubte Payload-Bereich hat sich bewegt." -ForegroundColor Green
}
Write-Host ""
$tableName = if ($What -eq 'kbmode') { "KB_MODE_TABLE" } else { "EDGE_MODE_TABLE" }
Write-Host "Jetzt noch von Hand: in 03_$reportName notieren, bei welcher Sekunde"
Write-Host "sich sichtbar etwas bewegt hat. Erst das entscheidet, welche Modus-Bytes"
Write-Host "in $tableName bleiben und was in Keyboard_Protocol.md als Confidence"
Write-Host "eingetragen wird. Ein 'accepted' im Report heisst nur gespeichert."

exit $diffRc
