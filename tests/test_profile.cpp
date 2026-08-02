/**
 * Tests for colour profile serialisation and storage.
 */

#include "../src/profile.h"
#include "../src/devices/evision.h"

#include <filesystem>
#include <fstream>

#include "test_framework.h"

namespace fs = std::filesystem;

namespace {

/// Unique scratch directory, removed when the test finishes.
class TempDir {
public:
    TempDir() {
        static int counter = 0;
        path_ = fs::temp_directory_path() /
                ("oneclickrgb_test_" + std::to_string(++counter));
        fs::remove_all(path_);
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    std::string str() const { return path_.string(); }

private:
    fs::path path_;
};

profile::Profile MakeDistinctProfile() {
    profile::Profile p;
    p.red = 12; p.green = 200; p.blue = 7;
    p.brightness = 3;
    p.speed = 5;
    p.kb_mode = 0x0C;
    p.edge_mode = 0x02;
    p.enable_aura = false;
    p.enable_mouse = true;
    p.enable_keyboard = false;
    p.enable_ram = true;
    p.enable_edge = false;
    return p;
}

}  // namespace

//=============================================================================
// Serialisation
//=============================================================================

TEST(profile, round_trip_preserves_every_field) {
    const profile::Profile original = MakeDistinctProfile();
    const profile::Profile parsed = profile::Parse(profile::Serialize(original));
    CHECK_MSG(parsed == original, "a field was lost in the serialise/parse cycle");
}

TEST(profile, defaults_survive_an_empty_document) {
    const profile::Profile parsed = profile::Parse("");
    CHECK(parsed == profile::Profile());
}

TEST(profile, unknown_keys_are_ignored) {
    const profile::Profile parsed = profile::Parse("red=5\nsomethingElse=9\nblue=6\n");
    CHECK_EQ((int)parsed.red, 5);
    CHECK_EQ((int)parsed.blue, 6);
}

TEST(profile, malformed_values_leave_the_default_instead_of_throwing) {
    // A truncated or hand-edited file must not take the application down.
    const profile::Profile parsed = profile::Parse("red=notanumber\ngreen=77\n");
    CHECK_EQ((int)parsed.red, (int)profile::Profile().red);
    CHECK_EQ((int)parsed.green, 77);
}

TEST(profile, values_outside_byte_range_are_clamped) {
    const profile::Profile parsed = profile::Parse("red=999\ngreen=-5\n");
    CHECK_EQ((int)parsed.red, 255);
    CHECK_EQ((int)parsed.green, 0);
}

TEST(profile, windows_line_endings_are_handled) {
    const profile::Profile parsed = profile::Parse("red=5\r\ngreen=6\r\n");
    CHECK_EQ((int)parsed.red, 5);
    CHECK_EQ((int)parsed.green, 6);
}

//=============================================================================
// Name validation
//=============================================================================

TEST(profile_names, ordinary_names_are_accepted) {
    CHECK(profile::IsValidName("Gaming"));
    CHECK(profile::IsValidName("Nacht 2"));
    CHECK(profile::IsValidName("profil-1_test"));
}

TEST(profile_names, path_traversal_is_rejected) {
    // The name comes from a free-text field; without this a profile could be
    // written anywhere on disk.
    CHECK(!profile::IsValidName("../evil"));
    CHECK(!profile::IsValidName("..\\evil"));
    CHECK(!profile::IsValidName("sub/dir"));
    CHECK(!profile::IsValidName("C:\\Windows\\x"));
}

TEST(profile_names, characters_windows_forbids_are_rejected) {
    for (const char* bad : {"a:b", "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b"})
        CHECK_MSG(!profile::IsValidName(bad), std::string("accepted: ") + bad);
}

TEST(profile_names, empty_and_overlong_names_are_rejected) {
    CHECK(!profile::IsValidName(""));
    CHECK(!profile::IsValidName(std::string(200, 'x')));
}

TEST(profile_names, trailing_dot_or_space_is_rejected) {
    // Windows strips these silently, so Save() and Load() would disagree about
    // which file the profile lives in.
    CHECK(!profile::IsValidName("name."));
    CHECK(!profile::IsValidName("name "));
}

//=============================================================================
// Storage
//=============================================================================

TEST(profile_storage, saved_profile_loads_back_identically) {
    TempDir dir;
    const profile::Profile original = MakeDistinctProfile();

    REQUIRE(profile::Save(dir.str(), "Testprofil", original));

    profile::Profile loaded;
    REQUIRE(profile::Load(dir.str(), "Testprofil", loaded));
    CHECK(loaded == original);
}

TEST(profile_storage, save_creates_a_missing_directory) {
    TempDir dir;
    const std::string nested = (fs::path(dir.str()) / "profiles").string();

    CHECK(profile::Save(nested, "Neu", profile::Profile()));
    CHECK(fs::exists(fs::path(nested) / "Neu.rgb"));
}

TEST(profile_storage, loading_an_absent_profile_fails_cleanly) {
    TempDir dir;
    profile::Profile p;
    CHECK(!profile::Load(dir.str(), "GibtEsNicht", p));
}

TEST(profile_storage, invalid_names_are_refused_at_the_storage_layer) {
    TempDir dir;
    profile::Profile p;
    CHECK(!profile::Save(dir.str(), "../escape", p));
    CHECK(!profile::Load(dir.str(), "../escape", p));
}

TEST(profile_storage, list_returns_sorted_names_without_the_extension) {
    TempDir dir;
    profile::Save(dir.str(), "Zulu", profile::Profile());
    profile::Save(dir.str(), "Alpha", profile::Profile());
    profile::Save(dir.str(), "Mike", profile::Profile());

    const auto names = profile::List(dir.str());
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "Alpha");
    CHECK(names[1] == "Mike");
    CHECK(names[2] == "Zulu");
}

TEST(profile_storage, list_ignores_foreign_files) {
    TempDir dir;
    profile::Save(dir.str(), "Echt", profile::Profile());
    std::ofstream(fs::path(dir.str()) / "notes.txt") << "hello";
    std::ofstream(fs::path(dir.str()) / "config.json") << "{}";

    const auto names = profile::List(dir.str());
    REQUIRE(names.size() == 1);
    CHECK(names[0] == "Echt");
}

TEST(profile_storage, list_of_a_missing_directory_is_empty) {
    const auto names = profile::List("Z:\\does\\not\\exist\\at\\all");
    CHECK_EQ((int)names.size(), 0);
}

TEST(profile_storage, a_truncated_file_still_yields_a_usable_profile) {
    TempDir dir;
    std::ofstream(fs::path(dir.str()) / "Kaputt.rgb") << "red=42\ngreen=";

    profile::Profile loaded;
    REQUIRE(profile::Load(dir.str(), "Kaputt", loaded));
    CHECK_EQ((int)loaded.red, 42);
    CHECK_EQ((int)loaded.green, (int)profile::Profile().green);
}

TEST(profile, an_effect_mode_the_device_does_not_implement_is_replaced) {
    // Profiles saved before the index/value confusion was fixed carry a combo
    // index. Loading one used to put it straight on the wire: edgeMode 0 is
    // FREEZE, which the UI never offered and which leaves the side lighting
    // stuck.
    const auto p = profile::Parse("kbMode=0\nedgeMode=0\n");
    CHECK_EQ((int)p.kb_mode, (int)devices::evision::KB_MODE_STATIC);
    CHECK_EQ((int)p.edge_mode, (int)devices::evision::EDGE_MODE_STATIC);
}

TEST(profile, a_valid_effect_mode_survives_the_round_trip) {
    const auto p = profile::Parse("kbMode=12\nedgeMode=5\n");
    CHECK_EQ((int)p.kb_mode, (int)devices::evision::KB_MODE_RAINBOW);
    CHECK_EQ((int)p.edge_mode, (int)devices::evision::EDGE_MODE_OFF);
}
