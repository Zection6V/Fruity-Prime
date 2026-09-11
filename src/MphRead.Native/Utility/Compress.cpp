#include "Compress.hpp"

#include "../Program.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    using MphRead::EndOfStreamException;
    using MphRead::IndexOutOfRangeException;
    using MphRead::OverflowException;

    std::vector<std::uint8_t> NewByteArray(std::int64_t length)
    {
        if (length < 0)
        {
            throw OverflowException();
        }
        return std::vector<std::uint8_t>(static_cast<std::size_t>(length));
    }

    std::uint8_t& At(std::vector<std::uint8_t>& buffer, std::int64_t index)
    {
        if (index < 0 || static_cast<std::uint64_t>(index) >= buffer.size())
        {
            throw IndexOutOfRangeException();
        }
        return buffer[static_cast<std::size_t>(index)];
    }

    std::uint8_t At(const std::vector<std::uint8_t>& buffer, std::int64_t index)
    {
        if (index < 0 || static_cast<std::uint64_t>(index) >= buffer.size())
        {
            throw IndexOutOfRangeException();
        }
        return buffer[static_cast<std::size_t>(index)];
    }

    std::int32_t UncheckedAdd(std::int32_t left, std::int32_t right)
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    std::int32_t UncheckedSubtract(std::int32_t left, std::int32_t right)
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
    }

    std::int32_t ReadByte(std::istream& stream)
    {
        const std::char_traits<char>::int_type value = stream.rdbuf()->sbumpc();
        if (std::char_traits<char>::eq_int_type(value, std::char_traits<char>::eof()))
        {
            return -1;
        }
        return static_cast<std::uint8_t>(std::char_traits<char>::to_char_type(value));
    }

    std::int32_t Read(std::istream& stream, std::uint8_t* buffer, std::int32_t count)
    {
        if (count == 0)
        {
            return 0;
        }
        const std::streamsize read = stream.rdbuf()->sgetn(
            reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(count));
        return static_cast<std::int32_t>(read);
    }

    void ReadExactly(std::istream& stream, std::uint8_t* buffer, std::int32_t count)
    {
        std::int32_t total = 0;
        while (total < count)
        {
            const std::streamsize read = stream.rdbuf()->sgetn(
                reinterpret_cast<char*>(buffer + total),
                static_cast<std::streamsize>(count - total));
            if (read <= 0)
            {
                throw EndOfStreamException();
            }
            total += static_cast<std::int32_t>(read);
        }
    }

    void WriteByte(std::ostream& stream, std::uint8_t value)
    {
        const std::char_traits<char>::int_type result =
            stream.rdbuf()->sputc(static_cast<char>(value));
        if (std::char_traits<char>::eq_int_type(result, std::char_traits<char>::eof()))
        {
            throw std::ios_base::failure("Write failed");
        }
    }

    void Write(std::ostream& stream, const std::uint8_t* buffer, std::int32_t count)
    {
        if (count == 0)
        {
            return;
        }
        const std::streamsize written = stream.rdbuf()->sputn(
            reinterpret_cast<const char*>(buffer), static_cast<std::streamsize>(count));
        if (written != count)
        {
            throw std::ios_base::failure("Write failed");
        }
    }

    void Flush(std::ostream& stream)
    {
        if (stream.rdbuf()->pubsync() != 0)
        {
            throw std::ios_base::failure("Flush failed");
        }
    }

    std::int64_t Position(std::istream& stream)
    {
        const std::streampos position = stream.rdbuf()->pubseekoff(
            0, std::ios_base::cur, std::ios_base::in);
        if (position == std::streampos(std::streamoff(-1)))
        {
            throw std::ios_base::failure("Stream does not support seeking");
        }
        return static_cast<std::int64_t>(static_cast<std::streamoff>(position));
    }

    void PositionAdd(std::istream& stream, std::int64_t delta)
    {
        const std::streampos position = stream.rdbuf()->pubseekoff(
            static_cast<std::streamoff>(delta), std::ios_base::cur, std::ios_base::in);
        if (position == std::streampos(std::streamoff(-1)))
        {
            throw std::ios_base::failure("Stream does not support seeking");
        }
    }

    std::int64_t Length(std::istream& stream)
    {
        const std::streampos current = stream.rdbuf()->pubseekoff(
            0, std::ios_base::cur, std::ios_base::in);
        if (current == std::streampos(std::streamoff(-1)))
        {
            throw std::ios_base::failure("Stream does not support seeking");
        }
        const std::streampos end = stream.rdbuf()->pubseekoff(
            0, std::ios_base::end, std::ios_base::in);
        if (end == std::streampos(std::streamoff(-1)))
        {
            throw std::ios_base::failure("Stream does not support seeking");
        }
        if (stream.rdbuf()->pubseekpos(current, std::ios_base::in)
            == std::streampos(std::streamoff(-1)))
        {
            throw std::ios_base::failure("Stream does not support seeking");
        }
        return static_cast<std::int64_t>(static_cast<std::streamoff>(end));
    }

    void ThrowOpenError()
    {
        const int error = errno == 0 ? EIO : errno;
        throw std::system_error(error, std::generic_category());
    }

    std::string HexUnsigned(std::uint64_t value)
    {
        std::ostringstream stream;
        stream << std::uppercase << std::hex << value;
        return stream.str();
    }

    std::string Hex(std::int32_t value)
    {
        if (value < 0)
        {
            return HexUnsigned(static_cast<std::uint32_t>(value));
        }
        return HexUnsigned(static_cast<std::uint32_t>(value));
    }

    std::string Hex(std::int64_t value)
    {
        if (value < 0)
        {
            return HexUnsigned(static_cast<std::uint64_t>(value));
        }
        return HexUnsigned(static_cast<std::uint64_t>(value));
    }
}

namespace MphRead
{
    EndOfStreamException::EndOfStreamException()
        : std::runtime_error("Unable to read beyond the end of the stream.")
    {
    }

    InvalidDataException::InvalidDataException(const std::string& message)
        : std::runtime_error(message)
    {
    }

    IndexOutOfRangeException::IndexOutOfRangeException()
        : std::out_of_range("Index was outside the bounds of the array.")
    {
    }

    OverflowException::OverflowException()
        : std::overflow_error("Arithmetic operation resulted in an overflow.")
    {
    }

    std::int32_t LZUtil::GetOccurrenceLength(std::uint8_t* newPtr, std::int32_t newLength,
        std::uint8_t* oldPtr, std::int32_t oldLength, std::int32_t& disp, std::int32_t minDisp)
    {
        disp = 0;
        if (newLength == 0)
        {
            return 0;
        }

        std::int32_t maxLength = 0;
        for (std::int32_t i = 0; i < UncheckedSubtract(oldLength, minDisp); i++)
        {
            std::uint8_t* currentOldStart = oldPtr + i;
            std::int32_t currentLength = 0;
            for (std::int32_t j = 0; j < newLength; j++)
            {
                if (*(currentOldStart + j) != *(newPtr + j))
                {
                    break;
                }

                currentLength++;
            }

            if (currentLength > maxLength)
            {
                maxLength = currentLength;
                disp = UncheckedSubtract(oldLength, i);

                if (maxLength == newLength)
                {
                    break;
                }
            }
        }
        return maxLength;
    }

    std::uint8_t LZ10::MagicByte()
    {
        return 0x10;
    }

    std::int64_t LZ10::Decompress(const std::string& input, const std::string& output)
    {
        std::ifstream inStream(input, std::ios::binary);
        if (!inStream.is_open())
        {
            ThrowOpenError();
        }
        std::fstream outStream(output,
            std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!outStream.is_open())
        {
            ThrowOpenError();
        }
        return Decompress(inStream, Length(inStream), outStream);
    }

    std::int64_t LZ10::Decompress(
        std::istream& instream, std::int64_t inLength, std::ostream& outstream)
    {
        std::int64_t readBytes = 0;

        const std::uint8_t type = static_cast<std::uint8_t>(ReadByte(instream));
        if (type != MagicByte())
        {
            throw InvalidDataException(
                "The provided stream is not a valid LZ-0x10 compressed stream (invalid type 0x"
                + Hex(static_cast<std::int32_t>(type)) + ")");
        }

        std::vector<std::uint8_t> sizeBytes(3);
        ReadExactly(instream, sizeBytes.data(), 3);
        std::int32_t decompressedSize = ToNDSu24(sizeBytes, 0);
        readBytes += 4;
        if (decompressedSize == 0)
        {
            sizeBytes = std::vector<std::uint8_t>(4);
            ReadExactly(instream, sizeBytes.data(), 4);
            decompressedSize = ToNDSs32(sizeBytes, 0);
            readBytes += 4;
        }

        const std::int32_t bufferLength = 0x1000;
        std::vector<std::uint8_t> buffer(static_cast<std::size_t>(bufferLength));
        std::int32_t bufferOffset = 0;

        std::int32_t currentOutSize = 0;
        std::int32_t flags = 0;
        std::int32_t mask = 1;
        while (currentOutSize < decompressedSize)
        {
            if (mask == 1)
            {
                if (readBytes >= inLength)
                {
                    throw ProgramException(
                        "Not enough data " + std::to_string(currentOutSize)
                        + ", " + std::to_string(decompressedSize));
                }

                flags = ReadByte(instream);
                readBytes++;
                if (flags < 0)
                {
                    throw ProgramException("Stream too short");
                }

                mask = 0x80;
            }
            else
            {
                mask >>= 1;
            }

            if ((flags & mask) > 0)
            {
                if (readBytes + 1 >= inLength)
                {
                    if (readBytes < inLength)
                    {
                        ReadByte(instream);
                        readBytes++;
                    }
                    throw ProgramException(
                        "Not enough data " + std::to_string(currentOutSize)
                        + ", " + std::to_string(decompressedSize));
                }

                const std::int32_t byte1 = ReadByte(instream);
                readBytes++;
                const std::int32_t byte2 = ReadByte(instream);
                readBytes++;
                if (byte2 < 0)
                {
                    throw ProgramException("Stream too short");
                }

                std::int32_t length = byte1 >> 4;
                length += 3;

                std::int32_t disp = ((byte1 & 0x0F) << 8) | byte2;
                disp += 1;

                if (disp > currentOutSize)
                {
                    throw InvalidDataException(
                        "Cannot go back more than already written. DISP = 0x"
                        + Hex(disp) + ", #written bytes = 0x" + Hex(currentOutSize)
                        + " at 0x" + Hex(Position(instream) - 2));
                }

                std::int32_t bufIdx = bufferOffset + bufferLength - disp;
                for (std::int32_t i = 0; i < length; i++)
                {
                    const std::uint8_t next = At(buffer, bufIdx % bufferLength);
                    bufIdx++;
                    WriteByte(outstream, next);
                    At(buffer, bufferOffset) = next;
                    bufferOffset = (bufferOffset + 1) % bufferLength;
                }
                currentOutSize = UncheckedAdd(currentOutSize, length);
            }
            else
            {
                if (readBytes >= inLength)
                {
                    throw ProgramException(
                        "Not enough data " + std::to_string(currentOutSize)
                        + ", " + std::to_string(decompressedSize));
                }

                const std::int32_t next = ReadByte(instream);
                readBytes++;
                if (next < 0)
                {
                    throw ProgramException("Stream too short");
                }

                currentOutSize++;
                WriteByte(outstream, static_cast<std::uint8_t>(next));
                At(buffer, bufferOffset) = static_cast<std::uint8_t>(next);
                bufferOffset = (bufferOffset + 1) % bufferLength;
            }
            Flush(outstream);
        }

        if (readBytes < inLength)
        {
            if ((readBytes ^ (readBytes & 3)) + 4 < inLength)
            {
                throw ProgramException(
                    "Too much input " + std::to_string(readBytes)
                    + ", " + std::to_string(inLength));
            }
        }

        return decompressedSize;
    }

    std::int32_t LZ10::Compress(const std::string& input, const std::string& output)
    {
        std::fstream inStream(input, std::ios::binary | std::ios::in | std::ios::out);
        if (!inStream.is_open())
        {
            ThrowOpenError();
        }

        std::fstream outStream(output,
            std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!outStream.is_open())
        {
            ThrowOpenError();
        }
        return Compress(inStream, Length(inStream), outStream);
    }

    std::int32_t LZ10::Compress(
        std::istream& instream, std::int64_t inLength, std::ostream& outstream)
    {
        if (inLength > 0xFFFFFF)
        {
            throw ProgramException("Input too large");
        }

        std::vector<std::uint8_t> indata = NewByteArray(inLength);
        const std::int32_t numReadBytes = Read(
            instream, indata.data(), static_cast<std::int32_t>(inLength));
        if (numReadBytes != inLength)
        {
            throw ProgramException("Stream too short");
        }

        WriteByte(outstream, MagicByte());
        WriteByte(outstream, static_cast<std::uint8_t>(inLength & 0xFF));
        WriteByte(outstream, static_cast<std::uint8_t>((inLength >> 8) & 0xFF));
        WriteByte(outstream, static_cast<std::uint8_t>((inLength >> 16) & 0xFF));

        std::int32_t compressedLength = 4;

        if (indata.empty())
        {
            throw IndexOutOfRangeException();
        }
        std::uint8_t* instart = &indata[0];

        std::vector<std::uint8_t> outbuffer(8 * 2 + 1);
        outbuffer[0] = 0;
        std::int32_t bufferlength = 1;
        std::int32_t bufferedBlocks = 0;
        std::int32_t readBytes = 0;
        while (readBytes < inLength)
        {
            if (bufferedBlocks == 8)
            {
                Write(outstream, outbuffer.data(), bufferlength);
                compressedLength += bufferlength;
                outbuffer[0] = 0;
                bufferlength = 1;
                bufferedBlocks = 0;
            }

            const std::int32_t oldLength = std::min(readBytes, 0x1000);
            std::int32_t disp;
            const std::int32_t length = LZUtil::GetOccurrenceLength(
                instart + readBytes,
                static_cast<std::int32_t>(std::min<std::int64_t>(inLength - readBytes, 0x12)),
                instart + readBytes - oldLength, oldLength, disp);

            if (length < 3)
            {
                At(outbuffer, bufferlength++) = *(instart + readBytes++);
            }
            else
            {
                readBytes += length;

                outbuffer[0] |= static_cast<std::uint8_t>(1 << (7 - bufferedBlocks));

                At(outbuffer, bufferlength) =
                    static_cast<std::uint8_t>(((length - 3) << 4) & 0xF0);
                At(outbuffer, bufferlength) |=
                    static_cast<std::uint8_t>(((disp - 1) >> 8) & 0x0F);
                bufferlength++;
                At(outbuffer, bufferlength) =
                    static_cast<std::uint8_t>((disp - 1) & 0xFF);
                bufferlength++;
            }
            bufferedBlocks++;
        }

        if (bufferedBlocks > 0)
        {
            Write(outstream, outbuffer.data(), bufferlength);
            compressedLength += bufferlength;
        }

        return compressedLength;
    }

    std::int32_t LZ10::ToNDSu24(
        const std::vector<std::uint8_t>& buffer, std::int32_t offset)
    {
        return At(buffer, offset)
            | (At(buffer, UncheckedAdd(offset, 1)) << 8)
            | (At(buffer, UncheckedAdd(offset, 2)) << 16);
    }

    std::int32_t LZ10::ToNDSs32(
        const std::vector<std::uint8_t>& buffer, std::int32_t offset)
    {
        const std::uint32_t value =
            static_cast<std::uint32_t>(At(buffer, offset))
            | (static_cast<std::uint32_t>(At(buffer, UncheckedAdd(offset, 1))) << 8)
            | (static_cast<std::uint32_t>(At(buffer, UncheckedAdd(offset, 2))) << 16)
            | (static_cast<std::uint32_t>(At(buffer, UncheckedAdd(offset, 3))) << 24);
        return std::bit_cast<std::int32_t>(value);
    }

    std::int64_t LZBackward::Decompress(const std::string& input, const std::string& output)
    {
        std::ifstream inStream(input, std::ios::binary);
        if (!inStream.is_open())
        {
            ThrowOpenError();
        }
        std::fstream outStream(output,
            std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!outStream.is_open())
        {
            ThrowOpenError();
        }
        return Decompress(inStream, Length(inStream), outStream);
    }

    std::int64_t LZBackward::Decompress(
        std::istream& instream, std::int64_t inLength, std::ostream& outstream)
    {
        PositionAdd(instream, inLength - 4);

        std::vector<std::uint8_t> buffer(4);
        try
        {
            ReadExactly(instream, buffer.data(), 4);
        }
        catch (const EndOfStreamException&)
        {
            throw ProgramException("Stream too short");
        }
        const std::uint32_t extraSize = ToNDSu32(buffer, 0);

        if (extraSize == 0)
        {
            PositionAdd(instream, -inLength);
            buffer = NewByteArray(inLength - 4);
            ReadExactly(instream, buffer.data(), static_cast<std::int32_t>(inLength - 4));
            Write(outstream, buffer.data(), static_cast<std::int32_t>(inLength - 4));

            PositionAdd(instream, 4);

            return inLength - 4;
        }

        PositionAdd(instream, -5);
        const std::int32_t headerSize = ReadByte(instream);

        PositionAdd(instream, -4);
        ReadExactly(instream, buffer.data(), 3);
        std::int32_t compressedSize =
            At(buffer, 0) | (At(buffer, 1) << 8) | (At(buffer, 2) << 16);
        compressedSize -= headerSize;

        if (static_cast<std::int64_t>(compressedSize) + headerSize >= inLength)
        {
            compressedSize = static_cast<std::int32_t>(inLength - headerSize);
        }

        buffer = NewByteArray(inLength - headerSize - compressedSize);
        PositionAdd(instream, -(inLength - 5));
        ReadExactly(instream, buffer.data(), static_cast<std::int32_t>(buffer.size()));
        Write(outstream, buffer.data(), static_cast<std::int32_t>(buffer.size()));

        buffer = NewByteArray(compressedSize);
        ReadExactly(instream, buffer.data(), compressedSize);

        std::vector<std::uint8_t> outbuffer = NewByteArray(
            static_cast<std::int64_t>(compressedSize) + headerSize + extraSize);

        std::int32_t currentOutSize = 0;
        const std::int32_t decompressedLength = static_cast<std::int32_t>(outbuffer.size());
        std::int32_t readBytes = 0;
        std::uint8_t flags = 0;
        std::uint8_t mask = 1;
        while (currentOutSize < decompressedLength)
        {
            if (mask == 1)
            {
                if (readBytes >= compressedSize)
                {
                    break;
                }
                flags = At(buffer, static_cast<std::int64_t>(buffer.size()) - 1 - readBytes);
                readBytes++;
                mask = 0x80;
            }
            else
            {
                mask >>= 1;
            }

            if ((flags & mask) > 0)
            {
                if (static_cast<std::int64_t>(readBytes) + 1 >= inLength)
                {
                    throw ProgramException(
                        "Not enough data " + std::to_string(currentOutSize)
                        + ", " + std::to_string(decompressedLength));
                }

                const std::int32_t bufIndex = compressedSize - 1 - readBytes;
                if (bufIndex == -1)
                {
                    break;
                }
                const std::int32_t byte1 = At(buffer, bufIndex);
                readBytes++;

                const std::int32_t index2 = compressedSize - 1 - readBytes;
                if (index2 == -1)
                {
                    break;
                }
                const std::int32_t byte2 = At(buffer, index2);
                readBytes++;

                std::int32_t length = byte1 >> 4;
                length += 3;

                std::int32_t disp = ((byte1 & 0x0F) << 8) | byte2;
                disp += 3;

                if (disp > currentOutSize)
                {
                    if (currentOutSize < 2)
                    {
                        throw InvalidDataException(
                            "Cannot go back more than already written; attempt to go back 0x"
                            + Hex(disp) + " when only 0x" + Hex(currentOutSize)
                            + " bytes have been written.");
                    }
                    disp = 2;
                }

                std::int32_t bufIdx = currentOutSize - disp;
                for (std::int32_t i = 0; i < length; i++)
                {
                    const std::uint8_t next = At(
                        outbuffer, static_cast<std::int64_t>(outbuffer.size()) - 1 - bufIdx);
                    bufIdx++;

                    const std::int32_t outIndex =
                        static_cast<std::int32_t>(outbuffer.size()) - 1 - currentOutSize;
                    if (outIndex == -1)
                    {
                        break;
                    }
                    At(outbuffer, outIndex) = next;
                    currentOutSize++;
                }
            }
            else
            {
                if (readBytes >= inLength)
                {
                    throw ProgramException(
                        "Not enough data " + std::to_string(currentOutSize)
                        + ", " + std::to_string(decompressedLength));
                }
                const std::int32_t nextIndex =
                    static_cast<std::int32_t>(buffer.size()) - 1 - readBytes;
                if (nextIndex == -1)
                {
                    break;
                }
                const std::uint8_t next = At(buffer, nextIndex);
                readBytes++;

                At(outbuffer,
                    static_cast<std::int64_t>(outbuffer.size()) - 1 - currentOutSize) = next;
                currentOutSize++;
            }
        }

        Write(outstream, outbuffer.data(), static_cast<std::int32_t>(outbuffer.size()));

        PositionAdd(instream, headerSize);

        return static_cast<std::int64_t>(decompressedLength)
            + (inLength - headerSize - compressedSize);
    }

    std::int32_t LZBackward::Compress(
        std::istream& instream, std::int64_t inLength, std::ostream& outstream)
    {
        if (inLength > 0xFFFFFF)
        {
            throw ProgramException("Input too large");
        }

        std::vector<std::uint8_t> indata = NewByteArray(inLength);
        ReadExactly(instream, indata.data(), static_cast<std::int32_t>(inLength));
        std::reverse(indata.begin(), indata.end());

        std::string inputText;
        if (!indata.empty())
        {
            inputText.assign(
                reinterpret_cast<const char*>(indata.data()), indata.size());
        }
        std::istringstream inMemStream(inputText, std::ios::in | std::ios::binary);
        std::ostringstream outMemStream(std::ios::out | std::ios::binary);
        const std::int32_t compressedLength =
            CompressNormal(inMemStream, inLength, outMemStream);

        const std::string compressedText = outMemStream.str();
        std::int32_t totalCompFileLength =
            static_cast<std::int32_t>(compressedText.size()) + 8;
        if (totalCompFileLength % 4 != 0)
        {
            totalCompFileLength += 4 - totalCompFileLength % 4;
        }

        if (totalCompFileLength < inLength)
        {
            std::vector<std::uint8_t> compData(
                compressedText.begin(), compressedText.end());
            std::reverse(compData.begin(), compData.end());
            Write(outstream, compData.data(), static_cast<std::int32_t>(compData.size()));
            std::int32_t writtenBytes = static_cast<std::int32_t>(compData.size());

            while (writtenBytes % 4 != 0)
            {
                WriteByte(outstream, 0xFF);
                writtenBytes++;
            }

            WriteByte(outstream, static_cast<std::uint8_t>(compressedLength & 0xFF));
            WriteByte(outstream, static_cast<std::uint8_t>((compressedLength >> 8) & 0xFF));
            WriteByte(outstream, static_cast<std::uint8_t>((compressedLength >> 16) & 0xFF));

            const std::int32_t headerLength =
                totalCompFileLength - static_cast<std::int32_t>(compData.size());
            WriteByte(outstream, static_cast<std::uint8_t>(headerLength));

            const std::int32_t extraSize =
                static_cast<std::int32_t>(inLength) - totalCompFileLength;
            WriteByte(outstream, static_cast<std::uint8_t>(extraSize & 0xFF));
            WriteByte(outstream, static_cast<std::uint8_t>((extraSize >> 8) & 0xFF));
            WriteByte(outstream, static_cast<std::uint8_t>((extraSize >> 16) & 0xFF));
            WriteByte(outstream, static_cast<std::uint8_t>((extraSize >> 24) & 0xFF));

            return totalCompFileLength;
        }

        std::reverse(indata.begin(), indata.end());
        Write(outstream, indata.data(), static_cast<std::int32_t>(inLength));
        WriteByte(outstream, 0);
        WriteByte(outstream, 0);
        WriteByte(outstream, 0);
        WriteByte(outstream, 0);
        return static_cast<std::int32_t>(inLength) + 4;
    }

    std::int32_t LZBackward::CompressNormal(
        std::istream& instream, std::int64_t inLength, std::ostream& outstream)
    {
        if (inLength > 0xFFFFFF)
        {
            throw ProgramException("Input too large");
        }

        std::vector<std::uint8_t> indata = NewByteArray(inLength);
        const std::int32_t numReadBytes = Read(
            instream, indata.data(), static_cast<std::int32_t>(inLength));
        if (numReadBytes != inLength)
        {
            throw ProgramException("Stream too short");
        }

        std::int32_t compressedLength = 0;

        if (indata.empty())
        {
            throw IndexOutOfRangeException();
        }
        std::uint8_t* instart = &indata[0];

        std::vector<std::uint8_t> outbuffer(8 * 2 + 1);
        outbuffer[0] = 0;
        std::int32_t bufferlength = 1;
        std::int32_t bufferedBlocks = 0;
        std::int32_t readBytes = 0;
        while (readBytes < inLength)
        {
            if (bufferedBlocks == 8)
            {
                Write(outstream, outbuffer.data(), bufferlength);
                compressedLength += bufferlength;
                outbuffer[0] = 0;
                bufferlength = 1;
                bufferedBlocks = 0;
            }

            const std::int32_t oldLength = std::min(readBytes, 0x1001);
            std::int32_t disp;
            std::int32_t length = LZUtil::GetOccurrenceLength(
                instart + readBytes,
                static_cast<std::int32_t>(std::min<std::int64_t>(inLength - readBytes, 0x12)),
                instart + readBytes - oldLength, oldLength, disp);

            if (disp == 1)
            {
                length = 1;
            }
            else if (disp == 2)
            {
                length = 1;
            }

            if (length < 3)
            {
                At(outbuffer, bufferlength++) = *(instart + readBytes++);
            }
            else
            {
                readBytes += length;

                outbuffer[0] |= static_cast<std::uint8_t>(1 << (7 - bufferedBlocks));

                At(outbuffer, bufferlength) =
                    static_cast<std::uint8_t>(((length - 3) << 4) & 0xF0);
                At(outbuffer, bufferlength) |=
                    static_cast<std::uint8_t>(((disp - 3) >> 8) & 0x0F);
                bufferlength++;
                At(outbuffer, bufferlength) =
                    static_cast<std::uint8_t>((disp - 3) & 0xFF);
                bufferlength++;
            }
            bufferedBlocks++;
        }

        if (bufferedBlocks > 0)
        {
            Write(outstream, outbuffer.data(), bufferlength);
            compressedLength += bufferlength;
        }

        return compressedLength;
    }

    std::uint32_t LZBackward::ToNDSu32(
        const std::vector<std::uint8_t>& buffer, std::int32_t offset)
    {
        return static_cast<std::uint32_t>(At(buffer, offset))
            | (static_cast<std::uint32_t>(At(buffer, UncheckedAdd(offset, 1))) << 8)
            | (static_cast<std::uint32_t>(At(buffer, UncheckedAdd(offset, 2))) << 16)
            | (static_cast<std::uint32_t>(At(buffer, UncheckedAdd(offset, 3))) << 24);
    }
}
