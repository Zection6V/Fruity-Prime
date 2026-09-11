#pragma once

#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace MphRead
{
    class EndOfStreamException final : public std::runtime_error
    {
    public:
        EndOfStreamException();
    };

    class InvalidDataException final : public std::runtime_error
    {
    public:
        explicit InvalidDataException(const std::string& message);
    };

    class IndexOutOfRangeException final : public std::out_of_range
    {
    public:
        IndexOutOfRangeException();
    };

    class OverflowException final : public std::overflow_error
    {
    public:
        OverflowException();
    };

    class LZUtil final
    {
    public:
        static std::int32_t GetOccurrenceLength(std::uint8_t* newPtr, std::int32_t newLength,
            std::uint8_t* oldPtr, std::int32_t oldLength, std::int32_t& disp, std::int32_t minDisp = 1);

        LZUtil() = delete;
        LZUtil(const LZUtil&) = delete;
        LZUtil& operator=(const LZUtil&) = delete;
    };

    class LZ10 final
    {
    public:
        static std::uint8_t MagicByte();

        static std::int64_t Decompress(const std::string& input, const std::string& output);
        static std::int64_t Decompress(std::istream& instream, std::int64_t inLength, std::ostream& outstream);

        static std::int32_t Compress(const std::string& input, const std::string& output);
        static std::int32_t Compress(std::istream& instream, std::int64_t inLength, std::ostream& outstream);

        LZ10() = delete;
        LZ10(const LZ10&) = delete;
        LZ10& operator=(const LZ10&) = delete;

    private:
        static std::int32_t ToNDSu24(const std::vector<std::uint8_t>& buffer, std::int32_t offset);
        static std::int32_t ToNDSs32(const std::vector<std::uint8_t>& buffer, std::int32_t offset);
    };

    class LZBackward final
    {
    public:
        static std::int64_t Decompress(const std::string& input, const std::string& output);
        static std::int64_t Decompress(std::istream& instream, std::int64_t inLength, std::ostream& outstream);

        static std::int32_t Compress(std::istream& instream, std::int64_t inLength, std::ostream& outstream);

        static std::uint32_t ToNDSu32(const std::vector<std::uint8_t>& buffer, std::int32_t offset);

        LZBackward() = delete;
        LZBackward(const LZBackward&) = delete;
        LZBackward& operator=(const LZBackward&) = delete;

    private:
        static std::int32_t CompressNormal(std::istream& instream, std::int64_t inLength, std::ostream& outstream);
    };
}
