#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace NCSFCommon
{
    namespace NC
    {
        class SBNK;
        class SSEQ;
        class SWAR;
    }

    class Channel;
    class Track;

    class Player
    {
    public:
        virtual ~Player() = default;

        Player(const Player&) = delete;
        Player(Player&&) = delete;
        Player& operator=(const Player&) = delete;
        Player& operator=(Player&&) = delete;

        static constexpr std::int32_t ChannelCount = 16;

        static constexpr std::uint32_t ARM7Clock = 33514000U;
        static constexpr float SecondsPerClockCycle = 64 * 2728.0F / ARM7Clock;

        [[nodiscard]] std::uint8_t Priority() const noexcept;
        void Priority(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Volume() const noexcept;
        void Volume(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint16_t Tempo() const noexcept;
        void Tempo(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint16_t TempoRatio() const noexcept;
        void TempoRatio(std::uint16_t value) noexcept;

        [[nodiscard]] std::span<const std::int16_t> Variables() const noexcept;

        [[nodiscard]] virtual std::uint32_t SampleRate() const = 0;
        virtual void SampleRate(std::uint32_t value) = 0;

        [[nodiscard]] std::shared_ptr<NC::SBNK> SBNK() const noexcept;
        void SBNK(std::shared_ptr<NC::SBNK> value) noexcept;

        [[nodiscard]] std::span<const std::shared_ptr<NC::SWAR>> SWARs() const noexcept;

        [[nodiscard]] std::int16_t SSEQVolume() const noexcept;
        void SSEQVolume(std::int16_t value) noexcept;

        [[nodiscard]] std::span<const std::shared_ptr<Channel>> Channels();

        [[nodiscard]] std::uint16_t ChannelMask() const noexcept;
        void ChannelMask(std::uint16_t value) noexcept;

        [[nodiscard]] std::shared_ptr<Channel> AllocateChannel(
            std::uint32_t channelMask,
            std::int32_t priority,
            std::function<void(Channel*, bool)> callback);

        virtual void SequenceMain() = 0;

        void PrepareSequence(const NC::SSEQ* sseq, std::int32_t offset, std::int16_t sseqVol);
        void Init(std::int16_t sseqVol);
        virtual void Main();

        [[nodiscard]] std::shared_ptr<Track> GetTrack(std::int32_t track);
        void StopTrack(std::int32_t trackIndex);
        void Stop();
        void UpdateChannel();

        virtual void StepTicks() = 0;

        [[nodiscard]] std::int32_t AllocateTrack();

        void SetVariable(std::int8_t variableNumber, std::int16_t value);
        void SetSWAR(std::int32_t index, std::shared_ptr<NC::SWAR> swar);

        [[nodiscard]] static float MulDiv7(float val, std::uint8_t mul) noexcept;

    protected:
        Player();

        static constexpr std::int32_t TrackCount = 16;
        static constexpr std::uint16_t TimerRate = 240;

        std::uint16_t tempoRatio = 256;
        std::uint16_t tempoCounter = TimerRate;

        [[nodiscard]] virtual std::span<std::shared_ptr<Track>> tracks() = 0;
        [[nodiscard]] virtual std::span<std::shared_ptr<Channel>> channels() = 0;

    private:
        static constexpr std::uint8_t InvalidTrackIndex = 0xFFU;
        static const std::array<std::uint8_t, ChannelCount> ChannelAllocationOrder;

        std::uint8_t _priority = 64;
        std::uint8_t _volume = 0x7FU;

        std::array<std::uint8_t, TrackCount> trackIds{};

        std::uint16_t _tempo = 120;

        std::array<std::int16_t, 32> variables{};

        std::shared_ptr<NC::SBNK> _sbnk;
        std::array<std::shared_ptr<NC::SWAR>, 4> swars{};
        std::int16_t _sseqVolume = 0;

        std::uint16_t _channelMask = 0;
    };
}
