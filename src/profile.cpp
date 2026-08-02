#include "profile.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace profile {

namespace fs = std::filesystem;

bool Profile::operator==(const Profile& o) const {
    return red == o.red && green == o.green && blue == o.blue &&
           brightness == o.brightness && speed == o.speed &&
           kb_mode == o.kb_mode && edge_mode == o.edge_mode &&
           enable_aura == o.enable_aura && enable_mouse == o.enable_mouse &&
           enable_keyboard == o.enable_keyboard && enable_ram == o.enable_ram &&
           enable_edge == o.enable_edge;
}

std::string Serialize(const Profile& p) {
    std::ostringstream out;
    out << "red="            << (int)p.red             << "\n"
        << "green="          << (int)p.green           << "\n"
        << "blue="           << (int)p.blue            << "\n"
        << "brightness="     << (int)p.brightness      << "\n"
        << "speed="          << (int)p.speed           << "\n"
        << "kbMode="         << (int)p.kb_mode         << "\n"
        << "edgeMode="       << (int)p.edge_mode       << "\n"
        << "enableAura="     << (p.enable_aura     ? 1 : 0) << "\n"
        << "enableMouse="    << (p.enable_mouse    ? 1 : 0) << "\n"
        << "enableKeyboard=" << (p.enable_keyboard ? 1 : 0) << "\n"
        << "enableRAM="      << (p.enable_ram      ? 1 : 0) << "\n"
        << "enableEdge="     << (p.enable_edge     ? 1 : 0) << "\n";
    return out.str();
}

namespace {

/// Parses an integer, returning false rather than throwing on garbage.
bool ParseInt(const std::string& s, int& out) {
    try {
        size_t consumed = 0;
        int value = std::stoi(s, &consumed);
        if (consumed == 0) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

uint8_t ClampByte(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<uint8_t>(v);
}

}  // namespace

Profile Parse(const std::string& text) {
    Profile p;
    std::istringstream in(text);
    std::string line;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        const std::string key = line.substr(0, eq);
        int value = 0;
        if (!ParseInt(line.substr(eq + 1), value)) continue;

        if      (key == "red")            p.red             = ClampByte(value);
        else if (key == "green")          p.green           = ClampByte(value);
        else if (key == "blue")           p.blue            = ClampByte(value);
        else if (key == "brightness")     p.brightness      = ClampByte(value);
        else if (key == "speed")          p.speed           = ClampByte(value);
        else if (key == "kbMode")         p.kb_mode         = ClampByte(value);
        else if (key == "edgeMode")       p.edge_mode       = ClampByte(value);
        else if (key == "enableAura")     p.enable_aura     = value != 0;
        else if (key == "enableMouse")    p.enable_mouse    = value != 0;
        else if (key == "enableKeyboard") p.enable_keyboard = value != 0;
        else if (key == "enableRAM")      p.enable_ram      = value != 0;
        else if (key == "enableEdge")     p.enable_edge     = value != 0;
    }
    return p;
}

bool IsValidName(const std::string& name) {
    if (name.empty() || name.size() > 64) return false;
    if (name.find("..") != std::string::npos) return false;
    for (unsigned char c : name) {
        if (c < 0x20) return false;
        if (std::strchr("\\/:*?\"<>|", c) != nullptr) return false;
    }
    // Trailing dots and spaces are silently stripped by Windows, which would
    // make Save() and Load() disagree about the filename.
    if (name.back() == '.' || name.back() == ' ') return false;
    return true;
}

namespace {

fs::path ProfilePath(const std::string& dir, const std::string& name) {
    return fs::path(dir) / (name + EXTENSION);
}

}  // namespace

bool Save(const std::string& dir, const std::string& name, const Profile& p) {
    if (!IsValidName(name)) return false;

    std::error_code ec;
    fs::create_directories(dir, ec);

    std::ofstream f(ProfilePath(dir, name));
    if (!f) return false;
    f << Serialize(p);
    return static_cast<bool>(f);
}

bool Load(const std::string& dir, const std::string& name, Profile& out) {
    if (!IsValidName(name)) return false;

    std::ifstream f(ProfilePath(dir, name));
    if (!f) return false;

    std::ostringstream buffer;
    buffer << f.rdbuf();
    out = Parse(buffer.str());
    return true;
}

std::vector<std::string> List(const std::string& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return names;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != EXTENSION) continue;
        names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace profile
