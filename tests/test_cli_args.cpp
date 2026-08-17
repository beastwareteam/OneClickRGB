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
}

//-----------------------------------------------------------------------------
int main() {
    printf("OneClickRGB unit tests (cli_args.h, effect_limits.h, channel_config.h)\n");
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

    printf("\n------------------------------------------------\n");
    if (g_failures == 0) {
        printf("PASS  %d checks, 0 failures\n", g_checks);
        return 0;
    }
    printf("FAIL  %d checks, %d failures\n", g_checks, g_failures);
    return 1;
}
