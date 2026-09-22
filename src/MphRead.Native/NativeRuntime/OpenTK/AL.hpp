#pragma once

// OpenTK.Audio.OpenAL.AL as the game calls it (OpenTK 4.9). Enum values are the
// OpenAL constants; each function is the OpenAL entry point of the same name,
// implemented in AL.cpp. On Android the name AL is the Mods.Sound.AlEs class,
// as the C# Android project aliases it.

#include <cstdint>
#include <span>

#if defined(__ANDROID__)
#include "../../Mods/Sound/AlEs.hpp"

namespace OpenTK::Audio::OpenAL
{
    using AL = ::MphRead::Mods::Sound::AlEs;
}
#else
namespace OpenTK::Audio::OpenAL
{
    enum class ALFormat : std::int32_t
    {
        Mono8 = 0x1100,
        Mono16 = 0x1101,
        Stereo8 = 0x1102,
        Stereo16 = 0x1103
    };

    enum class ALSourcef : std::int32_t
    {
        Gain = 0x100A
    };

    enum class ALGetSourcei : std::int32_t
    {
        SourceState = 0x1010,
        BuffersProcessed = 0x1016
    };

    enum class ALSourceState : std::int32_t
    {
        Initial = 0x1011,
        Playing = 0x1012,
        Paused = 0x1013,
        Stopped = 0x1014
    };

    namespace AL
    {
        [[nodiscard]] std::int32_t GenSource();
        void GenBuffers(std::span<std::int32_t> buffers);
        void DeleteSource(std::int32_t source);
        void DeleteBuffers(std::span<const std::int32_t> buffers);
        void Source(std::int32_t source, ALSourcef param, float value);
        [[nodiscard]] std::int32_t GetSource(std::int32_t source, ALGetSourcei param);
        void SourcePlay(std::int32_t source);
        void SourceStop(std::int32_t source);
        void SourceQueueBuffers(std::int32_t source, std::span<const std::int32_t> buffers);
        void SourceUnqueueBuffers(std::int32_t source, std::span<std::int32_t> buffers);
        void BufferData(std::int32_t buffer, ALFormat format,
            std::span<const std::int16_t> data, std::int32_t frequency);
    }
}
#endif
