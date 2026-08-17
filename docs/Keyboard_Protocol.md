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

### 3.1 Edge LEDs — solved
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
region in **all three** profiles. The unlock write in `SetEVisionKeyboard`
clears only the first two bytes and only in the **active** profile. Current dump:

| Profile | `+0x14..0x1D` |
|---|---|
| P0 (active) | `00 00 00 04 02 00 04 00 04 02` — first two cleared |
| P1 | `04 02 00 04 02 00 04 00 04 02` — untouched garbage |
| P2 | `04 02 00 04 02 00 04 00 04 02` — untouched garbage |

Switching to profile 1 or 2 therefore still exposes the corrupted region. Do not
"clean" it by writing zeros across the range — the field semantics are unknown
and section 3's rule applies. It needs an Identify pass first.

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
   on a live read-back, not only on the capture. Remaining: the purely visual
   confirmation that the strip renders mode `0x04`.*
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
5. Per-key colour table at `0x2C0`+ is captured but undecoded. Per-key/per-zone
   keyboard colour is not implemented anywhere in the app; there is no
   `keyboardZones` model (only `mouseZones` in `light_zone.h`), and
   `SetSteelSeriesZones()` is reachable only from `--mouse-zones-test`, not from
   the UI or the normal apply path.
