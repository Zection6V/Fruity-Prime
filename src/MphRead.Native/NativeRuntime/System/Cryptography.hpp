#pragma once

// System.Security.Cryptography.MD5 and Convert's hex conversion: what the ROM
// whitelist checks a dump with.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace MphRead::NativeRuntime
{
    // MD5.Create().ComputeHash(File.OpenRead(path)). Throws the same
    // System::IO exceptions opening and reading the file would.
    [[nodiscard]] std::array<std::uint8_t, 16> Md5ComputeHashOfFile(const std::string& path);
    // Convert.ToHexString(bytes): upper-case, two characters a byte.
    [[nodiscard]] std::string ConvertToHexString(std::span<const std::uint8_t> bytes);

    // IncrementalHash.CreateHash(HashAlgorithmName.SHA256): AppendData any
    // number of times, then GetHashAndReset, which leaves it ready for the
    // next message.
    class IncrementalSha256 final
    {
    public:
        IncrementalSha256() noexcept;

        void AppendData(std::span<const std::uint8_t> data);
        [[nodiscard]] std::array<std::uint8_t, 32> GetHashAndReset();

    private:
        void Transform(const std::uint8_t* block) noexcept;
        void Reset() noexcept;

        std::array<std::uint32_t, 8> _state{};
        std::array<std::uint8_t, 64> _block{};
        std::size_t _pending = 0;
        std::uint64_t _length = 0;
    };

    // SHA256.HashData(data).
    [[nodiscard]] std::array<std::uint8_t, 32> Sha256HashData(std::span<const std::uint8_t> data);
}
