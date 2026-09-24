#include "MapBundle.hpp"

#include "MapDefinition.hpp"
#include "Q3Bsp.hpp"
#include "Q3Import.hpp"
#include "../../Formats/Types.hpp"
#include "../../Program.hpp"
#include "../../NativeRuntime/System/IO.hpp"

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
#include <cwctype>
#include <exception>
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
#include <locale.h>
#include <sys/types.h>
#endif

using ::MphRead::NativeRuntime::PathCombine;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathToUtf8;

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

    [[nodiscard]] std::vector<std::uint32_t> DecodeUtf8(const std::string& value)
    {
        std::vector<std::uint32_t> result;
        result.reserve(value.size());
        std::size_t position = 0;
        while (position < value.size())
        {
            std::uint32_t scalar = 0;
            (void)DecodeUtf8Scalar(
                reinterpret_cast<const std::uint8_t*>(value.data()), value.size(), position, scalar);
            result.push_back(scalar);
        }
        return result;
    }

#if !defined(_WIN32)
    [[nodiscard]] void* FindVersionedIcuSymbol(void* library, const char* base) noexcept
    {
        if (library == nullptr)
        {
            return nullptr;
        }
        if (void* symbol = dlsym(library, base); symbol != nullptr)
        {
            return symbol;
        }
        char name[96]{};
        for (int version = 99; version >= 50; --version)
        {
            const int count = std::snprintf(name, sizeof(name), "%s_%d", base, version);
            if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(name))
            {
                continue;
            }
            if (void* symbol = dlsym(library, name); symbol != nullptr)
            {
                return symbol;
            }
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
            if (library == nullptr)
            {
                library = dlopen("/usr/lib/libicucore.A.dylib", RTLD_LAZY | RTLD_LOCAL);
            }
#endif
            return reinterpret_cast<UpperFunction>(FindVersionedIcuSymbol(library, "u_toupper"));
        }();
        if (upper == nullptr || scalar > 0x10FFFFU)
        {
            return scalar;
        }
        const std::int32_t mapped = upper(static_cast<std::int32_t>(scalar));
        return mapped < 0 ? scalar : static_cast<std::uint32_t>(mapped);
    }
#endif

    [[nodiscard]] std::uint32_t InvariantUpper(std::uint32_t scalar) noexcept
    {
        // .NET OrdinalIgnoreCase deliberately keeps these two code points
        // distinct from their ordinary Latin uppercase counterparts.
        if (scalar == 0x0131U || scalar == 0x017FU)
        {
            return scalar;
        }
        if (scalar >= 'a' && scalar <= 'z')
        {
            return scalar - static_cast<std::uint32_t>('a' - 'A');
        }
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
        if (icuMapped != scalar)
        {
            return icuMapped;
        }
        static locale_t locale = []() noexcept
        {
            locale_t value = newlocale(LC_CTYPE_MASK, "C.UTF-8", nullptr);
            if (value == nullptr)
            {
                value = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
            }
            return value;
        }();
        if (locale != nullptr && scalar <= static_cast<std::uint32_t>(WCHAR_MAX))
        {
            const wint_t mapped = towupper_l(static_cast<wint_t>(scalar), locale);
            if (mapped != WEOF)
            {
                return static_cast<std::uint32_t>(mapped);
            }
        }
#endif
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
        std::vector<std::uint32_t> result = DecodeUtf8(value);
        for (std::uint32_t& scalar : result)
        {
            scalar = InvariantUpper(scalar);
        }
        return result;
    }

    [[nodiscard]] bool OrdinalIgnoreCaseEquals(
        const std::string& left, const std::string& right) noexcept
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

    [[nodiscard]] bool OrdinalIgnoreCaseEndsWith(
        const std::string& value, const std::string& suffix) noexcept
    {
        try
        {
            const std::vector<std::uint32_t> foldedValue = FoldOrdinalIgnoreCase(value);
            const std::vector<std::uint32_t> foldedSuffix = FoldOrdinalIgnoreCase(suffix);
            if (foldedSuffix.size() > foldedValue.size())
            {
                return false;
            }
            return std::equal(
                foldedSuffix.begin(), foldedSuffix.end(),
                foldedValue.end() - static_cast<std::ptrdiff_t>(foldedSuffix.size()));
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] std::string DecodeZipName(
        const std::uint8_t* data, std::size_t size)
    {
        std::string result;
        std::size_t position = 0;
        while (position < size)
        {
            std::uint32_t scalar = 0;
            (void)DecodeUtf8Scalar(data, size, position, scalar);
            AppendUtf8(result, scalar);
        }
        return result;
    }

    struct EncodedZipName final
    {
        std::string Bytes;
        bool Utf8 = false;
    };

    [[nodiscard]] EncodedZipName EncodeZipName(const std::string& value)
    {
        if (value.empty())
        {
            throw std::invalid_argument("The entry name cannot be empty.");
        }

        EncodedZipName result;
        result.Bytes.reserve(value.size());
        for (std::uint32_t scalar : DecodeUtf8(value))
        {
            if (scalar < 0x20U || scalar > 0x7EU)
            {
                result.Utf8 = true;
            }
            AppendUtf8(result.Bytes, scalar);
        }
        if (result.Bytes.size() > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::invalid_argument("Entry names cannot require more than 65535 bytes.");
        }
        return result;
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

    [[nodiscard]] ByteVector ReadAllBytes(
        std::ifstream& stream, const std::string& path)
    {
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

    [[nodiscard]] ByteVector ReadAllBytes(const std::string& path)
    {
        std::ifstream stream(PathFromUtf8(path), std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Could not open file: " + path);
        }
        return ReadAllBytes(stream, path);
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
        const ByteVector& input, std::size_t outputLimit)
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
        std::array<std::uint8_t, 65536> buffer{};
        std::size_t inputOffset = 0;
        try
        {
            int result = ZOk;
            while (result != ZStreamEnd && output.size() < outputLimit)
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

                const std::size_t remaining = outputLimit - output.size();
                const std::size_t capacity = std::min<std::size_t>(buffer.size(), remaining);
                stream.next_out = buffer.data();
                stream.avail_out = static_cast<ZUInt>(capacity);
                result = zlib.Inflate(&stream, ZNoFlush);
                if (result != ZOk && result != ZStreamEnd)
                {
                    throw std::runtime_error("DEFLATE decompression failed.");
                }
                const std::size_t produced = capacity - stream.avail_out;
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

    struct Deflate64HuffmanNode final
    {
        std::int32_t Child[2] = {-1, -1};
        std::int32_t Symbol = -1;
    };

    class Deflate64BitReader final
    {
    public:
        explicit Deflate64BitReader(const ByteVector& bytes) noexcept : _bytes(bytes) {}

        [[nodiscard]] std::uint32_t ReadBits(std::int32_t count)
        {
            std::uint32_t value = 0;
            for (std::int32_t i = 0; i < count; ++i)
            {
                if (_bitPosition >= _bytes.size() * 8ULL)
                {
                    throw std::runtime_error("Invalid deflate stream.");
                }
                const std::size_t byteIndex = _bitPosition >> 3;
                const std::size_t bitIndex = _bitPosition & 7U;
                value |= static_cast<std::uint32_t>(
                    (_bytes[byteIndex] >> bitIndex) & 1U) << i;
                ++_bitPosition;
            }
            return value;
        }

        void AlignByte() noexcept
        {
            _bitPosition = (_bitPosition + 7U) & ~std::size_t(7U);
        }

        [[nodiscard]] std::size_t BytePosition() const noexcept
        {
            return _bitPosition >> 3;
        }

        void BytePosition(std::size_t value) noexcept
        {
            _bitPosition = value << 3;
        }

    private:
        const ByteVector& _bytes;
        std::size_t _bitPosition = 0;
    };

    [[nodiscard]] std::vector<Deflate64HuffmanNode> BuildDeflate64Huffman(
        const std::vector<std::uint8_t>& lengths)
    {
        constexpr std::int32_t MaxBits = 15;
        std::array<std::int32_t, MaxBits + 1> counts{};
        for (std::uint8_t length : lengths)
        {
            if (length > MaxBits)
            {
                throw std::runtime_error("Invalid deflate Huffman code length.");
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

        std::vector<Deflate64HuffmanNode> nodes(1);
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
                if (nodes[node].Child[bit] < 0)
                {
                    nodes[node].Child[bit] = static_cast<std::int32_t>(nodes.size());
                    nodes.emplace_back();
                }
                node = nodes[node].Child[bit];
            }
            if (nodes[node].Symbol >= 0)
            {
                throw std::runtime_error("Invalid deflate Huffman tree.");
            }
            nodes[node].Symbol = static_cast<std::int32_t>(symbol);
        }
        return nodes;
    }

    [[nodiscard]] std::int32_t DecodeDeflate64Symbol(
        Deflate64BitReader& reader,
        const std::vector<Deflate64HuffmanNode>& tree)
    {
        std::int32_t node = 0;
        for (std::int32_t depth = 0; depth <= 15; ++depth)
        {
            if (tree[node].Symbol >= 0)
            {
                return tree[node].Symbol;
            }
            const std::int32_t bit = static_cast<std::int32_t>(reader.ReadBits(1));
            node = tree[node].Child[bit];
            if (node < 0)
            {
                throw std::runtime_error("Invalid deflate Huffman code.");
            }
        }
        throw std::runtime_error("Invalid deflate Huffman code.");
    }

    [[nodiscard]] bool InflateDeflate64Codes(
        Deflate64BitReader& reader,
        ByteVector& output,
        const std::vector<Deflate64HuffmanNode>& literalTree,
        const std::vector<Deflate64HuffmanNode>& distanceTree,
        std::size_t outputLimit)
    {
        static constexpr std::array<std::int32_t, 29> LengthBase{
            3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,
            131,163,195,227,258};
        static constexpr std::array<std::int32_t, 29> LengthExtra{
            0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
        static constexpr std::array<std::int32_t, 32> DistanceBase{
            1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
            1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,32769,49153};
        static constexpr std::array<std::int32_t, 32> DistanceExtra{
            0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,
            13,13,14,14};

        while (true)
        {
            const std::int32_t symbol = DecodeDeflate64Symbol(reader, literalTree);
            if (symbol < 256)
            {
                output.push_back(static_cast<std::uint8_t>(symbol));
                if (output.size() >= outputLimit)
                {
                    return true;
                }
                continue;
            }
            if (symbol == 256)
            {
                return false;
            }
            if (symbol < 257 || symbol > 285)
            {
                throw std::runtime_error("Invalid deflate length code.");
            }

            const std::size_t lengthIndex = static_cast<std::size_t>(symbol - 257);
            std::int32_t length = LengthBase[lengthIndex];
            std::int32_t lengthExtra = LengthExtra[lengthIndex];
            if (symbol == 285)
            {
                length = 3;
                lengthExtra = 16;
            }
            if (lengthExtra != 0)
            {
                length += static_cast<std::int32_t>(reader.ReadBits(lengthExtra));
            }

            const std::int32_t distanceSymbol = DecodeDeflate64Symbol(reader, distanceTree);
            if (distanceSymbol < 0 || distanceSymbol >= 32)
            {
                throw std::runtime_error("Invalid deflate distance code.");
            }
            std::int32_t distance = DistanceBase[distanceSymbol];
            if (DistanceExtra[distanceSymbol] != 0)
            {
                distance += static_cast<std::int32_t>(
                    reader.ReadBits(DistanceExtra[distanceSymbol]));
            }
            if (distance <= 0 || static_cast<std::size_t>(distance) > output.size())
            {
                throw std::runtime_error("Invalid deflate distance.");
            }
            for (std::int32_t i = 0; i < length; ++i)
            {
                output.push_back(output[output.size() - static_cast<std::size_t>(distance)]);
                if (output.size() >= outputLimit)
                {
                    return true;
                }
            }
        }
    }

    [[nodiscard]] ByteVector InflateDeflate64Raw(
        const ByteVector& input, std::size_t outputLimit)
    {
        ByteVector output;
        if (outputLimit == 0)
        {
            return output;
        }

        Deflate64BitReader reader(input);
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
                    throw std::runtime_error("Invalid stored deflate block.");
                }
                const std::uint16_t length = ReadU16(input, position);
                const std::uint16_t complement = ReadU16(input, position + 2);
                position += 4;
                if (static_cast<std::uint16_t>(~length) != complement)
                {
                    throw std::runtime_error("Invalid stored deflate block.");
                }
                const std::size_t remaining = outputLimit - output.size();
                const std::size_t copyCount = std::min<std::size_t>(length, remaining);
                if (position > input.size() || copyCount > input.size() - position)
                {
                    throw std::runtime_error("Invalid stored deflate block.");
                }
                output.insert(output.end(),
                    input.begin() + static_cast<std::ptrdiff_t>(position),
                    input.begin() + static_cast<std::ptrdiff_t>(position + copyCount));
                if (copyCount < length || output.size() >= outputLimit)
                {
                    return output;
                }
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
                if (InflateDeflate64Codes(
                        reader, output,
                        BuildDeflate64Huffman(literalLengths),
                        BuildDeflate64Huffman(distanceLengths), outputLimit))
                {
                    return output;
                }
            }
            else if (type == 2)
            {
                const std::int32_t literalCount
                    = static_cast<std::int32_t>(reader.ReadBits(5)) + 257;
                const std::int32_t distanceCount
                    = static_cast<std::int32_t>(reader.ReadBits(5)) + 1;
                const std::int32_t codeCount
                    = static_cast<std::int32_t>(reader.ReadBits(4)) + 4;
                static constexpr std::array<std::int32_t, 19> Order{
                    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                std::vector<std::uint8_t> codeLengths(19, 0);
                for (std::int32_t i = 0; i < codeCount; ++i)
                {
                    codeLengths[Order[i]] = static_cast<std::uint8_t>(reader.ReadBits(3));
                }
                const std::vector<Deflate64HuffmanNode> codeTree
                    = BuildDeflate64Huffman(codeLengths);
                std::vector<std::uint8_t> lengths;
                lengths.reserve(static_cast<std::size_t>(literalCount + distanceCount));
                while (static_cast<std::int32_t>(lengths.size())
                    < literalCount + distanceCount)
                {
                    const std::int32_t symbol = DecodeDeflate64Symbol(reader, codeTree);
                    if (symbol <= 15)
                    {
                        lengths.push_back(static_cast<std::uint8_t>(symbol));
                    }
                    else if (symbol == 16)
                    {
                        if (lengths.empty())
                        {
                            throw std::runtime_error("Invalid deflate repeat code.");
                        }
                        const std::int32_t repeat
                            = static_cast<std::int32_t>(reader.ReadBits(2)) + 3;
                        const std::uint8_t value = lengths.back();
                        for (std::int32_t i = 0; i < repeat; ++i)
                        {
                            lengths.push_back(value);
                        }
                    }
                    else if (symbol == 17)
                    {
                        const std::int32_t repeat
                            = static_cast<std::int32_t>(reader.ReadBits(3)) + 3;
                        for (std::int32_t i = 0; i < repeat; ++i)
                        {
                            lengths.push_back(0);
                        }
                    }
                    else if (symbol == 18)
                    {
                        const std::int32_t repeat
                            = static_cast<std::int32_t>(reader.ReadBits(7)) + 11;
                        for (std::int32_t i = 0; i < repeat; ++i)
                        {
                            lengths.push_back(0);
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Invalid deflate code-length symbol.");
                    }
                    if (static_cast<std::int32_t>(lengths.size())
                        > literalCount + distanceCount)
                    {
                        throw std::runtime_error("Invalid deflate code lengths.");
                    }
                }
                std::vector<std::uint8_t> literalLengths(
                    lengths.begin(), lengths.begin() + literalCount);
                std::vector<std::uint8_t> distanceLengths(
                    lengths.begin() + literalCount, lengths.end());
                if (InflateDeflate64Codes(
                        reader, output,
                        BuildDeflate64Huffman(literalLengths),
                        BuildDeflate64Huffman(distanceLengths), outputLimit))
                {
                    return output;
                }
            }
            else
            {
                throw std::runtime_error("Invalid deflate block type.");
            }
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
        std::uint32_t DiskNumberStart = 0;
        std::uint32_t ArchiveDiskNumber = 0;
    };

    [[nodiscard]] std::uint64_t ReadZip64SignedValue(
        const ByteVector& field, std::size_t& position)
    {
        if (position > field.size() || field.size() - position < 8)
        {
            throw std::runtime_error("Invalid ZIP64 extra field.");
        }
        const std::uint64_t value = ReadU64(field, position);
        position += 8;
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            throw std::runtime_error("ZIP64 field is too large.");
        }
        return value;
    }

    void ApplyZip64Extra(
        ZipEntry& entry, const ByteVector& extra,
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
                entry.UncompressedSize = ReadZip64SignedValue(field, position);
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
                entry.CompressedSize = ReadZip64SignedValue(field, position);
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
                entry.LocalOffset = ReadZip64SignedValue(field, position);
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
                entry.DiskNumberStart = ReadU32(field, position);
            }
            return;
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

        const std::uint16_t eocdDisk = ReadU16(archive, eocd + 4);
        const std::uint16_t eocdCentralDisk = ReadU16(archive, eocd + 6);
        if (eocdDisk != eocdCentralDisk)
        {
            throw std::runtime_error("Split or spanned ZIP archives are not supported.");
        }
        const std::uint16_t entriesOnDisk = ReadU16(archive, eocd + 8);
        const std::uint16_t totalEntries = ReadU16(archive, eocd + 10);
        if (entriesOnDisk != totalEntries)
        {
            throw std::runtime_error("Split or spanned ZIP archives are not supported.");
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
                throw std::runtime_error("ZIP64 End of Central Directory offset is too large.");
            }
            if (zip64Offset > archive.size()
                || archive.size() - static_cast<std::size_t>(zip64Offset) < 56
                || ReadU32(archive, static_cast<std::size_t>(zip64Offset)) != 0x06064B50U)
            {
                throw std::runtime_error("ZIP64 End of Central Directory record is invalid.");
            }

            const std::size_t offset = static_cast<std::size_t>(zip64Offset);
            archiveDiskNumber = ReadU32(archive, offset + 16);
            const std::uint64_t zip64EntriesOnDisk = ReadU64(archive, offset + 24);
            const std::uint64_t zip64EntryCount = ReadU64(archive, offset + 32);
            const std::uint64_t zip64CentralOffset = ReadU64(archive, offset + 48);
            if (zip64EntryCount > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
            {
                throw std::runtime_error("ZIP64 entry count is too large.");
            }
            if (zip64CentralOffset > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
            {
                throw std::runtime_error("ZIP64 Central Directory offset is too large.");
            }
            if (zip64EntryCount != zip64EntriesOnDisk)
            {
                throw std::runtime_error("Split or spanned ZIP archives are not supported.");
            }
            entryCount = zip64EntryCount;
            centralOffset = zip64CentralOffset;
        }

        if (centralOffset > archive.size())
        {
            throw std::runtime_error("Central Directory corrupt.");
        }
        if (entryCount > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            || entryCount > static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max()))
        {
            throw std::length_error("Too many ZIP entries.");
        }

        std::vector<ZipEntry> entries;
        std::size_t cursor = static_cast<std::size_t>(centralOffset);
        while (cursor + 4 <= archive.size()
            && ReadU32(archive, cursor) == 0x02014B50U)
        {
            if (archive.size() - cursor < 46)
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
            const std::uint16_t diskNumberStart16 = ReadU16(archive, cursor + 34);
            const std::uint32_t offset32 = ReadU32(archive, cursor + 42);
            const std::size_t variable = static_cast<std::size_t>(nameLength)
                + static_cast<std::size_t>(extraLength)
                + static_cast<std::size_t>(commentLength);
            if (46 + variable > archive.size() - cursor)
            {
                throw std::runtime_error("Central Directory corrupt.");
            }

            entry.Name = DecodeZipName(archive.data() + cursor + 46, nameLength);
            entry.CompressedSize = compressed32;
            entry.UncompressedSize = uncompressed32;
            entry.LocalOffset = offset32;
            entry.DiskNumberStart = diskNumberStart16;
            entry.ArchiveDiskNumber = archiveDiskNumber;

            ByteVector extra(
                archive.begin() + static_cast<std::ptrdiff_t>(cursor + 46 + nameLength),
                archive.begin() + static_cast<std::ptrdiff_t>(
                    cursor + 46 + nameLength + extraLength));
            ApplyZip64Extra(
                entry, extra,
                uncompressed32 == 0xFFFFFFFFU,
                compressed32 == 0xFFFFFFFFU,
                offset32 == 0xFFFFFFFFU,
                diskNumberStart16 == 0xFFFFU);
            entries.push_back(std::move(entry));
            cursor += 46 + variable;
        }

        if (entries.size() != static_cast<std::size_t>(entryCount))
        {
            throw std::runtime_error("Central Directory entry count is incorrect.");
        }
        return entries;
    }

    [[nodiscard]] ByteVector ReadZipEntry(
        const ByteVector& archive, const ZipEntry& entry)
    {
        if (entry.Method != 0 && entry.Method != 8 && entry.Method != 9)
        {
            throw std::runtime_error("The ZIP entry uses an unsupported compression method.");
        }
        if (entry.DiskNumberStart != entry.ArchiveDiskNumber)
        {
            throw std::runtime_error("Split or spanned ZIP archives are not supported.");
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
        if (entry.Method == 9)
        {
            return InflateDeflate64Raw(
                compressed, static_cast<std::size_t>(entry.UncompressedSize));
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

        const int year = local.tm_year + 1900;
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
        std::uint16_t Flags = 0;
        std::uint16_t Method = 8;
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
        // ZipArchive.CreateEntry first rejects an empty name, then the entry
        // constructor captures DateTimeOffset.Now before FullName encodes and
        // validates the complete entry name.
        if (name.empty())
        {
            throw std::invalid_argument("The entry name cannot be empty.");
        }
        const DosTimestamp timestamp = CurrentDosTimestamp();
        EncodedZipName encodedName = EncodeZipName(name);

        WrittenEntry result;
        result.Name = std::move(encodedName.Bytes);
        // CompressionLevel.SmallestSize maps to deflate-option value 2.
        // .NET retains those bits even when an empty entry is switched to Stored.
        result.Flags = static_cast<std::uint16_t>(
            0x0002U | (encodedName.Utf8 ? 0x0800U : 0U));
        result.Method = bytes.empty() ? 0U : 8U;
        result.Timestamp = timestamp;

        const ByteVector compressed = bytes.empty() ? ByteVector{} : DeflateRaw(bytes);
        result.Crc = Crc32(bytes);
        result.CompressedSize = compressed.size();
        result.UncompressedSize = bytes.size();
        result.LocalOffset = StreamPosition(stream);

        // For a seekable create-mode ZipArchive the local header is emitted on
        // the first non-empty write, before the final sizes are known. If either
        // final size later exceeds uint.MaxValue, .NET cannot grow that header
        // to add a ZIP64 extra field: it upgrades the version, sets the data-
        // descriptor flag, leaves local CRC/sizes at zero, then appends a
        // 64-bit descriptor after the compressed payload.
        const bool zip64Size = result.CompressedSize > 0xFFFFFFFFULL
            || result.UncompressedSize > 0xFFFFFFFFULL;
        if (zip64Size)
        {
            result.Flags = static_cast<std::uint16_t>(result.Flags | 0x0008U);
        }

        ByteVector header;
        PushU32(header, 0x04034B50U);
        PushU16(header, zip64Size ? 45U : 20U);
        PushU16(header, result.Flags);
        PushU16(header, result.Method);
        PushU16(header, result.Timestamp.Time);
        PushU16(header, result.Timestamp.Date);
        PushU32(header, zip64Size ? 0U : result.Crc);
        PushU32(header, zip64Size ? 0U
            : static_cast<std::uint32_t>(result.CompressedSize));
        PushU32(header, zip64Size ? 0U
            : static_cast<std::uint32_t>(result.UncompressedSize));
        PushU16(header, static_cast<std::uint16_t>(result.Name.size()));
        PushU16(header, 0U);

        WriteBytes(stream, header);
        WriteRaw(stream, result.Name.data(), result.Name.size());
        WriteBytes(stream, compressed);
        if (zip64Size)
        {
            // WriteCrcAndSizesInLocalHeader's "pretend streaming" path emits
            // this descriptor without the optional signature.
            ByteVector descriptor;
            PushU32(descriptor, result.Crc);
            PushU64(descriptor, result.CompressedSize);
            PushU64(descriptor, result.UncompressedSize);
            WriteBytes(stream, descriptor);
        }
        return result;
    }

    void FinishZip(std::ofstream& stream, const std::vector<WrittenEntry>& entries)
    {
        const std::uint64_t centralOffset = StreamPosition(stream);
        for (const WrittenEntry& entry : entries)
        {
            const bool zip64Sizes = entry.UncompressedSize > 0xFFFFFFFFULL
                || entry.CompressedSize > 0xFFFFFFFFULL;
            const bool zip64Offset = entry.LocalOffset > 0xFFFFFFFFULL;
            ByteVector extra;
            if (zip64Sizes || zip64Offset)
            {
                ByteVector values;
                // ZipArchiveEntry writes both size values when either one no
                // longer fits in the 32-bit central-directory fields.
                if (zip64Sizes)
                {
                    PushU64(values, entry.UncompressedSize);
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

            const bool zip64 = zip64Sizes || zip64Offset;
            ByteVector header;
            PushU32(header, 0x02014B50U);
#if defined(_WIN32)
            PushU16(header, static_cast<std::uint16_t>(zip64 ? 45U : 20U));
#else
            PushU16(header, static_cast<std::uint16_t>(0x0300U | (zip64 ? 45U : 20U)));
#endif
            PushU16(header, zip64 ? 45U : 20U);
            PushU16(header, entry.Flags);
            PushU16(header, entry.Method);
            PushU16(header, entry.Timestamp.Time);
            PushU16(header, entry.Timestamp.Date);
            PushU32(header, entry.Crc);
            PushU32(header, zip64Sizes ? 0xFFFFFFFFU
                : static_cast<std::uint32_t>(entry.CompressedSize));
            PushU32(header, zip64Sizes ? 0xFFFFFFFFU
                : static_cast<std::uint32_t>(entry.UncompressedSize));
            PushU16(header, static_cast<std::uint16_t>(entry.Name.size()));
            PushU16(header, static_cast<std::uint16_t>(extra.size()));
            PushU16(header, 0U);
            PushU16(header, 0U);
            PushU16(header, 0U);
#if defined(_WIN32)
            PushU32(header, 0U);
#else
            // ZipArchiveEntryConstants.Unix.DefaultFileExternalAttributes:
            // regular file type plus 0644, shifted into the upper 16 bits.
            PushU32(header, 0x81A40000U);
#endif
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
            // Zip64EndOfCentralDirectoryRecord.WriteBlock writes the
            // version-made-by field as MS-DOS/45 on every platform.
            PushU16(zip64End, 45U);
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
        const std::uint16_t entryCount16 = entries.size() > 0xFFFFU
            ? 0xFFFFU : static_cast<std::uint16_t>(entries.size());
        const std::uint32_t centralSize32 = centralSize > 0xFFFFFFFFULL
            ? 0xFFFFFFFFU : static_cast<std::uint32_t>(centralSize);
        const std::uint32_t centralOffset32 = centralOffset > 0xFFFFFFFFULL
            ? 0xFFFFFFFFU : static_cast<std::uint32_t>(centralOffset);
        PushU16(end, entryCount16);
        PushU16(end, entryCount16);
        PushU32(end, centralSize32);
        PushU32(end, centralOffset32);
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
                MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
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
            const int error = errno;
            if (error != EXDEV)
            {
                throw std::system_error(
                    error, std::generic_category(), "Could not move temporary bundle");
            }

            // File.Move(..., overwrite: true) on Unix falls back to copy+delete
            // when rename crosses a device/mount boundary.
            std::error_code copyError;
            std::filesystem::copy_file(
                sourcePath, destinationPath,
                std::filesystem::copy_options::overwrite_existing, copyError);
            if (copyError)
            {
                throw std::system_error(
                    copyError, "Could not copy temporary bundle across devices");
            }

            std::error_code deleteError;
            (void)std::filesystem::remove(sourcePath, deleteError);
            if (deleteError)
            {
                throw std::system_error(
                    deleteError, "Could not delete temporary bundle after cross-device copy");
            }
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

    [[nodiscard]] std::int64_t WrapAddInt64(
        std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(
            std::bit_cast<std::uint64_t>(left) + std::bit_cast<std::uint64_t>(right));
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
            throw System::NullReferenceException();
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

        const std::optional<std::string>& requestedMapName = import->MapName();
        const std::string mapName = requestedMapName
            ? *requestedMapName
            : GetFileNameWithoutExtension(*level);
        const ByteVector trimmed = Q3Bsp::Trim(
            Q3Bsp::ReadLevel(*level, import->MapName()));

        std::optional<std::string> texturePath = import->ResolveTextures();
        const std::optional<std::string>& requestedTextures = import->Textures();
        if (!texturePath && requestedTextures && !requestedTextures->empty())
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

        const std::string path = outputPath
            ? *outputPath
            : PathCombine(
                CustomRooms::MapDirectory(),
                GetFileNameWithoutExtension(recipePath) + Extension);
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
            bool finalizationStarted = false;
            try
            {
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

                finalizationStarted = true;
                FinishZip(file, entries);
            }
            catch (...)
            {
                // The C# using scope disposes ZipArchive before FileStream even
                // while propagating an exception. If archive finalization has
                // not already started, finish the successfully-created entries.
                // A finalization failure replaces the prior failure; the outer
                // FileStream disposal still runs, and its failure replaces
                // either one.
                std::exception_ptr pending = std::current_exception();
                if (!finalizationStarted)
                {
                    finalizationStarted = true;
                    try
                    {
                        FinishZip(file, entries);
                    }
                    catch (...)
                    {
                        pending = std::current_exception();
                    }
                }
                file.close();
                if (!file)
                {
                    throw std::runtime_error("Could not close file: " + temporary);
                }
                std::rethrow_exception(pending);
            }

            file.close();
            if (!file)
            {
                throw std::runtime_error("Could not close file: " + temporary);
            }
        }

        MoveOverwrite(temporary, path);

        if (verbose)
        {
            const std::int64_t levelLength = FileLength(*level);
            const std::int64_t textureLength
                = texturePath ? FileLength(*texturePath) : 0;
            const std::int64_t before = WrapAddInt64(levelLength, textureLength);
            std::cout << "[mapbundle] " << definition->Name() << " -> " << path
                << " (" << FileLength(path) / 1024 << " KiB, from "
                << before / 1024 << " KiB)" << std::endl;
        }
        return path;
    }

    std::optional<std::string> MapBundle::ReadRecipe(const std::string& bundlePath)
    {
        std::ifstream file(PathFromUtf8(bundlePath), std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Could not open file: " + bundlePath);
        }
        const ByteVector archive = ReadAllBytes(file, bundlePath);
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

        std::ifstream file(PathFromUtf8(bundlePath), std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Could not open file: " + bundlePath);
        }
        const ByteVector archive = ReadAllBytes(file, bundlePath);
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
