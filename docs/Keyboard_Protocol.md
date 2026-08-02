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
| `+0x09..0x13` | colour-offset / padding | [?] | zeros |
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

## 4. Region `0xC0`+ — key-remap / macro table  [?]
Beyond the 3 profile blocks the memory continues with repeating `20 00 XX`
triples (e.g. `20 00 29 20 00 35 20 00 2B …`). This looks like a per-key
assignment / macro table (`0x20` entry prefix, `XX` = HID usage/scancode).

**Win-Lock candidate region.** No obvious Win-Lock flag exists in `0x00..0xBF`.
On these keyboards the Win key is usually disabled by *remapping its matrix
position to "no-op"* inside this table — so Win-Lock most likely lives in
`0xC0`+, **not** as a single flag byte. This must be confirmed with
`--probe-interactive` (toggle Win-Lock in vendor SW ↔ re-dump ↔ diff) **before**
any write. Until verified `[?]`, the app must never write here (avoids the
unwanted Win-key lock the old code caused).

## 5. Open items for interactive verification
1. Edge offset light-check: write only `+0x1E` of the active profile, confirm a
   single clean edge update (no brute-force).
2. Decode `+0x14..0x1D` zone tuples (Identify per zone).
3. Locate Win-Lock by diffing dumps with vendor SW toggled.
4. Confirm which `KB_MODE_*` values actually animate (drop dead modes).
