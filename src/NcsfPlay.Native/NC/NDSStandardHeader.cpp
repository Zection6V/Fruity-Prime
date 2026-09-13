#include "NDSStandardHeader.hpp"

#include "../Common.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace
{
#ifndef NDEBUG
    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset);
    }

    [[nodiscard]] std::span<const std::uint8_t> Slice(
        std::span<const std::uint8_t> span, std::size_t offset, std::size_t count)
    {
        if (offset > span.size() || count > span.size() - offset)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset, count);
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint32_t))
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return static_cast<std::uint32_t>(span[0])
            | (static_cast<std::uint32_t>(span[1]) << 8U)
            | (static_cast<std::uint32_t>(span[2]) << 16U)
            | (static_cast<std::uint32_t>(span[3]) << 24U);
    }

    [[nodiscard]] std::uint16_t ReadUInt16LittleEndian(std::span<const std::uint8_t> span)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(span[0])
            | (static_cast<std::uint16_t>(span[1]) << 8U));
    }
#endif

    [[nodiscard]] std::span<std::uint8_t> Slice(
        std::span<std::uint8_t> span, std::size_t offset)
    {
        if (offset > span.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset);
    }

    void CopyTo(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination)
    {
        if (source.size() > destination.size())
        {
            throw std::invalid_argument("Destination is too short.");
        }
        std::copy(source.begin(), source.end(), destination.begin());
    }

    void WriteUInt32LittleEndian(std::span<std::uint8_t> span, std::uint32_t value)
    {
        if (span.size() < sizeof(std::uint32_t))
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        span[0] = static_cast<std::uint8_t>(value);
        span[1] = static_cast<std::uint8_t>(value >> 8U);
        span[2] = static_cast<std::uint8_t>(value >> 16U);
        span[3] = static_cast<std::uint8_t>(value >> 24U);
    }

    void WriteUInt16LittleEndian(std::span<std::uint8_t> span, std::uint16_t value)
    {
        if (span.size() < sizeof(std::uint16_t))
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        span[0] = static_cast<std::uint8_t>(value);
        span[1] = static_cast<std::uint8_t>(value >> 8U);
    }
}

namespace NCSFCommon::NC
{
    const std::vector<std::uint8_t>& NDSStandardHeader::ExpectedHeaderValue() const
    {
        std::unique_lock lock(_expectedHeaderMutex);
        for (;;)
        {
            switch (_expectedHeaderState)
            {
            case ExpectedHeaderState::Value:
                return _expectedHeader;
            case ExpectedHeaderState::Error:
                std::rethrow_exception(_expectedHeaderError);
            case ExpectedHeaderState::Initializing:
                if (_expectedHeaderThread == std::this_thread::get_id())
                {
                    throw std::logic_error(
                        "ValueFactory attempted to access the Value property of this instance.");
                }
                _expectedHeaderCondition.wait(lock, [this]
                {
                    return _expectedHeaderState != ExpectedHeaderState::Initializing;
                });
                continue;
            case ExpectedHeaderState::Uninitialized:
                _expectedHeaderState = ExpectedHeaderState::Initializing;
                _expectedHeaderThread = std::this_thread::get_id();
                lock.unlock();
                try
                {
                    std::vector<std::uint8_t> value = ExpectedHeader();
                    lock.lock();
                    _expectedHeader = std::move(value);
                    _expectedHeaderThread = {};
                    _expectedHeaderState = ExpectedHeaderState::Value;
                    lock.unlock();
                    _expectedHeaderCondition.notify_all();
                    return _expectedHeader;
                }
                catch (...)
                {
                    const std::exception_ptr error = std::current_exception();
                    lock.lock();
                    _expectedHeaderError = error;
                    _expectedHeaderThread = {};
                    _expectedHeaderState = ExpectedHeaderState::Error;
                    lock.unlock();
                    _expectedHeaderCondition.notify_all();
                    std::rethrow_exception(error);
                }
            }
        }
    }

    std::span<const std::uint8_t> NDSStandardHeader::Header() const
    {
        const std::vector<std::uint8_t>& expectedHeader = ExpectedHeaderValue();
        return std::span<const std::uint8_t>(expectedHeader.data(), expectedHeader.size());
    }

    void NDSStandardHeader::Read(std::span<const std::uint8_t> span) const
    {
#ifndef NDEBUG
        const std::span<const std::uint8_t> actualHeader = Slice(span, 0, 0x04);
        const std::span<const std::uint8_t> expectedHeader = Header();
        assert(Common::VerifyHeader(actualHeader, expectedHeader));

        const std::span<const std::uint8_t> magicSpan = Slice(span, 0x04);
        const std::uint32_t actualMagic = ReadUInt32LittleEndian(magicSpan);
        const std::uint32_t expectedMagic = Magic();
        assert(actualMagic == expectedMagic);

        const std::span<const std::uint8_t> headerSizeSpan = Slice(span, 0x0C);
        const std::uint16_t actualHeaderSize = ReadUInt16LittleEndian(headerSizeSpan);
        const std::uint16_t expectedHeaderSize = HeaderSize();
        assert(actualHeaderSize == expectedHeaderSize);
#else
        (void)span;
#endif
    }

    void NDSStandardHeader::Write(std::span<std::uint8_t> span)
    {
        CopyTo(Header(), span);

        std::span<std::uint8_t> magicSpan = Slice(span, 0x04);
        const std::uint32_t magic = Magic();
        WriteUInt32LittleEndian(magicSpan, magic);

        std::span<std::uint8_t> fileSizeSpan = Slice(span, 0x08);
        const std::uint32_t fileSize = FileSize();
        WriteUInt32LittleEndian(fileSizeSpan, fileSize);

        std::span<std::uint8_t> headerSizeSpan = Slice(span, 0x0C);
        const std::uint16_t headerSize = HeaderSize();
        WriteUInt16LittleEndian(headerSizeSpan, headerSize);

        std::span<std::uint8_t> blocksSpan = Slice(span, 0x0E);
        const std::uint16_t blocks = Blocks();
        WriteUInt16LittleEndian(blocksSpan, blocks);
    }
}
