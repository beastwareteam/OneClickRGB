# OneClickRGB — Sanierungsplan

**Stand:** 2026-08-17 · **Umfang:** alle 27 im Audit gefundenen Probleme

## Kontext

Nach der Reparatur des Tastatur-Commit-Bytes (`+0x0A`) hat ein Audit aller
Geräte- und Testpfade 27 weitere Probleme aufgedeckt. Der Kern des Befunds:
**die beiden EVision-Pfade sind die einzigen, die den neuen Projektregeln aus
`CLAUDE.md` genügen.** Die drei übrigen Gerätepfade melden Erfolg, sobald sich
ein Geräte-Handle öffnen ließ, und die gesamte Testschicht besteht aus Skripten,
die auch ohne angeschlossene Hardware bestehen.

Konkret: `run_regression_smoke.cmd` meldet nach 130 Sekunden
`[OK] … fails=0`, wenn kein einziges Gerät angesteckt ist — weil
`--switch-test` bei [oneclick_rgb_complete.cpp:4967](../src/oneclick_rgb_complete.cpp#L4967)
unbedingt `return 0` macht. Das ist genau die blinde Erfolgsmeldung, die Regel 1
verbietet.

**Ziel:** Jeder Schreibvorgang wird verifiziert oder ehrlich als
„nicht rücklesbar" gekennzeichnet; jeder Test hat einen echten Exit-Code; die
bereits vorhandenen, aber unerreichbaren Zonen-Modelle werden nutzbar.

**Reihenfolge:** Regel-1/2-Sanierung zuerst, Features danach. Phase 4
(Hardware-Wahrheitsfindung) braucht dich und die Geräte und läuft deshalb
parallel zur Codearbeit — sie blockiert nichts.

---

## Grundlage: einheitliche Verifikation

Der wichtigste Einzelbaustein. Ohne ihn entstehen fünf handgeschnitzte
Implementierungen. Neu in `oneclick_rgb_complete.cpp`, direkt vor den
Gerätepfaden (~Zeile 1330):

```cpp
// Wie belastbar ist die Erfolgsmeldung eines Gerätepfads?
enum class VerifyLevel {
    ReadBack,    // geschrieben, zurückgelesen, Soll==Ist  -> "verifiziert"
    WriteAcked,  // Transport hat quittiert, Rücklesen technisch unmöglich
    NotFound,    // Gerät nicht vorhanden
    Mismatch,    // zurückgelesen, Soll != Ist
    WriteFailed  // Transport hat den Write abgelehnt
};

struct DeviceResult {
    const wchar_t* device = L"?";
    VerifyLevel    level  = VerifyLevel::NotFound;
    std::wstring   detail;            // Soll/Ist bei Mismatch
    bool ok() const { return level == VerifyLevel::ReadBack ||
                             level == VerifyLevel::WriteAcked; }
};
```

**Ehrliche Einordnung der fünf Geräte** — nicht alle können zurücklesen, und
das darf nicht verwischt werden:

| Gerät | Rücklesen möglich? | Grundlage |
|---|---|---|
| EVision Keyboard | **ja** | `EVisionQuery` cmd `0x05`, bereits implementiert |
| EVision Edge | **ja** | dito, bereits implementiert |
| G.Skill RAM | **ja** | `ene_read` existiert ([:2058](../src/oneclick_rgb_complete.cpp#L2058)), wird für Modulname und LED-Zahl schon benutzt — nur die Farbregister `0x8100+i*3` werden nie geprüft |
| ASUS Aura | **nein** | kein bestätigtes GET-Kommando für Direct-Mode-Farben, siehe [hardware_probe.h:158-160](../src/hardware_probe.h#L158) → `WriteAcked` über den `hid_write`-Rückgabewert |
| SteelSeries | **nein** | Rival-600-Protokoll ist schreibend-only → `WriteAcked` |

Für die beiden `WriteAcked`-Geräte lautet die Statusmeldung **„gesendet
(nicht rücklesbar)"**, nicht „verifiziert". Das ist der Unterschied zwischen
einer ehrlichen und einer blinden Meldung.

---

## Phase 0 — Akute Gefahren *(erledigt 2026-08-17)*

Diese vier Punkte richten aktiv Schaden an oder verfälschen Diagnosen.
Alle vier sind umgesetzt, der Build ist grün. Was am Gerät nachgeprüft wurde,
steht unter „Nach Phase 0 am Gerät geprüft".

### [x] 0.1 — Brute-Force in `edge-diagnose` entfernen *(Problem 6)*
[oneclick_rgb_complete.cpp:4809-4861](../src/oneclick_rgb_complete.cpp#L4809)
enthält die verbotene 15-Offset-Schleife (`0x13/0x16/0x19/0x1B/0x1E` × 3
Profile) **wortgleich weiter** — genau der Code, der `+0x14..0x1D` in allen drei
Profilen zerstört hat. Aus `SetEVisionEdge` ist er entfernt, hier lebt er.

→ Schleife durch einen einzelnen Write auf `activeProfile*0x40 + 0x1E` mit
Rücklesung ersetzen. Zusätzlich: `FILE* diag` wird bei
[:4812](../src/oneclick_rgb_complete.cpp#L4812) ohne NULL-Prüfung
dereferenziert und per `fopen` relativ zum CWD geöffnet — auf
`GetAppDataPath() + L"\\docs"` umstellen wie alle anderen Reports.

**Umgesetzt.** Ein Write, aktives Profil, `+0x1E`. Der Report liegt jetzt unter
`%APPDATA%\OneClickRGB\docs\edge_diagnose.txt` und wird vor dem ersten
`fprintf` geprüft; lässt er sich nicht öffnen, bricht der Zweig mit `rc=1` ab.
Drei Entscheidungen dabei:

- **Payload identisch zur Produktion**, inklusive Commit-Flag `[9]=0x01`. Vorher
  stand dort `0x00`: der Diagnose-Write wurde nie übernommen, die LED blieb
  dunkel, und wer „welche LED wurde blau?" beantworten sollte, sah nichts. Eine
  Diagnose, die den Produktionspfad nicht reproduziert, belegt nichts über ihn.
  Der Write geht damit nicht weiter als ein normales Apply.
- **Vorher-Bytes werden protokolliert**, bevor geschrieben wird — der alte
  Zustand bleibt nachvollziehbar.
- **Der Unlock-Write auf `0x14` ist raus.** Er existierte nur, um zu reparieren,
  was die Brute-Force direkt davor zerstört hatte, und adressierte fest Profil 0
  statt des aktiven. `SetEVisionKeyboard` räumt das Feld beim Apply ohnehin für
  das aktive Profil.

Der Zweig gibt jetzt einen echten Exit-Code zurück (`0` verifiziert, `1`
Mismatch/Read-Fehler, `2` Gerät nicht geöffnet) — Vorgriff auf 2.1, aber hier
ohne Aufwand zu haben. `run_regression_smoke.cmd` ist nicht betroffen, es ruft
`--switch-test=edge` und `=mouse`, nicht `edge-diagnose`.

### [x] 0.2 — `--dry-run` wird zu spät ausgewertet *(Problem 14, 15)*
`g_state.dryRun` wird bei [:4794](../src/oneclick_rgb_complete.cpp#L4794)
gesetzt — **nachdem** `--probe`, `--identify`, `--kbdump`, `--kbtest`,
`--kbmode*` und `--mouse-zones-test` bereits gelaufen und zurückgekehrt sind.
`--dry-run --kbmode-sweep` schreibt 21 Mode-Bytes in den Flash.

→ Flag-Parsing **vor** den ersten Diagnosezweig ziehen. Zusätzlich greift der
Dry-Run-Schutz nur in `ApplyColors` ([:2336](../src/oneclick_rgb_complete.cpp#L2336));
`testApply` ([:4914](../src/oneclick_rgb_complete.cpp#L4914)) ruft die Setter
direkt → dort ebenfalls prüfen. Sauberer: eine Wache in den Settern selbst,
dann kann kein künftiger Pfad sie umgehen.

**Umgesetzt, beide Hälften.** Das Parsing ist die erste Anweisung nach
`LogDebug("WinMain started")`. Die Wache heißt `DryRunSkip(name)` und sitzt in
jedem öffentlichen Setter — `SetAsusAura`, `SetAsusAuraQuick`, `SetSteelSeries`,
`SetSteelSeriesZones`, `IdentifyMouseZone`, `SetEVisionKeyboard`,
`SetEVisionEdge`, `SetGSkillRAM`, `FullHIDReset` — jeweils vor dem Öffnen des
Handles. Drei Zweige schreiben an den Settern vorbei direkt per `EVisionQuery`
und prüfen das Flag deshalb selbst: `--kbtest`, `--kbmode*`, `edge-diagnose`.

Zwei bewusste Festlegungen:

- **`DryRunSkip` liefert `false`, nie `true`.** Ein Dry-Run belegt nichts über
  die Hardware; ein `true` wäre genau die blinde Erfolgsmeldung aus Regel 1.
  Für 2.1 heißt das: Exit-Codes müssen den Dry-Run gesondert behandeln, sonst
  meldet `--dry-run --switch-test` künftig „Fehler" statt „nichts geprüft".
- **`--kbmode*` steigt sofort aus**, statt 21 Schritte lang zu schlafen und den
  Report mit `READ FAILED` zu füllen. Der Report sagt jetzt „DRY RUN — no write
  was sent and no value was read".

Lesende Pfade bleiben erlaubt: `--kbdump` funktioniert unter `--dry-run`
unverändert, weil die Wache Writes blockt, nicht Reads.

### [x] 0.3 — `g_lastKbVerify` liefert Altwerte *(Problem 16)*
Das Reset steht bei [:1704](../src/oneclick_rgb_complete.cpp#L1704), also
**hinter** dem `if (!dev) return false` bei
[:1651](../src/oneclick_rgb_complete.cpp#L1651). Fällt die Tastatur mitten im
`--kbmode-sweep` ab, druckt jede weitere Zeile in `kbmode_probe.txt` den
letzten erfolgreichen Read-back als wäre er frisch — ein alter Dump, als
Live-Wert ausgegeben, ausgerechnet in der Sonde, die das verhindern soll.

→ Reset als allererste Anweisung der Funktion, vor die Geräteöffnung.

**Umgesetzt.** `g_lastKbVerify = KbVerifyResult{};` ist jetzt die erste Zeile
von `SetEVisionKeyboard` — vor der Dry-Run-Wache und vor jedem `return false`.
Jeder Ausstieg hinterlässt damit `valid=false` statt des letzten Erfolgs.

### [x] 0.4 — Zwei widersprüchliche ASUS-Fallback-Tabellen *(Problem 18)*
[:1439-1444](../src/oneclick_rgb_complete.cpp#L1439) (7 Kanäle inkl. `0x0B`/`0x0C`)
gegen [:1482-1488](../src/oneclick_rgb_complete.cpp#L1482) (8 Kanäle `0..7`).
Zwei Pfade beschreiben dieselbe Hardware unterschiedlich.

→ Eine `static const` Tabelle, von beiden benutzt. Welche der beiden korrekt
ist, klärt Phase 4.2 am Gerät — bis dahin die konservativere übernehmen und den
offenen Punkt im Code vermerken.

**Umgesetzt** als `AURA_FALLBACK_CHANNELS`, benutzt von `SetAsusAura` und
`SetAsusAuraQuick`. Übernommen ist die **Schnittmenge** beider Tabellen:

```
{0x00,60} {0x01,120} {0x02,120} {0x03,60} {0x04,60}
```

Die LED-Zahlen waren in beiden Tabellen für `0..4` identisch, nur die Enden
widersprachen sich (`0x0B/0x0C` gegen `5/6/7`). Keins der beiden Enden ist
irgendwo belegt, also fällt beides weg statt geraten zu werden. Die
Identitäts-Abbildung Index→Kanal passt außerdem zu den zwei anderen Stellen,
die dieselbe Annahme schon treffen: `ParseAsusConfig()` nummeriert die
adressierbaren Header als `0,1,2,…`, und `ShowAsusTestDialog` hat den
Default-Kommentar „use index as channel".

> **Verhaltensänderung, einzige in Phase 0:** greift der Fallback (also nur,
> wenn die `0xB0`-Konfigtabelle nicht lesbar war), werden zwei Kanäle weniger
> angefahren als vorher im Apply-Pfad. Vorher waren Apply und Live-Vorschau auf
> demselben Board ohnehin uneinig — welche der beiden Varianten Licht machte,
> hing davon ab, welcher Codepfad gerade lief. Falls dein Board im Fallback
> hängt und jetzt LEDs dunkel bleiben, ist das der Punkt: dann liefert 4.3 die
> richtige Tabelle. **Bei gültiger Hardware-Konfig ändert sich nichts** — dann
> war der Fallback nie aktiv.

---

## Nach Phase 0 am Gerät geprüft *(2026-08-17, GK650)*

Ohne Geräte geprüft: Build grün, und alle schreibenden Headless-Pfade unter
`--dry-run` senden nachweislich nichts (`--kbmode-sweep=1` bricht nach ~1 s ab
statt 21 Schritte zu laufen, `edge-diagnose` legt keinen Report an, `--kbtest`
steigt aus).

Am Gerät nachgeholt — `--kbdump` → `--switch-test=edge-diagnose` → `--kbdump`:

- **0.1 verifiziert.** Genau eine `before/wrote/after`-Gruppe, `VERDICT:
  verified`, Exit-Code `0`. Aktives Profil war P0, Write ging auf `0x1E`:
  `03 04 04 00 00 00 13 FF 00 01` → `04 04 02 00 00 00 00 FF 00 01`, Rücklesung
  deckungsgleich. Geöffnete Schnittstelle: `MI_01&Col04` (UsagePage `0xFF1C`),
  6 Interfaces am VID `0x3299` gelistet.
- **Gegenprobe bestanden.** Im Diff der beiden Dumps über `0x000..0x3FF`
  bewegen sich **ausschließlich** `0x1E..0x27` — die 10 Payload-Bytes selbst.
  `+0x14..0x1D` steht in allen drei Profilen unverändert (P1 `0x54..0x5D`,
  P2 `0x94..0x9D` byte-identisch, ebenso P0). Die 15-Offset-Schleife ist damit
  auch am Gerät nachweislich weg; früher hätte ein Lauf hier alle drei Profile
  überschrieben.
- **Offen bleibt nur die Sichtprüfung** — dass die Leiste Mode `0x04` auch
  wirklich rendert, kann kein Read-Back belegen. Danach ein normales Apply, um
  die Farbe zurückzusetzen.

Die Altlast `04 02 00 04 02 …` in `+0x14..0x1D` steht weiterhin drin — sie zu
räumen ist 2.4, nicht Phase 0.

---

## Phase 1 — Regel 1 in allen Gerätepfaden

Baut auf der `DeviceResult`-Grundlage auf. Reihenfolge: erst die Setter, dann
die Aggregation.

### [ ] 1.1 — `SetGSkillRAM` verifizieren *(Problem 4)*
Der einzige der drei unsanierten Pfade, der **echt** zurücklesen kann.
`write_word`/`write_byte`/`ene_write` ([:2044-2056](../src/oneclick_rgb_complete.cpp#L2044))
sind `void` und verschlucken den `-1` aus `smbus_xfer`.

→ Rückgabetyp auf `bool`/`int` ändern, Fehler durchreichen. Nach der
Farbschleife ([:2088-2093](../src/oneclick_rgb_complete.cpp#L2088)) und **vor**
dem Commit `0x80A0` die geschriebenen Register `0x8100+i*3` per `ene_read`
zurücklesen und vergleichen. Achtung auf die ENE-Reihenfolge **RBG** (nicht
RGB) bei [:2090-2092](../src/oneclick_rgb_complete.cpp#L2090) — der Vergleich
muss dieselbe Vertauschung anwenden, sonst meldet er falsche Abweichungen.
`found++` ([:2096](../src/oneclick_rgb_complete.cpp#L2096)) zählt heute
*erkannte Module*, nicht erfolgreiche Writes — trennen.

### [ ] 1.2 — `SetAsusAura` ehrlich machen *(Problem 2)*
`hid_write` bei [:1392](../src/oneclick_rgb_complete.cpp#L1392) verwirft den
Rückgabewert; `SetAsusAura` gibt `true` zurück, sobald sich das Gerät öffnen
ließ.

→ `SetAsusChannel` gibt die Zahl erfolgreicher Pakete zurück;
`ApplyAsusChannelColor` reicht sie durch (heute `void` mit unbedingtem
`setCount++`). Ergebnis `WriteAcked` bei vollständiger Quittung, sonst
`WriteFailed`. Meldung: „N Kanäle gesendet (nicht rücklesbar)".

Ebenfalls hier: das Direct-Mode-Kommando `0x35` wird **nur** in `FullHIDReset`
([:2270-2281](../src/oneclick_rgb_complete.cpp#L2270)) gesendet, und dort mit
hartkodierten Kanälen `0..7` statt der gescannten `directChannel`-Werte. Bei
einem normalen Apply ist der Kanalmodus also das, was die Firmware zuletzt
hatte — läuft ein Firmware-Effekt, überschreibt er die `0x40`-Writes. Direct
Mode in den Apply-Pfad ziehen, mit den gescannten Kanalnummern.

### [ ] 1.3 — SteelSeries-Wortlaut korrigieren *(Problem 3)*
Das Protokoll ist tatsächlich schreibend-only — hier ist nichts zu
„reparieren" außer der Behauptung. `hid_write`-Rückgabewerte in
`SetSteelSeries` ([:1521](../src/oneclick_rgb_complete.cpp#L1521)),
`SSWriteZone`, `SSSave` prüfen, Ergebnis `WriteAcked`, Meldung
„gesendet" statt „set".

### [ ] 1.4 — `FullHIDReset` *(Problem 5)*
Liest `0xB0` bei [:2257](../src/oneclick_rgb_complete.cpp#L2257) und wirft es
weg. Entweder auswerten (Konfigtabelle gegen `g_asusHwConfig` prüfen) oder den
Read entfernen — aber nicht so stehen lassen.

### [ ] 1.5 — `ApplyColors` aggregiert ehrlich *(Problem 1)*
Der wichtigste Punkt der Phase. Heute werden bei
[:2356-2370](../src/oneclick_rgb_complete.cpp#L2356) **alle fünf**
Rückgabewerte verworfen und bei [:2375](../src/oneclick_rgb_complete.cpp#L2375)
unbedingt `=== Done! ===` gedruckt — `SetEVisionKeyboard` kann „MISMATCH"
zurückgeben und die Zusammenfassung meldet trotzdem Erfolg.

→ `std::vector<DeviceResult>` sammeln, Schlusszeile aus dem schlechtesten
Ergebnis bilden:

```
=== Fertig: 3 verifiziert, 2 gesendet ===
=== FEHLER: Tastatur MISMATCH (mode 0x05->0x06) ===
```

Zusätzlich das Ergebnis in einer globalen `g_lastApplyResults` ablegen — Phase 2
braucht es für Exit-Codes.

---

## Phase 2 — Testschicht wieder echt machen

Setzt Phase 1 voraus (ohne echte Verdikte gibt es nichts, woraus ein Exit-Code
entstehen könnte).

### [ ] 2.1 — Exit-Codes für die Headless-Modi *(Problem 8)*
Die Wurzel des Problems: [:4967](../src/oneclick_rgb_complete.cpp#L4967) beendet
den `--switch-test`-Zweig mit unbedingtem `return 0`. Der Verify-Bool von
`SetEVisionEdge` wird bei [:4925](../src/oneclick_rgb_complete.cpp#L4925)
berechnet, bei [:4928](../src/oneclick_rgb_complete.cpp#L4928) geloggt — und
dann weggeworfen.

→ `testApply` sammelt `DeviceResult`s; der Zweig gibt `0` nur zurück, wenn jedes
angeforderte Gerät `ok()` liefert, sonst `1` (Verifikationsfehler) bzw. `2`
(Gerät nicht gefunden). Damit wird `if errorlevel 1` in den Skripten zum ersten
Mal aussagekräftig. Gleiches für `--mouse-zones-test`
([:4775](../src/oneclick_rgb_complete.cpp#L4775)), das heute gar nichts meldet.

### [ ] 2.2 — Kaputte Skripte reparieren oder löschen *(Problem 9, 10, 11)*
- `test_aura_only.bat` benutzt `--only-aura` (:33) und `--profile=` (:36) —
  **beide Flags existieren nicht**. Unbekannte Argumente werden ignoriert, also
  startet die normale GUI und fährt *alle* Geräte an; die Behauptung bei
  :47-49 („All other devices are disabled") ist schlicht falsch. Entweder die
  Flags implementieren oder das Skript auf `--switch-test=aura` umstellen.
- `test_edge_only.bat:58` / `test_mouse_only.bat:24` drucken eine handkopierte
  Sollsequenz aus [:4955-4963](../src/oneclick_rgb_complete.cpp#L4955). Nichts
  hält beide synchron → die Sequenz soll das Programm selbst ausgeben.
- `pause` am Ende beider Skripte verhindert unbeaufsichtigten Lauf → hinter
  eine `%1`-Abfrage (`--ci`) legen.
- `test_edge_aura_13s.bat` erbt alles und blockiert am inneren `pause`.

### [ ] 2.3 — `validate_profiles.ps1` an die Quelle binden *(Problem 12)*
Gute Arbeit, aber :20-40 transkribiert die `kbMode`/`edgeMode`-Whitelists und
Slider-Bereiche aus dem C++ **ohne Rückbindung**. Sobald Phase 4.1 einen
Mode-Wert widerlegt, validiert das Skript weiter gegen die alte Tabelle.

→ Neuer Modus `--dump-schema`, der `KB_MODE_TABLE`, die Edge-Modi und die
Slider-Ranges als JSON ausgibt; das Skript liest sie daraus statt sie zu kennen.
Nebenbei: harter `exit 1` bei fehlendem Profilverzeichnis (:43-45) → als „nichts
zu prüfen" behandeln; nackter `[int]`-Cast bei :146 abfangen.

### [ ] 2.4 — `--kbclean=<profil>` *(Problem 7)*
Der Live-Dump zeigt in **P1 und P2** noch `04 02 00 04 02 00 04 00 04 02` bei
`+0x14..0x1D`; der Unlock räumt nur zwei Bytes im aktiven Profil. Beim
Profilwechsel kann die Win-Sperre also zurückkehren.

→ Opt-in-Kommando: Bereich dumpen → nullen → zurücklesen → Vorher/Nachher-Report
nach `%APPDATA%\OneClickRGB\docs\kbclean_<p>.txt`. **Niemals automatisch beim
Apply** — die Feldsemantik ist unbekannt (Regel 2).

---

## Phase 3 — CI *(Problem 13)*

Alle vier Workflows sind tot: sie rufen `cmake ..`, und **es gibt kein
CMakeLists.txt im Repo**. `tests.yml` baut ein Target `tests` und filtert auf
`tests/**` — beides hat nie existiert. `|| true` bei :55 und :101 sorgt dafür,
dass ein echter Test den Job nie rot machen könnte. Ziel sind veraltete
`ubuntu-20.04`-Runner, und `build.yml` enthält einen **macOS-Clang-Job für eine
Win32/GDI+/PawnIO-Anwendung**. Es ist noch nie CI gelaufen.

*Annahme (von dir nicht explizit entschieden): Umstellung auf `build_native.bat`
statt eines neuen CMake-Systems — kein zweiter Build-Pfad neben dem echten.*

### [ ] 3.1 `build.yml` → `windows-latest` + `build_native.bat`, Nicht-Windows-Jobs raus
### [ ] 3.2 `tests.yml` → hardwarefreie Gates: `--dump-schema` (2.3), `validate_profiles.ps1`, `--dry-run`-Start ohne Absturz. `|| true` entfernen
### [ ] 3.3 `security.yml` CodeQL auf den MSVC-Build umstellen; `release.yml` analog
### [ ] 3.4 Actions-Versionen anheben (`upload-artifact@v3` → v4 etc.)

**Wichtig:** Hardware-Tests gehören *nicht* in CI. Der Runner hat keine Geräte.
CI prüft: baut es, ist das Schema konsistent, sind die Profile gültig.

---

## Phase 4 — Hardware-Wahrheitsfindung *(braucht dich + die Geräte)*

Läuft parallel zu Phase 1-3, blockiert nichts.

### [ ] 4.1 — Welche Tastatur-Modi animieren wirklich? *(Problem 21)*
Nur `0x06` = STATIC war je gegen die Hardware validiert; die anderen zehn
Einträge in `KB_MODE_TABLE`
([:601-606](../src/oneclick_rgb_complete.cpp#L601)) sind Annahmen. Das Werkzeug
steht: `--kbmode-sweep=4` läuft `0x00..0x14` durch.

**Ablauf:** Sweep starten, Tastatur beobachten, Sekunde notieren, an der sich
etwas bewegt; die `t[s]`-Spalte in `kbmode_probe.txt` bildet das auf das
Mode-Byte ab. Danach: tote Modi aus `KB_MODE_TABLE` und der Combo
([:3338-3340](../src/oneclick_rgb_complete.cpp#L3338)) entfernen, echte Werte
eintragen, `docs/Keyboard_Protocol.md` Abschnitt 5.4 schließen.

> „accepted" im Report heißt nur, dass das Byte gespeichert wurde — nicht, dass
> der Effekt gerendert wird. Diese Unterscheidung ist der ganze Zweck des Laufs.

### [ ] 4.2 — Identify-Schleife implementieren *(Problem 17, 20)*
`--probe-interactive` wird bei [:4561](../src/oneclick_rgb_complete.cpp#L4561)
geparst, aber `RunHardwareProbe(bool /*interactive*/)`
([hardware_probe.h:583](../src/hardware_probe.h#L583)) verwirft den Parameter —
**nichts Interaktives passiert**. Gleichzeitig bewirbt
[hardware_probe.h:629](../src/hardware_probe.h#L629) genau diesen Schalter als
Weg, „verified units in config.json zu segnen", und jede Einheit trägt fest
`"verified": false` ([:357-358](../src/hardware_probe.h#L357)) — es gibt nichts,
was das je umstellen könnte. `IdentifyMouseZone`
([:1575](../src/oneclick_rgb_complete.cpp#L1575)) schreibt sein Ergebnis
nirgendwo hin (`AppendStatus` an eine GUI, die in diesem Modus nie existiert).

→ Interaktiven Zweig implementieren: Einheit blinken, Rückfrage, bei
Bestätigung `LightZone::verified = true` in `g_config` schreiben. Das ist die
Voraussetzung dafür, dass ein Zonen-Editor echte statt geratener Zuordnungen
anzeigt.

### [ ] 4.3 — ASUS-Fallbacktabelle am Gerät klären → 0.4 abschließen

---

## Phase 5 — Zonen erreichen die Hardware

Ab hier Features. Kein UI nötig — erst der Datenweg.

### [ ] 5.1 — Maus-Zonen in den Apply-Pfad *(Problem 22)*
`ApplyColors` ruft bei [:2360](../src/oneclick_rgb_complete.cpp#L2360)
`SetSteelSeries(r,g,b)` global. `SetSteelSeriesZones`
([:1554](../src/oneclick_rgb_complete.cpp#L1554)) ist **einzig** aus
`--mouse-zones-test` erreichbar. `g_config.mouseZones` wird geladen,
gespeichert, ist in JSON editierbar — und erreicht die Maus nie.

→ In `ApplyColors`: sind Zonen konfiguriert und weicht mindestens eine von der
Globalfarbe ab → `SetSteelSeriesZones()`, sonst der bisherige Pfad. Nebenbei
[:1560](../src/oneclick_rgb_complete.cpp#L1560): die *globale*
`steelseries`-Korrektur wird auf jede Zone angewandt, damit ist keine
zonenweise Helligkeit ausdrückbar — auf `LightZone` verlagern.

### [ ] 5.2 — ASUS-Kanalfarben nutzen *(Problem 23)*
Der Testdialog schreibt Kanalfarben nach
`g_asusHwConfig.channels[i].colorR/G/B`
([:3203-3206](../src/oneclick_rgb_complete.cpp#L3203)) und speichert sie — aber
`SetAsusAura` ([:1428-1436](../src/oneclick_rgb_complete.cpp#L1428)) ignoriert
sie und schiebt eine Globalfarbe auf alle Kanäle. Die Einstellung ist
schreibgeschützt in dem Sinn, dass sie beim nächsten Apply verlorengeht.

→ Gleiche Logik wie 5.1. **Einheitlich über `light_zone.h`** lösen, nicht drei
Sonderwege: `auraZones` ist in
[app_config.h:68-71](../src/app_config.h#L68) bereits deklariert und
serialisiert.

---

## Phase 6 — Zonen-Editor *(Problem 25)*

Das Datenmodell ist **fertig und ungenutzt**: `light_zone.h` hat `LightZone`
mit `x/y`-Layoutkoordinaten, `hwIndex`, `enabled`, `verified`, `ZoneColor` und
eine reichhaltige `ZoneEffect`-Struktur; `ZoneDefaults::Rival600`
([:114-133](../src/light_zone.h#L114)) legt 8 Mauszonen mit Koordinaten an;
`app_config.h` persistiert alles. Es fehlt nur die Oberfläche.

### [ ] 6.1 — Modaler Dialog statt fünfter Karte
**Empfehlung: modaler Dialog.** `g_cards[4]` ist ein Array fester Größe
([:315-316](../src/oneclick_rgb_complete.cpp#L315)), `WINDOW_HEIGHT` ist bei
[:175](../src/oneclick_rgb_complete.cpp#L175) hartkodiert `870`, das Fenster
nicht größenveränderlich. Eine fünfte Karte zieht drei gekoppelte Änderungen
nach sich; ein Button in der Geräte-Karte (freier Platz bei
[:3496-3503](../src/oneclick_rgb_complete.cpp#L3496)) kostet keine davon.

**Vorlage: Pattern A** = `ShowChannelSettingsDialog`
([:2839-2887](../src/oneclick_rgb_complete.cpp#L2839)) — `RegisterClassW` hinter
`static bool`, `CreateWindowExW` mit `WS_POPUP|WS_CAPTION`, zentriert,
eigene Modalschleife. Dessen `WndProc`
([:2627-2837](../src/oneclick_rgb_complete.cpp#L2627)) zeigt das
IDOK/IDRETRY/IDCANCEL-Muster, `GWLP_USERDATA` für Dialogzustand und die
Dark-Theme-Handler. **Nicht** Pattern B (ASUS-Test,
[:3239](../src/oneclick_rgb_complete.cpp#L3239)) kopieren: fabriziert ein
`DLGTEMPLATE` im Stack, ohne Titel und ohne Theming.

Freie Control-IDs: `8000+`. Zwei Fallen aus dem Bestand: die
`Strings`-Initializer ([:429-541](../src/oneclick_rgb_complete.cpp#L429)) sind
**positionell** — neue Member nur anhängen, sonst verschieben sich still alle
folgenden Strings in beiden Sprachtabellen. Und der Zustand gehört nach
`g_config`, nicht `g_state` (dort gibt es keine Zonenfelder), genau wie
`ChanSettingsWndProc` es mit `g_config.aura[i]` macht.

### [ ] 6.2 — Zonen visuell anordnen (`x/y` aus `LightZone` nutzen)
### [ ] 6.3 — Identify-Button je Zone (nutzt 4.2)
### [ ] 6.4 — Tote IDs aufräumen *(Problem 19)*: `ID_BTN_PICK_COLOR` (1002) wird bei [:3816](../src/oneclick_rgb_complete.cpp#L3816) behandelt, aber nie ein Control damit erzeugt; `ID_HOTKEY_TOGGLE` (4006) wird nur *ab*gemeldet ([:4120](../src/oneclick_rgb_complete.cpp#L4120)), nie registriert

---

## Phase 7 — ASUS Per-LED *(Problem 24)*

**Das Paketformat kann es bereits.** `SetAsusChannel`
([:1364](../src/oneclick_rgb_complete.cpp#L1364)) baut `buf[0x05 + i*3]` mit
explizitem `offset`/`count`-Fenster und `0x80`-Last-Flag, gestückelt zu
`ASUS_LEDS_PER_PACKET = 20` → 65-Byte-Report, 120 LEDs = 6 Pakete. Der Engpass
ist allein die Signatur: sie nimmt ein skalares Triplet, und die Schleife bei
[:1386-1390](../src/oneclick_rgb_complete.cpp#L1386) stempelt es in jeden Slot.

→ Überladung `SetAsusChannel(dev, channel, const ZoneColor* colors, int count)`.
**Am Packetiser, am Offset-Handling und am Last-Flag ändert sich nichts.**
`hardware_probe.h:165-170` liefert bereits ein `ledPoints`-Array
`{index, channel, offset}` je Kanal — das Adressmodell existiert also auch schon.

---

## Phase 8 — Profile tragen Zonen *(Problem 26)*

`.rgb`-Dateien sind `key=value`-Text mit exakt 12 flachen Ints
(`SaveProfile` [:929-956](../src/oneclick_rgb_complete.cpp#L929),
`LoadProfile` [:958-1026](../src/oneclick_rgb_complete.cpp#L958)). Zonenarbeit
geht bei jedem Profilwechsel verloren.

→ **Kein Formatwechsel.** `LoadProfile` seedet Locals aus dem aktuellen
`g_state` und ignoriert unbekannte Keys — also eine Zeile `zonesJson=<kompaktes
JSON>` anhängen. Alte Profile ohne den Key verhalten sich exakt wie bisher, und
eine ältere App-Version ignoriert die neue Zeile. Rückwärts- und
vorwärtskompatibel ohne Migration.

---

## Phase 9 — Per-Tasten-Tabelle *(Problem 27)*

Die Farbtabelle ab `0x2C0` ist gedumpt, aber undekodiert. Größter
Reverse-Engineering-Posten, offenes Ende, **bewusst zuletzt**.

Voraussetzung: 4.2 (Identify), sonst gibt es keine Möglichkeit, eine vermutete
Tastenzuordnung zu bestätigen. Vorgehen: eine Taste per Identify blinken,
Dump-Diff, Eintragsformat ableiten — wie es für die Remap-Tabelle bei `0xC0`
bereits gelungen ist (`docs/Keyboard_Protocol.md` Abschnitt 4).

---

## Verifikation

| Ebene | Womit | Braucht Hardware? |
|---|---|---|
| Build | `build_native.bat` | nein |
| Schema-Konsistenz | `--dump-schema` + `validate_profiles.ps1` | nein |
| Start ohne Absturz | `--dry-run` (nach 0.2 wirklich schreibfrei) | nein |
| Gerät antwortet | `--switch-test=<dev>` → Exit-Code (nach 2.1) | **ja** |
| Werte stimmen | `--kbmode=<n>`, `--kbdump` → Read-back-Diff | **ja** |
| Effekt sichtbar | `--kbmode-sweep=4` + Auge | **ja + Mensch** |

Die ersten drei Zeilen sind das CI-Gate. Die letzten drei bleiben bewusst
manuell — ein Runner hat keine LEDs, und kein Read-back der Welt beweist, dass
ein Effekt *sichtbar* animiert.

## Reihenfolge auf einen Blick

```
Phase 0  (4 Punkte)   akute Gefahren          ── ERLEDIGT 2026-08-17
Phase 1  (5 Punkte)   Regel 1 überall         ── braucht DeviceResult
Phase 2  (4 Punkte)   Tests echt machen       ── braucht Phase 1
Phase 3  (4 Punkte)   CI                      ── braucht Phase 2
Phase 4  (3 Punkte)   Hardware-Wahrheit       ── parallel, braucht dich
Phase 5  (2 Punkte)   Zonen an die Hardware   ── braucht Phase 1
Phase 6  (4 Punkte)   Zonen-Editor            ── braucht Phase 5, idealerweise 4.2
Phase 7  (1 Punkt)    ASUS Per-LED            ── braucht Phase 5
Phase 8  (1 Punkt)    Profile mit Zonen       ── braucht Phase 5
Phase 9  (offen)      Per-Tasten-Tabelle      ── braucht 4.2
```
