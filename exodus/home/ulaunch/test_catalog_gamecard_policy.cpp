// SPDX-License-Identifier: GPL-2.0-only
#include "catalog_gamecard_policy.hpp"

#include <cassert>
#include <iostream>

int main() {
    using namespace exodus::home;
    constexpr std::uint64_t title = 0x0100000000010000ULL;
    constexpr std::uint64_t different_title = 0x0100000000011000ULL;
    constexpr GameCardLaunchEvidence local{title, title, title, true, false, StartupLicense::Local, 0};
    assert(MayLaunchCatalogGameCard(title, local));
    assert(MayLaunchCatalogGameCard(title, {title, title, title, true, false, StartupLicense::Local, 1}));
    assert(!MayLaunchCatalogGameCard(title, {}));
    assert(!MayLaunchCatalogGameCard(0, {}));
    assert(!MayLaunchCatalogGameCard(different_title, local));

    constexpr std::uint64_t system_program = 0x010000000000100BULL;
    assert(!MayLaunchCatalogGameCard(system_program,
        {system_program, system_program, system_program, true, false, StartupLicense::Local, 0}));
    assert(!IsApplicationProgramId(0x0200000000010000ULL));

    assert(!MayLaunchCatalogGameCard(title, {0, title, title, true, false, StartupLicense::Local, 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, 0, title, true, false, StartupLicense::Local, 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, different_title, title, true, false, StartupLicense::Local, 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, title, different_title, true, false, StartupLicense::Local, 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, title, title, false, false, StartupLicense::Local, 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, title, title, true, true, StartupLicense::Local, 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, title, title, true, false, StartupLicense::Unknown, 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, title, title, true, false, StartupLicense::NetworkRequired, 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, title, title, true, false, static_cast<StartupLicense>(255), 0}));
    assert(!MayLaunchCatalogGameCard(title, {title, title, title, true, false, StartupLicense::Local, 2}));
    assert(!MayLaunchCatalogGameCard(title, {title, title, title, true, false, StartupLicense::Local, 0xFF}));
    std::cout << "HOME Safe Mode: unknown or non-local game card launch evidence denied\n";
}
