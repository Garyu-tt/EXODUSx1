// SPDX-License-Identifier: GPL-2.0-only
#include "verify.hpp"
#include "../home/ulaunch/catalog_steam_install.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#ifdef __SWITCH__
extern "C" {
#include <switch/crypto/sha256.h>
}
#else
#include <openssl/evp.h>
#endif

#if !defined(__SWITCH__) && !defined(ATMOSPHERE_OS_HORIZON) && defined(AT_SYMLINK_NOFOLLOW) \
    && defined(O_DIRECTORY) && defined(O_NOFOLLOW)
#define EXODUS_VERIFY_DIRECTORY_FD 1
#endif

namespace exodus::steam_integrity {

    namespace {

        using Digest = std::array<unsigned char, 32>;
        constexpr std::size_t ReferenceByteLimit = 8 * 1024 * 1024;
        constexpr std::size_t ReferenceFileLimit = 32768;
        constexpr std::size_t RelativePathLimit = 1024;
        // The path passed to libnx excludes the sdmc: prefix and includes a trailing NUL.
        constexpr std::size_t SdPathByteLimit = 0x301 - 1;
        constexpr std::size_t ReadChunkBytes = 64 * 1024;

        struct FileStamp {
            dev_t device{};
            ino_t inode{};
            off_t size{};
            time_t modified{};
            time_t changed{};
            long modified_ns{};
            long changed_ns{};

            explicit FileStamp(const struct stat &info)
                : device(info.st_dev), inode(info.st_ino), size(info.st_size),
                  modified(info.st_mtime), changed(info.st_ctime) {
#if defined(__APPLE__)
                modified_ns = info.st_mtimespec.tv_nsec;
                changed_ns = info.st_ctimespec.tv_nsec;
#elif !defined(__SWITCH__) && !defined(ATMOSPHERE_OS_HORIZON) && !defined(_WIN32)
                modified_ns = info.st_mtim.tv_nsec;
                changed_ns = info.st_ctim.tv_nsec;
#endif
            }

            FileStamp() = default;

            bool Matches(const struct stat &info) const {
                const FileStamp other(info);
                return S_ISREG(info.st_mode) && device == other.device && inode == other.inode
                    && size == other.size && modified == other.modified && changed == other.changed
                    && modified_ns == other.modified_ns && changed_ns == other.changed_ns;
            }
        };

        class Sha256 {
        public:
            Sha256() {
#ifdef __SWITCH__
                sha256ContextCreate(&context);
#else
                ready = context && EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) == 1;
#endif
            }

            bool Update(const void *data, std::size_t size) {
#ifdef __SWITCH__
                sha256ContextUpdate(&context, data, size);
                return true;
#else
                return ready && EVP_DigestUpdate(context.get(), data, size) == 1;
#endif
            }

            bool Finish(Digest &digest) {
#ifdef __SWITCH__
                sha256ContextGetHash(&context, digest.data());
                return true;
#else
                unsigned length = 0;
                return ready && EVP_DigestFinal_ex(context.get(), digest.data(), &length) == 1
                    && length == digest.size();
#endif
            }

        private:
#ifdef __SWITCH__
            Sha256Context context{};
#else
            std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context{EVP_MD_CTX_new(), EVP_MD_CTX_free};
            bool ready = false;
#endif
        };

        int HexValue(char ch, bool accept_uppercase) {
            if (ch >= '0' && ch <= '9')
                return ch - '0';
            if (ch >= 'a' && ch <= 'f')
                return ch - 'a' + 10;
            if (accept_uppercase && ch >= 'A' && ch <= 'F')
                return ch - 'A' + 10;
            return -1;
        }

        bool ParseDigest(std::string_view text, Digest &digest, bool accept_uppercase = false) {
            if (text.size() != 2 * digest.size())
                return false;
            for (std::size_t i = 0; i < digest.size(); ++i) {
                const auto high = HexValue(text[2 * i], accept_uppercase);
                const auto low = HexValue(text[2 * i + 1], accept_uppercase);
                if (high < 0 || low < 0)
                    return false;
                digest[i] = static_cast<unsigned char>(16 * high + low);
            }
            return true;
        }

        bool ParseUint64(std::string_view text, std::uint64_t &value) {
            if (text.empty() || (text.size() > 1 && text.front() == '0'))
                return false;
            const auto end = text.data() + text.size();
            const auto parsed = std::from_chars(text.data(), end, value);
            return parsed.ec == std::errc{} && parsed.ptr == end;
        }

        bool ValidPathText(std::string_view text) {
            constexpr std::array<std::uint32_t, 5> minimum{0, 0, 0x80, 0x800, 0x10000};
            for (std::size_t offset = 0; offset < text.size();) {
                const auto lead = static_cast<unsigned char>(text[offset++]);
                if (lead < 0x80) {
                    if (lead < 0x20 || lead == 0x7f)
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
                    const auto next = static_cast<unsigned char>(text[offset++]);
                    if ((next & 0xc0u) != 0x80u)
                        return false;
                    codepoint = (codepoint << 6) | (next & 0x3fu);
                }
                if (codepoint < minimum[length] || codepoint < 0xa0 || codepoint > 0x10ffff
                    || (codepoint >= 0xd800 && codepoint <= 0xdfff))
                    return false;
            }
            return true;
        }

        unsigned char FoldAscii(unsigned char byte) {
            return byte >= 'A' && byte <= 'Z' ? static_cast<unsigned char>(byte + ('a' - 'A')) : byte;
        }

        bool InReferenceOrder(std::string_view previous, std::string_view current) {
            bool spelling_changed = false;
            const auto common_size = std::min(previous.size(), current.size());
            for (std::size_t i = 0; i < common_size; ++i) {
                const auto lhs = FoldAscii(static_cast<unsigned char>(previous[i]));
                const auto rhs = FoldAscii(static_cast<unsigned char>(current[i]));
                if (lhs != rhs)
                    return lhs < rhs;
                spelling_changed |= previous[i] != current[i];
                if (lhs == '/' && spelling_changed)
                    return false;
            }
            return previous.size() < current.size();
        }

        bool SafeComponent(std::string_view name) {
            if (name.empty() || name.size() > 255 || name.front() == ' ' || name.back() == '.'
                || name.back() == ' ' || name.find_first_of("\\:*?\"<>|") != std::string_view::npos)
                return false;
            auto stem = name.substr(0, name.find('.'));
            while (!stem.empty() && stem.back() == ' ')
                stem.remove_suffix(1);
            if (stem.size() > 7)
                return true;
            std::array<char, 7> folded{};
            std::transform(stem.begin(), stem.end(), folded.begin(), [](unsigned char byte) {
                return static_cast<char>(FoldAscii(byte));
            });
            const std::string_view lower(folded.data(), stem.size());
            if (lower == "con" || lower == "prn" || lower == "aux" || lower == "nul"
                || lower == "conin$" || lower == "conout$")
                return false;
            if (lower.substr(0, 3) == "com" || lower.substr(0, 3) == "lpt") {
                const auto digit = lower.substr(3);
                if ((digit.size() == 1 && digit.front() >= '1' && digit.front() <= '9')
                    || digit == "\xc2\xb9" || digit == "\xc2\xb2" || digit == "\xc2\xb3")
                    return false;
            }
            return true;
        }

        bool SafeRelativePath(std::string_view path) {
            if (path.size() > RelativePathLimit || !ValidPathText(path))
                return false;
            for (;;) {
                const auto slash = path.find('/');
                if (!SafeComponent(path.substr(0, slash)))
                    return false;
                if (slash == std::string_view::npos)
                    return true;
                path.remove_prefix(slash + 1);
            }
        }

        bool TakeLine(std::string_view &text, std::string_view &line) {
            const auto end = text.find('\n');
            if (end == std::string_view::npos)
                return false;
            line = text.substr(0, end);
            text.remove_prefix(end + 1);
            return true;
        }

        struct Record {
            Digest digest{};
            std::uint64_t size = 0;
            std::string_view path;
            FileStamp compared_file;
        };

        bool ParseRecord(std::string_view line, Record &record) {
            const auto first_tab = line.find('\t');
            if (first_tab != record.digest.size() * 2 || !ParseDigest(line.substr(0, first_tab), record.digest))
                return false;
            line.remove_prefix(first_tab + 1);
            const auto second_tab = line.find('\t');
            if (second_tab == std::string_view::npos || !ParseUint64(line.substr(0, second_tab), record.size))
                return false;
            record.path = line.substr(second_tab + 1);
            return SafeRelativePath(record.path);
        }

        bool ParseReference(std::string_view text, std::string_view id, std::vector<Record> &records) {
            std::string_view line;
            std::uint64_t count = 0;
            if (!TakeLine(text, line) || line != "EXODUS-STEAM-SHA256 1"
                || !TakeLine(text, line) || line != id
                || !TakeLine(text, line) || !ParseUint64(line, count) || count == 0 || count > ReferenceFileLimit)
                return false;
            records.reserve(static_cast<std::size_t>(count));
            std::uint64_t total_size = 0;
            while (records.size() < count) {
                Record record;
                if (!TakeLine(text, line) || !ParseRecord(line, record)
                    || (!records.empty() && !InReferenceOrder(records.back().path, record.path))
                    || record.size > std::numeric_limits<std::uint64_t>::max() - total_size)
                    return false;
                total_size += record.size;
                records.push_back(record);
            }
            return text.empty();
        }

        class Descriptor {
        public:
            Descriptor() = default;
            explicit Descriptor(int fd) : value(fd) {}
            Descriptor(const Descriptor &) = delete;
            Descriptor &operator=(const Descriptor &) = delete;

            Descriptor(Descriptor &&other) noexcept : value(std::exchange(other.value, -1)) {}

            Descriptor &operator=(Descriptor &&other) noexcept {
                if (this != &other) {
                    if (value >= 0)
                        close(value);
                    value = std::exchange(other.value, -1);
                }
                return *this;
            }

            ~Descriptor() {
                if (value >= 0)
                    close(value);
            }

            int Get() const { return value; }

        private:
            int value = -1;
        };

        int ReadFlags() {
            int flags = O_RDONLY;
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

        struct Directory {
#ifdef EXODUS_VERIFY_DIRECTORY_FD
            Descriptor fd;

            bool IsOpen() const { return fd.Get() >= 0; }

            Directory OpenChild(const std::string &name) const {
                return {Descriptor(openat(fd.Get(), name.c_str(), ReadFlags() | O_DIRECTORY))};
            }

            int Stat(const std::string &name, struct stat &info) const {
                return fstatat(fd.Get(), name.c_str(), &info, AT_SYMLINK_NOFOLLOW);
            }

            Descriptor OpenFile(const std::string &name) const {
                return Descriptor(openat(fd.Get(), name.c_str(), ReadFlags()));
            }
#else
            // libnx SD mounts are FAT/exFAT and do not expose directory-relative descriptors.
            std::string path;

            bool IsOpen() const { return !path.empty(); }

            std::string ChildPath(const std::string &name) const {
                return path.back() == '/' ? path + name : path + '/' + name;
            }

            Directory OpenChild(const std::string &name) const {
                auto child = ChildPath(name);
                struct stat info{};
                if (lstat(child.c_str(), &info) != 0 || !S_ISDIR(info.st_mode))
                    return {};
                return {std::move(child)};
            }

            int Stat(const std::string &name, struct stat &info) const {
                return lstat(ChildPath(name).c_str(), &info);
            }

            Descriptor OpenFile(const std::string &name) const {
                return Descriptor(open(ChildPath(name).c_str(), ReadFlags()));
            }
#endif
        };

        Directory OpenRoot(std::string_view mount) {
            if (mount.empty() || mount.find('\0') != std::string_view::npos)
                return {};
            while (mount.size() > 1 && mount.back() == '/')
                mount.remove_suffix(1);
            std::string path(mount);
#ifdef EXODUS_VERIFY_DIRECTORY_FD
            return {Descriptor(open(path.c_str(), ReadFlags() | O_DIRECTORY))};
#else
            if (path.back() == ':')
                path += '/';
            struct stat info{};
            if (lstat(path.c_str(), &info) != 0 || !S_ISDIR(info.st_mode))
                return {};
            return {std::move(path)};
#endif
        }

        Directory OpenDirectory(const Directory &root, std::string_view path) {
            Directory current;
            const Directory *parent = &root;
            while (parent->IsOpen()) {
                const auto slash = path.find('/');
                current = parent->OpenChild(std::string(path.substr(0, slash)));
                if (slash == std::string_view::npos)
                    return current;
                parent = &current;
                path.remove_prefix(slash + 1);
            }
            return current;
        }

        enum class OpenStatus { Opened, MissingOrUnsafe, Failed };

        OpenStatus OpenRegularFile(const Directory &root, std::string_view path, Descriptor &file,
            std::uint64_t &size, struct stat *metadata = nullptr) {
            Directory directory;
            const Directory *parent = &root;
            const auto slash = path.rfind('/');
            if (slash != std::string_view::npos) {
                directory = OpenDirectory(root, path.substr(0, slash));
                parent = &directory;
                path.remove_prefix(slash + 1);
            }
            if (!parent->IsOpen())
                return OpenStatus::MissingOrUnsafe;

            const std::string filename(path);
            struct stat before{};
            if (parent->Stat(filename, before) != 0)
                return errno == ENOENT || errno == ENOTDIR || errno == ELOOP ? OpenStatus::MissingOrUnsafe : OpenStatus::Failed;
            if (!S_ISREG(before.st_mode))
                return OpenStatus::MissingOrUnsafe;

            file = parent->OpenFile(filename);
            if (file.Get() < 0)
                return errno == ENOENT || errno == ENOTDIR || errno == ELOOP ? OpenStatus::MissingOrUnsafe : OpenStatus::Failed;
            struct stat opened{};
            if (fstat(file.Get(), &opened) != 0 || opened.st_size < 0)
                return OpenStatus::Failed;
            if (!S_ISREG(opened.st_mode) || before.st_dev != opened.st_dev || before.st_ino != opened.st_ino)
                return OpenStatus::MissingOrUnsafe;
            size = static_cast<std::uint64_t>(opened.st_size);
            if (metadata != nullptr)
                *metadata = opened;
            return OpenStatus::Opened;
        }

        ssize_t Read(int fd, void *data, std::size_t size) {
            ssize_t count;
            do {
                count = read(fd, data, size);
            } while (count < 0 && errno == EINTR);
            return count;
        }

        enum class ReferenceRead { Complete, Failed, Cancelled };

        template<typename KeepGoing>
        ReferenceRead ReadReference(int fd, std::string &text, std::size_t size, const KeepGoing &keep_going) {
            text.resize(size);
            std::size_t offset = 0;
            while (offset < size) {
                const auto count = Read(fd, text.data() + offset, std::min(size - offset, ReadChunkBytes));
                if (count <= 0)
                    return ReferenceRead::Failed;
                offset += static_cast<std::size_t>(count);
                if (!keep_going())
                    return ReferenceRead::Cancelled;
            }
            char extra;
            return Read(fd, &extra, 1) == 0 ? ReferenceRead::Complete : ReferenceRead::Failed;
        }

    }

    VerificationResult VerifySteamInstallation(std::string_view sd_mount, std::uint32_t app_id,
        std::string_view expected_reference_sha256, ProgressCallback progress, void *context) {
        VerificationResult result{Status::ReferenceUnavailable, 0, 0, 0, {}};
        const auto finish = [&](Status status) {
            result.status = status;
            return result;
        };
        const auto keep_going = [&]() {
            return progress == nullptr || progress(result.checked_files, result.total_files, result.compared_bytes, context);
        };
        if (!keep_going())
            return finish(Status::Cancelled);

        Digest expected{};
        if (app_id == 0 || !ParseDigest(expected_reference_sha256, expected, true))
            return finish(Status::InvalidReference);

        const auto id = std::to_string(app_id);
        const auto sd = OpenRoot(sd_mount);
        std::string text;
        {
            Descriptor reference;
            std::uint64_t size = 0;
            if (!sd.IsOpen() || OpenRegularFile(sd, "switch/exodus-steam/references/" + id + ".files", reference, size)
                != OpenStatus::Opened)
                return finish(Status::ReferenceUnavailable);
            if (size == 0 || size > ReferenceByteLimit)
                return finish(Status::InvalidReference);
            switch (ReadReference(reference.Get(), text, static_cast<std::size_t>(size), keep_going)) {
                case ReferenceRead::Failed: return finish(Status::ReferenceUnavailable);
                case ReferenceRead::Cancelled: return finish(Status::Cancelled);
                case ReferenceRead::Complete: break;
            }
        }

        if (!keep_going())
            return finish(Status::Cancelled);
        Sha256 reference_hash;
        Digest actual_reference{};
        if (!reference_hash.Update(text.data(), text.size()) || !reference_hash.Finish(actual_reference))
            return finish(Status::ReadFailure);
        if (!keep_going())
            return finish(Status::Cancelled);
        if (actual_reference != expected)
            return finish(Status::ReferenceDigestMismatch);

        std::vector<Record> records;
        const bool valid = ParseReference(text, id, records);
        if (valid)
            result.total_files = records.size();
        if (!keep_going())
            return finish(Status::Cancelled);
        if (!valid)
            return finish(Status::InvalidReference);

        const auto game_name = home::FindLocalSteamGameDirectory(sd_mount, app_id);
        if (!game_name)
            return finish(Status::InstallationUnavailable);
        const auto game_path = "switch/exodus-steam/steamapps/common/" + *game_name;
        for (const auto &record : records) {
            // Count the leading slash and the separator following the game directory.
            if (game_path.size() + 2 + record.path.size() > SdPathByteLimit) {
                result.path = record.path;
                return finish(Status::InvalidReference);
            }
        }
        const auto game = OpenDirectory(sd, game_path);
        if (!game.IsOpen())
            return finish(Status::InstallationUnavailable);

        std::vector<unsigned char> buffer(ReadChunkBytes);
        for (auto &record : records) {
            result.path = record.path;
            Descriptor file;
            std::uint64_t size = 0;
            struct stat before{};
            const auto opened = OpenRegularFile(game, record.path, file, size, &before);
            if (opened != OpenStatus::Opened)
                return finish(opened == OpenStatus::MissingOrUnsafe ? Status::MissingOrUnsafeFile : Status::ReadFailure);
            if (size != record.size)
                return finish(Status::SizeMismatch);
            record.compared_file = FileStamp(before);

            Sha256 hash;
            while (size != 0) {
                const auto want = static_cast<std::size_t>(std::min<std::uint64_t>(size, buffer.size()));
                const auto count = Read(file.Get(), buffer.data(), want);
                if (count < 0)
                    return finish(Status::ReadFailure);
                if (count == 0)
                    return finish(Status::SizeMismatch);
                const auto bytes = static_cast<std::size_t>(count);
                if (!hash.Update(buffer.data(), bytes))
                    return finish(Status::ReadFailure);
                size -= bytes;
                result.compared_bytes += bytes;
                if (!keep_going())
                    return finish(Status::Cancelled);
            }
            char extra;
            const auto count = Read(file.Get(), &extra, 1);
            if (count < 0)
                return finish(Status::ReadFailure);
            if (count != 0)
                return finish(Status::SizeMismatch);
            Digest actual{};
            if (!hash.Finish(actual))
                return finish(Status::ReadFailure);
            if (actual != record.digest)
                return finish(Status::HashMismatch);
            struct stat after{};
            if (fstat(file.Get(), &after) != 0)
                return finish(Status::ReadFailure);
            if (!record.compared_file.Matches(after))
                return finish(Status::ChangedDuringCheck);
            ++result.checked_files;
            if (!keep_going())
                return finish(Status::Cancelled);
        }

        // Resolve paths again: an updater can replace a previously checked file
        // or directory while a later file is being hashed. Do not retain only
        // the original directory descriptor, which can still refer to removed data.
        result.path.clear();
        if (home::FindLocalSteamGameDirectory(sd_mount, app_id) != game_name)
            return finish(Status::ChangedDuringCheck);
        const auto current_sd = OpenRoot(sd_mount);
        if (!current_sd.IsOpen())
            return finish(Status::ChangedDuringCheck);
        const auto current_game = OpenDirectory(current_sd, game_path);
        if (!current_game.IsOpen())
            return finish(Status::ChangedDuringCheck);
        for (const auto &record : records) {
            result.path = record.path;
            if (!keep_going())
                return finish(Status::Cancelled);
            Descriptor current;
            std::uint64_t size = 0;
            struct stat metadata{};
            const auto opened = OpenRegularFile(current_game, record.path, current, size, &metadata);
            if (opened == OpenStatus::Failed)
                return finish(Status::ReadFailure);
            if (opened != OpenStatus::Opened || !record.compared_file.Matches(metadata))
                return finish(Status::ChangedDuringCheck);
        }

        result.path.clear();
        return finish(Status::Match);
    }

}
