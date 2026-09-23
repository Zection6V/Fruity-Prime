#include "ZipArchive.hpp"

#include "Exceptions.hpp"
#include "IO.hpp"

#include <cstring>

#include <zlib.h>

namespace MphRead::NativeRuntime
{
    namespace
    {
        [[nodiscard]] std::uint16_t ReadU16(const std::uint8_t* data) noexcept
        {
            return static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(data[0])
                | (static_cast<std::uint16_t>(data[1]) << 8));
        }

        [[nodiscard]] std::uint32_t ReadU32(const std::uint8_t* data) noexcept
        {
            return static_cast<std::uint32_t>(data[0])
                | (static_cast<std::uint32_t>(data[1]) << 8)
                | (static_cast<std::uint32_t>(data[2]) << 16)
                | (static_cast<std::uint32_t>(data[3]) << 24);
        }

        constexpr std::uint32_t EndOfCentralDirectory = 0x06054B50U;
        constexpr std::uint32_t CentralFileHeader = 0x02014B50U;
    }

    std::shared_ptr<ZipArchive> ZipArchive::OpenRead(const std::string& path)
    {
        std::vector<std::uint8_t> bytes = FileReadAllBytes(path);
        if (bytes.size() < 22)
        {
            return nullptr;
        }
        // The end record is last, after a comment of up to 64 KB.
        std::size_t end = bytes.size() - 22;
        while (true)
        {
            if (ReadU32(bytes.data() + end) == EndOfCentralDirectory)
            {
                break;
            }
            if (end == 0 || bytes.size() - end > 0xFFFFU + 22U)
            {
                return nullptr;
            }
            --end;
        }
        const std::uint16_t count = ReadU16(bytes.data() + end + 10);
        std::uint32_t offset = ReadU32(bytes.data() + end + 16);

        auto archive = std::make_shared<ZipArchive>();
        archive->_entries.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i)
        {
            if (static_cast<std::size_t>(offset) + 46 > bytes.size()
                || ReadU32(bytes.data() + offset) != CentralFileHeader)
            {
                return nullptr;
            }
            Entry entry;
            entry.Method = ReadU16(bytes.data() + offset + 10);
            entry.CompressedSize = ReadU32(bytes.data() + offset + 20);
            entry.UncompressedSize = ReadU32(bytes.data() + offset + 24);
            const std::uint16_t nameLength = ReadU16(bytes.data() + offset + 28);
            const std::uint16_t extraLength = ReadU16(bytes.data() + offset + 30);
            const std::uint16_t commentLength = ReadU16(bytes.data() + offset + 32);
            entry.LocalHeaderOffset = ReadU32(bytes.data() + offset + 42);
            entry.FullName.assign(
                reinterpret_cast<const char*>(bytes.data() + offset + 46), nameLength);
            archive->_entries.push_back(std::move(entry));
            offset += 46U + nameLength + extraLength + commentLength;
        }
        archive->_bytes = std::move(bytes);
        return archive;
    }

    const std::string& ZipArchive::FullName(std::size_t index) const
    {
        if (index >= _entries.size())
        {
            throw System::ArgumentOutOfRangeException();
        }
        return _entries[index].FullName;
    }

    std::vector<std::uint8_t> ZipArchive::Read(std::size_t index) const
    {
        if (index >= _entries.size())
        {
            throw System::ArgumentOutOfRangeException();
        }
        const Entry& entry = _entries[index];
        const std::size_t header = entry.LocalHeaderOffset;
        if (header + 30 > _bytes.size())
        {
            throw System::IO::InvalidDataException();
        }
        // The local header repeats the name and extra lengths, which are the
        // only fields in it that matter here.
        const std::uint16_t nameLength = ReadU16(_bytes.data() + header + 26);
        const std::uint16_t extraLength = ReadU16(_bytes.data() + header + 28);
        const std::size_t start = header + 30U + nameLength + extraLength;
        if (start + entry.CompressedSize > _bytes.size())
        {
            throw System::IO::InvalidDataException();
        }

        if (entry.Method == 0)
        {
            return std::vector<std::uint8_t>(
                _bytes.begin() + static_cast<std::ptrdiff_t>(start),
                _bytes.begin() + static_cast<std::ptrdiff_t>(start + entry.CompressedSize));
        }
        if (entry.Method != 8)
        {
            throw System::IO::InvalidDataException(
                "The archive entry was compressed using an unsupported compression method.");
        }

        std::vector<std::uint8_t> out(entry.UncompressedSize);
        z_stream stream{};
        // -MAX_WBITS: the entry holds raw deflate, with no zlib wrapper.
        if (::inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        {
            throw System::IO::InvalidDataException();
        }
        stream.next_in = const_cast<Bytef*>(
            reinterpret_cast<const Bytef*>(_bytes.data() + start));
        stream.avail_in = static_cast<uInt>(entry.CompressedSize);
        stream.next_out = reinterpret_cast<Bytef*>(out.data());
        stream.avail_out = static_cast<uInt>(out.size());
        const int result = ::inflate(&stream, Z_FINISH);
        ::inflateEnd(&stream);
        if (result != Z_STREAM_END && result != Z_OK)
        {
            throw System::IO::InvalidDataException();
        }
        out.resize(out.size() - stream.avail_out);
        return out;
    }
}
