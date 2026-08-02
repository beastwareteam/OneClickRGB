/**
 * light_zone.h - Generic per-zone lighting model (device-independent)
 *
 * One LightZone = one independently addressable lighting unit (a mouse zone,
 * an Aura channel, a RAM module/LED, a keyboard segment/edge). The visual
 * layout editor and the per-zone adapters work against this single model so
 * every device is handled uniformly.
 *
 * Colour values are KNOWN (baked defaults / global colour) - nothing is read
 * back from vendor software at runtime.
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ZoneColor {
    uint8_t r = 0, g = 0, b = 0;
    json to_json() const { return json{{"r", r}, {"g", g}, {"b", b}}; }
    void from_json(const json& j) {
        r = (uint8_t)j.value("r", (int)r);
        g = (uint8_t)j.value("g", (int)g);
        b = (uint8_t)j.value("b", (int)b);
    }
};

// Full effect model, shaped after the SteelSeries GG lighting schema so the
// mouse (and later other devices) can express more than a single static colour.
// effectType 0 = static single colour (default); higher values = vendor effects.
struct ZoneEffect {
    int  effectType    = 0;          // 0=static, 1=gradient/colorshift, 2=reactive, ...
    int  speed         = 1000;
    int  numColors     = 1;
    bool hasDirection  = false;
    int  direction     = 0;
    int  triggerMask   = 0;
    std::vector<ZoneColor> colors;        // gradient stops
    std::vector<int>       positions;     // gradient stop positions (0..100)
    ZoneColor initialColor;
    ZoneColor reactColor;
    int       reactTime = 0;

    json to_json() const {
        json jc = json::array(); for (auto& c : colors)    jc.push_back(c.to_json());
        json jp = json::array(); for (auto& p : positions) jp.push_back(p);
        return json{
            {"effectType", effectType}, {"speed", speed}, {"numColors", numColors},
            {"hasDirection", hasDirection}, {"direction", direction},
            {"triggerMask", triggerMask}, {"colors", jc}, {"positions", jp},
            {"initialColor", initialColor.to_json()},
            {"reactColor", reactColor.to_json()}, {"reactTime", reactTime}
        };
    }
    void from_json(const json& j) {
        effectType   = j.value("effectType", effectType);
        speed        = j.value("speed", speed);
        numColors    = j.value("numColors", numColors);
        hasDirection = j.value("hasDirection", hasDirection);
        direction    = j.value("direction", direction);
        triggerMask  = j.value("triggerMask", triggerMask);
        reactTime    = j.value("reactTime", reactTime);
        colors.clear();
        if (j.contains("colors") && j["colors"].is_array())
            for (auto& c : j["colors"]) { ZoneColor zc; zc.from_json(c); colors.push_back(zc); }
        positions.clear();
        if (j.contains("positions") && j["positions"].is_array())
            for (auto& p : j["positions"]) positions.push_back(p.get<int>());
        if (j.contains("initialColor")) initialColor.from_json(j["initialColor"]);
        if (j.contains("reactColor"))   reactColor.from_json(j["reactColor"]);
    }
};

struct LightZone {
    std::string id;          // stable key, e.g. "wheel_lighting", "aura0", "ram0"
    std::string name;        // display name
    int  x = 0, y = 0;       // layout coordinates for the visual editor
    int  hwIndex = 0;        // hardware addressing (zone bit / channel / module)
    bool enabled = true;
    bool verified = false;   // true once Identify confirmed phys<->logical mapping
    ZoneColor color;         // base/static colour (always valid)
    ZoneEffect effect;       // advanced effect (effectType 0 => just use color)

    json to_json() const {
        return json{
            {"id", id}, {"name", name}, {"x", x}, {"y", y},
            {"hwIndex", hwIndex}, {"enabled", enabled}, {"verified", verified},
            {"color", color.to_json()}, {"effect", effect.to_json()}
        };
    }
    void from_json(const json& j) {
        id      = j.value("id", id);
        name    = j.value("name", name);
        x       = j.value("x", x);
        y       = j.value("y", y);
        hwIndex = j.value("hwIndex", hwIndex);
        enabled = j.value("enabled", enabled);
        verified= j.value("verified", verified);
        if (j.contains("color"))  color.from_json(j["color"]);
        if (j.contains("effect")) effect.from_json(j["effect"]);
    }
};

//-----------------------------------------------------------------------------
// Known default zone tables (colour values are known, not read from vendor SW)
//-----------------------------------------------------------------------------
namespace ZoneDefaults {

// SteelSeries Rival 600 - 8 zones (see docs/Mouse_Protocol.md).
// hwIndex = zone bit (1<<effect_index) used by the current SetSteelSeries path.
inline std::vector<LightZone> Rival600(ZoneColor base) {
    struct Z { const char* id; const char* name; int eff; int x, y; };
    static const Z t[8] = {
        { "wheel_lighting", "Scroll Wheel", 0,  85,  70 },
        { "logo_lighting",  "Logo",         1,  85, 270 },
        { "z2_lighting",    "Side Z2",      2,  50, 145 },
        { "z3_lighting",    "Side Z3",      3, 120, 145 },
        { "z4_lighting",    "Side Z4",      4,  45, 180 },
        { "z5_lighting",    "Side Z5",      5, 125, 180 },
        { "z6_lighting",    "Side Z6",      6,  25, 210 },
        { "z7_lighting",    "Side Z7",      7, 145, 210 },
    };
    std::vector<LightZone> v;
    for (auto& z : t) {
        LightZone lz;
        lz.id = z.id; lz.name = z.name; lz.hwIndex = (1 << z.eff);
        lz.x = z.x; lz.y = z.y; lz.color = base;
        v.push_back(lz);
    }
    return v;
}

} // namespace ZoneDefaults
