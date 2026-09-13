#include "Track.hpp"

#include "Channel.hpp"
#include "NC/SBNK.hpp"
#include "NC/SBNKInstrument.hpp"
#include "NC/SBNKInstrumentEntry.hpp"
#include "Player.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
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

    [[noreturn]] void ThrowArgumentOutOfRange()
    {
        throw std::out_of_range("ArgumentOutOfRangeException");
    }

    [[nodiscard]] constexpr std::int32_t WrapAdd32(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapSub32(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapMul32(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapShiftLeft32(std::int32_t value, unsigned count) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) << (count & 31U));
    }

    [[nodiscard]] constexpr std::int32_t ArithmeticShiftRight32(std::int32_t value, unsigned count) noexcept
    {
        count &= 31U;
        if (count == 0U)
            return value;
        const std::uint32_t bits = static_cast<std::uint32_t>(value);
        const std::uint32_t shifted = bits >> count;
        if (value >= 0)
            return static_cast<std::int32_t>(shifted);
        return std::bit_cast<std::int32_t>(shifted | (~0U << (32U - count)));
    }

    [[nodiscard]] constexpr std::int8_t WrapSByte(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
    }

    [[nodiscard]] constexpr std::int16_t WrapInt16(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
    }

    [[nodiscard]] std::span<const std::uint8_t> SliceFrom(
        std::span<const std::uint8_t> span, std::int32_t offset)
    {
        if (offset < 0 || static_cast<std::size_t>(offset) > span.size())
            ThrowArgumentOutOfRange();
        return span.subspan(static_cast<std::size_t>(offset));
    }

    template <typename T, std::size_t Extent>
    [[nodiscard]] T& SpanAt(std::span<T, Extent> span, std::size_t index)
    {
        if (index >= span.size())
            ThrowIndexOutOfRange();
        return span[index];
    }

    [[nodiscard, maybe_unused]] NCSFCommon::Channel* RawChannel(NCSFCommon::Channel* channel) noexcept
    {
        return channel;
    }

    template <typename T>
    [[nodiscard]] auto RawChannel(const T& channel) noexcept -> decltype(channel.get())
    {
        return channel.get();
    }
}

namespace NCSFCommon
{
    std::uint32_t Track::RandomU = 0x12345678U;

    Track::Track()
        : modulation(std::make_shared<LFOParam>())
    {
    }

    Track::~Track() = default;

    TrackFlag Track::Flags() const noexcept
    {
        return _flags;
    }

    void Track::Flags(TrackFlag value) noexcept
    {
        _flags = value;
    }

    std::uint8_t Track::Volume() const noexcept
    {
        return volume;
    }

    void Track::Volume(std::uint8_t value) noexcept
    {
        volume = value;
    }

    std::int32_t Track::CurrentPos() const noexcept
    {
        return _currentPos;
    }

    void Track::CurrentPos(std::int32_t value) noexcept
    {
        _currentPos = value;
    }

    NCSFCommon::Player* Track::Player() const noexcept
    {
        return _player;
    }

    void Track::Player(NCSFCommon::Player* value) noexcept
    {
        _player = value;
    }

    std::uint8_t Track::Id() const noexcept
    {
        return _id;
    }

    void Track::Id(std::uint8_t value) noexcept
    {
        _id = value;
    }

    bool Track::Mute() const noexcept
    {
        return _mute;
    }

    void Track::Mute(bool value) noexcept
    {
        _mute = value;
    }

    std::uint8_t Track::ReadU8()
    {
        const std::int32_t pos = _currentPos;
        _currentPos = WrapAdd32(_currentPos, 1);
        if (pos < 0)
            ThrowIndexOutOfRange();
        return base[static_cast<std::size_t>(pos)];
    }

    std::uint16_t Track::ReadU16()
    {
        const auto span = SliceFrom(base.Span(), _currentPos);
        if (span.size() < 2U)
            ThrowArgumentOutOfRange();

        const std::uint16_t result = static_cast<std::uint16_t>(span[0])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(span[1]) << 8U);
        _currentPos = WrapAdd32(_currentPos, 2);
        return result;
    }

    std::uint32_t Track::ReadU24()
    {
        const auto span = SliceFrom(base.Span(), _currentPos);
        if (span.size() < 3U)
            ThrowArgumentOutOfRange();

        const std::uint32_t result = static_cast<std::uint32_t>(span[0])
            | (static_cast<std::uint32_t>(span[1]) << 8U)
            | (static_cast<std::uint32_t>(span[2]) << 16U);
        _currentPos = WrapAdd32(_currentPos, 3);
        return result;
    }

    std::int32_t Track::ReadVLV()
    {
        const auto span = SliceFrom(base.Span(), _currentPos);
        std::int32_t pos = 0;
        std::int32_t retval = 0;
        std::int32_t b;

        do
        {
            const std::int32_t index = pos;
            pos = WrapAdd32(pos, 1);
            if (index < 0 || static_cast<std::size_t>(index) >= span.size())
                ThrowIndexOutOfRange();
            b = span[static_cast<std::size_t>(index)];
            retval = std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(WrapShiftLeft32(retval, 7))
                | static_cast<std::uint32_t>(b & 0x7F));
        } while ((b & 0x80) != 0);

        _currentPos = WrapAdd32(_currentPos, pos);
        return retval;
    }

    std::uint16_t Track::CalculateRandom() noexcept
    {
        RandomU = RandomU * 1664525U + 1013904223U;
        return static_cast<std::uint16_t>(RandomU >> 16U);
    }

    std::int32_t Track::ParseValue(ValueType valueType)
    {
        switch (valueType)
        {
        case ValueType::U8:
            return ReadU8();
        case ValueType::U16:
            return ReadU16();
        case ValueType::VLV:
            return ReadVLV();
        case ValueType::Variable:
        {
            if (_player == nullptr)
                ThrowNullReference();
            const auto variables = _player->Variables();
            const std::uint8_t index = ReadU8();
            return SpanAt(variables, index);
        }
        case ValueType::Random:
        {
            const std::int32_t lo = WrapShiftLeft32(ReadU16(), 16);
            const std::int32_t hi = WrapInt16(ReadU16());
            const std::int32_t ran = CalculateRandom();
            const std::int32_t lower = ArithmeticShiftRight32(lo, 16);
            std::int32_t retval = WrapSub32(hi, lower);
            retval = WrapAdd32(retval, 1);
            retval = ArithmeticShiftRight32(WrapMul32(ran, retval), 16);
            retval = WrapAdd32(retval, lower);
            return retval;
        }
        }

        return 0;
    }

    void Track::Init()
    {
        base = {};
        _currentPos = -1;

        _flags |= TrackFlag::NoteWait | TrackFlag::Compare;
        _flags &= ~TrackFlag::Tie & ~TrackFlag::NoteFinishWait & ~TrackFlag::Portamento;

        callStackDepth = portamentoTime = 0;
        program = 0;
        priority = 64;
        volume = expression = panRange = 127;
        pan = pitchBend = transpose = 0;
        envelopeAttack = envelopeDecay = envelopeSustain = envelopeRelease = 255;
        bendRange = 2;
        portamentoKey = 60;
        sweepPitch = 0;
        modulation = std::make_shared<LFOParam>();
        wait = 0;
        channels.clear();
    }

    void Track::Start(NC::SSEQReadOnlyMemory data, std::int32_t offset)
    {
        base = std::move(data);
        _currentPos = offset;
    }

    void Track::ReleaseChannels(std::int32_t release)
    {
        UpdateChannel(false);

        if (_player == nullptr)
            ThrowNullReference();
        const auto playerChannels = _player->Channels();
        for (const std::int32_t channelId : channels)
        {
            if (channelId < 0)
                ThrowIndexOutOfRange();
            Channel* channel = RawChannel(SpanAt(playerChannels, static_cast<std::size_t>(channelId)));
            if (channel == nullptr)
                ThrowNullReference();

            if (channel->IsActive())
            {
                if (release >= 0)
                    channel->SetRelease(release & 0xFF);
                channel->Priority(1);
                channel->Release();
            }
        }
    }

    void Track::FreeChannels()
    {
        if (_player == nullptr)
            ThrowNullReference();
        const auto playerChannels = _player->Channels();
        for (const std::int32_t channelId : channels)
        {
            if (channelId < 0)
                ThrowIndexOutOfRange();
            Channel* channel = RawChannel(SpanAt(playerChannels, static_cast<std::size_t>(channelId)));
            if (channel == nullptr)
                ThrowNullReference();
            channel->Free();
        }
        channels.clear();
    }

    void Track::Stop()
    {
        _currentPos = -1;
        ReleaseChannels(-1);
        FreeChannels();
    }

    void Track::ChannelCallback(Channel* channel, bool free)
    {
        if (channel == nullptr)
            ThrowNullReference();

        if (free)
        {
            channel->Priority(0);
            channel->Free();
        }

        const auto found = std::find(channels.begin(), channels.end(), static_cast<std::int32_t>(channel->Id()));
        if (found != channels.end())
            channels.erase(found);
    }

    void Track::UpdateChannel(bool release)
    {
        std::int32_t vol;
        if (_mute)
            vol = -0x8000;
        else
        {
            if (_player == nullptr)
                ThrowNullReference();
            vol = WrapAdd32(
                WrapAdd32(
                    WrapAdd32(Channel::ConvertSustain(volume), Channel::ConvertSustain(expression)),
                    Channel::ConvertSustain(_player->Volume())),
                _player->SSEQVolume());
        }

        std::int32_t pitch = pitchBend;
        pitch = WrapMul32(pitch, static_cast<std::int32_t>(bendRange) << 6U);
        pitch = ArithmeticShiftRight32(pitch, 7);

        std::int32_t panValue = pan;

        if (panRange != 127)
            panValue = ArithmeticShiftRight32(
                WrapAdd32(WrapMul32(panValue, panRange), 0x40), 7);

        if (vol < -0x8000)
            vol = -0x8000;

        panValue = std::clamp(panValue, -128, 127);

        if (_player == nullptr)
            ThrowNullReference();
        const auto playerChannels = _player->Channels();
        for (const std::int32_t channelId : channels)
        {
            if (channelId < 0)
                ThrowIndexOutOfRange();
            Channel* channel = RawChannel(SpanAt(playerChannels, static_cast<std::size_t>(channelId)));
            if (channel == nullptr)
                ThrowNullReference();

            if (channel->EnvelopeStatus() != ChannelState::Release)
            {
                channel->UserDecay(WrapInt16(vol));
                channel->UserPitch(WrapInt16(pitch));
                channel->UserPan(WrapSByte(panValue));
                channel->PanRange(panRange);

                if (!modulation)
                    ThrowNullReference();
                const auto& channelParam = channel->LFO().Param();
                modulation->CopyTo(channelParam.get());

                if (channel->Length() == 0 && release)
                {
                    channel->Priority(1);
                    channel->Release();
                }
            }
        }
    }

    std::shared_ptr<NC::SBNKInstrument> Track::ReadInstrumentData(
        std::int32_t programNumber, std::int32_t midiKey)
    {
        if (_player == nullptr)
            ThrowNullReference();
        const auto sbnk = _player->SBNK();
        if (!sbnk)
            ThrowNullReference();

        const auto sbnkEntries = sbnk->Entries();
        if (programNumber < 0 || static_cast<std::size_t>(programNumber) >= sbnkEntries.size())
            return {};

        const auto& entry = sbnkEntries[static_cast<std::size_t>(programNumber)];
        if (!entry)
            ThrowNullReference();

        const auto instruments = entry->Instruments();
        switch (entry->Record())
        {
        case 1:
        case 2:
        case 3:
        case 5:
            return SpanAt(instruments, 0U);
        case 16:
        {
            const auto& first = SpanAt(instruments, 0U);
            if (!first)
                ThrowNullReference();
            if (midiKey < first->LowNote())
                return {};

            const auto& last = SpanAt(instruments, instruments.size() - 1U);
            if (!last)
                ThrowNullReference();
            if (midiKey > last->HighNote())
                return {};

            const std::int32_t index = WrapSub32(midiKey, first->LowNote());
            if (index < 0)
                ThrowIndexOutOfRange();
            return SpanAt(instruments, static_cast<std::size_t>(index));
        }
        case 17:
        {
            std::size_t reg = 0;
            const std::size_t entries = instruments.size();
            for (; reg < entries; ++reg)
            {
                const auto& instrument = instruments[reg];
                if (!instrument)
                    ThrowNullReference();
                if (midiKey <= instrument->HighNote())
                    break;
            }
            return reg == entries ? std::shared_ptr<NC::SBNKInstrument>{} : instruments[reg];
        }
        default:
            return {};
        }
    }

    void Track::PlayNote(std::int32_t midiKey, std::int32_t velocity, std::int32_t length)
    {
        Channel* channel = nullptr;

        if ((_flags & TrackFlag::Tie) == TrackFlag::Tie && !channels.empty())
        {
            if (_player == nullptr)
                ThrowNullReference();
            const auto playerChannels = _player->Channels();
            const std::int32_t channelId = channels[0];
            if (channelId < 0)
                ThrowIndexOutOfRange();
            channel = RawChannel(SpanAt(playerChannels, static_cast<std::size_t>(channelId)));
            if (channel == nullptr)
                ThrowNullReference();
            channel->MidiKey(static_cast<std::uint8_t>(midiKey));
            channel->Velocity(static_cast<std::uint8_t>(velocity));
        }

        if (channel == nullptr)
        {
            const auto noteDef = ReadInstrumentData(program, midiKey);
            if (!noteDef)
                return;

            std::uint32_t allowedChannels;
            switch (noteDef->Record())
            {
            case 1:
                allowedChannels = 0xFFFFU;
                break;
            case 2:
                allowedChannels = 0x3F00U;
                break;
            case 3:
                allowedChannels = 0xC000U;
                break;
            default:
                return;
            }

            if (_player == nullptr)
                ThrowNullReference();
            allowedChannels &= _player->ChannelMask();

            auto allocated = _player->AllocateChannel(
                allowedChannels,
                static_cast<std::int32_t>(_player->Priority()) + priority,
                [this](Channel* callbackChannel, bool free)
                {
                    ChannelCallback(callbackChannel, free);
                });
            channel = RawChannel(allocated);
            if (channel == nullptr)
                return;

            if (!channel->NoteOn(
                    midiKey,
                    velocity,
                    ((_flags & TrackFlag::Tie) == TrackFlag::Tie) ? -1 : length,
                    noteDef.get()))
            {
                channel->Priority(0);
                channel->Free();
                return;
            }

            channels.insert(channels.begin(), channel->Id());
        }

        if (envelopeAttack != 0xFF)
            channel->SetAttack(envelopeAttack);

        if (envelopeDecay != 0xFF)
            channel->SetDecay(envelopeDecay);

        if (envelopeSustain != 0xFF)
            channel->SetSustain(envelopeSustain);

        if (envelopeRelease != 0xFF)
            channel->SetRelease(envelopeRelease);

        channel->SweepPitch(sweepPitch);
        if ((_flags & TrackFlag::Portamento) == TrackFlag::Portamento)
        {
            const std::int32_t keyDelta = WrapSub32(portamentoKey, midiKey);
            const std::int16_t portamentoPitch = WrapInt16(WrapShiftLeft32(keyDelta, 6));
            channel->SweepPitch(WrapInt16(
                static_cast<std::int32_t>(channel->SweepPitch()) + portamentoPitch));
        }

        if (portamentoTime != 0)
        {
            std::int32_t swp = static_cast<std::int32_t>(portamentoTime) * portamentoTime;
            const std::int32_t channelSweepPitch = channel->SweepPitch();
            swp = WrapMul32(
                swp,
                channelSweepPitch < 0 ? -channelSweepPitch : channelSweepPitch);
            swp = ArithmeticShiftRight32(swp, 11);
            channel->SweepLength(swp);
        }
        else
        {
            channel->SweepLength(length);
            channel->Flags(channel->Flags() & ~ChannelFlag::AutoSweep);
        }

        channel->SweepCounter(0);
    }
}
