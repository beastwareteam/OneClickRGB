// Unit tests for src/cli_args.h, src/effect_limits.h and src/channel_config.h.
//
// These three headers hold the logic that decides *which* bytes reach the
// hardware: which probe flag was meant, what value it carries, what range a
// mode/brightness/speed value is allowed to have, and which colour a channel
// gets when it has an override of its own. That logic is pure, so
// unlike everything else in this project it can be tested without a GK650 on the
// USB bus - and per CLAUDE.md rule 1 the tests assert real behaviour, never a
// value copied out of an old dump.
//
// What is deliberately NOT tested here: whether the firmware renders a mode.
// No unit test can know that; it needs --edgemode-sweep and a human watching the
// strip. The tests below pin down the parts that were getting that decision
// wrong before the light was ever consulted.
//
// Build and run: tests\run_tests.cmd

#include <cstdio>
#include <cstring>

#include "cli_args.h"
#include "effect_limits.h"
#include "channel_config.h"
#include "keyboard_layout.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        g_checks++;                                                       \
        if (!(cond)) {                                                    \
            g_failures++;                                                 \
            printf("  FAIL  line %-4d  %s\n", __LINE__, #cond);           \
        }                                                                 \
    } while (0)

#define CHECK_EQ(actual, expected)                                        \
    do {                                                                  \
        g_checks++;                                                       \
        const long _a = (long)(actual);                                   \
        const long _e = (long)(expected);                                 \
        if (_a != _e) {                                                   \
            g_failures++;                                                 \
            printf("  FAIL  line %-4d  %s: want %ld, got %ld\n",          \
                   __LINE__, #actual, _e, _a);                            \
        }                                                                 \
    } while (0)

#define CHECK_STR(actual, expected)                                       \
    do {                                                                  \
        g_checks++;                                                       \
        if (std::string(actual) != std::string(expected)) {               \
            g_failures++;                                                 \
            printf("  FAIL  line %-4d  %s: want \"%s\", got \"%s\"\n",    \
                   __LINE__, #actual, expected,                           \
                   std::string(actual).c_str());                          \
        }                                                                 \
    } while (0)

static void group(const char* name) { printf("\n%s\n", name); }

//-----------------------------------------------------------------------------
static void TestTokenize() {
    group("cli::Tokenize");

    CHECK_EQ(cli::Tokenize(nullptr).size(), 0);
    CHECK_EQ(cli::Tokenize("").size(), 0);
    CHECK_EQ(cli::Tokenize("   \t  ").size(), 0);
    CHECK_EQ(cli::Tokenize("--a --b").size(), 2);
    CHECK_EQ(cli::Tokenize("  --a\t\t--b   ").size(), 2);

    // A quoted value stays one token; the quotes themselves are dropped, the way
    // the shell hands arguments over.
    std::vector<std::string> t = cli::Tokenize("--path=\"c:\\a b\\x\" --dry-run");
    CHECK_EQ(t.size(), 2);
    if (t.size() == 2) {
        CHECK_STR(t[0], "--path=c:\\a b\\x");
        CHECK_STR(t[1], "--dry-run");
    }
}

//-----------------------------------------------------------------------------
static void TestFindExactToken() {
    group("cli::Find - whole-token matching");

    cli::Flag f = cli::Find("--edgemode=3", "--edgemode");
    CHECK(f.present);
    CHECK(f.hasValue);
    CHECK_STR(f.value, "3");

    f = cli::Find("--edgemode", "--edgemode");
    CHECK(f.present);
    CHECK(!f.hasValue);

    // "--flag=" is present with an empty value, not absent. The caller can then
    // say "needs a value" instead of silently probing mode 0.
    f = cli::Find("--edgemode=", "--edgemode");
    CHECK(f.present);
    CHECK(f.hasValue);
    CHECK_STR(f.value, "");

    CHECK(!cli::Find(nullptr, "--edgemode").present);
    CHECK(!cli::Find("--dry-run", "--edgemode").present);
    CHECK(!cli::Find("--edgemode", "").present);

    // First occurrence wins, deterministically.
    f = cli::Find("--edgemode=1 --edgemode=2", "--edgemode");
    CHECK_STR(f.value, "1");

    // Order on the command line is irrelevant.
    f = cli::Find("--dry-run --foreground --edgemode-sweep=7", "--edgemode-sweep");
    CHECK(f.present);
    CHECK_STR(f.value, "7");
}

//-----------------------------------------------------------------------------
static void TestFindNoSubstringBleed() {
    group("cli::Find - the bugs strstr had");

    // 1) A token that merely *starts* with the flag must not trigger it. This is
    //    the real strstr hazard, and the first CHECK of each pair records that
    //    strstr does match - so these lines document a behaviour difference and
    //    not a hypothetical one. Forgetting the '=' ("--kbmode-sweep5") or
    //    mistyping the tail ("--kbmode-sweeper") used to start a 21-step sweep of
    //    writes into the keyboard's flash, with the default hold time, silently.
    CHECK(strstr("--kbmode-sweep5", "--kbmode-sweep") != nullptr);
    CHECK(!cli::Find("--kbmode-sweep5", "--kbmode-sweep").present);

    CHECK(strstr("--kbmode-sweeper", "--kbmode-sweep") != nullptr);
    CHECK(!cli::Find("--kbmode-sweeper", "--kbmode-sweep").present);

    CHECK(strstr("--edgemode-sweep-off", "--edgemode-sweep") != nullptr);
    CHECK(!cli::Find("--edgemode-sweep-off", "--edgemode-sweep").present);

    // An occurrence inside another token - a quoted path, a value handed to some
    // other flag - is text, not a request to write to hardware.
    CHECK(strstr("--log=\"c:\\tmp\\--edgemode-sweep.txt\"", "--edgemode-sweep") != nullptr);
    CHECK(!cli::Find("--log=\"c:\\tmp\\--edgemode-sweep.txt\"", "--edgemode-sweep").present);

    // A leading negation does not reach strstr's needle here (only one dash ends
    // up in front of the flag name), but it must stay absent either way.
    CHECK(!cli::Find("--no-kbmode-sweep", "--kbmode-sweep").present);
    CHECK(!cli::Find("--skip-edgemode-sweep", "--edgemode-sweep").present);
    CHECK(!cli::Find("xx--edgemode-sweep", "--edgemode-sweep").present);

    // 2) The pair --edgemode / --edgemode-sweep must not catch each other in
    //    either direction. Substring matching keeps them apart only by accident
    //    of spelling; token matching does it by construction.
    CHECK(!cli::Find("--edgemode-sweep=5", "--edgemode").present);
    CHECK(!cli::Find("--edgemode-sweep", "--edgemode").present);
    CHECK(!cli::Find("--edgemode=3", "--edgemode-sweep").present);
    CHECK(!cli::Find("--edgespeed-sweep=4", "--edgemode").present);
    CHECK(!cli::Find("--edgespeed-sweep=4", "--edgemode-sweep").present);
    CHECK(!cli::Find("--edgespeed-mode=3", "--edgespeed-sweep").present);
    CHECK(!cli::Find("--kbmode-sweep", "--kbmode").present);

    // 3) A longer flag whose name starts with a shorter one is still findable.
    CHECK(cli::Find("--edgespeed-sweep=4", "--edgespeed-sweep").present);
    CHECK(cli::Find("--edgemode-sweep --dry-run", "--edgemode-sweep").present);
}

//-----------------------------------------------------------------------------
static void TestCountPresent() {
    group("cli::CountPresent - mutually exclusive actions");

    const cli::Flag a = cli::Find("--edgemode=3", "--edgemode");
    const cli::Flag b = cli::Find("--edgemode=3", "--edgemode-sweep");
    const cli::Flag c = cli::Find("--edgemode=3", "--edgespeed-sweep");
    CHECK_EQ(cli::CountPresent({&a, &b, &c}), 1);

    const char* both = "--edgemode=3 --edgemode-sweep=5";
    const cli::Flag d = cli::Find(both, "--edgemode");
    const cli::Flag e = cli::Find(both, "--edgemode-sweep");
    CHECK_EQ(cli::CountPresent({&d, &e}), 2);   // -> caller must refuse, not rank

    const cli::Flag none = cli::Find("--dry-run", "--edgemode");
    CHECK_EQ(cli::CountPresent({&none}), 0);
    CHECK_EQ(cli::CountPresent({nullptr, &none}), 0);
}

//-----------------------------------------------------------------------------
static void TestParseByte() {
    group("cli::ParseByte - strict, unlike strtol");

    uint8_t v = 0xAA;
    CHECK(cli::ParseByte("0", v));   CHECK_EQ(v, 0);
    CHECK(cli::ParseByte("3", v));   CHECK_EQ(v, 3);
    CHECK(cli::ParseByte("255", v)); CHECK_EQ(v, 255);
    CHECK(cli::ParseByte("0x0A", v)); CHECK_EQ(v, 10);
    CHECK(cli::ParseByte("0X1e", v)); CHECK_EQ(v, 0x1E);
    CHECK(cli::ParseByte("0xff", v)); CHECK_EQ(v, 255);

    // Everything below used to be accepted as 0 by strtol(..., nullptr, 0),
    // so "--kbmode=banana" wrote mode 0x00 and the report claimed the request.
    CHECK(!cli::ParseByte("banana", v));
    CHECK(!cli::ParseByte("5abc", v));
    CHECK(!cli::ParseByte("", v));
    CHECK(!cli::ParseByte("0x", v));
    CHECK(!cli::ParseByte(" 5", v));
    CHECK(!cli::ParseByte("5 ", v));
    CHECK(!cli::ParseByte("-1", v));
    CHECK(!cli::ParseByte("+5", v));
    CHECK(!cli::ParseByte("256", v));
    CHECK(!cli::ParseByte("0x100", v));

    unsigned long u = 0;
    CHECK(cli::ParseUInt("16777215", u)); CHECK_EQ(u, 0xFFFFFF);
    CHECK(!cli::ParseUInt("16777216", u));   // sanity bound, not a byte cap
}

//-----------------------------------------------------------------------------
static void TestHoldSeconds() {
    group("cli::HoldSeconds - sweep hold time");

    CHECK_EQ(cli::HoldSeconds(cli::Find("--dry-run", "--edgemode-sweep"), 5, 1, 60), 5);
    CHECK_EQ(cli::HoldSeconds(cli::Find("--edgemode-sweep", "--edgemode-sweep"), 5, 1, 60), 5);
    CHECK_EQ(cli::HoldSeconds(cli::Find("--edgemode-sweep=4", "--edgemode-sweep"), 5, 1, 60), 4);

    // A 0 s hold would show the user nothing; an absurd one would hang the probe.
    CHECK_EQ(cli::HoldSeconds(cli::Find("--edgemode-sweep=0", "--edgemode-sweep"), 5, 1, 60), 1);
    CHECK_EQ(cli::HoldSeconds(cli::Find("--edgemode-sweep=9999", "--edgemode-sweep"), 5, 1, 60), 60);

    // Garbage falls back to the default rather than to 0.
    CHECK_EQ(cli::HoldSeconds(cli::Find("--edgemode-sweep=soon", "--edgemode-sweep"), 5, 1, 60), 5);
    CHECK_EQ(cli::HoldSeconds(cli::Find("--edgemode-sweep=", "--edgemode-sweep"), 5, 1, 60), 5);
}

//-----------------------------------------------------------------------------
static void TestClamps() {
    group("effect_limits - brightness/speed clamps");

    // The ranges come from docs/Keyboard_Protocol.md section 3 (+0x02 / +0x03).
    CHECK_EQ(EFFECT_BRIGHTNESS_MAX, 4);
    CHECK_EQ(EFFECT_SPEED_MAX, 5);

    CHECK_EQ(ClampBrightness(-1), 0);
    CHECK_EQ(ClampBrightness(0), 0);
    CHECK_EQ(ClampBrightness(4), 4);
    CHECK_EQ(ClampBrightness(5), 4);
    CHECK_EQ(ClampBrightness(200), 4);

    CHECK_EQ(ClampSpeed(-3), 0);
    CHECK_EQ(ClampSpeed(0), 0);
    CHECK_EQ(ClampSpeed(5), 5);
    CHECK_EQ(ClampSpeed(6), 5);
    CHECK_EQ(ClampSpeed(255), 5);

    // A .rgb profile is plain text: these are the values a hand-edited or
    // older-build profile can carry into LoadProfile, and from there straight
    // into the keyboard's profile block if nothing clamps them.
    CHECK_EQ(ClampSpeed(200), 5);
    CHECK_EQ(ClampBrightness(100), 4);
}

//-----------------------------------------------------------------------------
static void TestEdgeModeTable() {
    group("effect_limits - edge mode table");

    // Static is 0x00. The edge-diagnose write probe used the literal 0x04 while
    // its comment claimed EDGE_MODE_STATIC, which is how the strip ended up on
    // an undocumented mode. 0x04 is the legacy value NormalizeEdgeMode maps away.
    CHECK_EQ(EDGE_MODE_STATIC, 0x00);
    CHECK(EDGE_MODE_STATIC != 0x04);
    CHECK_EQ(EDGE_MODE_OFF, 0x05);
    CHECK_EQ(EDGE_MODE_WAVE, 0x01);
    CHECK_EQ(EDGE_MODE_SPECTRUM, 0x02);
    CHECK_EQ(EDGE_MODE_BREATHING, 0x03);

    // ComboBox index <-> byte value round trip, both directions.
    for (int i = 0; i < EDGE_MODE_COUNT; i++)
        CHECK_EQ(EdgeModeToIndex(IndexToEdgeMode(i)), i);

    CHECK_EQ(IndexToEdgeMode(-1), EDGE_MODE_STATIC);
    CHECK_EQ(IndexToEdgeMode(EDGE_MODE_COUNT), EDGE_MODE_STATIC);
    CHECK_EQ(IndexToEdgeMode(999), EDGE_MODE_STATIC);
    CHECK_EQ(EdgeModeToIndex(0x77), 0);

    // Every mode the ComboBox can produce must survive NormalizeEdgeMode. If it
    // does not, the UI can set a mode that LoadProfile silently turns back into
    // Static - the setting would apply once and then vanish on the next load.
    // This is the invariant that breaks first when the table is trimmed after
    // --edgemode-sweep, which is exactly when it should shout.
    for (int i = 0; i < EDGE_MODE_COUNT; i++) {
        const uint8_t m = IndexToEdgeMode(i);
        CHECK_EQ(NormalizeEdgeMode(m), m);
    }

    CHECK_EQ(NormalizeEdgeMode(0x04), EDGE_MODE_STATIC);   // legacy
    CHECK_EQ(NormalizeEdgeMode(0x06), EDGE_MODE_STATIC);   // undocumented
    CHECK_EQ(NormalizeEdgeMode(0xFF), EDGE_MODE_STATIC);
    CHECK_EQ(NormalizeEdgeMode(EDGE_MODE_OFF), EDGE_MODE_OFF);
}

//-----------------------------------------------------------------------------
static void TestEdgeModeConfidence() {
    group("effect_limits - rendering confidence is honest");

    // The point of the confidence split: a read-back proves storage, not
    // rendering.
    //
    // 0x00 and 0x05 used to be claimed as RENDER_SEEN. --rendercheck=edge on
    // 2026-08-17 disproved both: static white, off, static red and static green
    // were each held at profile_base+0x1E and asked about, and nothing on the
    // device reacted to any of them - while every payload verified by read-back.
    // Nothing at this offset renders, so nothing here may claim to.
    //
    // Promoting any value back to RENDER_SEEN requires a measurement in the same
    // commit, plus docs/Keyboard_Protocol.md 3.1 (CLAUDE.md rule 1).
    CHECK_EQ(EdgeModeConfidenceOf(EDGE_MODE_STATIC), EDGE_CONF_STORED_ONLY);
    CHECK_EQ(EdgeModeConfidenceOf(EDGE_MODE_OFF), EDGE_CONF_STORED_ONLY);
    CHECK_EQ(EdgeModeConfidenceOf(EDGE_MODE_WAVE), EDGE_CONF_STORED_ONLY);
    CHECK_EQ(EdgeModeConfidenceOf(EDGE_MODE_SPECTRUM), EDGE_CONF_STORED_ONLY);
    CHECK_EQ(EdgeModeConfidenceOf(EDGE_MODE_BREATHING), EDGE_CONF_STORED_ONLY);
    CHECK_EQ(EdgeModeConfidenceOf(0x0A), EDGE_CONF_STORED_ONLY);

    CHECK_STR(EdgeModeName(EDGE_MODE_STATIC), "static/freeze");
    CHECK_STR(EdgeModeName(EDGE_MODE_OFF), "off");
    CHECK_STR(EdgeModeName(0x09), "undocumented");
}

//-----------------------------------------------------------------------------
// The probe flags as WinMain resolves them, exercised end to end on the string
// level. This is the table that decides which hardware write a command line
// causes; every row is one command line a user can actually type.
static void TestProbeFlagResolution() {
    group("probe flag resolution (as WinMain sees it)");

    struct Row {
        const char* cmdline;
        int         actions;      // how many mutually exclusive actions matched
        bool        edgeSingle;
        bool        edgeModeSweep;
        bool        edgeSpeedSweep;
    };
    static const Row rows[] = {
        { "",                                  0, false, false, false },
        { "--dry-run",                         0, false, false, false },
        { "--edgemode=3",                      1, true,  false, false },
        { "--edgemode=0x03 --dry-run",         1, true,  false, false },
        { "--edgemode-sweep",                  1, false, true,  false },
        { "--edgemode-sweep=5",                1, false, true,  false },
        { "--dry-run --edgemode-sweep=5",      1, false, true,  false },
        { "--edgespeed-sweep=4",               1, false, false, true  },
        { "--edgespeed-sweep --edgespeed-mode=1", 1, false, false, true },
        { "--edgemode=3 --edgemode-sweep",     2, true,  true,  false },
        { "--no-edgemode-sweep",               0, false, false, false },
        { "--minimized --no-apply",            0, false, false, false },
    };

    for (const Row& r : rows) {
        const cli::Flag one   = cli::Find(r.cmdline, "--edgemode");
        const cli::Flag msw   = cli::Find(r.cmdline, "--edgemode-sweep");
        const cli::Flag ssw   = cli::Find(r.cmdline, "--edgespeed-sweep");
        const int actions = cli::CountPresent({&one, &msw, &ssw});

        g_checks++;
        if (actions != r.actions || one.present != r.edgeSingle ||
            msw.present != r.edgeModeSweep || ssw.present != r.edgeSpeedSweep) {
            g_failures++;
            printf("  FAIL  \"%s\": actions %d/%d single %d/%d mode-sweep %d/%d speed-sweep %d/%d\n",
                   r.cmdline, actions, r.actions,
                   (int)one.present, (int)r.edgeSingle,
                   (int)msw.present, (int)r.edgeModeSweep,
                   (int)ssw.present, (int)r.edgeSpeedSweep);
        }
    }

    // --edgespeed-mode is a parameter, not an action: on its own it must not
    // start anything.
    const cli::Flag o = cli::Find("--edgespeed-mode=3", "--edgemode");
    const cli::Flag m = cli::Find("--edgespeed-mode=3", "--edgemode-sweep");
    const cli::Flag s = cli::Find("--edgespeed-mode=3", "--edgespeed-sweep");
    CHECK_EQ(cli::CountPresent({&o, &m, &s}), 0);
}

//-----------------------------------------------------------------------------
// --rendercheck and --confirm.
//
// --rendercheck is the anchor probe (does the firmware render this block at
// all); it REQUIRES a target, because guessing one would point four hardware
// writes at a device the user did not name. --confirm is a modifier that turns
// a sweep's hold time into a per-step question - it must never count as an
// action of its own, or "--confirm" alone would look like a probe request.
static void TestRenderCheckAndConfirm() {
    group("--rendercheck target / --confirm modifier");

    // Target is mandatory and must be one of the two known ones. Everything
    // else has to be refused rather than defaulted.
    struct Row { const char* cmdline; bool present; bool valid; const char* value; };
    static const Row rows[] = {
        { "--rendercheck=edge",            true,  true,  "edge" },
        { "--rendercheck=kb",              true,  true,  "kb"   },
        { "--rendercheck",                 true,  false, ""     },  // no target
        { "--rendercheck=",                true,  false, ""     },  // empty target
        { "--rendercheck=both",            true,  false, "both" },  // unknown target
        { "--rendercheck=EDGE",            true,  false, "EDGE" },  // case matters
        { "--renderchecked=edge",          false, false, ""     },  // not the flag
        { "--rendercheck-edge",            false, false, ""     },  // not the flag
        { "",                              false, false, ""     },
    };

    for (const Row& r : rows) {
        const cli::Flag f = cli::Find(r.cmdline, "--rendercheck");
        const bool wantEdge = f.hasValue && f.value == "edge";
        const bool wantKb   = f.hasValue && f.value == "kb";

        CHECK_EQ((int)f.present, (int)r.present);
        CHECK_EQ((int)(wantEdge || wantKb), (int)r.valid);
        if (r.present && r.value[0]) CHECK_STR(f.value, r.value);
    }

    // The modifier: present when given, absent otherwise, and token-exact so a
    // "--no-confirm" or a longer word cannot switch the dialogs on.
    CHECK_EQ((int)cli::Find("--edgemode-sweep --confirm", "--confirm").present, 1);
    CHECK_EQ((int)cli::Find("--kbmode-sweep=5 --confirm", "--confirm").present, 1);
    CHECK_EQ((int)cli::Find("--edgemode-sweep", "--confirm").present, 0);
    CHECK_EQ((int)cli::Find("--no-confirm", "--confirm").present, 0);
    CHECK_EQ((int)cli::Find("--confirmation", "--confirm").present, 0);

    // --confirm is not an action. A command line carrying only the modifier must
    // resolve to zero probe actions, so nothing is written.
    const cli::Flag c1 = cli::Find("--confirm", "--edgemode");
    const cli::Flag c2 = cli::Find("--confirm", "--edgemode-sweep");
    const cli::Flag c3 = cli::Find("--confirm", "--edgespeed-sweep");
    CHECK_EQ(cli::CountPresent({&c1, &c2, &c3}), 0);

    // ... and adding it to a real sweep must not turn one action into two.
    const cli::Flag d1 = cli::Find("--edgemode-sweep=5 --confirm", "--edgemode");
    const cli::Flag d2 = cli::Find("--edgemode-sweep=5 --confirm", "--edgemode-sweep");
    const cli::Flag d3 = cli::Find("--edgemode-sweep=5 --confirm", "--edgespeed-sweep");
    CHECK_EQ(cli::CountPresent({&d1, &d2, &d3}), 1);
    CHECK_EQ(cli::HoldSeconds(d2, 5, 1, 60), 5);
}

//-----------------------------------------------------------------------------
// ResolveChannelColor decides whether a channel follows the global colour or
// keeps one of its own. Every ASUS apply path funnels through it, so a
// regression here is exactly the bug this was written for: the test dialog's
// setting surviving until the next slider move and no longer.
static void TestResolveChannelColor() {
    group("ChannelConfig::ResolveChannelColor");

    uint8_t r = 0, g = 0, b = 0;

    // No override -> the global colour, untouched (neutral correction).
    ChannelConfig plain;
    ResolveChannelColor(plain, 10, 20, 30, r, g, b);
    CHECK_EQ(r, 10); CHECK_EQ(g, 20); CHECK_EQ(b, 30);

    // Override -> the channel's own colour, whatever the global colour is.
    ChannelConfig ovr;
    ovr.override_active = true;
    ovr.override_r = 200; ovr.override_g = 100; ovr.override_b = 50;
    ResolveChannelColor(ovr, 10, 20, 30, r, g, b);
    CHECK_EQ(r, 200); CHECK_EQ(g, 100); CHECK_EQ(b, 50);
    ResolveChannelColor(ovr, 255, 255, 255, r, g, b);
    CHECK_EQ(r, 200); CHECK_EQ(g, 100); CHECK_EQ(b, 50);

    // The channel correction still applies to an overridden channel - dialog
    // and apply path must compute the same bytes.
    ChannelConfig dim = ovr;
    dim.red_adjust = 50;               // 200 * 50 * 100 / 10000 = 100
    ResolveChannelColor(dim, 0, 0, 0, r, g, b);
    CHECK_EQ(r, 100); CHECK_EQ(g, 100); CHECK_EQ(b, 50);

    ChannelConfig half = ovr;
    half.brightness = 50;              // every component halved
    ResolveChannelColor(half, 0, 0, 0, r, g, b);
    CHECK_EQ(r, 100); CHECK_EQ(g, 50); CHECK_EQ(b, 25);

    // The correction also applies without an override.
    ChannelConfig dimGlobal;
    dimGlobal.brightness = 50;
    ResolveChannelColor(dimGlobal, 200, 100, 50, r, g, b);
    CHECK_EQ(r, 100); CHECK_EQ(g, 50); CHECK_EQ(b, 25);

    // Disabled wins over everything, override included.
    ChannelConfig off = ovr;
    off.enabled = false;
    ResolveChannelColor(off, 255, 255, 255, r, g, b);
    CHECK_EQ(r, 0); CHECK_EQ(g, 0); CHECK_EQ(b, 0);

    ChannelConfig offPlain;
    offPlain.enabled = false;
    ResolveChannelColor(offPlain, 255, 255, 255, r, g, b);
    CHECK_EQ(r, 0); CHECK_EQ(g, 0); CHECK_EQ(b, 0);

    // A fresh channel follows the global colour: an old config.json without the
    // override keys must not start overriding anything.
    ChannelConfig fresh;
    CHECK(fresh.override_active == false);

    // --- ignoreOverride: der Ausschaltweg ---------------------------------
    //
    // Der Anlass ist gemessen, nicht ausgedacht: in der echten config.json
    // dieses Rechners standen aura[1] und aura[2] auf override_active mit
    // (0,3,255). Beide ignorierten damit jedes Ausschalten - auch den Blackout
    // vor dem Standby - und blieben an. Fuer den Bediener sah der Aus-Knopf
    // kaputt aus, obwohl er tat, was er sollte.
    ResolveChannelColor(ovr, 0, 0, 0, r, g, b, /*ignoreOverride*/ true);
    CHECK_EQ(r, 0); CHECK_EQ(g, 0); CHECK_EQ(b, 0);

    // Dieselbe Kanalkonfiguration wie im gemessenen Fall.
    ChannelConfig pinned;
    pinned.override_active = true;
    pinned.override_r = 0; pinned.override_g = 3; pinned.override_b = 255;
    ResolveChannelColor(pinned, 0, 0, 0, r, g, b);                       // alter Weg
    CHECK_EQ(r, 0); CHECK_EQ(g, 3); CHECK_EQ(b, 255);                    // blieb an
    ResolveChannelColor(pinned, 0, 0, 0, r, g, b, true);                 // Ausschaltweg
    CHECK_EQ(r, 0); CHECK_EQ(g, 0); CHECK_EQ(b, 0);                      // ist aus

    // Der Bypass gilt NUR fuers Ausschalten. Eine Farbwahl darf gepinnte
    // Kanaele weiterhin nicht anfassen - sonst waere die eigene Kanalfarbe
    // beim ersten Preset-Klick weg, und das war ausdruecklich nicht gewollt.
    ResolveChannelColor(pinned, 255, 0, 0, r, g, b);
    CHECK_EQ(r, 0); CHECK_EQ(g, 3); CHECK_EQ(b, 255);

    // ignoreOverride reicht die globale Farbe durch, es erzwingt kein Schwarz.
    // Der Aufrufer entscheidet, was ankommt; hier steht nur, wessen Farbe gilt.
    ResolveChannelColor(pinned, 12, 34, 56, r, g, b, true);
    CHECK_EQ(r, 12); CHECK_EQ(g, 34); CHECK_EQ(b, 56);

    // Ein Kanal ohne Override verhaelt sich mit und ohne Bypass gleich.
    ResolveChannelColor(plain, 10, 20, 30, r, g, b, true);
    CHECK_EQ(r, 10); CHECK_EQ(g, 20); CHECK_EQ(b, 30);

    // "Aktiv" abgewaehlt bleibt aus, auch auf dem Ausschaltweg - der Bypass
    // hebt den Override auf, nicht die Kanalabschaltung.
    ResolveChannelColor(off, 255, 255, 255, r, g, b, true);
    CHECK_EQ(r, 0); CHECK_EQ(g, 0); CHECK_EQ(b, 0);
}

//-----------------------------------------------------------------------------
// --kbdump-range. The old dump was hardcoded to 0x000..0x3FF, which is why the
// tail of the per-key colour table was never seen. A range argument that quietly
// repairs a malformed value would be worse than no range at all: the dump is the
// reference state every collateral check compares against, so it has to cover
// exactly what was asked for or refuse.
static void TestParseRange16() {
    group("cli::ParseRange16 - --kbdump-range");

    uint16_t lo = 0xAAAA, hi = 0xAAAA;
    CHECK(cli::ParseRange16("0x000-0x3FF", lo, hi));
    CHECK_EQ(lo, 0x000); CHECK_EQ(hi, 0x3FF);

    CHECK(cli::ParseRange16("0x2C0-0x43A", lo, hi));
    CHECK_EQ(lo, 0x2C0); CHECK_EQ(hi, 0x43A);

    CHECK(cli::ParseRange16("0-1023", lo, hi));
    CHECK_EQ(lo, 0); CHECK_EQ(hi, 1023);

    // A single address is a legitimate range of one byte.
    CHECK(cli::ParseRange16("0x2C0-0x2C0", lo, hi));
    CHECK_EQ(lo, 0x2C0); CHECK_EQ(hi, 0x2C0);

    CHECK(cli::ParseRange16("0-0xFFFF", lo, hi));
    CHECK_EQ(hi, 0xFFFF);

    // Reversed, incomplete, absent, oversized, mistyped.
    CHECK(!cli::ParseRange16("0x300-0x200", lo, hi));
    CHECK(!cli::ParseRange16("0x300-", lo, hi));
    CHECK(!cli::ParseRange16("-0x300", lo, hi));
    CHECK(!cli::ParseRange16("0x300", lo, hi));
    CHECK(!cli::ParseRange16("", lo, hi));
    CHECK(!cli::ParseRange16("-", lo, hi));
    CHECK(!cli::ParseRange16("1-2-3", lo, hi));
    CHECK(!cli::ParseRange16("0x0-0x10000", lo, hi));   // past 16 bit
    CHECK(!cli::ParseRange16("0x2C0 - 0x400", lo, hi)); // spaces are not numbers
    CHECK(!cli::ParseRange16("lo-hi", lo, hi));

    // As WinMain resolves it: the flag is a modifier and needs a value.
    const cli::Flag ok = cli::Find("--kbdump --kbdump-range=0x2C0-0x4FF", "--kbdump-range");
    CHECK(ok.present); CHECK(ok.hasValue);
    CHECK(cli::ParseRange16(ok.value, lo, hi));
    CHECK_EQ(lo, 0x2C0); CHECK_EQ(hi, 0x4FF);

    const cli::Flag bare = cli::Find("--kbdump --kbdump-range", "--kbdump-range");
    CHECK(bare.present); CHECK(!bare.hasValue);   // -> caller must refuse

    // Token-exact, like every other flag here.
    CHECK(!cli::Find("--kbdump-ranger=1-2", "--kbdump-range").present);
    CHECK(!cli::Find("--kbdump", "--kbdump-range").present);
}

//-----------------------------------------------------------------------------
// --kbmode-only and --keyidentify both take lists. A list where one element
// silently vanished is the dangerous case: the report would still name the full
// set while the walk skipped a candidate, i.e. a measurement of something other
// than what it claims.
static void TestParseLists() {
    group("cli::ParseByteList / ParseOffsetList");

    std::vector<uint8_t> bytes;
    CHECK(cli::ParseByteList("0x00,0x04,0x09,0x13,0x14", bytes));
    CHECK_EQ(bytes.size(), 5);
    if (bytes.size() == 5) {
        CHECK_EQ(bytes[0], 0x00); CHECK_EQ(bytes[1], 0x04);
        CHECK_EQ(bytes[4], 0x14);
    }

    CHECK(cli::ParseByteList("6", bytes));
    CHECK_EQ(bytes.size(), 1);
    CHECK(cli::ParseByteList("1,2,3", bytes));
    CHECK_EQ(bytes.size(), 3);

    // Duplicates are the user's business - walking a mode twice is legal.
    CHECK(cli::ParseByteList("5,5", bytes));
    CHECK_EQ(bytes.size(), 2);

    // One bad element rejects the whole list, and leaves nothing behind.
    CHECK(!cli::ParseByteList("0x00,banana,0x09", bytes));
    CHECK_EQ(bytes.size(), 0);
    CHECK(!cli::ParseByteList("0x00,,0x09", bytes));
    CHECK(!cli::ParseByteList("0x00,", bytes));
    CHECK(!cli::ParseByteList(",0x00", bytes));
    CHECK(!cli::ParseByteList("", bytes));
    CHECK(!cli::ParseByteList("256", bytes));
    CHECK(!cli::ParseByteList("0x00, 0x04", bytes));   // space is not a digit

    // The cap exists so a pasted file cannot turn into thousands of writes.
    CHECK(!cli::ParseByteList("1,2,3,4", bytes, 3));
    CHECK(cli::ParseByteList("1,2,3", bytes, 3));

    std::vector<uint16_t> offs;
    CHECK(cli::ParseOffsetList("0x2C0", offs));
    CHECK_EQ(offs.size(), 1);
    CHECK_EQ(offs[0], 0x2C0);

    CHECK(cli::ParseOffsetList("0x2C0,0x2C3,0x2F0,0x437", offs));
    CHECK_EQ(offs.size(), 4);
    if (offs.size() == 4) { CHECK_EQ(offs[2], 0x2F0); CHECK_EQ(offs[3], 0x437); }

    CHECK(!cli::ParseOffsetList("0x2C0,0x10000", offs));   // past 16 bit
    CHECK(!cli::ParseOffsetList("0x2C0,", offs));
    CHECK(!cli::ParseOffsetList("", offs));
}

//-----------------------------------------------------------------------------
// --ask selects the question a --confirm dialog puts on screen. An answer only
// means something together with the question it answered, so a malformed --ask
// must be refused rather than defaulted - substituting a different question
// silently is how a "nein" about motion ends up recorded as a statement about
// per-key colour.
static void TestAsk() {
    group("cli::ParseAsk / ResolveAsk");

    cli::AskKind k = cli::ASK_MOTION;
    CHECK(cli::ParseAsk("motion", k)); CHECK_EQ(k, cli::ASK_MOTION);
    CHECK(cli::ParseAsk("lit", k));    CHECK_EQ(k, cli::ASK_LIT);
    CHECK(cli::ParseAsk("perkey", k)); CHECK_EQ(k, cli::ASK_PERKEY);

    CHECK(!cli::ParseAsk("Motion", k));    // case matters, like --rendercheck
    CHECK(!cli::ParseAsk("", k));
    CHECK(!cli::ParseAsk("colour", k));
    CHECK(!cli::ParseAsk("motion ", k));

    // Absent -> the default, and today's behaviour is unchanged.
    k = cli::ASK_PERKEY;
    CHECK(cli::ResolveAsk(cli::Find("--kbmode-sweep", "--ask"), k));
    CHECK_EQ(k, cli::ASK_MOTION);

    CHECK(cli::ResolveAsk(cli::Find("--kbmode-sweep --ask=perkey", "--ask"), k));
    CHECK_EQ(k, cli::ASK_PERKEY);

    // Present but malformed -> error, not default.
    CHECK(!cli::ResolveAsk(cli::Find("--ask", "--ask"), k));
    CHECK(!cli::ResolveAsk(cli::Find("--ask=", "--ask"), k));
    CHECK(!cli::ResolveAsk(cli::Find("--ask=whatever", "--ask"), k));

    // Not an action of its own, exactly like --confirm.
    const cli::Flag a1 = cli::Find("--ask=perkey", "--kbmode");
    const cli::Flag a2 = cli::Find("--ask=perkey", "--kbmode-sweep");
    CHECK_EQ(cli::CountPresent({&a1, &a2}), 0);

    // --kbmode-only is a modifier too, and token-exact.
    CHECK(cli::Find("--kbmode-sweep --kbmode-only=0x00,0x04", "--kbmode-only").present);
    CHECK(!cli::Find("--kbmode-only=0x00", "--kbmode-sweep").present);
    CHECK(!cli::Find("--kbmode-only=0x00", "--kbmode").present);
}

//-----------------------------------------------------------------------------
static void TestParseRgb() {
    group("cli::ParseRgb - --kbcolor");

    uint8_t r = 0, g = 0, b = 0;
    CHECK(cli::ParseRgb("FF0000", r, g, b));
    CHECK_EQ(r, 255); CHECK_EQ(g, 0); CHECK_EQ(b, 0);
    CHECK(cli::ParseRgb("0022ff", r, g, b));
    CHECK_EQ(r, 0); CHECK_EQ(g, 0x22); CHECK_EQ(b, 0xFF);
    CHECK(cli::ParseRgb("000000", r, g, b));
    CHECK_EQ(r, 0); CHECK_EQ(g, 0); CHECK_EQ(b, 0);

    CHECK(!cli::ParseRgb("#FF0000", r, g, b));
    CHECK(!cli::ParseRgb("0xFF0000", r, g, b));
    CHECK(!cli::ParseRgb("FF00", r, g, b));
    CHECK(!cli::ParseRgb("FF00000", r, g, b));
    CHECK(!cli::ParseRgb("", r, g, b));
    CHECK(!cli::ParseRgb("GGGGGG", r, g, b));
}

//-----------------------------------------------------------------------------
// The key matrix decoder. Two very different confidence levels live in this
// header and the tests keep them apart:
//
//   * The decoding rules (3-byte entries, prefix meanings, HID usage names) are
//     spec and are asserted directly.
//   * The COLOUR table geometry is an inference. These tests pin the arithmetic
//     the code uses, not a claim about the hardware - which slot really drives
//     which key is what --keyidentify measures.
static void TestKeyLayoutGeometry() {
    group("kblayout - matrix geometry");

    // Cross-checked against the live dump in docs/Keyboard_Protocol.md section 4:
    // the table starts at 0xC0 and the column stride is 0x12, so exactly six
    // entries per column.
    CHECK_EQ(kblayout::REMAP_BASE, 0xC0);
    CHECK_EQ(kblayout::MATRIX_ROWS, 6);
    CHECK_EQ(kblayout::REMAP_COL_STRIDE, kblayout::MATRIX_ROWS * 3);
    CHECK_EQ(kblayout::REMAP_END, 0x23A);   // 0xC0 + 21*0x12, where padding starts

    CHECK_EQ(kblayout::RemapOffset(0, 0), 0xC0);
    CHECK_EQ(kblayout::RemapOffset(0, 1), 0xC3);
    CHECK_EQ(kblayout::RemapOffset(0, 5), 0xCF);
    CHECK_EQ(kblayout::RemapOffset(1, 0), 0xD2);   // next column
    CHECK_EQ(kblayout::RemapOffset(1, 1), 0xD5);

    // Column-major, matching the order the dump walks.
    CHECK_EQ(kblayout::SlotIndex(0, 0), 0);
    CHECK_EQ(kblayout::SlotIndex(0, 5), 5);
    CHECK_EQ(kblayout::SlotIndex(1, 0), 6);
    CHECK_EQ(kblayout::SlotIndex(20, 5), 125);

    CHECK_EQ(kblayout::MATRIX_SLOTS, 126);
    CHECK_EQ(kblayout::KEYCOLOR_BASE, 0x2C0);

    // Two boundaries, deliberately not equal:
    //   SLOTS_END  = where one-triple-per-matrix-slot would end   [inferred]
    //   REGION_END = where the non-zero bytes actually stop       [measured]
    // The gap between them is a finding (docs 5.5), so a change that quietly
    // makes them agree has to break a test rather than pass silently.
    CHECK_EQ(kblayout::KEYCOLOR_SLOTS_END, 0x2C0 + 126 * 3);   // 0x43A
    CHECK_EQ(kblayout::KEYCOLOR_REGION_END, 0x440);
    CHECK_EQ(kblayout::KEYCOLOR_TRIPLES, 128);
    CHECK(kblayout::KEYCOLOR_REGION_END > kblayout::KEYCOLOR_SLOTS_END);
    CHECK_EQ(kblayout::KEYCOLOR_REGION_END - kblayout::KEYCOLOR_SLOTS_END, 6);  // two triples

    // The number that motivated --kbdump-range: both ends lie past the window
    // the old dump was hardcoded to, so none of this was visible before.
    CHECK(kblayout::KEYCOLOR_SLOTS_END > 0x400);
    CHECK(kblayout::KEYCOLOR_REGION_END > 0x400);

    CHECK_EQ(kblayout::KeyColorOffset(0, 0), 0x2C0);
    CHECK_EQ(kblayout::KeyColorOffset(0, 1), 0x2C3);
    CHECK_EQ(kblayout::KeyColorOffset(1, 0), 0x2C0 + 18);

    // A write must start on a triple boundary; starting mid-triple would shift
    // every following key's colour by one channel.
    CHECK(kblayout::IsKeyColorOffset(0x2C0));
    CHECK(kblayout::IsKeyColorOffset(0x2C3));
    CHECK(!kblayout::IsKeyColorOffset(0x2C1));
    CHECK(!kblayout::IsKeyColorOffset(0x2C2));
    CHECK(!kblayout::IsKeyColorOffset(0x2BF));        // before the table
    CHECK(!kblayout::IsKeyColorOffset(kblayout::KEYCOLOR_REGION_END));      // one past
    CHECK(kblayout::IsKeyColorOffset(kblayout::KEYCOLOR_REGION_END - 3));   // last triple

    // The two triples past the matrix are addressable on purpose: they are the
    // offsets where the slot assumption breaks, so --keyidentify must be able to
    // point at them. They just have no predicted key.
    CHECK(kblayout::IsKeyColorOffset(kblayout::KEYCOLOR_SLOTS_END));
    CHECK(kblayout::IsKeyColorOffset(kblayout::KEYCOLOR_SLOTS_END + 3));

    int col = -1, row = -1;
    CHECK(!kblayout::KeyColorOffsetToSlot(kblayout::KEYCOLOR_SLOTS_END, col, row));
    CHECK_EQ(col, -1);
    CHECK(!kblayout::KeyColorOffsetToSlot(kblayout::KEYCOLOR_REGION_END - 3, col, row));

    CHECK(kblayout::KeyColorOffsetToSlot(0x2C0, col, row));
    CHECK_EQ(col, 0); CHECK_EQ(row, 0);
    CHECK(kblayout::KeyColorOffsetToSlot(0x2C3, col, row));
    CHECK_EQ(col, 0); CHECK_EQ(row, 1);
    CHECK(kblayout::KeyColorOffsetToSlot(0x2C0 + 18, col, row));
    CHECK_EQ(col, 1); CHECK_EQ(row, 0);
    CHECK(!kblayout::KeyColorOffsetToSlot(0x2C1, col, row));
    CHECK_EQ(col, -1);

    // Round trip over every slot.
    for (int c = 0; c < kblayout::MATRIX_COLS; c++) {
        for (int r2 = 0; r2 < kblayout::MATRIX_ROWS; r2++) {
            const uint16_t off = kblayout::KeyColorOffset(c, r2);
            int gc = -1, gr = -1;
            g_checks++;
            if (!kblayout::KeyColorOffsetToSlot(off, gc, gr) || gc != c || gr != r2) {
                g_failures++;
                printf("  FAIL  slot (%d,%d) -> 0x%04X -> (%d,%d)\n", c, r2, off, gc, gr);
            }
        }
    }
}

//-----------------------------------------------------------------------------
static void TestKeyEntryDecode() {
    group("kblayout - remap entry decoding");

    // "20 00 00" is the documented marker for an empty matrix position. It must
    // never become a drawable key: the UI would offer a key nobody has.
    kblayout::KeyEntry e = kblayout::DecodeEntry(0x20, 0x00, 0x00);
    CHECK(!e.assigned);
    e = kblayout::DecodeEntry(0x00, 0x00, 0x00);   // padding past the table
    CHECK(!e.assigned);

    // Standard keys, prefix 0x20 with a HID usage. These usages are the HID
    // spec, not values copied out of a dump.
    e = kblayout::DecodeEntry(0x20, 0x00, 0x29); CHECK(e.assigned); CHECK_STR(e.label, "Esc");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x35); CHECK_STR(e.label, "`");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x2B); CHECK_STR(e.label, "Tab");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x39); CHECK_STR(e.label, "Caps");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x04); CHECK_STR(e.label, "A");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x1D); CHECK_STR(e.label, "Z");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x1E); CHECK_STR(e.label, "1");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x2C); CHECK_STR(e.label, "Space");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x45); CHECK_STR(e.label, "F12");
    e = kblayout::DecodeEntry(0x20, 0x00, 0x64); CHECK_STR(e.label, "NonUS\\");

    // Modifiers come from the bitmask, not the usage byte.
    e = kblayout::DecodeEntry(0x20, 0x02, 0x00); CHECK_STR(e.label, "LShift");
    e = kblayout::DecodeEntry(0x20, 0x01, 0x00); CHECK_STR(e.label, "LCtrl");
    e = kblayout::DecodeEntry(0x20, 0x08, 0x00); CHECK_STR(e.label, "LWin");
    e = kblayout::DecodeEntry(0x20, 0x40, 0x00); CHECK_STR(e.label, "RAlt");

    // Consumer usages are little endian.
    e = kblayout::DecodeEntry(0x30, 0x92, 0x01); CHECK(e.assigned); CHECK_STR(e.label, "Calc");
    e = kblayout::DecodeEntry(0x30, 0xE2, 0x00); CHECK_STR(e.label, "Mute");
    e = kblayout::DecodeEntry(0x30, 0xEA, 0x00); CHECK_STR(e.label, "Vol-");

    e = kblayout::DecodeEntry(0xA0, 0x01, 0x00); CHECK(e.assigned); CHECK_STR(e.label, "Fn1");

    // Unknown codes are reported as themselves rather than guessed at - a wrong
    // cap on a key the write path then addresses by that name is worse than a
    // raw number.
    e = kblayout::DecodeEntry(0x20, 0x00, 0xF0); CHECK(e.assigned); CHECK_STR(e.label, "uF0");
    e = kblayout::DecodeEntry(0x20, 0x03, 0x00); CHECK_STR(e.label, "mod03");   // two bits
    e = kblayout::DecodeEntry(0x30, 0x34, 0x12); CHECK_STR(e.label, "c1234");
    e = kblayout::DecodeEntry(0x77, 0x01, 0x02); CHECK_STR(e.label, "?770102");
}

//-----------------------------------------------------------------------------
static void TestBuildLayout() {
    group("kblayout - BuildLayout");

    // A synthetic remap block: the first column as the live dump has it, one
    // hole, then zeros. The point is the decoder's behaviour, so the buffer is
    // built here rather than read from a file.
    const int len = (int)kblayout::REMAP_END - (int)kblayout::REMAP_BASE;
    std::vector<uint8_t> remap((size_t)len, 0x00);

    struct E { int col, row; uint8_t a, b, c; };
    const E entries[] = {
        { 0, 0, 0x20, 0x00, 0x29 },   // Esc
        { 0, 1, 0x20, 0x00, 0x35 },   // `
        { 0, 2, 0x20, 0x00, 0x2B },   // Tab
        { 0, 3, 0x20, 0x00, 0x39 },   // Caps
        { 0, 4, 0x20, 0x02, 0x00 },   // LShift
        { 0, 5, 0x20, 0x01, 0x00 },   // LCtrl
        { 1, 0, 0x20, 0x00, 0x00 },   // hole
        { 1, 1, 0x20, 0x00, 0x1E },   // 1
        { 1, 5, 0x20, 0x08, 0x00 },   // LWin
        { 20, 5, 0x30, 0x92, 0x01 },  // Calc, last slot
    };
    for (const E& e : entries) {
        const int idx = (int)kblayout::RemapOffset(e.col, e.row) - (int)kblayout::REMAP_BASE;
        remap[(size_t)idx + 0] = e.a;
        remap[(size_t)idx + 1] = e.b;
        remap[(size_t)idx + 2] = e.c;
    }

    std::vector<kblayout::Key> keys = kblayout::BuildLayout(remap.data(), remap.size());

    // Nine assigned entries: the hole and all the zero padding are dropped.
    CHECK_EQ(keys.size(), 9);

    CHECK_STR(keys[0].label, "Esc");
    CHECK_EQ(keys[0].col, 0); CHECK_EQ(keys[0].row, 0);
    CHECK_EQ(keys[0].remapOffset, 0xC0);
    CHECK_EQ(keys[0].colorOffset, 0x2C0);
    CHECK(keys[0].assigned);
    // Nothing has read a colour yet, so no colour may be claimed.
    CHECK(!keys[0].colorKnown);

    CHECK_STR(keys[5].label, "LCtrl");
    CHECK_EQ(keys[5].remapOffset, 0xCF);

    // Column-major order survives the hole: (1,1) follows (0,5), and (1,0) is
    // simply absent.
    CHECK_STR(keys[6].label, "1");
    CHECK_EQ(keys[6].col, 1); CHECK_EQ(keys[6].row, 1);
    CHECK_STR(keys[7].label, "LWin");
    CHECK_STR(keys[8].label, "Calc");
    // The last matrix slot maps to the last SLOT-mapped triple, not to the last
    // triple in the region - the region has two more.
    CHECK_EQ(keys[8].colorOffset, kblayout::KEYCOLOR_SLOTS_END - 3);

    // A short buffer produces a short layout, never invented entries.
    std::vector<kblayout::Key> partial = kblayout::BuildLayout(remap.data(), 9);
    CHECK_EQ(partial.size(), 3);   // three entries fit in nine bytes
    CHECK_EQ(kblayout::BuildLayout(nullptr, 100).size(), 0);
    CHECK_EQ(kblayout::BuildLayout(remap.data(), 0).size(), 0);

    // The editor starts from the colour the keyboard really shows - the
    // profile's own colour - and never from the unverified 0x2C0 bytes. Reading
    // those at a 3-byte stride produced a board full of colours nobody had set,
    // because the region repeats with a 16-byte period from 0x2F0 (docs 5.5).
    kblayout::SeedKeyColors(keys, 0x00, 0x22, 0xFF);
    for (size_t i = 0; i < keys.size(); i++) {
        g_checks++;
        if (!keys[i].colorKnown || keys[i].r != 0x00 || keys[i].g != 0x22 || keys[i].b != 0xFF) {
            g_failures++;
            printf("  FAIL  key %u not seeded with the profile colour\n", (unsigned)i);
        }
    }

    // Ids are keyed by matrix position, so a remapped key keeps its place.
    CHECK_STR(kblayout::KeyId(0, 0), "key_c00r0");
    CHECK_STR(kblayout::KeyId(20, 5), "key_c20r5");
    CHECK(kblayout::KeyId(1, 2) != kblayout::KeyId(2, 1));
}

//-----------------------------------------------------------------------------
// Board drawing geometry. This is the one part of keyboard_layout.h that is NOT
// device data - key widths are not in the config memory and cannot be - so what
// these tests pin down is that the presentation is self-consistent: rows that
// end flush, clusters that line up, and an unknown key that still gets a cap.
//
// A geometry bug is a cap of the wrong width. It cannot send a byte to the
// wrong key, because the write path addresses kblayout::Key::colorOffset and
// never a rectangle.
static void TestBoardGeometry() {
    group("kblayout - board drawing geometry");

    // Widths: the ISO set, chosen so a main-block row adds up to exactly 15.0.
    CHECK(kblayout::KeyWidthUnits("Q") == 1.00f);
    CHECK(kblayout::KeyWidthUnits("Backspace") == 2.00f);
    CHECK(kblayout::KeyWidthUnits("Tab") == 1.50f);
    CHECK(kblayout::KeyWidthUnits("Caps") == 1.75f);
    CHECK(kblayout::KeyWidthUnits("Space") == 6.25f);
    CHECK(kblayout::KeyWidthUnits("RShift") == 2.75f);
    CHECK(kblayout::KeyWidthUnits("Fn1") == 1.25f);
    // An unknown key is a normal key, not a missing one.
    CHECK(kblayout::KeyWidthUnits("u9A") == 1.00f);
    CHECK(kblayout::KeyWidthUnits("c1234") == 1.00f);

    CHECK_EQ(kblayout::ClusterOfLabel("Q"), kblayout::CLUSTER_MAIN);
    CHECK_EQ(kblayout::ClusterOfLabel("Space"), kblayout::CLUSTER_MAIN);
    CHECK_EQ(kblayout::ClusterOfLabel("u9A"), kblayout::CLUSTER_MAIN);
    CHECK_EQ(kblayout::ClusterOfLabel("Ins"), kblayout::CLUSTER_NAV);
    CHECK_EQ(kblayout::ClusterOfLabel("Up"), kblayout::CLUSTER_NAV);
    CHECK_EQ(kblayout::ClusterOfLabel("KP7"), kblayout::CLUSTER_NUMPAD);
    CHECK_EQ(kblayout::ClusterOfLabel("NumLk"), kblayout::CLUSTER_NUMPAD);
    CHECK_EQ(kblayout::ClusterOfLabel("Mute"), kblayout::CLUSTER_NUMPAD);   // sits over it
    // The browser-home key must not be mistaken for the nav Home, or the two
    // would share a cap and the wrong one gets coloured.
    CHECK_EQ(kblayout::ClusterOfLabel("WWW"), kblayout::CLUSTER_NUMPAD);
    CHECK_EQ(kblayout::ClusterOfLabel("Home"), kblayout::CLUSTER_NAV);
    CHECK_STR(kblayout::ConsumerUsageName(0x0223), "WWW");

    // Build one real row (the number row) and check it ends flush at 15.0.
    struct E { int col, row; uint8_t a, b, c; };
    const int len = (int)kblayout::REMAP_END - (int)kblayout::REMAP_BASE;
    std::vector<uint8_t> remap((size_t)len, 0x00);
    auto put = [&](int col, int row, uint8_t a, uint8_t b, uint8_t c) {
        const int idx = (int)kblayout::RemapOffset(col, row) - (int)kblayout::REMAP_BASE;
        remap[(size_t)idx] = a; remap[(size_t)idx + 1] = b; remap[(size_t)idx + 2] = c;
    };
    // Row 1 of the live board: ` 1 2 3 4 5 6 7 8 9 0 - = Backspace
    const uint8_t rowUsages[] = { 0x35, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
                                  0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A };
    for (int i = 0; i < 14; i++) put(i, 1, 0x20, 0x00, rowUsages[i]);
    // Numpad column 20: KP- (row 1), KP+ (row 2), hole (row 3) -> KP+ is tall.
    put(20, 1, 0x20, 0x00, 0x56);
    put(20, 2, 0x20, 0x00, 0x57);
    // Nav: Up with a hole either side, Down below it.
    put(15, 4, 0x20, 0x00, 0x52);
    put(15, 5, 0x20, 0x00, 0x51);

    std::vector<kblayout::Key> keys = kblayout::BuildLayout(remap.data(), remap.size());
    std::vector<kblayout::KeyBox> boxes = kblayout::ComputeKeyBoxes(keys);
    CHECK_EQ(boxes.size(), keys.size());

    float rowRight = 0.0f;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].row != 1 || kblayout::ClusterOfLabel(keys[i].label) != kblayout::CLUSTER_MAIN)
            continue;
        const float right = boxes[i].x + boxes[i].w;
        if (right > rowRight) rowRight = right;
    }
    CHECK(rowRight > 14.99f && rowRight < 15.01f);   // flush with every other row

    // Nav sits at its fixed origin, and Up is above Down rather than shoved left.
    float upX = -1, downX = -1, kpPlusH = 0;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].label == "Up")   upX   = boxes[i].x;
        if (keys[i].label == "Dn" || keys[i].label == "Down") downX = boxes[i].x;
        if (keys[i].label == "KP+")  kpPlusH = boxes[i].h;
    }
    CHECK(upX > 0);
    CHECK(upX == downX);
    CHECK(upX == kblayout::BOARD_NAV_X + 1.0f);

    // KP+ has a hole under it in the device data, so it is drawn two rows tall.
    // This is the one geometry fact that DOES come from the device.
    CHECK(kpPlusH == 2.0f);

    // Everything stays inside the declared board size.
    for (size_t i = 0; i < boxes.size(); i++) {
        g_checks++;
        if (boxes[i].x < 0 || boxes[i].x + boxes[i].w > kblayout::BOARD_WIDTH + 0.01f ||
            boxes[i].y < 0 || boxes[i].y + boxes[i].h > kblayout::BOARD_HEIGHT + 0.01f) {
            g_failures++;
            printf("  FAIL  %s outside board: x=%.2f w=%.2f y=%.2f h=%.2f\n",
                   keys[i].label.c_str(), boxes[i].x, boxes[i].w, boxes[i].y, boxes[i].h);
        }
    }

    // Short caps are display-only: the full label must survive in the model,
    // because that is what the cluster rules and the reports read.
    CHECK_STR(kblayout::ShortLabel("Down"), "Dn");
    CHECK_STR(kblayout::ShortLabel("KPEnter"), "Ent");
    CHECK_STR(kblayout::ShortLabel("Q"), "Q");
    CHECK_STR(kblayout::ShortLabel("Space"), "Space");
    CHECK_EQ(kblayout::ClusterOfLabel("KPEnter"), kblayout::CLUSTER_NUMPAD);
}

//-----------------------------------------------------------------------------
// --keyidentify writes into the colour table, so its argument decides which
// bytes get written. Every offset has to be inside the documented extent and on
// a triple boundary before anything is sent (CLAUDE.md rule 2).
static void TestKeyIdentifyArgs() {
    group("--keyidentify argument resolution");

    struct Row { const char* cmdline; bool present; bool valid; size_t count; };
    static const Row rows[] = {
        { "--keyidentify=0x2C0",                 true,  true,  1 },
        { "--keyidentify=0x2C0,0x2C3,0x2F0",     true,  true,  3 },
        { "--keyidentify=704",                   true,  true,  1 },   // 0x2C0 decimal
        { "--keyidentify",                       true,  false, 0 },   // no value
        { "--keyidentify=",                      true,  false, 0 },
        { "--keyidentify=0x2C1",                 true,  false, 0 },   // mid-triple
        { "--keyidentify=0x100",                 true,  false, 0 },   // before the table
        { "--keyidentify=0x2C0,0x900",           true,  false, 0 },   // past the region
        { "--keyidentify=0x43A,0x43D",           true,  true,  2 },   // the two unmapped triples
        { "--keyidentify=0x43D",                 true,  true,  1 },   // last triple in region
        { "--keyidentify=0x440",                 true,  false, 0 },   // one past the region
        { "--keyidentify=0x2C0,junk",            true,  false, 0 },
        { "--keyidentified=0x2C0",               false, false, 0 },
        { "",                                    false, false, 0 },
    };

    for (const Row& r : rows) {
        const cli::Flag f = cli::Find(r.cmdline, "--keyidentify");
        CHECK_EQ((int)f.present, (int)r.present);

        std::vector<uint16_t> offs;
        bool ok = f.hasValue && cli::ParseOffsetList(f.value, offs, 64);
        if (ok) {
            for (uint16_t o : offs)
                if (!kblayout::IsKeyColorOffset(o)) { ok = false; break; }
        }
        CHECK_EQ((int)ok, (int)r.valid);
        if (r.valid) CHECK_EQ(offs.size(), r.count);
    }

    // --keypattern takes nothing or "restore"; anything else has to be refused
    // rather than treated as "apply", because those two do opposite things.
    CHECK(cli::Find("--keypattern", "--keypattern").present);
    CHECK(!cli::Find("--keypattern", "--keypattern").hasValue);
    const cli::Flag rest = cli::Find("--keypattern=restore", "--keypattern");
    CHECK(rest.hasValue);
    CHECK_STR(rest.value, "restore");
    CHECK_STR(cli::Find("--keypattern=Restore", "--keypattern").value, "Restore"); // != restore
    CHECK(!cli::Find("--keypatterns", "--keypattern").present);
}

//-----------------------------------------------------------------------------
static void TestAudioProbeArgs() {
    group("cli::Find - Audio-Sonde (Phase 6)");

    // Die Audio-Sonde hat drei Aktionsflags, von denen zwei mit dem dritten
    // anfangen: --audioprobe, --audioprobe-selftest, --audioprobe-list. Mit
    // strstr waere jedes "--audioprobe-*" auch ein --audioprobe gewesen, und
    // --audioprobe faehrt die volle Messreihe: 6 Pegelstufen x 8 Frequenzen mit
    // Dialogen dazwischen. Ein Tippfehler im Namen haette also statt einer
    // 5-Sekunden-Selbstprobe eine vierminuetige Tonfolge gestartet.
    CHECK(strstr("--audioprobe-selftest", "--audioprobe") != nullptr);
    CHECK(!cli::Find("--audioprobe-selftest", "--audioprobe").present);

    CHECK(strstr("--audioprobe-list", "--audioprobe") != nullptr);
    CHECK(!cli::Find("--audioprobe-list", "--audioprobe").present);

    // Umgekehrt darf das kurze Flag die langen nicht ausloesen.
    CHECK(!cli::Find("--audioprobe", "--audioprobe-selftest").present);
    CHECK(!cli::Find("--audioprobe", "--audioprobe-list").present);
    CHECK(!cli::Find("--audioprobe-list", "--audioprobe-selftest").present);

    // Und jedes fuer sich muss gefunden werden.
    CHECK(cli::Find("--audioprobe", "--audioprobe").present);
    CHECK(cli::Find("--audioprobe-selftest", "--audioprobe-selftest").present);
    CHECK(cli::Find("--audioprobe-list", "--audioprobe-list").present);
    CHECK(cli::Find("--dry-run --audioprobe", "--audioprobe").present);

    // CountPresent traegt die Ablehnung mehrerer Aktionen. Ohne sie wuerde eine
    // davon still gewinnen, und der Bediener bekaeme eine andere Messung als die
    // angeforderte.
    {
        const cli::Flag run  = cli::Find("--audioprobe --audioprobe-list", "--audioprobe");
        const cli::Flag self = cli::Find("--audioprobe --audioprobe-list", "--audioprobe-selftest");
        const cli::Flag list = cli::Find("--audioprobe --audioprobe-list", "--audioprobe-list");
        CHECK_EQ(cli::CountPresent({ &run, &self, &list }), 2);
    }
    {
        const cli::Flag run  = cli::Find("--audioprobe-selftest", "--audioprobe");
        const cli::Flag self = cli::Find("--audioprobe-selftest", "--audioprobe-selftest");
        const cli::Flag list = cli::Find("--audioprobe-selftest", "--audioprobe-list");
        CHECK_EQ(cli::CountPresent({ &run, &self, &list }), 1);
    }

    // Haltezeit je Ton: --audioprobe=<sek>, geklammert auf [1,20].
    CHECK_EQ(cli::HoldSeconds(cli::Find("--audioprobe", "--audioprobe"), 5, 1, 20), 5);
    CHECK_EQ(cli::HoldSeconds(cli::Find("--audioprobe=3", "--audioprobe"), 5, 1, 20), 3);
    CHECK_EQ(cli::HoldSeconds(cli::Find("--audioprobe=0", "--audioprobe"), 5, 1, 20), 1);
    CHECK_EQ(cli::HoldSeconds(cli::Find("--audioprobe=999", "--audioprobe"), 5, 1, 20), 20);
    // Muell faellt auf die Vorgabe zurueck, nicht auf 0: ein Ton von 0 s
    // Haltezeit zeigt weder der Rolle noch dem Menschen etwas.
    CHECK_EQ(cli::HoldSeconds(cli::Find("--audioprobe=laut", "--audioprobe"), 5, 1, 20), 5);

    // Der Endpunkt-Teilstring wird als Wert durchgereicht, auch mit Leerzeichen
    // in Anfuehrungszeichen - Endpunktnamen wie "Lautsprecher (Q18)" haben welche.
    {
        const cli::Flag ep = cli::Find("--audioprobe \"--audio-endpoint=Lautsprecher (Q18)\"",
                                       "--audio-endpoint");
        CHECK(ep.present);
        CHECK(ep.hasValue);
        CHECK_STR(ep.value, "Lautsprecher (Q18)");
    }
}

int main() {
    printf("OneClickRGB unit tests (cli_args.h, effect_limits.h, channel_config.h,\n");
    printf("keyboard_layout.h)\n");
    printf("no hardware involved - these cover the pure decision logic only\n");

    TestTokenize();
    TestFindExactToken();
    TestFindNoSubstringBleed();
    TestCountPresent();
    TestParseByte();
    TestHoldSeconds();
    TestClamps();
    TestEdgeModeTable();
    TestEdgeModeConfidence();
    TestProbeFlagResolution();
    TestRenderCheckAndConfirm();
    TestResolveChannelColor();
    TestParseRange16();
    TestParseLists();
    TestAsk();
    TestParseRgb();
    TestKeyLayoutGeometry();
    TestKeyEntryDecode();
    TestBuildLayout();
    TestBoardGeometry();
    TestKeyIdentifyArgs();
    TestAudioProbeArgs();

    printf("\n------------------------------------------------\n");
    if (g_failures == 0) {
        printf("PASS  %d checks, 0 failures\n", g_checks);
        return 0;
    }
    printf("FAIL  %d checks, %d failures\n", g_checks, g_failures);
    return 1;
}
