// SPDX-License-Identifier: GPL-2.0-only
#include "sync.hpp"

#include "../home/ulaunch/catalog_refresh.hpp"

#include <array>
#include <cstdio>
#include <switch.h>

namespace {

    namespace steam = exodus::steam_switch;
    namespace home = exodus::home;
    constexpr char GatewayFile[] = "sdmc:/switch/exodus-steam-sync/gateway.txt";

    std::string HexToken(const std::array<u8, 32> &bytes) {
        constexpr char digits[] = "0123456789abcdef";
        std::string token;
        token.reserve(bytes.size() * 2);
        for (const auto byte : bytes) {
            token += digits[byte >> 4];
            token += digits[byte & 15];
        }
        return token;
    }

    bool OpenSteamLogin(const std::string &url, const std::string &finished) {
        WebCommonConfig config{};
        WebCommonReply reply{};
        if (R_FAILED(webPageCreate(&config, url.c_str())) ||
            R_FAILED(webConfigSetCallbackUrl(&config, finished.c_str())))
            return false;
        return R_SUCCEEDED(webConfigShow(&config, &reply));
    }

    const char *StatusMessage(steam::SyncStatus status) {
        switch (status) {
            case steam::SyncStatus::Ready: return "Steam library received.";
            case steam::SyncStatus::GatewayUnavailable: return "Unable to reach the gateway or retrieve the library.";
            case steam::SyncStatus::InvalidSession: return "The gateway returned an invalid sign-in session.";
            case steam::SyncStatus::BrowserUnavailable: return "The Switch WebApplet could not open sign-in.";
            case steam::SyncStatus::SignInIncomplete: return "Sign-in is incomplete or was cancelled.";
            case steam::SyncStatus::SessionExpired: return "The sign-in session expired.";
            case steam::SyncStatus::InvalidCatalog: return "The gateway returned an invalid Steam catalog.";
        }
        return "Unable to sync the Steam library.";
    }

    void UpdateLibrary() {
        const auto gateway = steam::ReadGatewayOrigin(GatewayFile);
        if (!gateway) {
            std::puts("Put the operator's HTTPS origin in gateway.txt.");
            return;
        }

        std::array<u8, 32> verifier{};
        std::array<u8, 32> challenge{};
        randomGet(verifier.data(), verifier.size());
        sha256CalculateHash(challenge.data(), verifier.data(), verifier.size());
        std::puts("Connecting to the configured Steam gateway...");
        consoleUpdate(nullptr);
        const auto snapshot = steam::FetchSteamSnapshot(
            *gateway, HexToken(challenge), HexToken(verifier),
            [&](std::string_view path, std::string_view body, std::size_t limit) {
                return steam::PostHttps(*gateway, path, body, limit);
            }, OpenSteamLogin);
        if (snapshot.status != steam::SyncStatus::Ready) {
            std::puts(StatusMessage(snapshot.status));
            return;
        }

        if (!home::PublishSteamSnapshot(home::SteamSnapshotsPath, snapshot.catalog)) {
            std::puts("Could not publish the Steam snapshot.");
            return;
        }
        std::puts("Steam library saved. Reopen EXODUS library in HOME.");
    }

}

int main() {
    consoleInit(nullptr);
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    const bool sockets = R_SUCCEEDED(socketInitializeDefault());
    const bool tls = sockets && R_SUCCEEDED(sslInitialize(1));
    std::puts("EXODUS Steam Sync\n");
    if (tls)
        std::puts("A  Sign in and sync\n+  Exit\n");
    else
        std::puts("Network services are unavailable.\n+  Exit\n");

    while (appletMainLoop()) {
        padUpdate(&pad);
        const auto pressed = padGetButtonsDown(&pad);
        if (pressed & HidNpadButton_Plus)
            break;
        if (tls && (pressed & HidNpadButton_A)) {
            consoleClear();
            UpdateLibrary();
            std::puts("\nA  Start another sync\n+  Exit\n");
        }
        consoleUpdate(nullptr);
    }

    if (tls)
        sslExit();
    if (sockets)
        socketExit();
    consoleExit(nullptr);
    return 0;
}
