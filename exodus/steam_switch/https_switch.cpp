// SPDX-License-Identifier: GPL-2.0-only
#include "sync.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <switch.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace exodus::steam_switch {

    namespace {

        using Clock = std::chrono::steady_clock;
        using namespace std::chrono_literals;
        constexpr std::size_t HeaderLimit = 8192;
        constexpr Result SslWouldBlock = MAKERESULT(123, 204);

        int RemainingMilliseconds(Clock::time_point deadline) {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
            return remaining > 0ms ? static_cast<int>(remaining.count()) : 0;
        }

        int ConnectSocket(const GatewayOrigin &origin, Clock::time_point deadline) {
            addrinfo hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo *addresses = nullptr;
            if (getaddrinfo(origin.hostname.c_str(), origin.port.c_str(), &hints, &addresses) != 0)
                return -1;
            int connected = -1;
            for (auto *address = addresses; address && RemainingMilliseconds(deadline) > 0;
                 address = address->ai_next) {
                const int fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
                if (fd < 0)
                    continue;
                const auto flags = fcntl(fd, F_GETFL);
                if (flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0) {
                    if (connect(fd, address->ai_addr, address->ai_addrlen) == 0) {
                        connected = fd;
                    } else if (errno == EINPROGRESS) {
                        pollfd waiting{fd, POLLOUT, 0};
                        int socket_error = 0;
                        socklen_t length = sizeof(socket_error);
                        if (poll(&waiting, 1, RemainingMilliseconds(deadline)) > 0 &&
                            (waiting.revents & POLLOUT) &&
                            getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &length) == 0 &&
                            socket_error == 0)
                            connected = fd;
                    }
                }
                if (connected >= 0)
                    break;
                close(fd);
            }
            freeaddrinfo(addresses);
            return connected;
        }

        struct TlsConnection {
            int socket = -1;
            SslContext context{};
            SslConnection connection{};
            bool has_context = false;
            bool has_connection = false;

            ~TlsConnection() {
                if (has_connection)
                    sslConnectionClose(&connection);
                if (has_context)
                    sslContextClose(&context);
                if (socket >= 0)
                    close(socket);
            }
        };

        bool WaitForSsl(SslConnection &connection, u32 event, Clock::time_point deadline) {
            const auto timeout = RemainingMilliseconds(deadline);
            if (timeout == 0)
                return false;
            u32 ready = 0;
            return R_SUCCEEDED(sslConnectionPoll(&connection, event, &ready, timeout)) &&
                   (ready & event) != 0;
        }

        bool EstablishTls(TlsConnection &tls, const GatewayOrigin &origin,
                          Clock::time_point deadline) {
            tls.socket = ConnectSocket(origin, deadline);
            if (tls.socket < 0)
                return false;

            u32 versions = SslVersion_TlsV12;
            if (hosversionAtLeast(11, 0, 0))
                versions |= SslVersion_TlsV13;
            tls.has_context = R_SUCCEEDED(sslCreateContext(&tls.context, versions));
            if (!tls.has_context)
                return false;
            auto &connection = tls.connection;
            tls.has_connection = R_SUCCEEDED(sslContextCreateConnection(&tls.context, &connection));
            if (!tls.has_connection ||
                R_FAILED(sslConnectionSetOption(&connection, SslOptionType_DoNotCloseSocket, true)))
                return false;

            // The wrapper reports ENOENT when the system takes no ownership of the socket.
            const int managed_socket = socketSslConnectionSetSocketDescriptor(&connection, tls.socket);
            if (managed_socket >= 0) {
                close(managed_socket);
                return false;
            }
            if (errno != ENOENT)
                return false;
            constexpr auto verify = SslVerifyOption_PeerCa | SslVerifyOption_HostName |
                                    SslVerifyOption_DateCheck;
            if (R_FAILED(sslConnectionSetHostName(&connection, origin.hostname.c_str(),
                                                  static_cast<u32>(origin.hostname.size()))) ||
                R_FAILED(sslConnectionSetVerifyOption(&connection, verify)) ||
                R_FAILED(sslConnectionSetIoMode(&connection, SslIoMode_NonBlocking)))
                return false;

            while (RemainingMilliseconds(deadline) > 0) {
                const auto result = sslConnectionDoHandshake(&connection, nullptr, nullptr, nullptr, 0);
                if (R_SUCCEEDED(result))
                    return true;
                if (R_VALUE(result) != SslWouldBlock ||
                    !WaitForSsl(connection, SslPollEvent_Read, deadline))
                    return false;
            }
            return false;
        }

        bool WriteRequest(SslConnection &connection, std::string_view request,
                          Clock::time_point deadline) {
            while (!request.empty() && RemainingMilliseconds(deadline) > 0) {
                u32 written = 0;
                const auto result = sslConnectionWrite(&connection, request.data(),
                                                       static_cast<u32>(request.size()), &written);
                if (R_SUCCEEDED(result)) {
                    if (written == 0 || written > request.size())
                        return false;
                    request.remove_prefix(written);
                } else if (R_VALUE(result) != SslWouldBlock ||
                           !WaitForSsl(connection, SslPollEvent_Write, deadline)) {
                    return false;
                }
            }
            return request.empty();
        }

        HttpReply ReadReply(SslConnection &connection, std::size_t body_limit,
                            Clock::time_point deadline) {
            std::array<char, 4096> buffer{};
            std::string text;
            std::optional<HttpHeaders> headers;
            std::size_t body_start = 0;
            while (RemainingMilliseconds(deadline) > 0) {
                u32 bytes = 0;
                const auto result = sslConnectionRead(&connection, buffer.data(), buffer.size(), &bytes);
                if (R_FAILED(result)) {
                    if (R_VALUE(result) == SslWouldBlock &&
                        WaitForSsl(connection, SslPollEvent_Read, deadline))
                        continue;
                    return {};
                }
                if (bytes == 0 || bytes > buffer.size())
                    return {};
                text.append(buffer.data(), bytes);
                if (!headers) {
                    const auto separator = text.find("\r\n\r\n");
                    if (separator == text.npos && text.size() <= HeaderLimit)
                        continue;
                    if (separator > HeaderLimit)
                        return {};
                    headers = ParseHttpHeaders(std::string_view(text).substr(0, separator), body_limit);
                    if (!headers)
                        return {};
                    body_start = separator + 4;
                }
                const auto received = text.size() - body_start;
                if (received > headers->content_length)
                    return {};
                if (received == headers->content_length) {
                    text.erase(0, body_start);
                    return {headers->status, std::move(text)};
                }
            }
            return {};
        }

    }

    HttpReply PostHttps(const GatewayOrigin &origin, std::string_view path,
                        std::string_view body, std::size_t body_limit) {
        const auto deadline = Clock::now() + 25s;
        TlsConnection tls;
        if (!EstablishTls(tls, origin, deadline))
            return {};

        // The gateway is a fixed HTTP endpoint over verified TLS. HTTP/1.0 avoids
        // chunked framing and the client makes no follow-up requests to redirects.
        std::string request = "POST " + std::string(path) + " HTTP/1.0\r\nHost: " + origin.authority +
                              "\r\nContent-Type: application/json\r\nAccept: application/json\r\n"
                              "Accept-Encoding: identity\r\nConnection: close\r\nContent-Length: " +
                              std::to_string(body.size()) + "\r\n\r\n";
        request.append(body);
        if (!WriteRequest(tls.connection, request, deadline))
            return {};
        return ReadReply(tls.connection, body_limit, deadline);
    }

}
