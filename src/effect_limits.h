#pragma once

//=============================================================================
// Effect parameter limits and the edge-strip mode table.
//
// Pulled out of oneclick_rgb_complete.cpp so that the values which end up in the
// keyboard's flash have exactly one definition and can be unit-tested without
// hardware (tests/test_cli_args.cpp). Pure: no Windows headers, no globals.
//=============================================================================

#include <cstdint>

//-----------------------------------------------------------------------------
// Hardware ranges, from docs/Keyboard_Protocol.md section 3:
//   profile+0x02 brightness 0..4, profile+0x03 speed 0..5.
//
// Neither end of the app enforces that on its own: the trackbars are set to
// those ranges but the .rgb profile files are plain text and can hold anything,
// and a probe flag can pass whatever it likes. SetEVisionEdge clamped, but
// SetEVisionKeyboard wrote both bytes through unclamped - so a hand-edited
// profile could put e.g. speed=200 into the keyboard block. Every path into a
// device write goes through these two.
//-----------------------------------------------------------------------------
static const uint8_t EFFECT_BRIGHTNESS_MAX = 4;
static const uint8_t EFFECT_SPEED_MAX      = 5;

inline uint8_t ClampBrightness(int v) {
    if (v < 0) return 0;
    if (v > (int)EFFECT_BRIGHTNESS_MAX) return EFFECT_BRIGHTNESS_MAX;
    return (uint8_t)v;
}

inline uint8_t ClampSpeed(int v) {
    if (v < 0) return 0;
    if (v > (int)EFFECT_SPEED_MAX) return EFFECT_SPEED_MAX;
    return (uint8_t)v;
}

//-----------------------------------------------------------------------------
// Edge modes (Endorfy EVision edge strip, payload byte 0 at profile+0x1E)
//-----------------------------------------------------------------------------
enum EdgeMode {
    EDGE_MODE_FREEZE = 0x00,
    EDGE_MODE_WAVE = 0x01,
    EDGE_MODE_SPECTRUM = 0x02,
    EDGE_MODE_BREATHING = 0x03,
    // On Endorfy EVision edge strips, solid color is encoded as 0x00.
    // Keep the STATIC symbol for UI semantics, but map it to FREEZE.
    EDGE_MODE_STATIC = EDGE_MODE_FREEZE,
    EDGE_MODE_OFF = 0x05
};

// Maps ComboBox edge indices to hardware byte values
// ComboBox order: Static(0), Breathing(1), Wave(2), Spectrum(3), Off(4)
static const uint8_t EDGE_MODE_TABLE[] = {
    EDGE_MODE_STATIC, EDGE_MODE_BREATHING, EDGE_MODE_WAVE, EDGE_MODE_SPECTRUM, EDGE_MODE_OFF
};
static const int EDGE_MODE_COUNT = 5;

// How much is actually known about each mode byte, per docs/Keyboard_Protocol.md
// section 3.1. The distinction matters because a read-back proves only that the
// firmware *stored* the byte - the GK650 acknowledges and stores mode bytes it
// then does not render, which is precisely the "mode verified, strip does not
// move" symptom this table is annotated for.
enum EdgeModeConfidence {
    EDGE_CONF_RENDER_SEEN = 0,   // observed on the strip by a human
    EDGE_CONF_STORED_ONLY = 1    // firmware stores it; rendering never confirmed
};

// Measured 2026-08-17 with --rendercheck=edge: NO edge mode renders on this
// device. The probe held static white, off, static red and static green at
// profile_base+0x1E, asked about each state while it was showing, and the
// answer was "nein" four times over - with every payload verified by read-back
// and the pre-probe state restored (rc=0, rendercheck_edge.txt).
//
// That retires the two entries this table used to call RENDER_SEEN. `0x00`
// carried [HIGH] "colour changes are visible on the strip" and `0x05` [HIGH]
// "strip goes dark"; both came from the era when the app wrote a colour to
// +0x23..0x25 and the same colour was then read back there, which is evidence
// about the flash and not about a light. The same run showed the firmware
// accepting *every* byte 0x00..0x0A - no validation, because nothing consumes
// them.
//
// So nothing here is RENDER_SEEN any more. The enum keeps both values because
// the keyboard block at +0x01 does render (measured the same day,
// rendercheck_kb.txt) and its mode bytes still have to be classified; promoting
// anything back to RENDER_SEEN needs a measurement in the same commit
// (CLAUDE.md rule 1).
inline EdgeModeConfidence EdgeModeConfidenceOf(uint8_t /*mode*/) {
    return EDGE_CONF_STORED_ONLY;
}

inline const char* EdgeModeName(uint8_t mode) {
    switch (mode) {
        case EDGE_MODE_STATIC:    return "static/freeze";
        case EDGE_MODE_WAVE:      return "wave?";
        case EDGE_MODE_SPECTRUM:  return "spectrum?";
        case EDGE_MODE_BREATHING: return "breathing?";
        case EDGE_MODE_OFF:       return "off";
        default:                  return "undocumented";
    }
}

inline int EdgeModeToIndex(uint8_t mode) {
    for (int i = 0; i < EDGE_MODE_COUNT; i++)
        if (EDGE_MODE_TABLE[i] == mode) return i;
    return 0; // fallback: Static
}

inline uint8_t IndexToEdgeMode(int idx) {
    if (idx >= 0 && idx < EDGE_MODE_COUNT) return EDGE_MODE_TABLE[idx];
    return EDGE_MODE_STATIC;
}

inline uint8_t NormalizeEdgeMode(uint8_t mode) {
    // Legacy configs used 0x04 for "static" and often produced rainbow-only behavior.
    if (mode == 0x04) return EDGE_MODE_STATIC;
    if (mode == EDGE_MODE_OFF ||
        mode == EDGE_MODE_STATIC ||
        mode == EDGE_MODE_BREATHING ||
        mode == EDGE_MODE_WAVE ||
        mode == EDGE_MODE_SPECTRUM) {
        return mode;
    }
    return EDGE_MODE_STATIC;
}
