// SPDX-License-Identifier: GPL-2.0-only
#include "catalog_steam_install.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#if !defined(__SWITCH__) && !defined(ATMOSPHERE_OS_HORIZON) && defined(AT_SYMLINK_NOFOLLOW) \
    && defined(O_DIRECTORY) && defined(O_NOFOLLOW)
#define EXODUS_STEAM_DIRECTORY_FD 1
#endif

namespace exodus::home {

    namespace {

        constexpr std::size_t ManifestByteLimit = 64 * 1024;
        constexpr std::size_t ManifestDepthLimit = 16;
        constexpr std::uint32_t FullyInstalled = 4;
        constexpr std::array<std::uint32_t, 5> MinimumUtf8Codepoint{0, 0, 0x80, 0x800, 0x10000};

        bool ValidText(std::string_view text) {
            for (std::size_t offset = 0; offset < text.size();) {
                const auto lead = static_cast<unsigned char>(text[offset++]);
                if (lead < 0x80) {
                    if ((lead < 0x20 && lead != '\t' && lead != '\n' && lead != '\r') || lead == 0x7f)
                        return false;
                    continue;
                }
                if (lead < 0xc2 || lead > 0xf4)
                    return false;
                const auto length = lead < 0xe0 ? 2u : lead < 0xf0 ? 3u : 4u;
                if (length - 1 > text.size() - offset)
                    return false;
                std::uint32_t codepoint = lead & ((1u << (7 - length)) - 1);
                for (std::size_t byte = 1; byte < length; ++byte) {
                    const auto continuation = static_cast<unsigned char>(text[offset++]);
                    if ((continuation & 0xc0u) != 0x80u)
                        return false;
                    codepoint = (codepoint << 6) | (continuation & 0x3fu);
                }
                if (codepoint < MinimumUtf8Codepoint[length] || codepoint > 0x10ffff
                    || (codepoint >= 0xd800 && codepoint <= 0xdfff))
                    return false;
            }
            return true;
        }

        char LowerAscii(char ch) {
            return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
        }

        bool EqualsAsciiCase(std::string_view text, std::string_view lower) {
            if (text.size() != lower.size())
                return false;
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (LowerAscii(text[i]) != lower[i])
                    return false;
            }
            return true;
        }

        bool IsSafeInstallDirectory(std::string_view name) {
            if (name.empty() || name.size() > 255 || name.front() == ' '
                || name.back() == '.' || name.back() == ' '
                || name.find_first_of("/\\:*?\"<>|\t\r\n") != std::string_view::npos)
                return false;

            // Avoid Win32 device-name aliases when Steam libraries move between FAT and Windows.
            auto stem = name.substr(0, name.find('.'));
            while (!stem.empty() && stem.back() == ' ')
                stem.remove_suffix(1);
            if (EqualsAsciiCase(stem, "con") || EqualsAsciiCase(stem, "prn")
                || EqualsAsciiCase(stem, "aux") || EqualsAsciiCase(stem, "nul"))
                return false;
            if (stem.size() == 4 && stem.back() >= '1' && stem.back() <= '9') {
                const auto prefix = stem.substr(0, 3);
                return !EqualsAsciiCase(prefix, "com") && !EqualsAsciiCase(prefix, "lpt");
            }
            return true;
        }

        enum class TokenKind { End, Text, Open, Close, Invalid };

        struct Token {
            TokenKind kind;
            std::string_view text{};
        };

        class ManifestTokens {
        public:
            explicit ManifestTokens(std::string_view text) : remaining(text) {}

            Token Next() {
                constexpr std::string_view whitespace = " \t\r\n";
                while (!remaining.empty()) {
                    const auto start = remaining.find_first_not_of(whitespace);
                    if (start == std::string_view::npos)
                        break;
                    remaining.remove_prefix(start);
                    if (remaining.substr(0, 2) != "//")
                        return ReadToken();
                    const auto end = remaining.find_first_of("\r\n", 2);
                    if (end == std::string_view::npos)
                        break;
                    remaining.remove_prefix(end);
                }
                remaining = {};
                return {TokenKind::End};
            }

        private:
            Token ReadToken() {
                const char first = remaining.front();
                if (first == '{' || first == '}') {
                    remaining.remove_prefix(1);
                    return {first == '{' ? TokenKind::Open : TokenKind::Close};
                }
                if (first == '"') {
                    for (std::size_t i = 1; i < remaining.size(); ++i) {
                        if (remaining[i] == '"') {
                            const auto text = remaining.substr(1, i - 1);
                            remaining.remove_prefix(i + 1);
                            return {TokenKind::Text, text};
                        }
                        if (remaining[i] == '\\' && i + 1 < remaining.size()
                            && (remaining[i + 1] == '"' || remaining[i + 1] == '\\'))
                            ++i;
                    }
                    return {TokenKind::Invalid};
                }
                const auto text = remaining.substr(0, remaining.find_first_of(" \t\r\n{}\""));
                remaining.remove_prefix(text.size());
                return {TokenKind::Text, text};
            }

            std::string_view remaining;
        };

        struct InstallationManifest {
            std::string_view directory;
            std::uint32_t state_flags;
        };

        std::optional<InstallationManifest> ParseManifest(std::string_view text, std::string_view expected_id) {
            if (text.substr(0, 3) == "\xef\xbb\xbf")
                text.remove_prefix(3);
            if (!ValidText(text))
                return std::nullopt;

            ManifestTokens tokens(text);
            const auto root = tokens.Next();
            if (root.kind != TokenKind::Text || !EqualsAsciiCase(root.text, "appstate")
                || tokens.Next().kind != TokenKind::Open)
                return std::nullopt;

            constexpr std::array<std::string_view, 3> names{"appid", "installdir", "stateflags"};
            std::array<std::optional<std::string_view>, names.size()> fields{};
            std::size_t depth = 1;
            while (depth != 0) {
                const auto key = tokens.Next();
                if (key.kind == TokenKind::Close) {
                    --depth;
                    continue;
                }
                if (key.kind != TokenKind::Text || key.text.empty())
                    return std::nullopt;
                const auto value = tokens.Next();
                if (depth == 1) {
                    for (std::size_t i = 0; i < names.size(); ++i) {
                        if (!EqualsAsciiCase(key.text, names[i]))
                            continue;
                        if (fields[i] || value.kind != TokenKind::Text)
                            return std::nullopt;
                        fields[i] = value.text;
                        break;
                    }
                }
                if (value.kind == TokenKind::Open) {
                    if (++depth > ManifestDepthLimit)
                        return std::nullopt;
                } else if (value.kind != TokenKind::Text) {
                    return std::nullopt;
                }
            }

            const auto &[id, directory, flags] = fields;
            if (tokens.Next().kind != TokenKind::End || id != expected_id
                || !directory || !IsSafeInstallDirectory(*directory)
                || !flags || flags->empty() || flags->size() > 10)
                return std::nullopt;

            std::uint32_t state = 0;
            const auto end = flags->data() + flags->size();
            const auto result = std::from_chars(flags->data(), end, state);
            if (result.ec != std::errc{} || result.ptr != end)
                return std::nullopt;
            return InstallationManifest{*directory, state};
        }

        class FileDescriptor {
        public:
            explicit FileDescriptor(int fd = -1) : value(fd) {}
            FileDescriptor(const FileDescriptor &) = delete;
            FileDescriptor &operator=(const FileDescriptor &) = delete;

            ~FileDescriptor() {
                if (value >= 0)
                    close(value);
            }

            void Reset(int fd) {
                if (value >= 0)
                    close(value);
                value = fd;
            }

            int Get() const { return value; }

        private:
            int value;
        };

        enum class DirectoryStatus { Found, Missing, NotDirectory, Unavailable };

        struct Directory {
#ifdef EXODUS_STEAM_DIRECTORY_FD
            FileDescriptor fd;

            int Stat(const char *name, struct stat &entry) const {
                return fstatat(fd.Get(), name, &entry, AT_SYMLINK_NOFOLLOW);
            }

            int Open(const char *name, int flags) const {
                return openat(fd.Get(), name, O_RDONLY | flags);
            }
#else
            // Horizon's SD filesystem is FAT/exFAT. libnx exposes these path operations, not openat.
            std::string path;

            std::string ChildPath(const char *name) const {
                return path.back() == '/' ? path + name : path + '/' + name;
            }

            int Stat(const char *name, struct stat &entry) const {
                return lstat(ChildPath(name).c_str(), &entry);
            }

            int Open(const char *name, int flags) const {
                return open(ChildPath(name).c_str(), O_RDONLY | flags);
            }
#endif
        };

        int FileOpenFlags() {
            int flags = 0;
#ifdef O_NOFOLLOW
            flags |= O_NOFOLLOW;
#endif
#ifdef O_NONBLOCK
            flags |= O_NONBLOCK;
#endif
#ifdef O_CLOEXEC
            flags |= O_CLOEXEC;
#endif
#ifdef O_BINARY
            flags |= O_BINARY;
#endif
            return flags;
        }

        bool OpenSdRoot(std::string_view mount, Directory &root) {
            if (mount.empty() || mount.find('\0') != std::string_view::npos)
                return false;
            while (mount.size() > 1 && mount.back() == '/')
                mount.remove_suffix(1);
#ifdef EXODUS_STEAM_DIRECTORY_FD
            root.fd.Reset(open(std::string(mount).c_str(), O_RDONLY | O_DIRECTORY | FileOpenFlags()));
            return root.fd.Get() >= 0;
#else
            root.path = mount;
            if (root.path.back() == ':')
                root.path += '/';
            struct stat entry{};
            return lstat(root.path.c_str(), &entry) == 0 && S_ISDIR(entry.st_mode);
#endif
        }

        DirectoryStatus OpenSubdirectory(const Directory &parent, const char *name, Directory &child) {
            struct stat entry{};
            if (parent.Stat(name, entry) != 0)
                return errno == ENOENT ? DirectoryStatus::Missing : DirectoryStatus::Unavailable;
            if (S_ISLNK(entry.st_mode))
                return DirectoryStatus::Unavailable;
            if (!S_ISDIR(entry.st_mode))
                return DirectoryStatus::NotDirectory;
#ifdef EXODUS_STEAM_DIRECTORY_FD
            child.fd.Reset(parent.Open(name, O_DIRECTORY | FileOpenFlags()));
            if (child.fd.Get() < 0)
                return DirectoryStatus::Unavailable;
#else
            child.path = parent.ChildPath(name);
#endif
            return DirectoryStatus::Found;
        }

        enum class ManifestRead { Found, Missing, Invalid, Unavailable };

        ssize_t Read(int fd, void *output, std::size_t size) {
            ssize_t count;
            do {
                count = read(fd, output, size);
            } while (count < 0 && errno == EINTR);
            return count;
        }

        ManifestRead ReadManifest(const Directory &steamapps, const char *filename, std::string &text) {
            struct stat before{};
            if (steamapps.Stat(filename, before) != 0)
                return errno == ENOENT ? ManifestRead::Missing : ManifestRead::Unavailable;
            if (!S_ISREG(before.st_mode))
                return ManifestRead::Invalid;

            const FileDescriptor input(steamapps.Open(filename, FileOpenFlags()));
            struct stat opened{};
            if (input.Get() < 0 || fstat(input.Get(), &opened) != 0
                || before.st_dev != opened.st_dev || before.st_ino != opened.st_ino)
                return ManifestRead::Unavailable;
            if (!S_ISREG(opened.st_mode) || opened.st_size <= 0
                || static_cast<std::uint64_t>(opened.st_size) > ManifestByteLimit)
                return ManifestRead::Invalid;

            text.resize(static_cast<std::size_t>(opened.st_size));
            std::size_t offset = 0;
            while (offset < text.size()) {
                const auto count = Read(input.Get(), text.data() + offset, text.size() - offset);
                if (count <= 0)
                    return ManifestRead::Unavailable;
                offset += static_cast<std::size_t>(count);
            }
            char extra;
            return Read(input.Get(), &extra, 1) == 0 ? ManifestRead::Found : ManifestRead::Unavailable;
        }

    }

    static SteamInstallEvidence InspectLocalSteamInstallation(std::string_view sd_mount, std::uint32_t app_id,
        std::string *game_directory) {
        constexpr std::array<const char *, 3> library_path{"switch", "exodus-steam", "steamapps"};
        std::array<Directory, library_path.size() + 1> directories;
        if (app_id == 0 || !OpenSdRoot(sd_mount, directories.front()))
            return SteamInstallEvidence::Unavailable;

        for (std::size_t i = 0; i < library_path.size(); ++i) {
            const auto status = OpenSubdirectory(directories[i], library_path[i], directories[i + 1]);
            if (status == DirectoryStatus::Missing)
                return SteamInstallEvidence::NoManifest;
            if (status != DirectoryStatus::Found)
                return SteamInstallEvidence::Unavailable;
        }

        const auto id = std::to_string(app_id);
        const auto filename = "appmanifest_" + id + ".acf";
        const auto &steamapps = directories.back();
        std::string text;
        switch (ReadManifest(steamapps, filename.c_str(), text)) {
            case ManifestRead::Missing: return SteamInstallEvidence::NoManifest;
            case ManifestRead::Invalid: return SteamInstallEvidence::InvalidManifest;
            case ManifestRead::Unavailable: return SteamInstallEvidence::Unavailable;
            case ManifestRead::Found: break;
        }

        const auto manifest = ParseManifest(text, id);
        if (!manifest)
            return SteamInstallEvidence::InvalidManifest;
        if ((manifest->state_flags & FullyInstalled) == 0)
            return SteamInstallEvidence::IncompleteManifest;

        Directory common;
        const auto common_status = OpenSubdirectory(steamapps, "common", common);
        if (common_status == DirectoryStatus::Missing)
            return SteamInstallEvidence::MissingGameDirectory;
        if (common_status != DirectoryStatus::Found)
            return SteamInstallEvidence::Unavailable;

        Directory game;
        const auto path = std::string(manifest->directory);
        switch (OpenSubdirectory(common, path.c_str(), game)) {
            case DirectoryStatus::Found:
                if (game_directory != nullptr)
                    *game_directory = path;
                return SteamInstallEvidence::ManifestAndDirectoryFound;
            case DirectoryStatus::Missing:
            case DirectoryStatus::NotDirectory: return SteamInstallEvidence::MissingGameDirectory;
            case DirectoryStatus::Unavailable: return SteamInstallEvidence::Unavailable;
        }
        return SteamInstallEvidence::Unavailable;
    }

    SteamInstallEvidence ProbeLocalSteamInstallation(std::string_view sd_mount, std::uint32_t app_id) {
        return InspectLocalSteamInstallation(sd_mount, app_id, nullptr);
    }

    std::optional<std::string> FindLocalSteamGameDirectory(std::string_view sd_mount, std::uint32_t app_id) {
        std::string directory;
        if (InspectLocalSteamInstallation(sd_mount, app_id, &directory) != SteamInstallEvidence::ManifestAndDirectoryFound)
            return std::nullopt;
        return directory;
    }

}
