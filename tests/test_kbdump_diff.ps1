<#
    test_kbdump_diff.ps1 - Selbsttest fuer tools\kbdump_diff.ps1.

    Kein Geraet beteiligt: die Dumps werden synthetisch erzeugt. Geprueft wird,
    dass das Werkzeug genau die Aenderungen findet und einordnet, um die es geht -
    insbesondere einen Schreibzugriff auf +0x14 (die Stelle, die der entfernte
    Null-Write in SetEVisionKeyboard getroffen hat) und einen auf einen inaktiven
    Profilblock (was die alte Brute-Force getan hat).

    Ein Pruefwerkzeug, das man nie gegen einen bekannten Fehler laufen laesst,
    ist selbst nur eine Behauptung.

    Aufruf: powershell -ExecutionPolicy Bypass -File tests\test_kbdump_diff.ps1
#>

[CmdletBinding()] param()
$ErrorActionPreference = 'Stop'

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$tool = Join-Path $scriptDir "..\tools\kbdump_diff.ps1"
if (-not (Test-Path $tool)) { Write-Host "kbdump_diff.ps1 nicht gefunden" -ForegroundColor Red; exit 1 }

$tmp = Join-Path ([IO.Path]::GetTempPath()) ("kbdump_diff_test_" + [Guid]::NewGuid().ToString("N").Substring(0,8))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

$pass = 0; $fail = 0

# Baut einen Dump im Format von --kbdump: 0x400 Bytes, 16 pro Zeile.
function New-Dump([hashtable]$overrides, [int]$activeProfile = 0, [int[]]$failedLines = @()) {
    $sb = [Text.StringBuilder]::new()
    [void]$sb.AppendLine("EVision GK650 config memory dump")
    [void]$sb.AppendLine("activeProfile=$activeProfile")
    [void]$sb.AppendLine("")
    for ($off = 0; $off -lt 0x400; $off += 16) {
        $line = "{0:X4}: " -f $off
        for ($i = 0; $i -lt 16; $i++) {
            $v = if ($overrides.ContainsKey($off + $i)) { $overrides[$off + $i] } else { 0 }
            $line += "{0:X2} " -f $v
        }
        $rr = if ($failedLines -contains $off) { -2 } else { 16 }
        [void]$sb.AppendLine("$line  rr=$rr")
    }
    $sb.ToString()
}

function Run-Case([string]$name, [string]$beforeText, [string]$afterText,
                  [string[]]$allowed, [int]$wantExit, [string]$wantMatch) {
    $b = Join-Path $script:tmp "before.txt"
    $a = Join-Path $script:tmp "after.txt"
    Set-Content -Path $b -Value $beforeText -Encoding ASCII
    Set-Content -Path $a -Value $afterText  -Encoding ASCII

    $out = & powershell -NoProfile -ExecutionPolicy Bypass -File $script:tool `
                -Before $b -After $a -Allowed $allowed 2>&1
    $rc = $LASTEXITCODE
    $text = ($out | Out-String)

    $problems = @()
    if ($rc -ne $wantExit) { $problems += "Exitcode $rc, erwartet $wantExit" }
    if ($wantMatch -and $text -notmatch $wantMatch) { $problems += "Ausgabe enthaelt '$wantMatch' nicht" }

    if ($problems.Count -eq 0) {
        Write-Host "  OK   $name" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "  FAIL $name" -ForegroundColor Red
        $problems | ForEach-Object { Write-Host "         - $_" -ForegroundColor Red }
        Write-Host ($text -split "`n" | ForEach-Object { "         | $_" }) -ForegroundColor DarkGray
        $script:fail++
    }
}

Write-Host ""
Write-Host "kbdump_diff.ps1 Selbsttest"
Write-Host ""

# Ausgangszustand: P0 mit Edge-Payload auf 0x1E und dem bekannten Muell in
# +0x14..0x1D, wie der echte Dump ihn zeigt.
$base = @{}
0x14,0x15,0x16 | ForEach-Object { $base[$_] = 0x00 }
$base[0x17]=0x04; $base[0x18]=0x02; $base[0x19]=0x00; $base[0x1A]=0x04
$base[0x1B]=0x00; $base[0x1C]=0x04; $base[0x1D]=0x02
$base[0x1E]=0x00; $base[0x1F]=0x04; $base[0x20]=0x02; $base[0x25]=0xFF; $base[0x27]=0x01
$before = New-Dump $base

# 1) identisch
Run-Case "identische Dumps -> keine Aenderung" $before $before @("0x1E-0x27") 0 "Kein einziges Byte"

# 2) nur das Modus-Byte im Edge-Payload
$m = $base.Clone(); $m[0x1E] = 0x03
Run-Case "Mode-Byte auf 0x1E -> erlaubt" $before (New-Dump $m) @("0x1E-0x27") 0 "im erlaubten Bereich"

# 3) alle zehn Payload-Bytes
$m = $base.Clone()
$m[0x1E]=0x03; $m[0x1F]=0x04; $m[0x20]=0x05; $m[0x21]=0x00; $m[0x22]=0x00
$m[0x23]=0x11; $m[0x24]=0x22; $m[0x25]=0x33; $m[0x26]=0x00; $m[0x27]=0x01
Run-Case "ganzer Payload 0x1E..0x27 -> erlaubt" $before (New-Dump $m) @("0x1E-0x27") 0 "im erlaubten Bereich"

# 4) DER Fall: Schreibzugriff auf +0x14, die Stelle des entfernten Null-Writes
$m = $base.Clone(); $m[0x14] = 0x04; $m[0x15] = 0x02
Run-Case "Schreibzugriff auf +0x14/0x15 -> Verstoss" $before (New-Dump $m) @("0x1E-0x27") 1 "P0 \+ 0x14"

# 5) fremder Profilblock (das Muster der alten Brute-Force)
$m = $base.Clone(); $m[0x54] = 0x04; $m[0x94] = 0x04
Run-Case "P1/P2 beruehrt -> Verstoss" $before (New-Dump $m) @("0x1E-0x27") 1 "P1 \+ 0x14"

# 6) Key-Remap-Tabelle. Der Wert muss sich vom Ausgangsdump unterscheiden -
#    im ersten Versuch stand hier 0x00 auf 0x00, also gar keine Aenderung, und
#    der Testfall hat nicht das geprueft, was sein Name behauptet.
$m = $base.Clone(); $m[0xE1] = 0x08
Run-Case "Key-Remap-Tabelle 0xC0+ -> Verstoss" $before (New-Dump $m) @("0x1E-0x27") 1 "Key-Remap"

# 7) anderer Profilblock ausdruecklich erlaubt (P1: 0x5E..0x67)
$m = $base.Clone(); $m[0x5E] = 0x03
Run-Case "-Allowed 0x5E-0x67 fuer P1" $before (New-Dump $m) @("0x5E-0x67") 0 "im erlaubten Bereich"

# 8) Zeilen mit rr<0 enthalten keine Messwerte und duerfen nicht als 16
#    Aenderungen gezaehlt werden.
$failed = New-Dump $base 0 @(0x100)
$m = $base.Clone(); $m[0x100] = 0x77
Run-Case "rr<0-Zeilen werden ignoriert" $failed (New-Dump $m) @("0x1E-0x27") 0 "Kein einziges Byte"

# 9) aktives Profil hat gewechselt -> Warnung, aber kein Abbruch
Run-Case "activeProfile-Wechsel warnt" $before (New-Dump $base 1) @("0x1E-0x27") 0 "activeProfile hat sich geaendert"

# 10) unlesbare Eingabe faellt auf, statt "keine Aenderung" zu behaupten
Set-Content -Path (Join-Path $tmp "junk.txt") -Value "kein dump" -Encoding ASCII
$b = Join-Path $tmp "before.txt"
$junkOut = ""
$junkRc = 0
# ErrorActionPreference lokal entschaerfen: ein Fehlertext auf stderr aus dem
# Kindprozess wuerde diesen Selbsttest sonst mitten im Lauf abbrechen.
$savedEAP = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try {
    $junkOut = (& powershell -NoProfile -ExecutionPolicy Bypass -File $tool `
                    -Before $b -After (Join-Path $tmp "junk.txt") 2>&1 | Out-String)
    $junkRc = $LASTEXITCODE
} finally { $ErrorActionPreference = $savedEAP }

if ($junkRc -ne 0 -and $junkOut -match 'FEHLER') {
    Write-Host "  OK   unlesbarer Dump -> Fehler statt 'keine Aenderung'" -ForegroundColor Green
    $pass++
} else {
    Write-Host "  FAIL unlesbarer Dump: Exitcode $junkRc, Ausgabe: $($junkOut.Trim())" -ForegroundColor Red
    $fail++
}

Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "------------------------------------------------"
if ($fail -eq 0) { Write-Host "ALLE $pass PRUEFUNGEN OK" -ForegroundColor Green; exit 0 }
Write-Host "$fail von $($pass + $fail) Pruefungen fehlgeschlagen" -ForegroundColor Red
exit 1
