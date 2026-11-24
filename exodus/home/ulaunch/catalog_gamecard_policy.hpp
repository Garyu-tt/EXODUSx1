// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstdint>

namespace exodus::home {

    enum class StartupLicense {
        Unknown,
        Local,
        NetworkRequired,
    };

    struct GameCardLaunchEvidence {
        std::uint64_t registered_id = 0;
        std::uint64_t inserted_card_id = 0;
        std::uint64_t view_id = 0;
        bool ready = false;
        bool application_suspended = false;
        StartupLicense startup_license = StartupLicense::Unknown;
        std::uint8_t startup_user_account = 0xFF;
    };

    constexpr bool IsApplicationProgramId(std::uint64_t id) {
        return id >= 0x0100000000010000ULL && id <= 0x01FFFFFFFFFFFFFFULL;
    }

    // Evidence comes from the local NS provider at the point of use.
    constexpr bool MayLaunchCatalogGameCard(std::uint64_t id, const GameCardLaunchEvidence &evidence) {
        return IsApplicationProgramId(id) && evidence.registered_id == id &&
               evidence.inserted_card_id == id && evidence.view_id == id &&
               evidence.ready && !evidence.application_suspended &&
               evidence.startup_license == StartupLicense::Local && evidence.startup_user_account <= 1;
    }

}
