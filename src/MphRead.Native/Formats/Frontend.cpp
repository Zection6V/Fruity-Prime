#include "Frontend.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Managed.hpp"

#include <bit>
#include <cassert>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

using ::MphRead::NativeRuntime::FileReadAllBytes;
using ::MphRead::NativeRuntime::UInt32ToInt32;

namespace MphRead::Formats
{
    namespace
    {
        using ByteSpan = std::span<const std::uint8_t>;

        // int + uint wrapped back to an int: the one mixed-sign addition
        // this file makes, kept apart from the shared UncheckedAdd.
        std::int32_t UncheckedAddUInt32(std::int32_t value, std::uint32_t increment) noexcept
        {
            return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value) + increment);
        }

        ByteSpan Slice(ByteSpan bytes, std::int32_t start, std::uint32_t length)
        {
            std::int32_t end = UncheckedAddUInt32(start, length);
            if (start < 0 || end < start || end < 0
                || static_cast<std::size_t>(end) > bytes.size())
            {
                throw std::out_of_range("ReadOnlySpan slice is outside the source buffer.");
            }
            return bytes.subspan(static_cast<std::size_t>(start), length);
        }

        template <typename T>
        T ReadStruct(ByteSpan bytes)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            T result;
            // Read.ReadStruct<T> pins bytes.ToArray() and asks Marshal.PtrToStructure<T>
            // to consume the native representation. It does not pre-check the span length.
            std::memcpy(&result, bytes.data(), sizeof(T));
            return result;
        }

        template <typename T>
        T DoOffset(ByteSpan bytes, std::uint32_t offset)
        {
            // Read.DoOffset<T> calls DoOffsets(..., count: 1). A null offset therefore
            // produces an empty list and its [0] access throws.
            if (offset == 0)
            {
                throw std::out_of_range("Read.DoOffset<T> received a null offset.");
            }
            std::int32_t ioffset = UInt32ToInt32(offset);
            return ReadStruct<T>(Slice(bytes, ioffset, static_cast<std::uint32_t>(sizeof(T))));
        }

        std::vector<std::uint32_t> DoListNullEnd(ByteSpan bytes, std::uint32_t offset)
        {
            std::vector<std::uint32_t> results;
            if (offset != 0)
            {
                std::int32_t ioffset = UInt32ToInt32(offset);
                for (;; ioffset = UncheckedAddUInt32(ioffset, sizeof(std::uint32_t)))
                {
                    std::uint32_t result = ReadStruct<std::uint32_t>(
                        Slice(bytes, ioffset, sizeof(std::uint32_t)));
                    if (result == 0)
                    {
                        break;
                    }
                    results.push_back(result);
                }
            }
            return results;
        }

        bool IsMarm(const FrontendHeader& header) noexcept
        {
            return std::string_view(header.Type, sizeof(header.Type)) == "MARM";
        }

        void WriteLine(std::uint32_t value)
        {
            char buffer[10];
            std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
            std::cout.write(buffer, result.ptr - buffer);
            std::cout.put('\n');
        }
    }

    void Frontend::Parse()
    {
        // todo: parse DP version (different header)
        // const char* path = R"(D:\Cdrv\MPH\_FS\amhe1\frontend\single_metroidhunters.bin)";
        const char* path = R"(D:\Cdrv\MPH\_FS\amhe1\frontend\metroidhunters.bin)";
        std::vector<std::uint8_t> storage = FileReadAllBytes(path);
        ByteSpan bytes(storage.data(), storage.size());
        FrontendHeader header = ReadStruct<FrontendHeader>(bytes);
        assert(IsMarm(header));
        std::vector<MenuStruct1> list1;
        for (std::uint32_t offset : DoListNullEnd(bytes, header.Offfset1))
        {
            MenuStruct1 item = DoOffset<MenuStruct1>(bytes, offset);
            assert(item.Offset1 == 0);
            list1.push_back(item);
            for (std::uint32_t subOffset : DoListNullEnd(bytes, item.Offset2))
            {
                WriteLine(subOffset);
                MenuStruct1A subItem = DoOffset<MenuStruct1A>(bytes, subOffset);
                assert(subItem.Offset1 == 0);
            }
        }
        Nop();
    }

    void Frontend::Nop()
    {
    }
}
