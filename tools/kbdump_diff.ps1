<#
    kbdump_diff.ps1 - Vergleicht zwei --kbdump-Ausgaben Byte fuer Byte und
    prueft, ob sich ausschliesslich die erwarteten Offsets bewegt haben.

    Rein lesend. Aendert weder Hardware noch Dateien.

    Hintergrund: Regel 2 (CLAUDE.md) verbietet Schreibzugriffe ausserhalb der
    dokumentierten Byte-Bereiche. Die alte 15-Offset-Brute-Force in
    SetEVisionEdge hat genau dagegen verstossen und +0x14..0x1D in allen drei
    Profilen zerstoert. Der Beleg dafuer, dass das nicht mehr passiert, ist ein
    Dump vor und nach dem Schreibvorgang - und der Vergleich von Hand ueber
    0x400 Bytes ist genau die Arbeit, die man einmal macht und danach nie wieder.
    Dieses Skript macht sie reproduzierbar.

    Beispiel (Edge-Payload des aktiven Profils, P0):
        .\tools\kbdump_diff.ps1 -Before 01_before.txt -After 02_after.txt

    Anderer Profilblock (P1 -> 0x5E..0x67):
        .\tools\kbdump_diff.ps1 -Before a.txt -After b.txt -Allowed "0x5E-0x67"

    Exitcode 0 = nur erlaubte Offsets geaendert (oder gar keine).
    Exitcode 1 = Aenderung ausserhalb des erlaubten Bereichs, oder Eingabefehler.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]   $Before,
    [Parameter(Mandatory = $true)] [string]   $After,
    # Erlaubte Bereiche als "0x1E-0x27" oder Einzeloffsets "0x1E". Mehrere
    # Angaben moeglich. Standard: der Edge-Payload des aktiven Profils P0.
    [string[]] $Allowed = @("0x1E-0x27")
)

$ErrorActionPreference = 'Stop'

function Read-Dump([string]$path) {
    if (-not (Test-Path $path)) { throw "Dump nicht gefunden: $path" }
    $bytes = @{}
    $activeProfile = $null
    foreach ($line in (Get-Content $path)) {
        if ($line -match '^\s*activeProfile\s*=\s*(\d+)') { $activeProfile = [int]$Matches[1]; continue }
        # Format aus --kbdump:  "0010: 00 00 .. 04   rr=16"
        if ($line -notmatch '^([0-9A-Fa-f]{4}):\s+((?:[0-9A-Fa-f]{2}\s+){1,16})') { continue }
        $base = [Convert]::ToInt32($Matches[1], 16)
        $hex  = ($Matches[2].Trim() -split '\s+')
        # Eine Zeile mit rr<0 enthaelt keine gelesenen Daten, sondern Nullen aus
        # dem Puffer. Als Messwert zaehlt sie nicht - sonst diffen wir gegen
        # einen fehlgeschlagenen Read und melden 16 "Aenderungen".
        $rr = $null
        if ($line -match 'rr=(-?\d+)') { $rr = [int]$Matches[1] }
        if ($null -ne $rr -and $rr -lt 0) { continue }
        for ($i = 0; $i -lt $hex.Count; $i++) {
            $bytes[$base + $i] = [Convert]::ToInt32($hex[$i], 16)
        }
    }
    if ($bytes.Count -eq 0) { throw "Keine Dump-Zeilen erkannt in: $path" }
    [pscustomobject]@{ Path = $path; Bytes = $bytes; ActiveProfile = $activeProfile }
}

function Parse-Ranges([string[]]$spec) {
    $ranges = @()
    foreach ($s in $spec) {
        if ($s -match '^\s*(0x[0-9A-Fa-f]+|\d+)\s*-\s*(0x[0-9A-Fa-f]+|\d+)\s*$') {
            $lo = [Convert]::ToInt32($Matches[1], $(if ($Matches[1] -like '0x*') { 16 } else { 10 }))
            $hi = [Convert]::ToInt32($Matches[2], $(if ($Matches[2] -like '0x*') { 16 } else { 10 }))
            if ($hi -lt $lo) { throw "Ungueltiger Bereich: $s" }
            $ranges += ,@($lo, $hi)
        }
        elseif ($s -match '^\s*(0x[0-9A-Fa-f]+|\d+)\s*$') {
            $v = [Convert]::ToInt32($Matches[1], $(if ($Matches[1] -like '0x*') { 16 } else { 10 }))
            $ranges += ,@($v, $v)
        }
        else { throw "Ungueltige -Allowed-Angabe: $s" }
    }
    # Ohne das Komma rollt PowerShell die aeussere Liste beim Rueckgeben auf:
    # aus @(@(30,39)) wird @(30,39), und die Bereichspruefung vergleicht danach
    # gegen zwei Zahlen statt gegen einen Bereich. Der Selbsttest hat genau das
    # gefunden (0x1E wurde als "ausserhalb 0x1E..0x27" gemeldet).
    return ,$ranges
}

function Format-Ranges($ranges) {
    ($ranges | ForEach-Object { "0x{0:X2}..0x{1:X2}" -f $_[0], $_[1] }) -join ", "
}

function Test-InRanges([int]$off, $ranges) {
    foreach ($r in $ranges) { if ($off -ge $r[0] -and $off -le $r[1]) { return $true } }
    return $false
}

try {
    $b = Read-Dump $Before
    $a = Read-Dump $After
    $ranges = Parse-Ranges $Allowed
} catch {
    Write-Host "FEHLER: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host "vorher : $($b.Path)  (activeProfile=$($b.ActiveProfile), $($b.Bytes.Count) Bytes)"
Write-Host "nachher: $($a.Path)  (activeProfile=$($a.ActiveProfile), $($a.Bytes.Count) Bytes)"
Write-Host ("erlaubt: " + (Format-Ranges $ranges))
Write-Host ""

$exit = 0

if ($b.ActiveProfile -ne $a.ActiveProfile) {
    Write-Host "WARNUNG: activeProfile hat sich geaendert ($($b.ActiveProfile) -> $($a.ActiveProfile))." -ForegroundColor Yellow
    Write-Host "         Die Offsets beider Dumps zeigen damit auf verschiedene Profilbloecke." -ForegroundColor Yellow
}

# Offsets, die nur in einem der Dumps gelesen wurden, sind kein Diff-Ergebnis,
# sondern eine unterschiedliche Abdeckung. Sie werden getrennt gemeldet.
$onlyBefore = @($b.Bytes.Keys | Where-Object { -not $a.Bytes.ContainsKey($_) })
$onlyAfter  = @($a.Bytes.Keys | Where-Object { -not $b.Bytes.ContainsKey($_) })
if ($onlyBefore.Count -or $onlyAfter.Count) {
    Write-Host ("HINWEIS: unterschiedliche Abdeckung - nur vorher: {0} Bytes, nur nachher: {1} Bytes" -f $onlyBefore.Count, $onlyAfter.Count) -ForegroundColor Yellow
}

$common = @($b.Bytes.Keys | Where-Object { $a.Bytes.ContainsKey($_) } | Sort-Object)
$changed = @()
foreach ($off in $common) {
    if ($b.Bytes[$off] -ne $a.Bytes[$off]) {
        $ok = Test-InRanges $off $ranges
        $changed += [pscustomobject]@{
            Offset  = $off
            Before  = $b.Bytes[$off]
            After   = $a.Bytes[$off]
            Erlaubt = $ok
        }
    }
}

if ($changed.Count -eq 0) {
    Write-Host "Kein einziges Byte hat sich geaendert." -ForegroundColor Green
    Write-Host "Achtung: das ist nur dann ein gutes Ergebnis, wenn auch nichts geschrieben"
    Write-Host "werden sollte. Nach einem Sweep bedeutet es, dass der Write nicht angekommen ist."
    exit 0
}

Write-Host ("{0} geaenderte Bytes:" -f $changed.Count)
foreach ($c in $changed) {
    $mark = if ($c.Erlaubt) { "ok " } else { "!! " }
    $color = if ($c.Erlaubt) { "Gray" } else { "Red" }
    Write-Host ("  {0}0x{1:X4}: {2:X2} -> {3:X2}" -f $mark, $c.Offset, $c.Before, $c.After) -ForegroundColor $color
}

$outside = @($changed | Where-Object { -not $_.Erlaubt })
Write-Host ""
if ($outside.Count -eq 0) {
    Write-Host ("OK - alle {0} Aenderungen liegen im erlaubten Bereich." -f $changed.Count) -ForegroundColor Green
} else {
    Write-Host ("VERSTOSS gegen Regel 2 - {0} Byte(s) ausserhalb des erlaubten Bereichs:" -f $outside.Count) -ForegroundColor Red
    foreach ($c in $outside) {
        $prof = [Math]::Floor($c.Offset / 0x40)
        $rel  = $c.Offset % 0x40
        if ($c.Offset -lt 0xC0) {
            Write-Host ("  0x{0:X4} = Profil P{1} + 0x{2:X2}" -f $c.Offset, $prof, $rel) -ForegroundColor Red
        } else {
            Write-Host ("  0x{0:X4} liegt in der Key-Remap-/Farbtabelle (0xC0+)" -f $c.Offset) -ForegroundColor Red
        }
    }
    $exit = 1
}

exit $exit
