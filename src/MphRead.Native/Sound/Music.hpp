#pragma once

#include "../Formats/Enums.hpp"
#include "../Metadata/SoundMeta.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace NCSF123
{
    enum class VolumeType : std::int32_t
    {
        ReplayGainAlbum = 0
    };
}

namespace NCSF123::NCSFCommon
{
    struct Track
    {
        std::uint8_t Volume = 127;
        bool Mute = false;
    };
}

namespace SoundFlow::Enums
{
    enum class SampleFormat : std::int32_t
    {
        F32 = 0
    };

    enum class PlaybackState : std::int32_t
    {
        Stopped = 0,
        Playing = 1,
        Paused = 2
    };
}

namespace SoundFlow::Structs
{
    struct AudioFormat
    {
        std::int32_t SampleRate = 0;
        std::int32_t Channels = 0;
        Enums::SampleFormat Format = Enums::SampleFormat::F32;
    };
}

namespace SoundFlow::Providers
{
    class RawDataProvider final
    {
    public:
        template <typename TStream>
        RawDataProvider(std::shared_ptr<TStream> stream, Enums::SampleFormat format, std::int32_t sampleRate)
            : _owner(std::move(stream)), _format(format), _sampleRate(sampleRate)
        {
            std::shared_ptr<TStream> typed = std::static_pointer_cast<TStream>(_owner);
            _read = [typed](std::span<std::uint8_t> buffer, std::int32_t offset, std::int32_t count)
            {
                return typed->Read(buffer, offset, count);
            };
        }

        std::int32_t Read(std::span<std::uint8_t> buffer, std::int32_t offset, std::int32_t count);
        void Dispose() noexcept;

    private:
        std::shared_ptr<void> _owner;
        std::function<std::int32_t(std::span<std::uint8_t>, std::int32_t, std::int32_t)> _read;
        Enums::SampleFormat _format;
        std::int32_t _sampleRate;
        std::atomic<bool> _disposed{false};
    };
}

namespace SoundFlow::Backends::MiniAudio
{
    class MiniAudioEngine;
}

namespace SoundFlow::Components
{
    class SoundPlayer;

    class Mixer final
    {
    public:
        void AddComponent(const std::shared_ptr<SoundPlayer>& player);
        void RemoveComponent(const std::shared_ptr<SoundPlayer>& player);

    private:
        mutable std::mutex _mutex;
        std::vector<std::shared_ptr<SoundPlayer>> _components;
    };

    class SoundPlayer final
    {
    public:
        SoundPlayer(const std::shared_ptr<Backends::MiniAudio::MiniAudioEngine>& engine,
            const Structs::AudioFormat& format, const std::shared_ptr<Providers::RawDataProvider>& provider);
        ~SoundPlayer();

        void Play();
        void Pause();
        void Stop();
        void Dispose() noexcept;
        [[nodiscard]] Enums::PlaybackState State() const noexcept;

    private:
        struct Impl;
        std::shared_ptr<Impl> _impl;
    };
}

namespace SoundFlow::Abstracts::Devices
{
    class AudioPlaybackDevice final
    {
    public:
        AudioPlaybackDevice();
        ~AudioPlaybackDevice();
        void Start();
        void Stop();
        Components::Mixer MasterMixer;

    private:
        struct Impl;
        std::shared_ptr<Impl> _impl;
    };
}

namespace SoundFlow::Backends::MiniAudio
{
    class MiniAudioEngine final
    {
    public:
        MiniAudioEngine();
        ~MiniAudioEngine();
        std::shared_ptr<Abstracts::Devices::AudioPlaybackDevice> InitializePlaybackDevice(
            const void* deviceInfo, const Structs::AudioFormat& format);

    private:
        struct Impl;
        std::shared_ptr<Impl> _impl;
    };
}

namespace MphRead
{
    class Music final
    {
    public:
        Music() = delete;

        [[nodiscard]] static float UserVolume() noexcept;
        static void UserVolume(float value) noexcept;
        static void SetUserVolume(float volume);
        [[nodiscard]] static float MusicVolume() noexcept;
        static void MusicVolume(float value) noexcept;
        [[nodiscard]] static float Volume() noexcept;
        [[nodiscard]] static bool IsPaused() noexcept;
        [[nodiscard]] static std::int32_t MusicEncounterSuspension() noexcept;
        [[nodiscard]] static MusicId MusicToResume() noexcept;
        static void MusicToResume(MusicId value) noexcept;

        static void Init();
        static void PlayMusic(MusicId musicId, std::optional<std::uint16_t> tracks = std::nullopt,
            bool toggleOnTracks = false, bool toggleOffTracks = false);
        static void TryPlayRoomMusic(std::int32_t roomId, std::int32_t track);
        static void PlayRoomMusic(std::int32_t roomId, std::int32_t track);
        static void PlaySeq(SeqId seqId, bool notReady = true);
        static void PlaySeq(SeqId seqId, std::uint16_t tracks, bool queue = false, bool notReady = false,
            std::uint16_t fadeOutFrames = 0, std::uint16_t fadeInFrames = 0);
        static void UpdateMusic();
        static void UpdateMusicIdIfPaused(MusicId musicId);
        static void UpdateEscapeMusic();
        static void UpdateEventMusic(float time);
        static void PlayEncounterMusic(Hunter hunter);
        static void UpdateEncounterMusic(std::int32_t clearId);
        static void PlayPausedMusic();
        static void Pause();
        static void Stop(float fadeTime = 0.0F);
        static void FadeVolume(float volume, float time, bool stopAfterFade = false);
        static void UpdateTempo(std::uint16_t tempo, float time);

    private:
        static void SwitchEscapeMusicIfNeeded();
        static void ProcessVolume();
        static void ProcessTempo();
        static void UpdateTrackVolume(std::uint16_t tracks, std::uint8_t volume);
        static void SetTrackFaders(std::uint16_t tracks, std::uint8_t target, float time);
        static void ProcessTrackFaders();
    };

    class MusicPlayer final
    {
    public:
        MusicPlayer() = delete;

        [[nodiscard]] static bool Available() noexcept;
        [[nodiscard]] static std::shared_ptr<SoundFlow::Backends::MiniAudio::MiniAudioEngine> Engine() noexcept;
        [[nodiscard]] static std::shared_ptr<SoundFlow::Abstracts::Devices::AudioPlaybackDevice> PlaybackDevice() noexcept;
        [[nodiscard]] static SoundFlow::Structs::AudioFormat Format() noexcept;
        [[nodiscard]] static bool Loading() noexcept;
        [[nodiscard]] static bool StopLoading() noexcept;
        static void StopLoading(bool value) noexcept;

        static void Load(SeqId seqId, std::uint16_t tracks = UINT16_MAX, float volume = 1.0F);
        static void WaitForLoad(std::int32_t sleepMs = 100);
        static void Play(float volume);
        static void Pause();
        [[nodiscard]] static SoundFlow::Enums::PlaybackState State() noexcept;
        [[nodiscard]] static float Volume() noexcept;
        static void Volume(float value) noexcept;
        [[nodiscard]] static std::uint16_t Tempo() noexcept;
        static void Tempo(std::uint16_t value) noexcept;
        [[nodiscard]] static NCSF123::NCSFCommon::Track* GetTrack(std::int32_t index) noexcept;
        static void Stop();
        static void Remove(bool shutdown = false);
    };
}
