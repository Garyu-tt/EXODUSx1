// SPDX-License-Identifier: GPL-2.0-only
#include "catalog_steam_install.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

using exodus::home::ProbeLocalSteamInstallation;
using exodus::home::SteamInstallEvidence;
namespace fs = std::filesystem;

namespace {

    constexpr std::uint32_t AppId = 730;
    constexpr std::string_view Sample = "\xef\xbb\xbf" R"vdf(
// Steam app manifest
"AppState"
{
    "appid" "730"
    "name" "Example \"Title\""
    "installdir" "Valve Sample Ö"
    "StateFlags" "4"
    "InstalledDepots"
    {
        "731" { "manifest" "123456789" "size" "1000" "appid" "731" }
    }
    "UserConfig" { "language" "english" }
}
// End of manifest)vdf";

    [[noreturn]] void Fail(std::string_view message) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    class Fixture {
    public:
        Fixture() {
            auto path = (fs::temp_directory_path() / "exodus-steam-install-XXXXXX").string();
            if (mkdtemp(path.data()) == nullptr)
                Fail("Could not create Steam installation test directory");
            mount = path;
        }

        ~Fixture() {
            std::error_code ignored;
            fs::remove_all(mount, ignored);
        }

        fs::path mount;
        fs::path SteamApps() const { return mount / "switch/exodus-steam/steamapps"; }
        fs::path Manifest() const { return SteamApps() / "appmanifest_730.acf"; }
        fs::path Common() const { return SteamApps() / "common"; }
        fs::path Game() const { return Common() / "Valve Sample Ö"; }

        void Write(const fs::path &path, std::string_view content) const {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file.write(content.data(), static_cast<std::streamsize>(content.size())))
                Fail("Could not write Steam installation fixture");
        }

        void Check(SteamInstallEvidence expected, std::string_view message, std::uint32_t id = AppId) const {
            const auto result = ProbeLocalSteamInstallation(mount.string(), id);
            if (result != expected) {
                std::cerr << message << " expected " << static_cast<unsigned>(expected)
                    << " but got " << static_cast<unsigned>(result) << '\n';
                std::exit(EXIT_FAILURE);
            }
        }
    };

    std::string ManifestWith(std::string_view directory, std::string_view flags = "4") {
        return "AppState { appid 730 installdir \"" + std::string(directory) + "\" StateFlags "
            + std::string(flags) + " }";
    }

}

int main() {
    Fixture fixture;
    if (ProbeLocalSteamInstallation({}, AppId) != SteamInstallEvidence::Unavailable
        || ProbeLocalSteamInstallation((fixture.mount / "not-mounted").string(), AppId) != SteamInstallEvidence::Unavailable)
        Fail("Inactive SD mount should be unavailable");
    fixture.Check(SteamInstallEvidence::Unavailable, "AppID zero", 0);
    fixture.Check(SteamInstallEvidence::NoManifest, "No Steam library on the SD card");

    fs::create_directories(fixture.SteamApps());
    fixture.Write(fixture.SteamApps() / "appmanifest_731.acf", "AppState { appid 731 }");
    fixture.Check(SteamInstallEvidence::NoManifest, "A different AppID manifest is ignored");
    fixture.Write(fixture.Manifest(), Sample);
    fixture.Check(SteamInstallEvidence::MissingGameDirectory, "No common directory");

    fs::create_directories(fixture.Game());
    fixture.Check(SteamInstallEvidence::ManifestAndDirectoryFound, "BOM, nested depots, named local directory");
    fixture.Write(fixture.Manifest(), ManifestWith("Valve Sample Ö", "68"));
    fixture.Check(SteamInstallEvidence::ManifestAndDirectoryFound, "Fully installed bit alongside other flags");
    fixture.Write(fixture.Manifest(), ManifestWith("Valve Sample Ö", "0"));
    fixture.Check(SteamInstallEvidence::IncompleteManifest, "Directory exists while install is incomplete");
    fixture.Write(fixture.Manifest(), ManifestWith("Valve Sample Ö", "2"));
    fixture.Check(SteamInstallEvidence::IncompleteManifest, "Other state flags do not imply installation");

    for (const auto *text : {
        "",
        "AppState { appid 731 installdir Other StateFlags 4 }",
        "AppState { appid 0730 installdir Other StateFlags 4 }",
        "AppState { appid 730 AppID 730 installdir Other StateFlags 4 }",
        "AppState { appid 730 installdir Other InstallDir Other StateFlags 4 }",
        "AppState { appid 730 installdir Other StateFlags 4 stateflags 4 }",
        "AppState { appid 730 installdir { name Other } StateFlags 4 }",
        "AppState { appid 730 installdir Other }",
        "AppState { appid 730 installdir Other StateFlags 4294967296 }",
        "AppState { appid 730 installdir Other StateFlags +4 }",
        "AppState { appid 730 installdir Other StateFlags -4 }",
        "AppState { appid 730 installdir Other StateFlags 4x }",
        "AppState { appid 730 installdir Other StateFlags 4",
        "AppState { appid 730 installdir \"Other StateFlags 4 }",
        "AppState { appid 730 installdir Other StateFlags 4 } AppState { }",
        "AppState { appid 730 installdir Other StateFlags 4 depots { missing_value } }",
        "#include \"another.acf\" AppState { appid 730 installdir Other StateFlags 4 }",
        "AppState { appid 730 installdir \"\xed\xa0\x80\" StateFlags 4 }",
    }) {
        fixture.Write(fixture.Manifest(), text);
        fixture.Check(SteamInstallEvidence::InvalidManifest, text);
    }

    for (const auto *directory : {"", ".", "..", "other/path", "..\\escape", "sdmc:game", "extra.",
                                  "trailing space ", " leading space", "LPT1.exe", "NUL", "bad*name", "bad\nname"}) {
        fixture.Write(fixture.Manifest(), ManifestWith(directory));
        fixture.Check(SteamInstallEvidence::InvalidManifest, "Unsafe FAT directory component");
    }

    for (const auto *directory : {"CONIN$", "conout$", "ConIn$.dat", "CONOUT$  .log",
                                  "COM\xc2\xb9", "com\xc2\xb2.bin", "CoM\xc2\xb3 .data",
                                  "lpt\xc2\xb9", "LpT\xc2\xb2.ext", "LPT\xc2\xb3"}) {
        fs::create_directory(fixture.Common() / directory);
        fixture.Write(fixture.Manifest(), ManifestWith(directory));
        fixture.Check(SteamInstallEvidence::InvalidManifest, "Reserved device alias with an existing directory");
        fs::remove(fixture.Common() / directory);
    }

    for (unsigned control = 0x80; control <= 0x9f; ++control) {
        const auto directory = "Game" + std::string{static_cast<char>(0xc2), static_cast<char>(control)} + "Files";
        fs::create_directory(fixture.Common() / directory);
        fixture.Write(fixture.Manifest(), ManifestWith(directory));
        fixture.Check(SteamInstallEvidence::InvalidManifest, "C1 control in an existing game directory");
        fs::remove(fixture.Common() / directory);
    }

    for (const auto *directory : {"CONIN", "CONOUT", "CONSOLE", "COM0", "COM10", "LPT0", "LPT10",
                                  "COM\xc2\xb9" "Extra", "LPT\xc2\xb2" "0", "Game\xc2\xa0" "Files", "日本語"}) {
        fs::create_directory(fixture.Common() / directory);
        fixture.Write(fixture.Manifest(), ManifestWith(directory));
        fixture.Check(SteamInstallEvidence::ManifestAndDirectoryFound, "Valid Unicode or non-device near match");
        fs::remove(fixture.Common() / directory);
    }

    fixture.Write(fixture.Manifest(), ManifestWith(std::string(256, 'x')));
    fixture.Check(SteamInstallEvidence::InvalidManifest, "An overlong directory component");
    auto nul = ManifestWith("Valve Sample Ö");
    nul.append(1, '\0');
    fixture.Write(fixture.Manifest(), nul);
    fixture.Check(SteamInstallEvidence::InvalidManifest, "An embedded NUL is invalid");

    std::string depth = "AppState { appid 730 installdir Other StateFlags 4";
    for (int i = 0; i < 16; ++i)
        depth += " nested {";
    depth.append(17, '}');
    fixture.Write(fixture.Manifest(), depth);
    fixture.Check(SteamInstallEvidence::InvalidManifest, "Excessive KeyValues nesting");

    auto maximum = ManifestWith("Valve Sample Ö");
    maximum.resize(64 * 1024, ' ');
    fixture.Write(fixture.Manifest(), maximum);
    fixture.Check(SteamInstallEvidence::ManifestAndDirectoryFound, "Maximum manifest size is inclusive");
    maximum += ' ';
    fixture.Write(fixture.Manifest(), maximum);
    fixture.Check(SteamInstallEvidence::InvalidManifest, "Manifest size cap");

    fs::remove(fixture.Manifest());
    fs::create_directory(fixture.Manifest());
    fixture.Check(SteamInstallEvidence::InvalidManifest, "Manifest is a directory");
    fs::remove(fixture.Manifest());
    if (mkfifo(fixture.Manifest().c_str(), 0600) != 0)
        Fail("Could not create the nonregular-file fixture");
    fixture.Check(SteamInstallEvidence::InvalidManifest, "Manifest is a FIFO");
    fs::remove(fixture.Manifest());
    const auto foreign_manifest = fixture.mount / "foreign.acf";
    fixture.Write(foreign_manifest, Sample);
    fs::create_symlink(foreign_manifest, fixture.Manifest());
    fixture.Check(SteamInstallEvidence::InvalidManifest, "Manifest is a symlink");
    fs::remove(fixture.Manifest());
    fixture.Write(fixture.Manifest(), Sample);

    fs::remove(fixture.Game());
    fixture.Check(SteamInstallEvidence::MissingGameDirectory, "Stale manifest with removed game directory");
    fixture.Write(fixture.Game(), "not a directory");
    fixture.Check(SteamInstallEvidence::MissingGameDirectory, "Expected game path is a regular file");
    fs::remove(fixture.Game());
    const auto foreign_directory = fixture.mount / "foreign-directory";
    fs::create_directory(foreign_directory);
    fs::create_directory_symlink(foreign_directory, fixture.Game());
    fixture.Check(SteamInstallEvidence::Unavailable, "Expected game directory is a symlink");
    fs::remove(fixture.Game());
    fs::remove(fixture.Common());
    fs::create_directory_symlink(foreign_directory, fixture.Common());
    fixture.Check(SteamInstallEvidence::Unavailable, "Steam common directory is a symlink");

    const auto relocated = fixture.mount / "relocated-steamapps";
    fs::rename(fixture.SteamApps(), relocated);
    fs::create_directory_symlink(relocated, fixture.SteamApps());
    fixture.Check(SteamInstallEvidence::Unavailable, "Steam library directory is a symlink");
    fs::remove(fixture.SteamApps());
    fixture.Write(fixture.SteamApps(), "not a directory");
    fixture.Check(SteamInstallEvidence::Unavailable, "Steam library path is a regular file");

    std::cout << "Steam local installation evidence checks passed\n";
}
