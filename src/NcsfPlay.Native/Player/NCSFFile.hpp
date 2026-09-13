#pragma once

#include "../NCSF.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace NCSF123
{
    enum class VolumeType : std::int32_t
    {
        None = 0,
        Volume = 1,
        ReplayGainTrack = 2,
        ReplayGainAlbum = 3
    };

    enum class PeakType : std::int32_t
    {
        None = 0,
        ReplayGainTrack = 1,
        ReplayGainAlbum = 2
    };

    class NCSFFile
    {
    public:
        explicit NCSFFile(std::u16string path);

        NCSFFile(const NCSFFile&) = delete;
        NCSFFile& operator=(const NCSFFile&) = delete;
        NCSFFile(NCSFFile&&) noexcept = default;
        NCSFFile& operator=(NCSFFile&&) noexcept = default;

        [[nodiscard]] const std::u16string& FilePath() const noexcept;
        [[nodiscard]] std::span<const std::uint8_t> ProgramSection() const noexcept;
        [[nodiscard]] std::span<const std::uint8_t> ReservedSection() const noexcept;
        [[nodiscard]] NCSFCommon::TagList& Tags() noexcept;
        [[nodiscard]] const NCSFCommon::TagList& Tags() const noexcept;

        [[nodiscard]] std::int32_t GetFadeMS(std::int32_t defaultFade) const;
        [[nodiscard]] std::int32_t GetLengthMS(std::int32_t defaultLength) const;
        [[nodiscard]] float GetVolume(VolumeType preferredVolumeType, PeakType preferredPeakType) const;

    private:
        static constexpr std::int32_t VersionByte = 0x25;
        static constexpr std::int32_t ProgramSizeOffset = 8;
        static constexpr std::int32_t ProgramHeaderSize = 12;

        void ReadNCSF(std::u16string_view path, bool readTagsOnly = false);

        std::vector<std::uint8_t> _rawData;
        std::vector<std::uint8_t> _reservedSection;
        std::vector<std::uint8_t> _programSection;
        std::u16string _filePath;
        NCSFCommon::TagList _tags;
    };
}
