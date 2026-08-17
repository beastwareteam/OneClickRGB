# OneClickRGB — Projektregeln

## Regel 1: Tests müssen Live-Werte prüfen — keine veralteten Tests

**Verbindlich für dieses Projekt. Gilt für jeden Gerätepfad (EVision Keyboard/Edge,
ASUS Aura, SteelSeries, G.Skill RAM).**

Ein Schreibvorgang auf Hardware gilt erst dann als erfolgreich, wenn er
**zurückgelesen und mit dem Sollwert verglichen** wurde. Konkret:

- Rückgabewerte von `EVisionQuery` (und jedem anderen HID-Write) **nie ignorieren**.
- Nach dem Write **read-back** an derselben Adresse, Ist gegen Soll diffen.
- Statusmeldungen dürfen nur melden, was **verifiziert** wurde.
  Verboten: `"Keyboard set (Mode: 0x05)"` direkt nach einem ungeprüften Write —
  die Firmware quittiert auch Writes, die sie still verwirft.
  Richtig: `"Mode 0x05 rejected — device kept 0x06"` bzw. `"… verified"`.
- Testskripte und `--switch-test` / `--kb*`-Pfade dürfen keine Konstanten
  behaupten, die aus einem alten Dump stammen. Sie lesen den aktuellen Zustand
  vom Gerät.
- Findet ein Live-Test heraus, dass ein dokumentierter Wert falsch ist, wird
  `docs/Keyboard_Protocol.md` im selben Zug korrigiert — inkl. Confidence-Level.

**Warum:** Mehrere Fehler in diesem Projekt sind entstanden, weil die App Erfolg
gemeldet hat, ohne das Gerät zu fragen (Win-Key-Lock, Edge-Tempo, KB-Modi). Ein
grüner Test gegen eine ausgedachte Erwartung ist schlimmer als kein Test.

## Regel 2: Nie außerhalb dokumentierter Byte-Bereiche schreiben

Nur die in `docs/Keyboard_Protocol.md` Abschnitt 3 belegten Offsets beschreiben.
Keine Brute-Force-Schleifen über Offset-Kandidaten — die alte 15-Offset-Schleife
in `SetEVisionEdge` hat `+0x14..0x1D` in allen drei Profilen zerstört.
Unbekannte Bereiche zuerst per read-only Dump (`--kbdump`) und Identify klären.

## Regel 3: Read-modify-write statt Blindschreiben

Profilblöcke erst lesen, dann nur die bekannten Felder patchen, dann zurück-
schreiben. Unbekannte Bytes bleiben unangetastet.

## Build

Vor jedem Rebuild eine laufende `OneClickRGB.exe` beenden — sonst schlägt das
Linken an der gesperrten Ausgabedatei fehl.
