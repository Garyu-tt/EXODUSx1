// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace exodus::steam_integrity {

    enum class Status {
        Match,
        ReferenceUnavailable,
        ReferenceDigestMismatch,
        InvalidReference,
        InstallationUnavailable,
        MissingOrUnsafeFile,
        SizeMismatch,
        HashMismatch,
        ReadFailure,
        Cancelled,
    };

    struct VerificationResult {
        Status status;
        std::size_t checked_files;
        std::size_t total_files;
        std::uint64_t compared_bytes;
        std::string path;
    };

    using ProgressCallback = bool (*)(std::size_t checked_files, std::size_t total_files,
        std::uint64_t compared_bytes, void *context);

    VerificationResult VerifySteamInstallation(std::string_view sd_mount, std::uint32_t app_id,
        std::string_view expected_reference_sha256, ProgressCallback progress = nullptr, void *context = nullptr);

}
