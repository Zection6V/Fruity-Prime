#include "Q3Bsp.hpp"

#include "../../Formats/Types.hpp"
#include "../../Program.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cctype>
#include <clocale>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include <mutex>
#include <random>
#include <sstream>
#include <string_view>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <locale.h>
#include <dlfcn.h>
#endif

using ::MphRead::NativeRuntime::AppendUtf8;
using ::MphRead::NativeRuntime::FileReadAllBytes;
using ::MphRead::NativeRuntime::OperationStatus;
using ::MphRead::NativeRuntime::PathGetFileName;
using ::MphRead::NativeRuntime::RuneDecodeFromUtf8;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::Utf16ToUtf8;
using ::MphRead::NativeRuntime::Utf8GetString;
using ::MphRead::NativeRuntime::Utf8Scalar;
using ::MphRead::NativeRuntime::Utf8ToUtf16;
using ::MphRead::NativeRuntime::Utf8ToUtf32;
using ::MphRead::NativeRuntime::Utf8ToWide;
using ::MphRead::NativeRuntime::WideToUtf8;

namespace
{
    using ByteVector = std::vector<std::uint8_t>;


#if !defined(_WIN32)
    [[nodiscard]] void* FindVersionedIcuSymbol(void* library, const char* base) noexcept
    {
        if (library == nullptr) return nullptr;
        if (void* symbol = dlsym(library, base); symbol != nullptr) return symbol;
        char name[96]{};
        for (int version = 99; version >= 50; --version)
        {
            const int count = std::snprintf(name, sizeof(name), "%s_%d", base, version);
            if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(name)) continue;
            if (void* symbol = dlsym(library, name); symbol != nullptr) return symbol;
        }
        return nullptr;
    }

    [[nodiscard]] std::uint32_t IcuUpper(std::uint32_t scalar) noexcept
    {
        using UpperFunction = std::int32_t (*)(std::int32_t);
        static UpperFunction upper = []() noexcept -> UpperFunction
        {
            void* library = dlopen("libicuuc.so", RTLD_LAZY | RTLD_LOCAL);
#if defined(__APPLE__)
            if (library == nullptr) library = dlopen("/usr/lib/libicucore.A.dylib", RTLD_LAZY | RTLD_LOCAL);
#endif
            return reinterpret_cast<UpperFunction>(FindVersionedIcuSymbol(library, "u_toupper"));
        }();
        if (upper == nullptr || scalar > 0x10FFFFU) return scalar;
        const std::int32_t mapped = upper(static_cast<std::int32_t>(scalar));
        return mapped < 0 ? scalar : static_cast<std::uint32_t>(mapped);
    }
#endif

    [[nodiscard]] std::uint32_t InvariantUpper(std::uint32_t scalar) noexcept
    {
        // .NET OrdinalIgnoreCase intentionally disables these two ICU uppercase
        // mappings so that dotless-i and long-s remain distinct in ordinal comparisons.
        if (scalar == 0x0131U || scalar == 0x017FU) return scalar;
        if (scalar >= 'a' && scalar <= 'z') return scalar - ('a' - 'A');
#if defined(_WIN32)
        if (scalar <= 0xFFFFU)
        {
            const wchar_t source = static_cast<wchar_t>(scalar);
            wchar_t target = source;
            if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
                &source, 1, &target, 1, nullptr, nullptr, 0) == 1)
            {
                return static_cast<std::uint32_t>(target);
            }
        }
#else
        const std::uint32_t icuMapped = IcuUpper(scalar);
        if (icuMapped != scalar) return icuMapped;
        static locale_t locale = []() noexcept
        {
            locale_t value = newlocale(LC_CTYPE_MASK, "C.UTF-8", nullptr);
            if (value == nullptr) value = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
            return value;
        }();
        if (locale != nullptr && scalar <= static_cast<std::uint32_t>(WCHAR_MAX))
        {
            const wint_t mapped = towupper_l(static_cast<wint_t>(scalar), locale);
            if (mapped != WEOF) return static_cast<std::uint32_t>(mapped);
        }
#endif
        // Unicode simple-uppercase fallbacks used when the platform has no Unicode locale.
        if (scalar >= 0x00E0U && scalar <= 0x00F6U) return scalar - 0x20U;
        if (scalar >= 0x00F8U && scalar <= 0x00FEU) return scalar - 0x20U;
        if (scalar == 0x00FFU) return 0x0178U;
        if (scalar >= 0x03B1U && scalar <= 0x03C1U) return scalar - 0x20U;
        if (scalar >= 0x03C3U && scalar <= 0x03CBU) return scalar - 0x20U;
        if (scalar >= 0x0430U && scalar <= 0x044FU) return scalar - 0x20U;
        return scalar;
    }

    [[nodiscard]] std::vector<std::uint32_t> FoldOrdinalIgnoreCase(const std::string& value)
    {
        const std::u32string decoded = Utf8ToUtf32(value);
        std::vector<std::uint32_t> result(decoded.begin(), decoded.end());
        for (std::uint32_t& scalar : result) scalar = InvariantUpper(scalar);
        return result;
    }

    [[nodiscard]] bool OrdinalIgnoreCaseEquals(const std::string& left, const std::string& right) noexcept
    {
        try
        {
            return FoldOrdinalIgnoreCase(left) == FoldOrdinalIgnoreCase(right);
        }
        catch (...)
        {
            return left == right;
        }
    }

    [[nodiscard]] bool OrdinalIgnoreCaseEndsWith(const std::string& value, const std::string& suffix) noexcept
    {
        try
        {
            const std::vector<std::uint32_t> foldedValue = FoldOrdinalIgnoreCase(value);
            const std::vector<std::uint32_t> foldedSuffix = FoldOrdinalIgnoreCase(suffix);
            if (foldedSuffix.size() > foldedValue.size()) return false;
            return std::equal(foldedSuffix.begin(), foldedSuffix.end(),
                foldedValue.end() - static_cast<std::ptrdiff_t>(foldedSuffix.size()));
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] std::uint16_t ReadU16(const ByteVector& bytes, std::size_t position)
    {
        if (position > bytes.size() || bytes.size() - position < 2)
        {
            throw System::IO::InvalidDataException("Unexpected end of data.");
        }
        return static_cast<std::uint16_t>(bytes[position])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[position + 1]) << 8);
    }

    [[nodiscard]] std::uint32_t ReadU32(const ByteVector& bytes, std::size_t position)
    {
        if (position > bytes.size() || bytes.size() - position < 4)
        {
            throw System::IO::InvalidDataException("Unexpected end of data.");
        }
        return static_cast<std::uint32_t>(bytes[position])
            | (static_cast<std::uint32_t>(bytes[position + 1]) << 8)
            | (static_cast<std::uint32_t>(bytes[position + 2]) << 16)
            | (static_cast<std::uint32_t>(bytes[position + 3]) << 24);
    }

    [[nodiscard]] std::uint64_t ReadU64(const ByteVector& bytes, std::size_t position)
    {
        const std::uint64_t low = ReadU32(bytes, position);
        const std::uint64_t high = ReadU32(bytes, position + 4);
        return low | (high << 32);
    }

    [[nodiscard]] std::int32_t ReadI32(const ByteVector& bytes, std::size_t position)
    {
        if (position > bytes.size() || bytes.size() - position < sizeof(std::int32_t))
        {
            throw std::runtime_error("Unexpected end of data.");
        }
        std::int32_t value = 0;
        std::memcpy(&value, bytes.data() + position, sizeof(value));
        return value;
    }

    void WriteI32(ByteVector& bytes, std::size_t position, std::int32_t value)
    {
        if (position > bytes.size() || bytes.size() - position < sizeof(value))
        {
            throw std::out_of_range("Destination is too short.");
        }
        std::memcpy(bytes.data() + position, &value, sizeof(value));
    }

    [[nodiscard]] std::string DecodeAscii(const std::uint8_t* data, std::size_t count)
    {
        std::string result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            result.push_back(data[i] <= 0x7F ? static_cast<char>(data[i]) : '?');
        }
        return result;
    }

    [[nodiscard]] std::string DecodeAscii(const ByteVector& bytes, std::size_t offset, std::size_t count)
    {
        if (offset > bytes.size() || count > bytes.size() - offset)
        {
            throw System::ArgumentOutOfRangeException();
        }
        return DecodeAscii(bytes.data() + offset, count);
    }

    [[nodiscard]] std::string FileNameWithoutExtension(const std::string& path)
    {
        const std::string fileName = PathGetFileName(path);
        const std::size_t dot = fileName.find_last_of('.');
        return dot == std::string::npos ? fileName : fileName.substr(0, dot);
    }

    [[nodiscard]] std::string Extension(const std::string& path)
    {
        const std::string fileName = PathGetFileName(path);
        const std::size_t dot = fileName.find_last_of('.');
        if (dot == std::string::npos || dot + 1 == fileName.size()) return {};
        return fileName.substr(dot);
    }

    class ByteReader
    {
    public:
        explicit ByteReader(const ByteVector& bytes) noexcept : _bytes(bytes) {}

        void Position(std::int64_t value)
        {
            if (value < 0)
            {
                throw System::ArgumentOutOfRangeException();
            }
            _position = static_cast<std::uint64_t>(value);
        }

        [[nodiscard]] std::int32_t ReadInt32()
        {
            const std::uint32_t value = ReadExactU32();
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] float ReadSingle()
        {
            return std::bit_cast<float>(ReadExactU32());
        }

        [[nodiscard]] ByteVector ReadBytes(std::size_t count)
        {
            if (_position >= _bytes.size())
            {
                return {};
            }
            const std::size_t position = static_cast<std::size_t>(_position);
            const std::size_t actual = std::min(count, _bytes.size() - position);
            ByteVector result(
                _bytes.begin() + static_cast<std::ptrdiff_t>(position),
                _bytes.begin() + static_cast<std::ptrdiff_t>(position + actual));
            _position += actual;
            return result;
        }

        [[nodiscard]] std::string ReadChars(std::size_t count)
        {
            std::string result;
            std::size_t charCount = 0;
            while (charCount < count && _position < _bytes.size())
            {
                const std::size_t position = static_cast<std::size_t>(_position);
                const Utf8Scalar decoded = RuneDecodeFromUtf8(std::string_view(
                    reinterpret_cast<const char*>(_bytes.data()) + position, _bytes.size() - position));
                const char32_t scalar = decoded.Value;

                // BinaryReader's UTF-8 Decoder is invoked with flush:false. An
                // incomplete terminal sequence is consumed into decoder state but
                // emits no replacement character before end-of-stream is observed.
                _position += decoded.Length;
                if (decoded.Status == OperationStatus::NeedMoreData)
                {
                    break;
                }

                const std::size_t units = scalar > 0xFFFFU ? 2U : 1U;
                if (units > count - charCount)
                {
                    // Decoder.GetChars throws when a complete surrogate pair cannot
                    // fit in the remaining destination rather than splitting it.
                    throw System::ArgumentException();
                }
                AppendUtf8(result, scalar);
                charCount += units;
            }
            return result;
        }

    private:
        const ByteVector& _bytes;
        std::uint64_t _position = 0;

        [[nodiscard]] std::uint32_t ReadExactU32()
        {
            if (_position > _bytes.size() || _bytes.size() - static_cast<std::size_t>(_position) < 4)
            {
                throw System::IO::EndOfStreamException();
            }
            const std::uint32_t value = ReadU32(_bytes, static_cast<std::size_t>(_position));
            _position += 4;
            return value;
        }
    };

    struct HuffmanNode
    {
        std::int32_t child[2] = {-1, -1};
        std::int32_t symbol = -1;
    };

    class BitReader
    {
    public:
        explicit BitReader(const ByteVector& bytes) noexcept : _bytes(bytes) {}

        [[nodiscard]] std::uint32_t ReadBits(std::int32_t count)
        {
            std::uint32_t value = 0;
            for (std::int32_t i = 0; i < count; ++i)
            {
                if (_bitPosition >= _bytes.size() * 8ULL)
                {
                    throw System::IO::InvalidDataException("Invalid deflate stream.");
                }
                const std::size_t byteIndex = _bitPosition >> 3;
                const std::size_t bitIndex = _bitPosition & 7;
                value |= static_cast<std::uint32_t>((_bytes[byteIndex] >> bitIndex) & 1U) << i;
                ++_bitPosition;
            }
            return value;
        }

        void AlignByte() noexcept
        {
            _bitPosition = (_bitPosition + 7U) & ~std::size_t(7U);
        }

        [[nodiscard]] std::size_t BytePosition() const noexcept { return _bitPosition >> 3; }
        void BytePosition(std::size_t value) noexcept { _bitPosition = value << 3; }

    private:
        const ByteVector& _bytes;
        std::size_t _bitPosition = 0;
    };

    [[nodiscard]] std::vector<HuffmanNode> BuildHuffman(const std::vector<std::uint8_t>& lengths)
    {
        constexpr std::int32_t MaxBits = 15;
        std::array<std::int32_t, MaxBits + 1> counts{};
        for (std::uint8_t length : lengths)
        {
            if (length > MaxBits)
            {
                throw System::IO::InvalidDataException("Invalid deflate Huffman code length.");
            }
            if (length != 0)
            {
                ++counts[length];
            }
        }

        std::array<std::int32_t, MaxBits + 1> next{};
        std::int32_t code = 0;
        for (std::int32_t bits = 1; bits <= MaxBits; ++bits)
        {
            code = (code + counts[bits - 1]) << 1;
            next[bits] = code;
        }

        std::vector<HuffmanNode> nodes(1);
        for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol)
        {
            const std::int32_t length = lengths[symbol];
            if (length == 0)
            {
                continue;
            }
            const std::int32_t assigned = next[length]++;
            std::int32_t node = 0;
            for (std::int32_t bitIndex = length - 1; bitIndex >= 0; --bitIndex)
            {
                const std::int32_t bit = (assigned >> bitIndex) & 1;
                if (nodes[node].child[bit] < 0)
                {
                    nodes[node].child[bit] = static_cast<std::int32_t>(nodes.size());
                    nodes.emplace_back();
                }
                node = nodes[node].child[bit];
            }
            if (nodes[node].symbol >= 0)
            {
                throw System::IO::InvalidDataException("Invalid deflate Huffman tree.");
            }
            nodes[node].symbol = static_cast<std::int32_t>(symbol);
        }
        return nodes;
    }

    [[nodiscard]] std::int32_t DecodeSymbol(BitReader& reader, const std::vector<HuffmanNode>& tree)
    {
        std::int32_t node = 0;
        for (std::int32_t depth = 0; depth <= 15; ++depth)
        {
            if (tree[node].symbol >= 0)
            {
                return tree[node].symbol;
            }
            const std::int32_t bit = static_cast<std::int32_t>(reader.ReadBits(1));
            node = tree[node].child[bit];
            if (node < 0)
            {
                throw System::IO::InvalidDataException("Invalid deflate Huffman code.");
            }
        }
        throw System::IO::InvalidDataException("Invalid deflate Huffman code.");
    }

    [[nodiscard]] bool InflateCodes(BitReader& reader, ByteVector& output,
        const std::vector<HuffmanNode>& literalTree,
        const std::vector<HuffmanNode>& distanceTree, bool deflate64, std::size_t outputLimit)
    {
        static constexpr std::array<std::int32_t, 29> LengthBase{
            3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
        static constexpr std::array<std::int32_t, 29> LengthExtra{
            0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
        static constexpr std::array<std::int32_t, 32> DistanceBase{
            1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
            1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,32769,49153};
        static constexpr std::array<std::int32_t, 32> DistanceExtra{
            0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14};

        while (true)
        {
            const std::int32_t symbol = DecodeSymbol(reader, literalTree);
            if (symbol < 256)
            {
                output.push_back(static_cast<std::uint8_t>(symbol));
                if (output.size() >= outputLimit) return true;
                continue;
            }
            if (symbol == 256)
            {
                return false;
            }
            if (symbol < 257 || symbol > 285)
            {
                throw System::IO::InvalidDataException("Invalid deflate length code.");
            }
            const std::size_t lengthIndex = static_cast<std::size_t>(symbol - 257);
            std::int32_t length = LengthBase[lengthIndex];
            std::int32_t lengthExtra = LengthExtra[lengthIndex];
            if (deflate64 && symbol == 285)
            {
                length = 3;
                lengthExtra = 16;
            }
            if (lengthExtra != 0)
            {
                length += static_cast<std::int32_t>(reader.ReadBits(lengthExtra));
            }

            const std::int32_t distanceSymbol = DecodeSymbol(reader, distanceTree);
            const std::int32_t distanceCodeCount = deflate64 ? 32 : 30;
            if (distanceSymbol < 0 || distanceSymbol >= distanceCodeCount)
            {
                throw System::IO::InvalidDataException("Invalid deflate distance code.");
            }
            std::int32_t distance = DistanceBase[distanceSymbol];
            if (DistanceExtra[distanceSymbol] != 0)
            {
                distance += static_cast<std::int32_t>(reader.ReadBits(DistanceExtra[distanceSymbol]));
            }
            if (distance <= 0 || static_cast<std::size_t>(distance) > output.size())
            {
                throw System::IO::InvalidDataException("Invalid deflate distance.");
            }
            for (std::int32_t i = 0; i < length; ++i)
            {
                output.push_back(output[output.size() - static_cast<std::size_t>(distance)]);
                if (output.size() >= outputLimit) return true;
            }
        }
    }

    [[nodiscard]] ByteVector InflateRaw(const ByteVector& input, std::size_t outputLimit, bool deflate64 = false)
    {
        ByteVector output;
        if (outputLimit == 0) return output;
        BitReader reader(input);
        bool finalBlock = false;
        while (!finalBlock)
        {
            finalBlock = reader.ReadBits(1) != 0;
            const std::uint32_t type = reader.ReadBits(2);
            if (type == 0)
            {
                reader.AlignByte();
                std::size_t position = reader.BytePosition();
                if (position > input.size() || input.size() - position < 4)
                {
                    throw System::IO::InvalidDataException("Invalid stored deflate block.");
                }
                const std::uint16_t length = ReadU16(input, position);
                const std::uint16_t complement = ReadU16(input, position + 2);
                position += 4;
                if (static_cast<std::uint16_t>(~length) != complement)
                {
                    throw System::IO::InvalidDataException("Invalid stored deflate block.");
                }
                const std::size_t remaining = outputLimit - output.size();
                const std::size_t copyCount = std::min<std::size_t>(length, remaining);
                if (position > input.size() || copyCount > input.size() - position)
                {
                    throw System::IO::InvalidDataException("Invalid stored deflate block.");
                }
                output.insert(output.end(),
                    input.begin() + static_cast<std::ptrdiff_t>(position),
                    input.begin() + static_cast<std::ptrdiff_t>(position + copyCount));
                if (copyCount < length || output.size() >= outputLimit) return output;
                reader.BytePosition(position + length);
            }
            else if (type == 1)
            {
                std::vector<std::uint8_t> literalLengths(288);
                for (std::int32_t i = 0; i <= 143; ++i) literalLengths[i] = 8;
                for (std::int32_t i = 144; i <= 255; ++i) literalLengths[i] = 9;
                for (std::int32_t i = 256; i <= 279; ++i) literalLengths[i] = 7;
                for (std::int32_t i = 280; i <= 287; ++i) literalLengths[i] = 8;
                std::vector<std::uint8_t> distanceLengths(32, 5);
                if (InflateCodes(reader, output, BuildHuffman(literalLengths),
                    BuildHuffman(distanceLengths), deflate64, outputLimit)) return output;
            }
            else if (type == 2)
            {
                const std::int32_t literalCount = static_cast<std::int32_t>(reader.ReadBits(5)) + 257;
                const std::int32_t distanceCount = static_cast<std::int32_t>(reader.ReadBits(5)) + 1;
                const std::int32_t codeCount = static_cast<std::int32_t>(reader.ReadBits(4)) + 4;
                static constexpr std::array<std::int32_t, 19> Order{
                    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                std::vector<std::uint8_t> codeLengths(19, 0);
                for (std::int32_t i = 0; i < codeCount; ++i)
                {
                    codeLengths[Order[i]] = static_cast<std::uint8_t>(reader.ReadBits(3));
                }
                const std::vector<HuffmanNode> codeTree = BuildHuffman(codeLengths);
                std::vector<std::uint8_t> lengths;
                lengths.reserve(static_cast<std::size_t>(literalCount + distanceCount));
                while (static_cast<std::int32_t>(lengths.size()) < literalCount + distanceCount)
                {
                    const std::int32_t symbol = DecodeSymbol(reader, codeTree);
                    if (symbol <= 15)
                    {
                        lengths.push_back(static_cast<std::uint8_t>(symbol));
                    }
                    else if (symbol == 16)
                    {
                        if (lengths.empty()) throw System::IO::InvalidDataException("Invalid deflate repeat code.");
                        const std::int32_t repeat = static_cast<std::int32_t>(reader.ReadBits(2)) + 3;
                        const std::uint8_t value = lengths.back();
                        for (std::int32_t i = 0; i < repeat; ++i) lengths.push_back(value);
                    }
                    else if (symbol == 17)
                    {
                        const std::int32_t repeat = static_cast<std::int32_t>(reader.ReadBits(3)) + 3;
                        for (std::int32_t i = 0; i < repeat; ++i) lengths.push_back(0);
                    }
                    else if (symbol == 18)
                    {
                        const std::int32_t repeat = static_cast<std::int32_t>(reader.ReadBits(7)) + 11;
                        for (std::int32_t i = 0; i < repeat; ++i) lengths.push_back(0);
                    }
                    else
                    {
                        throw System::IO::InvalidDataException("Invalid deflate code-length symbol.");
                    }
                    if (static_cast<std::int32_t>(lengths.size()) > literalCount + distanceCount)
                    {
                        throw System::IO::InvalidDataException("Invalid deflate code lengths.");
                    }
                }
                std::vector<std::uint8_t> literalLengths(lengths.begin(), lengths.begin() + literalCount);
                std::vector<std::uint8_t> distanceLengths(lengths.begin() + literalCount, lengths.end());
                if (InflateCodes(reader, output, BuildHuffman(literalLengths),
                    BuildHuffman(distanceLengths), deflate64, outputLimit)) return output;
            }
            else
            {
                throw System::IO::InvalidDataException("Invalid deflate block type.");
            }
        }
        return output;
    }

    struct ZipEntry
    {
        std::string name;
        std::uint16_t flags = 0;
        std::uint16_t method = 0;
        std::uint64_t compressedSize = 0;
        std::uint64_t uncompressedSize = 0;
        std::uint64_t localOffset = 0;
        std::uint32_t diskNumberStart = 0;
        std::uint32_t archiveDiskNumber = 0;
    };

    [[nodiscard]] std::uint64_t ReadZip64SignedValue(
        const ByteVector& field, std::size_t& position)
    {
        if (position > field.size() || field.size() - position < 8)
        {
            throw System::IO::InvalidDataException("Invalid ZIP64 extra field.");
        }
        const std::uint64_t value = ReadU64(field, position);
        position += 8;
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            throw System::IO::InvalidDataException("ZIP64 field is too large.");
        }
        return value;
    }

    void ApplyZip64Extra(ZipEntry& entry, const ByteVector& extra,
        bool needUncompressed, bool needCompressed, bool needOffset, bool needDisk)
    {
        std::size_t cursor = 0;
        while (cursor + 4 <= extra.size())
        {
            const std::uint16_t tag = ReadU16(extra, cursor);
            const std::uint16_t size = ReadU16(extra, cursor + 2);
            cursor += 4;
            if (size > extra.size() - cursor)
            {
                return;
            }
            if (tag != 0x0001U)
            {
                cursor += size;
                continue;
            }
            if (size < 8)
            {
                return;
            }

            ByteVector field(
                extra.begin() + static_cast<std::ptrdiff_t>(cursor),
                extra.begin() + static_cast<std::ptrdiff_t>(cursor + size));
            const bool readAllFields = size >= 28;
            std::size_t position = 0;

            if (needUncompressed)
            {
                entry.uncompressedSize = ReadZip64SignedValue(field, position);
            }
            else if (readAllFields)
            {
                position += 8;
            }

            if (position > field.size() - 8)
            {
                return;
            }
            if (needCompressed)
            {
                entry.compressedSize = ReadZip64SignedValue(field, position);
            }
            else if (readAllFields)
            {
                position += 8;
            }

            if (position > field.size() - 8)
            {
                return;
            }
            if (needOffset)
            {
                entry.localOffset = ReadZip64SignedValue(field, position);
            }
            else if (readAllFields)
            {
                position += 8;
            }

            if (position > field.size() - 4)
            {
                return;
            }
            if (needDisk)
            {
                entry.diskNumberStart = ReadU32(field, position);
            }
            return;
        }
    }

    [[nodiscard]] std::vector<ZipEntry> ReadZipEntries(const ByteVector& archive)
    {
        if (archive.size() < 22)
        {
            throw System::IO::InvalidDataException("Central Directory corrupt.");
        }
        const std::size_t searchStart = archive.size() > 65557
            ? archive.size() - 65557 : 0;
        std::size_t eocd = std::numeric_limits<std::size_t>::max();
        for (std::size_t position = archive.size() - 22;; --position)
        {
            if (ReadU32(archive, position) == 0x06054B50U)
            {
                eocd = position;
                break;
            }
            if (position == searchStart)
            {
                break;
            }
        }
        if (eocd == std::numeric_limits<std::size_t>::max())
        {
            throw System::IO::InvalidDataException("End of Central Directory record could not be found.");
        }

        const std::uint16_t eocdDisk = ReadU16(archive, eocd + 4);
        const std::uint16_t eocdCentralDisk = ReadU16(archive, eocd + 6);
        if (eocdDisk != eocdCentralDisk)
        {
            throw System::IO::InvalidDataException("Split or spanned ZIP archives are not supported.");
        }
        const std::uint16_t entriesOnDisk = ReadU16(archive, eocd + 8);
        const std::uint16_t totalEntries = ReadU16(archive, eocd + 10);
        if (entriesOnDisk != totalEntries)
        {
            throw System::IO::InvalidDataException("Split or spanned ZIP archives are not supported.");
        }

        std::uint32_t archiveDiskNumber = eocdDisk;
        std::uint64_t entryCount = totalEntries;
        std::uint64_t centralOffset = ReadU32(archive, eocd + 16);

        const bool suspectZip64 = eocdDisk == 0xFFFFU
            || centralOffset == 0xFFFFFFFFULL || entryCount == 0xFFFFU;
        if (suspectZip64 && eocd >= 20 && ReadU32(archive, eocd - 20) == 0x07064B50U)
        {
            const std::uint64_t zip64Offset = ReadU64(archive, eocd - 12);
            if (zip64Offset > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
            {
                throw System::IO::InvalidDataException("ZIP64 End of Central Directory offset is too large.");
            }
            if (zip64Offset > archive.size()
                || archive.size() - static_cast<std::size_t>(zip64Offset) < 56
                || ReadU32(archive, static_cast<std::size_t>(zip64Offset)) != 0x06064B50U)
            {
                throw System::IO::InvalidDataException("ZIP64 End of Central Directory record is invalid.");
            }

            const std::size_t offset = static_cast<std::size_t>(zip64Offset);
            archiveDiskNumber = ReadU32(archive, offset + 16);
            const std::uint64_t zip64EntriesOnDisk = ReadU64(archive, offset + 24);
            const std::uint64_t zip64EntryCount = ReadU64(archive, offset + 32);
            const std::uint64_t zip64CentralOffset = ReadU64(archive, offset + 48);
            if (zip64EntryCount > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
            {
                throw System::IO::InvalidDataException("ZIP64 entry count is too large.");
            }
            if (zip64CentralOffset > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
            {
                throw System::IO::InvalidDataException("ZIP64 Central Directory offset is too large.");
            }
            if (zip64EntryCount != zip64EntriesOnDisk)
            {
                throw System::IO::InvalidDataException("Split or spanned ZIP archives are not supported.");
            }
            entryCount = zip64EntryCount;
            centralOffset = zip64CentralOffset;
        }

        if (centralOffset > archive.size())
        {
            throw System::IO::InvalidDataException("Central Directory corrupt.");
        }
        if (entryCount > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            || entryCount > static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max()))
        {
            throw System::IO::InvalidDataException("Too many ZIP entries.");
        }

        std::vector<ZipEntry> entries;
        std::size_t cursor = static_cast<std::size_t>(centralOffset);
        while (cursor + 4 <= archive.size()
            && ReadU32(archive, cursor) == 0x02014B50U)
        {
            if (archive.size() - cursor < 46)
            {
                throw System::IO::InvalidDataException("Central Directory corrupt.");
            }

            ZipEntry entry;
            entry.flags = ReadU16(archive, cursor + 8);
            entry.method = ReadU16(archive, cursor + 10);
            const std::uint32_t compressed32 = ReadU32(archive, cursor + 20);
            const std::uint32_t uncompressed32 = ReadU32(archive, cursor + 24);
            const std::uint16_t nameLength = ReadU16(archive, cursor + 28);
            const std::uint16_t extraLength = ReadU16(archive, cursor + 30);
            const std::uint16_t commentLength = ReadU16(archive, cursor + 32);
            const std::uint16_t diskNumberStart16 = ReadU16(archive, cursor + 34);
            const std::uint32_t offset32 = ReadU32(archive, cursor + 42);
            const std::size_t variable = static_cast<std::size_t>(nameLength)
                + static_cast<std::size_t>(extraLength)
                + static_cast<std::size_t>(commentLength);
            if (46 + variable > archive.size() - cursor)
            {
                throw System::IO::InvalidDataException("Central Directory corrupt.");
            }

            entry.name = Utf8GetString(std::span<const std::uint8_t>(archive.data() + cursor + 46, nameLength));
            entry.compressedSize = compressed32;
            entry.uncompressedSize = uncompressed32;
            entry.localOffset = offset32;
            entry.diskNumberStart = diskNumberStart16;
            entry.archiveDiskNumber = archiveDiskNumber;

            ByteVector extra(
                archive.begin() + static_cast<std::ptrdiff_t>(cursor + 46 + nameLength),
                archive.begin() + static_cast<std::ptrdiff_t>(
                    cursor + 46 + nameLength + extraLength));
            ApplyZip64Extra(entry, extra,
                uncompressed32 == 0xFFFFFFFFU,
                compressed32 == 0xFFFFFFFFU,
                offset32 == 0xFFFFFFFFU,
                diskNumberStart16 == 0xFFFFU);
            entries.push_back(std::move(entry));
            cursor += 46 + variable;
        }

        if (entries.size() != static_cast<std::size_t>(entryCount))
        {
            throw System::IO::InvalidDataException("Central Directory entry count is incorrect.");
        }
        return entries;
    }

    [[nodiscard]] ByteVector ReadZipEntry(const ByteVector& archive, const ZipEntry& entry)
    {
        if (entry.method != 0 && entry.method != 8 && entry.method != 9)
        {
            throw System::IO::InvalidDataException("The ZIP entry uses an unsupported compression method.");
        }
        if (entry.diskNumberStart != entry.archiveDiskNumber)
        {
            throw System::IO::InvalidDataException("Split or spanned ZIP archives are not supported.");
        }
        if (entry.localOffset > archive.size()
            || archive.size() - static_cast<std::size_t>(entry.localOffset) < 30)
        {
            throw System::IO::InvalidDataException("Local file header is invalid.");
        }
        const std::size_t local = static_cast<std::size_t>(entry.localOffset);
        if (ReadU32(archive, local) != 0x04034B50U)
        {
            throw System::IO::InvalidDataException("Local file header is invalid.");
        }
        const std::uint16_t nameLength = ReadU16(archive, local + 26);
        const std::uint16_t extraLength = ReadU16(archive, local + 28);
        const std::uint64_t dataOffset64 = entry.localOffset + 30ULL + nameLength + extraLength;
        if (dataOffset64 > archive.size()
            || entry.compressedSize > archive.size() - static_cast<std::size_t>(dataOffset64))
        {
            throw System::IO::InvalidDataException("ZIP entry data is invalid.");
        }
        if (entry.compressedSize
            > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
            || entry.uncompressedSize
            > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            throw System::IO::InvalidDataException("ZIP entry is too large.");
        }

        const std::size_t dataOffset = static_cast<std::size_t>(dataOffset64);
        const std::size_t compressedSize = static_cast<std::size_t>(entry.compressedSize);
        ByteVector compressed(
            archive.begin() + static_cast<std::ptrdiff_t>(dataOffset),
            archive.begin() + static_cast<std::ptrdiff_t>(dataOffset + compressedSize));
        if (entry.method == 0)
        {
            return compressed;
        }
        if (entry.method == 8)
        {
            return InflateRaw(compressed, static_cast<std::size_t>(entry.uncompressedSize));
        }
        if (entry.method == 9)
        {
            return InflateRaw(compressed, static_cast<std::size_t>(entry.uncompressedSize), true);
        }
        throw System::IO::InvalidDataException("The ZIP entry uses an unsupported compression method.");
    }

    [[nodiscard]] bool CultureLess(const std::string& left, const std::string& right)
    {
#if defined(_WIN32)
        const std::wstring leftWide = Utf8ToWide(left);
        const std::wstring rightWide = Utf8ToWide(right);
        const int result = CompareStringEx(LOCALE_NAME_USER_DEFAULT, 0,
            leftWide.data(), static_cast<int>(leftWide.size()),
            rightWide.data(), static_cast<int>(rightWide.size()),
            nullptr, nullptr, 0);
        if (result != 0) return result == CSTR_LESS_THAN;
#else
        struct IcuCollation final
        {
            using Open = void* (*)(const char*, std::int32_t*);
            using Compare = std::int32_t (*)(const void*, const char*, std::int32_t,
                const char*, std::int32_t, std::int32_t*);
            using Close = void (*)(void*);
            void* library = nullptr;
            Open open = nullptr;
            Compare compare = nullptr;
            Close close = nullptr;

            IcuCollation() noexcept
            {
                library = dlopen("libicui18n.so", RTLD_LAZY | RTLD_LOCAL);
#if defined(__APPLE__)
                if (library == nullptr) library = dlopen("/usr/lib/libicucore.A.dylib", RTLD_LAZY | RTLD_LOCAL);
#endif
                open = reinterpret_cast<Open>(FindVersionedIcuSymbol(library, "ucol_open"));
                compare = reinterpret_cast<Compare>(FindVersionedIcuSymbol(library, "ucol_strcollUTF8"));
                close = reinterpret_cast<Close>(FindVersionedIcuSymbol(library, "ucol_close"));
            }
        };
        static const IcuCollation icu{};
        if (icu.open != nullptr && icu.compare != nullptr && icu.close != nullptr)
        {
            std::int32_t status = 0;
            void* collator = icu.open(nullptr, &status);
            if (collator != nullptr && status <= 0)
            {
                status = 0;
                const std::int32_t result = icu.compare(collator,
                    left.data(), static_cast<std::int32_t>(left.size()),
                    right.data(), static_cast<std::int32_t>(right.size()), &status);
                icu.close(collator);
                if (status <= 0) return result < 0;
            }
            else if (collator != nullptr)
            {
                icu.close(collator);
            }
        }
#endif
        try
        {
            const std::locale locale("");
            const std::wstring leftWide = Utf8ToWide(left);
            const std::wstring rightWide = Utf8ToWide(right);
            const auto& collate = std::use_facet<std::collate<wchar_t>>(locale);
            return collate.compare(leftWide.data(), leftWide.data() + leftWide.size(),
                rightWide.data(), rightWide.data() + rightWide.size()) < 0;
        }
        catch (const std::runtime_error&)
        {
            return Utf8ToUtf32(left) < Utf8ToUtf32(right);
        }
    }

    template <typename T, typename Read>
    [[nodiscard]] std::vector<T> ReadLump(ByteReader& reader,
        std::pair<std::int32_t, std::int32_t> lump, std::int32_t size, Read read)
    {
        const std::int32_t count = lump.second / size;
        if (count < 0)
        {
            throw System::ArgumentOutOfRangeException();
        }
        std::vector<T> results;
        results.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i)
        {
            const std::int32_t position = UncheckedAdd(lump.first, UncheckedMultiply(i, size));
            reader.Position(position);
            results.push_back(read(reader));
        }
        return results;
    }
}

namespace MphRead::Mods::MapGen::Q3RecordRuntime
{
    namespace
    {
        [[nodiscard]] std::uint32_t ProcessSeed() noexcept
        {
            static const std::uint32_t seed = []() noexcept
            {
                try
                {
                    std::random_device device;
                    const std::uint32_t first = device();
                    const std::uint32_t second = device();
                    return first ^ std::rotl(second, 13) ^ 0x9E3779B9U;
                }
                catch (...)
                {
                    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(&ProcessSeed);
                    return static_cast<std::uint32_t>(address)
                        ^ static_cast<std::uint32_t>(address >> 32) ^ 0x9E3779B9U;
                }
            }();
            return seed;
        }

        [[nodiscard]] std::uint32_t Mix(std::uint32_t hash, std::uint32_t value) noexcept
        {
            hash ^= value + 0x9E3779B9U + (hash << 6) + (hash >> 2);
            hash = std::rotl(hash, 13) * 0x85EBCA6BU;
            return hash;
        }

        [[nodiscard]] std::uint32_t HashUtf16(const std::string& value) noexcept
        {
            try
            {
                std::uint32_t hash = ProcessSeed() ^ 0x6D2B79F5U;
                for (const char16_t unit : Utf8ToUtf16(value))
                {
                    hash = Mix(hash, unit);
                }
                return hash;
            }
            catch (...)
            {
                return ProcessSeed();
            }
        }

        struct NumberSymbols final
        {
            std::string decimalSeparator = ".";
            std::string negativeSign = "-";
            std::string positiveSign = "+";
            std::string nan = "NaN";
            std::string positiveInfinity = "Infinity";
            std::string negativeInfinity = "-Infinity";
        };

#if defined(_WIN32)
        [[nodiscard]] std::string WindowsLocaleString(LCTYPE type, const std::string& fallback)
        {
            wchar_t buffer[128]{};
            const int count = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, type,
                buffer, static_cast<int>(std::size(buffer)));
            if (count <= 1) return fallback;
            return WideToUtf8(std::wstring_view(buffer, static_cast<std::size_t>(count - 1)));
        }
#endif

        [[nodiscard]] NumberSymbols CurrentNumberSymbols() noexcept
        {
            NumberSymbols symbols;
            try
            {
#if defined(_WIN32)
                symbols.decimalSeparator = WindowsLocaleString(LOCALE_SDECIMAL, symbols.decimalSeparator);
                symbols.negativeSign = WindowsLocaleString(LOCALE_SNEGATIVESIGN, symbols.negativeSign);
                symbols.positiveSign = WindowsLocaleString(LOCALE_SPOSITIVESIGN, symbols.positiveSign);
#if defined(LOCALE_SNAN)
                symbols.nan = WindowsLocaleString(LOCALE_SNAN, symbols.nan);
#endif
#if defined(LOCALE_SPOSINFINITY)
                symbols.positiveInfinity = WindowsLocaleString(LOCALE_SPOSINFINITY, symbols.positiveInfinity);
#endif
#if defined(LOCALE_SNEGINFINITY)
                symbols.negativeInfinity = WindowsLocaleString(LOCALE_SNEGINFINITY,
                    symbols.negativeSign + symbols.positiveInfinity);
#else
                symbols.negativeInfinity = symbols.negativeSign + symbols.positiveInfinity;
#endif
#else
                struct IcuNumbers final
                {
                    using Open = void* (*)(std::int32_t, const std::uint16_t*, std::int32_t,
                        const char*, void*, std::int32_t*);
                    using GetSymbol = std::int32_t (*)(const void*, std::int32_t,
                        std::uint16_t*, std::int32_t, std::int32_t*);
                    using Close = void (*)(void*);
                    void* library = nullptr;
                    Open open = nullptr;
                    GetSymbol getSymbol = nullptr;
                    Close close = nullptr;

                    IcuNumbers() noexcept
                    {
                        library = dlopen("libicui18n.so", RTLD_LAZY | RTLD_LOCAL);
#if defined(__APPLE__)
                        if (library == nullptr)
                        {
                            library = dlopen("/usr/lib/libicucore.A.dylib", RTLD_LAZY | RTLD_LOCAL);
                        }
#endif
                        open = reinterpret_cast<Open>(FindVersionedIcuSymbol(library, "unum_open"));
                        getSymbol = reinterpret_cast<GetSymbol>(FindVersionedIcuSymbol(library, "unum_getSymbol"));
                        close = reinterpret_cast<Close>(FindVersionedIcuSymbol(library, "unum_close"));
                    }
                };
                static const IcuNumbers icu{};
                if (icu.open != nullptr && icu.getSymbol != nullptr && icu.close != nullptr)
                {
                    std::int32_t status = 0;
                    // UNUM_DECIMAL = 1. A null locale selects ICU's current default locale,
                    // which is the same culture source used by .NET's ICU globalization path.
                    void* formatter = icu.open(1, nullptr, 0, nullptr, nullptr, &status);
                    if (formatter != nullptr && status <= 0)
                    {
                        const auto readSymbol = [&](std::int32_t symbol, const std::string& fallback)
                        {
                            std::uint16_t buffer[128]{};
                            std::int32_t localStatus = 0;
                            const std::int32_t length = icu.getSymbol(formatter, symbol, buffer,
                                static_cast<std::int32_t>(std::size(buffer)), &localStatus);
                            if (localStatus > 0 || length < 0
                                || length > static_cast<std::int32_t>(std::size(buffer)))
                            {
                                return fallback;
                            }
                            return Utf16ToUtf8(std::u16string_view(reinterpret_cast<const char16_t*>(buffer), static_cast<std::size_t>(length)));
                        };
                        // UNumberFormatSymbol values from ICU: decimal=0, minus=6,
                        // plus=7, infinity=14, NaN=15.
                        symbols.decimalSeparator = readSymbol(0, symbols.decimalSeparator);
                        symbols.negativeSign = readSymbol(6, symbols.negativeSign);
                        symbols.positiveSign = readSymbol(7, symbols.positiveSign);
                        symbols.positiveInfinity = readSymbol(14, symbols.positiveInfinity);
                        symbols.nan = readSymbol(15, symbols.nan);
                        // .NET's ICU CultureData has no separate negative-infinity
                        // symbol and synthesizes it as NegativeSign + PositiveInfinitySymbol.
                        symbols.negativeInfinity = symbols.negativeSign + symbols.positiveInfinity;
                        icu.close(formatter);
                        return symbols;
                    }
                    if (formatter != nullptr) icu.close(formatter);
                }
                try
                {
                    const auto& punctuation = std::use_facet<std::numpunct<char>>(std::locale(""));
                    symbols.decimalSeparator.assign(1, punctuation.decimal_point());
                }
                catch (...)
                {
                }
                symbols.negativeInfinity = symbols.negativeSign + symbols.positiveInfinity;
#endif
            }
            catch (...)
            {
            }
            return symbols;
        }

        struct ShortestFloat final
        {
            bool negative = false;
            std::string digits;
            std::int32_t scale = 0;
        };

        [[nodiscard]] ShortestFloat DecomposeShortestFloat(float value)
        {
            ShortestFloat result;
            result.negative = std::signbit(value);
            const float magnitude = std::fabs(value);
            char buffer[64]{};
            const auto conversion = std::to_chars(std::begin(buffer), std::end(buffer),
                magnitude, std::chars_format::general);
            std::string text = conversion.ec == std::errc{}
                ? std::string(buffer, conversion.ptr) : std::to_string(magnitude);

            const std::size_t exponentPosition = text.find_first_of("eE");
            std::int32_t exponent = 0;
            if (exponentPosition != std::string::npos)
            {
                const char* first = text.data() + static_cast<std::ptrdiff_t>(exponentPosition + 1);
                const char* last = text.data() + static_cast<std::ptrdiff_t>(text.size());
                if (first != last && *first == '+') ++first;
                (void)std::from_chars(first, last, exponent);
                text.resize(exponentPosition);
            }

            const std::size_t decimalPosition = text.find('.');
            const std::size_t integerDigits = decimalPosition == std::string::npos
                ? text.size() : decimalPosition;
            if (decimalPosition != std::string::npos) text.erase(decimalPosition, 1);

            std::size_t leadingZeros = 0;
            while (leadingZeros < text.size() && text[leadingZeros] == '0') ++leadingZeros;
            if (leadingZeros == text.size())
            {
                result.digits.clear();
                result.scale = 0;
                return result;
            }
            result.digits = text.substr(leadingZeros);
            while (!result.digits.empty() && result.digits.back() == '0') result.digits.pop_back();
            result.scale = static_cast<std::int32_t>(integerDigits)
                - static_cast<std::int32_t>(leadingZeros) + exponent;
            return result;
        }

        [[nodiscard]] std::string DotNetGeneralFloat(float value, const NumberSymbols& symbols)
        {
            const ShortestFloat number = DecomposeShortestFloat(value);
            std::string result;
            if (number.negative) result += symbols.negativeSign;
            if (number.digits.empty())
            {
                result += '0';
                return result;
            }

            // Single's default G format uses the shortest round-trippable digits,
            // but does not switch to scientific notation until Scale > max(DigitsCount, 9)
            // or Scale < -3. This is the exact FormatGeneral threshold in .NET 9.
            const std::int32_t maxDigits = std::max<std::int32_t>(
                static_cast<std::int32_t>(number.digits.size()), 9);
            const bool scientific = number.scale > maxDigits || number.scale < -3;
            if (!scientific)
            {
                if (number.scale > 0)
                {
                    const std::size_t whole = static_cast<std::size_t>(number.scale);
                    if (whole >= number.digits.size())
                    {
                        result += number.digits;
                        result.append(whole - number.digits.size(), '0');
                    }
                    else
                    {
                        result.append(number.digits.data(), whole);
                        result += symbols.decimalSeparator;
                        result.append(number.digits.data() + static_cast<std::ptrdiff_t>(whole),
                            number.digits.size() - whole);
                    }
                }
                else
                {
                    result += '0';
                    result += symbols.decimalSeparator;
                    result.append(static_cast<std::size_t>(-number.scale), '0');
                    result += number.digits;
                }
                return result;
            }

            result.push_back(number.digits.front());
            if (number.digits.size() > 1)
            {
                result += symbols.decimalSeparator;
                result.append(number.digits.begin() + 1, number.digits.end());
            }
            result += 'E';
            const std::int32_t exponent = number.scale - 1;
            const std::uint32_t magnitude = exponent < 0
                ? static_cast<std::uint32_t>(-static_cast<std::int64_t>(exponent))
                : static_cast<std::uint32_t>(exponent);
            result += exponent < 0 ? symbols.negativeSign : symbols.positiveSign;
            char exponentBuffer[16]{};
            const auto exponentConversion = std::to_chars(
                std::begin(exponentBuffer), std::end(exponentBuffer), magnitude);
            std::string exponentText(exponentBuffer, exponentConversion.ptr);
            if (exponentText.size() < 2) result.append(2 - exponentText.size(), '0');
            result += exponentText;
            return result;
        }
    }

    std::uint32_t TypeHash(const std::type_info& type) noexcept
    {
        std::uint32_t hash = ProcessSeed() ^ 0xA5A5A5A5U;
        for (const unsigned char c : std::string_view(type.name()))
        {
            hash = Mix(hash, c);
        }
        return hash;
    }

    std::uint32_t StringHash(const Q3String& value) noexcept
    {
        return value.HasValue() ? HashUtf16(value.Value()) : 0U;
    }

    std::uint32_t ReferenceHash(const void* value) noexcept
    {
        if (value == nullptr) return 0U;
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(value);
        std::uint32_t hash = ProcessSeed() ^ 0xC2B2AE35U;
        hash = Mix(hash, static_cast<std::uint32_t>(address));
        if constexpr (sizeof(std::uintptr_t) > sizeof(std::uint32_t))
        {
            hash = Mix(hash, static_cast<std::uint32_t>(address >> 32));
        }
        return hash;
    }

    std::string IntString(std::int32_t value)
    {
        const NumberSymbols symbols = CurrentNumberSymbols();
        const bool negative = value < 0;
        const std::uint32_t magnitude = negative
            ? static_cast<std::uint32_t>(-static_cast<std::int64_t>(value))
            : static_cast<std::uint32_t>(value);
        char buffer[32]{};
        const auto result = std::to_chars(std::begin(buffer), std::end(buffer), magnitude);
        std::string text = result.ec == std::errc{}
            ? std::string(buffer, result.ptr) : std::to_string(magnitude);
        return negative ? symbols.negativeSign + text : text;
    }

    std::string FloatString(float value)
    {
        const NumberSymbols symbols = CurrentNumberSymbols();
        if (std::isnan(value)) return symbols.nan;
        if (std::isinf(value))
        {
            return std::signbit(value) ? symbols.negativeInfinity : symbols.positiveInfinity;
        }
        return DotNetGeneralFloat(value, symbols);
    }
}

namespace MphRead::Mods::MapGen
{
    const Q3UsedLumps Q3Bsp::UsedLumps{};

    std::size_t Q3StringHash::operator()(const std::string& value) const noexcept
    {
        try
        {
            std::size_t hash = 1469598103934665603ULL;
            for (std::uint32_t scalar : FoldOrdinalIgnoreCase(value))
            {
                for (std::int32_t shift = 0; shift < 32; shift += 8)
                {
                    hash ^= static_cast<std::uint8_t>(scalar >> shift);
                    hash *= 1099511628211ULL;
                }
            }
            return hash;
        }
        catch (...)
        {
            return 0;
        }
    }

    bool Q3StringEqual::operator()(const std::string& left, const std::string& right) const noexcept
    {
        return OrdinalIgnoreCaseEquals(left, right);
    }

    Q3Texture::Q3Texture(Q3String name, std::int32_t flags, std::int32_t contents) noexcept
        : _name(std::move(name)), _flags(flags), _contents(contents) {}
    Q3Texture::Q3Texture(std::nullptr_t, std::int32_t flags, std::int32_t contents) noexcept
        : Q3Texture(Q3String(nullptr), flags, contents) {}
    Q3Texture::Q3Texture(std::string name, std::int32_t flags, std::int32_t contents)
        : Q3Texture(Q3String(std::move(name)), flags, contents) {}
    Q3Texture::Q3Texture(const char* name, std::int32_t flags, std::int32_t contents)
        : Q3Texture(Q3String(name), flags, contents) {}
    const Q3String& Q3Texture::Name() const noexcept { return _name; }
    std::int32_t Q3Texture::Flags() const noexcept { return _flags; }
    std::int32_t Q3Texture::Contents() const noexcept { return _contents; }

    Q3Plane::Q3Plane(float x, float y, float z, float distance) noexcept
        : _x(x), _y(y), _z(z), _distance(distance) {}
    float Q3Plane::X() const noexcept { return _x; }
    float Q3Plane::Y() const noexcept { return _y; }
    float Q3Plane::Z() const noexcept { return _z; }
    float Q3Plane::Distance() const noexcept { return _distance; }

    Q3Brush::Q3Brush(std::int32_t firstSide, std::int32_t sideCount, std::int32_t texture) noexcept
        : _firstSide(firstSide), _sideCount(sideCount), _texture(texture) {}
    std::int32_t Q3Brush::FirstSide() const noexcept { return _firstSide; }
    std::int32_t Q3Brush::SideCount() const noexcept { return _sideCount; }
    std::int32_t Q3Brush::Texture() const noexcept { return _texture; }

    Q3BrushSide::Q3BrushSide(std::int32_t plane, std::int32_t texture) noexcept
        : _plane(plane), _texture(texture) {}
    std::int32_t Q3BrushSide::Plane() const noexcept { return _plane; }
    std::int32_t Q3BrushSide::Texture() const noexcept { return _texture; }

    Q3Vertex::Q3Vertex(Q3FloatArray position, Q3FloatArray surface, Q3FloatArray normal, Q3ByteArray color) noexcept
        : _position(std::move(position)), _surface(std::move(surface)),
          _normal(std::move(normal)), _color(std::move(color)) {}
    Q3FloatArray Q3Vertex::Position() const noexcept { return _position; }
    Q3FloatArray Q3Vertex::Surface() const noexcept { return _surface; }
    Q3FloatArray Q3Vertex::Normal() const noexcept { return _normal; }
    Q3ByteArray Q3Vertex::Color() const noexcept { return _color; }

    Q3Face::Q3Face(std::int32_t texture, std::int32_t effect, std::int32_t type,
        std::int32_t vertex, std::int32_t vertexCount, std::int32_t meshVert,
        std::int32_t meshVertCount, Q3FloatArray normal, Q3IntArray size) noexcept
        : _texture(texture), _effect(effect), _type(type), _vertex(vertex),
          _vertexCount(vertexCount), _meshVert(meshVert), _meshVertCount(meshVertCount),
          _normal(std::move(normal)), _size(std::move(size)) {}
    std::int32_t Q3Face::Texture() const noexcept { return _texture; }
    std::int32_t Q3Face::Effect() const noexcept { return _effect; }
    std::int32_t Q3Face::Type() const noexcept { return _type; }
    std::int32_t Q3Face::Vertex() const noexcept { return _vertex; }
    std::int32_t Q3Face::VertexCount() const noexcept { return _vertexCount; }
    std::int32_t Q3Face::MeshVert() const noexcept { return _meshVert; }
    std::int32_t Q3Face::MeshVertCount() const noexcept { return _meshVertCount; }
    Q3FloatArray Q3Face::Normal() const noexcept { return _normal; }
    Q3IntArray Q3Face::Size() const noexcept { return _size; }

    Q3Model::Q3Model(Q3FloatArray mins, Q3FloatArray maxs, std::int32_t face,
        std::int32_t faceCount, std::int32_t brush, std::int32_t brushCount) noexcept
        : _mins(std::move(mins)), _maxs(std::move(maxs)), _face(face),
          _faceCount(faceCount), _brush(brush), _brushCount(brushCount) {}
    Q3FloatArray Q3Model::Mins() const noexcept { return _mins; }
    Q3FloatArray Q3Model::Maxs() const noexcept { return _maxs; }
    std::int32_t Q3Model::Face() const noexcept { return _face; }
    std::int32_t Q3Model::FaceCount() const noexcept { return _faceCount; }
    std::int32_t Q3Model::Brush() const noexcept { return _brush; }
    std::int32_t Q3Model::BrushCount() const noexcept { return _brushCount; }

    const Q3Bsp::TextureList& Q3Bsp::Textures() const noexcept { return _textures.Get(); }
    const Q3Bsp::PlaneList& Q3Bsp::Planes() const noexcept { return _planes.Get(); }
    const Q3Bsp::BrushList& Q3Bsp::Brushes() const noexcept { return _brushes.Get(); }
    const Q3Bsp::BrushSideList& Q3Bsp::BrushSides() const noexcept { return _brushSides.Get(); }
    const Q3Bsp::VertexList& Q3Bsp::Vertices() const noexcept { return _vertices.Get(); }
    const Q3Bsp::MeshVertList& Q3Bsp::MeshVerts() const noexcept { return _meshVerts.Get(); }
    const Q3Bsp::FaceList& Q3Bsp::Faces() const noexcept { return _faces.Get(); }
    const Q3Bsp::ModelList& Q3Bsp::Models() const noexcept { return _models.Get(); }
    const Q3Bsp::EntityList& Q3Bsp::Entities() const noexcept { return _entities.Get(); }

    std::vector<std::uint8_t> Q3Bsp::Trim(const std::vector<std::uint8_t>& bsp)
    {
        return Trim(&bsp);
    }

    std::vector<std::uint8_t> Q3Bsp::Trim(const std::vector<std::uint8_t>* bspReference)
    {
        if (bspReference == nullptr)
        {
            throw System::NullReferenceException();
        }
        const std::vector<std::uint8_t>& bsp = *bspReference;
        constexpr std::size_t HeaderSize = 8 + 17 * 8;
        if (bsp.size() < HeaderSize)
        {
            throw ProgramException("Not a Quake 3 level: too short to hold a header.");
        }
        ByteVector output;
        output.reserve(bsp.size() / 4);
        output.insert(output.end(), bsp.begin(), bsp.begin() + static_cast<std::ptrdiff_t>(HeaderSize));
        for (std::int32_t i = 0; i < 17; ++i)
        {
            const std::int32_t offset = ReadI32(bsp, 8 + static_cast<std::size_t>(i) * 8);
            const std::int32_t length = ReadI32(bsp, 8 + static_cast<std::size_t>(i) * 8 + 4);
            const bool used = std::find(UsedLumps.begin(), UsedLumps.end(), i) != UsedLumps.end();
            const std::int32_t end = UncheckedAdd(offset, length);
            if (!used || offset < 0 || length <= 0
                || (end > 0 && static_cast<std::uint64_t>(end) > bsp.size()))
            {
                WriteI32(output, 8 + static_cast<std::size_t>(i) * 8,
                    static_cast<std::int32_t>(output.size()));
                WriteI32(output, 8 + static_cast<std::size_t>(i) * 8 + 4, 0);
                continue;
            }
            WriteI32(output, 8 + static_cast<std::size_t>(i) * 8,
                static_cast<std::int32_t>(output.size()));
            WriteI32(output, 8 + static_cast<std::size_t>(i) * 8 + 4, length);
            const std::size_t begin = static_cast<std::size_t>(offset);
            const std::size_t count = static_cast<std::size_t>(length);
            if (begin > bsp.size() || count > bsp.size() - begin)
            {
                throw System::ArgumentOutOfRangeException();
            }
            output.insert(output.end(),
                bsp.begin() + static_cast<std::ptrdiff_t>(begin),
                bsp.begin() + static_cast<std::ptrdiff_t>(begin + count));
            while (output.size() % 4 != 0)
            {
                output.push_back(0);
            }
        }
        return output;
    }

    std::shared_ptr<Q3Bsp> Q3Bsp::Load(
        const std::string& source, const std::optional<std::string>& mapName)
    {
        return Load(&source, mapName);
    }

    std::shared_ptr<Q3Bsp> Q3Bsp::Load(
        const std::string* source, const std::optional<std::string>& mapName)
    {
        return Parse(ReadLevel(source, mapName));
    }

    std::vector<std::uint8_t> Q3Bsp::ReadLevel(
        const std::string& source, const std::optional<std::string>& mapName)
    {
        return ReadLevel(&source, mapName);
    }

    std::vector<std::uint8_t> Q3Bsp::ReadLevel(
        const std::string* source, const std::optional<std::string>& mapName)
    {
        const std::string sourceText = source == nullptr ? std::string{} : *source;
        if (source == nullptr || !MphRead::NativeRuntime::FileExists(sourceText))
        {
            throw ProgramException("No such file: " + sourceText);
        }
        if (OrdinalIgnoreCaseEquals(Extension(sourceText), ".bsp"))
        {
            return FileReadAllBytes(sourceText);
        }
        const ByteVector archive = FileReadAllBytes(sourceText);
        const std::vector<ZipEntry> entries = ReadZipEntries(archive);
        std::vector<const ZipEntry*> maps;
        for (const ZipEntry& entry : entries)
        {
            if (OrdinalIgnoreCaseEndsWith(entry.name, ".bsp"))
            {
                maps.push_back(&entry);
            }
        }
        if (maps.empty())
        {
            throw ProgramException(PathGetFileName(sourceText) + " contains no .bsp.");
        }
        const ZipEntry* selected = nullptr;
        if (!mapName.has_value())
        {
            selected = maps.front();
        }
        else
        {
            for (const ZipEntry* entry : maps)
            {
                if (OrdinalIgnoreCaseEquals(FileNameWithoutExtension(entry->name), *mapName))
                {
                    selected = entry;
                    break;
                }
            }
        }
        if (selected == nullptr)
        {
            std::vector<std::string> available;
            available.reserve(maps.size());
            for (const ZipEntry* entry : maps)
            {
                available.push_back(FileNameWithoutExtension(entry->name));
            }
            std::stable_sort(available.begin(), available.end(), CultureLess);
            std::string joined;
            for (std::size_t i = 0; i < available.size(); ++i)
            {
                if (i != 0) joined += ", ";
                joined += available[i];
            }
            throw ProgramException(PathGetFileName(sourceText) + " has no map " + *mapName + ". It has: " + joined);
        }
        return ReadZipEntry(archive, *selected);
    }

    std::vector<std::string> Q3Bsp::ListMaps(const std::string& source)
    {
        return ListMaps(&source);
    }

    std::vector<std::string> Q3Bsp::ListMaps(const std::string* source)
    {
        if (source == nullptr)
        {
            throw System::NullReferenceException();
        }
        const std::string& sourceText = *source;
        if (OrdinalIgnoreCaseEquals(Extension(sourceText), ".bsp"))
        {
            return {FileNameWithoutExtension(sourceText)};
        }
        const ByteVector archive = FileReadAllBytes(sourceText);
        const std::vector<ZipEntry> entries = ReadZipEntries(archive);
        std::vector<std::string> maps;
        for (const ZipEntry& entry : entries)
        {
            if (OrdinalIgnoreCaseEndsWith(entry.name, ".bsp"))
            {
                maps.push_back(FileNameWithoutExtension(entry.name));
            }
        }
        std::stable_sort(maps.begin(), maps.end(), CultureLess);
        return maps;
    }

    std::shared_ptr<Q3Bsp> Q3Bsp::Parse(const std::vector<std::uint8_t>& bytes)
    {
        ByteReader reader(bytes);
        const std::string magic = reader.ReadChars(4);
        const std::int32_t version = reader.ReadInt32();
        if (magic != "IBSP" || version != 46)
        {
            throw ProgramException("Not a Quake 3 level (magic " + magic
                + ", version " + std::to_string(version) + ").");
        }
        std::array<std::pair<std::int32_t, std::int32_t>, 17> offsets{};
        for (auto& offset : offsets)
        {
            offset = {reader.ReadInt32(), reader.ReadInt32()};
        }
        auto bsp = std::make_shared<Q3Bsp>();
        const std::int32_t entityOffset = offsets[0].first;
        const std::int32_t entityLength = offsets[0].second;
        if (entityOffset < 0 || entityLength < 0
            || static_cast<std::uint64_t>(entityOffset) > bytes.size()
            || static_cast<std::uint64_t>(entityLength) > bytes.size() - static_cast<std::size_t>(entityOffset))
        {
            throw System::ArgumentOutOfRangeException();
        }
        bsp->_entities = ParseEntities(DecodeAscii(bytes,
            static_cast<std::size_t>(entityOffset), static_cast<std::size_t>(entityLength)));

        bsp->_textures = ReadLump<std::shared_ptr<Q3Texture>>(reader, offsets[1], 72,
            [](ByteReader& r)
            {
                ByteVector nameBytes = r.ReadBytes(64);
                std::string name = DecodeAscii(nameBytes.data(), nameBytes.size());
                while (!name.empty() && name.back() == '\0') name.pop_back();
                const std::int32_t flags = r.ReadInt32();
                const std::int32_t contents = r.ReadInt32();
                return std::make_shared<Q3Texture>(name, flags, contents);
            });
        bsp->_planes = ReadLump<std::shared_ptr<Q3Plane>>(reader, offsets[2], 16,
            [](ByteReader& r)
            {
                const float x = r.ReadSingle();
                const float y = r.ReadSingle();
                const float z = r.ReadSingle();
                const float distance = r.ReadSingle();
                return std::make_shared<Q3Plane>(x, y, z, distance);
            });
        bsp->_models = ReadLump<std::shared_ptr<Q3Model>>(reader, offsets[7], 40,
            [](ByteReader& r)
            {
                auto mins = std::make_shared<std::vector<float>>(std::initializer_list<float>{
                    r.ReadSingle(), r.ReadSingle(), r.ReadSingle()});
                auto maxs = std::make_shared<std::vector<float>>(std::initializer_list<float>{
                    r.ReadSingle(), r.ReadSingle(), r.ReadSingle()});
                const std::int32_t face = r.ReadInt32();
                const std::int32_t faceCount = r.ReadInt32();
                const std::int32_t brush = r.ReadInt32();
                const std::int32_t brushCount = r.ReadInt32();
                return std::make_shared<Q3Model>(mins, maxs, face, faceCount, brush, brushCount);
            });
        bsp->_brushes = ReadLump<std::shared_ptr<Q3Brush>>(reader, offsets[8], 12,
            [](ByteReader& r)
            {
                const std::int32_t firstSide = r.ReadInt32();
                const std::int32_t sideCount = r.ReadInt32();
                const std::int32_t texture = r.ReadInt32();
                return std::make_shared<Q3Brush>(firstSide, sideCount, texture);
            });
        bsp->_brushSides = ReadLump<std::shared_ptr<Q3BrushSide>>(reader, offsets[9], 8,
            [](ByteReader& r)
            {
                const std::int32_t plane = r.ReadInt32();
                const std::int32_t texture = r.ReadInt32();
                return std::make_shared<Q3BrushSide>(plane, texture);
            });
        bsp->_vertices = ReadLump<std::shared_ptr<Q3Vertex>>(reader, offsets[10], 44,
            [](ByteReader& r)
            {
                auto position = std::make_shared<std::vector<float>>(std::initializer_list<float>{
                    r.ReadSingle(), r.ReadSingle(), r.ReadSingle()});
                auto surface = std::make_shared<std::vector<float>>(std::initializer_list<float>{
                    r.ReadSingle(), r.ReadSingle()});
                (void)r.ReadSingle();
                (void)r.ReadSingle();
                auto normal = std::make_shared<std::vector<float>>(std::initializer_list<float>{
                    r.ReadSingle(), r.ReadSingle(), r.ReadSingle()});
                auto color = std::make_shared<std::vector<std::uint8_t>>(r.ReadBytes(4));
                return std::make_shared<Q3Vertex>(position, surface, normal, color);
            });
        bsp->_meshVerts = ReadLump<std::int32_t>(reader, offsets[11], 4,
            [](ByteReader& r) { return r.ReadInt32(); });
        bsp->_faces = ReadLump<std::shared_ptr<Q3Face>>(reader, offsets[13], 104,
            [](ByteReader& r)
            {
                const std::int32_t texture = r.ReadInt32();
                const std::int32_t effect = r.ReadInt32();
                const std::int32_t type = r.ReadInt32();
                const std::int32_t vertex = r.ReadInt32();
                const std::int32_t vertexCount = r.ReadInt32();
                const std::int32_t meshVert = r.ReadInt32();
                const std::int32_t meshVertCount = r.ReadInt32();
                (void)r.ReadInt32();
                (void)r.ReadBytes(8 + 8 + 12 + 24);
                auto normal = std::make_shared<std::vector<float>>(std::initializer_list<float>{
                    r.ReadSingle(), r.ReadSingle(), r.ReadSingle()});
                auto size = std::make_shared<std::vector<std::int32_t>>(std::initializer_list<std::int32_t>{
                    r.ReadInt32(), r.ReadInt32()});
                return std::make_shared<Q3Face>(texture, effect, type, vertex, vertexCount,
                    meshVert, meshVertCount, normal, size);
            });
        return bsp;
    }

    Q3Bsp::EntityList Q3Bsp::ParseEntities(const std::string& text)
    {
        EntityList results;
        std::shared_ptr<Q3Entity> current;
        std::string token;
        std::vector<std::string> tokens;
        bool inString = false;
        for (char c : text)
        {
            if (c == '"')
            {
                if (inString)
                {
                    tokens.push_back(token);
                    token.clear();
                }
                inString = !inString;
                continue;
            }
            if (inString)
            {
                token.push_back(c);
                continue;
            }
            if (c == '{')
            {
                current = std::make_shared<Q3Entity>();
                tokens.clear();
            }
            else if (c == '}')
            {
                if (current)
                {
                    for (std::size_t i = 0; i + 1 < tokens.size(); i += 2)
                    {
                        (*current)[tokens[i]] = tokens[i + 1];
                    }
                    results.push_back(current);
                    current.reset();
                }
                tokens.clear();
            }
        }
        return results;
    }
}
