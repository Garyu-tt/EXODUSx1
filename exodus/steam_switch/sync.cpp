// SPDX-License-Identifier: GPL-2.0-only
#include "sync.hpp"

#include "../home/ulaunch/catalog_refresh.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <json.hpp>
#include <utility>

namespace exodus::steam_switch {

    namespace {

        using JSON = nlohmann::json;
        constexpr std::size_t SessionBodyLimit = 512;
        constexpr std::string_view SessionsPath = "/v1/steam/sessions";
        constexpr std::string_view SnapshotPath = "/v1/steam/catalog";
        constexpr std::string_view CancelPath = "/v1/steam/cancel";

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
        const auto proof = JSON{{"session", *session}, {"verifier", verifier}}.dump();
        const auto cancel = [&] {
            // Cleanup is best effort; a failed request leaves the expiry fallback
            // in place and must not change the original sign-in result.
            post(CancelPath, proof, SessionBodyLimit);
        };
        if (!open_login(origin.url + "/v1/steam/login/" + *session, origin.url + "/v1/steam/done")) {
            cancel();
            return {SyncStatus::BrowserUnavailable, {}};
        }

        auto fetched = post(SnapshotPath, proof, home::CatalogByteLimit);
        if (fetched.status == 409) {
            cancel();
            return {SyncStatus::SignInIncomplete, {}};
        }
        if (fetched.status == 404)
            return {SyncStatus::SessionExpired, {}};
        if (fetched.status != 200)
            return {SyncStatus::GatewayUnavailable, {}};
        if (!home::IsSteamCatalog(home::ParseCatalog(fetched.body)))
            return {SyncStatus::InvalidCatalog, {}};
        return {SyncStatus::Ready, std::move(fetched.body)};
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

}
