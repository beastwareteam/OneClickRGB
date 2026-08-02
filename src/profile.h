/**
 * profile.h - Colour profile serialisation
 *
 * Split out of the main window code so the round trip can be tested without a
 * message loop. Parsing and serialisation work on strings; file access is a
 * thin layer on top that takes the directory as an argument rather than
 * reaching for %APPDATA% itself.
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace profile {

/// File extension profiles are stored under.
constexpr const char* EXTENSION = ".rgb";

struct Profile {
    uint8_t red = 0, green = 34, blue = 255;
    uint8_t brightness = 4;
    uint8_t speed = 2;
    uint8_t kb_mode = 0x06;   // evision::KB_MODE_STATIC
    uint8_t edge_mode = 0x04; // evision::EDGE_MODE_STATIC

    bool enable_aura = true;
    bool enable_mouse = true;
    bool enable_keyboard = true;
    bool enable_ram = true;
    bool enable_edge = true;

    bool operator==(const Profile& o) const;
    bool operator!=(const Profile& o) const { return !(*this == o); }
};

/// Renders a profile as the key=value text stored on disk.
std::string Serialize(const Profile& p);

/// Parses profile text. Unknown keys are ignored and malformed values leave the
/// corresponding field at its default, so a partially corrupted file still
/// yields a usable profile rather than throwing.
Profile Parse(const std::string& text);

/// Rejects names that would escape the profile directory or break the
/// filesystem. Profile names come from a free-text field in the UI.
bool IsValidName(const std::string& name);

/// Directory is created if missing. Returns false on invalid name or I/O error.
bool Save(const std::string& dir, const std::string& name, const Profile& p);

/// Returns false when the profile does not exist or the name is invalid.
bool Load(const std::string& dir, const std::string& name, Profile& out);

/// Profile names present in `dir`, without the extension, sorted.
std::vector<std::string> List(const std::string& dir);

}  // namespace profile
