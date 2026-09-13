#pragma once

#include "NC/SSEQ.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace NCSFCommon
{
    class Channel;
    class LFOParam;
    class Player;

    namespace NC
    {
        class SBNKInstrument;
    }

    enum class TrackFlag : std::uint8_t
    {
        Active = 1U << 0,
        NoteWait = 1U << 1,
        Tie = 1U << 2,
        NoteFinishWait = 1U << 3,
        Portamento = 1U << 4,
        Compare = 1U << 5
    };

    [[nodiscard]] constexpr TrackFlag operator|(TrackFlag left, TrackFlag right) noexcept
    {
        return static_cast<TrackFlag>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr TrackFlag operator&(TrackFlag left, TrackFlag right) noexcept
    {
        return static_cast<TrackFlag>(
            static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr TrackFlag operator~(TrackFlag value) noexcept
    {
        return static_cast<TrackFlag>(static_cast<std::uint8_t>(
            ~static_cast<std::uint8_t>(value)));
    }

    constexpr TrackFlag& operator|=(TrackFlag& left, TrackFlag right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr TrackFlag& operator&=(TrackFlag& left, TrackFlag right) noexcept
    {
        left = left & right;
        return left;
    }

    enum class ValueType : std::uint8_t
    {
        U8,
        U16,
        VLV,
        Variable,
        Random
    };

    class Track
    {
    public:
        Track();
        virtual ~Track();

        Track(const Track&) = delete;
        Track(Track&&) = delete;
        Track& operator=(const Track&) = delete;
        Track& operator=(Track&&) = delete;

    protected:
        static constexpr std::int32_t MaxCall = 3;

    public:
        [[nodiscard]] TrackFlag Flags() const noexcept;
        void Flags(TrackFlag value) noexcept;

    protected:
        std::uint8_t panRange = 0;
        std::uint16_t program = 0;

        std::uint8_t volume = 0;

    public:
        [[nodiscard]] std::uint8_t Volume() const noexcept;
        void Volume(std::uint8_t value) noexcept;

    protected:
        std::uint8_t expression = 0;
        std::int8_t pitchBend = 0;
        std::uint8_t bendRange = 0;

        std::int8_t pan = 0;
        std::uint8_t envelopeAttack = 0;
        std::uint8_t envelopeDecay = 0;
        std::uint8_t envelopeSustain = 0;
        std::uint8_t envelopeRelease = 0;
        std::uint8_t priority = 0;
        std::int8_t transpose = 0;

        std::uint8_t portamentoKey = 0;
        std::uint8_t portamentoTime = 0;
        std::int16_t sweepPitch = 0;

        std::shared_ptr<LFOParam> modulation;

        std::int32_t wait = 0;

        NC::SSEQReadOnlyMemory base;

    public:
        [[nodiscard]] std::int32_t CurrentPos() const noexcept;
        void CurrentPos(std::int32_t value) noexcept;

    protected:
        std::array<std::int32_t, MaxCall> positionCallStack{};
        std::array<std::uint8_t, MaxCall> loopCount{};
        std::uint8_t callStackDepth = 0;

    public:
        [[nodiscard]] NCSFCommon::Player* Player() const noexcept;
        void Player(NCSFCommon::Player* value) noexcept;

        [[nodiscard]] std::uint8_t Id() const noexcept;
        void Id(std::uint8_t value) noexcept;

        [[nodiscard]] bool Mute() const noexcept;
        void Mute(bool value) noexcept;

    protected:
        std::vector<std::int32_t> channels;

    public:
        [[nodiscard]] std::uint8_t ReadU8();
        [[nodiscard]] std::uint16_t ReadU16();
        [[nodiscard]] std::uint32_t ReadU24();
        [[nodiscard]] std::int32_t ReadVLV();

    private:
        static std::uint32_t RandomU;

    protected:
        [[nodiscard]] static std::uint16_t CalculateRandom() noexcept;

    public:
        [[nodiscard]] std::int32_t ParseValue(ValueType valueType);

        virtual void Init();
        void Start(NC::SSEQReadOnlyMemory data, std::int32_t offset);
        void ReleaseChannels(std::int32_t release);
        void FreeChannels();
        void Stop();

    private:
        void ChannelCallback(Channel* channel, bool free);

    public:
        void UpdateChannel(bool release);

        [[nodiscard]] std::shared_ptr<NC::SBNKInstrument> ReadInstrumentData(
            std::int32_t program, std::int32_t midiKey);
        void PlayNote(std::int32_t midiKey, std::int32_t velocity, std::int32_t length);

    protected:
        enum class SSEQCommand : std::uint8_t
        {
            AllocateTrack = 0xFE,
            OpenTrack = 0x93,

            Rest = 0x80,
            Patch = 0x81,
            Pan = 0xC0,
            Volume = 0xC1,
            MasterVolume = 0xC2,
            Priority = 0xC6,
            NoteWait = 0xC7,
            Tie = 0xC8,
            Expression = 0xD5,
            Tempo = 0xE1,
            End = 0xFF,

            Goto = 0x94,
            Call = 0x95,
            Return = 0xFD,
            LoopStart = 0xD4,
            LoopEnd = 0xFC,

            Transpose = 0xC3,
            PitchBend = 0xC4,
            PitchBendRange = 0xC5,

            Attack = 0xD0,
            Decay = 0xD1,
            Sustain = 0xD2,
            Release = 0xD3,

            PortamentoKey = 0xC9,
            PortamentoFlag = 0xCE,
            PortamentoTime = 0xCF,
            SweepPitch = 0xE3,

            ModulationDepth = 0xCA,
            ModulationSpeed = 0xCB,
            ModulationType = 0xCC,
            ModulationRange = 0xCD,
            ModulationDelay = 0xE0,

            Random = 0xA0,
            PrintVariable = 0xD6,
            If = 0xA2,
            FromVariable = 0xA1,
            SetVariable = 0xB0,
            AddVariable = 0xB1,
            SubtractVariable = 0xB2,
            MultiplyVariable = 0xB3,
            DivideVariable = 0xB4,
            ShiftVariable = 0xB5,
            RandomizeVariable = 0xB6,
            CompareEquals = 0xB8,
            CompareGreaterThanOrEquals = 0xB9,
            CompareGreaterThan = 0xBA,
            CompareLessThanOrEquals = 0xBB,
            CompareLessThan = 0xBC,
            CompareNotEquals = 0xBD,

            Mute = 0xD7
        };

    public:
        [[nodiscard]] virtual bool StepTicks() = 0;

    private:
        TrackFlag _flags = static_cast<TrackFlag>(0);
        std::int32_t _currentPos = 0;
        NCSFCommon::Player* _player = nullptr;
        std::uint8_t _id = 0;
        bool _mute = false;

        friend class Player;
    };
}
