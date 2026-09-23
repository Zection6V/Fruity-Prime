// SoundFlow's MiniAudio backend, as the managed build uses it: an engine, one
// playback device with a master mixer, and a player that pulls float samples
// from a RawDataProvider.
//
// The device here is OpenAL, which the program already links for its sound
// effects. What SoundFlow is asked for in this program is exactly one thing --
// keep pulling from the provider and play what comes back -- and that is what
// this does; nothing of SoundFlow's wider API is reproduced.

#include "../../Sound/Music.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <AL/al.h>
#include <AL/alc.h>

namespace
{
    // How much sound is kept queued ahead of the device.
    constexpr std::int32_t BufferCount = 4;
    constexpr std::int32_t FramesPerBuffer = 2048;

    // One OpenAL context for the process, opened when the first device is.
    class AlContext final
    {
    public:
        static AlContext& Instance()
        {
            static AlContext instance;
            return instance;
        }

        [[nodiscard]] bool Ready() const noexcept { return _context != nullptr; }

    private:
        AlContext()
        {
            _device = ::alcOpenDevice(nullptr);
            if (_device == nullptr)
            {
                return;
            }
            _context = ::alcCreateContext(_device, nullptr);
            if (_context != nullptr)
            {
                ::alcMakeContextCurrent(_context);
            }
        }

        ~AlContext()
        {
            if (_context != nullptr)
            {
                ::alcMakeContextCurrent(nullptr);
                ::alcDestroyContext(_context);
            }
            if (_device != nullptr)
            {
                ::alcCloseDevice(_device);
            }
        }

        AlContext(const AlContext&) = delete;
        AlContext& operator=(const AlContext&) = delete;

        ALCdevice* _device = nullptr;
        ALCcontext* _context = nullptr;
    };
}

namespace SoundFlow::Components
{
    struct SoundPlayer::Impl final
    {
        Structs::AudioFormat Format{};
        std::shared_ptr<Providers::RawDataProvider> Provider;
        std::atomic<Enums::PlaybackState> State{Enums::PlaybackState::Stopped};
        std::atomic<bool> Disposed{false};
        std::thread Worker;
        ALuint Source = 0;
        ALuint Buffers[BufferCount]{};

        void Run()
        {
            const std::int32_t channels = Format.Channels > 0 ? Format.Channels : 2;
            const std::int32_t rate = Format.SampleRate > 0 ? Format.SampleRate : 48000;
            std::vector<std::uint8_t> raw(
                static_cast<std::size_t>(FramesPerBuffer) * channels * sizeof(float));
            std::vector<std::int16_t> samples(
                static_cast<std::size_t>(FramesPerBuffer) * channels);
            std::int32_t queued = 0;

            while (!Disposed.load(std::memory_order_relaxed))
            {
                if (State.load(std::memory_order_relaxed) != Enums::PlaybackState::Playing)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }

                ALint processed = 0;
                ::alGetSourcei(Source, AL_BUFFERS_PROCESSED, &processed);
                while (processed-- > 0)
                {
                    ALuint done = 0;
                    ::alSourceUnqueueBuffers(Source, 1, &done);
                    --queued;
                }

                if (queued >= BufferCount)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }

                const std::int32_t read = Provider->Read(
                    std::span<std::uint8_t>(raw.data(), raw.size()), 0,
                    static_cast<std::int32_t>(raw.size()));
                if (read <= 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }

                // The provider hands back 32-bit floats; OpenAL takes 16-bit
                // samples.
                const std::size_t count
                    = static_cast<std::size_t>(read) / sizeof(float);
                const auto* const floats = reinterpret_cast<const float*>(raw.data());
                for (std::size_t i = 0; i < count; ++i)
                {
                    const float clamped = std::clamp(floats[i], -1.0F, 1.0F);
                    samples[i] = static_cast<std::int16_t>(clamped * 32767.0F);
                }

                const ALuint buffer = Buffers[static_cast<std::size_t>(queued) % BufferCount];
                ::alBufferData(buffer, channels >= 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16,
                    samples.data(), static_cast<ALsizei>(count * sizeof(std::int16_t)), rate);
                ::alSourceQueueBuffers(Source, 1, &buffer);
                ++queued;

                ALint state = 0;
                ::alGetSourcei(Source, AL_SOURCE_STATE, &state);
                if (state != AL_PLAYING)
                {
                    ::alSourcePlay(Source);
                }
            }
        }
    };

    SoundPlayer::SoundPlayer(
        const std::shared_ptr<Backends::MiniAudio::MiniAudioEngine>& engine,
        const Structs::AudioFormat& format,
        const std::shared_ptr<Providers::RawDataProvider>& provider)
        : _impl(std::make_shared<Impl>())
    {
        (void)engine;
        _impl->Format = format;
        _impl->Provider = provider;
        if (AlContext::Instance().Ready())
        {
            ::alGenSources(1, &_impl->Source);
            ::alGenBuffers(BufferCount, _impl->Buffers);
        }
        _impl->Worker = std::thread([impl = _impl]() { impl->Run(); });
    }

    SoundPlayer::~SoundPlayer()
    {
        Dispose();
    }

    void SoundPlayer::Play()
    {
        _impl->State.store(Enums::PlaybackState::Playing, std::memory_order_relaxed);
    }

    void SoundPlayer::Pause()
    {
        _impl->State.store(Enums::PlaybackState::Paused, std::memory_order_relaxed);
        if (_impl->Source != 0)
        {
            ::alSourcePause(_impl->Source);
        }
    }

    void SoundPlayer::Stop()
    {
        _impl->State.store(Enums::PlaybackState::Stopped, std::memory_order_relaxed);
        if (_impl->Source != 0)
        {
            ::alSourceStop(_impl->Source);
        }
    }

    void SoundPlayer::Dispose() noexcept
    {
        if (_impl == nullptr || _impl->Disposed.exchange(true))
        {
            return;
        }
        if (_impl->Worker.joinable())
        {
            _impl->Worker.join();
        }
        if (_impl->Source != 0)
        {
            ::alSourceStop(_impl->Source);
            ::alDeleteSources(1, &_impl->Source);
            ::alDeleteBuffers(BufferCount, _impl->Buffers);
            _impl->Source = 0;
        }
    }

    Enums::PlaybackState SoundPlayer::State() const noexcept
    {
        return _impl->State.load(std::memory_order_relaxed);
    }
}

namespace SoundFlow::Abstracts::Devices
{
    struct AudioPlaybackDevice::Impl final
    {
        std::atomic<bool> Started{false};
    };

    AudioPlaybackDevice::AudioPlaybackDevice()
        : _impl(std::make_shared<Impl>())
    {
    }

    AudioPlaybackDevice::~AudioPlaybackDevice() = default;

    void AudioPlaybackDevice::Start()
    {
        _impl->Started.store(true, std::memory_order_relaxed);
    }

    void AudioPlaybackDevice::Stop()
    {
        _impl->Started.store(false, std::memory_order_relaxed);
    }
}

namespace SoundFlow::Backends::MiniAudio
{
    struct MiniAudioEngine::Impl final
    {
        std::shared_ptr<Abstracts::Devices::AudioPlaybackDevice> Device;
    };

    MiniAudioEngine::MiniAudioEngine()
        : _impl(std::make_shared<Impl>())
    {
        // Opening the device here is what makes InitializePlaybackDevice able
        // to hand one back.
        (void)AlContext::Instance().Ready();
    }

    MiniAudioEngine::~MiniAudioEngine() = default;

    std::shared_ptr<Abstracts::Devices::AudioPlaybackDevice>
        MiniAudioEngine::InitializePlaybackDevice(
            const void* deviceInfo, const Structs::AudioFormat& format)
    {
        (void)deviceInfo;
        (void)format;
        if (_impl->Device == nullptr)
        {
            _impl->Device = std::make_shared<Abstracts::Devices::AudioPlaybackDevice>();
        }
        return _impl->Device;
    }
}
