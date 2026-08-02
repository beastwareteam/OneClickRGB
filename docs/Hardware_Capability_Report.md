# OneClickRGB - Hardware Capability Report

Generated: 20260605-155806  
Schema version: 1

## Summary

- Hardware detected: **4**
- Ready to use: ASUS Aura Mainboard, G.Skill Trident Z5 RGB (SMBus)
- Needs work: Endorfy/EVision Keyboard, SteelSeries Rival 600
- Critical risks: Endorfy/EVision Keyboard: Win-Lock / segment / effect bytes NOT yet identified - no targeted write must occur until verified via --probe-interactive

## ASUS Aura Mainboard  (`aura`)

- VID:PID = `0x0B05:0x19AF`
- Status: **verified**
- Firmware: `AULA3-AR32-0222`

### HID interfaces / collections

| MI | UsagePage | Usage | Product |
|---|---|---|---|
| 2 | 0xFF72 | 0x00A1 | AURA LED Controller |

### Addressing units / LED points

- **Mainboard (4 LEDs)** - 4 LED(s)
- **Addressable 1 (max 120 LEDs)** - 1 LED(s)
- **Addressable 2 (max 120 LEDs)** - 1 LED(s)
- **Addressable 3 (max 120 LEDs)** - 1 LED(s)

### Risks / open questions

- Live per-channel effect mode has no confirmed GET command - rainbow/firmware-mode detection deferred to interactive probe

## Endorfy/EVision Keyboard  (`keyboard`)

- VID:PID = `0x3299:0x4E9F`
- Status: **partial**

### HID interfaces / collections

| MI | UsagePage | Usage | Product |
|---|---|---|---|
| 1 | 0x0001 | 0x0006 | GK650 Gaming Keyboard |
| 1 | 0x0001 | 0x0002 | GK650 Gaming Keyboard |
| 0 | 0x0001 | 0x0006 | GK650 Gaming Keyboard |
| 1 | 0x0001 | 0x0080 | GK650 Gaming Keyboard |
| 1 | 0x000C | 0x0001 | GK650 Gaming Keyboard |
| 1 | 0xFF1C | 0x0092 | GK650 Gaming Keyboard |

### Addressing units / LED points

- **MI_1 UP=0xFF1C** [partial]
    - profile 0 base 0x0001: `06 04 02 00 00 00 00 00 00 00 00 00 00 00 00 00  00 00 00 04 02 00 04 02 00 04 00 04 02 00 04 02  00 00 00 13 FF 00 01 00 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  `
    - profile 1 base 0x0041: `06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  00 00 00 04 02 00 04 02 00 04 00 04 02 00 04 02  00 00 00 13 FF 00 01 00 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  `
    - profile 2 base 0x0081: `06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  00 00 00 04 02 00 04 02 00 04 00 04 02 00 04 02  00 00 00 13 FF 00 01 00 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 20  `

### Risks / open questions

- Win-Lock / segment / effect bytes NOT yet identified - no targeted write must occur until verified via --probe-interactive

## SteelSeries Rival 600  (`mouse`)

- VID:PID = `0x1038:0x1724`
- Status: **partial**

### HID interfaces / collections

| MI | UsagePage | Usage | Product |
|---|---|---|---|
| 2 | 0x0001 | 0x0006 | SteelSeries Rival 600 |
| 2 | 0x000C | 0x0001 | SteelSeries Rival 600 |
| 1 | 0x0001 | 0x0002 |  |
| 1 | 0x000C | 0x0001 |  |
| 0 | 0xFFC0 | 0x0001 | SteelSeries Rival 600 |
| 1 | 0x0001 | 0x0006 |  |

### Addressing units / LED points

- **Scroll Wheel** - 1 LED(s)
- **Logo** - 1 LED(s)
- **Side Z2** - 1 LED(s)
- **Side Z3** - 1 LED(s)
- **Side Z4** - 1 LED(s)
- **Side Z5** - 1 LED(s)
- **Side Z6** - 1 LED(s)
- **Side Z7** - 1 LED(s)

### Risks / open questions

- Zone identity/layout taken from SteelSeries GG DB (8 zones: wheel/logo/z2-z7); write protocol is write-only
- Bit (1<<i) -> physical-zone mapping NOT yet light-verified - confirm via Identify before blessing as verified

## G.Skill Trident Z5 RGB (SMBus)  (`ram`)

- VID:PID = `SMBus:-`
- Status: **verified**

### Addressing units / LED points

- **AUDA0-E6K5-0101** - 8 LED(s) @ 0x71
- **AUDA0-E6K5-0101** - 8 LED(s) @ 0x73

## Next steps

1. Review keyboard configDump to locate Win-Lock / segment / effect bytes
1. Run --probe-interactive to bless verified units (Identify) into config.json
1. Confirm the single correct Edge-LED offset to replace the 15x brute-force write
1. Determine a confirmed Aura per-channel effect-mode GET for rainbow detection
