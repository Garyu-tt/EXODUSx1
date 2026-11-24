// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "catalog_homebrew_policy.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <json.hpp>

namespace exodus::home {

    namespace homebrew_provider {

        inline constexpr std::size_t GridEntryLimit = 4096;
        inline constexpr std::size_t GridRecordByteLimit = 16 * 1024;
        inline constexpr std::size_t NroPrefixBytes = 0x80; // libnx NroStart followed by NroHeader.
        inline constexpr std::uint32_t NroMagic = 0x304f524e;
        inline constexpr std::uint32_t NroPageBytes = 0x1000;
        using File = std::unique_ptr<std::FILE, int (*)(std::FILE *)>;

        inline bool IsDirectory(const std::string &path) {
            struct stat state{};
            return lstat(path.c_str(), &state) == 0 && S_ISDIR(state.st_mode);
        }

        inline File OpenRegularFile(const std::string &path, std::uint64_t &file_size) {
            File file(nullptr, std::fclose);
            struct stat before{};
            if (lstat(path.c_str(), &before) != 0 || !S_ISREG(before.st_mode))
                return file;

            int flags = O_RDONLY;
#ifdef O_NOFOLLOW
            flags |= O_NOFOLLOW;
#endif
#ifdef O_NONBLOCK
            flags |= O_NONBLOCK;
#endif
#ifdef O_BINARY
            flags |= O_BINARY;
#endif
            const auto fd = open(path.c_str(), flags);
            if (fd < 0)
                return file;
            struct stat opened{};
            if (fstat(fd, &opened) != 0 || !S_ISREG(opened.st_mode) || opened.st_size < 0
                || opened.st_dev != before.st_dev || opened.st_ino != before.st_ino) {
                close(fd);
                return file;
            }
            file.reset(fdopen(fd, "rb"));
            if (!file) {
                close(fd);
                return file;
            }
            file_size = static_cast<std::uint64_t>(opened.st_size);
            return file;
        }

        inline std::optional<nlohmann::json> ReadGridRecord(const std::string &path) {
            std::uint64_t size = 0;
            const auto file = OpenRegularFile(path, size);
            if (!file || size == 0 || size > GridRecordByteLimit)
                return std::nullopt;
            std::string text(static_cast<std::size_t>(size), '\0');
            if (std::fread(text.data(), 1, text.size(), file.get()) != text.size()
                || std::fgetc(file.get()) != EOF || std::ferror(file.get())
                || text.find('\0') != std::string::npos)
                return std::nullopt;

            using Json = nlohmann::json;
            using Event = Json::parse_event_t;
            bool seen_type = false;
            bool seen_path = false;
            bool invalid = false;
            auto record = Json::parse(text, [&](int depth, Event event, Json &value) {
                if (event == Event::array_start || (event == Event::object_start && depth != 0))
                    invalid = true;
                if (event == Event::key && depth == 1) {
                    if (value == "type") {
                        invalid = invalid || seen_type;
                        seen_type = true;
                    } else if (value == "nro_path") {
                        invalid = invalid || seen_path;
                        seen_path = true;
                    }
                }
                return !invalid;
            }, false);
            if (invalid || !record.is_object())
                return std::nullopt;
            return record;
        }

        inline std::uint32_t ReadLe32(const std::array<unsigned char, NroPrefixBytes> &prefix,
            std::size_t offset) {
            return static_cast<std::uint32_t>(prefix[offset])
                | (static_cast<std::uint32_t>(prefix[offset + 1]) << 8)
                | (static_cast<std::uint32_t>(prefix[offset + 2]) << 16)
                | (static_cast<std::uint32_t>(prefix[offset + 3]) << 24);
        }

        inline bool HasConsistentNroHeader(const std::string &path) {
            std::uint64_t file_size = 0;
            const auto file = OpenRegularFile(path, file_size);
            std::array<unsigned char, NroPrefixBytes> prefix{};
            if (!file || file_size < prefix.size()
                || std::fread(prefix.data(), 1, prefix.size(), file.get()) != prefix.size()
                || ReadLe32(prefix, 0x10) != NroMagic)
                return false;

            const auto image_size = ReadLe32(prefix, 0x18);
            const auto bss_size = ReadLe32(prefix, 0x38);
            if (image_size <= prefix.size() || image_size > file_size
                || ReadLe32(prefix, 0x04) >= image_size
                || bss_size > std::numeric_limits<std::uint32_t>::max() - image_size)
                return false;

            std::uint32_t previous_end = 0;
            for (std::size_t segment = 0; segment < 3; ++segment) {
                const auto offset = ReadLe32(prefix, 0x20 + 8 * segment);
                const auto size = ReadLe32(prefix, 0x24 + 8 * segment);
                if ((segment == 0 && (offset != 0 || size < prefix.size()))
                    || offset % NroPageBytes != 0 || (segment < 2 && size % NroPageBytes != 0)
                    || offset < previous_end || offset >= image_size || size == 0
                    || size > image_size - offset)
                    return false;
                previous_end = offset + size;
            }
            return previous_end == image_size;
        }

        inline bool HasLocalNro(std::string_view registered_path, std::string_view sd_mount) {
            std::string local_path(sd_mount);
            const auto switch_directory = HomebrewAppletDirectory.substr(HomebrewSdMount.size());
            local_path.append(switch_directory.substr(0, switch_directory.size() - 1));
            auto remaining = registered_path.substr(HomebrewAppletDirectory.size());
            for (;;) {
                if (!IsDirectory(local_path))
                    return false;
                const auto slash = remaining.find('/');
                local_path += '/';
                local_path.append(remaining.substr(0, slash));
                if (slash == std::string_view::npos)
                    return HasConsistentNroHeader(local_path);
                remaining.remove_prefix(slash + 1);
            }
        }

    }

    // The backend supplies the active profile's root and the mounted SD path.
    inline std::optional<std::string> ResolveRegisteredHomebrewApplet(std::string_view grid_root,
        std::string_view id, std::size_t path_capacity, std::uint32_t homebrew_type,
        std::string_view sd_mount = HomebrewSdMount) {
        if (!IsCanonicalHomebrewAppletId(id, path_capacity) || grid_root.empty() || sd_mount.empty()
            || grid_root.find('\0') != std::string_view::npos || sd_mount.find('\0') != std::string_view::npos)
            return std::nullopt;
        const std::string root(grid_root);
        if (!homebrew_provider::IsDirectory(root))
            return std::nullopt;
        const std::unique_ptr<DIR, int (*)(DIR *)> directory(opendir(root.c_str()), closedir);
        if (!directory)
            return std::nullopt;

        std::optional<std::string> registered;
        std::size_t inspected = 0;
        for (;;) {
            errno = 0;
            const auto entry = readdir(directory.get());
            if (entry == nullptr) {
#if defined(__SWITCH__) || defined(ATMOSPHERE_OS_HORIZON)
                // libnx fsdev_dirnext reports a successful empty read as ENOENT.
                if (errno != 0 && errno != ENOENT)
#else
                if (errno != 0)
#endif
                    return std::nullopt;
                break;
            }
            const std::string_view name(entry->d_name);
            if (name == "." || name == "..")
                continue;
            if (++inspected > homebrew_provider::GridEntryLimit)
                return std::nullopt;
            constexpr std::string_view suffix = ".m.json";
            if (entry->d_type == DT_DIR || name.size() < suffix.size()
                || name.substr(name.size() - suffix.size()) != suffix)
                continue;

            const auto record = homebrew_provider::ReadGridRecord(root + '/' + std::string(name));
            if (!record)
                return std::nullopt;
            const auto type = record->find("type");
            if (type == record->end() || !type->is_number_integer())
                return std::nullopt;
            if (*type != homebrew_type)
                continue;

            const auto path = record->find("nro_path");
            if (path == record->end() || !path->is_string())
                return std::nullopt;
            const auto &value = path->get_ref<const std::string &>();
            if (!HomebrewAppletPathMatches(id, value))
                continue;
            if (registered)
                return std::nullopt;
            registered = value;
        }

        if (!registered || !homebrew_provider::HasLocalNro(*registered, sd_mount))
            return std::nullopt;
        return registered;
    }

}
