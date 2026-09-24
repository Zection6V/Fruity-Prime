#include "Cryptography.hpp"

#include "Exceptions.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace MphRead::NativeRuntime
{
    namespace
    {
        // RFC 1321: the per-round shift amounts and the sine table.
        constexpr std::array<std::uint32_t, 64> Shifts = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

        constexpr std::array<std::uint32_t, 64> Sine = {
            0xD76AA478U, 0xE8C7B756U, 0x242070DBU, 0xC1BDCEEEU,
            0xF57C0FAFU, 0x4787C62AU, 0xA8304613U, 0xFD469501U,
            0x698098D8U, 0x8B44F7AFU, 0xFFFF5BB1U, 0x895CD7BEU,
            0x6B901122U, 0xFD987193U, 0xA679438EU, 0x49B40821U,
            0xF61E2562U, 0xC040B340U, 0x265E5A51U, 0xE9B6C7AAU,
            0xD62F105DU, 0x02441453U, 0xD8A1E681U, 0xE7D3FBC8U,
            0x21E1CDE6U, 0xC33707D6U, 0xF4D50D87U, 0x455A14EDU,
            0xA9E3E905U, 0xFCEFA3F8U, 0x676F02D9U, 0x8D2A4C8AU,
            0xFFFA3942U, 0x8771F681U, 0x6D9D6122U, 0xFDE5380CU,
            0xA4BEEA44U, 0x4BDECFA9U, 0xF6BB4B60U, 0xBEBFBC70U,
            0x289B7EC6U, 0xEAA127FAU, 0xD4EF3085U, 0x04881D05U,
            0xD9D4D039U, 0xE6DB99E5U, 0x1FA27CF8U, 0xC4AC5665U,
            0xF4292244U, 0x432AFF97U, 0xAB9423A7U, 0xFC93A039U,
            0x655B59C3U, 0x8F0CCC92U, 0xFFEFF47DU, 0x85845DD1U,
            0x6FA87E4FU, 0xFE2CE6E0U, 0xA3014314U, 0x4E0811A1U,
            0xF7537E82U, 0xBD3AF235U, 0x2AD7D2BBU, 0xEB86D391U};

        class Md5 final
        {
        public:
            void Append(const std::uint8_t* data, std::size_t size)
            {
                _length += static_cast<std::uint64_t>(size);
                while (size > 0)
                {
                    const std::size_t take = std::min<std::size_t>(size, 64U - _pending);
                    std::memcpy(_block.data() + _pending, data, take);
                    _pending += take;
                    data += take;
                    size -= take;
                    if (_pending == 64)
                    {
                        Transform(_block.data());
                        _pending = 0;
                    }
                }
            }

            [[nodiscard]] std::array<std::uint8_t, 16> Finish()
            {
                const std::uint64_t bits = _length * 8U;
                const std::uint8_t padding = 0x80U;
                Append(&padding, 1);
                const std::uint8_t zero = 0U;
                while (_pending != 56)
                {
                    Append(&zero, 1);
                }
                // The length goes in without being counted by Append.
                for (std::size_t i = 0; i < 8; ++i)
                {
                    _block[56 + i] = static_cast<std::uint8_t>((bits >> (8U * i)) & 0xFFU);
                }
                Transform(_block.data());
                std::array<std::uint8_t, 16> digest{};
                for (std::size_t word = 0; word < 4; ++word)
                {
                    for (std::size_t byte = 0; byte < 4; ++byte)
                    {
                        digest[word * 4 + byte] = static_cast<std::uint8_t>(
                            (_state[word] >> (8U * byte)) & 0xFFU);
                    }
                }
                return digest;
            }

        private:
            void Transform(const std::uint8_t* block) noexcept
            {
                std::array<std::uint32_t, 16> words{};
                for (std::size_t i = 0; i < 16; ++i)
                {
                    words[i] = static_cast<std::uint32_t>(block[i * 4])
                        | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 8)
                        | (static_cast<std::uint32_t>(block[i * 4 + 2]) << 16)
                        | (static_cast<std::uint32_t>(block[i * 4 + 3]) << 24);
                }
                std::uint32_t a = _state[0];
                std::uint32_t b = _state[1];
                std::uint32_t c = _state[2];
                std::uint32_t d = _state[3];
                for (std::uint32_t i = 0; i < 64; ++i)
                {
                    std::uint32_t f = 0;
                    std::uint32_t g = 0;
                    if (i < 16)
                    {
                        f = (b & c) | (~b & d);
                        g = i;
                    }
                    else if (i < 32)
                    {
                        f = (d & b) | (~d & c);
                        g = (5U * i + 1U) % 16U;
                    }
                    else if (i < 48)
                    {
                        f = b ^ c ^ d;
                        g = (3U * i + 5U) % 16U;
                    }
                    else
                    {
                        f = c ^ (b | ~d);
                        g = (7U * i) % 16U;
                    }
                    const std::uint32_t previous = d;
                    d = c;
                    c = b;
                    const std::uint32_t sum = a + f + Sine[i] + words[g];
                    b = b + std::rotl(sum, static_cast<int>(Shifts[i]));
                    a = previous;
                }
                _state[0] += a;
                _state[1] += b;
                _state[2] += c;
                _state[3] += d;
            }

            std::array<std::uint32_t, 4> _state{
                0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U};
            std::array<std::uint8_t, 64> _block{};
            std::size_t _pending = 0;
            std::uint64_t _length = 0;
        };
    }

    std::array<std::uint8_t, 16> Md5ComputeHashOfFile(const std::string& path)
    {
        std::ifstream file(
            std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary);
        if (!file)
        {
            throw System::IO::FileNotFoundException("Could not find file '" + path + "'.");
        }
        Md5 md5;
        std::vector<char> buffer(64U * 1024U);
        while (file)
        {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize read = file.gcount();
            if (read > 0)
            {
                md5.Append(reinterpret_cast<const std::uint8_t*>(buffer.data()),
                    static_cast<std::size_t>(read));
            }
        }
        if (file.bad())
        {
            throw System::IO::IOException("Could not read '" + path + "'.");
        }
        return md5.Finish();
    }

    std::string ConvertToHexString(std::span<const std::uint8_t> bytes)
    {
        static constexpr char Digits[] = "0123456789ABCDEF";
        std::string result;
        result.reserve(bytes.size() * 2);
        for (const std::uint8_t value : bytes)
        {
            result.push_back(Digits[value >> 4U]);
            result.push_back(Digits[value & 0x0FU]);
        }
        return result;
    }
}
