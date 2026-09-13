#pragma once

#include "TagList.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace NCSFCommon
{
    class NCSF final
    {
    public:
        NCSF() = delete;

        static void MakeNCSF(
            std::u16string_view filename,
            std::span<const std::uint8_t> reservedSectionData,
            std::span<const std::uint8_t> programSectionData,
            const TagList* tags = nullptr);

        static void CheckForValidPSF(std::span<const std::uint8_t> span, std::uint8_t versionByte);

        [[nodiscard]] static std::vector<std::uint8_t> GetProgramSectionFromPSF(
            std::span<const std::uint8_t> span,
            std::uint8_t versionByte,
            std::uint32_t programHeaderSize,
            std::uint32_t programSectionOffset,
            bool addHeaderSize = false);

        [[nodiscard]] static std::optional<std::int32_t> FindOffsetsInFile(
            std::span<const std::uint8_t> search,
            std::span<const std::uint8_t> memory);

        [[nodiscard]] static TagList GetTagsFromPSF(
            std::span<const std::uint8_t> memory,
            std::uint8_t versionByte);

        [[nodiscard]] static std::int16_t ConvertScale(std::int32_t scale);

    private:
        enum class EncodingKind : std::uint8_t
        {
            SystemCodePage,
            Utf8
        };

        [[nodiscard]] static TagList GetTagsFromPSFWithEncoding(
            std::span<const std::uint8_t> span,
            EncodingKind encoding);

        static constexpr std::array<std::uint8_t, 3> PSFHeader{ 'P', 'S', 'F' };
        static constexpr std::array<std::uint8_t, 5> TAGHeader{ '[', 'T', 'A', 'G', ']' };
        static const std::uint32_t SystemCodePageEncoding;
        static constexpr std::array<std::int16_t, 128> convertScaleLookupTable
        {
            -32768, -421, -361, -325, -300, -281, -265, -252,
            -240, -230, -221, -212, -205, -198, -192, -186,
            -180, -175, -170, -165, -161, -156, -152, -148,
            -145, -141, -138, -134, -131, -128, -125, -122,
            -120, -117, -114, -112, -110, -107, -105, -103,
            -100, -98, -96, -94, -92, -90, -88, -86,
            -85, -83, -81, -79, -78, -76, -74, -73,
            -71, -70, -68, -67, -65, -64, -62, -61,
            -60, -58, -57, -56, -54, -53, -52, -51,
            -49, -48, -47, -46, -45, -43, -42, -41,
            -40, -39, -38, -37, -36, -35, -34, -33,
            -32, -31, -30, -29, -28, -27, -26, -25,
            -24, -23, -23, -22, -21, -20, -19, -18,
            -17, -17, -16, -15, -14, -13, -12, -12,
            -11, -10, -9, -9, -8, -7, -6, -6,
            -5, -4, -3, -3, -2, -1, -1, 0
        };
    };
}
