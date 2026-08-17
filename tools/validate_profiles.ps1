<#
    validate_profiles.ps1 - Prueft die gespeicherten Profile in
    %APPDATA%\OneClickRGB\profiles gegen das, was LoadProfile()
    in src/oneclick_rgb_complete.cpp tatsaechlich akzeptiert.

    Rein lesend. Aendert nichts.

    Hintergrund: LoadProfile() ist bewusst nachsichtig - unbekannte Schluessel
    werden ignoriert, fehlende Schluessel behalten den aktuellen Laufzeitwert,
    und ein nicht parsebarer Wert wird uebersprungen. Ein Profil kann also
    "funktionieren" und trotzdem unvollstaendig sein: die fehlenden Felder
    kommen dann still aus dem vorherigen Zustand statt aus der Datei.
    Genau das macht dieser Check sichtbar.

    Seit den Edge-Fixes klemmt LoadProfile() brightness und speed beim Einlesen
    (ClampBrightness/ClampSpeed aus src/effect_limits.h), so wie edgeMode schon
    immer durch NormalizeEdgeMode() lief. Ein Wert ausserhalb 0..4 bzw. 0..5
    gelangt also nicht mehr in den Profilblock der Tastatur - er wird gekappt.
    Dieses Skript meldet ihn weiterhin, weil eine Datei, die geklemmt werden
    muss, etwas anderes enthaelt als das, was die App danach benutzt.
#>

$profileDir = Join-Path $env:APPDATA "OneClickRGB\profiles"

# Die 12 Schluessel, die LoadProfile() auswertet, mit ihren gueltigen Bereichen.
# Bereiche stammen aus den TBM_SETRANGE-Aufrufen bzw. den Modus-Tabellen.
$schema = [ordered]@{
    red            = @{ min = 0; max = 255 }
    green          = @{ min = 0; max = 255 }
    blue           = @{ min = 0; max = 255 }
    brightness     = @{ min = 0; max = 4   }   # TBM_SETRANGE 0..4
    speed          = @{ min = 0; max = 5   }   # TBM_SETRANGE 0..5
    kbMode         = @{ allowed = @(0x06,0x05,0x04,0x01,0x02,0x03,0x07,0x08,0x0A,0x0C,0x0D) }
    edgeMode       = @{ allowed = @(0x00,0x01,0x02,0x03,0x05); legacy = @(0x04) }
    enableAura     = @{ allowed = @(0,1) }
    enableMouse    = @{ allowed = @(0,1) }
    enableKeyboard = @{ allowed = @(0,1) }
    enableRAM      = @{ allowed = @(0,1) }
    enableEdge     = @{ allowed = @(0,1) }
}

$kbModeNames = @{
    0x01='Wave Short'; 0x02='Wave Long'; 0x03='Color Wheel'; 0x04='Spectrum'
    0x05='Breathing';  0x06='Static';    0x07='Reactive';    0x08='Ripple'
    0x0A='Starlight';  0x0C='Rainbow';   0x0D='Hurricane'
}
$edgeModeNames = @{ 0x00='Static/Freeze'; 0x01='Wave'; 0x02='Spectrum'; 0x03='Breathing'; 0x05='Off' }

if (-not (Test-Path $profileDir)) {
    Write-Host "Profilordner nicht gefunden: $profileDir" -ForegroundColor Red
    exit 1
}

Write-Host "Profilordner: $profileDir`n"

# --- Dateien, die die App gar nicht sieht -----------------------------------
# RefreshProfileList() sucht ausschliesslich nach *.rgb. Alles andere im
# Ordner ist fuer die App unsichtbar, auch wenn es wie ein Profil aussieht.
$ignored = Get-ChildItem $profileDir -File | Where-Object { $_.Extension -ne '.rgb' }
if ($ignored) {
    Write-Host "UNSICHTBAR fuer die App (RefreshProfileList liest nur *.rgb):" -ForegroundColor Yellow
    $ignored | ForEach-Object { Write-Host ("  {0}  ({1} Bytes, {2:dd.MM.yyyy})" -f $_.Name,$_.Length,$_.LastWriteTime) }
    Write-Host ""
}

$files = Get-ChildItem $profileDir -Filter *.rgb -File
if (-not $files) { Write-Host "Keine .rgb-Profile vorhanden." -ForegroundColor Yellow; exit 0 }

$exit = 0

foreach ($f in $files) {
    Write-Host ("=== {0} ===" -f $f.BaseName) -ForegroundColor Cyan
    Write-Host ("    {0} Bytes, zuletzt geaendert {1:dd.MM.yyyy HH:mm:ss}" -f $f.Length,$f.LastWriteTime)

    $found = @{}
    $problems = @()
    # Hinweise sind keine Fehler: gueltige Werte, die aber erklaeren, warum
    # nichts zu sehen ist. Sie aendern den Exitcode nicht.
    $hints = @()
    $lineNo = 0

    foreach ($line in (Get-Content $f.FullName)) {
        $lineNo++
        if ($line.Trim() -eq '') { continue }
        $eq = $line.IndexOf('=')
        if ($eq -lt 0) { $problems += "Zeile ${lineNo}: kein '=' -> wird ignoriert: '$line'"; continue }

        $key = $line.Substring(0, $eq)
        $raw = $line.Substring($eq + 1)

        if (-not $schema.Contains($key)) { $problems += "Zeile ${lineNo}: unbekannter Schluessel '$key' -> wird ignoriert"; continue }
        if ($found.ContainsKey($key))    { $problems += "Zeile ${lineNo}: '$key' doppelt -> letzter Wert gewinnt" }

        $val = 0
        if (-not [int]::TryParse($raw, [ref]$val)) {
            $problems += "Zeile ${lineNo}: '$key' hat keinen Zahlenwert ('$raw') -> Zeile wird uebersprungen, Feld behaelt alten Wert"
            continue
        }
        $found[$key] = $val

        $rule = $schema[$key]
        if ($rule.Contains('min')) {
            if ($val -lt $rule.min -or $val -gt $rule.max) {
                $clamped = [Math]::Max($rule.min, [Math]::Min($rule.max, $val))
                $problems += "'$key' = $val liegt ausserhalb $($rule.min)..$($rule.max) -> LoadProfile klemmt auf $clamped"
            }
        }
        elseif ($rule.Contains('allowed')) {
            if ($rule.allowed -notcontains $val) {
                if ($rule.Contains('legacy') -and $rule.legacy -contains $val) {
                    $problems += "'$key' = 0x{0:X2} ist ein Legacy-Wert, NormalizeEdgeMode() bildet ihn auf Static ab" -f $val
                } else {
                    $problems += ("'$key' = {0} (0x{0:X2}) ist kein gueltiger Modus" -f $val)
                }
            }
        }
    }

    $missing = $schema.Keys | Where-Object { -not $found.ContainsKey($_) }
    if ($missing) {
        $problems += "fehlende Schluessel: $($missing -join ', ') -> diese Felder kommen beim Laden still aus dem vorherigen Zustand, NICHT aus dem Profil"
    }

    # --- Werte, die im Bereich liegen und trotzdem "nichts passiert" bedeuten ---
    # speed=0 und brightness=0 sind gueltig, aber sie sind auch die haeufigste
    # Erklaerung fuer "der Effekt laeuft nicht" bzw. "die LEDs sind aus". Ein
    # Profil mit einem Animationsmodus und speed=0 friert jeden Effekt beim Laden
    # wieder ein - solange die Polaritaet von +0x03/+0x20 nicht gemessen ist
    # (Keyboard_Protocol.md 5.7), ist nicht einmal sicher, welches Ende das
    # langsame ist. Deshalb Hinweis, nicht Fehler.
    $animatedKb   = @(0x01,0x02,0x03,0x04,0x05,0x07,0x08,0x0A,0x0C,0x0D)
    $animatedEdge = @(0x01,0x02,0x03)
    if ($found.ContainsKey('speed') -and [int]$found['speed'] -eq 0) {
        $kbAnim   = $found.ContainsKey('kbMode')   -and ($animatedKb   -contains [int]$found['kbMode'])
        $edgeAnim = $found.ContainsKey('edgeMode') -and ($animatedEdge -contains [int]$found['edgeMode'])
        if ($kbAnim -or $edgeAnim) {
            $which = @(); if ($kbAnim) { $which += 'kbMode' }; if ($edgeAnim) { $which += 'edgeMode' }
            $hints += "speed=0 zusammen mit einem Animationsmodus ($($which -join ' + ')) -> beim Laden steht der Effekt still"
        } else {
            $hints += "speed=0 -> bei einem statischen Modus ohne Wirkung, bei einem Effekt wuerde er stillstehen"
        }
    }
    if ($found.ContainsKey('brightness') -and [int]$found['brightness'] -eq 0) {
        $hints += "brightness=0 -> die LEDs bleiben dunkel, egal welcher Modus eingestellt ist"
    }

    if ($found.Count -gt 0) {
        $kb = if ($found.ContainsKey('kbMode'))   { $kbModeNames[[int]$found['kbMode']] }     else { $null }
        $ed = if ($found.ContainsKey('edgeMode')) { $edgeModeNames[[int]$found['edgeMode']] } else { $null }
        Write-Host ("    rgb=({0},{1},{2})  brightness={3}  speed={4}" -f $found['red'],$found['green'],$found['blue'],$found['brightness'],$found['speed'])
        Write-Host ("    kbMode=0x{0:X2} {1}   edgeMode=0x{2:X2} {3}" -f $found['kbMode'],$kb,$found['edgeMode'],$ed)
        Write-Host ("    aura={0} mouse={1} kb={2} ram={3} edge={4}" -f $found['enableAura'],$found['enableMouse'],$found['enableKeyboard'],$found['enableRAM'],$found['enableEdge'])
    }

    if ($problems) {
        $exit = 1
        Write-Host "    BEFUNDE:" -ForegroundColor Yellow
        $problems | ForEach-Object { Write-Host "      - $_" -ForegroundColor Yellow }
    } else {
        Write-Host "    OK - vollstaendig und alle Werte im gueltigen Bereich" -ForegroundColor Green
    }
    if ($hints) {
        Write-Host "    HINWEISE (gueltig, aber wirkungslos):" -ForegroundColor Cyan
        $hints | ForEach-Object { Write-Host "      - $_" -ForegroundColor Cyan }
    }
    Write-Host ""
}

# --- Abgleich mit der aktiven Konfiguration ---------------------------------
# config.json wird bei jedem Beenden aus dem Laufzeitzustand geschrieben.
# lastProfile ist der Name, den die Combobox beim Start vorauswaehlt - das Profil
# wird dabei aber NICHT geladen. Das ist ausdruecklich so gewollt und im Code
# begruendet (WM_CREATE, "Deliberately NOT auto-loading lastProfile here"):
# sonst wuerde ein Profil beim naechsten Start jede zwischenzeitliche Aenderung
# ueberschreiben.
#
# Frueher stand hier das Gegenteil ("wird beim Start automatisch geladen") und
# eine Abweichung galt als Fehler. Das war schlicht falsch: eine Abweichung ist
# der Normalfall, sobald man nach dem Laden noch etwas verstellt hat. Sie ist
# eine Information, kein Befund - und ein Pruefskript, das eine ueberholte
# Behauptung durchsetzt, ist schlimmer als keines (Regel 1).
$cfgPath = Join-Path $env:APPDATA "OneClickRGB\config.json"
if (Test-Path $cfgPath) {
    $cfg = Get-Content $cfgPath -Raw | ConvertFrom-Json
    $last = $cfg.lastProfile
    Write-Host "=== Abgleich config.json <-> lastProfile ===" -ForegroundColor Cyan
    Write-Host "    lastProfile = '$last'  (nur in der Combobox vorausgewaehlt, wird NICHT automatisch geladen)"
    $lastFile = Join-Path $profileDir "$last.rgb"
    if ($last -and (Test-Path $lastFile)) {
        $pf = @{}
        Get-Content $lastFile | ForEach-Object {
            $eq = $_.IndexOf('='); if ($eq -ge 0) { $pf[$_.Substring(0,$eq)] = [int]$_.Substring($eq+1) }
        }
        $diff = @()
        foreach ($k in 'red','green','blue','brightness','speed','kbMode','edgeMode') {
            if ($pf.ContainsKey($k) -and $cfg.$k -ne $pf[$k]) { $diff += "$k : config=$($cfg.$k) profil=$($pf[$k])" }
        }
        if ($diff) {
            Write-Host "    Aktueller Zustand weicht vom Profil ab (erst 'Laden' uebernimmt es):" -ForegroundColor Cyan
            $diff | ForEach-Object { Write-Host "      - $_" -ForegroundColor Cyan }
        } else {
            Write-Host "    identisch mit $last.rgb" -ForegroundColor Green
        }
    } elseif ($last) {
        Write-Host "    WARNUNG: lastProfile zeigt auf '$last', aber $last.rgb existiert nicht" -ForegroundColor Red
        $exit = 1
    }
}

exit $exit
