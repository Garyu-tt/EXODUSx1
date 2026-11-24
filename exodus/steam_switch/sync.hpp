// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace exodus::steam_switch {

    struct GatewayOrigin {
        std::string url;
        std::string authority;
        std::string hostname;
        std::string port;
    };

    struct HttpReply {
        int status = 0;
        std::string body;
    };

    struct HttpHeaders {
        int status;
        std::size_t content_length;
    };

    enum class SyncStatus {
        Ready,
        GatewayUnavailable,
        InvalidSession,
        BrowserUnavailable,
        SignInIncomplete,
        SessionExpired,
        InvalidCatalog,
    };

    struct SyncResult {
        SyncStatus status;
        std::string catalog;
    };

    enum class ReadStatus { Ready, Missing, Error };

    struct TextFile {
        ReadStatus status;
        std::string text;
    };

    using PostRequest = std::function<HttpReply(std::string_view, std::string_view, std::size_t)>;
    using OpenLogin = std::function<bool(const std::string &, const std::string &)>;

    std::optional<GatewayOrigin> ParseGatewayOrigin(std::string_view text);
    std::optional<HttpHeaders> ParseHttpHeaders(std::string_view headers, std::size_t body_limit);
    SyncResult FetchSteamSnapshot(const GatewayOrigin &origin, std::string_view challenge,
                                  std::string_view verifier, const PostRequest &post,
                                  const OpenLogin &open_login);
    std::optional<std::string> MergeSteamSnapshot(std::string_view steam,
                                                  std::optional<std::string_view> base);
    TextFile ReadTextFile(const char *path, std::size_t limit);
    std::optional<std::string> SaveNewSnapshot(const std::string &directory, std::string_view catalog);

#ifdef __SWITCH__
    HttpReply PostHttps(const GatewayOrigin &origin, std::string_view path,
                        std::string_view body, std::size_t body_limit);
#endif

}
