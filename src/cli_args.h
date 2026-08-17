#pragma once

//=============================================================================
// Token-exact command-line flag matching.
//
// Everything in this header is pure: no Windows headers, no globals, no I/O.
// That is deliberate. It is the part of the probe plumbing that decides *which*
// hardware write happens, and it is the only part that can be exercised without
// a keyboard on the USB bus - see tests/test_cli_args.cpp.
//
// Why not strstr(lpCmdLine, "--flag"):
//
//  1. Substring matching hits things that are not the flag. The existing device
//     branches use strstr over the whole command line, so anything that merely
//     starts with the flag name triggers it: "--kbmode-sweep5" (the '=' left
//     out), "--kbmode-sweeper" (tail mistyped), or the flag name appearing inside
//     some other flag's quoted value all start a 21-step write sweep over the
//     keyboard's flash, at the default hold time, without a word about it.
//  2. It cannot distinguish "flag present, no value" from "flag absent", so a
//     pair like --edgemode= / --edgemode-sweep is only ever kept apart by
//     accident of spelling - here they happen not to collide, but nothing in the
//     matching says so. Find() returns both facts separately, and callers use
//     CountPresent() to refuse combinations instead of letting one win: the
//     kbmode block tested the sweep first, so "--kbmode=3 --kbmode-sweep" ran
//     the sweep and never mentioned the ignored flag.
//  3. strtol() on the value accepted anything: "--kbmode=banana" parsed as 0 and
//     probed mode 0x00 while reporting the user's request. ParseByte() refuses.
//
// A flag mentioned twice resolves to its first occurrence. Callers that must not
// be combined use CountPresent() and abort instead of silently letting one win.
//=============================================================================

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace cli {

// One parsed flag. `present` without `hasValue` is "--foo"; with `hasValue` it
// is "--foo=<value>", where value may legitimately be the empty string ("--foo=").
struct Flag {
    bool        present  = false;
    bool        hasValue = false;
    std::string value;
};

// Splits a command line on whitespace, honouring double quotes so that a
// quoted path containing spaces stays one token. The quote characters
// themselves are dropped, matching how the shell hands arguments over.
inline std::vector<std::string> Tokenize(const char* cmdline) {
    std::vector<std::string> out;
    if (!cmdline) return out;

    std::string cur;
    bool inQuotes = false;
    bool started  = false;   // distinguishes "" (an empty token) from no token

    for (const char* p = cmdline; *p; ++p) {
        const char c = *p;
        if (c == '"') { inQuotes = !inQuotes; started = true; continue; }
        if (!inQuotes && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
            if (started) { out.push_back(cur); cur.clear(); started = false; }
            continue;
        }
        cur.push_back(c);
        started = true;
    }
    if (started) out.push_back(cur);
    return out;
}

// Looks for `name` as a whole token, either bare or as "name=value".
// `name` is spelled without the '=' (e.g. "--edgemode").
inline Flag Find(const char* cmdline, const char* name) {
    Flag f;
    if (!name || !*name) return f;
    const std::string n(name);

    for (const std::string& t : Tokenize(cmdline)) {
        if (t == n) {
            f.present  = true;
            f.hasValue = false;
            return f;
        }
        if (t.size() > n.size() && t.compare(0, n.size(), n) == 0 && t[n.size()] == '=') {
            f.present  = true;
            f.hasValue = true;
            f.value    = t.substr(n.size() + 1);
            return f;
        }
    }
    return f;
}

// How many of the given flags are present. Used to reject mutually exclusive
// probe actions instead of picking a winner behind the user's back.
inline int CountPresent(std::initializer_list<const Flag*> flags) {
    int n = 0;
    for (const Flag* f : flags) if (f && f->present) n++;
    return n;
}

// Strict unsigned parse, decimal or 0x-hex. The *whole* string must be digits:
// no leading '+', no trailing junk, no empty string, no bare "0x". Values above
// 24 bits are rejected - every caller here wants a byte or a second count, and a
// number that large is a typo, not an intent.
inline bool ParseUInt(const std::string& s, unsigned long& out) {
    if (s.empty()) return false;

    int    base = 10;
    size_t i    = 0;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; i = 2; }

    unsigned long v = 0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        int d;
        if (c >= '0' && c <= '9')                        d = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f')     d = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F')     d = c - 'A' + 10;
        else return false;

        v = v * (unsigned long)base + (unsigned long)d;
        if (v > 0xFFFFFFuL) return false;
    }
    out = v;
    return true;
}

// Byte value for a mode argument. Rejects anything that is not a clean 0..255.
inline bool ParseByte(const std::string& s, uint8_t& out) {
    unsigned long v = 0;
    if (!ParseUInt(s, v) || v > 0xFF) return false;
    out = (uint8_t)v;
    return true;
}

// Hold time for the sweep flags: "--flag" -> defSec, "--flag=<n>" -> n clamped
// into [minSec,maxSec]. A value that does not parse falls back to defSec rather
// than to 0 - a sweep with a 0 s hold shows the user nothing.
inline int HoldSeconds(const Flag& f, int defSec, int minSec, int maxSec) {
    if (!f.present || !f.hasValue) return defSec;
    unsigned long v = 0;
    if (!ParseUInt(f.value, v)) return defSec;
    const long s = (long)v;
    if (s < (long)minSec) return minSec;
    if (s > (long)maxSec) return maxSec;
    return (int)s;
}

// 16-bit config-memory offset. Separate from ParseByte because the address
// space of the on-board config memory is wider than a byte - the per-key colour
// table starts at 0x2C0 and, going by the matrix size, does not end inside the
// 0x400 window the old --kbdump was hardcoded to.
inline bool ParseOffset16(const std::string& s, uint16_t& out) {
    unsigned long v = 0;
    if (!ParseUInt(s, v) || v > 0xFFFFuL) return false;
    out = (uint16_t)v;
    return true;
}

// "<lo>-<hi>", both ends inclusive, both 16-bit, lo <= hi. Used by
// --kbdump-range. Deliberately strict in the ways a range argument goes wrong:
// a missing end ("0x300-"), a reversed pair ("0x300-0x200") and a bare single
// value are all rejected rather than repaired, because each of them would make
// the dump cover something other than what was asked for - and a dump is the
// reference state every collateral check is compared against.
inline bool ParseRange16(const std::string& s, uint16_t& lo, uint16_t& hi) {
    const size_t dash = s.find('-');
    if (dash == std::string::npos || dash == 0 || dash + 1 >= s.size()) return false;
    // A second '-' means something like "1-2-3": not a range.
    if (s.find('-', dash + 1) != std::string::npos) return false;

    uint16_t a = 0, b = 0;
    if (!ParseOffset16(s.substr(0, dash), a)) return false;
    if (!ParseOffset16(s.substr(dash + 1), b)) return false;
    if (b < a) return false;
    lo = a; hi = b;
    return true;
}

// Comma-separated list of byte values ("0x00,0x04,9"). Every element must parse
// or the whole list is refused: a list where one entry silently vanished would
// make a --kbmode-only walk skip a candidate while the report still names it.
// An empty list, a trailing comma and duplicates-only-garbage all fail.
inline bool ParseByteList(const std::string& s, std::vector<uint8_t>& out,
                          size_t maxItems = 256) {
    out.clear();
    if (s.empty()) return false;

    size_t start = 0;
    while (true) {
        const size_t comma = s.find(',', start);
        const std::string item = s.substr(start, comma == std::string::npos
                                                     ? std::string::npos
                                                     : comma - start);
        uint8_t v = 0;
        if (!ParseByte(item, v)) { out.clear(); return false; }
        if (out.size() >= maxItems)  { out.clear(); return false; }
        out.push_back(v);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return true;
}

// The same for 16-bit offsets ("0x2C0,0x2C3,0x2F0"), used by --keyidentify.
inline bool ParseOffsetList(const std::string& s, std::vector<uint16_t>& out,
                            size_t maxItems = 256) {
    out.clear();
    if (s.empty()) return false;

    size_t start = 0;
    while (true) {
        const size_t comma = s.find(',', start);
        const std::string item = s.substr(start, comma == std::string::npos
                                                     ? std::string::npos
                                                     : comma - start);
        uint16_t v = 0;
        if (!ParseOffset16(item, v)) { out.clear(); return false; }
        if (out.size() >= maxItems)  { out.clear(); return false; }
        out.push_back(v);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return true;
}

// "RRGGBB", exactly six hex digits, no 0x prefix and no '#'. Used by --kbcolor
// to make a differential dump possible without a script editing the user's
// config.json behind their back: the colour table at 0x2C0 can only be found by
// writing colour A, dumping, writing colour B and dumping again, and the two
// colours have to differ or the diff is empty by construction.
inline bool ParseRgb(const std::string& s, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (s.size() != 6) return false;
    unsigned v[6];
    for (size_t i = 0; i < 6; i++) {
        const char c = s[i];
        if      (c >= '0' && c <= '9') v[i] = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v[i] = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v[i] = (unsigned)(c - 'A' + 10);
        else return false;
    }
    r = (uint8_t)((v[0] << 4) | v[1]);
    g = (uint8_t)((v[2] << 4) | v[3]);
    b = (uint8_t)((v[4] << 4) | v[5]);
    return true;
}

// Which question a --confirm dialog asks. The sweeps used to have exactly one
// wording ("does anything move?"), which is the right question for a mode sweep
// looking for an animation and the wrong one for every other probe: a run
// looking for the per-key colour mode needs "are different keys different
// colours?", and an answer to a question nobody meant to ask is worse than no
// answer at all (rule 1). Default stays MOTION, so existing command lines
// behave exactly as before.
enum AskKind {
    ASK_MOTION = 0,
    ASK_LIT    = 1,
    ASK_PERKEY = 2
};

inline bool ParseAsk(const std::string& s, AskKind& out) {
    if (s == "motion") { out = ASK_MOTION; return true; }
    if (s == "lit")    { out = ASK_LIT;    return true; }
    if (s == "perkey") { out = ASK_PERKEY; return true; }
    return false;
}

// Resolves the flag; a missing flag is the default, a malformed one is an error
// the caller has to refuse rather than paper over with the default.
inline bool ResolveAsk(const Flag& f, AskKind& out) {
    out = ASK_MOTION;
    if (!f.present) return true;
    if (!f.hasValue) return false;
    return ParseAsk(f.value, out);
}

} // namespace cli
