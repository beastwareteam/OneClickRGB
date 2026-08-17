<#
    keycolor_map_session.ps1 - Stufe 1: die Per-Tasten-Farbtabelle read-only
    dekodieren, per Differenzdump.

    ACHTUNG: Dieses Skript schreibt auf die Tastatur - aber ausschliesslich in
    den Profilblock (globale Farbe ueber den Produktionspfad, --kbmode=0x06
    --kbcolor=RRGGBB). In die Farbtabelle ab 0x2C0 wird NICHT geschrieben. Genau
    das ist der Punkt: erst wenn belegt ist, welche Bytes sich mit der Farbe
    bewegen, ist ein gezielter Write dorthin dokumentiert und regelkonform
    (CLAUDE.md Regel 2).

    Die Frage, die hier beantwortet wird:

      Ab 0x2C0 steht im Dump lueckenlos "00 22 FF" - exakt die Profilfarbe. Das
      legt eine Farbtabelle nahe, beweist sie aber nicht: dieselben drei Bytes
      koennten alles Moegliche sein, was zufaellig so aussieht. Ab 0x2F0 kippt
      das Muster ausserdem auf eine 16-Byte-Periode, und der alte Dump endete bei
      0x400 - die Tabelle offensichtlich nicht (bei einem Tripel je Matrixplatz
      liefe sie bis 0x43A).

      Ein Differenzdump entscheidet das ohne jede Vermutung: Farbe A schreiben,
      dumpen, Farbe B schreiben, dumpen. Die Bytes, die sich mitbewegt haben,
      SIND die Tabelle. Ausdehnung, Stride und Reihenfolge fallen direkt an.

    Ablauf:
      1. --kbdump-range  -> Referenzzustand inkl. des Bereichs jenseits 0x400
      2. Farbe A setzen  -> Dump A
      3. Farbe B setzen  -> Dump B
      4. Diff A/B        -> das ist das Messergebnis
      5. Ursprungsfarbe zurueckschreiben -> Dump danach
      6. Kollateral-Gegenprobe vorher/nachher (Regel 2)

    Alles landet in %APPDATA%\OneClickRGB\docs\sessions\keycolor_<stamp>\.

    Aufruf:
        .\tools\keycolor_map_session.ps1
        .\tools\keycolor_map_session.ps1 -ColorA FF0000 -ColorB 00FF00
        .\tools\keycolor_map_session.ps1 -RangeHi 0x5FF
#>

[CmdletBinding()]
param(
    [ValidatePattern('^[0-9A-Fa-f]{6}$')] [string] $ColorA = "FF0000",
    [ValidatePattern('^[0-9A-Fa-f]{6}$')] [string] $ColorB = "00FF00",
    # Wie weit gedumpt wird. Default reicht bewusst ueber 0x43A hinaus: wo der
    # Konfigspeicher endet, ist selbst eine offene Frage, und der Dump meldet
    # den Offset, ab dem das Geraet nicht mehr antwortet.
    [string] $RangeLo = "0x000",
    [string] $RangeHi = "0x5FF",
    [string] $Exe = "",
    [switch] $Yes
)

$ErrorActionPreference = 'Stop'

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if ([string]::IsNullOrWhiteSpace($Exe)) { $Exe = Join-Path $scriptDir "..\build\OneClickRGB.exe" }
if (-not (Test-Path $Exe)) {
    Write-Host "Executable nicht gefunden: $Exe" -ForegroundColor Red
    Write-Host "Vorher build_native.bat laufen lassen."
    exit 1
}
$Exe = (Resolve-Path $Exe).Path
$diffTool = Join-Path $scriptDir "kbdump_diff.ps1"
$docsDir  = Join-Path $env:APPDATA "OneClickRGB\docs"

if ($ColorA -eq $ColorB) {
    Write-Host "Farbe A und B sind gleich - der Diff waere per Konstruktion leer." -ForegroundColor Red
    exit 1
}

# Eine laufende Instanz wendet bei jeder Aenderung Einstellungen an und wuerde
# mitten in die Messung schreiben. Ausserdem haelt jeder Probe die Instanzsperre;
# eine zweite Instanz wuerde ohnehin mit Exit 3 abbrechen.
$running = Get-Process OneClickRGB -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "Es laeuft noch eine OneClickRGB-Instanz (PID $($running.Id -join ', '))." -ForegroundColor Yellow
    Write-Host "Sie wuerde in die Messung hineinschreiben. Bitte erst beenden." -ForegroundColor Yellow
    exit 1
}

$stamp      = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDir = Join-Path $docsDir "sessions\keycolor_$stamp"
New-Item -ItemType Directory -Force -Path $sessionDir | Out-Null

Write-Host ""
Write-Host "=== Stufe 1: Farbtabelle per Differenzdump ===" -ForegroundColor Cyan
Write-Host "Dumpbereich    : $RangeLo-$RangeHi"
Write-Host "Farbe A / B    : $ColorA / $ColorB"
Write-Host "Sitzungsordner : $sessionDir"
Write-Host ""
Write-Host "Geschrieben wird nur der Profilblock (globale Farbe, Modus 0x06)."
Write-Host "In die Farbtabelle ab 0x2C0 schreibt dieses Skript nichts."
Write-Host ""

if (-not $Yes) {
    $answer = Read-Host "Bereit? [j/N]"
    if ($answer -notmatch '^(j|J|y|Y)') { Write-Host "Abgebrochen - nichts geschrieben."; exit 130 }
}

function Invoke-Exe([string[]]$exeArgs, [string]$what) {
    Write-Host "-> $what" -ForegroundColor Gray
    $p = Start-Process -FilePath $script:Exe -ArgumentList $exeArgs -PassThru -Wait
    if ($p.ExitCode -ne 0) {
        Write-Host "   Exitcode $($p.ExitCode)" -ForegroundColor Yellow
    }
    return $p.ExitCode
}

function Save-Report([string]$name, [string]$target) {
    $src = Join-Path $script:docsDir $name
    if (Test-Path $src) { Copy-Item $src (Join-Path $script:sessionDir $target) -Force; return $true }
    Write-Host "   WARNUNG: $name wurde nicht geschrieben" -ForegroundColor Yellow
    return $false
}

function Take-Dump([string]$target, [string]$what) {
    $rc = Invoke-Exe @("--kbdump", "--kbdump-range=$script:RangeLo-$script:RangeHi") $what
    if (-not (Save-Report "kbdump.txt" $target)) { exit 1 }
    return $rc
}

# --- 0. Referenzdump ---------------------------------------------------------
Take-Dump "01_kbdump_before.txt" "kbdump (vorher)" | Out-Null
$beforePath = Join-Path $sessionDir "01_kbdump_before.txt"

# Die Ursprungsfarbe steht im aktiven Profilblock bei +0x06..+0x08. Sie wird aus
# dem Dump gelesen und am Ende zurueckgeschrieben - eine Messung, die die
# Beleuchtung auf Gruen stehen laesst, hat den Zustand veraendert, den sie nur
# beobachten sollte.
$activeProfile = 0
$mem = @{}
foreach ($line in (Get-Content $beforePath)) {
    if ($line -match '^\s*activeProfile\s*=\s*(\d+)') { $activeProfile = [int]$Matches[1]; continue }
    if ($line -notmatch '^([0-9A-Fa-f]{4}):\s+((?:[0-9A-Fa-f]{2}\s+){1,16})') { continue }
    if ($line -match 'rr=(-?\d+)' -and [int]$Matches[1] -lt 0) { continue }
    $base = [Convert]::ToInt32($Matches[1], 16)
    $hex  = ($Matches[2].Trim() -split '\s+')
    for ($i = 0; $i -lt $hex.Count; $i++) { $mem[$base + $i] = [Convert]::ToInt32($hex[$i], 16) }
}
$colBase = $activeProfile * 0x40 + 0x06
$origColor = $null
if ($mem.ContainsKey($colBase) -and $mem.ContainsKey($colBase + 1) -and $mem.ContainsKey($colBase + 2)) {
    $origColor = "{0:X2}{1:X2}{2:X2}" -f $mem[$colBase], $mem[$colBase + 1], $mem[$colBase + 2]
    Write-Host ("   aktives Profil P{0}, Ursprungsfarbe {1} (aus +0x06..0x08)" -f $activeProfile, $origColor)
} else {
    Write-Host "   WARNUNG: Ursprungsfarbe nicht lesbar - am Ende wird nichts zurueckgesetzt." -ForegroundColor Yellow
}

# --- 1./2. Farbe A -----------------------------------------------------------
Write-Host ""
Invoke-Exe @("--kbmode=0x06", "--kbcolor=$ColorA") "Farbe A ($ColorA) setzen" | Out-Null
Save-Report "kbmode_probe.txt" "02_kbmode_colorA.txt" | Out-Null
Take-Dump "03_kbdump_colorA.txt" "kbdump (Farbe A)" | Out-Null

# --- 3. Farbe B --------------------------------------------------------------
Write-Host ""
Invoke-Exe @("--kbmode=0x06", "--kbcolor=$ColorB") "Farbe B ($ColorB) setzen" | Out-Null
Save-Report "kbmode_probe.txt" "04_kbmode_colorB.txt" | Out-Null
Take-Dump "05_kbdump_colorB.txt" "kbdump (Farbe B)" | Out-Null

# --- 4. Das Messergebnis -----------------------------------------------------
# Erlaubt ist hier der Profilblock: dort SOLL sich die Farbe aendern. Alles
# andere, was sich bewegt hat, wird als Verstoss markiert - und genau diese
# Markierungen sind das gesuchte Ergebnis, nicht ein Fehler. Deswegen wird der
# Exitcode dieses Diffs auch nicht als Misserfolg gewertet.
Write-Host ""
Write-Host "-> Differenz Farbe A gegen Farbe B (das ist die Messung)" -ForegroundColor Gray
$profLo = $activeProfile * 0x40 + 0x01
$profHi = $activeProfile * 0x40 + 0x12
$profRange = "0x{0:X2}-0x{1:X2}" -f $profLo, $profHi
$diffAB = & powershell -NoProfile -ExecutionPolicy Bypass -File $diffTool `
    -Before (Join-Path $sessionDir "03_kbdump_colorA.txt") `
    -After  (Join-Path $sessionDir "05_kbdump_colorB.txt") `
    -Allowed $profRange 2>&1
$diffAB | Tee-Object -FilePath (Join-Path $sessionDir "06_diff_colorA_colorB.txt") | Out-Host

# --- 5. Ursprungsfarbe zurueck ----------------------------------------------
Write-Host ""
if ($origColor) {
    Invoke-Exe @("--kbmode=0x06", "--kbcolor=$origColor") "Ursprungsfarbe ($origColor) zurueckschreiben" | Out-Null
    Save-Report "kbmode_probe.txt" "07_kbmode_restore.txt" | Out-Null
}
Take-Dump "08_kbdump_after.txt" "kbdump (nachher)" | Out-Null

# --- 6. Kollateral-Gegenprobe ------------------------------------------------
Write-Host ""
Write-Host "-> Kollateral-Gegenprobe vorher/nachher (Regel 2)" -ForegroundColor Gray
$collateral = & powershell -NoProfile -ExecutionPolicy Bypass -File $diffTool `
    -Before $beforePath `
    -After  (Join-Path $sessionDir "08_kbdump_after.txt") `
    -Allowed $profRange 2>&1
$collRc = $LASTEXITCODE
$collateral | Tee-Object -FilePath (Join-Path $sessionDir "09_collateral_diff.txt") | Out-Host

# --- Zusammenfassung ---------------------------------------------------------
Write-Host ""
Write-Host "=== Zusammenfassung ===" -ForegroundColor Cyan
Write-Host "Sitzungsordner : $sessionDir"
Write-Host "Messung        : 06_diff_colorA_colorB.txt"
Write-Host "Kollateral     : 09_collateral_diff.txt (Exitcode $collRc)"
Write-Host ""
Write-Host "So wird 06_diff_colorA_colorB.txt gelesen:" -ForegroundColor Yellow
Write-Host "  - Jeder Offset ausserhalb $profRange, der sich geaendert hat, gehoert zu"
Write-Host "    etwas, das die globale Farbe mitfuehrt. Liegen diese Offsets ab 0x2C0"
Write-Host "    in einem gleichmaessigen Raster, ist das die Farbtabelle - und der"
Write-Host "    Abstand zwischen ihnen ist der Stride."
Write-Host "  - Der hoechste geaenderte Offset ist die untere Schranke fuer die"
Write-Host "    Ausdehnung. Bewegt sich bei 0x2F0+ nichts, ist die 16-Byte-Periode"
Write-Host "    dort etwas anderes und die Tabelle endet vorher."
Write-Host "  - Aendert sich ausserhalb des Profilblocks GAR nichts, dann fuellt die"
Write-Host "    Firmware die Tabelle nicht aus der globalen Farbe. Auch das ist ein"
Write-Host "    Ergebnis - und es heisst, dass Stufe 2 einen anderen Weg braucht."
Write-Host ""
Write-Host "Das Ergebnis gehoert nach docs\Keyboard_Protocol.md Abschnitt 5 Punkt 5,"
Write-Host "mit Confidence. Erst danach darf irgendetwas nach 0x2C0 schreiben."
if ($collRc -ne 0) {
    Write-Host ""
    Write-Host "ACHTUNG: vorher/nachher unterscheiden sich ausserhalb $profRange." -ForegroundColor Red
    Write-Host "Wenn das die Farbtabelle ist, ist es erwartet (die Farbe steht wieder auf" -ForegroundColor Red
    Write-Host "dem Ursprungswert - dann duerfte sie sich nicht unterscheiden). Erst" -ForegroundColor Red
    Write-Host "klaeren, bevor irgendetwas daraus abgeleitet wird." -ForegroundColor Red
}

exit 0
