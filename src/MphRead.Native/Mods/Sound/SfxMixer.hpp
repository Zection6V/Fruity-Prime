#pragma once

#if defined(__ANDROID__)
#include "AlEs.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace SoundFlow::Components
{
    class SoundPlayer;
}

namespace SoundFlow::Providers
{
    class RawDataProvider;
}

namespace MphRead::Mods::Sound
{
    class SfxMixer final
    {
    public:
        SfxMixer() = delete;

        static bool Open();
        static void Close();

        static std::int32_t NewBuffer();
        static void DeleteBuffer(std::int32_t id);
        static std::int32_t NewSource();
        static void DeleteSource(std::int32_t id);

        static void FillBuffer(std::int32_t id, OpenTK::Audio::OpenAL::ALFormat format,
            std::span<const std::uint8_t> data, std::int32_t sampleRate);
        static void SetLoopPoints(std::int32_t id, std::int32_t start, std::int32_t end);

        static void SetSource(std::int32_t id, OpenTK::Audio::OpenAL::ALSourceb param, bool value);
        static void SetSource(std::int32_t id, OpenTK::Audio::OpenAL::ALSourcei param,
            std::int32_t value);
        static void SetSource(std::int32_t id, OpenTK::Audio::OpenAL::ALSourcef param, float value);
        static void SetSource(std::int32_t id, OpenTK::Audio::OpenAL::ALSource3f param,
            OpenTK::Mathematics::Vector3 value);

        static void Play(std::int32_t id);
        static void Pause(std::int32_t id);
        static void StopSource(std::int32_t id);
        static void QueueBuffers(std::int32_t id, std::span<std::int32_t> buffers);
        static void UnqueueBuffers(std::int32_t id, std::span<std::int32_t> buffers);
        static std::int32_t GetSource(std::int32_t id, OpenTK::Audio::OpenAL::ALGetSourcei param);

        static void SetListener(OpenTK::Audio::OpenAL::ALListener3f param,
            OpenTK::Mathematics::Vector3 value);
        static void SetListenerOrientation(OpenTK::Mathematics::Vector3 facing,
            OpenTK::Mathematics::Vector3 up);

    private:
        struct Buffer;
        struct Voice;
        struct VoiceEntry;
        class FloatSpan;
        class MixStream;

        static Voice* FindVoice(std::int32_t id) noexcept;
        static void Mix(FloatSpan output);
        static void MixVoice(Voice& voice, FloatSpan output);
        static void Sample(const Buffer& buffer, double cursor, float& left, float& right);
        static float Lerp(float a, float b, float t);
        static void Placement(const Voice& voice, float& left, float& right);

        static std::recursive_mutex _lock;
        static std::map<std::int32_t, std::shared_ptr<Buffer>> _buffers;
        static std::vector<VoiceEntry> _voices;
        static std::ptrdiff_t _freeVoiceHead;
        static std::int32_t _nextBuffer;
        static std::int32_t _nextSource;

        static OpenTK::Mathematics::Vector3 _listenerPosition;
        static OpenTK::Mathematics::Vector3 _listenerFacing;
        static OpenTK::Mathematics::Vector3 _listenerUp;

        static std::shared_ptr<MixStream> _stream;
        static std::shared_ptr<SoundFlow::Providers::RawDataProvider> _provider;
        static std::shared_ptr<SoundFlow::Components::SoundPlayer> _player;
        static std::atomic<bool> _unavailable;
        static std::int32_t _outputRate;
    };
}
#endif
