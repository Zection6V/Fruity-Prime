#pragma once

#if defined(__ANDROID__)
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace System
{
    class DllNotFoundException final : public std::runtime_error
    {
    public:
        explicit DllNotFoundException(const char* message)
            : std::runtime_error(message)
        {
        }
    };
}

namespace OpenTK::Mathematics
{
    struct Vector3
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;

        Vector3() = default;
        Vector3(float x, float y, float z)
            : X(x), Y(y), Z(z)
        {
        }
    };
}

namespace OpenTK::Audio::OpenAL
{
    enum class ALFormat : std::int32_t
    {
        Mono8 = 0x1100,
        Mono16 = 0x1101,
        Stereo8 = 0x1102,
        Stereo16 = 0x1103
    };

    enum class ALSourceb : std::int32_t
    {
        SourceRelative = 0x202,
        Looping = 0x1007
    };

    enum class ALSourcei : std::int32_t
    {
        Buffer = 0x1009
    };

    enum class ALSourcef : std::int32_t
    {
        ReferenceDistance = 0x1020,
        MaxDistance = 0x1023,
        RolloffFactor = 0x1021,
        Pitch = 0x1003,
        Gain = 0x100A
    };

    enum class ALSource3f : std::int32_t
    {
        Position = 0x1004
    };

    enum class ALGetSourcei : std::int32_t
    {
        Buffer = 0x1009,
        SourceState = 0x1010,
        BuffersQueued = 0x1015,
        BuffersProcessed = 0x1016
    };

    enum class ALListener3f : std::int32_t
    {
        Position = 0x1004
    };

    enum class ALListenerfv : std::int32_t
    {
        Orientation = 0x100F
    };

    enum class ALDistanceModel : std::int32_t
    {
        LinearDistanceClamped = 0xD004
    };

    enum class ALError : std::int32_t
    {
        NoError = 0
    };

    enum class BufferLoopPoint : std::int32_t
    {
        LoopPointsSOFT = 0x2015
    };

    enum class AlcError : std::int32_t
    {
        NoError = 0
    };

    struct ALDevice
    {
        static const ALDevice Null;

        std::intptr_t Handle = 0;

        ALDevice() = default;
        explicit ALDevice(std::intptr_t handle)
            : Handle(handle)
        {
        }
    };

    struct ALContext
    {
        static const ALContext Null;

        std::intptr_t Handle = 0;

        ALContext() = default;
        explicit ALContext(std::intptr_t handle)
            : Handle(handle)
        {
        }
    };

    class ALContextAttributes
    {
    public:
        ALContextAttributes() = default;
    };
}

namespace MphRead::Mods::Sound
{
    class AlEs final
    {
    public:
        AlEs() = delete;

        static void GenBuffers(std::span<std::int32_t> buffers);
        static std::int32_t GenBuffer();
        static void DeleteBuffers(std::span<std::int32_t> buffers);
        static void DeleteBuffer(std::int32_t buffer);

        static void GenSources(std::span<std::int32_t> sources);
        static std::int32_t GenSource();
        static void DeleteSources(std::span<std::int32_t> sources);
        static void DeleteSource(std::int32_t source);

        template <typename T>
            requires std::is_trivially_copyable_v<T>
        static void BufferData(std::int32_t buffer, OpenTK::Audio::OpenAL::ALFormat format,
            const std::vector<T>* data, std::int32_t sampleRate)
        {
            if (data == nullptr)
            {
                BufferDataBytes(buffer, format, {}, sampleRate);
                return;
            }
            BufferData(buffer, format, std::span<const T>(*data), sampleRate);
        }

        template <typename T>
            requires std::is_trivially_copyable_v<T>
        static void BufferData(std::int32_t buffer, OpenTK::Audio::OpenAL::ALFormat format,
            std::span<const T> data, std::int32_t sampleRate)
        {
            const std::span<const std::byte> bytes = std::as_bytes(data);
            BufferDataBytes(buffer, format,
                std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()),
                sampleRate);
        }

        static void Source(std::int32_t source, OpenTK::Audio::OpenAL::ALSourceb param, bool value);
        static void Source(std::int32_t source, OpenTK::Audio::OpenAL::ALSourcei param, std::int32_t value);
        static void Source(std::int32_t source, OpenTK::Audio::OpenAL::ALSourcef param, float value);
        static void Source(std::int32_t source, OpenTK::Audio::OpenAL::ALSource3f param,
            OpenTK::Mathematics::Vector3& value);
        static void Source(std::int32_t source, OpenTK::Audio::OpenAL::ALSource3f param,
            float x, float y, float z);

        static void SourcePlay(std::int32_t source);
        static void SourceStop(std::int32_t source);
        static void SourcePause(std::int32_t source);
        static void SourceQueueBuffers(std::int32_t source, std::span<std::int32_t> buffers);
        static void SourceUnqueueBuffers(std::int32_t source, std::span<std::int32_t> buffers);

        static void GetSource(std::int32_t source, OpenTK::Audio::OpenAL::ALGetSourcei param,
            std::int32_t& value);
        static std::int32_t GetSource(
            std::int32_t source, OpenTK::Audio::OpenAL::ALGetSourcei param);

        static void Listener(OpenTK::Audio::OpenAL::ALListener3f param,
            OpenTK::Mathematics::Vector3& value);
        static void Listener(OpenTK::Audio::OpenAL::ALListenerfv param,
            OpenTK::Mathematics::Vector3& at, OpenTK::Mathematics::Vector3& up);

        static void DistanceModel(OpenTK::Audio::OpenAL::ALDistanceModel model);
        static OpenTK::Audio::OpenAL::ALError GetError();

        class LoopPoints final
        {
        public:
            LoopPoints() = delete;

            static bool IsExtensionPresent();
            static void Buffer(std::int32_t buffer, OpenTK::Audio::OpenAL::BufferLoopPoint param,
                std::int32_t start, std::int32_t end);
        };

    private:
        static void BufferDataBytes(std::int32_t buffer, OpenTK::Audio::OpenAL::ALFormat format,
            std::span<const std::uint8_t> data, std::int32_t sampleRate);
    };

    class AlcEs final
    {
    public:
        AlcEs() = delete;

        static OpenTK::Audio::OpenAL::ALDevice OpenDevice(
            std::optional<std::string_view> deviceName);
        static OpenTK::Audio::OpenAL::ALContext CreateContext(
            OpenTK::Audio::OpenAL::ALDevice device,
            const OpenTK::Audio::OpenAL::ALContextAttributes* attributes);
        static bool MakeContextCurrent(OpenTK::Audio::OpenAL::ALContext context);
        static bool DestroyContext(OpenTK::Audio::OpenAL::ALContext context);
        static bool CloseDevice(OpenTK::Audio::OpenAL::ALDevice device);
        static OpenTK::Audio::OpenAL::AlcError GetError(OpenTK::Audio::OpenAL::ALDevice device);

    private:
        static const std::intptr_t _handle;
    };
}
#endif
