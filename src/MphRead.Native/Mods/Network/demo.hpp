#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::demo {

// This is the native counterpart of Mods/Network/DemoFile.cs.  The header is
// deliberately kept byte-for-byte compatible: FPDM, format 2, then the
// one-byte network protocol version.  Records after the header are a raw
// DEFLATE stream.
inline constexpr std::uint8_t FormatVersion = 2;
inline constexpr std::uint8_t LongGap = 0xff;
inline constexpr std::size_t HeaderSize = 6;
inline constexpr std::string_view Extension = ".fpdemo";

struct Record {
    std::uint32_t frame = 0;
    std::vector<std::uint8_t> data;
};

class Writer {
public:
    explicit Writer(const std::filesystem::path& path,
                    std::uint8_t protocol = net::NetConfig::ProtocolVersion);
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) = delete;
    Writer& operator=(Writer&&) = delete;
    ~Writer();

    // Frames are monotonically clamped just like the managed writer.  The
    // native writer uses stored DEFLATE blocks; that keeps this boundary
    // dependency-free while remaining readable by DeflateStream.
    void write_record(std::uint32_t frame, std::span<const std::uint8_t> data);
    void close();

private:
    void write_stored_blocks(std::span<const std::uint8_t> bytes);

    std::ofstream stream_;
    std::uint32_t last_frame_ = 0;
    bool closed_ = false;
};

class Reader {
public:
    // Bad magic, unsupported format, or an unreadable file returns nullopt.
    // A truncated Deflate tail is tolerated; complete records before that
    // point remain readable, matching the managed crash-recovery behavior.
    [[nodiscard]] static std::optional<Reader> open(
        const std::filesystem::path& path);

    Reader(Reader&&) noexcept = default;
    Reader& operator=(Reader&&) noexcept = default;
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    [[nodiscard]] std::uint8_t protocol_version() const noexcept {
        return protocol_version_;
    }
    [[nodiscard]] std::optional<Record> read_next();
    [[nodiscard]] std::size_t decompressed_size() const noexcept {
        return decompressed_.size();
    }
    [[nodiscard]] bool had_deflate_error() const noexcept {
        return deflate_error_;
    }

private:
    Reader(std::uint8_t protocol, std::vector<std::uint8_t> decompressed,
           bool deflate_error)
        : protocol_version_(protocol),
          decompressed_(std::move(decompressed)),
          deflate_error_(deflate_error) {}

    std::uint8_t protocol_version_ = 0;
    std::vector<std::uint8_t> decompressed_;
    std::size_t cursor_ = 0;
    std::uint32_t frame_ = 0;
    bool deflate_error_ = false;
};

} // namespace fruityprime::demo
