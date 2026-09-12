#include "Music.hpp"

#include "../Formats/Sound.hpp"
#include "../GameState.hpp"
#include "../Paths.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

namespace NCSFPlayer
{
    enum class Interpolation : std::int32_t { None = 0 };
    enum class PeakType : std::int32_t { ReplayGainTrack = 0 };

    class PlayerState final
    {
    public:
        [[nodiscard]] std::uint16_t TempoRatio() const noexcept;
        void TempoRatio(std::uint16_t value) noexcept;
        [[nodiscard]] NCSF123::NCSFCommon::Track* GetTrack(std::int32_t index) noexcept;
    };

    class NCSFPlayerStream final
    {
    public:
        NCSFPlayerStream(const std::string& path, std::uint32_t sampleRate, Interpolation interpolation,
            std::int32_t skipSilenceOnStartSec, std::int32_t defaultLengthInMS, std::int32_t defaultFadeInMS,
            NCSF123::VolumeType volumeType, PeakType peakType, bool playForever, float volume,
            std::uint16_t channelMutes, std::uint16_t trackMutes, bool ignoreVolume);
        ~NCSFPlayerStream();

        std::int32_t Read(std::span<std::uint8_t> buffer, std::int32_t offset, std::int32_t count);
        [[nodiscard]] float VolumeModification() const noexcept;
        void VolumeModification(float value) noexcept;
        [[nodiscard]] PlayerState& Player() noexcept;
        void Dispose() noexcept;
    };
}

namespace SoundFlow::Providers
{
    std::int32_t RawDataProvider::Read(std::span<std::uint8_t> buffer, std::int32_t offset, std::int32_t count)
    {
        if (_disposed.load(std::memory_order_acquire) || !_read)
        {
            return 0;
        }
        return _read(buffer, offset, count);
    }

    void RawDataProvider::Dispose() noexcept
    {
        _disposed.store(true, std::memory_order_release);
        _read = {};
        _owner.reset();
    }
}

namespace SoundFlow::Components
{
    void Mixer::AddComponent(const std::shared_ptr<SoundPlayer>& player)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _components.push_back(player);
    }

    void Mixer::RemoveComponent(const std::shared_ptr<SoundPlayer>& player)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        const auto it = std::find(_components.begin(), _components.end(), player);
        if (it != _components.end())
        {
            _components.erase(it);
        }
    }
}

namespace MphRead
{
    using Formats::Sound::MusicTrack;
    using Formats::Sound::RoomMusic;
    using Formats::Sound::SoundRead;
    using NCSFPlayer::NCSFPlayerStream;
    using SoundFlow::Abstracts::Devices::AudioPlaybackDevice;
    using SoundFlow::Backends::MiniAudio::MiniAudioEngine;
    using SoundFlow::Components::SoundPlayer;
    using SoundFlow::Enums::PlaybackState;
    using SoundFlow::Enums::SampleFormat;
    using SoundFlow::Providers::RawDataProvider;
    using SoundFlow::Structs::AudioFormat;

    namespace
    {
        class DotNetStopwatch final
        {
        public:
            void Reset() noexcept
            {
                _running = false;
                _elapsed = std::chrono::steady_clock::duration::zero();
            }

            void Restart() noexcept
            {
                _elapsed = std::chrono::steady_clock::duration::zero();
                _start = std::chrono::steady_clock::now();
                _running = true;
            }

            void Stop() noexcept
            {
                if (_running)
                {
                    _elapsed += std::chrono::steady_clock::now() - _start;
                    _running = false;
                }
            }

            [[nodiscard]] bool IsRunning() const noexcept { return _running; }

            [[nodiscard]] std::int64_t ElapsedMilliseconds() const noexcept
            {
                auto value = _elapsed;
                if (_running)
                {
                    value += std::chrono::steady_clock::now() - _start;
                }
                return std::chrono::duration_cast<std::chrono::milliseconds>(value).count();
            }

        private:
            bool _running = false;
            std::chrono::steady_clock::time_point _start{};
            std::chrono::steady_clock::duration _elapsed{};
        };

        struct TrackFaderState
        {
            std::uint8_t Start = 127;
            std::uint8_t Target = 127;
            float TimeMs = 0.0F;
            DotNetStopwatch Timer;

            void Reset(std::uint8_t volume = 127) noexcept
            {
                Start = volume;
                Target = volume;
                TimeMs = 0.0F;
                Timer.Reset();
            }
        };

        float ClampFloat(float value, float minimum, float maximum)
        {
            if (minimum > maximum)
            {
                throw std::invalid_argument("min");
            }
            if (value < minimum) return minimum;
            if (value > maximum) return maximum;
            return value;
        }

        std::int32_t ClampInt(std::int32_t value, std::int32_t minimum, std::int32_t maximum)
        {
            return std::min(std::max(value, minimum), maximum);
        }

        float g_userVolume = 1.0F;
        float g_musicVolume = 1.0F;
        std::vector<MusicTrack> g_musicInfo;
        std::vector<RoomMusic> g_roomMusic;
        bool g_musicInfoInitialized = false;
        bool g_roomMusicInitialized = false;
        bool g_playing = false;
        bool g_paused = false;
        bool g_nextTrackNotReady = false;
        bool g_isReady = false;
        bool g_musicQueued = false;
        std::uint16_t g_nextFadeInFrames = 0;
        MusicId g_currentMusicId = MusicId::None;
        SeqId g_currentMusicSeq = SeqId::None;
        SeqId g_nextMusicSeq = SeqId::None;
        std::int32_t g_musicEncounterSuspension = 0;
        MusicId g_musicToResume = MusicId::None;
        std::uint16_t g_nextTracks = 0;
        std::uint16_t g_pendingTracks = 0;
        std::uint16_t g_activeTracks = 0;
        std::uint16_t g_mutedTracks = 0;
        std::uint16_t g_fadingTracks = 0;

        float g_volumeFadeStart = 1.0F;
        float g_volumeFadeTarget = 1.0F;
        float g_volumeFadeTimeMs = 0.0F;
        DotNetStopwatch g_volumeFadeTimer;
        bool g_stopAfterFade = false;

        std::uint16_t g_baseTempo = 256;
        std::uint16_t g_tempoUpdateStart = 256;
        std::uint16_t g_tempoUpdateTarget = 256;
        float g_tempoUpdateTimeMs = 0.0F;
        DotNetStopwatch g_tempoUpdateTimer;

        std::array<TrackFaderState, 16> g_trackFaders{};
        constexpr std::array<std::int32_t, 8> g_hunterMusicTracks{16, 4, 1, 0, 2, 5, 3, 16};

        std::once_flag g_playerInitFlag;
        std::shared_ptr<MiniAudioEngine> g_audioEngine;
        std::shared_ptr<AudioPlaybackDevice> g_playbackDevice;
        const AudioFormat g_format{32728, 2, SampleFormat::F32};
        std::shared_ptr<RawDataProvider> g_provider;
        std::shared_ptr<NCSFPlayerStream> g_stream;
        std::shared_ptr<SoundPlayer> g_player;
        std::atomic<bool> g_available{false};
        std::atomic<bool> g_loading{false};
        std::atomic<bool> g_stopLoading{false};
        std::recursive_mutex g_playerMutex;
        constexpr std::int32_t SampleRate = 32728;

        const std::vector<MusicTrack>& MusicInfo()
        {
            if (!g_musicInfoInitialized)
            {
                throw std::runtime_error("Object reference not set to an instance of an object.");
            }
            return g_musicInfo;
        }

        const std::vector<RoomMusic>& RoomMusicInfo()
        {
            if (!g_roomMusicInitialized)
            {
                throw std::runtime_error("Object reference not set to an instance of an object.");
            }
            return g_roomMusic;
        }

        void EnsureMusicPlayerInitialized() noexcept
        {
            std::call_once(g_playerInitFlag, []
            {
                try
                {
                    g_audioEngine = std::make_shared<MiniAudioEngine>();
                    g_playbackDevice = g_audioEngine->InitializePlaybackDevice(nullptr, g_format);
                    g_available.store(true, std::memory_order_release);
                }
                catch (const std::exception& ex)
                {
                    std::cout << "[sound] no audio device (" << ex.what() << "); continuing without sound\n";
                    g_audioEngine.reset();
                    g_playbackDevice.reset();
                    g_available.store(false, std::memory_order_release);
                }
                catch (...)
                {
                    std::cout << "[sound] no audio device (unknown error); continuing without sound\n";
                    g_audioEngine.reset();
                    g_playbackDevice.reset();
                    g_available.store(false, std::memory_order_release);
                }
            });
        }
    }

    float Music::UserVolume() noexcept { return g_userVolume; }
    void Music::UserVolume(float value) noexcept { g_userVolume = value; }

    void Music::SetUserVolume(float volume)
    {
        g_userVolume = ClampFloat(volume, 0.0F, 1.0F);
        if (MusicPlayer::State() != PlaybackState::Stopped)
        {
            MusicPlayer::Volume(Volume());
        }
    }

    float Music::MusicVolume() noexcept { return g_musicVolume; }
    void Music::MusicVolume(float value) noexcept { g_musicVolume = value; }
    float Music::Volume() noexcept { return g_userVolume * g_musicVolume; }
    bool Music::IsPaused() noexcept { return g_paused; }
    std::int32_t Music::MusicEncounterSuspension() noexcept { return g_musicEncounterSuspension; }
    MusicId Music::MusicToResume() noexcept { return g_musicToResume; }
    void Music::MusicToResume(MusicId value) noexcept { g_musicToResume = value; }

    void Music::Init()
    {
        g_musicInfo = SoundRead::ReadInterMusicInfo();
        g_musicInfoInitialized = true;
        g_roomMusic = SoundRead::ReadAssignMusic();
        g_roomMusicInitialized = true;
        g_pendingTracks = 0;
        g_currentMusicId = MusicId::None;
        g_playing = false;
        g_paused = false;
        g_nextTrackNotReady = false;
        g_isReady = false;
        g_currentMusicSeq = SeqId::None;
        g_musicQueued = false;
        g_nextMusicSeq = SeqId::None;
        g_nextFadeInFrames = 0;
        g_nextTracks = 0;
        g_musicEncounterSuspension = 0;
        g_musicToResume = MusicId::None;
        g_tempoUpdateStart = 256;
        g_tempoUpdateTarget = 256;
        g_tempoUpdateTimeMs = 0;
        g_tempoUpdateTimer.Reset();
        g_volumeFadeStart = 1;
        g_volumeFadeTarget = 1;
        g_volumeFadeTimeMs = 0;
        g_volumeFadeTimer.Reset();
        g_stopAfterFade = false;
        for (TrackFaderState& fader : g_trackFaders) fader.Reset();
        if (!MusicPlayer::Available()) return;
        MusicPlayer::Load(SeqId::WIN);
        MusicPlayer::WaitForLoad();
        MusicPlayer::Play(Volume());
        MusicPlayer::Stop();
    }

    void Music::PlayMusic(MusicId musicId, std::optional<std::uint16_t> tracks,
        bool toggleOnTracks, bool toggleOffTracks)
    {
        const std::vector<MusicTrack>& musicInfo = MusicInfo();
        const std::int32_t index = static_cast<std::int32_t>(musicId);
        if (index < 0 || index >= static_cast<std::int32_t>(musicInfo.size())) return;
        const MusicTrack& info = musicInfo[static_cast<std::size_t>(index)];
        if (info.SeqId == SeqId::None) return;
        if (!tracks.has_value()) tracks = info.Tracks;
        if (toggleOnTracks) g_pendingTracks = static_cast<std::uint16_t>(g_pendingTracks | *tracks);
        else if (toggleOffTracks) g_pendingTracks = static_cast<std::uint16_t>(g_pendingTracks & static_cast<std::uint16_t>(~*tracks));
        else g_pendingTracks = *tracks;
        g_currentMusicId = musicId;
        g_paused = false;
        PlaySeq(info.SeqId, g_pendingTracks, true, true, info.FadeOutFrames, info.FadeInFrames);
    }

    void Music::TryPlayRoomMusic(std::int32_t roomId, std::int32_t track)
    {
        if ((GameState::EscapeTimer() == -1.0F || GameState::EscapeState() != EscapeState::Escape)
            && g_musicEncounterSuspension == 0)
        {
            PlayRoomMusic(roomId, track);
        }
    }

    void Music::PlayRoomMusic(std::int32_t roomId, std::int32_t track)
    {
        track = ClampInt(track, 0, 2);
        for (const RoomMusic& room : RoomMusicInfo())
        {
            if (room.RoomId == roomId)
            {
                PlayMusic(static_cast<MusicId>(room.TrackIds[static_cast<std::size_t>(track)]));
                return;
            }
        }
    }

    void Music::PlaySeq(SeqId seqId, bool notReady)
    {
        PlaySeq(seqId, UINT16_MAX, false, notReady);
    }

    void Music::PlaySeq(SeqId seqId, std::uint16_t tracks, bool queue, bool notReady,
        std::uint16_t fadeOutFrames, std::uint16_t fadeInFrames)
    {
        if (!queue)
        {
            g_nextTrackNotReady = false;
            Stop();
            g_isReady = !notReady;
            g_activeTracks = tracks;
            g_currentMusicSeq = seqId;
            g_mutedTracks = 0;
            g_fadingTracks = static_cast<std::uint16_t>(~g_activeTracks);
            g_musicVolume = 1.0F;
            g_volumeFadeTarget = 1.0F;
            g_volumeFadeTimer.Stop();
            for (std::int32_t i = 0; i < 16; i++)
            {
                g_trackFaders[static_cast<std::size_t>(i)].Reset(
                    static_cast<std::uint8_t>((g_activeTracks & (1U << i)) != 0 ? 127 : 0));
            }
            MusicPlayer::Load(seqId, tracks);
            g_playing = true;
            if (g_isReady)
            {
                MusicPlayer::WaitForLoad();
                MusicPlayer::Play(Volume());
                g_mutedTracks = 0;
                UpdateTrackVolume(g_fadingTracks, 0);
                g_fadingTracks = 0;
            }
            UpdateTempo(256, 0);
            g_musicQueued = false;
            g_nextMusicSeq = seqId;
        }
        else
        {
            if (!g_musicQueued)
            {
                if (g_nextMusicSeq == seqId)
                {
                    if (g_isReady)
                    {
                        SetTrackFaders(tracks, 127, fadeInFrames / 30.0F);
                        SetTrackFaders(static_cast<std::uint16_t>(tracks ^ UINT16_MAX), 0, fadeOutFrames / 30.0F);
                    }
                    else
                    {
                        g_activeTracks = tracks;
                    }
                    return;
                }
                Stop(fadeOutFrames / 30.0F);
                g_musicQueued = true;
            }
            g_nextTrackNotReady = notReady;
            g_nextMusicSeq = seqId;
            g_nextTracks = tracks;
            g_nextFadeInFrames = fadeInFrames;
        }
    }

    void Music::UpdateMusic()
    {
        if (!g_isReady && !MusicPlayer::Loading())
        {
            g_isReady = true;
            if (g_playing)
            {
                MusicPlayer::Volume(Volume());
                MusicPlayer::Tempo(g_baseTempo);
                MusicPlayer::Play(Volume());
                for (std::int32_t i = 0; i < 16; i++)
                {
                    if ((g_fadingTracks & (1U << i)) == 0)
                    {
                        g_fadingTracks = static_cast<std::uint16_t>(g_fadingTracks | (1U << i));
                        g_mutedTracks = 0;
                        g_trackFaders[static_cast<std::size_t>(i)].TimeMs = std::numeric_limits<float>::denorm_min();
                    }
                }
            }
        }
        if (!g_isReady || MusicPlayer::State() != PlaybackState::Stopped)
        {
            ProcessVolume();
            ProcessTempo();
            ProcessTrackFaders();
        }
        else if (g_musicQueued)
        {
            PlaySeq(g_nextMusicSeq, g_nextTracks, false, g_nextTrackNotReady, 0, g_nextFadeInFrames);
        }
    }

    void Music::UpdateMusicIdIfPaused(MusicId musicId)
    {
        if (g_paused) g_currentMusicId = musicId;
    }

    void Music::UpdateEscapeMusic()
    {
        if (g_currentMusicSeq != SeqId::OREGANO) return;
        if (GameState::EscapeTimer() >= 5400 / 30.0F)
        {
            UpdateTempo(245, 1 / 30.0F);
        }
        else if (GameState::EscapeTimer() >= 5100 / 30.0F)
        {
            const std::int32_t frames = static_cast<std::int32_t>(GameState::EscapeTimer() * 30);
            const std::int32_t tempo = 266 - ((frames - 5100) << 11) / 30000;
            UpdateTempo(static_cast<std::uint16_t>(tempo), 1 / 30.0F);
        }
        else if (GameState::EscapeTimer() >= 3600 / 30.0F)
        {
            UpdateTempo(266, 1 / 30.0F);
        }
        else if (GameState::EscapeTimer() >= 3400 / 30.0F)
        {
            const std::int32_t frames = static_cast<std::int32_t>(GameState::EscapeTimer() * 30);
            const std::int32_t tempo = 281 - 1536 * (frames - 3400) / 20000;
            UpdateTempo(static_cast<std::uint16_t>(tempo), 1 / 30.0F);
        }
        else if (GameState::EscapeTimer() >= 1800 / 30.0F)
        {
            if (GameState::EscapeTimer() == 1800) SwitchEscapeMusicIfNeeded();
            UpdateTempo(281, 1 / 30.0F);
        }
        else if (GameState::EscapeTimer() >= 1700 / 30.0F)
        {
            SwitchEscapeMusicIfNeeded();
            const std::int32_t frames = static_cast<std::int32_t>(GameState::EscapeTimer() * 30);
            const std::int32_t tempo = 294 - 1280 * (frames - 1700) / 10000;
            UpdateTempo(static_cast<std::uint16_t>(tempo), 1 / 30.0F);
        }
        else
        {
            SwitchEscapeMusicIfNeeded();
            UpdateTempo(294, 1 / 30.0F);
        }
    }

    void Music::SwitchEscapeMusicIfNeeded()
    {
        if (g_currentMusicId != MusicId::SEQ_OREGANO_M56)
        {
            PlayMusic(MusicId::SEQ_OREGANO_M56);
        }
    }

    void Music::UpdateEventMusic(float time)
    {
        if (g_currentMusicId == MusicId::SEQ_ENERGY_TIMER_M51 && time >= 600 / 30.0F && time < 1800 / 30.0F)
        {
            const std::int32_t frames = static_cast<std::int32_t>(time * 30);
            const std::int32_t tempo = 384 - 12800 * (frames - 600) / 120000;
            UpdateTempo(static_cast<std::uint16_t>(tempo), 1 / 30.0F);
        }
    }

    void Music::PlayEncounterMusic(Hunter hunter)
    {
        if (g_musicEncounterSuspension == 0)
        {
            g_musicToResume = g_currentMusicId;
            if (hunter == Hunter::Guardian) PlayMusic(MusicId::SEQ_GUARDIAN_M18);
        }
        std::int32_t bitIndex;
        if (hunter == Hunter::Guardian)
        {
            std::int32_t i;
            for (i = 7; i <= 9; i++) if ((g_musicEncounterSuspension & (1 << i)) == 0) break;
            bitIndex = i;
        }
        else
        {
            bitIndex = static_cast<std::int32_t>(hunter);
            if (g_currentMusicId != MusicId::SEQ_GUMBO_M3)
            {
                PlayMusic(MusicId::SEQ_GUMBO_M3);
                PlayMusic(MusicId::SEQ_GUMBO_M3, static_cast<std::uint16_t>(1U << 4), false, true);
            }
            PlayMusic(MusicId::SEQ_GUMBO_M3,
                static_cast<std::uint16_t>(1U << g_hunterMusicTracks.at(static_cast<std::size_t>(bitIndex))), true, false);
            if (hunter == Hunter::Spire)
            {
                PlayMusic(MusicId::SEQ_GUMBO_M3, static_cast<std::uint16_t>(1U << 9), false, true);
            }
        }
        g_musicEncounterSuspension |= (1 << bitIndex);
    }

    void Music::UpdateEncounterMusic(std::int32_t clearId)
    {
        if (g_musicEncounterSuspension == 0) return;
        if (clearId < 0)
        {
            g_musicEncounterSuspension = 0;
            if (clearId != -2) PlayMusic(g_musicToResume);
        }
        else
        {
            std::int32_t bitIndex = clearId;
            const Hunter hunter = static_cast<Hunter>(clearId);
            if (hunter == Hunter::Guardian)
            {
                std::int32_t i;
                for (i = 9; i >= 7; i--) if ((g_musicEncounterSuspension & (1 << i)) != 0) break;
                bitIndex = i;
            }
            if ((g_musicEncounterSuspension & (1 << bitIndex)) != 0)
            {
                g_musicEncounterSuspension &= ~(1 << bitIndex);
                if (g_musicEncounterSuspension == 0)
                {
                    if (g_musicToResume == MusicId::None) Stop(1.0F);
                    else PlayMusic(g_musicToResume);
                }
                else if (hunter != Hunter::Guardian)
                {
                    if ((g_musicEncounterSuspension & 0x7F) != 0)
                    {
                        PlayMusic(MusicId::SEQ_GUMBO_M3,
                            static_cast<std::uint16_t>(1U << g_hunterMusicTracks.at(static_cast<std::size_t>(bitIndex))), false, true);
                        if (hunter == Hunter::Spire)
                        {
                            PlayMusic(MusicId::SEQ_GUMBO_M3, static_cast<std::uint16_t>(1U << 9), true, false);
                        }
                    }
                    else
                    {
                        PlayMusic(MusicId::SEQ_GUARDIAN_M18);
                    }
                }
            }
        }
    }

    void Music::PlayPausedMusic()
    {
        if (!g_paused) return;
        const std::vector<MusicTrack>& musicInfo = MusicInfo();
        const std::int32_t index = static_cast<std::int32_t>(g_currentMusicId);
        if (index < 0 || index >= static_cast<std::int32_t>(musicInfo.size())) return;
        const MusicTrack& info = musicInfo[static_cast<std::size_t>(index)];
        if (info.SeqId == SeqId::None) return;
        g_paused = false;
        PlaySeq(info.SeqId, g_pendingTracks, true, true);
    }

    void Music::Pause()
    {
        if (!g_paused)
        {
            Stop();
            g_paused = true;
        }
    }

    void Music::Stop(float fadeTime)
    {
        g_playing = false;
        g_musicQueued = false;
        g_nextMusicSeq = SeqId::None;
        if (!g_isReady && MusicPlayer::Loading()) MusicPlayer::StopLoading(true);
        g_isReady = true;
        if (fadeTime <= 0) MusicPlayer::Stop();
        else FadeVolume(0, fadeTime, true);
    }

    void Music::FadeVolume(float volume, float time, bool stopAfterFade)
    {
        g_musicVolume = volume;
        g_volumeFadeStart = MusicPlayer::Volume();
        g_volumeFadeTarget = volume;
        g_volumeFadeTimeMs = time * 1000;
        if (g_volumeFadeTimeMs <= 0) g_volumeFadeTimeMs = 1;
        g_volumeFadeTimer.Restart();
        g_stopAfterFade = stopAfterFade;
    }

    void Music::ProcessVolume()
    {
        if (g_volumeFadeTimer.IsRunning())
        {
            const float pct = static_cast<float>(g_volumeFadeTimer.ElapsedMilliseconds()) / g_volumeFadeTimeMs;
            if (pct >= 1)
            {
                g_musicVolume = g_volumeFadeTarget;
                g_volumeFadeTimer.Stop();
                if (g_stopAfterFade) MusicPlayer::Stop();
                g_stopAfterFade = false;
            }
            else
            {
                g_musicVolume = g_volumeFadeStart + (g_volumeFadeTarget - g_volumeFadeStart) * pct;
            }
            MusicPlayer::Volume(Volume());
        }
    }

    void Music::UpdateTempo(std::uint16_t tempo, float time)
    {
        if (time <= 0)
        {
            MusicPlayer::Tempo(tempo);
            g_baseTempo = tempo;
            g_tempoUpdateStart = g_tempoUpdateTarget = tempo;
            g_tempoUpdateTimeMs = 0;
            g_tempoUpdateTimer.Reset();
        }
        else if (g_tempoUpdateTarget != tempo)
        {
            g_tempoUpdateStart = MusicPlayer::Tempo();
            g_tempoUpdateTarget = tempo;
            g_tempoUpdateTimeMs = time * 1000;
            g_tempoUpdateTimer.Restart();
        }
    }

    void Music::ProcessTempo()
    {
        if (MusicPlayer::Tempo() != g_tempoUpdateTarget && g_tempoUpdateTimer.IsRunning())
        {
            const float pct = static_cast<float>(g_tempoUpdateTimer.ElapsedMilliseconds()) / g_tempoUpdateTimeMs;
            if (pct >= 1)
            {
                MusicPlayer::Tempo(g_tempoUpdateTarget);
                g_tempoUpdateTimer.Stop();
            }
            else
            {
                MusicPlayer::Tempo(static_cast<std::uint16_t>(g_tempoUpdateStart
                    + (g_tempoUpdateTarget - g_tempoUpdateStart) * pct));
            }
        }
    }

    void Music::UpdateTrackVolume(std::uint16_t tracks, std::uint8_t volume)
    {
        if (volume > 0)
        {
            for (std::int32_t i = 0; i < 16; i++)
            {
                if ((tracks & (1U << i)) != 0)
                {
                    if (auto* track = MusicPlayer::GetTrack(i)) track->Volume = volume;
                }
            }
            if ((g_mutedTracks & tracks) != 0)
            {
                if (g_isReady)
                {
                    for (std::int32_t i = 0; i < 16; i++)
                    {
                        if ((tracks & (1U << i)) != 0)
                        {
                            if (auto* track = MusicPlayer::GetTrack(i)) track->Mute = false;
                        }
                    }
                }
                g_mutedTracks = static_cast<std::uint16_t>(g_mutedTracks & static_cast<std::uint16_t>(~(g_mutedTracks & tracks)));
            }
        }
        else if ((g_mutedTracks & tracks) != tracks)
        {
            if (g_isReady)
            {
                for (std::int32_t i = 0; i < 16; i++)
                {
                    if ((tracks & (1U << i)) != 0)
                    {
                        if (auto* track = MusicPlayer::GetTrack(i)) track->Mute = true;
                    }
                }
            }
            g_mutedTracks = static_cast<std::uint16_t>(g_mutedTracks | tracks);
        }
    }

    void Music::SetTrackFaders(std::uint16_t tracks, std::uint8_t target, float time)
    {
        if (target == 0) g_activeTracks = static_cast<std::uint16_t>(g_activeTracks & static_cast<std::uint16_t>(~tracks));
        else g_activeTracks = static_cast<std::uint16_t>(g_activeTracks | tracks);
        for (std::int32_t i = 0; i < 16; i++)
        {
            if ((tracks & (1U << i)) != 0)
            {
                TrackFaderState& fader = g_trackFaders[static_cast<std::size_t>(i)];
                if (auto* track = MusicPlayer::GetTrack(i)) fader.Start = track->Volume;
                fader.Target = target;
                fader.TimeMs = time * 1000;
                fader.Timer.Restart();
            }
        }
        g_fadingTracks = static_cast<std::uint16_t>(g_fadingTracks | tracks);
    }

    void Music::ProcessTrackFaders()
    {
        if (g_fadingTracks == 0) return;
        for (std::int32_t i = 0; i < 16; i++)
        {
            if ((g_fadingTracks & (1U << i)) == 0) continue;
            TrackFaderState& fader = g_trackFaders[static_cast<std::size_t>(i)];
            if (fader.Timer.IsRunning())
            {
                NCSF123::NCSFCommon::Track* track = MusicPlayer::GetTrack(i);
                if (track != nullptr && track->Volume != fader.Target)
                {
                    const float pct = static_cast<float>(fader.Timer.ElapsedMilliseconds()) / fader.TimeMs;
                    if (pct >= 1)
                    {
                        UpdateTrackVolume(static_cast<std::uint16_t>(1U << i), fader.Target);
                        fader.Timer.Stop();
                        g_fadingTracks = static_cast<std::uint16_t>(g_fadingTracks & static_cast<std::uint16_t>(~(1U << i)));
                    }
                    else
                    {
                        UpdateTrackVolume(static_cast<std::uint16_t>(1U << i), static_cast<std::uint8_t>(
                            fader.Start + (fader.Target - fader.Start) * pct));
                    }
                    track->Mute = track->Volume == 0;
                }
            }
        }
    }

    bool MusicPlayer::Available() noexcept
    {
        EnsureMusicPlayerInitialized();
        return g_available.load(std::memory_order_acquire);
    }

    std::shared_ptr<MiniAudioEngine> MusicPlayer::Engine() noexcept
    {
        EnsureMusicPlayerInitialized();
        return g_available.load(std::memory_order_acquire) ? g_audioEngine : nullptr;
    }

    std::shared_ptr<AudioPlaybackDevice> MusicPlayer::PlaybackDevice() noexcept
    {
        EnsureMusicPlayerInitialized();
        return g_available.load(std::memory_order_acquire) ? g_playbackDevice : nullptr;
    }

    AudioFormat MusicPlayer::Format() noexcept
    {
        EnsureMusicPlayerInitialized();
        return g_format;
    }

    bool MusicPlayer::Loading() noexcept { EnsureMusicPlayerInitialized(); return g_loading.load(std::memory_order_acquire); }
    bool MusicPlayer::StopLoading() noexcept { EnsureMusicPlayerInitialized(); return g_stopLoading.load(std::memory_order_acquire); }
    void MusicPlayer::StopLoading(bool value) noexcept { EnsureMusicPlayerInitialized(); g_stopLoading.store(value, std::memory_order_release); }

    void MusicPlayer::Load(SeqId seqId, std::uint16_t tracks, float volume)
    {
        EnsureMusicPlayerInitialized();
        if (!g_available.load(std::memory_order_acquire)) return;
        g_loading.store(true, std::memory_order_release);
        Stop();
        if (seqId == SeqId::None)
        {
            g_loading.store(false, std::memory_order_release);
            return;
        }
        std::thread([seqId, tracks, volume]
        {
            struct LoadFinally final
            {
                ~LoadFinally()
                {
                    g_loading.store(false, std::memory_order_release);
                    g_stopLoading.store(false, std::memory_order_release);
                }
            } finally;
            try
            {
                while (g_stopLoading.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                MusicPlayer::Remove();
                if (g_stopLoading.load(std::memory_order_acquire)) return;

                const std::string path = Paths::Combine(Paths::FileSystem(), "_seq",
                    Metadata::SequenceFiles.at(static_cast<std::size_t>(seqId)));
                auto stream = std::make_shared<NCSFPlayerStream>(path, static_cast<std::uint32_t>(SampleRate),
                    NCSFPlayer::Interpolation::None, 5, 115000, 5000,
                    NCSF123::VolumeType::ReplayGainAlbum, NCSFPlayer::PeakType::ReplayGainTrack,
                    true, volume, 0, 0, false);
                {
                    std::lock_guard<std::recursive_mutex> guard(g_playerMutex);
                    g_stream = stream;
                }
                for (std::int32_t i = 0; i < 16; i++)
                {
                    if ((tracks & (1U << i)) == 0)
                    {
                        if (auto* track = MusicPlayer::GetTrack(i))
                        {
                            track->Volume = 0;
                            track->Mute = true;
                        }
                    }
                }
                if (g_stopLoading.load(std::memory_order_acquire)) return;

                auto provider = std::make_shared<RawDataProvider>(stream, SampleFormat::F32, SampleRate);
                {
                    std::lock_guard<std::recursive_mutex> guard(g_playerMutex);
                    g_provider = provider;
                }
                if (g_stopLoading.load(std::memory_order_acquire)) return;

                auto player = std::make_shared<SoundPlayer>(g_audioEngine, g_format, provider);
                {
                    std::lock_guard<std::recursive_mutex> guard(g_playerMutex);
                    g_player = player;
                }
                if (g_stopLoading.load(std::memory_order_acquire)) return;
                g_playbackDevice->MasterMixer.AddComponent(player);
                if (g_stopLoading.load(std::memory_order_acquire)) return;
                g_playbackDevice->Start();
            }
            catch (...)
            {
                // Task.Run captures faults in the Task; because Music.cs does not retain/await it,
                // there is no synchronous propagation to the game thread.
            }
        }).detach();
    }

    void MusicPlayer::WaitForLoad(std::int32_t sleepMs)
    {
        EnsureMusicPlayerInitialized();
        while (g_loading.load(std::memory_order_acquire))
        {
            if (sleepMs < -1)
            {
                throw std::out_of_range("sleepMs");
            }
            if (sleepMs == -1)
            {
                for (;;)
                {
                    std::this_thread::sleep_for(std::chrono::hours(24));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
    }

    void MusicPlayer::Play(float volume)
    {
        EnsureMusicPlayerInitialized();
        if (!g_available.load(std::memory_order_acquire)) return;
        Volume(volume);
        std::shared_ptr<SoundPlayer> player;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); player = g_player; }
        if (player) player->Play();
    }

    void MusicPlayer::Pause()
    {
        EnsureMusicPlayerInitialized();
        std::shared_ptr<SoundPlayer> player;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); player = g_player; }
        if (player) player->Pause();
    }

    PlaybackState MusicPlayer::State() noexcept
    {
        EnsureMusicPlayerInitialized();
        std::shared_ptr<SoundPlayer> player;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); player = g_player; }
        return player ? player->State() : PlaybackState::Stopped;
    }

    float MusicPlayer::Volume() noexcept
    {
        EnsureMusicPlayerInitialized();
        std::shared_ptr<NCSFPlayerStream> stream;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); stream = g_stream; }
        return stream ? stream->VolumeModification() : 0.0F;
    }

    void MusicPlayer::Volume(float value) noexcept
    {
        EnsureMusicPlayerInitialized();
        std::shared_ptr<NCSFPlayerStream> stream;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); stream = g_stream; }
        if (stream) stream->VolumeModification(ClampFloat(value, 0.0F, 1.0F));
    }

    std::uint16_t MusicPlayer::Tempo() noexcept
    {
        EnsureMusicPlayerInitialized();
        std::shared_ptr<NCSFPlayerStream> stream;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); stream = g_stream; }
        return stream ? stream->Player().TempoRatio() : 0;
    }

    void MusicPlayer::Tempo(std::uint16_t value) noexcept
    {
        EnsureMusicPlayerInitialized();
        std::shared_ptr<NCSFPlayerStream> stream;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); stream = g_stream; }
        if (stream) stream->Player().TempoRatio(value);
    }

    NCSF123::NCSFCommon::Track* MusicPlayer::GetTrack(std::int32_t index) noexcept
    {
        EnsureMusicPlayerInitialized();
        std::shared_ptr<NCSFPlayerStream> stream;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); stream = g_stream; }
        return stream ? stream->Player().GetTrack(index) : nullptr;
    }

    void MusicPlayer::Stop()
    {
        EnsureMusicPlayerInitialized();
        if (!g_available.load(std::memory_order_acquire)) return;
        std::shared_ptr<SoundPlayer> player;
        { std::lock_guard<std::recursive_mutex> guard(g_playerMutex); player = g_player; }
        if (player) player->Stop();
    }

    void MusicPlayer::Remove(bool shutdown)
    {
        EnsureMusicPlayerInitialized();
        std::shared_ptr<SoundPlayer> player;
        std::shared_ptr<RawDataProvider> provider;
        std::shared_ptr<NCSFPlayerStream> stream;
        {
            std::lock_guard<std::recursive_mutex> guard(g_playerMutex);
            player = g_player;
            provider = g_provider;
            stream = g_stream;
        }
        if (player)
        {
            assert(provider != nullptr);
            assert(stream != nullptr);
            player->Stop();
            if (shutdown) g_playbackDevice->Stop();
            g_playbackDevice->MasterMixer.RemoveComponent(player);
            provider->Dispose();
            stream->Dispose();
            player->Dispose();
            std::lock_guard<std::recursive_mutex> guard(g_playerMutex);
            g_provider.reset();
            g_stream.reset();
            g_player.reset();
        }
    }
}
