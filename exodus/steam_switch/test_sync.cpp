// SPDX-License-Identifier: GPL-2.0-only
#include "sync.hpp"

#include "../home/ulaunch/catalog.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <json.hpp>
#include <unistd.h>

namespace {

    using namespace exodus::steam_switch;
    using JSON = nlohmann::json;
    const std::string Challenge(64, 'a');
    const std::string Verifier(64, 'b');
    const std::string Session(64, 'c');

    JSON Entry(std::string provider, std::string id, std::string name) {
        return {{"provider", provider}, {"provider_id", id}, {"name", name}, {"artwork", nullptr},
                {"installation_state", "unknown"}, {"compatibility_state", "unknown"},
                {"last_played", nullptr}, {"launch_mode", nullptr}};
    }

    std::string Catalog(std::initializer_list<JSON> entries) {
        JSON array = JSON::array();
        for (const auto &entry : entries)
            array.push_back(entry);
        return JSON{{"schema_version", 1}, {"entries", array}}.dump();
    }

    void TestOrigins() {
        const auto origin = ParseGatewayOrigin("https://steam.example.test:8443/\r\n");
        assert(origin && origin->url == "https://steam.example.test:8443");
        assert(origin->hostname == "steam.example.test" && origin->port == "8443");
        const auto default_port = ParseGatewayOrigin("https://Steam.Example.Test:443/");
        assert(default_port && default_port->url == "https://steam.example.test" &&
               default_port->port == "443");
        for (const char *bad : {"http://steam.example.test", "https://", "https://host.example/path",
                               "https://user@host.example", "https://host.example?query=1", "https://host.example#url",
                               "https://host.example:abc", "https://host.example:0", "https://host.example:70000",
                               "https://host.example\r\nHost: other", "https://host..example", "https://host.example."}) {
            assert(!ParseGatewayOrigin(bad));
        }
    }

    void TestResponseHeaders() {
        const auto result = ParseHttpHeaders(
            "HTTP/1.1 201 Created\r\ncontent-Length: 12\r\nContent-Type: application/json", 30);
        assert(result && result->status == 201 && result->content_length == 12);
        assert(ParseHttpHeaders("HTTP/1.0 404 Not Found\r\nContent-Length: 0", 0));
        for (const char *bad : {"HTTP/2 200\r\nContent-Length: 1", "HTTP/1.0 100 Continue",
                               "HTTP/1.0 200 OK\r\nContent-Length: 100000000",
                               "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 1",
                               "HTTP/1.0 200 OK\r\nContent-Length: -1",
                               "HTTP/1.0 200 OK\r\nContent-Length : 1",
                               "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nTransfer-Encoding: chunked",
                               "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Encoding: gzip"}) {
            assert(!ParseHttpHeaders(bad, 1024));
        }
    }

    void TestSignIn() {
        const auto origin = ParseGatewayOrigin("https://steam.example.test");
        assert(origin);
        const std::string snapshot = Catalog({Entry("steam", "123", "Example Steam Game")});
        assert(exodus::home::ParseCatalog(snapshot).status == exodus::home::CatalogStatus::Ready);
        const std::string issued = JSON{{"session", Session}}.dump();
        const auto redeem = [&](int status, std::string body, bool opened = true, int cancel_status = 200) {
            int requests = 0;
            bool browser = false;
            bool cancelled = false;
            const bool abandoned = !opened || (status != 200 && status != 404);
            auto result = FetchSteamSnapshot(
                *origin, Challenge, Verifier,
                [&](std::string_view path, std::string_view request, std::size_t limit) {
                    const auto payload = JSON::parse(request);
                    if (++requests == 1) {
                        assert(path == "/v1/steam/sessions" && payload.size() == 1);
                        assert(payload["challenge"] == Challenge && limit == 512);
                        return HttpReply{201, issued};
                    }
                    assert(browser);
                    assert(payload.size() == 2 && payload["verifier"] == Verifier && payload["session"] == Session);
                    if (path == "/v1/steam/cancel") {
                        assert(!cancelled && abandoned);
                        assert(requests == (opened ? 3 : 2) && limit == 512);
                        cancelled = true;
                        return HttpReply{cancel_status, {}};
                    }
                    assert(opened && requests == 2 && path == "/v1/steam/catalog");
                    assert(limit == exodus::home::CatalogByteLimit);
                    return HttpReply{status, body};
                },
                [&](const std::string &url, const std::string &done) {
                    assert(requests == 1 && !browser);
                    assert(url == origin->url + "/v1/steam/login/" + Session);
                    assert(done == origin->url + "/v1/steam/done");
                    browser = true;
                    return opened;
                });
            assert(requests == (opened ? 2 : 1) + (abandoned ? 1 : 0) && browser);
            assert(cancelled == abandoned);
            if (abandoned)
                assert(result.catalog.empty());
            return result;
        };
        const auto success = redeem(200, snapshot);
        assert(success.status == SyncStatus::Ready && success.catalog == snapshot);
        assert(redeem(200, Catalog({})).status == SyncStatus::Ready);
        assert(redeem(404, {}).status == SyncStatus::SessionExpired);
        for (const int cleanup : {200, 404, 503, 0}) {
            assert(redeem(0, {}, false, cleanup).status == SyncStatus::BrowserUnavailable);
            assert(redeem(409, {}, true, cleanup).status == SyncStatus::SignInIncomplete);
            for (const int failure : {0, 302, 400, 403, 429, 500, 502, 503, 504})
                assert(redeem(failure, {}, true, cleanup).status == SyncStatus::GatewayUnavailable);
        }
        assert(redeem(200, "{invalid").status == SyncStatus::InvalidCatalog);
        assert(redeem(200, Catalog({Entry("switch", "9", "Not from Steam")})).status == SyncStatus::InvalidCatalog);
        for (const auto *id : {"0", "0123", "123x", "4294967296"}) {
            const auto unusable = redeem(200, Catalog({Entry("steam", id, "Invalid AppID")}));
            assert(unusable.status == SyncStatus::InvalidCatalog && unusable.catalog.empty());
        }
        const auto largest = Catalog({Entry("steam", "4294967295", "Largest valid AppID")});
        const auto largest_result = redeem(200, largest);
        assert(largest_result.status == SyncStatus::Ready && largest_result.catalog == largest);

        const auto invalid = "{\"session\":\"" + Session + "\",\"session\":\"" + Session + "\"}";
        const auto refused = FetchSteamSnapshot(
            *origin, Challenge, Verifier,
            [&](std::string_view path, std::string_view, std::size_t) {
                assert(path == "/v1/steam/sessions");
                return HttpReply{201, invalid};
            },
            [](const auto &, const auto &) { assert(false); return false; });
        assert(refused.status == SyncStatus::InvalidSession);
    }

    void TestGatewayFile() {
        char temporary[] = "build/exodus-steam-sync-test-XXXXXX";
        const auto directory_path = mkdtemp(temporary);
        assert(directory_path);
        const std::string directory(directory_path);
        const auto path = directory + "/gateway.txt";
        static const std::string settings = "https://steam.example.test:8443/\r\n";
        const auto write_settings = [&](std::string_view text) {
            auto *file = std::fopen(path.c_str(), "wb");
            assert(file);
            assert(std::fwrite(text.data(), 1, text.size(), file) == text.size());
            assert(std::fclose(file) == 0);
        };
        const auto utf16le = [](std::string_view text) {
            std::string encoded = "\xff\xfe";
            for (char ch : text) {
                encoded.push_back(ch);
                encoded.push_back('\0');
            }
            return encoded;
        };
        const auto expect_origin = [&](std::string_view text) {
            write_settings(text);
            const auto origin = ReadGatewayOrigin(path.c_str());
            assert(origin && origin->url == "https://steam.example.test:8443" &&
                   origin->hostname == "steam.example.test" && origin->port == "8443");
        };

        assert(!ReadGatewayOrigin((directory + "/missing").c_str()));
        expect_origin(settings);
        assert(ReadTextFile(path.c_str(), 16).status == ReadStatus::Error);
        assert(ReadTextFile((directory + "/missing").c_str(), 512).status == ReadStatus::Missing);
        const auto saved = ReadTextFile(path.c_str(), 512);
        assert(saved.status == ReadStatus::Ready && saved.text == settings);
        expect_origin("\xef\xbb\xbf" + settings);
        expect_origin(utf16le(settings));

        const std::string exact = settings + std::string(512 - settings.size(), ' ');
        expect_origin(exact);
        expect_origin("\xef\xbb\xbf" + exact);
        expect_origin(utf16le(exact));
        auto bad_unit = utf16le(settings);
        bad_unit[3] = '\x01';
        for (const auto &invalid : {exact + " ", "\xef\xbb\xbf" + exact + " ",
                                    utf16le(exact + " "), utf16le(settings) + "x", bad_unit,
                                    utf16le("https://steam.example.test/path"),
                                    utf16le("http://steam.example.test")}) {
            write_settings(invalid);
            assert(!ReadGatewayOrigin(path.c_str()));
        }
        assert(std::remove(path.c_str()) == 0);
        assert(rmdir(directory.c_str()) == 0);
    }

}

int main() {
    TestOrigins();
    TestResponseHeaders();
    TestSignIn();
    TestGatewayFile();
    std::puts("Switch Steam client flow, response framing, and gateway configuration checks passed.");
}
