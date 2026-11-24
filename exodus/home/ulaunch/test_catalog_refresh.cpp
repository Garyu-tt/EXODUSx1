// SPDX-License-Identifier: GPL-2.0-only
#include "catalog_refresh.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <json.hpp>
#include <thread>

namespace {

    using namespace exodus::home;
    using JSON = nlohmann::json;
    namespace fs = std::filesystem;

    struct Fixture {
        fs::path root;
        std::string base;
        std::string store;

        Fixture() {
            char temporary[] = "exodus-catalog-refresh-test-XXXXXX";
            const auto *created = mkdtemp(temporary);
            assert(created);
            root = fs::absolute(created);
            base = (root / "home.json").string();
            store = (root / "snapshots").string();
        }

        ~Fixture() {
            std::error_code error;
            fs::remove_all(root, error);
        }

        std::string Snapshot(std::uint64_t generation) const {
            const auto digits = std::to_string(generation);
            return store + "/steam-catalog-" + std::string(20 - digits.size(), '0') + digits + ".json";
        }

        Catalog Read() const {
            return ReadRefreshedCatalog(base.c_str(), store.c_str());
        }
    };

    void Write(const std::string &path, std::string_view bytes) {
        std::ofstream stream(path, std::ios::binary);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.close();
        assert(stream.good());
    }

    std::string Read(const std::string &path) {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    JSON EntryJson(std::string_view provider, std::string_view id, std::string_view title) {
        return {{"provider", provider}, {"provider_id", id}, {"name", title},
                {"artwork", nullptr}, {"installation_state", "unknown"},
                {"compatibility_state", "unknown"}, {"last_played", nullptr}, {"launch_mode", nullptr}};
    }

    std::string SnapshotJson(std::string_view title = "Signed-in Steam title") {
        return JSON{{"schema_version", 1}, {"entries", JSON::array({EntryJson("steam", "7", title)})}}.dump();
    }

    std::string Utf16Le(std::u16string_view text) {
        std::string encoded = "\xff\xfe";
        for (char16_t code : text) {
            encoded.push_back(static_cast<char>(code & 0xff));
            encoded.push_back(static_cast<char>((code >> 8) & 0xff));
        }
        return encoded;
    }

    void TestReopenAndEmptySnapshot() {
        Fixture files;
        const auto base = Utf16Le(
            uR"({"schema_version":1,"entries":[)"
            uR"({"provider":"steam","provider_id":"1","name":"Manual Steam","artwork":null,"installation_state":"unknown","compatibility_state":"unknown","last_played":null,"launch_mode":null},)"
            uR"({"provider":"switch","provider_id":"01002","name":"タイトル","artwork":null,"installation_state":"unknown","compatibility_state":"unknown","last_played":null,"launch_mode":null},)"
            uR"({"provider":"switch_homebrew","provider_id":"homebrew.nro","name":"Local homebrew","artwork":null,"installation_state":"unknown","compatibility_state":"unknown","last_played":null,"launch_mode":null}]})");
        Write(files.base, base);
        const auto before = files.Read();
        assert(before.status == CatalogStatus::Ready && before.entries.size() == 3);
        assert(before.entries[0].title == "Manual Steam");

        const auto first = PublishSteamSnapshot(files.store.c_str(), SnapshotJson());
        assert(first && *first == 1);
        const auto refreshed = files.Read();
        assert(refreshed.entries.size() == 3);
        assert(refreshed.entries[0].provider == Provider::Steam &&
               refreshed.entries[0].title == "Signed-in Steam title");
        assert(refreshed.entries[1].provider == Provider::Switch &&
               refreshed.entries[1].title == "タイトル");
        assert(refreshed.entries[2].provider == Provider::Homebrew &&
               refreshed.entries[2].title == "Local homebrew");

        Write(files.Snapshot(2), "{\"schema_version\":");
        Write(files.store + "/.steam-catalog-partial", SnapshotJson("Staged data"));
        auto fallback = files.Read();
        assert(fallback.entries.size() == 3 && fallback.entries[0].title == "Signed-in Steam title");
        const auto empty = PublishSteamSnapshot(files.store.c_str(), R"({"schema_version":1,"entries":[]})");
        assert(empty && *empty == 3);
        const auto without_steam = files.Read();
        assert(without_steam.entries.size() == 2);
        assert(without_steam.entries[0].provider == Provider::Switch);
        assert(without_steam.entries[1].provider == Provider::Homebrew);
        assert(Read(files.base) == base);
        assert(Read(files.Snapshot(2)) == "{\"schema_version\":");
    }

    void TestCatalogFallback() {
        Fixture files;
        assert(files.Read().status == CatalogStatus::Unavailable);
        assert(PublishSteamSnapshot(files.store.c_str(), SnapshotJson()) == 1);
        assert(files.Read().entries.size() == 1);

        auto non_steam = JSON{{"schema_version", 1}, {"entries", JSON::array({
            EntryJson("switch", "1", "Unexpected row")})}}.dump();
        assert(!PublishSteamSnapshot(files.store.c_str(), non_steam));
        Write(files.Snapshot(2), non_steam);
        Write(files.Snapshot(3), std::string(CatalogByteLimit + 1, ' '));
        assert(files.Read().entries[0].title == "Signed-in Steam title");

        Write(files.base, "corrupt base");
        assert(files.Read().status == CatalogStatus::Invalid);
        Write(files.Snapshot(UINT64_MAX), "occupied maximum generation");
        assert(!PublishSteamSnapshot(files.store.c_str(), SnapshotJson("New entry")));
        assert(Read(files.Snapshot(UINT64_MAX)) == "occupied maximum generation");
    }

    void TestStoreUnavailable() {
        Fixture files;
        const auto manual = SnapshotJson("Manual catalog");
        Write(files.base, manual);
        Write(files.store, "occupied store path");
        assert(!PublishSteamSnapshot(files.store.c_str(), SnapshotJson()));
        assert(files.Read().entries[0].title == "Manual catalog");
        assert(Read(files.base) == manual && Read(files.store) == "occupied store path");
    }

    void TestRetainsValidFallbacks() {
        Fixture files;
        assert(PublishSteamSnapshot(files.store.c_str(), SnapshotJson("Original")) == 1);
        Write(files.store + "/instructions.txt", "user notes");
        Write(files.store + "/.steam-catalog-unfinished", "{partial");
        Write(files.Snapshot(9), "{broken generation");
        assert(files.Read().entries[0].title == "Original");
        for (std::uint64_t generation = 10; generation < 20; ++generation) {
            assert(PublishSteamSnapshot(files.store.c_str(), SnapshotJson(std::to_string(generation))) ==
                   generation);
        }
        assert(!fs::exists(files.Snapshot(1)) && !fs::exists(files.Snapshot(11)));
        assert(fs::exists(files.Snapshot(12)) && fs::exists(files.Snapshot(19)));
        assert(Read(files.Snapshot(9)) == "{broken generation");
        assert(Read(files.store + "/instructions.txt") == "user notes");
        assert(Read(files.store + "/.steam-catalog-unfinished") == "{partial");
        Write(files.Snapshot(19), "truncated newest");
        assert(files.Read().entries[0].title == "18");
    }

    void TestMergedEntryLimit() {
        Fixture files;
        auto entries = JSON::array();
        for (std::size_t i = 0; i < CatalogEntryLimit; ++i)
            entries.push_back(EntryJson("switch", std::to_string(i), "Local title"));
        Write(files.base, JSON{{"schema_version", 1}, {"entries", entries}}.dump());
        assert(PublishSteamSnapshot(files.store.c_str(), R"({"schema_version":1,"entries":[]})") == 1);
        assert(PublishSteamSnapshot(files.store.c_str(), SnapshotJson()) == 2);
        const auto fallback = files.Read();
        assert(fallback.status == CatalogStatus::Ready && fallback.entries.size() == CatalogEntryLimit);
        assert(fallback.entries[0].provider == Provider::Switch);
    }

    void TestConcurrentPublish() {
        Fixture files;
        std::atomic<unsigned> arrived{0};
        std::array<std::optional<std::uint64_t>, 2> written;
        const auto publish = [&](std::size_t writer) {
            const auto data = SnapshotJson(std::to_string(writer));
            arrived.fetch_add(1);
            while (arrived.load() != written.size())
                std::this_thread::yield();
            written[writer] = PublishSteamSnapshot(files.store.c_str(), data);
        };
        std::thread first(publish, 0);
        std::thread second(publish, 1);
        first.join();
        second.join();
        assert(written[0] && written[1] && *written[0] != *written[1]);
        assert(std::min(*written[0], *written[1]) == 1 && std::max(*written[0], *written[1]) == 2);
        for (std::size_t writer = 0; writer < written.size(); ++writer) {
            const auto catalog = ReadCatalog(files.Snapshot(*written[writer]).c_str());
            assert(IsSteamCatalog(catalog) && catalog.entries.size() == 1);
            assert(catalog.entries[0].title == std::to_string(writer));
        }
        assert(IsSteamCatalog(files.Read()));
    }

}

int main() {
    TestReopenAndEmptySnapshot();
    TestCatalogFallback();
    TestStoreUnavailable();
    TestRetainsValidFallbacks();
    TestMergedEntryLimit();
    TestConcurrentPublish();
    std::cout << "HOME Steam refresh, fallback and generation checks passed\n";
}
