#include "DemoFile.hpp"

#include "../../Utility/Compress.hpp"

#include <algorithm>
#include <ios>
#include <new>
#include <utility>

namespace
{
    void WriteUInt16LittleEndian(std::span<std::uint8_t> destination, std::uint16_t value)
    {
        destination[0] = static_cast<std::uint8_t>(value);
        destination[1] = static_cast<std::uint8_t>(value >> 8);
    }

    void WriteUInt32LittleEndian(std::span<std::uint8_t> destination, std::uint32_t value)
    {
        destination[0] = static_cast<std::uint8_t>(value);
        destination[1] = static_cast<std::uint8_t>(value >> 8);
        destination[2] = static_cast<std::uint8_t>(value >> 16);
        destination[3] = static_cast<std::uint8_t>(value >> 24);
    }

    std::uint16_t ReadUInt16LittleEndian(std::span<const std::uint8_t> source)
    {
        return static_cast<std::uint16_t>(source[0])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(source[1]) << 8);
    }

    std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> source)
    {
        return static_cast<std::uint32_t>(source[0])
            | (static_cast<std::uint32_t>(source[1]) << 8)
            | (static_cast<std::uint32_t>(source[2]) << 16)
            | (static_cast<std::uint32_t>(source[3]) << 24);
    }
}

namespace MphRead::Mods::Network
{
    std::array<std::uint8_t, 4> DemoFile::Magic = {
        static_cast<std::uint8_t>('F'),
        static_cast<std::uint8_t>('P'),
        static_cast<std::uint8_t>('D'),
        static_cast<std::uint8_t>('M')
    };

    DemoWriter::DemoWriter(const std::string& path)
    {
        Detail::DemoFilePrepareDirectory(path);
        _stream = Detail::DemoFileOpenCreateWriteShareRead(path);
        Detail::DemoFileStreamWrite(_stream, DemoFile::Magic);
        Detail::DemoFileStreamWriteByte(_stream, DemoFile::FormatVersion);
        Detail::DemoFileStreamWriteByte(_stream,
            static_cast<std::uint8_t>(Detail::DemoFileProtocolVersion()));
        Detail::DemoFileStreamFlush(_stream);
        _deflate = Detail::DemoFileCreateDeflateFastest(_stream, true);
    }

    void DemoWriter::WriteRecord(std::uint32_t frame, std::span<const std::uint8_t> data)
    {
        if (frame < _lastFrame)
        {
            frame = _lastFrame;
        }
        const std::uint32_t delta = frame - _lastFrame;
        _lastFrame = frame;

        std::size_t at = 0;
        if (delta < DemoFile::LongGap)
        {
            _header[at++] = static_cast<std::uint8_t>(delta);
        }
        else
        {
            _header[at++] = DemoFile::LongGap;
            WriteUInt32LittleEndian(std::span<std::uint8_t>(_header).subspan(at), delta);
            at += 4;
        }
        WriteUInt16LittleEndian(std::span<std::uint8_t>(_header).subspan(at),
            static_cast<std::uint16_t>(data.size()));
        at += 2;

        Detail::DemoDeflateStreamWrite(_deflate,
            std::span<const std::uint8_t>(_header).first(at));
        Detail::DemoDeflateStreamWrite(_deflate, data);
        if (frame - _lastFlushFrame >= FlushIntervalFrames)
        {
            _lastFlushFrame = frame;
            Detail::DemoDeflateStreamFlush(_deflate);
            Detail::DemoFileStreamFlush(_stream);
        }
    }

    void DemoWriter::Dispose()
    {
        Detail::DemoDeflateStreamDispose(_deflate);
        Detail::DemoFileStreamDispose(_stream);
    }

    DemoRecord::DemoRecord()
        : Frame(0), Data(nullptr)
    {
    }

    DemoRecord::DemoRecord(std::uint32_t frame,
        std::shared_ptr<std::vector<std::uint8_t>> data)
        : Frame(frame), Data(std::move(data))
    {
    }

    DemoRecord& DemoRecord::operator=(const DemoRecord& other)
    {
        if (this != &other)
        {
            this->~DemoRecord();
            new (this) DemoRecord(other);
        }
        return *this;
    }

    DemoRecord& DemoRecord::operator=(DemoRecord&& other) noexcept
    {
        if (this != &other)
        {
            this->~DemoRecord();
            new (this) DemoRecord(std::move(other));
        }
        return *this;
    }

    std::unique_ptr<DemoReader> DemoReader::Open(const std::string& path)
    {
        std::shared_ptr<Detail::DemoFileStreamHandle> stream;
        try
        {
            stream = Detail::DemoFileOpenReadShareRead(path);
            std::array<std::uint8_t, static_cast<std::size_t>(DemoFile::HeaderSize)> header{};
            if (Detail::DemoFileStreamReadAtLeast(stream, header, false) < header.size())
            {
                Detail::DemoFileStreamDispose(stream);
                return nullptr;
            }
            if (!std::equal(DemoFile::Magic.begin(), DemoFile::Magic.end(), header.begin())
                || header[4] != DemoFile::FormatVersion)
            {
                Detail::DemoFileStreamDispose(stream);
                return nullptr;
            }
            return std::unique_ptr<DemoReader>(new DemoReader(stream, header[5]));
        }
        catch (const std::ios_base::failure&)
        {
            if (stream != nullptr)
            {
                Detail::DemoFileStreamDispose(stream);
            }
            return nullptr;
        }
    }

    DemoReader::DemoReader(std::shared_ptr<Detail::DemoFileStreamHandle> stream,
        std::uint8_t protocolVersion)
        : _stream(std::move(stream)),
          _deflate(Detail::DemoFileCreateDeflateDecompress(_stream, true)),
          _protocolVersion(protocolVersion)
    {
    }

    std::optional<DemoRecord> DemoReader::ReadNext()
    {
        try
        {
            if (!Fill(std::span<std::uint8_t>(_header).first(1)))
            {
                return std::nullopt;
            }
            std::uint32_t delta = _header[0];
            if (delta == DemoFile::LongGap)
            {
                if (!Fill(std::span<std::uint8_t>(_header).first(4)))
                {
                    return std::nullopt;
                }
                delta = ReadUInt32LittleEndian(std::span<const std::uint8_t>(_header).first(4));
            }
            if (!Fill(std::span<std::uint8_t>(_header).first(2)))
            {
                return std::nullopt;
            }
            const std::uint16_t length =
                ReadUInt16LittleEndian(std::span<const std::uint8_t>(_header).first(2));
            auto data = std::make_shared<std::vector<std::uint8_t>>(length);
            if (!Fill(*data))
            {
                return std::nullopt;
            }
            _frame += delta;
            return DemoRecord(_frame, std::move(data));
        }
        catch (const InvalidDataException&)
        {
            return std::nullopt;
        }
        catch (const std::ios_base::failure&)
        {
            return std::nullopt;
        }
    }

    bool DemoReader::Fill(std::span<std::uint8_t> destination)
    {
        return Detail::DemoDeflateStreamReadAtLeast(_deflate, destination, false)
            == destination.size();
    }

    std::uint8_t DemoReader::ProtocolVersion() const
    {
        return _protocolVersion;
    }

    void DemoReader::Dispose()
    {
        Detail::DemoDeflateStreamDispose(_deflate);
        Detail::DemoFileStreamDispose(_stream);
    }
}
