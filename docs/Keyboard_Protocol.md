# Endorfy / EVision Keyboard Protocol (GK650)

Derived from the live `--probe` full-address sweep of the on-board config
memory (collection `MI_1 / usage_page 0xFF1C / usage 0x0092`), cross-validated
against the simultaneously-captured RAM/keyboard live colour
(**R=0x00, G=0x13, B=0xFF**, i.e. R0 G19 B255).

> Confidence legend: **[HIGH]** exact structural match / cross-validated ·
> **[MED]** strong inference, not yet light-verified · **[?]** unknown, needs
> `--probe-interactive` Identify before any write.

## 1. Device facts (live-confirmed)

| Item | Value |
|---|---|
| VID:PID | `0x3299:0x4E9F` |
| Product string | `GK650 Gaming Keyboard` |
| RGB control collection | `MI_1`, usage_page `0xFF1C`, usage `0x0092` |
| Other collections | MI_0 keyboard (0x0001), MI_1 0x0001/0x000C consumer |
| Transport | EVision V2: report id `0x04`, 64-byte packet, 16-bit checksum |

**Important deviation from the original plan:** the plan assumed Win-Lock /
segments / profiles lived in *unused vendor collections `MI_01 COL02–04`*. The
live scan shows **only one** vendor collection (`0xFF1C`) — the one already in
use. Everything must be decoded from this collection's on-board config memory
(below), not from extra collections.

## 2. Access pattern

```
EVisionQuery(dev, 0x01, ...)              // begin-configure (required first)
EVisionQuery(dev, 0x05, offset, len)      // READ  config memory
EVisionQuery(dev, 0x06, offset, data,len) // WRITE config memory
EVisionQuery(dev, 0x02, ...)              // end-configure
```
Active profile index = read 1 byte at offset `0x00`.

## 3. Memory map

Three identical **0x40-byte profile blocks**: P0 `0x00`, P1 `0x40`, P2 `0x80`.
Offsets below are **relative to the profile base** (add `0x40`/`0x80` for P1/P2).

| Rel. offset | Field | Confidence | Notes |
|---|---|---|---|
| `+0x00` | Active-profile selector | [HIGH] | read at abs `0x00` = 0 (P0 active) |
| `+0x01` | Keyboard effect **mode** | [HIGH] | `0x06` = STATIC (matches `KB_MODE_STATIC`) |
| `+0x02` | Brightness (0–4) | [HIGH] | P0=`0x04`; inactive profiles `0x00` |
| `+0x03` | Speed (0–5) | [MED] | `0x00` in capture |
| `+0x04` | Direction | [MED] | `0x00` |
| `+0x05` | Random-colour flag | [MED] | `0x00` |
| `+0x06..08` | Key **R,G,B** | [HIGH] | `00 13 FF` = live colour ✔ |
| `+0x09` | colour offset | [MED] | `0x00` |
| **`+0x0A`** | **Commit / save to flash** | **[HIGH]** | must be `0x01` or the effect is stored but never applied — see 3.2 |
| `+0x0B..0x13` | padding | [?] | zeros |
| `+0x14..0x1D` | per-zone tuples (`04 02 00 …`) | [?] | brightness/speed-like; zone boundaries need Identify |
| **`+0x1E..0x27`** | **EDGE payload (10 bytes)** | **[HIGH]** | `00 04 02 00 00 00 13 FF 00 01` = exact `[mode,bright,speed,dir,rand,R,G,B,coloff,save]` |
| `+0x28..0x3F` | padding | [?] | zeros |

### 3.1 Edge LEDs — offset solved, but nothing there renders

> **Result of 2026-08-17 (`rendercheck_edge.txt`, rc=0, all four states verified
> and restored):** the payload at `profile_base+0x1E` stores everything and
> **renders nothing**. Static white, off, static red, static green — four held
> states, four times "nein". Meanwhile the keyboard block at `+0x01` passed the
> same four states (`rendercheck_kb.txt`, four times "ja"), so the device, the
> transport and the probe are all fine; this offset simply does not drive a
> light on this unit.
>
> Read the rest of this section with that in mind: the byte layout below is
> correct and confirmed, the *conclusion* that it controls an edge strip is not.
> Most likely the 0x40-byte profile carries two effect payloads — `+0x01` for
> the main lighting and `+0x1E` for a second zone this model does not populate.
> That also explains why the firmware accepts every value `0x00..0x0A` there
> without validating anything: nothing consumes them.
>
> Open and **not** a software question: does this keyboard have side/edge
> lighting at all? Until that is answered, the Edge controls in the UI offer
> something no measurement supports.
The edge strip data is written as the 10-byte payload at **`profile_base + 0x1E`**
(absolute `0x1E` / `0x5E` / `0x9E`). In the live capture P0+0x1E reads
`00 04 02 00 00 00 13 FF 00 01`:

```
+0x1E mode=0x00 (FREEZE/STATIC)   +0x23 R=0x00
+0x1F brightness=0x04             +0x24 G=0x13
+0x20 speed=0x02                  +0x25 B=0xFF   (= live colour ✔)
+0x21 direction=0x00              +0x26 colour-offset=0x00
+0x22 random=0x00                 +0x27 save=0x01
```

➡ **Phase 2 action:** replace the 15-offset brute-force loop in `SetEVisionEdge`
with a single write to `activeProfile*0x40 + 0x1E`. The current "P-direct"
candidates (`0x1E/0x5E/0x9E`) are the correct ones; the other 12 are noise.
— *done; the write goes to `active_profile*0x40 + 0x1E` only.*

#### Mode byte `+0x1E` — confidence per value

The offset is [HIGH]. The **values** are not, and the two must not be conflated:
the firmware acknowledges, stores and reads back mode bytes it then does not
render. A matching read-back is therefore evidence about the flash, not about
the light (rule 1).

| Value | Meaning | Rendering confidence | Basis |
|---|---|---|---|
| `0x00` | static / freeze | **[HIGH — does NOT render]** | `--rendercheck=edge` held static white, red and green here; nothing reacted |
| `0x01` | wave? | **[HIGH — does NOT render]** | swept 13:11, no observation; anchor disproves the whole offset |
| `0x02` | spectrum? | **[HIGH — does NOT render]** | as above |
| `0x03` | breathing? | **[HIGH — does NOT render]** | as above |
| `0x05` | off | **[HIGH — does NOT render]** | `--rendercheck=edge` held off (brightness 0, RGB black); nothing went dark |
| `0x04` | legacy "static" | — | mapped to `0x00` by `NormalizeEdgeMode` |

The two `[HIGH]` entries this table used to carry (`0x00` "colour changes are
visible on the strip", `0x05` "strip goes dark") were **wrong**, and the way
they were wrong is worth keeping in view: both came from the app writing a
colour to `+0x23..0x25` and reading the same colour back. That is evidence about
flash, not about light. Nobody had ever held one state and looked.

Live state on 2026-08-17, straight from `--kbdump` (P0 `+0x1E..0x27`):

```
03 04 05 00 00 00 13 FF 00 01
^mode 0x03 (breathing)  ^bright 4  ^speed 5           ^commit 01
```

Everything the protocol asks for is in place — mode set, brightness non-zero,
speed non-zero, commit flag set — and the strip still does not move. That is the
open question, and it is not answerable by reading bytes back.

#### What narrows it (2026-08-17, from the device)

Colour **does** render — a static colour change is visible on both the keyboard
and the strip — while no effect animates on either. Colour (`+0x06..0x08`) and
mode (`+0x01`) sit in the same 10-byte payload, leave in the same write and read
back identically. If the firmware renders the colour from that block, it reads
the block; so the offset is not the suspect any more. What is left is the **mode
values**, and `KB_MODE_TABLE` is an unverified guess outside `0x06`.

#### What is *not* evidence, despite being on disk

* `edgemode_probe.txt` and `unlock_winkey.txt` in `%APPDATA%\OneClickRGB\docs`
  are artefacts of `tools\check_dryrun_flags.ps1` (the deliberate
  `--dry-run --edgemode=3 --edgemode-sweep` case, exit 2) and of a dry run. The
  mode and speed sweeps have **never run on hardware**; there is no measurement
  yet.
* `A_vor_cycle.txt` / `B_nach_cycle.txt` are byte-identical (same MD5) and were
  a failed attempt, not a null result. They prove nothing and are discarded.
* The **13:11 edge mode sweep** wrote all eleven values `0x00..0x0A`, each
  verified by read-back, and restored the pre-probe payload — technically a clean
  run. Its
  `animated?` column is empty because nobody was watching, so it establishes
  nothing about rendering either. Do not read it as "no mode animates".

* The **first `--rendercheck` run** is void, and it did damage. `--rendercheck=edge`
  and `--rendercheck=kb` were started at the same time. `deviceIoMutex` only
  serialises threads inside one process, so the two instances shared the HID
  collection: they interleaved their output into one report file (a keyboard
  header above an edge restore line), each read back traffic the other had
  caused, one of them read the keyboard block as **18 zero bytes** — and
  restored those zeros into flash. The keyboard block went to mode 0,
  brightness 0, commit 0, i.e. dark, while every line of the report said
  `verified`. Repaired with `--kbmode=0x06`; the block reads
  `06 04 05 00 00 00 13 FF 00 01` again.

  Preliminary observation from that run, **not** yet a measurement: the
  keyboard lighting reacted to all four states, the side/edge lighting to none.
  Plausible, and it matches everything else here, but it was made while two
  processes drove the device — it needs one clean repeat per target before
  anything is changed on the strength of it.

  Fixed since: a named probe lock (`Local\OneClickRGB_HidProbe`) that every
  hardware probe takes and a second instance refuses to start against (exit 3);
  one report file per target; and both snapshot helpers now read twice and
  refuse to become a restore point unless the two reads agree. A read-back is
  only evidence under exclusivity — that assumption was implicit everywhere in
  this document and is now enforced.

One thing that run *did* show: the firmware stored **every** value, including
`0x06..0x0A`, which can hardly all be valid edge modes. A block that is really
interpreted would be expected to refuse something. That leans towards "this
memory is not what renders" over "the values are wrong" — which is precisely
what `--rendercheck` was built to decide, instead of another round of guessing.

➡ **Tooling:** `--edgemode=<n>` writes one mode through the production path and
reports the full 10-byte read-back; `--edgemode-sweep[=sec]` walks `0x00..0x0A`
with a hold time; `--edgespeed-sweep[=sec]` holds one mode and walks the speed
byte `0..5` (`--edgespeed-mode=<n>` picks the mode, default `0x03`). Report:
`%APPDATA%\OneClickRGB\docs\edgemode_probe.txt`. Both sweeps restore the payload
they found before the first step. `tools\edge_probe_session.ps1` drives the whole
sequence including the `--kbdump` before/after collateral check.

**Add `--confirm` to either sweep.** Without it the probe sleeps through the hold
time and leaves an empty `animated?` column for someone to fill in later — which
is exactly how the 2026-08-17 13:11 run produced eleven flawless rows and zero
information. With it, each step holds its state while a dialog asks about that
state, and the answer (`ja` / `nein` / `abgebrochen`) goes straight into the
column. Nothing to time, nothing to reconstruct, nothing to remember. Cancel
ends the sweep and triggers the restore; a dismissed dialog is never recorded as
"nein" — an answer nobody gave is not a measurement.

**Before any sweep, run the anchor:** `--rendercheck=edge` / `--rendercheck=kb`
holds four states — white, off, red, green — and asks about each one. It answers
the prior question that every mode sweep silently assumes: *does this memory
drive the lighting at all?* If off does not darken and no colour arrives, the
block is storage rather than live state, no mode value can help, and the probe
says so in its own verdict line. Report: `rendercheck.txt`.

The three `[?]` entries stay in `EDGE_MODE_TABLE` until that measurement exists —
removing them first would delete the very entries the sweep has to test. Once it
has run, whatever does not animate comes out of the table and out of the ComboBox.

### 3.2 Keyboard block is the same 10-byte payload — solved

The keyboard block at `+0x01` has **the identical layout** to the edge payload at
`+0x1E`:

```
[mode, brightness, speed, direction, random, R, G, B, colourOffset, save]
```

`SetEVisionKeyboard` wrote only the first **nine** bytes and left the trailing
`save` flag at `+0x0A` untouched. The mode byte therefore reached the flash and
read back correctly, while the keyboard kept rendering the previous effect —
selecting Breathing in the UI changed the colour but never the animation.

Live evidence, read back straight after writing mode `0x05` (Breathing):

```
before fix:  05 04 04 00 00 00 13 FF 00 [00]   <- +0x0A = 00, not applied
after fix:   05 04 04 00 00 00 13 FF 00 [01]   <- +0x0A = 01, committed
```

➡ **Rule:** every effect write to a profile block — keyboard *and* edge — must
set the tenth byte to `0x01`. A write result of `18` and a matching read-back are
**not** sufficient proof that an effect was applied.

### 3.3 State of `+0x14..0x1D` in the live device (2026-08-17)

The old brute-force left the pattern `04 02 00 04 02 00 04 00 04 02` in this
region in **all three** profiles. Current dump:

| Profile | `+0x14..0x1D` |
|---|---|
| P0 (active) | `00 00 00 04 02 00 04 00 04 02` — first three read `00` |
| P1 | `04 02 00 04 02 00 04 00 04 02` — untouched garbage |
| P2 | `04 02 00 04 02 00 04 00 04 02` — untouched garbage |

Switching to profile 1 or 2 therefore still exposes the corrupted region. Do not
"clean" it by writing zeros across the range — the field semantics are unknown
and section 3's rule applies. It needs an Identify pass first.

**No apply writes here anymore.** `SetEVisionKeyboard` used to zero `+0x14/+0x15`
of the active profile on *every* apply, unverified, as a "Win-key unlock". That
write is gone. Three reasons, each sufficient on its own:

* it targets a region this document marks `[?]` (rule 2), blindly (rule 3), and
  never read the result back (rule 1);
* section 4.1 below already disproves its purpose — the lock is not in this
  config memory at all;
* if `+0x14/+0x15` turn out to hold the **edge zone's** brightness and speed —
  the leading suspicion for the frozen edge effects — then zeroing them on the
  keyboard path, which runs *before* the edge write in `ApplyColors`, is what
  sets the edge animation to speed 0 on every apply.

It survives as the explicit one-shot `--unlock-winkey`, which reads the region
first, writes only those two bytes, reads back and reports both states to
`%APPDATA%\OneClickRGB\docs\unlock_winkey.txt`. Note what this means for
verification: because P0 `+0x14/+0x15` already read `00 00`, a dump diff cannot
distinguish "the write is gone" from "it wrote zeros over zeros". The removal is
established by the source change; the dump only confirms no *new* collateral.
Restoring the region needs the factory-reset dump from item 2 of section 5.

## 4. Region `0xC0`+ — key-remap table  [HIGH, decoded]

Captured in full with `--kbdump` (read-only, command `0x05` over `0x000..0x3FF`).
The table starts at `0xC0` and runs to ~`0x239`, then zero padding, then a
per-key colour table from `0x2C0` to the end.

**Entry format — 3 bytes, matrix order (column by column, left to right):**

| Prefix | Meaning | Bytes 2–3 |
|---|---|---|
| `0x20` | standard key | `<HID modifier bitmask> <HID usage>` |
| `0x30` | consumer/media key | 16-bit consumer usage, little-endian |
| `0xA0` | Fn / layer key | layer index |
| `20 00 00` | unassigned matrix position | — |

Modifier bitmask follows the HID convention: `01` LCtrl, `02` LShift, `04` LAlt,
`08` **LGUI (Windows)**, `10` RCtrl, `20` RShift, `40` RAlt, `80` RGUI.

Decoded start of the table, confirming the column-major matrix order:

```
0xC0 20 00 29 Esc     0xD2 20 00 00 (none)
0xC3 20 00 35 `       0xD5 20 00 1E 1
0xC6 20 00 2B Tab     0xD8 20 00 14 Q
0xC9 20 00 39 Caps    0xDB 20 00 04 A
0xCC 20 02 00 LShift  0xDE 20 00 64 NonUS-\
0xCF 20 01 00 LCtrl   0xE1 20 08 00 LGUI  <- Windows key
```

Examples of the other prefixes: `0x1F2 = 30 92 01` (consumer `0x0192`,
Calculator), `0x204 = 30 E2 00` (Mute), `0x216 = 30 EA 00` (Volume Down),
`0x195 = A0 01 00` (Fn, layer 1).

### 4.1 Win-Lock is NOT in the config memory  [HIGH]

The earlier assumption — that Win-Lock is implemented by remapping the Win key
to a no-op in this table — is **disproved**. In a live dump taken while the Win
key was locked, entry `0xE1` reads `20 08 00`, i.e. Left GUI, fully intact. No
matrix position is blanked.

The per-profile candidate flag at `profile_base + 0x2E` is also **not** it:
`--kbtest=unlock` cleared it to `0x00` in the active profile (verified by
read-back) and the Win key stayed locked.

Nothing else in `0x00..0x3FF` correlates. Conclusion: the lock is keyboard-side
state that the vendor config memory does not expose — on these boards it is the
**Fn-layer hardware toggle** (Fn + Win, sometimes labelled Game Mode). The
application can neither read nor set it, and must not try.

➡ **Rule:** never write outside the byte ranges documented in section 3. The old
15-offset brute-force in `SetEVisionEdge` violated this and corrupted
`+0x14..0x1D` across all three profiles; it has been replaced by a single write
to `active_profile*0x40 + 0x1E`.

## 5. Open items for interactive verification
1. Edge offset light-check: write only `+0x1E` of the active profile, confirm a
   single clean edge update (no brute-force). — *read-back verified on device
   2026-08-17, `--switch-test=edge-diagnose` rc=0 on GK650 (VID 0x3299 /
   PID 0x4E9F, RGB interface = `MI_01&Col04`, UsagePage 0xFF1C):*

   ```
   before: 03 04 04 00 00 00 13 FF 00 01
   wrote : 04 04 02 00 00 00 00 FF 00 01
   after : 04 04 02 00 00 00 00 FF 00 01   -> verified
   ```

   *Collateral check by `--kbdump` before/after: the only bytes that moved in
   `0x000..0x3FF` are `0x1E..0x27` — the 10 payload bytes themselves.
   `+0x14..0x1D` is untouched in **all three** profiles (P1 `0x54..0x5D` and
   P2 `0x94..0x9D` byte-identical), which is the direct counter-proof that the
   15-offset brute-force is gone. Confidence for `+0x1E..0x27` [HIGH] now rests
   on a live read-back, not only on the capture.*

   *Correction to that run: the payload it wrote had mode `0x04`, not
   `EDGE_MODE_STATIC` as the code comment beside it claimed — `0x04` is the
   legacy value `NormalizeEdgeMode` maps away. So it verified the offset and the
   commit flag, but it left the strip on an undocumented mode and it was not
   byte-identical to the production path. The constant now reads
   `EDGE_MODE_STATIC` and the collateral finding above is unaffected.*

   *Automated since: `tools\kbdump_diff.ps1` performs this before/after
   comparison over all `0x400` bytes and fails on any change outside a given
   range; `tests\test_kbdump_diff.ps1` checks it against synthetic dumps that
   contain known violations (a write to `+0x14`, a write into P1/P2, a write into
   the remap table).*
2. Decode `+0x14..0x1D` zone tuples (Identify per zone). Note these bytes were
   overwritten by the old brute-force, so the current contents are garbage and
   a factory reset is needed before they can be decoded meaningfully.
3. ~~Locate Win-Lock by diffing dumps with vendor SW toggled.~~ — *resolved, see
   section 4.1: not present in the config memory at all.*
4. Confirm which `KB_MODE_*` values actually animate (drop dead modes). Only
   `0x06` = STATIC is cross-validated; the remaining ten entries in
   `KB_MODE_TABLE` are unverified guesses. Tooling now exists:
   `--kbmode=<n>` writes one mode through the production path and reports the
   read-back, `--kbmode-sweep[=sec]` walks `0x00..0x14` with a hold time.
   Report goes to `%APPDATA%\OneClickRGB\docs\kbmode_probe.txt`. Note that the
   probe's "accepted" verdict only means the byte was stored — after the `+0x0A`
   fix (3.2) the remaining question is purely which modes the firmware renders.

   *Since 2026-08-17 the sweep rolls back:* it snapshots the 18-byte block at
   `profile_base+0x01` before the first step and restores it after the last,
   read-back verified, and reports the outcome — the same contract the edge
   sweeps already had via `RestoreEVisionEdgePayload`. Without it a run left the
   keyboard parked on `0x14`, a value that is not even in `KB_MODE_TABLE`. A
   failed read-back or a failed restore now makes the probe exit non-zero; a
   `REJECTED` row does not, because which bytes the firmware refuses is the
   measurement. `--kbmode=<n>` still deliberately keeps what it set. **The sweep
   has not been run on hardware yet.**
5. Per-key colour table at `0x2C0`+ is captured but undecoded. Per-key/per-zone
   keyboard colour is not implemented anywhere in the app; there is no
   `keyboardZones` model (only `mouseZones` in `light_zone.h`), and
   `SetSteelSeriesZones()` is reachable only from `--mouse-zones-test`, not from
   the UI or the normal apply path.
6. **Which `+0x1E` mode bytes the firmware renders** — the open question that
   `--edgemode-sweep` exists for, see the confidence table in 3.1. As of
   2026-08-17 the strip holds `03 04 05 … 01` (breathing, brightness 4, speed 5,
   commit set) and does not animate. Byte transfer is verified; rendering is not.
   Candidates, in the order the evidence supports them:
   1. `+0x14/+0x15` hold the edge zone's brightness/speed and the apply-time
      zero-write was setting the animation to speed 0. Note carefully what
      removing that write does and does not achieve: it stops *new* damage, but
      P0 `+0x14/+0x15` already read `00 00` and nothing writes non-zero there, so
      if this candidate is right the strip stays frozen on P0 even now.

      **Cheapest way to separate this candidate from 3 and 4** — no undocumented
      write, no factory reset. P1 and P2 still carry the original `04 02` at
      `+0x14/+0x15` (3.3), while P0 has zeros. Switch the keyboard's on-board
      profile (usually Fn+1/2/3), select Breathing so the app writes `0x03` to
      that profile's own edge slot at `+0x5E`/`+0x9E` — a documented offset — and
      look:

      | Observation | Conclusion |
      |---|---|
      | animates on P1/P2, frozen on P0 | candidate 1 confirmed: `+0x14/+0x15` carry the edge zone's brightness/speed, and `04 02` is the value to restore |
      | frozen on all three profiles | candidate 1 ruled out; `+0x14/+0x15` are not it, continue with `--edgespeed-sweep` and `--edgemode-sweep` |

      The second row also disposes of candidate 2 as an explanation on its own,
      since P1/P2 hold the *undamaged* pattern.
   2. the whole region `+0x14..0x1D` is brute-force debris in all three profiles
      and needs the factory reset from item 2 before anything there can be
      trusted.
   3. speed polarity is inverted, so the animation runs at the invisible end —
      `--edgespeed-sweep` settles this.
   4. `0x01/0x02/0x03` are simply the wrong bytes — `--edgemode-sweep` settles
      this.
7. **Speed byte `+0x03` / `+0x20` polarity is unknown.** The comment in
   `SetEVisionKeyboard` claimed "0-5, inverted" while the probe next to it assumed
   the opposite; neither had been checked against a moving light, so the claim has
   been removed rather than picked. The value is passed through as the slider
   gives it until `--edgespeed-sweep` produces an observation. Whichever way it
   resolves, note that the range is 0..5 and **both** ends are now clamped on
   every path into a write (`ClampSpeed`/`ClampBrightness` in
   `src/effect_limits.h`) — including `LoadProfile`, which used to pass whatever
   a `.rgb` file contained straight into the profile block.

## 6. Automated checks

Nothing here can prove an effect renders — that needs an eye. What it does prove
is that the app writes only where it is allowed to, reports only what it read
back, and does not write at all under `--dry-run`.

| Command | Hardware | Checks |
|---|---|---|
| `tests\run_tests.cmd` | no | 138 unit checks over `cli_args.h` + `effect_limits.h`: flag tokenising, strict value parsing, clamps, edge-mode table round-trip, `NormalizeEdgeMode` closure over the ComboBox table, the `EDGE_MODE_STATIC != 0x04` invariant |
| `tests\test_kbdump_diff.ps1` | no | the collateral checker itself, against synthetic dumps containing known rule-2 violations |
| `tools\check_dryrun_flags.ps1` | reads only | every probe flag under `--dry-run`: exit code, immediate abort instead of sleeping through the hold times, report without measurement rows, skip line in `debug.log`; plus that a mistyped `--kbmode-sweep5` starts no sweep. Also covers the dialog paths (`--rendercheck`, `--confirm`) — under `--dry-run` they must return *before* the first `MessageBox`, since a dialog waiting for a click in an unattended test would hang it to the timeout and disguise the failure |
| `--rendercheck=edge\|kb` | **writes** | the anchor: four held states (white / off / red / green), one dialog each, verdict line, snapshot + verified restore. Answers whether the block renders at all before any mode sweep is worth running. Report per target: `rendercheck_edge.txt` / `rendercheck_kb.txt` |

**Run one probe at a time.** Every hardware probe takes the named lock
`Local\OneClickRGB_HidProbe` and a second one exits 3 without touching the
device. This is not a convenience: `EVisionQuery` writes a report and reads
whichever answer arrives next, so with two processes on the collection a
read-back can belong to the other one's request — and a snapshot read that way
gets written back into flash at the end of the probe. That is how the keyboard
was switched off by two diagnostics that both reported success. The lock is
taken *after* the `--dry-run` bail-outs, so dry runs may still overlap freely.
| `tools\kbdump_diff.ps1` | no | before/after dump comparison over `0x000..0x3FF`, fails on any change outside a given range |
| `tools\edge_probe_session.ps1` | **writes** | drives dump → sweep → dump → collateral diff in one go, and refuses to start while the GUI is running. `-What mode` / `-What speed` sweep the edge payload, `-What kbmode` the keyboard block; the allowed-range check for the collateral diff follows the chosen path (`+0x1E..0x27` vs `+0x01..0x12`) |
| `tools\validate_profiles.ps1` | no | `.rgb` profiles against what `LoadProfile` accepts |
