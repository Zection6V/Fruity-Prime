#include "Track.hpp"

#include "../Channel.hpp"
#include "../Player.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>

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

    [[nodiscard]] constexpr bool HasFlag(NCSFCommon::TrackFlag value, NCSFCommon::TrackFlag flag) noexcept
    {
        return (value & flag) == flag;
    }

    [[nodiscard]] constexpr bool HasFlag(NCSFCommon::ChannelFlag value, NCSFCommon::ChannelFlag flag) noexcept
    {
        return (value & flag) == flag;
    }

    [[nodiscard]] constexpr std::int8_t WrapInt8(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
    }

    [[nodiscard]] constexpr std::int16_t WrapInt16(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
    }

    [[nodiscard]] constexpr std::int32_t WrapShiftLeft32(std::int32_t value, std::uint32_t count) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(value) << (count & 31U));
    }

    [[nodiscard]] constexpr std::int32_t ArithmeticShiftRight32(std::int32_t value, std::uint32_t count) noexcept
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

    [[nodiscard]] NCSFCommon::Channel* ChannelAt(
        std::span<const std::shared_ptr<NCSFCommon::Channel>> channels,
        std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= channels.size())
            ThrowIndexOutOfRange();
        NCSFCommon::Channel* channel = channels[static_cast<std::size_t>(index)].get();
        if (channel == nullptr)
            ThrowNullReference();
        return channel;
    }

    [[nodiscard]] std::int16_t VariableAt(std::span<const std::int16_t> variables, std::uint8_t index)
    {
        if (static_cast<std::size_t>(index) >= variables.size())
            ThrowIndexOutOfRange();
        return variables[static_cast<std::size_t>(index)];
    }
}

namespace NCSFPlayer
{
    bool Track::StepTicks()
    {
        NCSFCommon::Player* playerForChannels = Player();
        if (playerForChannels == nullptr)
            ThrowNullReference();
        const auto playerChannels = playerForChannels->Channels();
        for (const std::int32_t channelId : channels)
        {
            NCSFCommon::Channel* channel = ChannelAt(playerChannels, channelId);

            if (channel->Length() > 0)
                channel->Length(channel->Length() - 1);

            if (!HasFlag(channel->Flags(), NCSFCommon::ChannelFlag::AutoSweep)
                && channel->SweepCounter() < channel->SweepLength())
            {
                channel->SweepCounter(channel->SweepCounter() + 1);
            }
        }

        if (HasFlag(Flags(), NCSFCommon::TrackFlag::NoteFinishWait))
        {
            if (!channels.empty())
                return true;
            Flags(Flags() & ~NCSFCommon::TrackFlag::NoteFinishWait);
        }

        if (wait > 0 && --wait > 0)
            return true;

        while (wait == 0 && !HasFlag(Flags(), NCSFCommon::TrackFlag::NoteFinishWait))
        {
            bool runCmd = true;
            bool hasValueType = false;
            NCSFCommon::ValueType valueType = NCSFCommon::ValueType::U8;

            std::uint8_t cmd = ReadU8();

            if (cmd == static_cast<std::uint8_t>(SSEQCommand::If))
            {
                cmd = ReadU8();
                runCmd = HasFlag(Flags(), NCSFCommon::TrackFlag::Compare);
            }

            if (cmd == static_cast<std::uint8_t>(SSEQCommand::Random))
            {
                cmd = ReadU8();
                valueType = NCSFCommon::ValueType::Random;
                hasValueType = true;
            }

            if (cmd == static_cast<std::uint8_t>(SSEQCommand::FromVariable))
            {
                cmd = ReadU8();
                valueType = NCSFCommon::ValueType::Variable;
                hasValueType = true;
            }

            if ((cmd & 0x80U) == 0U)
            {
                const std::int32_t par = ReadU8();
                const std::int32_t length = ParseValue(
                    hasValueType ? valueType : NCSFCommon::ValueType::VLV);

                if (runCmd)
                {
                    const std::int32_t midiKey = std::clamp(
                        static_cast<std::int32_t>(cmd) + static_cast<std::int32_t>(transpose),
                        0,
                        127);

                    PlayNote(midiKey, par, length > 0 ? length : -1);

                    portamentoKey = static_cast<std::uint8_t>(midiKey);

                    if (HasFlag(Flags(), NCSFCommon::TrackFlag::NoteWait))
                    {
                        wait = length;
                        if (length == 0)
                            Flags(Flags() | NCSFCommon::TrackFlag::NoteFinishWait);
                    }
                }

                continue;
            }

            switch (cmd & 0xF0U)
            {
            case 0x80U:
            {
                const std::int32_t par = ParseValue(
                    hasValueType ? valueType : NCSFCommon::ValueType::VLV);
                if (runCmd)
                {
                    switch (static_cast<SSEQCommand>(cmd))
                    {
                    case SSEQCommand::Rest:
                        wait = par;
                        break;
                    case SSEQCommand::Patch:
                        if (par < 0x10000)
                            program = static_cast<std::uint16_t>(par);
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
            case 0x90U:
                switch (static_cast<SSEQCommand>(cmd))
                {
                case SSEQCommand::OpenTrack:
                {
                    const std::int32_t par = ReadU8();
                    const std::uint32_t off = ReadU24();
                    if (runCmd)
                    {
                        NCSFCommon::Player* player = Player();
                        if (player == nullptr)
                            ThrowNullReference();
                        const auto newTrack = player->GetTrack(par);
                        if (newTrack && newTrack.get() != this)
                        {
                            newTrack->Stop();
                            newTrack->Start(base, static_cast<std::int32_t>(off));
                        }
                    }
                    break;
                }
                case SSEQCommand::Goto:
                {
                    const std::uint32_t off = ReadU24();
                    if (runCmd)
                        CurrentPos(static_cast<std::int32_t>(off));
                    break;
                }
                case SSEQCommand::Call:
                {
                    const std::uint32_t off = ReadU24();
                    if (runCmd && callStackDepth < MaxCall)
                    {
                        positionCallStack[callStackDepth++] = CurrentPos();
                        CurrentPos(static_cast<std::int32_t>(off));
                    }
                    break;
                }
                default:
                    break;
                }
                break;
            case 0xC0U:
            case 0xD0U:
            {
                const std::uint8_t par = static_cast<std::uint8_t>(ParseValue(
                    hasValueType ? valueType : NCSFCommon::ValueType::U8));
                if (runCmd)
                {
                    switch (static_cast<SSEQCommand>(cmd))
                    {
                    case SSEQCommand::Volume:
                        volume = par;
                        break;
                    case SSEQCommand::Expression:
                        expression = par;
                        break;
                    case SSEQCommand::MasterVolume:
                    {
                        NCSFCommon::Player* player = Player();
                        if (player == nullptr)
                            ThrowNullReference();
                        player->Volume(par);
                        break;
                    }
                    case SSEQCommand::PitchBendRange:
                        bendRange = par;
                        break;
                    case SSEQCommand::Priority:
                        priority = par;
                        break;
                    case SSEQCommand::NoteWait:
                        if (par != 0U)
                            Flags(Flags() | NCSFCommon::TrackFlag::NoteWait);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::NoteWait);
                        break;
                    case SSEQCommand::PortamentoTime:
                        portamentoTime = par;
                        break;
                    case SSEQCommand::ModulationDepth:
                        if (!modulation)
                            ThrowNullReference();
                        modulation->Depth(par);
                        break;
                    case SSEQCommand::ModulationSpeed:
                        if (!modulation)
                            ThrowNullReference();
                        modulation->Speed(par);
                        break;
                    case SSEQCommand::ModulationType:
                        if (!modulation)
                            ThrowNullReference();
                        modulation->Target(static_cast<NCSFCommon::LFOTarget>(par));
                        break;
                    case SSEQCommand::ModulationRange:
                        if (!modulation)
                            ThrowNullReference();
                        modulation->Range(par);
                        break;
                    case SSEQCommand::Attack:
                        envelopeAttack = par;
                        break;
                    case SSEQCommand::Decay:
                        envelopeDecay = par;
                        break;
                    case SSEQCommand::Sustain:
                        envelopeSustain = par;
                        break;
                    case SSEQCommand::Release:
                        envelopeRelease = par;
                        break;
                    case SSEQCommand::LoopStart:
                        if (callStackDepth < MaxCall)
                        {
                            positionCallStack[callStackDepth] = CurrentPos();
                            loopCount[callStackDepth++] = par;
                        }
                        break;
                    case SSEQCommand::Tie:
                        if (par != 0U)
                            Flags(Flags() | NCSFCommon::TrackFlag::Tie);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::Tie);
                        ReleaseChannels(-1);
                        FreeChannels();
                        break;
                    case SSEQCommand::PortamentoKey:
                        portamentoKey = static_cast<std::uint8_t>(
                            static_cast<std::int32_t>(par) + static_cast<std::int32_t>(transpose));
                        Flags(Flags() | NCSFCommon::TrackFlag::Portamento);
                        break;
                    case SSEQCommand::PortamentoFlag:
                        if (par != 0U)
                            Flags(Flags() | NCSFCommon::TrackFlag::Portamento);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::Portamento);
                        break;
                    case SSEQCommand::Transpose:
                        transpose = WrapInt8(par);
                        break;
                    case SSEQCommand::PitchBend:
                        pitchBend = WrapInt8(par);
                        break;
                    case SSEQCommand::Pan:
                        pan = WrapInt8(static_cast<std::int32_t>(par) - 0x40);
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
            case 0xE0U:
            {
                const std::int16_t par = WrapInt16(ParseValue(
                    hasValueType ? valueType : NCSFCommon::ValueType::U16));
                if (runCmd)
                {
                    switch (static_cast<SSEQCommand>(cmd))
                    {
                    case SSEQCommand::SweepPitch:
                        sweepPitch = par;
                        break;
                    case SSEQCommand::Tempo:
                    {
                        NCSFCommon::Player* player = Player();
                        if (player == nullptr)
                            ThrowNullReference();
                        player->Tempo(static_cast<std::uint16_t>(par));
                        break;
                    }
                    case SSEQCommand::ModulationDelay:
                        if (!modulation)
                            ThrowNullReference();
                        modulation->Delay(static_cast<std::uint16_t>(par));
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
            case 0xB0U:
            {
                const std::uint8_t varNum = ReadU8();

                std::int16_t par = WrapInt16(ParseValue(
                    hasValueType ? valueType : NCSFCommon::ValueType::U16));
                if (runCmd)
                {
                    NCSFCommon::Player* variablePlayer = Player();
                    if (variablePlayer == nullptr)
                        ThrowNullReference();
                    std::int16_t var = VariableAt(variablePlayer->Variables(), varNum);
                    switch (static_cast<SSEQCommand>(cmd))
                    {
                    case SSEQCommand::SetVariable:
                        var = par;
                        break;
                    case SSEQCommand::AddVariable:
                        var = WrapInt16(static_cast<std::int32_t>(var) + static_cast<std::int32_t>(par));
                        break;
                    case SSEQCommand::SubtractVariable:
                        var = WrapInt16(static_cast<std::int32_t>(var) - static_cast<std::int32_t>(par));
                        break;
                    case SSEQCommand::MultiplyVariable:
                        var = WrapInt16(static_cast<std::int32_t>(var) * static_cast<std::int32_t>(par));
                        break;
                    case SSEQCommand::DivideVariable:
                        if (par != 0)
                        {
                            var = WrapInt16(
                                static_cast<std::int32_t>(var) / static_cast<std::int32_t>(par));
                        }
                        break;
                    case SSEQCommand::ShiftVariable:
                        if (par >= 0)
                        {
                            var = WrapInt16(WrapShiftLeft32(
                                static_cast<std::int32_t>(var),
                                static_cast<std::uint32_t>(par)));
                        }
                        else
                        {
                            const std::uint32_t shift = static_cast<std::uint32_t>(
                                -static_cast<std::int32_t>(par));
                            var = WrapInt16(ArithmeticShiftRight32(
                                static_cast<std::int32_t>(var), shift));
                        }
                        break;
                    case SSEQCommand::RandomizeVariable:
                    {
                        bool neg = false;
                        if (par < 0)
                        {
                            neg = true;
                            par = WrapInt16(-static_cast<std::int32_t>(par));
                        }
                        std::int32_t random = CalculateRandom();
                        random = ArithmeticShiftRight32(
                            random * (static_cast<std::int32_t>(par) + 1), 16U);
                        if (neg)
                            random = -random;
                        var = WrapInt16(random);
                        break;
                    }
                    case SSEQCommand::CompareEquals:
                        if (var == par)
                            Flags(Flags() | NCSFCommon::TrackFlag::Compare);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::Compare);
                        break;
                    case SSEQCommand::CompareGreaterThanOrEquals:
                        if (var >= par)
                            Flags(Flags() | NCSFCommon::TrackFlag::Compare);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::Compare);
                        break;
                    case SSEQCommand::CompareGreaterThan:
                        if (var > par)
                            Flags(Flags() | NCSFCommon::TrackFlag::Compare);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::Compare);
                        break;
                    case SSEQCommand::CompareLessThanOrEquals:
                        if (var <= par)
                            Flags(Flags() | NCSFCommon::TrackFlag::Compare);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::Compare);
                        break;
                    case SSEQCommand::CompareLessThan:
                        if (var < par)
                            Flags(Flags() | NCSFCommon::TrackFlag::Compare);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::Compare);
                        break;
                    case SSEQCommand::CompareNotEquals:
                        if (var != par)
                            Flags(Flags() | NCSFCommon::TrackFlag::Compare);
                        else
                            Flags(Flags() & ~NCSFCommon::TrackFlag::Compare);
                        break;
                    default:
                        break;
                    }

                    NCSFCommon::Player* setVariablePlayer = Player();
                    if (setVariablePlayer == nullptr)
                        ThrowNullReference();
                    setVariablePlayer->SetVariable(WrapInt8(varNum), var);
                }
                break;
            }
            case 0xF0U:
                if (runCmd)
                {
                    switch (static_cast<SSEQCommand>(cmd))
                    {
                    case SSEQCommand::Return:
                        if (callStackDepth != 0U)
                            CurrentPos(positionCallStack[--callStackDepth]);
                        break;
                    case SSEQCommand::LoopEnd:
                        if (callStackDepth != 0U)
                        {
                            std::uint8_t count = loopCount[callStackDepth - 1U];
                            if (count != 0U && --count == 0U)
                            {
                                --callStackDepth;
                                break;
                            }
                            loopCount[callStackDepth - 1U] = count;
                            CurrentPos(positionCallStack[callStackDepth - 1U]);
                        }
                        break;
                    case SSEQCommand::End:
                        return false;
                    default:
                        break;
                    }
                }
                break;
            default:
                break;
            }
        }

        return true;
    }
}
