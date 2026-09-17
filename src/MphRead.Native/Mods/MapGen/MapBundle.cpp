#include "MapBundle.hpp"

#include "MapDefinition.hpp"
#include "Q3Bsp.hpp"
#include "Q3Import.hpp"
#include "../../Program.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/types.h>
#endif

namespace MphRead::Mods::MapGen
{
    // Transitional dependency surface. CustomRooms is a later dependency-order
    // item; this declaration binds exactly the member used by MapBundle without
    // changing that file in this migration slice.
    class CustomRooms final
    {
    public:
        [[nodiscard]] static const std::string& MapDirectory();
    };
}

namespace
{
    using ByteVector = std::vector<std::uint8_t>;
    using MphRead::Mods::MapGen::MapBundle;

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
    {
#if defined(__cpp_char8_t)
        std::u8string converted;
        converted.reserve(value.size());
        for (unsigned char ch : value)
        {
            converted.push_back(static_cast<char8_t>(ch));
        }
        return std::filesystem::path(converted);
#else
        return std::filesystem::u8path(value.begin(), value.end());
#endif
    }

    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        std::string result;
        result.reserve(value.size());
        for (char8_t ch : value)
        {
            result.push_back(static_cast<char>(ch));
        }
        return result;
#else
        return path.u8string();
#endif
    }

    [[nodiscard]] std::string GetFileName(const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).filename());
    }

    [[nodiscard]] std::string GetFileNameWithoutExtension(const std::string& path)
    {
        std::string fileName = GetFileName(path);
        const std::size_t dot = fileName.find_last_of('.');
        if (dot == std::string::npos)
        {
            return fileName;
        }
        fileName.resize(dot);
        return fileName;
    }

    [[nodiscard]] std::string GetExtension(const std::string& path)
    {
        const std::string fileName = GetFileName(path);
        const std::size_t dot = fileName.find_last_of('.');
        if (dot == std::string::npos || dot + 1 == fileName.size())
        {
            return {};
        }
        return fileName.substr(dot);
    }

    [[nodiscard]] std::string CombinePath(const std::string& left, const std::string& right)
    {
        return PathToUtf8(PathFromUtf8(left) / PathFromUtf8(right));
    }

    void AppendUtf8(std::string& output, std::uint32_t scalar)
    {
        if (scalar <= 0x7FU)
        {
            output.push_back(static_cast<char>(scalar));
        }
        else if (scalar <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (scalar >> 6)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
        else if (scalar <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (scalar >> 12)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (scalar >> 18)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
    }

    enum class Utf8Status
    {
        Done,
        NeedMore,
        Invalid
    };

    [[nodiscard]] Utf8Status DecodeUtf8Scalar(
        const std::uint8_t* data, std::size_t size, std::size_t& position,
        std::uint32_t& scalar) noexcept
    {
        const std::size_t start = position;
        if (start >= size)
        {
            scalar = 0xFFFDU;
            return Utf8Status::NeedMore;
        }
        const std::uint32_t first = data[start];
        if (first <= 0x7FU)
        {
            scalar = first;
            position = start + 1;
            return Utf8Status::Done;
        }
        if (first < 0xC2U || first > 0xF4U)
        {
            scalar = 0xFFFDU;
            position = start + 1;
            return Utf8Status::Invalid;
        }
        if (start + 1 >= size)
        {
            scalar = 0xFFFDU;
            position = size;
            return Utf8Status::NeedMore;
        }
        const std::uint32_t second = data[start + 1];
        if ((second & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            position = start + 1;
            return Utf8Status::Invalid;
        }
        if (first <= 0xDFU)
        {
            scalar = ((first & 0x1FU) << 6) | (second & 0x3FU);
            position = start + 2;
            return Utf8Status::Done;
        }
        if ((first == 0xE0U && second < 0xA0U)
            || (first == 0xEDU && second >= 0xA0U)
            || (first == 0xF0U && second < 0x90U)
            || (first == 0xF4U && second >= 0x90U))
        {
            scalar = 0xFFFDU;
            position = start + 1;
            return Utf8Status::Invalid;
        }
        if (start + 2 >= size)
        {
            scalar = 0xFFFDU;
            position = size;
            return Utf8Status::NeedMore;
        }
        const std::uint32_t third = data[start + 2];
        if ((third & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            position = start + 2;
            return Utf8Status::Invalid;
        }
        if (first <= 0xEFU)
        {
            scalar = ((first & 0x0FU) << 12) | ((second & 0x3FU) << 6) | (third & 0x3FU);
            position = start + 3;
            return Utf8Status::Done;
        }
        if (start + 3 >= size)
        {
            scalar = 0xFFFDU;
            position = size;
            return Utf8Status::NeedMore;
        }
        const std::uint32_t fourth = data[start + 3];
        if ((fourth & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            position = start + 3;
            return Utf8Status::Invalid;
        }
        scalar = ((first & 0x07U) << 18) | ((second & 0x3FU) << 12)
            | ((third & 0x3FU) << 6) | (fourth & 0x3FU);
        position = start + 4;
        return Utf8Status::Done;
    }

    [[nodiscard]] std::vector<std::uint32_t> DecodeUtf8Folded(const std::string& value)
    {
        std::vector<std::uint32_t> result;
        result.reserve(value.size());
        std::size_t position = 0;
        while (position < value.size())
        {
            std::uint32_t scalar = 0;
            (void)DecodeUtf8Scalar(
                reinterpret_cast<const std::uint8_t*>(value.data()), value.size(), position, scalar);
            if (scalar >= 'a' && scalar <= 'z')
            {
                scalar -= static_cast<std::uint32_t>('a' - 'A');
            }
            else if (scalar == 0x00FFU)
            {
                scalar = 0x0178U;
            }
            else if (scalar >= 0x00E0U && scalar <= 0x00F6U)
            {
                scalar -= 0x20U;
            }
            else if (scalar >= 0x00F8U && scalar <= 0x00FEU)
            {
                scalar -= 0x20U;
            }
            else if (scalar >= 0x03B1U && scalar <= 0x03C1U)
            {
                scalar -= 0x20U;
            }
            else if (scalar >= 0x03C3U && scalar <= 0x03CBU)
            {
                scalar -= 0x20U;
            }
            else if (scalar >= 0x0430U && scalar <= 0x044FU)
            {
                scalar -= 0x20U;
            }
            // .NET OrdinalIgnoreCase intentionally leaves dotless-i and long-s
            // distinct in ordinal comparisons.
            result.push_back(scalar);
        }
        return result;
    }

    [[nodiscard]] bool OrdinalIgnoreCaseEquals(
        const std::string& left, const std::string& right)
    {
        if (left == right)
        {
            return true;
        }
        return DecodeUtf8Folded(left) == DecodeUtf8Folded(right);
    }

    [[nodiscard]] bool OrdinalIgnoreCaseEndsWith(
        const std::string& value, const std::string& suffix)
    {
        const std::vector<std::uint32_t> foldedValue = DecodeUtf8Folded(value);
        const std::vector<std::uint32_t> foldedSuffix = DecodeUtf8Folded(suffix);
        if (foldedSuffix.size() > foldedValue.size())
        {
            return false;
        }
        return std::equal(
            foldedSuffix.begin(), foldedSuffix.end(),
            foldedValue.end() - static_cast<std::ptrdiff_t>(foldedSuffix.size()));
    }

    [[nodiscard]] std::uint16_t ReadU16(const ByteVector& bytes, std::size_t offset)
    {
        if (offset > bytes.size() || bytes.size() - offset < 2)
        {
            throw std::runtime_error("Unexpected end of ZIP data.");
        }
        return static_cast<std::uint16_t>(bytes[offset])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
    }

    [[nodiscard]] std::uint32_t ReadU32(const ByteVector& bytes, std::size_t offset)
    {
        if (offset > bytes.size() || bytes.size() - offset < 4)
        {
            throw std::runtime_error("Unexpected end of ZIP data.");
        }
        return static_cast<std::uint32_t>(bytes[offset])
            | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
            | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
            | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }

    [[nodiscard]] std::uint64_t ReadU64(const ByteVector& bytes, std::size_t offset)
    {
        const std::uint64_t low = ReadU32(bytes, offset);
        const std::uint64_t high = ReadU32(bytes, offset + 4);
        return low | (high << 32);
    }

    void PushU16(ByteVector& bytes, std::uint16_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    void PushU32(ByteVector& bytes, std::uint32_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
        bytes.push_back(static_cast<std::uint8_t>(value >> 16));
        bytes.push_back(static_cast<std::uint8_t>(value >> 24));
    }

    void PushU64(ByteVector& bytes, std::uint64_t value)
    {
        PushU32(bytes, static_cast<std::uint32_t>(value));
        PushU32(bytes, static_cast<std::uint32_t>(value >> 32));
    }

    void WriteBytes(std::ofstream& stream, const ByteVector& bytes)
    {
        if (!bytes.empty())
        {
            stream.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        if (!stream)
        {
            throw std::runtime_error("Could not write ZIP archive.");
        }
    }

    [[nodiscard]] ByteVector ReadAllBytes(const std::string& path)
    {
        std::ifstream stream(PathFromUtf8(path), std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Could not open file: " + path);
        }
        stream.seekg(0, std::ios::end);
        const std::streamoff length = stream.tellg();
        if (length < 0)
        {
            throw std::runtime_error("Could not read file: " + path);
        }
        stream.seekg(0, std::ios::beg);
        ByteVector bytes(static_cast<std::size_t>(length));
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), length);
            if (!stream)
            {
                throw std::runtime_error("Could not read file: " + path);
            }
        }
        return bytes;
    }

    [[nodiscard]] std::uint32_t Crc32(const ByteVector& bytes) noexcept
    {
        std::uint32_t crc = 0xFFFFFFFFU;
        for (std::uint8_t byte : bytes)
        {
            crc ^= byte;
            for (int bit = 0; bit < 8; ++bit)
            {
                const std::uint32_t mask = 0U - (crc & 1U);
                crc = (crc >> 1) ^ (0xEDB88320U & mask);
            }
        }
        return ~crc;
    }

    using ZByte = unsigned char;
    using ZUInt = unsigned int;
    using ZULong = unsigned long;
    struct ZInternalState;
    using ZAlloc = void* (*)(void*, ZUInt, ZUInt);
    using ZFree = void (*)(void*, void*);

    struct ZStream
    {
        ZByte* next_in = nullptr;
        ZUInt avail_in = 0;
        ZULong total_in = 0;
        ZByte* next_out = nullptr;
        ZUInt avail_out = 0;
        ZULong total_out = 0;
        char* msg = nullptr;
        ZInternalState* state = nullptr;
        ZAlloc zalloc = nullptr;
        ZFree zfree = nullptr;
        void* opaque = nullptr;
        int data_type = 0;
        ZULong adler = 0;
        ZULong reserved = 0;
    };

    class ZLib final
    {
    public:
        using VersionFunction = const char* (*)();
        using Init2Function = int (*)(ZStream*, int, int, int, int, int, const char*, int);
        using ProcessFunction = int (*)(ZStream*, int);
        using EndFunction = int (*)(ZStream*);

        ZLib()
        {
#if defined(_WIN32)
            const wchar_t* names[] = {L"zlib1.dll", L"zlib.dll"};
            for (const wchar_t* name : names)
            {
                _library = LoadLibraryW(name);
                if (_library != nullptr)
                {
                    break;
                }
            }
            if (_library == nullptr)
            {
                throw std::runtime_error("zlib is required to read and write map bundles.");
            }
            _version = Load<VersionFunction>("zlibVersion");
            _deflateInit2 = Load<Init2Function>("deflateInit2_");
            _deflate = Load<ProcessFunction>("deflate");
            _deflateEnd = Load<EndFunction>("deflateEnd");
            _inflate = Load<ProcessFunction>("inflate");
            _inflateEnd = Load<EndFunction>("inflateEnd");
#else
            const char* names[] = {"libz.so.1", "libz.so", "libz.dylib"};
            for (const char* name : names)
            {
                _library = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
                if (_library != nullptr)
                {
                    break;
                }
            }
            if (_library == nullptr)
            {
                throw std::runtime_error("zlib is required to read and write map bundles.");
            }
            _version = Load<VersionFunction>("zlibVersion");
            _deflateInit2 = Load<Init2Function>("deflateInit2_");
            _deflate = Load<ProcessFunction>("deflate");
            _deflateEnd = Load<EndFunction>("deflateEnd");
            _inflate = Load<ProcessFunction>("inflate");
            _inflateEnd = Load<EndFunction>("inflateEnd");
#endif
        }

        ~ZLib()
        {
#if defined(_WIN32)
            if (_library != nullptr)
            {
                FreeLibrary(_library);
            }
#else
            if (_library != nullptr)
            {
                dlclose(_library);
            }
#endif
        }

        ZLib(const ZLib&) = delete;
        ZLib& operator=(const ZLib&) = delete;

        [[nodiscard]] const char* Version() const { return _version(); }
        [[nodiscard]] int DeflateInit2(
            ZStream* stream, int level, int method, int windowBits, int memoryLevel, int strategy) const
        {
            return _deflateInit2(
                stream, level, method, windowBits, memoryLevel, strategy, Version(),
                static_cast<int>(sizeof(ZStream)));
        }
        [[nodiscard]] int Deflate(ZStream* stream, int flush) const
        {
            return _deflate(stream, flush);
        }
        [[nodiscard]] int DeflateEnd(ZStream* stream) const
        {
            return _deflateEnd(stream);
        }
        [[nodiscard]] int Inflate(ZStream* stream, int flush) const
        {
            return _inflate(stream, flush);
        }
        [[nodiscard]] int InflateEnd(ZStream* stream) const
        {
            return _inflateEnd(stream);
        }

    private:
#if defined(_WIN32)
        HMODULE _library = nullptr;
        template <typename T>
        [[nodiscard]] T Load(const char* name)
        {
            FARPROC proc = GetProcAddress(_library, name);
            if (proc == nullptr)
            {
                throw std::runtime_error(std::string("zlib is missing ") + name + ".");
            }
            return reinterpret_cast<T>(proc);
        }
#else
        void* _library = nullptr;
        template <typename T>
        [[nodiscard]] T Load(const char* name)
        {
            void* proc = dlsym(_library, name);
            if (proc == nullptr)
            {
                throw std::runtime_error(std::string("zlib is missing ") + name + ".");
            }
            return reinterpret_cast<T>(proc);
        }
#endif

        VersionFunction _version = nullptr;
        Init2Function _deflateInit2 = nullptr;
        ProcessFunction _deflate = nullptr;
        EndFunction _deflateEnd = nullptr;
        ProcessFunction _inflate = nullptr;
        EndFunction _inflateEnd = nullptr;
    };

    [[nodiscard]] const ZLib& Zlib()
    {
        static const ZLib value;
        return value;
    }

    [[nodiscard]] ByteVector DeflateRaw(const ByteVector& input)
    {
        constexpr int ZNoFlush = 0;
        constexpr int ZFinish = 4;
        constexpr int ZOk = 0;
        constexpr int ZStreamEnd = 1;
        constexpr int ZDeflated = 8;
        constexpr int ZDefaultStrategy = 0;

        ZStream stream{};
        const ZLib& zlib = Zlib();
        if (zlib.DeflateInit2(&stream, 9, ZDeflated, -15, 8, ZDefaultStrategy) != ZOk)
        {
            throw std::runtime_error("Could not initialize DEFLATE compression.");
        }

        ByteVector output;
        std::array<std::uint8_t, 65536> buffer{};
        std::size_t inputOffset = 0;
        try
        {
            int result = ZOk;
            while (result != ZStreamEnd)
            {
                if (stream.avail_in == 0 && inputOffset < input.size())
                {
                    const std::size_t count = std::min<std::size_t>(
                        input.size() - inputOffset, std::numeric_limits<ZUInt>::max());
                    stream.next_in = const_cast<ZByte*>(
                        reinterpret_cast<const ZByte*>(input.data() + inputOffset));
                    stream.avail_in = static_cast<ZUInt>(count);
                    inputOffset += count;
                }

                stream.next_out = buffer.data();
                stream.avail_out = static_cast<ZUInt>(buffer.size());
                const int flush = inputOffset == input.size() && stream.avail_in == 0
                    ? ZFinish : ZNoFlush;
                result = zlib.Deflate(&stream, flush);
                if (result != ZOk && result != ZStreamEnd)
                {
                    throw std::runtime_error("DEFLATE compression failed.");
                }
                const std::size_t produced = buffer.size() - stream.avail_out;
                output.insert(output.end(), buffer.begin(),
                    buffer.begin() + static_cast<std::ptrdiff_t>(produced));
            }
        }
        catch (...)
        {
            (void)zlib.DeflateEnd(&stream);
            throw;
        }
        if (zlib.DeflateEnd(&stream) != ZOk)
        {
            throw std::runtime_error("Could not finish DEFLATE compression.");
        }
        return output;
    }

    [[nodiscard]] ByteVector InflateRaw(
        const ByteVector& input, std::size_t expectedSize)
    {
        constexpr int ZNoFlush = 0;
        constexpr int ZOk = 0;
        constexpr int ZStreamEnd = 1;

        ZStream stream{};
        const ZLib& zlib = Zlib();
        // inflateInit2_ has a different exported signature from deflateInit2_.
        using InflateInit2Function = int (*)(ZStream*, int, const char*, int);
#if defined(_WIN32)
        HMODULE library = LoadLibraryW(L"zlib1.dll");
        if (library == nullptr)
        {
            library = LoadLibraryW(L"zlib.dll");
        }
        if (library == nullptr)
        {
            throw std::runtime_error("zlib is required to read map bundles.");
        }
        auto init = reinterpret_cast<InflateInit2Function>(
            GetProcAddress(library, "inflateInit2_"));
#else
        void* library = dlopen("libz.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (library == nullptr)
        {
            library = dlopen("libz.so", RTLD_LAZY | RTLD_LOCAL);
        }
#if defined(__APPLE__)
        if (library == nullptr)
        {
            library = dlopen("libz.dylib", RTLD_LAZY | RTLD_LOCAL);
        }
#endif
        if (library == nullptr)
        {
            throw std::runtime_error("zlib is required to read map bundles.");
        }
        auto init = reinterpret_cast<InflateInit2Function>(dlsym(library, "inflateInit2_"));
#endif
        if (init == nullptr || init(&stream, -15, zlib.Version(),
            static_cast<int>(sizeof(ZStream))) != ZOk)
        {
#if defined(_WIN32)
            FreeLibrary(library);
#else
            dlclose(library);
#endif
            throw std::runtime_error("Could not initialize DEFLATE decompression.");
        }

        ByteVector output;
        output.reserve(expectedSize);
        std::array<std::uint8_t, 65536> buffer{};
        std::size_t inputOffset = 0;
        try
        {
            int result = ZOk;
            while (result != ZStreamEnd)
            {
                if (stream.avail_in == 0 && inputOffset < input.size())
                {
                    const std::size_t count = std::min<std::size_t>(
                        input.size() - inputOffset, std::numeric_limits<ZUInt>::max());
                    stream.next_in = const_cast<ZByte*>(
                        reinterpret_cast<const ZByte*>(input.data() + inputOffset));
                    stream.avail_in = static_cast<ZUInt>(count);
                    inputOffset += count;
                }

                stream.next_out = buffer.data();
                stream.avail_out = static_cast<ZUInt>(buffer.size());
                result = zlib.Inflate(&stream, ZNoFlush);
                if (result != ZOk && result != ZStreamEnd)
                {
                    throw std::runtime_error("DEFLATE decompression failed.");
                }
                const std::size_t produced = buffer.size() - stream.avail_out;
                output.insert(output.end(), buffer.begin(),
                    buffer.begin() + static_cast<std::ptrdiff_t>(produced));

                if (result != ZStreamEnd && stream.avail_in == 0
                    && inputOffset == input.size() && produced == 0)
                {
                    throw std::runtime_error("Unexpected end of DEFLATE data.");
                }
            }
        }
        catch (...)
        {
            (void)zlib.InflateEnd(&stream);
#if defined(_WIN32)
            FreeLibrary(library);
#else
            dlclose(library);
#endif
            throw;
        }
        const int endResult = zlib.InflateEnd(&stream);
#if defined(_WIN32)
        FreeLibrary(library);
#else
        dlclose(library);
#endif
        if (endResult != ZOk)
        {
            throw std::runtime_error("Could not finish DEFLATE decompression.");
        }
        return output;
    }

    struct ZipEntry
    {
        std::string Name;
        std::uint16_t Flags = 0;
        std::uint16_t Method = 0;
        std::uint64_t CompressedSize = 0;
        std::uint64_t UncompressedSize = 0;
        std::uint64_t LocalOffset = 0;
    };

    [[nodiscard]] std::uint64_t ReadZip64Value(
        const ByteVector& extra, std::size_t& position)
    {
        if (position > extra.size() || extra.size() - position < 8)
        {
            throw std::runtime_error("Invalid ZIP64 extra field.");
        }
        const std::uint64_t value = ReadU64(extra, position);
        position += 8;
        return value;
    }

    void ApplyZip64Extra(
        ZipEntry& entry, const ByteVector& extra,
        bool needUncompressed, bool needCompressed, bool needOffset)
    {
        std::size_t cursor = 0;
        while (cursor + 4 <= extra.size())
        {
            const std::uint16_t tag = ReadU16(extra, cursor);
            const std::uint16_t size = ReadU16(extra, cursor + 2);
            cursor += 4;
            if (cursor > extra.size() || size > extra.size() - cursor)
            {
                throw std::runtime_error("Invalid ZIP extra field.");
            }
            if (tag == 0x0001U)
            {
                ByteVector field(
                    extra.begin() + static_cast<std::ptrdiff_t>(cursor),
                    extra.begin() + static_cast<std::ptrdiff_t>(cursor + size));
                std::size_t position = 0;
                if (needUncompressed)
                {
                    entry.UncompressedSize = ReadZip64Value(field, position);
                }
                if (needCompressed)
                {
                    entry.CompressedSize = ReadZip64Value(field, position);
                }
                if (needOffset)
                {
                    entry.LocalOffset = ReadZip64Value(field, position);
                }
                return;
            }
            cursor += size;
        }
        if (needUncompressed || needCompressed || needOffset)
        {
            throw std::runtime_error("ZIP64 entry is missing its ZIP64 extra field.");
        }
    }

    [[nodiscard]] std::vector<ZipEntry> ReadZipEntries(const ByteVector& archive)
    {
        if (archive.size() < 22)
        {
            throw std::runtime_error("Central Directory corrupt.");
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
            throw std::runtime_error("End of Central Directory record could not be found.");
        }
        if (ReadU16(archive, eocd + 4) != 0 || ReadU16(archive, eocd + 6) != 0)
        {
            throw std::runtime_error("Split or spanned ZIP archives are not supported.");
        }

        std::uint64_t entryCount = ReadU16(archive, eocd + 10);
        std::uint64_t centralSize = ReadU32(archive, eocd + 12);
        std::uint64_t centralOffset = ReadU32(archive, eocd + 16);
        if (entryCount == 0xFFFFU || centralSize == 0xFFFFFFFFULL
            || centralOffset == 0xFFFFFFFFULL)
        {
            if (eocd < 20 || ReadU32(archive, eocd - 20) != 0x07064B50U)
            {
                throw std::runtime_error("ZIP64 locator could not be found.");
            }
            const std::uint64_t zip64Offset = ReadU64(archive, eocd - 12);
            if (zip64Offset > archive.size()
                || archive.size() - static_cast<std::size_t>(zip64Offset) < 56
                || ReadU32(archive, static_cast<std::size_t>(zip64Offset)) != 0x06064B50U)
            {
                throw std::runtime_error("ZIP64 End of Central Directory record is invalid.");
            }
            const std::size_t offset = static_cast<std::size_t>(zip64Offset);
            if (ReadU32(archive, offset + 16) != 0 || ReadU32(archive, offset + 20) != 0)
            {
                throw std::runtime_error("Split or spanned ZIP archives are not supported.");
            }
            entryCount = ReadU64(archive, offset + 32);
            centralSize = ReadU64(archive, offset + 40);
            centralOffset = ReadU64(archive, offset + 48);
        }

        if (centralOffset > archive.size()
            || centralSize > archive.size() - static_cast<std::size_t>(centralOffset))
        {
            throw std::runtime_error("Central Directory corrupt.");
        }
        if (entryCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            throw std::length_error("Too many ZIP entries.");
        }

        std::vector<ZipEntry> entries;
        entries.reserve(static_cast<std::size_t>(entryCount));
        std::size_t cursor = static_cast<std::size_t>(centralOffset);
        for (std::uint64_t index = 0; index < entryCount; ++index)
        {
            if (cursor > archive.size() || archive.size() - cursor < 46
                || ReadU32(archive, cursor) != 0x02014B50U)
            {
                throw std::runtime_error("Central Directory corrupt.");
            }

            ZipEntry entry;
            entry.Flags = ReadU16(archive, cursor + 8);
            entry.Method = ReadU16(archive, cursor + 10);
            const std::uint32_t compressed32 = ReadU32(archive, cursor + 20);
            const std::uint32_t uncompressed32 = ReadU32(archive, cursor + 24);
            const std::uint16_t nameLength = ReadU16(archive, cursor + 28);
            const std::uint16_t extraLength = ReadU16(archive, cursor + 30);
            const std::uint16_t commentLength = ReadU16(archive, cursor + 32);
            const std::uint32_t offset32 = ReadU32(archive, cursor + 42);
            const std::size_t variable = static_cast<std::size_t>(nameLength)
                + static_cast<std::size_t>(extraLength)
                + static_cast<std::size_t>(commentLength);
            if (46 + variable > archive.size() - cursor)
            {
                throw std::runtime_error("Central Directory corrupt.");
            }

            const char* name = reinterpret_cast<const char*>(archive.data() + cursor + 46);
            entry.Name.assign(name, name + nameLength);
            entry.CompressedSize = compressed32;
            entry.UncompressedSize = uncompressed32;
            entry.LocalOffset = offset32;

            ByteVector extra(
                archive.begin() + static_cast<std::ptrdiff_t>(cursor + 46 + nameLength),
                archive.begin() + static_cast<std::ptrdiff_t>(
                    cursor + 46 + nameLength + extraLength));
            ApplyZip64Extra(
                entry, extra,
                uncompressed32 == 0xFFFFFFFFU,
                compressed32 == 0xFFFFFFFFU,
                offset32 == 0xFFFFFFFFU);
            entries.push_back(std::move(entry));
            cursor += 46 + variable;
        }
        return entries;
    }

    [[nodiscard]] ByteVector ReadZipEntry(
        const ByteVector& archive, const ZipEntry& entry)
    {
        if ((entry.Flags & 1U) != 0)
        {
            throw std::runtime_error("Encrypted ZIP entries are not supported.");
        }
        if (entry.LocalOffset > archive.size()
            || archive.size() - static_cast<std::size_t>(entry.LocalOffset) < 30)
        {
            throw std::runtime_error("Local file header is invalid.");
        }
        const std::size_t local = static_cast<std::size_t>(entry.LocalOffset);
        if (ReadU32(archive, local) != 0x04034B50U)
        {
            throw std::runtime_error("Local file header is invalid.");
        }
        const std::uint16_t nameLength = ReadU16(archive, local + 26);
        const std::uint16_t extraLength = ReadU16(archive, local + 28);
        const std::uint64_t dataOffset64 =
            entry.LocalOffset + 30ULL + nameLength + extraLength;
        if (dataOffset64 > archive.size()
            || entry.CompressedSize > archive.size() - static_cast<std::size_t>(dataOffset64))
        {
            throw std::runtime_error("ZIP entry data is invalid.");
        }
        if (entry.CompressedSize
            > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
            || entry.UncompressedSize
            > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            throw std::length_error("ZIP entry is too large.");
        }

        const std::size_t dataOffset = static_cast<std::size_t>(dataOffset64);
        const std::size_t compressedSize = static_cast<std::size_t>(entry.CompressedSize);
        ByteVector compressed(
            archive.begin() + static_cast<std::ptrdiff_t>(dataOffset),
            archive.begin() + static_cast<std::ptrdiff_t>(dataOffset + compressedSize));
        if (entry.Method == 0)
        {
            return compressed;
        }
        if (entry.Method == 8)
        {
            return InflateRaw(compressed, static_cast<std::size_t>(entry.UncompressedSize));
        }
        throw std::runtime_error("The ZIP entry uses an unsupported compression method.");
    }

    [[nodiscard]] std::string DecodeUtf16(
        const ByteVector& bytes, std::size_t offset, bool littleEndian)
    {
        std::string result;
        for (std::size_t position = offset; position + 1 < bytes.size(); position += 2)
        {
            const std::uint16_t first = littleEndian
                ? static_cast<std::uint16_t>(bytes[position]
                    | (static_cast<std::uint16_t>(bytes[position + 1]) << 8))
                : static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(bytes[position]) << 8)
                    | bytes[position + 1]);
            if (first >= 0xD800U && first <= 0xDBFFU)
            {
                if (position + 3 < bytes.size())
                {
                    const std::uint16_t second = littleEndian
                        ? static_cast<std::uint16_t>(bytes[position + 2]
                            | (static_cast<std::uint16_t>(bytes[position + 3]) << 8))
                        : static_cast<std::uint16_t>(
                            (static_cast<std::uint16_t>(bytes[position + 2]) << 8)
                            | bytes[position + 3]);
                    if (second >= 0xDC00U && second <= 0xDFFFU)
                    {
                        AppendUtf8(result, 0x10000U
                            + ((static_cast<std::uint32_t>(first) - 0xD800U) << 10)
                            + (static_cast<std::uint32_t>(second) - 0xDC00U));
                        position += 2;
                        continue;
                    }
                }
                AppendUtf8(result, 0xFFFDU);
            }
            else if (first >= 0xDC00U && first <= 0xDFFFU)
            {
                AppendUtf8(result, 0xFFFDU);
            }
            else
            {
                AppendUtf8(result, first);
            }
        }
        if (((bytes.size() - offset) & 1U) != 0)
        {
            AppendUtf8(result, 0xFFFDU);
        }
        return result;
    }

    [[nodiscard]] std::string DecodeUtf32(
        const ByteVector& bytes, std::size_t offset, bool littleEndian)
    {
        std::string result;
        std::size_t position = offset;
        while (position + 3 < bytes.size())
        {
            const std::uint32_t scalar = littleEndian
                ? static_cast<std::uint32_t>(bytes[position])
                    | (static_cast<std::uint32_t>(bytes[position + 1]) << 8)
                    | (static_cast<std::uint32_t>(bytes[position + 2]) << 16)
                    | (static_cast<std::uint32_t>(bytes[position + 3]) << 24)
                : (static_cast<std::uint32_t>(bytes[position]) << 24)
                    | (static_cast<std::uint32_t>(bytes[position + 1]) << 16)
                    | (static_cast<std::uint32_t>(bytes[position + 2]) << 8)
                    | static_cast<std::uint32_t>(bytes[position + 3]);
            if (scalar > 0x10FFFFU || (scalar >= 0xD800U && scalar <= 0xDFFFU))
            {
                AppendUtf8(result, 0xFFFDU);
            }
            else
            {
                AppendUtf8(result, scalar);
            }
            position += 4;
        }
        if (position != bytes.size())
        {
            AppendUtf8(result, 0xFFFDU);
        }
        return result;
    }

    [[nodiscard]] std::string DecodeStreamReaderText(const ByteVector& bytes)
    {
        if (bytes.size() >= 4
            && bytes[0] == 0xFFU && bytes[1] == 0xFEU
            && bytes[2] == 0x00U && bytes[3] == 0x00U)
        {
            return DecodeUtf32(bytes, 4, true);
        }
        if (bytes.size() >= 4
            && bytes[0] == 0x00U && bytes[1] == 0x00U
            && bytes[2] == 0xFEU && bytes[3] == 0xFFU)
        {
            return DecodeUtf32(bytes, 4, false);
        }
        if (bytes.size() >= 3
            && bytes[0] == 0xEFU && bytes[1] == 0xBBU && bytes[2] == 0xBFU)
        {
            std::string result;
            std::size_t position = 3;
            while (position < bytes.size())
            {
                std::uint32_t scalar = 0;
                (void)DecodeUtf8Scalar(bytes.data(), bytes.size(), position, scalar);
                AppendUtf8(result, scalar);
            }
            return result;
        }
        if (bytes.size() >= 2 && bytes[0] == 0xFFU && bytes[1] == 0xFEU)
        {
            return DecodeUtf16(bytes, 2, true);
        }
        if (bytes.size() >= 2 && bytes[0] == 0xFEU && bytes[1] == 0xFFU)
        {
            return DecodeUtf16(bytes, 2, false);
        }

        std::string result;
        std::size_t position = 0;
        while (position < bytes.size())
        {
            std::uint32_t scalar = 0;
            (void)DecodeUtf8Scalar(bytes.data(), bytes.size(), position, scalar);
            AppendUtf8(result, scalar);
        }
        return result;
    }

    struct DosTimestamp
    {
        std::uint16_t Time = 0;
        std::uint16_t Date = 0;
    };

    [[nodiscard]] DosTimestamp CurrentDosTimestamp()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#if defined(_WIN32)
        if (localtime_s(&local, &now) != 0)
#else
        if (localtime_r(&now, &local) == nullptr)
#endif
        {
            throw std::runtime_error("Could not get the local time.");
        }

        int year = local.tm_year + 1900;
        if (year < 1980)
        {
            year = 1980;
        }
        if (year > 2107)
        {
            year = 2107;
        }
        DosTimestamp result;
        result.Time = static_cast<std::uint16_t>(
            ((local.tm_hour & 0x1F) << 11)
            | ((local.tm_min & 0x3F) << 5)
            | ((local.tm_sec / 2) & 0x1F));
        result.Date = static_cast<std::uint16_t>(
            (((year - 1980) & 0x7F) << 9)
            | (((local.tm_mon + 1) & 0x0F) << 5)
            | (local.tm_mday & 0x1F));
        return result;
    }

    struct WrittenEntry
    {
        std::string Name;
        std::uint32_t Crc = 0;
        std::uint64_t CompressedSize = 0;
        std::uint64_t UncompressedSize = 0;
        std::uint64_t LocalOffset = 0;
        DosTimestamp Timestamp{};
    };

    [[nodiscard]] std::uint64_t StreamPosition(std::ofstream& stream)
    {
        const std::streampos position = stream.tellp();
        if (position < 0)
        {
            throw std::runtime_error("Could not determine ZIP archive position.");
        }
        return static_cast<std::uint64_t>(position);
    }

    void WriteRaw(std::ofstream& stream, const char* data, std::size_t size)
    {
        if (size != 0)
        {
            stream.write(data, static_cast<std::streamsize>(size));
        }
        if (!stream)
        {
            throw std::runtime_error("Could not write ZIP archive.");
        }
    }

    [[nodiscard]] WrittenEntry WriteZipEntry(
        std::ofstream& stream, const std::string& name, const ByteVector& bytes)
    {
        const ByteVector compressed = DeflateRaw(bytes);
        WrittenEntry result;
        result.Name = name;
        result.Crc = Crc32(bytes);
        result.CompressedSize = compressed.size();
        result.UncompressedSize = bytes.size();
        result.LocalOffset = StreamPosition(stream);
        result.Timestamp = CurrentDosTimestamp();

        const bool zip64Size = result.CompressedSize >= 0xFFFFFFFFULL
            || result.UncompressedSize >= 0xFFFFFFFFULL;
        ByteVector extra;
        if (zip64Size)
        {
            PushU16(extra, 0x0001U);
            PushU16(extra, 16U);
            PushU64(extra, result.UncompressedSize);
            PushU64(extra, result.CompressedSize);
        }

        ByteVector header;
        PushU32(header, 0x04034B50U);
        PushU16(header, zip64Size ? 45U : 20U);
        PushU16(header, 0x0800U);
        PushU16(header, 8U);
        PushU16(header, result.Timestamp.Time);
        PushU16(header, result.Timestamp.Date);
        PushU32(header, result.Crc);
        PushU32(header, zip64Size ? 0xFFFFFFFFU
            : static_cast<std::uint32_t>(result.CompressedSize));
        PushU32(header, zip64Size ? 0xFFFFFFFFU
            : static_cast<std::uint32_t>(result.UncompressedSize));
        if (name.size() > std::numeric_limits<std::uint16_t>::max()
            || extra.size() > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::length_error("ZIP entry name or extra field is too long.");
        }
        PushU16(header, static_cast<std::uint16_t>(name.size()));
        PushU16(header, static_cast<std::uint16_t>(extra.size()));

        WriteBytes(stream, header);
        WriteRaw(stream, name.data(), name.size());
        WriteBytes(stream, extra);
        WriteBytes(stream, compressed);
        return result;
    }

    void FinishZip(std::ofstream& stream, const std::vector<WrittenEntry>& entries)
    {
        const std::uint64_t centralOffset = StreamPosition(stream);
        for (const WrittenEntry& entry : entries)
        {
            const bool zip64Uncompressed = entry.UncompressedSize >= 0xFFFFFFFFULL;
            const bool zip64Compressed = entry.CompressedSize >= 0xFFFFFFFFULL;
            const bool zip64Offset = entry.LocalOffset >= 0xFFFFFFFFULL;
            ByteVector extra;
            if (zip64Uncompressed || zip64Compressed || zip64Offset)
            {
                ByteVector values;
                if (zip64Uncompressed)
                {
                    PushU64(values, entry.UncompressedSize);
                }
                if (zip64Compressed)
                {
                    PushU64(values, entry.CompressedSize);
                }
                if (zip64Offset)
                {
                    PushU64(values, entry.LocalOffset);
                }
                PushU16(extra, 0x0001U);
                PushU16(extra, static_cast<std::uint16_t>(values.size()));
                extra.insert(extra.end(), values.begin(), values.end());
            }

            const bool zip64 = zip64Uncompressed || zip64Compressed || zip64Offset;
            ByteVector header;
            PushU32(header, 0x02014B50U);
#if defined(_WIN32)
            PushU16(header, static_cast<std::uint16_t>(zip64 ? 45U : 20U));
#else
            PushU16(header, static_cast<std::uint16_t>(0x0300U | (zip64 ? 45U : 20U)));
#endif
            PushU16(header, zip64 ? 45U : 20U);
            PushU16(header, 0x0800U);
            PushU16(header, 8U);
            PushU16(header, entry.Timestamp.Time);
            PushU16(header, entry.Timestamp.Date);
            PushU32(header, entry.Crc);
            PushU32(header, zip64Compressed ? 0xFFFFFFFFU
                : static_cast<std::uint32_t>(entry.CompressedSize));
            PushU32(header, zip64Uncompressed ? 0xFFFFFFFFU
                : static_cast<std::uint32_t>(entry.UncompressedSize));
            PushU16(header, static_cast<std::uint16_t>(entry.Name.size()));
            PushU16(header, static_cast<std::uint16_t>(extra.size()));
            PushU16(header, 0U);
            PushU16(header, 0U);
            PushU16(header, 0U);
            PushU32(header, 0U);
            PushU32(header, zip64Offset ? 0xFFFFFFFFU
                : static_cast<std::uint32_t>(entry.LocalOffset));
            WriteBytes(stream, header);
            WriteRaw(stream, entry.Name.data(), entry.Name.size());
            WriteBytes(stream, extra);
        }

        const std::uint64_t centralEnd = StreamPosition(stream);
        const std::uint64_t centralSize = centralEnd - centralOffset;
        const bool zip64 = entries.size() >= 0xFFFFU
            || centralOffset >= 0xFFFFFFFFULL || centralSize >= 0xFFFFFFFFULL;
        if (zip64)
        {
            const std::uint64_t zip64Offset = StreamPosition(stream);
            ByteVector zip64End;
            PushU32(zip64End, 0x06064B50U);
            PushU64(zip64End, 44U);
#if defined(_WIN32)
            PushU16(zip64End, 45U);
#else
            PushU16(zip64End, static_cast<std::uint16_t>(0x0300U | 45U));
#endif
            PushU16(zip64End, 45U);
            PushU32(zip64End, 0U);
            PushU32(zip64End, 0U);
            PushU64(zip64End, entries.size());
            PushU64(zip64End, entries.size());
            PushU64(zip64End, centralSize);
            PushU64(zip64End, centralOffset);
            WriteBytes(stream, zip64End);

            ByteVector locator;
            PushU32(locator, 0x07064B50U);
            PushU32(locator, 0U);
            PushU64(locator, zip64Offset);
            PushU32(locator, 1U);
            WriteBytes(stream, locator);
        }

        ByteVector end;
        PushU32(end, 0x06054B50U);
        PushU16(end, 0U);
        PushU16(end, 0U);
        PushU16(end, zip64 ? 0xFFFFU : static_cast<std::uint16_t>(entries.size()));
        PushU16(end, zip64 ? 0xFFFFU : static_cast<std::uint16_t>(entries.size()));
        PushU32(end, zip64 ? 0xFFFFFFFFU : static_cast<std::uint32_t>(centralSize));
        PushU32(end, zip64 ? 0xFFFFFFFFU : static_cast<std::uint32_t>(centralOffset));
        PushU16(end, 0U);
        WriteBytes(stream, end);
    }

    void MoveOverwrite(const std::string& source, const std::string& destination)
    {
#if defined(_WIN32)
        const std::filesystem::path sourcePath = PathFromUtf8(source);
        const std::filesystem::path destinationPath = PathFromUtf8(destination);
        if (!MoveFileExW(
                sourcePath.c_str(), destinationPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw std::system_error(
                static_cast<int>(GetLastError()), std::system_category(),
                "Could not move temporary bundle");
        }
#else
        const std::filesystem::path sourcePath = PathFromUtf8(source);
        const std::filesystem::path destinationPath = PathFromUtf8(destination);
        if (std::rename(sourcePath.c_str(), destinationPath.c_str()) != 0)
        {
            throw std::system_error(
                errno, std::generic_category(), "Could not move temporary bundle");
        }
#endif
    }

    [[nodiscard]] std::int64_t FileLength(const std::string& path)
    {
        const std::uintmax_t size = std::filesystem::file_size(PathFromUtf8(path));
        if (size > static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max()))
        {
            throw std::overflow_error("File length is too large.");
        }
        return static_cast<std::int64_t>(size);
    }
}

namespace MphRead::Mods::MapGen
{
    bool MapBundle::Is(const std::string& path)
    {
        return OrdinalIgnoreCaseEquals(GetExtension(path), Extension);
    }

    std::string MapBundle::Cook(
        MapDefinition* definition,
        const std::string& recipePath,
        const std::optional<std::string>& outputPath,
        bool verbose)
    {
        if (definition == nullptr)
        {
            throw std::runtime_error("Object reference not set to an instance of an object.");
        }

        MapImport* import = definition->Import();
        if (import == nullptr || import->Source().empty())
        {
            throw ProgramException(
                definition->Name()
                + " builds from its own description; there is no level to bundle.");
        }

        std::optional<std::string> level = import->Resolve();
        if (!level)
        {
            throw ProgramException(
                definition->Name() + ": its source level " + import->Source()
                + " is not here, so there is nothing to cook.");
        }

        const std::string mapName = import->MapName().value_or(
            GetFileNameWithoutExtension(*level));
        const ByteVector trimmed = Q3Bsp::Trim(
            Q3Bsp::ReadLevel(*level, import->MapName()));

        std::optional<std::string> texturePath = import->ResolveTextures();
        if (!texturePath && import->Textures() && !import->Textures()->empty())
        {
            texturePath = Q3Import::BakeTextures(
                Q3Bsp::Load(*level, import->MapName()), import, verbose);
            if (!texturePath)
            {
                throw ProgramException(
                    definition->Name() + ": its textures (" + *import->Textures()
                    + ") are not beside its recipe and could not be baked from "
                    + GetFileName(*level)
                    + ". A bundle without them is a room with no materials, "
                      "so this is a failure and not a bundle.");
            }
        }

        const std::string path = outputPath.value_or(
            CombinePath(
                CustomRooms::MapDirectory(),
                GetFileNameWithoutExtension(recipePath) + Extension));
        const std::string recipeName = GetFileName(recipePath);
        const std::string textureName = texturePath
            ? GetFileName(*texturePath) : std::string{};

        std::shared_ptr<MapDefinition> inside = MapDefinition::Load(recipePath);
        if (MapImport* insideImport = inside->Import(); insideImport != nullptr)
        {
            insideImport->Source(std::string(LevelDirectory) + mapName + ".bsp");
            insideImport->MapName(mapName);
            insideImport->Textures(textureName);
        }

        const std::string temporary = path + ".tmp";
        {
            std::ofstream file(
                PathFromUtf8(temporary),
                std::ios::binary | std::ios::out | std::ios::trunc);
            if (!file)
            {
                throw std::runtime_error("Could not create file: " + temporary);
            }

            std::vector<WrittenEntry> entries;
            entries.reserve(texturePath ? 3U : 2U);

            const std::string serialized = inside->Serialize();
            entries.push_back(WriteZipEntry(
                file, recipeName,
                ByteVector(serialized.begin(), serialized.end())));
            entries.push_back(WriteZipEntry(
                file, std::string(LevelDirectory) + mapName + ".bsp", trimmed));
            if (texturePath)
            {
                entries.push_back(WriteZipEntry(
                    file, textureName, ReadAllBytes(*texturePath)));
            }
            FinishZip(file, entries);
            file.close();
            if (!file)
            {
                throw std::runtime_error("Could not close file: " + temporary);
            }
        }

        MoveOverwrite(temporary, path);

        if (verbose)
        {
            const std::int64_t before = FileLength(*level)
                + (texturePath ? FileLength(*texturePath) : 0);
            std::cout << "[mapbundle] " << definition->Name() << " -> " << path
                << " (" << FileLength(path) / 1024 << " KiB, from "
                << before / 1024 << " KiB)" << std::endl;
        }
        return path;
    }

    std::optional<std::string> MapBundle::ReadRecipe(const std::string& bundlePath)
    {
        const ByteVector archive = ReadAllBytes(bundlePath);
        const std::vector<ZipEntry> entries = ReadZipEntries(archive);
        const auto found = std::find_if(entries.begin(), entries.end(),
            [](const ZipEntry& entry)
            {
                return OrdinalIgnoreCaseEndsWith(entry.Name, ".json");
            });
        if (found == entries.end())
        {
            return std::nullopt;
        }
        return DecodeStreamReaderText(ReadZipEntry(archive, *found));
    }

    std::optional<std::vector<std::uint8_t>> MapBundle::ReadEntry(
        const std::string& bundlePath, const std::string& name)
    {
        if (name.empty())
        {
            return std::nullopt;
        }

        const ByteVector archive = ReadAllBytes(bundlePath);
        const std::vector<ZipEntry> entries = ReadZipEntries(archive);
        auto found = std::find_if(entries.begin(), entries.end(),
            [&](const ZipEntry& entry)
            {
                return OrdinalIgnoreCaseEquals(entry.Name, name);
            });
        if (found == entries.end())
        {
            const std::string suffix = "/" + name;
            found = std::find_if(entries.begin(), entries.end(),
                [&](const ZipEntry& entry)
                {
                    return OrdinalIgnoreCaseEndsWith(entry.Name, suffix);
                });
        }
        if (found == entries.end())
        {
            return std::nullopt;
        }
        return ReadZipEntry(archive, *found);
    }
}
