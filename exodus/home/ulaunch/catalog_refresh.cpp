// SPDX-License-Identifier: GPL-2.0-only
#include "catalog_refresh.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace exodus::home {

    namespace {

        constexpr std::string_view SnapshotPrefix = "steam-catalog-";
        constexpr std::string_view SnapshotSuffix = ".json";
        constexpr std::size_t GenerationDigits = 20;
        constexpr std::size_t RetainedSnapshots = 8;
        constexpr unsigned PublishAttempts = 8;

        bool IsDirectory(const char *path) {
            struct stat metadata{};
            return lstat(path, &metadata) == 0 && S_ISDIR(metadata.st_mode);
        }

        bool IsRegularFile(const char *path) {
            struct stat metadata{};
            return lstat(path, &metadata) == 0 && S_ISREG(metadata.st_mode);
        }

        bool PathMissing(const char *path) {
            struct stat metadata{};
            return lstat(path, &metadata) != 0 && errno == ENOENT;
        }

        bool DirectoryReadFailed(int error) {
#ifdef __SWITCH__
            // The Switch devoptab reports normal end of directory as ENOENT.
            if (error == ENOENT)
                return false;
#endif
            return error != 0;
        }

        std::optional<std::uint64_t> SnapshotGeneration(std::string_view name) {
            if (name.size() != SnapshotPrefix.size() + GenerationDigits + SnapshotSuffix.size() ||
                name.substr(0, SnapshotPrefix.size()) != SnapshotPrefix ||
                name.substr(SnapshotPrefix.size() + GenerationDigits) != SnapshotSuffix)
                return std::nullopt;
            const auto field = name.substr(SnapshotPrefix.size(), GenerationDigits);
            std::uint64_t generation = 0;
            const auto parsed = std::from_chars(field.data(), field.data() + field.size(), generation);
            if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size() || generation == 0)
                return std::nullopt;
            return generation;
        }

        std::string SnapshotPath(const char *directory, std::uint64_t generation) {
            const auto digits = std::to_string(generation);
            return std::string(directory) + "/" + std::string(SnapshotPrefix) +
                   std::string(GenerationDigits - digits.size(), '0') + digits + std::string(SnapshotSuffix);
        }

        std::optional<std::vector<std::uint64_t>> ListSnapshots(const char *directory) {
            if (!IsDirectory(directory))
                return std::nullopt;
            auto *listing = opendir(directory);
            if (!listing)
                return std::nullopt;
            std::vector<std::uint64_t> generations;
            int read_error = 0;
            while (true) {
                errno = 0;
                const auto *entry = readdir(listing);
                if (!entry) {
                    read_error = errno;
                    break;
                }
                const auto generation = SnapshotGeneration(entry->d_name);
                if (generation)
                    generations.push_back(*generation);
            }
            if (closedir(listing) != 0 || DirectoryReadFailed(read_error))
                return std::nullopt;
            std::sort(generations.rbegin(), generations.rend());
            return generations;
        }

        Catalog ReadSteamSnapshot(const std::string &path) {
            if (!IsRegularFile(path.c_str()))
                return {CatalogStatus::Unavailable, {}};
            return ReadCatalog(path.c_str());
        }

        void PrunePublishedSnapshots(const char *directory) {
            const auto generations = ListSnapshots(directory);
            if (!generations || generations->size() <= RetainedSnapshots)
                return;
            std::size_t valid = 0;
            for (const auto generation : *generations) {
                const auto path = SnapshotPath(directory, generation);
                if (IsSteamCatalog(ReadSteamSnapshot(path)) && ++valid > RetainedSnapshots)
                    unlink(path.c_str());
            }
        }

        std::optional<std::string> StageSteam(const char *directory, std::string_view steam) {
            std::string pending_path = std::string(directory) + "/.steam-catalog-XXXXXX";
            const int fd = mkstemp(pending_path.data());
            if (fd < 0)
                return std::nullopt;
            auto *stream = fdopen(fd, "wb");
            if (!stream) {
                close(fd);
                unlink(pending_path.c_str());
                return std::nullopt;
            }
            const bool flushed = std::fwrite(steam.data(), 1, steam.size(), stream) == steam.size() &&
                                 std::fflush(stream) == 0 && fsync(fd) == 0;
            const auto closed = std::fclose(stream);
            if (!flushed || closed != 0) {
                unlink(pending_path.c_str());
                return std::nullopt;
            }
            return pending_path;
        }

        bool PublishNoOverwrite(const char *staged, const char *published) {
#ifdef __SWITCH__
            // The SD filesystem implements rename without replacing an existing path.
            return std::rename(staged, published) == 0;
#else
            // A host POSIX rename replaces its target, so use an exclusive publication.
            if (link(staged, published) != 0)
                return false;
            unlink(staged);
            return true;
#endif
        }

    }

    bool IsSteamCatalog(const Catalog &catalog) {
        return catalog.status == CatalogStatus::Ready &&
               std::all_of(catalog.entries.begin(), catalog.entries.end(),
                           [](const Entry &entry) { return entry.provider == Provider::Steam; });
    }

    std::optional<std::uint64_t> PublishSteamSnapshot(const char *directory, std::string_view steam) {
        if (!IsSteamCatalog(ParseCatalog(steam)))
            return std::nullopt;
        if (mkdir(directory, 0755) != 0 && (errno != EEXIST || !IsDirectory(directory)))
            return std::nullopt;

        const auto staged = StageSteam(directory, steam);
        if (!staged)
            return std::nullopt;
        std::uint64_t generation = 0;
        for (unsigned attempt = 0; attempt < PublishAttempts; ++attempt) {
            const auto generations = ListSnapshots(directory);
            if (!generations)
                break;
            if (!generations->empty())
                generation = std::max(generation, generations->front());
            if (generation == std::numeric_limits<std::uint64_t>::max())
                break;
            const auto path = SnapshotPath(directory, ++generation);
            if (PublishNoOverwrite(staged->c_str(), path.c_str())) {
                PrunePublishedSnapshots(directory);
                return generation;
            }
            if (PathMissing(path.c_str()))
                break;
        }
        unlink(staged->c_str());
        return std::nullopt;
    }

    Catalog ApplySteamSnapshots(Catalog current, const char *snapshot_directory) {
        if (current.status != CatalogStatus::Ready && current.status != CatalogStatus::Unavailable)
            return current;
        const auto generations = ListSnapshots(snapshot_directory);
        if (!generations)
            return current;

        const auto non_steam = static_cast<std::size_t>(std::count_if(
            current.entries.begin(), current.entries.end(),
            [](const Entry &entry) { return entry.provider != Provider::Steam; }));
        for (const auto generation : *generations) {
            auto steam = ReadSteamSnapshot(SnapshotPath(snapshot_directory, generation));
            if (!IsSteamCatalog(steam) || steam.entries.size() > CatalogEntryLimit - non_steam)
                continue;
            steam.entries.reserve(steam.entries.size() + non_steam);
            for (auto &entry : current.entries) {
                if (entry.provider != Provider::Steam)
                    steam.entries.push_back(std::move(entry));
            }
            steam.steam_snapshot_applied = true;
            return steam;
        }
        return current;
    }

    Catalog ReadRefreshedCatalog(const char *base_path, const char *snapshot_directory) {
        auto current = ReadCatalog(base_path);
        if (current.status == CatalogStatus::Unavailable && !PathMissing(base_path))
            return current;
        return ApplySteamSnapshots(std::move(current), snapshot_directory);
    }

}
