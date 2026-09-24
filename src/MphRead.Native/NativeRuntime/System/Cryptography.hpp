#pragma once

// System.Security.Cryptography.MD5 and Convert's hex conversion: what the ROM
// whitelist checks a dump with.

#include <array>
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
}
