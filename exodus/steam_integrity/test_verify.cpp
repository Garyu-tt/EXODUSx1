// SPDX-License-Identifier: GPL-2.0-only
#include "verify.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <vector>

using exodus::steam_integrity::ListReferenceAppIds;
using exodus::steam_integrity::Status;
using exodus::steam_integrity::VerificationResult;
using exodus::steam_integrity::VerifySteamInstallation;
namespace fs = std::filesystem;

namespace {

    constexpr std::uint32_t AppId = 730;
    constexpr std::size_t LargeSize = 131089;
    constexpr std::string_view AbcDigest = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    constexpr std::string_view EmptyDigest = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    constexpr std::string_view Reference =
        "EXODUS-STEAM-SHA256 1\n730\n3\n"
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\t3\tBin/engine.bin\n"
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\t0\tDocs/empty.dat\n"
        "e3c33f1a7c00a23610a13fa6b862df2882931be5b0262aeada7a47e282f1c679\t131089\tТест/large.bin\n";
    constexpr std::string_view ReferencePin = "e885694620199cb49268b09a421c9e7c925dabc2d41362f26a7e15f1de32e58a";

    [[noreturn]] void Fail(std::string_view message) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    void Require(bool condition, std::string_view message) {
        if (!condition)
            Fail(message);
    }

    void Expect(const VerificationResult &result, Status expected, std::string_view label) {
        if (result.status != expected) {
            std::cerr << label << " expected " << static_cast<unsigned>(expected)
                << " but got " << static_cast<unsigned>(result.status) << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    std::string Sha256Hex(std::string_view text) {
        std::array<unsigned char, 32> hash{};
        unsigned size = 0;
        Require(EVP_Digest(text.data(), text.size(), hash.data(), &size, EVP_sha256(), nullptr) == 1
                && size == hash.size(), "Could not hash the test reference");
        constexpr std::string_view hex = "0123456789abcdef";
        std::string result;
        for (const auto byte : hash) {
            result += hex[byte >> 4];
            result += hex[byte & 0x0f];
        }
        return result;
    }

    void Write(const fs::path &path, std::string_view text) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.write(text.data(), static_cast<std::streamsize>(text.size())))
            Fail("Could not write an integrity-test fixture");
    }

    class Fixture {
    public:
        Fixture() {
            auto path = (fs::temp_directory_path() / "exodus-steam-integrity-XXXXXX").string();
            if (mkdtemp(path.data()) == nullptr)
                Fail("Could not create an integrity-test directory");
            mount = path;
            fs::create_directories(ReferenceFile().parent_path());
            for (const auto *directory : {"Bin", "Docs", "Тест"})
                fs::create_directories(Game() / directory);
            Write(Acf(), "AppState { appid 730 installdir \"Example Game\" StateFlags 4 }");
            Write(Game() / "Bin/engine.bin", "abc");
            Write(Game() / "Docs/empty.dat", "");
            Write(Game() / "Тест/large.bin", std::string(LargeSize, 'x'));
            Write(Game() / "unlisted.bin", "An unlisted file");
            Write(ReferenceFile(), Reference);
        }

        ~Fixture() {
            std::error_code ignored;
            fs::remove_all(mount, ignored);
        }

        fs::path ReferenceFile() const { return mount / "switch/exodus-steam/references/730.files"; }
        fs::path Acf() const { return mount / "switch/exodus-steam/steamapps/appmanifest_730.acf"; }
        fs::path Game() const { return mount / "switch/exodus-steam/steamapps/common/Example Game"; }

        VerificationResult Verify(std::string_view pin = ReferencePin) const {
            return VerifySteamInstallation(mount.string(), AppId, pin);
        }

        VerificationResult WithReference(std::string_view text) const {
            Write(ReferenceFile(), text);
            return Verify(Sha256Hex(text));
        }

        fs::path mount;
    };

    void TestReferenceAppIds() {
        Fixture fixture;
        const auto references = fixture.ReferenceFile().parent_path();
        const auto mount = fixture.mount.string();
        for (const auto *name : {"10.files", "2.files", "4294967295.files"})
            Write(references / name, "Reference filename hint");
        fs::resize_file(references / "4294967295.files", 8 * 1024 * 1024);

        for (const auto *name : {"0.files", "01.files", "4294967296.files", "+5.files", "3.FILES", "4.files.bak", ".files"})
            Write(references / name, "Not a candidate name");
        fs::create_directory(references / "15.files");
        fs::create_symlink(fixture.ReferenceFile(), references / "16.files");
        Require(mkfifo((references / "17.files").c_str(), 0600) == 0, "Could not create a FIFO reference fixture");
        Write(references / "18.files", "");
        Write(references / "19.files", "");
        fs::resize_file(references / "19.files", 8 * 1024 * 1024 + 1);

        const auto ids = ListReferenceAppIds(mount);
        const std::vector<std::uint32_t> expected{2, 10, AppId, 4294967295u};
        Require(ids && *ids == expected, "Reference names must be canonical, backed by safe files and sorted numerically");
        Expect(VerifySteamInstallation(mount, 10, ReferencePin), Status::ReferenceDigestMismatch,
            "A listed filename still requires the independently supplied digest");

        const auto moved = fixture.mount / "moved-references";
        fs::rename(references, moved);
        Require(!ListReferenceAppIds(mount), "A missing reference directory has no list");
        fs::create_directory_symlink(moved, references);
        Require(!ListReferenceAppIds(mount), "A reference directory symlink must be rejected");
        fs::remove(references);
        Write(references, "A directory is required");
        Require(!ListReferenceAppIds(mount), "A regular file cannot serve as the reference directory");
        fs::remove(references);
        fs::create_directory(references);
        const auto empty = ListReferenceAppIds(mount);
        Require(empty && empty->empty(), "A readable empty reference directory has an empty list");
    }

    void TestReferenceDirectoryLimit() {
        Fixture fixture;
        const auto references = fixture.ReferenceFile().parent_path();
        const auto mount = fixture.mount.string();
        for (unsigned i = 0; i < 511; ++i)
            Write(references / (std::to_string(i) + ".txt"), "");
        const auto ids = ListReferenceAppIds(mount);
        Require(ids && *ids == std::vector<std::uint32_t>{AppId}, "The picker allows exactly 512 directory entries");
        Write(references / "extra.txt", "");
        Require(!ListReferenceAppIds(mount), "An oversized directory has no list, including when its extra entries are unrelated");
    }

    std::string ReferenceHeader(std::string_view id = "730", std::string_view count = "1") {
        return "EXODUS-STEAM-SHA256 1\n" + std::string(id) + '\n' + std::string(count) + '\n';
    }

    std::string Record(std::string_view path, std::string_view size = "0", std::string_view digest = EmptyDigest) {
        return std::string(digest) + '\t' + std::string(size) + '\t' + std::string(path) + '\n';
    }

    bool ContinueUntil(std::size_t, std::size_t, std::uint64_t compared_bytes, void *context) {
        return compared_bytes < *static_cast<const std::uint64_t *>(context);
    }

    bool CancelAfterReferenceChunks(std::size_t checked, std::size_t total, std::uint64_t compared_bytes, void *context) {
        Require(checked == 0 && total == 0 && compared_bytes == 0, "Reference reads must not count as comparisons");
        return ++*static_cast<std::size_t *>(context) < 6;
    }

    struct Mutation {
        std::size_t checked;
        std::uint64_t bytes;
        std::function<void()> action;
        bool performed = false;
    };

    bool MutateDuringCheck(std::size_t checked, std::size_t total, std::uint64_t bytes, void *context) {
        auto &mutation = *static_cast<Mutation *>(context);
        if (!mutation.performed && total != 0 && checked >= mutation.checked && bytes >= mutation.bytes) {
            mutation.action();
            mutation.performed = true;
        }
        return true;
    }

    void CheckMutation(const Fixture &fixture, Mutation &mutation, std::string_view path, std::string_view label) {
        const auto result = VerifySteamInstallation(fixture.mount.string(), AppId, ReferencePin,
            MutateDuringCheck, &mutation);
        Require(mutation.performed, "The fixture must change during the check");
        Expect(result, Status::ChangedDuringCheck, label);
        Require(result.path == path && result.total_files == 3 && result.checked_files <= 3
                && result.compared_bytes <= LargeSize + 3,
            "A changed file must retain its path and bounded comparison accounting");
    }

    bool CancelDuringRecheck(std::size_t checked, std::size_t total, std::uint64_t, void *context) {
        if (total != 0 && checked == total)
            return ++*static_cast<std::size_t *>(context) < 2;
        return true;
    }

}

int main() {
    TestReferenceAppIds();
    TestReferenceDirectoryLimit();
    Fixture fixture;
    Require(Sha256Hex(Reference) == ReferencePin, "Known SHA-256 reference pin changed");
    const auto matched = fixture.Verify("E885694620199CB49268b09A421C9E7C925dabc2d41362f26a7E15F1DE32E58A");
    Expect(matched, Status::Match, "All listed files match the pinned reference");
    Require(matched.checked_files == 3 && matched.total_files == 3 && matched.compared_bytes == LargeSize + 3
            && matched.path.empty(), "The complete streamed comparison must account for all listed files and bytes");

    {
        Fixture changed;
        const auto path = changed.Game() / "Тест/large.bin";
        Mutation replacement{0, LargeSize + 3, [&] {
            fs::rename(path, path.string() + ".old");
            Write(path, std::string(LargeSize, 'y'));
        }};
        CheckMutation(changed, replacement, "Тест/large.bin", "Replacement after the last read cannot match");
    }
    {
        Fixture changed;
        const auto path = changed.Game() / "Тест/large.bin";
        Mutation overwrite{2, 64 * 1024 + 3, [&] {
            // Change only bytes already hashed; the remaining bytes still match.
            const auto timestamp = fs::last_write_time(path);
            std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
            Require(static_cast<bool>(file.write("y", 1)), "Could not mutate an already read byte");
            file.close();
            fs::last_write_time(path, timestamp + std::chrono::seconds(2));
        }};
        CheckMutation(changed, overwrite, "Тест/large.bin", "In-place changes to previously read bytes cannot match");
    }
    for (const bool remove : {false, true}) {
        Fixture changed;
        const auto path = changed.Game() / "Bin/engine.bin";
        Mutation earlier{2, 64 * 1024 + 3, [&] {
            if (remove) {
                fs::remove(path);
            } else {
                const auto timestamp = fs::last_write_time(path);
                Write(path, "def");
                fs::last_write_time(path, timestamp + std::chrono::seconds(2));
            }
        }};
        CheckMutation(changed, earlier, "Bin/engine.bin", "Recheck a changed or removed earlier file");
    }
    for (const bool replace : {false, true}) {
        Fixture changed;
        const auto path = changed.Game() / "Docs/empty.dat";
        Mutation empty{2, 3, [&] {
            if (replace) {
                fs::rename(path, path.string() + ".old");
                Write(path, "");
            } else {
                Write(path, "not empty anymore");
            }
        }};
        CheckMutation(changed, empty, "Docs/empty.dat", "Zero-byte files need the same final identity check");
    }
    {
        Fixture changed;
        Mutation directory{0, LargeSize + 3, [&] {
            const auto original = changed.mount / "original-game";
            fs::rename(changed.Game(), original);
            fs::copy(original, changed.Game(), fs::copy_options::recursive);
        }};
        CheckMutation(changed, directory, "Bin/engine.bin", "Reopen the game directory before checking current paths");
    }
    std::size_t final_polls = 0;
    const auto stopped_during_recheck = VerifySteamInstallation(fixture.mount.string(), AppId, ReferencePin,
        CancelDuringRecheck, &final_polls);
    Expect(stopped_during_recheck, Status::Cancelled, "The final metadata pass remains cancellable");
    Require(final_polls == 2 && stopped_during_recheck.checked_files == 3
            && stopped_during_recheck.compared_bytes == LargeSize + 3,
        "Metadata checks do not count file bytes or completed hashes twice");

    Write(fixture.Game() / "Тест/large.bin", std::string(LargeSize, 'y'));
    const auto corrupted = fixture.Verify();
    Expect(corrupted, Status::HashMismatch, "Same-size corruption in the last file");
    Require(corrupted.checked_files == 2 && corrupted.total_files == 3 && corrupted.compared_bytes == LargeSize + 3
            && corrupted.path == "Тест/large.bin", "A hash failure must identify the first failed file");
    Write(fixture.Game() / "Тест/large.bin", std::string(LargeSize - 1, 'x'));
    const auto too_short = fixture.Verify();
    Expect(too_short, Status::SizeMismatch, "A truncated file must fail its exact size check");
    Require(too_short.checked_files == 2 && too_short.compared_bytes == 3, "Wrong-size file must be rejected before hashing");
    Write(fixture.Game() / "Тест/large.bin", std::string(LargeSize, 'x'));

    std::uint64_t stop_at = 0;
    const auto stopped_initially = VerifySteamInstallation(fixture.mount.string(), AppId, ReferencePin, ContinueUntil, &stop_at);
    Expect(stopped_initially, Status::Cancelled, "Cancel before any filesystem work");
    Require(stopped_initially.checked_files == 0 && stopped_initially.compared_bytes == 0, "Initial cancellation must read no game bytes");

    Write(fixture.ReferenceFile(), std::string(8 * 64 * 1024, 'x'));
    std::size_t reference_polls = 0;
    const auto stopped_during_reference = VerifySteamInstallation(fixture.mount.string(), AppId, ReferencePin,
        CancelAfterReferenceChunks, &reference_polls);
    Expect(stopped_during_reference, Status::Cancelled, "Cancel during a multi-chunk reference before hashing it");
    Require(reference_polls == 6 && stopped_during_reference.total_files == 0 && stopped_during_reference.path.empty(),
        "Reference cancellation must stop before parsing or accessing the installation");
    Write(fixture.ReferenceFile(), Reference);

    stop_at = 64 * 1024;
    const auto stopped_within_file = VerifySteamInstallation(fixture.mount.string(), AppId, ReferencePin, ContinueUntil, &stop_at);
    Expect(stopped_within_file, Status::Cancelled, "Cancel within the large file");
    Require(stopped_within_file.checked_files == 2 && stopped_within_file.compared_bytes == 64 * 1024 + 3
            && stopped_within_file.path == "Тест/large.bin", "Large-file cancellation must observe bounded progress");

    fs::remove(fixture.Acf());
    const auto bad_pin = fixture.Verify(std::string(64, '0'));
    Expect(bad_pin, Status::ReferenceDigestMismatch, "Independent digest mismatch precedes installation inspection");
    Require(bad_pin.total_files == 0 && bad_pin.compared_bytes == 0, "Do not interpret an untrusted reference");
    Expect(fixture.Verify("not a digest"), Status::InvalidReference, "Malformed pin");
    Expect(fixture.Verify(), Status::InstallationUnavailable, "Pinned reference with an absent AppState");
    Write(fixture.Acf(), "AppState { appid 730 installdir \"Example Game\" StateFlags 4 }");

    const auto wrong_app = fixture.WithReference(ReferenceHeader("731") + Record("Docs/empty.dat"));
    Expect(wrong_app, Status::InvalidReference, "Matching pin cannot substitute another AppID");
    for (const auto count : {"0", "01", "32769", "18446744073709551616"})
        Expect(fixture.WithReference(ReferenceHeader("730", count) + Record("Docs/empty.dat")), Status::InvalidReference,
            "Reference file-count grammar and cap");
    Expect(fixture.WithReference(ReferenceHeader("730", "2") + Record("Docs/empty.dat")), Status::InvalidReference, "Too few rows");
    Expect(fixture.WithReference(ReferenceHeader() + Record("Docs/empty.dat") + Record("Other.dat")),
        Status::InvalidReference, "Trailing uncounted row");
    Expect(fixture.WithReference(ReferenceHeader() + Record("Docs/empty.dat") + '\n'), Status::InvalidReference, "Trailing empty row");
    Expect(fixture.WithReference("\xef\xbb\xbf" + ReferenceHeader() + Record("Docs/empty.dat")),
        Status::InvalidReference, "Header does not accept a byte-order mark");
    auto no_lf = ReferenceHeader() + Record("Docs/empty.dat");
    no_lf.pop_back();
    Expect(fixture.WithReference(no_lf), Status::InvalidReference, "Record requires final LF");
    Expect(fixture.WithReference("EXODUS-STEAM-SHA256 1\r\n730\n1\n" + Record("Docs/empty.dat")),
        Status::InvalidReference, "Carriage-return delimiter is invalid");
    Expect(fixture.WithReference(ReferenceHeader() + Record("Docs/empty.dat", "0", std::string(64, 'A'))),
        Status::InvalidReference, "A row digest must be lowercase");
    for (const auto size : {"00", "+0", "-1", "1e2", "18446744073709551616"})
        Expect(fixture.WithReference(ReferenceHeader() + Record("Docs/empty.dat", size)), Status::InvalidReference,
            "Canonical uint64 size grammar");
    Expect(fixture.WithReference(ReferenceHeader("730", "2")
        + Record("a", "18446744073709551615") + Record("b", "1")), Status::InvalidReference, "Aggregate size overflow");
    Expect(fixture.WithReference(ReferenceHeader("730", "2") + Record("Z") + Record("a")),
        Status::InvalidReference, "Rows must be sorted by ASCII-folded path");
    Expect(fixture.WithReference(ReferenceHeader("730", "2") + Record("Data") + Record("data")),
        Status::InvalidReference, "Duplicate case aliases");
    Expect(fixture.WithReference(ReferenceHeader("730", "2") + Record("Data/item") + Record("data/other")),
        Status::InvalidReference, "Aliased directory spelling");

    auto nul = std::string("a");
    nul += '\0';
    nul += 'b';
    const std::vector<std::string> invalid_paths{"", "/absolute", "./a", "../a", "Data/../a", "a//b", "a/", "a\\b", "a:b",
        "a?b", "a\"b", "a\tb", "a\x7f" "b", "a\xc2\x82" "b", "\xed\xa0\x80", "dir/a.", "dir/a ", "dir/ a", "NUL.txt",
        "COM¹.bin", "CONIN$ .txt", "dir/LpT9.txt", std::string(256, 'x'), nul};
    for (const auto &path : invalid_paths) {
        const auto invalid = fixture.WithReference(ReferenceHeader() + Record(path));
        Expect(invalid, Status::InvalidReference, "Unsafe relative path");
        Require(invalid.compared_bytes == 0, "Validate reference paths before reading game bytes");
    }
    const auto component = std::string(255, 'x');
    Expect(fixture.WithReference(ReferenceHeader() + Record(component + '/' + component + '/' + component + '/' + component + "/x")),
        Status::InvalidReference, "Relative path byte cap");
    Expect(fixture.WithReference(std::string(8 * 1024 * 1024 + 1, 'x')), Status::InvalidReference, "Reference byte cap");
    const auto invalid_late = fixture.WithReference(ReferenceHeader("730", "2") + Record("Docs/empty.dat") + Record("../outside"));
    Expect(invalid_late, Status::InvalidReference, "A late invalid row rejects the entire reference");
    Require(invalid_late.checked_files == 0 && invalid_late.compared_bytes == 0, "All rows must be validated before comparisons");

    constexpr std::string_view native_game_prefix = "/switch/exodus-steam/steamapps/common/Example Game/";
    const auto nested = std::string(240, 'z') + '/' + std::string(240, 'b') + '/';
    const auto first_row = Record("Bin/engine.bin", "3", AbcDigest);
    const auto longest_native = nested + "\xc3\x96" + std::string(768 - native_game_prefix.size() - nested.size() - 2, 'c');
    const auto too_long_native = longest_native + 'd';
    fs::create_directories((fixture.Game() / longest_native).parent_path());
    Write(fixture.Game() / longest_native, "");
    Write(fixture.Game() / too_long_native, "");
    const auto longest = fixture.WithReference(ReferenceHeader("730", "2") + first_row + Record(longest_native));
    Expect(longest, Status::Match, "Complete native UTF-8 path is exactly 768 bytes");
    Require(longest.checked_files == 2 && longest.compared_bytes == 3, "Exact native limit still verifies both files");
    const auto too_long = fixture.WithReference(ReferenceHeader("730", "2") + first_row + Record(too_long_native));
    Expect(too_long, Status::InvalidReference, "Complete native UTF-8 path exceeds 768 bytes");
    Require(too_long.path == too_long_native && too_long.checked_files == 0 && too_long.compared_bytes == 0,
        "Report the oversized path before reading even the earlier valid file");

    Write(fixture.ReferenceFile(), Reference);
    const auto opened_file = fixture.Game() / "Bin/engine.bin";
    fs::remove(opened_file);
    Expect(fixture.Verify(), Status::MissingOrUnsafeFile, "Missing listed file");
    const auto other_file = fixture.mount / "outside.bin";
    Write(other_file, "abc");
    fs::create_symlink(other_file, opened_file);
    Expect(fixture.Verify(), Status::MissingOrUnsafeFile, "A symlink is rejected even when its external target would match");
    fs::remove(opened_file);
    if (mkfifo(opened_file.c_str(), 0600) != 0)
        Fail("Could not create a nonregular-file fixture");
    Expect(fixture.Verify(), Status::MissingOrUnsafeFile, "A FIFO is rejected without reading from it");
    fs::remove(opened_file);
    fs::remove(opened_file.parent_path());
    const auto other_directory = fixture.mount / "outside";
    fs::create_directories(other_directory);
    Write(other_directory / "engine.bin", "abc");
    fs::create_directory_symlink(other_directory, opened_file.parent_path());
    Expect(fixture.Verify(), Status::MissingOrUnsafeFile, "A symlinked component is rejected even when its content would match");

    fs::remove(fixture.ReferenceFile());
    Expect(fixture.Verify(), Status::ReferenceUnavailable, "Missing reference file");
    const auto outside_reference = fixture.mount / "outside.files";
    Write(outside_reference, Reference);
    fs::create_symlink(outside_reference, fixture.ReferenceFile());
    Expect(fixture.Verify(), Status::ReferenceUnavailable, "Reject a reference-file symlink");

    std::cout << "Steam file integrity checks passed\n";
}
