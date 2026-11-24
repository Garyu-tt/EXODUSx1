// SPDX-License-Identifier: GPL-2.0-only
#include "sync.hpp"

#include "../home/ulaunch/catalog.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <json.hpp>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace exodus::steam_switch {

    namespace {

        using JSON = nlohmann::json;
        constexpr std::size_t SessionBodyLimit = 512;
        constexpr std::string_view SessionsPath = "/v1/steam/sessions";
        constexpr std::string_view SnapshotPath = "/v1/steam/catalog";

        std::string_view Trim(std::string_view text, std::string_view characters) {
            const auto start = text.find_first_not_of(characters);
            if (start == text.npos)
                return {};
            return text.substr(start, text.find_last_not_of(characters) - start + 1);
        }

        bool IsAsciiLetter(char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        }

        bool IsDigit(char ch) {
            return ch >= '0' && ch <= '9';
        }

        bool IsAsciiAlnum(char ch) {
            return IsAsciiLetter(ch) || IsDigit(ch);
        }

        char LowerAscii(char ch) {
            return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
        }

        bool IsToken(std::string_view text) {
            return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char ch) {
                return IsDigit(ch) || (ch >= 'a' && ch <= 'f');
            });
        }

        bool IsHostName(std::string_view host) {
            if (host.size() > 253 || host.find('.') == host.npos)
                return false;
            while (true) {
                const auto separator = host.find('.');
                const auto label = host.substr(0, separator);
                if (label.empty() || label.size() > 63 || !IsAsciiAlnum(label.front()) ||
                    !IsAsciiAlnum(label.back()) ||
                    !std::all_of(label.begin(), label.end(), [](char ch) {
                        return IsAsciiAlnum(ch) || ch == '-';
                    }))
                    return false;
                if (separator == host.npos)
                    return true;
                host.remove_prefix(separator + 1);
            }
        }

        template<class Number>
        bool ParseNumber(std::string_view text, Number &number) {
            if (text.empty())
                return false;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }

        bool EqualsIgnoringCase(std::string_view left, std::string_view right) {
            if (left.size() != right.size())
                return false;
            return std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
                return LowerAscii(a) == b;
            });
        }

        std::optional<std::string> ReadSession(std::string_view text) {
            if (text.size() > SessionBodyLimit || text.find('\0') != text.npos)
                return std::nullopt;
            std::size_t fields = 0;
            const auto document = JSON::parse(text, [&](int, JSON::parse_event_t event, JSON &) {
                if (event == JSON::parse_event_t::key)
                    ++fields;
                return true;
            }, false);
            if (!document.is_object() || fields != 1 || !document.contains("session"))
                return std::nullopt;
            const auto &session = document["session"];
            if (!session.is_string() || !IsToken(session.get_ref<const std::string &>()))
                return std::nullopt;
            return session.get<std::string>();
        }

        bool IsSteamSnapshot(std::string_view text, std::string *normalized_utf8 = nullptr) {
            const auto parsed = home::ParseCatalog(text, normalized_utf8);
            return parsed.status == home::CatalogStatus::Ready &&
                   std::all_of(parsed.entries.begin(), parsed.entries.end(), [](const auto &entry) {
                       return entry.provider == home::Provider::Steam;
                   });
        }

    }

    std::optional<GatewayOrigin> ParseGatewayOrigin(std::string_view text) {
        text = Trim(text, " \t\r\n");
        constexpr std::string_view scheme = "https://";
        if (text.substr(0, scheme.size()) != scheme)
            return std::nullopt;
        text.remove_prefix(scheme.size());
        if (!text.empty() && text.back() == '/')
            text.remove_suffix(1);
        if (text.empty())
            return std::nullopt;

        const auto delimiter = text.find(':');
        const auto host = text.substr(0, delimiter);
        if (!IsHostName(host))
            return std::nullopt;

        unsigned port_number = 443;
        if (delimiter != text.npos) {
            const auto suffix = text.substr(delimiter + 1);
            if (suffix.size() > 5 || !ParseNumber(suffix, port_number) ||
                port_number == 0 || port_number > 65535)
                return std::nullopt;
        }

        std::string hostname(host);
        std::transform(hostname.begin(), hostname.end(), hostname.begin(), LowerAscii);
        std::string authority(hostname);
        const auto port = std::to_string(port_number);
        if (port_number != 443)
            authority += ":" + port;
        return GatewayOrigin{std::string(scheme) + authority, authority, hostname, port};
    }

    std::optional<HttpHeaders> ParseHttpHeaders(std::string_view headers, std::size_t body_limit) {
        if (headers.find('\0') != headers.npos)
            return std::nullopt;
        const auto first_end = headers.find("\r\n");
        auto status_line = headers.substr(0, first_end);
        int status = 0;
        if (status_line.size() < 12 || (status_line.substr(0, 9) != "HTTP/1.0 " &&
            status_line.substr(0, 9) != "HTTP/1.1 ") ||
            !ParseNumber(status_line.substr(9, 3), status) || status < 200 || status > 599 ||
            (status_line.size() > 12 && status_line[12] != ' '))
            return std::nullopt;

        std::optional<std::size_t> length;
        while (headers.find("\r\n") != headers.npos) {
            headers.remove_prefix(headers.find("\r\n") + 2);
            const auto line = headers.substr(0, headers.find("\r\n"));
            const auto separator = line.find(':');
            if (separator == 0 || separator == line.npos)
                return std::nullopt;
            const auto name = line.substr(0, separator);
            if (!std::all_of(name.begin(), name.end(), [](char ch) {
                return IsAsciiLetter(ch) || IsDigit(ch) ||
                       std::string_view("!#$%&'*+-.^`_|~").find(ch) != std::string_view::npos;
            }))
                return std::nullopt;
            const auto value = Trim(line.substr(separator + 1), " \t");
            if (value.find_first_of("\r\n") != value.npos || EqualsIgnoringCase(name, "transfer-encoding") ||
                (EqualsIgnoringCase(name, "content-encoding") && !EqualsIgnoringCase(value, "identity")))
                return std::nullopt;
            if (EqualsIgnoringCase(name, "content-length")) {
                std::size_t bytes = 0;
                if (length || !ParseNumber(value, bytes) || bytes > body_limit)
                    return std::nullopt;
                length = bytes;
            }
        }
        if (!length)
            return std::nullopt;
        return HttpHeaders{status, *length};
    }

    SyncResult FetchSteamSnapshot(const GatewayOrigin &origin, std::string_view challenge,
                                  std::string_view verifier, const PostRequest &post,
                                  const OpenLogin &open_login) {
        if (!IsToken(challenge) || !IsToken(verifier))
            return {SyncStatus::InvalidSession, {}};
        const auto created = post(SessionsPath, JSON{{"challenge", challenge}}.dump(), SessionBodyLimit);
        if (created.status != 201)
            return {SyncStatus::GatewayUnavailable, {}};
        const auto session = ReadSession(created.body);
        if (!session)
            return {SyncStatus::InvalidSession, {}};
        if (!open_login(origin.url + "/v1/steam/login/" + *session, origin.url + "/v1/steam/done"))
            return {SyncStatus::BrowserUnavailable, {}};

        auto fetched = post(SnapshotPath, JSON{{"session", *session}, {"verifier", verifier}}.dump(),
                            home::CatalogByteLimit);
        if (fetched.status == 409)
            return {SyncStatus::SignInIncomplete, {}};
        if (fetched.status == 404)
            return {SyncStatus::SessionExpired, {}};
        if (fetched.status != 200)
            return {SyncStatus::GatewayUnavailable, {}};
        if (!IsSteamSnapshot(fetched.body))
            return {SyncStatus::InvalidCatalog, {}};
        return {SyncStatus::Ready, std::move(fetched.body)};
    }

    std::optional<std::string> MergeSteamSnapshot(std::string_view steam,
                                                  std::optional<std::string_view> base) {
        std::string incoming_utf8;
        if (!IsSteamSnapshot(steam, base ? &incoming_utf8 : nullptr))
            return std::nullopt;
        if (!base)
            return std::string(steam);
        std::string existing_utf8;
        if (home::ParseCatalog(*base, &existing_utf8).status != home::CatalogStatus::Ready)
            return std::nullopt;

        auto incoming = JSON::parse(incoming_utf8, nullptr, false);
        auto existing = JSON::parse(existing_utf8, nullptr, false);
        auto &entries = incoming["entries"];
        for (auto &entry : existing["entries"]) {
            if (entry["provider"] == "steam")
                continue;
            if (entries.size() == home::CatalogEntryLimit)
                return std::nullopt;
            entries.push_back(std::move(entry));
        }
        auto result = incoming.dump();
        if (result.size() >= home::CatalogByteLimit)
            return std::nullopt;
        result.push_back('\n');
        return result;
    }

    TextFile ReadTextFile(const char *path, std::size_t limit) {
        auto *input = std::fopen(path, "rb");
        if (!input)
            return {errno == ENOENT ? ReadStatus::Missing : ReadStatus::Error, {}};
        std::array<char, 4096> buffer{};
        std::string text;
        bool good = true;
        while (good) {
            const auto count = std::fread(buffer.data(), 1, buffer.size(), input);
            good = !std::ferror(input) && count <= limit - text.size();
            if (!good)
                break;
            text.append(buffer.data(), count);
            if (count < buffer.size())
                break;
        }
        const auto closed = std::fclose(input);
        if (!good || closed != 0)
            return {ReadStatus::Error, {}};
        return {ReadStatus::Ready, std::move(text)};
    }

    std::optional<std::string> SaveNewSnapshot(const std::string &directory, std::string_view catalog) {
        if (home::ParseCatalog(catalog).status != home::CatalogStatus::Ready)
            return std::nullopt;
        const std::string pattern = directory + "/.steam-sync-XXXXXX";
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back('\0');
        const int fd = mkstemp(name.data());
        if (fd < 0)
            return std::nullopt;
        const std::string staged(name.data());
        const std::string filename = "exodus-catalog-" + staged.substr(staged.size() - 6) + ".json";
        const std::string path = directory + "/" + filename;

        FILE *output = fdopen(fd, "wb");
        if (!output) {
            close(fd);
            std::remove(staged.c_str());
            return std::nullopt;
        }
        const bool synced = std::fwrite(catalog.data(), 1, catalog.size(), output) == catalog.size() &&
                            std::fflush(output) == 0 && fsync(fd) == 0;
        const auto closed = std::fclose(output);
        bool published = false;
        if (synced && closed == 0) {
#ifdef __SWITCH__
            // The SD service refuses to rename over an existing file.
            if (std::rename(staged.c_str(), path.c_str()) == 0)
                return filename;
#else
            // POSIX rename can overwrite the destination so use an exclusive link on the host.
            published = link(staged.c_str(), path.c_str()) == 0;
#endif
        }
        std::remove(staged.c_str());
        if (!published)
            return std::nullopt;
        return filename;
    }

}
