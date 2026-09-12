#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace MphRead::Formats::Sound
{
    class SoundSample;

    struct FhSoundSampleHeader
    {
        const std::uint32_t DataSize = 0;
        const std::uint32_t DataPointer = 0; // always zero in the file
        const std::uint32_t SampleRate = 0;
        const std::uint16_t Volume = 0;
        const std::uint8_t FieldE = 0; // padding padding
        const std::uint8_t Format = 0;
        const std::uint32_t LoopStart = 0;
        const std::uint32_t LoopEnd = 0;
    };

    static_assert(std::is_standard_layout_v<FhSoundSampleHeader>);
    static_assert(sizeof(FhSoundSampleHeader) == 24);
    static_assert(alignof(FhSoundSampleHeader) == 4);
    static_assert(offsetof(FhSoundSampleHeader, DataSize) == 0);
    static_assert(offsetof(FhSoundSampleHeader, DataPointer) == 4);
    static_assert(offsetof(FhSoundSampleHeader, SampleRate) == 8);
    static_assert(offsetof(FhSoundSampleHeader, Volume) == 12);
    static_assert(offsetof(FhSoundSampleHeader, FieldE) == 14);
    static_assert(offsetof(FhSoundSampleHeader, Format) == 15);
    static_assert(offsetof(FhSoundSampleHeader, LoopStart) == 16);
    static_assert(offsetof(FhSoundSampleHeader, LoopEnd) == 20);

    class SoundRead final
    {
    public:
        SoundRead() = delete;

        static void ExportAllFh(bool adpcmRoundingError = false);
        static void ExportFhSfx(bool adpcmRoundingError = false);
        static void ExportFhBgm(bool adpcmRoundingError = false);
        static void ExportFhMenuSfx(bool adpcmRoundingError = false);
        static void ExportFhGlobalSfx(bool adpcmRoundingError = false);

        [[nodiscard]] static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhSfx();
        [[nodiscard]] static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhBgm();
        [[nodiscard]] static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhMenuSfx();
        [[nodiscard]] static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhGlobalSfx();
        [[nodiscard]] static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhSoundFile(
            const std::string& filename);
    };
}
