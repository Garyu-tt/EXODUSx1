// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string>

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *providers[] = {"steam", "switch", "switch_homebrew", "corkdeck", "other"};
    const char *labels[] = {"Steam", "Switch", "Homebrew", "CorkDeck", "Other"};
    {
        std::ofstream f(argv[1]);
        f << R"({"schema_version":1,"entries":[)";
        for (int i = 0; i < 5; ++i) {
            if (i) f << ',';
            f << R"({"provider":")" << providers[i] << R"(","provider_id":"1","name":"Game","artwork":null,"installation_state":"unknown","last_played":null,"compatibility_state":"unknown","launch_mode":null})";
        }
        f << "]}";
        assert(f.good());
    }
    auto *lib = exodus_library_open(argv[1]);
    assert(lib && exodus_library_count(lib) == 5);
    for (size_t i = 0; i < 5; ++i) {
        assert(!strcmp(exodus_library_provider(lib, i), labels[i]));
        assert(!strcmp(exodus_library_title(lib, i), "Game"));
    }
    assert(!strcmp(exodus_library_title(lib, 5), ""));
    assert(!strcmp(exodus_library_provider(lib, 5), "Unknown"));
    exodus_library_close(lib);
    // Folder discovery is read-only, and even an NRO suffix is not proof of launchability.
    const std::string root = std::string(argv[1]) + ".games";
    assert(std::filesystem::create_directory(root));
    const std::string game = root + "/Game.nsp";
    const std::string homebrew = root + "/Test.NRO";
    { std::ofstream f(game); f << "fixture"; }
    { std::ofstream f(homebrew); f << "fixture"; }
    { std::ofstream f(root + "/partial.xci.part"); f << "fixture"; }
    lib = exodus_library_open_with_games(argv[1], root.c_str());
    assert(lib && exodus_library_count(lib) == 7);
    assert(!strcmp(exodus_library_discovery_status(lib), "Games scan complete"));
    for (size_t i = 5; i < 7; ++i) {
        assert(!strcmp(exodus_library_provider(lib, i), "Other"));
        const std::string id = exodus_library_id(lib, i);
        assert(id == game || id == homebrew);
    }
    assert(strcmp(exodus_library_id(lib, 5), exodus_library_id(lib, 6)));
    exodus_library_close(lib);
    assert(std::filesystem::file_size(game) == 7);
    lib = exodus_library_open_with_games(argv[1], (root + "/missing").c_str());
    assert(lib && exodus_library_count(lib) == 5);
    assert(!strcmp(exodus_library_discovery_status(lib), "Games folder unavailable"));
    exodus_library_close(lib);
    assert(std::filesystem::remove_all(root) == 4);
    { std::ofstream f(argv[1]); f << "{}"; }
    lib = exodus_library_open(argv[1]);
    assert(lib && !strcmp(exodus_library_status(lib), "Invalid catalog"));
    assert(exodus_library_count(lib) == 0);
    exodus_library_close(lib);
    std::remove(argv[1]);
    return 0;
}
