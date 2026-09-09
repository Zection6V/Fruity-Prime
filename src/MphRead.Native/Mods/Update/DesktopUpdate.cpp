/*
 * Native counterpart of MphRead/Mods/Update/DesktopUpdate.cs.
 *
 * The copy is deliberately split into two processes.  The release package is
 * unpacked below the running installation, the staged executable is started
 * with -applyupdate, and this process then exits.  The staged process is
 * therefore the one that copies the new files over the old executable.
 */
#include "Mods/branding.hpp"
#include "Mods/Update/update.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fruityprime::update {
namespace {

constexpr std::size_t MaxExpandedBytes = 512U * 1024U * 1024U;
constexpr std::size_t MaxArchiveEntries = 100'000;

std::string& error_message() {
    static std::string value;
    return value;
}

void remember_error(std::string message) {
    error_message() = std::move(message);
}

[[nodiscard]] std::filesystem::path staging_directory(
    const std::filesystem::path& base_directory) {
    return base_directory / ".update";
}

[[nodiscard]] std::filesystem::path staged_directory(
    const std::filesystem::path& base_directory) {
    return staging_directory(base_directory) / "staged";
}

[[nodiscard]] std::vector<std::uint8_t> read_binary_file(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open update package "
                                 + path.string());
    }
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw std::runtime_error("could not read update package size");
    }
    input.seekg(0, std::ios::beg);
    if (static_cast<unsigned long long>(length)
        > static_cast<unsigned long long>(MaxExpandedBytes)) {
        throw std::runtime_error("update package is too large");
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    if (!data.empty()) {
        input.read(reinterpret_cast<char*>(data.data()), length);
        if (!input) {
            throw std::runtime_error("could not read update package");
        }
    }
    return data;
}

[[nodiscard]] bool in_range(std::size_t offset, std::size_t length,
                            std::size_t total) noexcept {
    return offset <= total && length <= total - offset;
}

[[nodiscard]] std::uint16_t little16(
    const std::vector<std::uint8_t>& data, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(data[offset])
        | static_cast<std::uint16_t>(data[offset + 1] << 8);
}

[[nodiscard]] std::uint32_t little32(
    const std::vector<std::uint8_t>& data, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(data[offset])
        | (static_cast<std::uint32_t>(data[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(data[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

[[nodiscard]] std::string field_string(const std::uint8_t* data,
                                       std::size_t length) {
    std::size_t end = 0;
    while (end < length && data[end] != 0) {
        ++end;
    }
    return std::string(reinterpret_cast<const char*>(data), end);
}

[[nodiscard]] std::optional<std::filesystem::path> safe_relative_path(
    std::string_view name) {
    if (name.empty() || name.find('\0') != std::string_view::npos
        || name.find('\\') != std::string_view::npos) {
        return std::nullopt;
    }
    while (!name.empty() && name.back() == '/') {
        name.remove_suffix(1);
    }
    if (name.empty()) {
        return std::nullopt;
    }
    const std::filesystem::path relative{std::string(name)};
    if (relative.empty() || relative.is_absolute() || relative.has_root_name()
        || relative.has_root_directory()) {
        return std::nullopt;
    }
    const std::filesystem::path normalized = relative.lexically_normal();
    if (normalized.empty() || normalized == std::filesystem::path(".")) {
        return std::nullopt;
    }
    for (const auto& component : normalized) {
        if (component == std::filesystem::path("..")
            || component == std::filesystem::path(".")) {
            return std::nullopt;
        }
    }
    return normalized;
}

[[nodiscard]] bool create_directory(const std::filesystem::path& path,
                                    std::string& error) {
    std::error_code code;
    std::filesystem::create_directories(path, code);
    if (code) {
        error = "could not create update directory " + path.string() + ": "
            + code.message();
        return false;
    }
    return true;
}

[[nodiscard]] bool write_entry(const std::filesystem::path& destination,
                               std::string_view name,
                               std::span<const std::uint8_t> data,
                               std::string& error) {
    const auto relative = safe_relative_path(name);
    if (!relative) {
        error = "update package contains an unsafe path: " + std::string(name);
        return false;
    }
    const std::filesystem::path output = destination / *relative;
    if (!create_directory(output.parent_path(), error)) {
        return false;
    }
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "could not create staged file " + output.string();
        return false;
    }
    if (!data.empty()) {
        file.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
    }
    if (!file) {
        error = "could not write staged file " + output.string();
        return false;
    }
    return true;
}

[[nodiscard]] bool inflate_buffer(
    std::span<const std::uint8_t> compressed, int window_bits,
    std::vector<std::uint8_t>& output, std::optional<std::size_t> expected,
    std::string& error) {
    output.clear();
    if (compressed.size() > std::numeric_limits<uInt>::max()) {
        error = "compressed update member is too large";
        return false;
    }
    if (expected && *expected > MaxExpandedBytes) {
        error = "update member expands beyond the safety limit";
        return false;
    }

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(
        reinterpret_cast<const Bytef*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, window_bits) != Z_OK) {
        error = "could not initialize update decompressor";
        return false;
    }

    std::array<std::uint8_t, 64U * 1024U> chunk{};
    for (;;) {
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        const int result = inflate(&stream, Z_NO_FLUSH);
        const std::size_t produced = chunk.size() - stream.avail_out;
        if (produced != 0) {
            if (output.size() > MaxExpandedBytes - produced) {
                inflateEnd(&stream);
                error = "update archive expands beyond the safety limit";
                return false;
            }
            output.insert(output.end(), chunk.begin(),
                          chunk.begin() + static_cast<std::ptrdiff_t>(produced));
        }
        if (result == Z_STREAM_END) {
            break;
        }
        if (result != Z_OK) {
            inflateEnd(&stream);
            error = "update archive has invalid compressed data";
            return false;
        }
        if (stream.avail_in == 0 && produced == 0) {
            inflateEnd(&stream);
            error = "update archive ended before the compressed stream";
            return false;
        }
    }
    inflateEnd(&stream);
    if (expected && output.size() != *expected) {
        error = "update archive has an unexpected uncompressed size";
        return false;
    }
    return true;
}

[[nodiscard]] bool verify_crc(std::span<const std::uint8_t> data,
                              std::uint32_t expected,
                              std::string& error) {
    if (data.size() > std::numeric_limits<uInt>::max()) {
        error = "update member is too large for CRC verification";
        return false;
    }
    const uLong actual = crc32(
        crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(data.data()),
        static_cast<uInt>(data.size()));
    if (static_cast<std::uint32_t>(actual) != expected) {
        error = "update archive failed its CRC check";
        return false;
    }
    return true;
}

[[nodiscard]] bool extract_zip(const std::vector<std::uint8_t>& archive,
                               const std::filesystem::path& destination,
                               std::string& error) {
    if (archive.size() < 22) {
        error = "update ZIP is too short";
        return false;
    }
    const std::size_t search_start = archive.size() - 22;
    const std::size_t search_end = archive.size() > 22 + 65'535
        ? archive.size() - 22 - 65'535 : 0;
    std::optional<std::size_t> eocd;
    for (std::size_t position = search_start;; --position) {
        if (little32(archive, position) == 0x06054b50U) {
            eocd = position;
            break;
        }
        if (position == search_end) {
            break;
        }
    }
    if (!eocd || !in_range(*eocd, 22, archive.size())) {
        error = "update ZIP has no end-of-directory record";
        return false;
    }
    const std::size_t end = *eocd;
    const std::uint16_t disk = little16(archive, end + 4);
    const std::uint16_t central_disk = little16(archive, end + 6);
    const std::uint16_t entries_on_disk = little16(archive, end + 8);
    const std::uint16_t entries = little16(archive, end + 10);
    const std::uint32_t central_size = little32(archive, end + 12);
    const std::uint32_t central_offset = little32(archive, end + 16);
    const std::uint16_t comment_size = little16(archive, end + 20);
    if (disk != 0 || central_disk != 0 || entries_on_disk != entries
        || entries == 0xffffU || central_size == 0xffffffffU
        || central_offset == 0xffffffffU
        || !in_range(end, 22U + comment_size, archive.size())
        || !in_range(central_offset, central_size, archive.size())
        || static_cast<std::size_t>(central_offset) + central_size > end) {
        error = "update ZIP uses an unsupported layout";
        return false;
    }
    if (entries > MaxArchiveEntries) {
        error = "update ZIP contains too many entries";
        return false;
    }
    if (!create_directory(destination, error)) {
        return false;
    }

    std::size_t cursor = central_offset;
    for (std::size_t entry_index = 0; entry_index < entries; ++entry_index) {
        if (!in_range(cursor, 46, archive.size())
            || little32(archive, cursor) != 0x02014b50U) {
            error = "update ZIP has an invalid central-directory entry";
            return false;
        }
        const std::uint16_t flags = little16(archive, cursor + 8);
        const std::uint16_t method = little16(archive, cursor + 10);
        const std::uint32_t crc = little32(archive, cursor + 16);
        const std::uint32_t compressed_size = little32(archive, cursor + 20);
        const std::uint32_t uncompressed_size = little32(archive, cursor + 24);
        const std::uint16_t name_size = little16(archive, cursor + 28);
        const std::uint16_t extra_size = little16(archive, cursor + 30);
        const std::uint16_t comment_length = little16(archive, cursor + 32);
        const std::uint32_t local_offset = little32(archive, cursor + 42);
        const std::size_t central_length = 46U + name_size + extra_size
            + comment_length;
        if (!in_range(cursor, central_length, archive.size())
            || (flags & 0x0001U) != 0
            || compressed_size > MaxExpandedBytes
            || uncompressed_size > MaxExpandedBytes) {
            error = "update ZIP has an unsupported or oversized entry";
            return false;
        }

        const std::string name(
            reinterpret_cast<const char*>(archive.data() + cursor + 46),
            name_size);
        const bool directory = !name.empty() && name.back() == '/';
        const auto relative = safe_relative_path(name);
        if (!relative) {
            error = "update package contains an unsafe path: " + name;
            return false;
        }
        const std::filesystem::path output = destination / *relative;
        if (directory) {
            if (!create_directory(output, error)) {
                return false;
            }
        } else {
            if (!in_range(local_offset, 30, archive.size())
                || little32(archive, local_offset) != 0x04034b50U) {
                error = "update ZIP has an invalid local-file header";
                return false;
            }
            const std::uint16_t local_name_size =
                little16(archive, local_offset + 26);
            const std::uint16_t local_extra_size =
                little16(archive, local_offset + 28);
            const std::size_t data_offset = static_cast<std::size_t>(local_offset)
                + 30U + local_name_size + local_extra_size;
            if (!in_range(data_offset, compressed_size, archive.size())) {
                error = "update ZIP has truncated file data";
                return false;
            }
            const auto compressed = std::span<const std::uint8_t>(
                archive.data() + data_offset, compressed_size);
            std::vector<std::uint8_t> plain;
            if (method == 0) {
                plain.assign(compressed.begin(), compressed.end());
                if (plain.size() != uncompressed_size) {
                    error = "update ZIP stored entry has a wrong size";
                    return false;
                }
            } else if (method == 8) {
                if (!inflate_buffer(compressed, -MAX_WBITS, plain,
                                    uncompressed_size, error)) {
                    return false;
                }
            } else {
                error = "update ZIP uses an unsupported compression method";
                return false;
            }
            if (!verify_crc(plain, crc, error)
                || !write_entry(destination, name, plain, error)) {
                return false;
            }
        }
        cursor += central_length;
    }
    return true;
}

[[nodiscard]] std::optional<std::uint64_t> parse_octal(
    const std::uint8_t* data, std::size_t length) {
    std::size_t index = 0;
    while (index < length && (data[index] == ' ' || data[index] == '\0')) {
        ++index;
    }
    if (index == length) {
        return std::uint64_t{0};
    }
    std::uint64_t result = 0;
    bool digit = false;
    for (; index < length && data[index] != '\0' && data[index] != ' '; ++index) {
        if (data[index] < '0' || data[index] > '7') {
            return std::nullopt;
        }
        digit = true;
        const std::uint64_t value = result * 8U + (data[index] - '0');
        if (value < result) {
            return std::nullopt;
        }
        result = value;
    }
    return digit ? std::optional<std::uint64_t>(result)
                 : std::optional<std::uint64_t>(std::uint64_t{0});
}

[[nodiscard]] bool zero_block(const std::uint8_t* data) noexcept {
    for (std::size_t index = 0; index < 512; ++index) {
        if (data[index] != 0) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool extract_tar_gz(
    const std::vector<std::uint8_t>& compressed,
    const std::filesystem::path& destination, std::string& error) {
    std::vector<std::uint8_t> archive;
    if (!inflate_buffer(compressed, 16 + MAX_WBITS, archive, std::nullopt,
                        error)) {
        return false;
    }
    if (!create_directory(destination, error)) {
        return false;
    }
    std::size_t offset = 0;
    std::size_t entries = 0;
    bool ended = false;
    while (offset + 512 <= archive.size()) {
        const std::uint8_t* header = archive.data() + offset;
        if (zero_block(header)) {
            ended = true;
            break;
        }
        if (++entries > MaxArchiveEntries) {
            error = "update tar contains too many entries";
            return false;
        }
        std::uint32_t checksum = 0;
        for (std::size_t index = 0; index < 512; ++index) {
            checksum += (index >= 148 && index < 156) ? 0x20U : header[index];
        }
        const auto stored_checksum = parse_octal(header + 148, 8);
        if (!stored_checksum || *stored_checksum != checksum) {
            error = "update tar has an invalid header checksum";
            return false;
        }
        std::string name = field_string(header, 100);
        const std::string prefix = field_string(header + 345, 155);
        if (!prefix.empty()) {
            name = prefix + "/" + name;
        }
        const auto relative = safe_relative_path(name);
        if (!relative) {
            error = "update package contains an unsafe path: " + name;
            return false;
        }
        const auto size = parse_octal(header + 124, 12);
        if (!size || *size > MaxExpandedBytes) {
            error = "update tar contains an oversized entry";
            return false;
        }
        const char type = static_cast<char>(header[156]);
        const std::size_t data_offset = offset + 512;
        if (*size > std::numeric_limits<std::size_t>::max() - data_offset
            || !in_range(data_offset, static_cast<std::size_t>(*size),
                        archive.size())) {
            error = "update tar contains truncated file data";
            return false;
        }
        const std::filesystem::path output = destination / *relative;
        if (type == '5') {
            if (!create_directory(output, error)) {
                return false;
            }
        } else if (type == '\0' || type == '0') {
            const auto data = std::span<const std::uint8_t>(
                archive.data() + data_offset, static_cast<std::size_t>(*size));
            if (!write_entry(destination, name, data, error)) {
                return false;
            }
        } else {
            error = "update tar contains an unsupported link or metadata entry";
            return false;
        }
        const std::size_t padded = static_cast<std::size_t>(*size)
            + (512U - (static_cast<std::size_t>(*size) % 512U)) % 512U;
        if (offset > std::numeric_limits<std::size_t>::max() - 512U - padded) {
            error = "update tar offset overflow";
            return false;
        }
        offset += 512U + padded;
    }
    if (!ended) {
        error = "update tar has no end-of-archive marker";
        return false;
    }
    return true;
}

[[nodiscard]] bool regular_file(const std::filesystem::path& path) noexcept {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

[[nodiscard]] std::optional<std::filesystem::path> find_binary(
    const std::filesystem::path& directory) {
    for (const bool server : {true, false}) {
        const auto candidate = directory / DesktopUpdate::binary_name(server);
        if (regular_file(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

void make_executable(const std::filesystem::path& path) noexcept {
#ifndef _WIN32
    std::error_code error;
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_exec
            | std::filesystem::perms::group_exec
            | std::filesystem::perms::others_exec,
        std::filesystem::perm_options::add, error);
    if (error) {
        std::cerr << "[update] could not make " << path.string()
                  << " executable: " << error.message() << '\n';
    }
#else
    static_cast<void>(path);
#endif
}

#ifdef _WIN32
[[nodiscard]] std::string quote_windows(std::string_view value) {
    std::string result;
    result.push_back('"');
    std::size_t backslashes = 0;
    for (const char character : value) {
        if (character == '\\') {
            ++backslashes;
            continue;
        }
        if (character == '"') {
            result.append(backslashes * 2U + 1U, '\\');
            result.push_back('"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, '\\');
        backslashes = 0;
        result.push_back(character);
    }
    result.append(backslashes * 2U, '\\');
    result.push_back('"');
    return result;
}
#endif

[[nodiscard]] bool start_process(
    const std::filesystem::path& executable,
    const std::filesystem::path& working_directory,
    std::span<const std::string> arguments, std::string& error) {
    std::error_code absolute_error;
    const auto absolute = std::filesystem::absolute(executable, absolute_error);
    const auto binary = !absolute_error ? absolute : executable;
    if (!regular_file(binary)) {
        error = "could not find update executable " + binary.string();
        return false;
    }
#ifdef _WIN32
    std::string command = quote_windows(binary.string());
    for (const std::string& argument : arguments) {
        command.push_back(' ');
        command += quote_windows(argument);
    }
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::string directory = working_directory.string();
    if (!CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, directory.c_str(), &startup, &process)) {
        error = "could not start update executable (Windows error "
            + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
#else
    std::vector<std::string> storage;
    storage.reserve(arguments.size() + 1);
    storage.push_back(binary.string());
    for (const std::string& argument : arguments) {
        storage.push_back(argument);
    }
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    for (std::string& value : storage) {
        argv.push_back(value.data());
    }
    argv.push_back(nullptr);
    const pid_t child = fork();
    if (child < 0) {
        error = "could not fork update executable";
        return false;
    }
    if (child == 0) {
        static_cast<void>(chdir(working_directory.string().c_str()));
        execv(binary.string().c_str(), argv.data());
        _exit(127);
    }
    return true;
#endif
}

void wait_for_exit(int pid) {
    if (pid > 0) {
#ifdef _WIN32
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE,
                                     static_cast<DWORD>(pid));
        if (process != nullptr) {
            if (WaitForSingleObject(process, 30'000) == WAIT_TIMEOUT) {
                std::cout << "[update] process " << pid
                          << " is still running; carrying on\n";
            }
            CloseHandle(process);
        }
#else
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(30);
        bool gone = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH) {
                gone = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!gone) {
            std::cout << "[update] process " << pid
                      << " is still running; carrying on\n";
        }
#endif
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
}

void copy_file_with_retries(const std::filesystem::path& source,
                            const std::filesystem::path& target) {
    std::string last_error;
    for (int attempt = 0; attempt <= 20; ++attempt) {
        std::error_code error;
        std::filesystem::copy_file(
            source, target, std::filesystem::copy_options::overwrite_existing,
            error);
        if (!error) {
            return;
        }
        last_error = error.message();
        if (attempt != 20) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
    throw std::runtime_error("could not copy " + source.string() + " to "
                             + target.string() + ": " + last_error);
}

void copy_tree(const std::filesystem::path& source,
               const std::filesystem::path& target) {
    std::error_code error;
    if (!std::filesystem::is_directory(source, error) || error) {
        throw std::runtime_error("staged update directory does not exist");
    }
    std::filesystem::create_directories(target, error);
    if (error) {
        throw std::runtime_error("could not create install directory: "
                                 + error.message());
    }
    std::filesystem::recursive_directory_iterator iterator(
        source, std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) {
        throw std::runtime_error("could not enumerate staged update: "
                                 + error.message());
    }
    for (; iterator != end; iterator.increment(error)) {
        if (error) {
            throw std::runtime_error("could not enumerate staged update: "
                                     + error.message());
        }
        const auto& entry = *iterator;
        if (entry.is_directory(error)) {
            if (error) {
                throw std::runtime_error("could not inspect staged directory: "
                                         + error.message());
            }
            continue;
        }
        if (entry.is_symlink(error) || error || !entry.is_regular_file(error)) {
            if (error) {
                throw std::runtime_error("could not inspect staged file: "
                                         + error.message());
            }
            throw std::runtime_error("staged update contains a non-file entry");
        }
        const auto relative = std::filesystem::relative(entry.path(), source,
                                                        error);
        if (error || relative.empty() || relative.is_absolute()
            || relative.lexically_normal() != relative) {
            throw std::runtime_error("staged update contains an unsafe path");
        }
        const auto destination = target / relative;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error) {
            throw std::runtime_error("could not create install subdirectory: "
                                     + error.message());
        }
        copy_file_with_retries(entry.path(), destination);
    }
}

} // namespace

bool DesktopUpdate::supported(
    const std::filesystem::path& base_directory) noexcept {
    if (!BuildVersion::current() || base_directory.empty()) {
        return false;
    }
    std::error_code error;
    if (!std::filesystem::is_directory(base_directory, error) || error) {
        return false;
    }
    const auto probe = base_directory / ".update-probe";
    std::ofstream output(probe, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.close();
    std::filesystem::remove(probe, error);
    return !error;
}

bool DesktopUpdate::stage(
    const UpdateInfo& update, const std::filesystem::path& base_directory,
    const std::function<void(float)>& progress) {
    remember_error({});
    if (update.asset_url.empty()) {
        remember_error("this release has no package for this platform");
        return false;
    }
    try {
        clean(base_directory);
        const auto staging = staging_directory(base_directory);
        const auto staged = staged_directory(base_directory);
        std::error_code error;
        std::filesystem::create_directories(staging, error);
        if (error) {
            throw std::runtime_error("could not create update staging area: "
                                     + error.message());
        }
        const bool zip = update.asset_name.size() >= 4
            && std::equal(update.asset_name.end() - 4, update.asset_name.end(),
                          ".zip", [](char left, char right) {
                              return std::tolower(static_cast<unsigned char>(left))
                                  == std::tolower(static_cast<unsigned char>(right));
                          });
        const auto archive = staging / (zip ? "package.zip" : "package.tar.gz");
        const DownloadResult downloaded = UpdateDownload::fetch(
            update.asset_url, archive.string(), update.asset_size, progress);
        if (!downloaded.ok) {
            throw std::runtime_error(downloaded.error.empty()
                                         ? "the download failed"
                                         : downloaded.error);
        }
        std::filesystem::create_directories(staged, error);
        if (error) {
            throw std::runtime_error("could not create staged update: "
                                     + error.message());
        }
        std::string extraction_error;
        if (!extract_package(archive, staged, zip, extraction_error)) {
            throw std::runtime_error(extraction_error);
        }
        std::filesystem::remove(archive, error);
        if (error) {
            throw std::runtime_error("could not remove downloaded update: "
                                     + error.message());
        }
        const auto binary = find_binary(staged);
        if (!binary) {
            throw std::runtime_error("the package does not contain a supported "
                                     "Fruity Prime executable");
        }
        make_executable(*binary);
        return true;
    } catch (const std::exception& exception) {
        remember_error(exception.what());
        std::cerr << "[update] could not stage the update: " << exception.what()
                  << '\n';
        return false;
    }
}

bool DesktopUpdate::extract_package(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination, bool zip, std::string& error) {
    error.clear();
    try {
        const auto data = read_binary_file(archive);
        if (zip) {
            return extract_zip(data, destination, error);
        }
        return extract_tar_gz(data, destination, error);
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool DesktopUpdate::launch(const std::filesystem::path& base_directory) {
    remember_error({});
    const auto staged = staged_directory(base_directory);
    const auto binary = find_binary(staged);
    if (!binary) {
        remember_error("there is no staged Fruity Prime executable to start");
        return false;
    }
    std::vector<std::string> arguments{
        "-" + std::string(ApplyFlag), base_directory.string(),
#ifdef _WIN32
        std::to_string(GetCurrentProcessId())
#else
        std::to_string(static_cast<long long>(getpid()))
#endif
    };
    std::string error;
    if (!start_process(*binary, staged, arguments, error)) {
        remember_error(error);
        std::cerr << "[update] could not start the update: " << error << '\n';
        return false;
    }
    return true;
}

int DesktopUpdate::apply(const std::filesystem::path& target_directory,
                         int wait_for_pid,
                         const std::filesystem::path& source_directory) {
    remember_error({});
    const auto source = source_directory.empty()
        ? std::filesystem::current_path() : source_directory;
    std::cout << "[update] applying to " << target_directory.string() << '\n';
    wait_for_exit(wait_for_pid);
    try {
        const auto source_binary = find_binary(source);
        if (!source_binary) {
            throw std::runtime_error(
                "staged update does not contain a supported executable");
        }
        std::error_code equivalent_error;
        if (std::filesystem::equivalent(source, target_directory,
                                        equivalent_error)
            && !equivalent_error) {
            throw std::runtime_error(
                "staged update and install directory are the same");
        }
        copy_tree(source, target_directory);
        const auto target_binary = target_directory / source_binary->filename();
        make_executable(target_binary);
        std::string restart_error;
        if (!start_process(target_binary, target_directory, {}, restart_error)) {
            throw std::runtime_error("updated, but could not restart: "
                                     + restart_error);
        }
    } catch (const std::exception& exception) {
        remember_error(exception.what());
        std::cout << "[update] the copy failed: " << exception.what() << '\n';
        std::cout << "[update] the new build is in " << source.string()
                  << " -- copy it over " << target_directory.string()
                  << " by hand\n";
        return 1;
    }
    std::cout << "[update] done\n";
    return 0;
}

void DesktopUpdate::clean(
    const std::filesystem::path& base_directory) noexcept {
    if (base_directory.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::remove_all(staging_directory(base_directory), error);
}

std::string DesktopUpdate::binary_name(bool server_build) {
    std::string name(branding::FileName);
    if (server_build) {
        name += "Server";
    }
#ifdef _WIN32
    name += ".exe";
#endif
    return name;
}

const std::string& DesktopUpdate::last_error() noexcept {
    return error_message();
}

} // namespace fruityprime::update
