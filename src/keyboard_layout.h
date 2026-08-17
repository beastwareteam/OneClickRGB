#pragma once

//=============================================================================
// keyboard_layout.h - the GK650 key matrix, decoded from the device.
//
// Everything here is pure: no Windows headers, no globals, no I/O, so it can be
// exercised without a keyboard on the USB bus (tests/test_cli_args.cpp). It
// takes the raw bytes of the on-board config memory and turns them into a list
// of keys with a matrix position, a label and the offset of that key's colour
// triple.
//
// Why decoded and not tabulated: the labels come out of the key-remap table at
// 0xC0 (docs/Keyboard_Protocol.md section 4), which is what the *device*
// currently has assigned to each matrix position. A hardcoded keyboard picture
// would go wrong the moment someone remaps a key, and it would silently claim
// knowledge about a model this code has never seen. Reading it means the layout
// is a measurement, and an unreadable device produces an empty layout rather
// than a plausible-looking fiction.
//
// Confidence, stated up front because two very different things live here:
//
//   * The remap table - base, 3-byte entries, column-major order, 0x12 stride,
//     six rows - is [HIGH]. It is cross-validated against a live dump: the
//     decode reads back as a keyboard (Esc/`/Tab/Caps/LShift/LCtrl down the
//     first column, then 1/Q/A/NonUS-\/LGUI, ...).
//   * The per-key colour table geometry - base 0x2C0, three bytes per key, same
//     slot order as the matrix - is [MED]: inferred from a dump in which
//     0x2C0.. holds an unbroken run of the profile's own colour. Which slot maps
//     to which key is exactly what --keyidentify measures, and the byte after
//     0x2EF does *not* continue the pattern, so the extent is not settled
//     either. Nothing here may be treated as verified until that measurement
//     exists (CLAUDE.md rule 1).
//=============================================================================

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace kblayout {

//-----------------------------------------------------------------------------
// Geometry
//-----------------------------------------------------------------------------

// Key-remap table (docs section 4) - [HIGH], decoded from a live dump.
static const uint16_t REMAP_BASE       = 0x00C0;
static const int      MATRIX_ROWS      = 6;
static const uint16_t REMAP_COL_STRIDE = 0x0012;   // MATRIX_ROWS * 3 bytes
static const int      MATRIX_COLS      = 21;
static const int      MATRIX_SLOTS     = MATRIX_ROWS * MATRIX_COLS;   // 126
// One past the last remap byte: 0xC0 + 21*0x12 = 0x23A. The dump shows zero
// padding from there, which is what fixes the column count at 21.
static const uint16_t REMAP_END        = (uint16_t)(REMAP_BASE + MATRIX_COLS * REMAP_COL_STRIDE);

// Per-key colour table (docs section 5.5). Two different boundaries live here
// and conflating them is how the six-byte discrepancy below would get lost.
static const uint16_t KEYCOLOR_BASE   = 0x02C0;
static const int      KEYCOLOR_STRIDE = 3;         // R, G, B

// [HIGH - measured] Where the non-zero region actually ends. Read-only dump of
// 2026-08-17, `--kbdump-range=0x2A0-0x5FF`: 0x2A0..0x2BF is zero, 0x2C0..0x43F
// is non-zero without a gap, 0x440 onwards is zero again. Every read answered
// (rr=16), so this is the data's own boundary and not the end of the memory -
// the config space keeps answering to at least 0x5FF.
static const uint16_t KEYCOLOR_REGION_END = 0x0440;
static const int      KEYCOLOR_TRIPLES =
    ((int)KEYCOLOR_REGION_END - (int)KEYCOLOR_BASE) / KEYCOLOR_STRIDE;   // 128

// [MED - inferred] Where the table would end if it held one triple per matrix
// slot in matrix order: 0x2C0 + 126*3 = 0x43A.
static const uint16_t KEYCOLOR_SLOTS_END =
    (uint16_t)(KEYCOLOR_BASE + MATRIX_SLOTS * KEYCOLOR_STRIDE);

// The two do NOT agree, and that gap is a finding rather than an inconvenience:
// the region holds 128 triples, the matrix has 126 slots, so two triples at the
// end belong to nothing under the current assumption. Possible readings - none
// of them chosen here - are that the table is indexed by a linear LED number
// rather than by matrix position, that it starts at a different phase, or that
// the last two triples are something else entirely. `--keyidentify` can address
// them (IsKeyColorOffset covers the measured region, not the inferred one),
// which makes them the most informative offsets to sample.
//
// Note also that KEYCOLOR_REGION_END is past 0x400: the whole tail of this
// region was invisible while --kbdump was hardcoded to a 0x400 window.

// Column-major: the dump walks a column top to bottom, then the next column.
inline int SlotIndex(int col, int row) { return col * MATRIX_ROWS + row; }

inline bool SlotValid(int col, int row) {
    return col >= 0 && col < MATRIX_COLS && row >= 0 && row < MATRIX_ROWS;
}

inline uint16_t RemapOffset(int col, int row) {
    return (uint16_t)(REMAP_BASE + col * REMAP_COL_STRIDE + row * 3);
}

inline uint16_t KeyColorOffset(int col, int row) {
    return (uint16_t)(KEYCOLOR_BASE + SlotIndex(col, row) * KEYCOLOR_STRIDE);
}

// True for an offset that is the FIRST byte of a colour triple inside the
// MEASURED region. A write that starts mid-triple would shift every following
// key's colour by one channel, so the alignment is checked rather than assumed.
//
// Bounded by the measured end, not the inferred one, so the two trailing
// triples that no matrix slot claims stay reachable for --keyidentify. They are
// where the assumption breaks; refusing to probe them would protect the
// assumption instead of testing it.
inline bool IsKeyColorOffset(uint16_t off) {
    if (off < KEYCOLOR_BASE || off >= KEYCOLOR_REGION_END) return false;
    return ((off - KEYCOLOR_BASE) % KEYCOLOR_STRIDE) == 0;
}

// Maps an offset back to a matrix position, and fails for the trailing triples
// that lie past the matrix. "No slot" is the honest answer there - inventing a
// 127th column would make the probe report a prediction nobody derived.
inline bool KeyColorOffsetToSlot(uint16_t off, int& col, int& row) {
    col = row = -1;
    if (!IsKeyColorOffset(off)) return false;
    if (off >= KEYCOLOR_SLOTS_END) return false;
    const int slot = (off - KEYCOLOR_BASE) / KEYCOLOR_STRIDE;
    col = slot / MATRIX_ROWS;
    row = slot % MATRIX_ROWS;
    return true;
}

//-----------------------------------------------------------------------------
// Entry decoding (docs section 4)
//
//   0x20 <modifier bitmask> <HID usage>   standard key
//   0x30 <consumer usage, little endian>  media key
//   0xA0 <layer index> 00                 Fn / layer key
//   20 00 00                              unassigned matrix position
//-----------------------------------------------------------------------------

static const uint8_t ENTRY_STANDARD = 0x20;
static const uint8_t ENTRY_CONSUMER = 0x30;
static const uint8_t ENTRY_LAYER    = 0xA0;

// HID keyboard usage page (0x07) names, ASCII only. This file is compiled
// without /utf-8 and has no BOM, so non-ASCII here would reach the UI as
// mojibake - the same trap the probe dialogs are commented for.
inline const char* HidUsageName(uint8_t usage) {
    if (usage >= 0x04 && usage <= 0x1D) {
        static const char* letters[] = {
            "A","B","C","D","E","F","G","H","I","J","K","L","M",
            "N","O","P","Q","R","S","T","U","V","W","X","Y","Z"
        };
        return letters[usage - 0x04];
    }
    switch (usage) {
        case 0x1E: return "1";      case 0x1F: return "2";      case 0x20: return "3";
        case 0x21: return "4";      case 0x22: return "5";      case 0x23: return "6";
        case 0x24: return "7";      case 0x25: return "8";      case 0x26: return "9";
        case 0x27: return "0";
        case 0x28: return "Enter";  case 0x29: return "Esc";    case 0x2A: return "Backspace";
        case 0x2B: return "Tab";    case 0x2C: return "Space";  case 0x2D: return "-";
        case 0x2E: return "=";      case 0x2F: return "[";      case 0x30: return "]";
        case 0x31: return "\\";     case 0x32: return "#";      case 0x33: return ";";
        case 0x34: return "'";      case 0x35: return "`";      case 0x36: return ",";
        case 0x37: return ".";      case 0x38: return "/";      case 0x39: return "Caps";
        case 0x3A: return "F1";     case 0x3B: return "F2";     case 0x3C: return "F3";
        case 0x3D: return "F4";     case 0x3E: return "F5";     case 0x3F: return "F6";
        case 0x40: return "F7";     case 0x41: return "F8";     case 0x42: return "F9";
        case 0x43: return "F10";    case 0x44: return "F11";    case 0x45: return "F12";
        case 0x46: return "PrtSc";  case 0x47: return "ScrLk";  case 0x48: return "Pause";
        case 0x49: return "Ins";    case 0x4A: return "Home";   case 0x4B: return "PgUp";
        case 0x4C: return "Del";    case 0x4D: return "End";    case 0x4E: return "PgDn";
        case 0x4F: return "Right";  case 0x50: return "Left";   case 0x51: return "Down";
        case 0x52: return "Up";     case 0x53: return "NumLk";  case 0x54: return "KP/";
        case 0x55: return "KP*";    case 0x56: return "KP-";    case 0x57: return "KP+";
        case 0x58: return "KPEnter";
        case 0x59: return "KP1";    case 0x5A: return "KP2";    case 0x5B: return "KP3";
        case 0x5C: return "KP4";    case 0x5D: return "KP5";    case 0x5E: return "KP6";
        case 0x5F: return "KP7";    case 0x60: return "KP8";    case 0x61: return "KP9";
        case 0x62: return "KP0";    case 0x63: return "KP.";    case 0x64: return "NonUS\\";
        case 0x65: return "Menu";   case 0x66: return "Power";  case 0x67: return "KP=";
        default:   return 0;
    }
}

// HID modifier bitmask (docs section 4: 01 LCtrl ... 80 RGUI). Only a single
// bit is a named modifier key; a combination is a macro, not a key cap.
inline const char* HidModifierName(uint8_t mask) {
    switch (mask) {
        case 0x01: return "LCtrl";  case 0x02: return "LShift";
        case 0x04: return "LAlt";   case 0x08: return "LWin";
        case 0x10: return "RCtrl";  case 0x20: return "RShift";
        case 0x40: return "RAlt";   case 0x80: return "RWin";
        default:   return 0;
    }
}

// Consumer page (0x0C) usages seen in this device's table. Anything else is
// printed as its raw usage rather than guessed at.
inline const char* ConsumerUsageName(uint16_t usage) {
    switch (usage) {
        case 0x00B5: return "Next";     case 0x00B6: return "Prev";
        case 0x00B7: return "Stop";     case 0x00CD: return "Play";
        case 0x00E2: return "Mute";     case 0x00E9: return "Vol+";
        case 0x00EA: return "Vol-";     case 0x018A: return "Mail";
        case 0x0192: return "Calc";     case 0x0194: return "MyPC";
        case 0x0196: return "Web";      case 0x019E: return "Lock";
        // 0x0223 is the browser's home page. Deliberately not called "Home":
        // HID usage 0x4A above already is, and two keys with the same cap in a
        // layout the user clicks on is a way to colour the wrong one.
        case 0x0221: return "Search";   case 0x0223: return "WWW";
        default:     return 0;
    }
}

struct KeyEntry {
    bool        assigned = false;   // false for the "20 00 00" hole
    uint8_t     raw[3]   = {0, 0, 0};
    std::string label;              // ASCII, may be empty for an unknown code
};

inline KeyEntry DecodeEntry(uint8_t a, uint8_t b, uint8_t c) {
    KeyEntry e;
    e.raw[0] = a; e.raw[1] = b; e.raw[2] = c;

    // The documented hole marker, and the all-zero padding past the end of the
    // table. Both mean "no key here"; neither may become a drawable cell.
    if ((a == ENTRY_STANDARD && b == 0 && c == 0) || (a == 0 && b == 0 && c == 0))
        return e;

    char buf[24];
    if (a == ENTRY_STANDARD) {
        if (b != 0) {
            const char* m = HidModifierName(b);
            if (m) { e.assigned = true; e.label = m; return e; }
            snprintf(buf, sizeof(buf), "mod%02X", (unsigned)b);
            e.assigned = true; e.label = buf; return e;
        }
        const char* n = HidUsageName(c);
        if (n) { e.assigned = true; e.label = n; return e; }
        snprintf(buf, sizeof(buf), "u%02X", (unsigned)c);
        e.assigned = true; e.label = buf; return e;
    }
    if (a == ENTRY_CONSUMER) {
        const uint16_t usage = (uint16_t)(b | (c << 8));
        const char* n = ConsumerUsageName(usage);
        if (n) { e.assigned = true; e.label = n; return e; }
        snprintf(buf, sizeof(buf), "c%04X", (unsigned)usage);
        e.assigned = true; e.label = buf; return e;
    }
    if (a == ENTRY_LAYER) {
        snprintf(buf, sizeof(buf), "Fn%u", (unsigned)b);
        e.assigned = true; e.label = buf; return e;
    }

    // An unknown prefix is reported as itself. Guessing at it would put a wrong
    // cap on a key that a write path then addresses by that name.
    snprintf(buf, sizeof(buf), "?%02X%02X%02X", (unsigned)a, (unsigned)b, (unsigned)c);
    e.assigned = true; e.label = buf;
    return e;
}

//-----------------------------------------------------------------------------
// The model the UI and the write path share
//-----------------------------------------------------------------------------

struct Key {
    int      col = 0, row = 0;
    uint16_t remapOffset = 0;
    uint16_t colorOffset = 0;      // where this key's triple would live
    bool     assigned    = false;  // false = empty matrix position
    std::string label;
    uint8_t  r = 0, g = 0, b = 0;  // last value read back from the device
    bool     colorKnown  = false;  // false until a colour was actually read
};

// Builds the layout from the remap block. `remap` points at the byte that lives
// at REMAP_BASE and must hold at least REMAP_END-REMAP_BASE bytes; anything
// shorter yields the keys that are covered and stops - a partial read produces a
// partial layout, never invented entries.
//
// Only assigned positions are returned. The holes in the matrix are real (the
// device marks them "20 00 00"), and a UI that drew them would offer the user
// keys that do not exist.
inline std::vector<Key> BuildLayout(const uint8_t* remap, size_t remapLen) {
    std::vector<Key> keys;
    if (!remap) return keys;

    for (int col = 0; col < MATRIX_COLS; col++) {
        for (int row = 0; row < MATRIX_ROWS; row++) {
            const uint16_t off = RemapOffset(col, row);
            const size_t   idx = (size_t)(off - REMAP_BASE);
            if (idx + 2 >= remapLen) return keys;

            const KeyEntry e = DecodeEntry(remap[idx], remap[idx + 1], remap[idx + 2]);
            if (!e.assigned) continue;

            Key k;
            k.col = col; k.row = row;
            k.remapOffset = off;
            k.colorOffset = KeyColorOffset(col, row);
            k.assigned    = true;
            k.label       = e.label;
            keys.push_back(k);
        }
    }
    return keys;
}

// Sets every key to one colour and marks it known.
//
// This is how the editor starts: with the colour the keyboard is ACTUALLY
// showing, read from the profile block at `profile_base+0x06..0x08` - an
// offset section 3 has at [HIGH] and whose rendering is confirmed (a static
// colour change is visible on the keyboard).
//
// What it deliberately does NOT do is read the per-key table at 0x2C0 and paint
// that. There used to be an ApplyColorTable() here doing exactly that, and the
// result was a board covered in reds, greens and blues that nobody had ever
// set. Those colours were an artefact: from 0x2F0 the region repeats with a
// 16-byte period, so reading it at a 3-byte stride drifts through the pattern
// and hands every few keys a different triple. It LOOKED like per-key state and
// was a picture of an assumption (rule 1). Nothing may treat those bytes as
// colours until 5.5 item 1 has measured that they are - at which point painting
// them is five lines, against a measurement.
inline void SeedKeyColors(std::vector<Key>& keys, uint8_t r, uint8_t g, uint8_t b) {
    for (Key& k : keys) {
        k.r = r; k.g = g; k.b = b;
        k.colorKnown = true;
    }
}

// Stable id for persistence in RGBConfig::keyboardZones. Keyed by matrix
// position, not by label: a remapped key keeps its place in the matrix, and its
// colour should stay with the place rather than follow the letter around.
inline std::string KeyId(int col, int row) {
    char buf[24];
    snprintf(buf, sizeof(buf), "key_c%02dr%d", col, row);
    return buf;
}

//=============================================================================
// DRAWING GEOMETRY - how wide a key is on screen
//
// Read this boundary carefully, because it is the one place in this header
// where something is NOT read from the device:
//
//   * WHICH keys exist, what they are called and which colour offset they own
//     comes from the device's remap table. That stays true above.
//   * How wide the Space bar is DRAWN cannot come from the device. Key sizes
//     and cluster spacing are simply not in the config memory - the matrix says
//     "column 6, row 5", not "6.25 units wide". So the shape below is derived
//     from the key's own label, and a label this table does not know gets a
//     plain 1-unit cell.
//
// That is not the "hardcoded keyboard map" the plan rules out. A hardcoded map
// would decide which keys the board has; this decides how many pixels a key the
// board already reported gets. Get it wrong and a cap is the wrong width - not
// a write to the wrong key.
//
// Units: 1.0 = one standard key. A full-size board is 22.5 x 6.35 units.
//=============================================================================

struct KeyBox {
    float x = 0, y = 0, w = 1, h = 1;   // in key units
};

// Which block of the board a key belongs to. Drives x positioning: the main
// block accumulates left to right (that is what produces the row stagger),
// while the nav and numpad blocks sit at fixed columns so their keys line up
// vertically - including the lone Up arrow, which has a hole either side of it.
enum KeyCluster { CLUSTER_MAIN = 0, CLUSTER_NAV = 1, CLUSTER_NUMPAD = 2 };

inline bool LabelIs(const std::string& s, const char* lit) { return s == lit; }

inline KeyCluster ClusterOfLabel(const std::string& label) {
    // Numpad: every KP* plus NumLock.
    if (label.size() >= 2 && label[0] == 'K' && label[1] == 'P') return CLUSTER_NUMPAD;
    if (LabelIs(label, "NumLk")) return CLUSTER_NUMPAD;

    // Media keys sit above the numpad on this board, so they share its column
    // origin. They are consumer-page entries, i.e. anything DecodeEntry got
    // from the 0x30 prefix.
    static const char* media[] = { "Calc", "Mute", "Vol+", "Vol-", "Play", "Next",
                                   "Prev", "Stop", "Mail", "WWW", "Lock", "Search" };
    for (const char* m : media) if (LabelIs(label, m)) return CLUSTER_NUMPAD;

    static const char* nav[] = { "PrtSc", "ScrLk", "Pause", "Ins", "Home", "PgUp",
                                 "Del", "End", "PgDn", "Up", "Down", "Left", "Right" };
    for (const char* n : nav) if (LabelIs(label, n)) return CLUSTER_NAV;

    return CLUSTER_MAIN;   // the fallback: an unknown key is a normal key
}

// Width in units. The values are the standard ISO full-size ones, and they are
// chosen so every main-block row adds up to exactly 15.0 - which is what makes
// the rows line up at the right edge instead of drifting apart.
inline float KeyWidthUnits(const std::string& label) {
    if (LabelIs(label, "Backspace")) return 2.00f;
    if (LabelIs(label, "Tab"))       return 1.50f;
    if (LabelIs(label, "\\"))        return 1.50f;
    if (LabelIs(label, "Caps"))      return 1.75f;
    if (LabelIs(label, "Enter"))     return 1.25f;   // ISO: the tall one is drawn flat
    if (LabelIs(label, "LShift"))    return 1.25f;   // ISO short shift (NonUS\ follows)
    if (LabelIs(label, "RShift"))    return 2.75f;
    if (LabelIs(label, "Space"))     return 6.25f;
    if (LabelIs(label, "LCtrl") || LabelIs(label, "LWin") || LabelIs(label, "LAlt") ||
        LabelIs(label, "RCtrl") || LabelIs(label, "RWin") || LabelIs(label, "RAlt") ||
        LabelIs(label, "Menu"))      return 1.25f;
    if (label.size() >= 2 && label[0] == 'F' && label[1] == 'n') return 1.25f;
    if (LabelIs(label, "KP0"))       return 2.00f;
    return 1.00f;
}

// Short cap text for the keys whose name does not fit a one-unit cap. Purely
// cosmetic and used only while drawing: the model, the status line, the reports
// and every lookup keep the full label, so shortening one here cannot change
// which key a cluster rule or a probe is talking about.
inline const char* ShortLabel(const std::string& label) {
    if (label == "NumLk")   return "Num";
    if (label == "KPEnter") return "Ent";
    if (label == "NonUS\\") return "ISO\\";
    if (label == "Down")    return "Dn";
    if (label == "Right")   return "Rt";
    if (label == "Left")    return "Lt";
    return label.c_str();
}

// Where each block starts, and the gap under the function row.
static const float BOARD_NAV_X    = 15.25f;
static const float BOARD_NUMPAD_X = 18.50f;
static const float BOARD_FROW_GAP = 0.35f;
static const float BOARD_WIDTH    = 22.50f;
static const float BOARD_HEIGHT   = 6.35f;

// Lays the keys out as a board. One box per key, same order as `keys`.
//
// Double height is decided from the DEVICE data, not from the label: a numpad
// key with a hole directly below it is a key that occupies both rows - which is
// exactly what KP+ and KPEnter are. Restricted to the numpad on purpose, because
// the holes around the space bar would otherwise stretch the comma key.
inline std::vector<KeyBox> ComputeKeyBoxes(const std::vector<Key>& keys) {
    std::vector<KeyBox> boxes(keys.size());

    // Fast lookup for "is the slot below this one a hole?"
    bool occupied[MATRIX_COLS][MATRIX_ROWS] = {};
    for (const Key& k : keys)
        if (SlotValid(k.col, k.row)) occupied[k.col][k.row] = true;

    // The main block is laid out per row by accumulating widths.
    float penX[MATRIX_ROWS];
    for (int r = 0; r < MATRIX_ROWS; r++) penX[r] = 0.0f;

    for (size_t i = 0; i < keys.size(); i++) {
        const Key& k = keys[i];
        KeyBox b;
        b.w = KeyWidthUnits(k.label);
        b.h = 1.0f;
        b.y = (k.row == 0) ? 0.0f : (float)k.row + BOARD_FROW_GAP;

        const KeyCluster cl = ClusterOfLabel(k.label);
        if (cl == CLUSTER_MAIN) {
            // The function row's two gaps, the ones that group F1-F4, F5-F8 and
            // F9-F12. Keyed off the labels because the matrix has no hole there.
            if (k.row == 0) {
                if (LabelIs(k.label, "F1")) penX[0] += 1.0f;
                else if (LabelIs(k.label, "F5") || LabelIs(k.label, "F9")) penX[0] += 0.5f;
            }
            b.x = penX[k.row];
            penX[k.row] += b.w;
        } else if (cl == CLUSTER_NAV) {
            // Fixed by matrix column, so Up lands above Down with a gap either
            // side instead of being pushed left by the missing keys.
            b.x = BOARD_NAV_X + (float)(k.col - 14);
        } else {
            b.x = BOARD_NUMPAD_X + (float)(k.col - 17);
            // KP0 is two units wide and starts one column further left than its
            // matrix position suggests - the matrix records it once, the board
            // gives it the width of two.
            if (LabelIs(k.label, "KP0")) b.x -= 1.0f;

            if (k.row + 1 < MATRIX_ROWS && !occupied[k.col][k.row + 1]) b.h = 2.0f;
        }

        boxes[i] = b;
    }
    return boxes;
}

} // namespace kblayout
