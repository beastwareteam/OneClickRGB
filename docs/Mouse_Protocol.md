# SteelSeries Rival 600 — Zonen- & Lighting-Modell

Abgeleitet **offline** aus der installierten SteelSeries-GG-Software (read-only,
`immutable=1` geöffnet — kein Schreibzugriff, kein Lock). Liefert das echte
Zonen-/Effekt-Modell der Maus, das die `--probe`-Diagnose sonst nur als
generische „Zone 0..7" sah.

> Wichtig: Die **rohen HID-Schreibsequenzen** liegen **nicht** in den DBs (sie
> sind in `SteelSeriesPrism.exe` einkompiliert). Die DB liefert **Zonen-Identität,
> Layout und das Effekt-Schema** — nicht die Bytes. Der Schreibpfad bleibt unser
> bestehender (siehe §4) bzw. OpenRGB als Referenz. Die App braucht **keine**
> Laufzeit-Abhängigkeit von SteelSeries.

## 1. Datenquellen

| Pfad | Inhalt |
|---|---|
| `C:\ProgramData\SteelSeries\GG\apps\engine\db\database.db` | Engine (64 Tab.); `devices.id=120` = `rival_600`, Spalte `settings` = JSON-Profil |
| `C:\ProgramData\SteelSeries\GG\apps\engine\prism\db\database.db` | Prism RGB; `zone_cache device_id=120` = 8 Zonen |
| `...\GG\apps\engine\configurationMigrations\rival_600.migration` | Layout-DSL (UI-Koordinaten der Zonen) |
| `...\Roaming\steelseries-gg-client` | nur Electron-Cache — **irrelevant** |

Extraktion (read-only, Windows-Pfade in Python sqlite3):
```python
uri = f"file:{path}?mode=ro&immutable=1"
con = sqlite3.connect(uri, uri=True)
con.execute("select settings from devices where id=120")
```

## 2. Geräte-Identität
- USB VID:PID = `0x1038:0x1724`.
- ENGINE `product_id 272111396` = `0x10381724` (VID<<16 | PID). ✔
- Bootloader-PID `272111397` (= `0x10381725`).

## 3. Zonen (autoritativ, 8 Stück)

Aus `settings`-JSON-Keys (`*_lighting`) + `effect_index` + `rival_600.migration`
(migration-schema 2) UI-Koordinaten:

| index | key | Name | effect_index | UI (x,y) |
|---|---|---|---|---|
| 0 | `wheel_lighting` | Scroll Wheel | 0 | 85, 70 |
| 1 | `logo_lighting` | Logo | 1 | 85, 270 |
| 2 | `z2_lighting` | Side Z2 | 2 | 50, 145 |
| 3 | `z3_lighting` | Side Z3 | 3 | 120, 145 |
| 4 | `z4_lighting` | Side Z4 | 4 | 45, 180 |
| 5 | `z5_lighting` | Side Z5 | 5 | 125, 180 |
| 6 | `z6_lighting` | Side Z6 | 6 | 25, 210 |
| 7 | `z7_lighting` | Side Z7 | 7 | 145, 210 |

→ Scrollrad + Logo + 6 Seiten-LEDs (links/rechts paarweise nach unten). Diese
Tabelle ist in [`ProbeMouse()`](../src/hardware_probe.h) fest hinterlegt.

## 4. Aktueller Schreibpfad (im Code, funktioniert)

[`SetSteelSeries`](../src/oneclick_rgb_complete.cpp#L1429) setzt je Zone:
```
pkt = { 0x1C, 0x27, 0x00, (1<<i), R, G, B, 0 }   // hid_write len=7, i=0..7
save = { 0x09, ... }                              // hid_write len=9 (commit)
```
**Offen / zu verifizieren:** ob Bit `1<<i` genau `effect_index i` (Tabelle §3)
trifft. Wird per **Identify** (genau eine Zone weiß↔aus blinken) gegen den
DB-Namen geprüft; erst dann `verified`. Abgleich gegen OpenRGB
`SteelSeriesRival600Controller` empfohlen, falls die Zuordnung abweicht.

## 5. Effekt-Schema je Zone (für vollen Ausbau)

Aus dem `*_lighting`-JSON. Pro Zone:
```
type, has_direction, direction_type, direction_inverted,
speed, scale, num_colors,
colors[14]   = [{r,g,b}, ...],         // bis 14 Stützfarben
positions[14]= [{pos:0..100}, ...],    // Gradient-Stops (z.B. 0/33/66)
initial_color={r,g,b},
react_color ={r,g,b,time},             // reaktiver Effekt
trigger_mask, settings_mask,
element_pos ={x,y},                    // = UI-Koordinate (§3)
effect_index
```
Beispiel `logo_lighting`: `num_colors=3`, colors `R/G/B`, positions `0/33/66`,
`speed=1000`, `effect_index=1`. Dieses Schema ist die Vorlage für die
generische `effect`-Struktur im Datenmodell (Plan §C) und den vollen
Effekt-Ausbau im Adapter (Plan §D).

## 6. Optionaler DB-Import (bei Bedarf)
Aktuelle Pro-Zonen-Farben/Effekte können beim ersten Start aus `devices.id=120
→ settings` als Startwerte übernommen werden (read-only). Fehlt SteelSeries →
still ignorieren, eingebackene Defaults gelten. Benötigt eine SQLite-Anbindung
im C++-Build (sqlite3-Amalgamation) — Entscheidung in Plan §C.
