# OneClickRGB Signal- und Datenpipeline (Ist-Zustand)

Stand: 2026-04-13
Datei-Basis: `src/oneclick_rgb_complete.cpp`

## 1) End-to-End Pipeline (aktuell)

1. **Input-Ebene (UI/System):**
   - `WM_HSCROLL` (RGB/Helligkeit/Geschwindigkeit)
   - `WM_COMMAND` (Checkboxen, Combos, Buttons, Presets, Profile)
   - `WM_HOTKEY`, Tray-Kommandos
   - `WM_TIMER` (`ID_TIMER_DEBOUNCE`, `ID_TIMER_RESUME`)
   - `WM_POWERBROADCAST`, `WM_WTSSESSION_CHANGE`
2. **State-Ebene:**
   - Direkte Mutation von `g_state` in Event-Handlern
3. **Persistenz:**
   - `SaveSettings()` -> `SaveAppSettings()` -> `g_config.Save()`
   - `LoadSettings()` -> `LoadAppSettings()` -> `g_config.Load()`
   - Profile separat via `*.rgb`
4. **Apply-Scheduler:**
   - `RequestApplyColors()` startet immer `std::thread(ApplyColors).detach()`
   - `ApplyColors()` nutzt `applying` + `applyPending` als Coalescing-Guard
5. **Hardware-Adapter:**
   - `SetAsusAura`, `SetSteelSeries`, `SetEVisionKeyboard`, `SetEVisionEdge`, `SetGSkillRAM`
6. **UI-Feedback/Logging:**
   - `AppendStatus()` schreibt in `g_state.statusLog` + direkt in Edit-Control

## 2) Primäre Signalquellen und Senken

### Signalquellen
- Nutzerinteraktion: Slider, Checkboxen, Combos, Buttons
- Systemereignisse: Resume, Unlock, Display-On
- Hintergrundthread: ResumeWatcher (`PostMessage(WM_USER + 100)`)

### Senken
- Persistenzdatei `%APPDATA%\\OneClickRGB\\config.json`
- Profil-Dateien `%APPDATA%\\OneClickRGB\\profiles\\*.rgb`
- HID/SMBus-Geräte
- Status-Log-Editfeld im Hauptfenster

## 3) Kritische Bruchstellen (Ist)

1. **Viele Einstiegspfade mutieren State direkt**
   - Kein zentraler Reducer/Intent-Layer
   - Erhöht Risiko für inkonsistente Reihenfolgen
2. **Logging ist nicht UI-thread-isoliert**
   - `AppendStatus()` kann aus Worker-Kontext UI-Funktionen aufrufen
   - Kann Repaint-Jitter und doppelte visuelle Updates begünstigen
3. **Renderpfade sind mehrschichtig und teilweise rekursiv**
   - Parent `WM_PRINTCLIENT` + Child Subclass-Paints
   - Erhöht Komplexität bei Flackern/Doppelzeichnung
4. **Persistenzpfad ist nicht vollständig atomar pro Intent**
   - Viele Call-Sites speichern separat, teils mehrfach pro Aktion
5. **Apply-Worker-Lebenszyklus ist "fire-and-forget"**
   - Detached Threads statt dedizierter Worker-Queue

## 4) Messbare Symptome (beobachtet)

- Visuelles Springen/Flickern bei Checkboxen
- Temporäre Doppelzeichnung bei Labels
- Eingabeblockaden bei Controls bei Überlappung/Redraw-Stress
- Doppelte oder sehr nahe Status-Log-Einträge

## 5) Zielbild (Best Practice, nächste Phasen)

## Phase A – Stabilisierung (kurzfristig)
- Zentraler Intent-Einstieg für UI-Events
- Eine serialisierte Apply-Queue (Single Consumer)
- UI-thread-safe Logging via `PostMessage(WM_APP+X)`
- Log-Dedupe (gleicher Text im Zeitfenster zusammenfassen)

## Phase B – Konsolidierung (mittelfristig)
- Persistenz nur noch über einen Commit-Pfad je Intent
- Profil-Laden als atomare Transaktion (State+Persist+Apply)
- Klare Trennung: UI-Render vs. State-Update

## Phase C – Modernisierung (optional)
- Event-Telemetrie (Latenz vom Input bis Device-Apply)
- Austauschbarer Device-Adapter-Layer mit klaren Contracts
- Optionaler Umzug auf strukturiertere UI-Architektur

## 6) Konkrete nächste Implementierungsschritte

1. `UI-Events zentral normalisieren`
2. `Apply-Queue zentralisieren`
3. `UI-thread-sicheres Logging einführen`
4. `Status-Log Dedupe einbauen`

Diese Reihenfolge liefert den größten Stabilitätsgewinn bei minimalem Risiko.

## 7) Umsetzungsstatus (2026-04-13)

Abgeschlossen:
- Zentrale Apply-Queue mit Coalescing (`applyPending`) statt unkontrollierter Detached-Apply-Stürme
- UI-thread-sicheres Status-Logging via Message-Posting
- Renderpfade für Controls stabilisiert (`WM_PAINT`/`WM_PRINTCLIENT`)
- Persistenzpfad konsolidiert; Profil-Load als atomarer Snapshot-Commit-Apply
- Geräteadapter-Zugriffe global serialisiert über `g_state.deviceIoMutex`
   - Hauptpfade: `ApplyColors`, `FullHIDReset`, Suspend-Zweig (`WM_POWERBROADCAST`)
   - Hilfspfade: `ScanAsusHardware`, `HasAsusHardwareChanged`, `SetAsusAuraQuick`, `TestAsusChannel`

Verifikation:
- Build: `build_native.bat` erfolgreich
- Headless: `OneClickRGB.exe --switch-test=edge` und `--switch-test=mouse` stabil
- Lasttest: 20x Edge + 20x Mouse ohne Exitcode-Fehler
- GUI-Runtime: Fensterstart + Screenshot (`docs/screenshots/capture_window.ps1`) erfolgreich

Regression-Script:
- `run_regression_smoke.cmd` führt Build + deterministische Headless-Loop-Tests aus
