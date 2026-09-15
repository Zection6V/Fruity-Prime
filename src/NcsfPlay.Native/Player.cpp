#include "Player.hpp"

#include "Channel.hpp"
#include "NC/SSEQ.hpp"
#include "Track.hpp"

#include <bit>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace
{
    [[noreturn]] void ThrowNullReference()
    {
        throw std::runtime_error("NullReferenceException");
    }

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("IndexOutOfRangeException");
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T>& SpanAt(
        std::span<std::shared_ptr<T>> values,
        std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            ThrowIndexOutOfRange();
        return values[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] constexpr std::int32_t WrapMul32(
        std::int32_t left,
        std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapSub32(
        std::int32_t left,
        std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t ArithmeticShiftRight32(
        std::int32_t value,
        unsigned count) noexcept
    {
        if (count == 0U)
            return value;
        const std::uint32_t bits = static_cast<std::uint32_t>(value);
        const std::uint32_t shifted = bits >> count;
        if (value >= 0)
            return static_cast<std::int32_t>(shifted);
        return std::bit_cast<std::int32_t>(shifted | (~0U << (32U - count)));
    }
}

namespace NCSFCommon
{
    const std::array<std::uint8_t, Player::ChannelCount> Player::ChannelAllocationOrder =
    {
        4, 5, 6, 7, 2, 0, 3, 1, 8, 9, 10, 11, 14, 12, 15, 13
    };

    Player::Player()
    {
        trackIds.fill(InvalidTrackIndex);
        variables.fill(-1);
    }

    std::uint8_t Player::Priority() const noexcept { return _priority; }
    void Player::Priority(std::uint8_t value) noexcept { _priority = value; }

    std::uint8_t Player::Volume() const noexcept { return _volume; }
    void Player::Volume(std::uint8_t value) noexcept { _volume = value; }

    std::uint16_t Player::Tempo() const noexcept { return _tempo; }
    void Player::Tempo(std::uint16_t value) noexcept { _tempo = value; }

    std::uint16_t Player::TempoRatio() const noexcept { return tempoRatio; }
    void Player::TempoRatio(std::uint16_t value) noexcept { tempoRatio = value; }

    std::span<const std::int16_t> Player::Variables() const noexcept
    {
        return variables;
    }

    std::shared_ptr<NC::SBNK> Player::SBNK() const noexcept
    {
        return _sbnk;
    }

    void Player::SBNK(std::shared_ptr<NC::SBNK> value) noexcept
    {
        _sbnk = std::move(value);
    }

    std::span<const std::shared_ptr<NC::SWAR>> Player::SWARs() const noexcept
    {
        return swars;
    }

    std::int16_t Player::SSEQVolume() const noexcept { return _sseqVolume; }
    void Player::SSEQVolume(std::int16_t value) noexcept { _sseqVolume = value; }

    std::span<const std::shared_ptr<Channel>> Player::Channels()
    {
        const auto currentChannels = channels();
        return {currentChannels.data(), currentChannels.size()};
    }

    std::uint16_t Player::ChannelMask() const noexcept { return _channelMask; }
    void Player::ChannelMask(std::uint16_t value) noexcept { _channelMask = value; }

    std::shared_ptr<Channel> Player::AllocateChannel(
        std::uint32_t channelMask,
        std::int32_t priority,
        std::function<void(Channel*, bool)> callback)
    {
        std::shared_ptr<Channel> chnPrev;

        for (const std::uint8_t channelCandidate : ChannelAllocationOrder)
        {
            if ((channelMask & (1U << channelCandidate)) != 0U)
            {
                auto chn = SpanAt(channels(), channelCandidate);

                if (!chnPrev)
                {
                    chnPrev = std::move(chn);
                }
                else
                {
                    if (!chn)
                        ThrowNullReference();

                    if (chn->Priority() <= chnPrev->Priority()
                        && (chn->Priority() != chnPrev->Priority()
                            || chnPrev->VolumeCompare(chn.get()) < 0))
                    {
                        chnPrev = std::move(chn);
                    }
                }
            }
        }

        if (!chnPrev || priority < chnPrev->Priority())
            return {};

        const auto previousCallback = chnPrev->Callback();
        if (previousCallback)
            previousCallback(chnPrev.get(), false);

        chnPrev->SyncFlags(ChannelSyncFlag::Stop);
        chnPrev->Flags(chnPrev->Flags() & ~ChannelFlag::Active);
        chnPrev->Setup(std::move(callback), priority);
        return chnPrev;
    }

    void Player::PrepareSequence(
        const NC::SSEQ* sseq,
        std::int32_t offset,
        std::int16_t sseqVol)
    {
        Stop();

        Init(sseqVol);

        std::int32_t allocateTrackIndex = AllocateTrack();

        if (allocateTrackIndex >= 0)
        {
            auto trk = SpanAt(tracks(), allocateTrackIndex);
            if (!trk)
                ThrowNullReference();
            trk->Init();
            if (sseq == nullptr)
                ThrowNullReference();
            trk->Start(sseq->Data(), offset);
            trackIds[0] = static_cast<std::uint8_t>(allocateTrackIndex);

            const std::uint8_t cmd = trk->ReadU8();

            if (cmd == static_cast<std::uint8_t>(Track::SSEQCommand::AllocateTrack))
            {
                std::int32_t track;
                std::uint16_t trackMask;

                for (trackMask = static_cast<std::uint16_t>(trk->ReadU16() >> 1U), track = 1;
                     trackMask != 0;
                     ++track, trackMask = static_cast<std::uint16_t>(trackMask >> 1U))
                {
                    if ((trackMask & 1U) != 0U)
                    {
                        allocateTrackIndex = AllocateTrack();
                        if (allocateTrackIndex < 0)
                            break;
                        auto nextTrack = SpanAt(tracks(), allocateTrackIndex);
                        if (!nextTrack)
                            ThrowNullReference();
                        nextTrack->Init();
                        trackIds[static_cast<std::size_t>(track)] =
                            static_cast<std::uint8_t>(allocateTrackIndex);
                    }
                }
            }
            else
            {
                trk->CurrentPos(WrapSub32(trk->CurrentPos(), 1));
            }
        }
    }

    void Player::Init(std::int16_t sseqVol)
    {
        _tempo = 120;
        tempoRatio = 256;
        tempoCounter = TimerRate;
        _volume = 0x7FU;
        _priority = 64;

        trackIds.fill(InvalidTrackIndex);
        variables.fill(-1);

        for (std::int32_t i = 0; i < TrackCount; ++i)
        {
            auto trackForId = SpanAt(tracks(), i);
            if (!trackForId)
                ThrowNullReference();
            trackForId->Id(static_cast<std::uint8_t>(i));

            auto trackForPlayer = SpanAt(tracks(), i);
            if (!trackForPlayer)
                ThrowNullReference();
            trackForPlayer->Player(this);

            auto trackForFlags = SpanAt(tracks(), i);
            if (!trackForFlags)
                ThrowNullReference();
            trackForFlags->Flags(trackForFlags->Flags() & ~TrackFlag::Active);
        }

        _sseqVolume = sseqVol;

        for (std::int32_t i = 0; i < ChannelCount; ++i)
        {
            auto channelForPlayer = SpanAt(channels(), i);
            if (!channelForPlayer)
                ThrowNullReference();
            channelForPlayer->Player(this);

            auto channelForInit = SpanAt(channels(), i);
            if (!channelForInit)
                ThrowNullReference();
            channelForInit->Init(i);
        }
    }

    void Player::Main()
    {
        std::int32_t ticks = 0;

        while (tempoCounter >= TimerRate)
        {
            tempoCounter = static_cast<std::uint16_t>(tempoCounter - TimerRate);
            ++ticks;
        }

        for (std::int32_t i = 0; i < ticks; ++i)
            StepTicks();

        std::int32_t tempoIncrease = static_cast<std::int32_t>(_tempo);
        tempoIncrease = WrapMul32(tempoIncrease, static_cast<std::int32_t>(tempoRatio));
        tempoIncrease = ArithmeticShiftRight32(tempoIncrease, 8U);

        tempoCounter = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(tempoCounter)
            + static_cast<std::uint16_t>(tempoIncrease));
    }

    std::shared_ptr<Track> Player::GetTrack(std::int32_t track)
    {
        if (track > TrackCount - 1)
            return {};
        if (track < 0)
            ThrowIndexOutOfRange();

        if (trackIds[static_cast<std::size_t>(track)] == InvalidTrackIndex)
            return {};

        auto currentTracks = tracks();
        const std::int32_t trackId = trackIds[static_cast<std::size_t>(track)];
        return SpanAt(currentTracks, trackId);
    }

    void Player::StopTrack(std::int32_t trackIndex)
    {
        auto track = GetTrack(trackIndex);

        if (track)
        {
            track->Stop();
            track->Flags(track->Flags() & ~TrackFlag::Active);
            trackIds[static_cast<std::size_t>(trackIndex)] = InvalidTrackIndex;
        }
    }

    void Player::Stop()
    {
        for (std::int32_t i = 0; i < TrackCount; ++i)
            StopTrack(i);
    }

    void Player::UpdateChannel()
    {
        for (std::int32_t i = 0; i < TrackCount; ++i)
        {
            auto track = GetTrack(i);
            if (track)
                track->UpdateChannel(true);
        }
    }

    std::int32_t Player::AllocateTrack()
    {
        for (std::int32_t i = 0; i < TrackCount; ++i)
        {
            auto trackForFlags = SpanAt(tracks(), i);
            if (!trackForFlags)
                ThrowNullReference();

            if ((trackForFlags->Flags() & TrackFlag::Active) != TrackFlag::Active)
            {
                auto trackForSet = SpanAt(tracks(), i);
                if (!trackForSet)
                    ThrowNullReference();
                trackForSet->Flags(trackForSet->Flags() | TrackFlag::Active);
                return i;
            }
        }
        return -1;
    }

    void Player::SetVariable(std::int8_t variableNumber, std::int16_t value)
    {
#ifndef NDEBUG
        assert(variableNumber >= 0 && variableNumber <= 31);
#endif
        variables.at(static_cast<std::size_t>(variableNumber)) = value;
    }

    void Player::SetSWAR(std::int32_t index, std::shared_ptr<NC::SWAR> swar)
    {
#ifndef NDEBUG
        assert(index >= 0 && index <= 3);
#endif
        swars.at(static_cast<std::size_t>(index)) = std::move(swar);
    }

    float Player::MulDiv7(float val, std::uint8_t mul) noexcept
    {
        return mul == 127U ? val : val * static_cast<float>(mul) * 0.0078125F;
    }
}
