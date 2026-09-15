#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace NCSFCommon
{
    namespace NC
    {
        class SBNKInstrument;
        class SWAV;
    }

    class Player;
    class Channel;

    class NDSSoundRegister
    {
    public:
        NDSSoundRegister() noexcept = default;

        NDSSoundRegister(const NDSSoundRegister&) = delete;
        NDSSoundRegister(NDSSoundRegister&&) = delete;
        NDSSoundRegister& operator=(const NDSSoundRegister&) = delete;
        NDSSoundRegister& operator=(NDSSoundRegister&&) = delete;

        [[nodiscard]] std::uint8_t VolumeMultiplier() const noexcept;
        void VolumeMultiplier(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t VolumeDivisor() const noexcept;
        void VolumeDivisor(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Panning() const noexcept;
        void Panning(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t WaveDuty() const noexcept;
        void WaveDuty(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t RepeatMode() const noexcept;
        void RepeatMode(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Format() const noexcept;
        void Format(std::uint8_t value) noexcept;

        [[nodiscard]] bool Enable() const noexcept;
        void Enable(bool value) noexcept;

        [[nodiscard]] const std::shared_ptr<NC::SWAV>& Source() const noexcept;
        void Source(std::shared_ptr<NC::SWAV> value) noexcept;

        [[nodiscard]] std::uint16_t Timer() const noexcept;
        void Timer(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint16_t PSGX() const noexcept;
        void PSGX(std::uint16_t value) noexcept;

        [[nodiscard]] float PSGLast() const noexcept;
        void PSGLast(float value) noexcept;

        [[nodiscard]] std::uint32_t PSGLastCount() const noexcept;
        void PSGLastCount(std::uint32_t value) noexcept;

        [[nodiscard]] double SamplePosition() const noexcept;
        void SamplePosition(double value) noexcept;

        [[nodiscard]] double SampleIncrease() const noexcept;
        void SampleIncrease(double value) noexcept;

        [[nodiscard]] std::uint32_t LoopStart() const noexcept;
        void LoopStart(std::uint32_t value) noexcept;

        [[nodiscard]] std::uint32_t Length() const noexcept;
        void Length(std::uint32_t value) noexcept;

        [[nodiscard]] std::uint32_t TotalLength() const noexcept;
        void TotalLength(std::uint32_t value) noexcept;

        void ClearControlRegister() noexcept;

    private:
        std::uint8_t _volumeMultiplier = 0;
        std::uint8_t _volumeDivisor = 0;
        std::uint8_t _panning = 0;
        std::uint8_t _waveDuty = 0;
        std::uint8_t _repeatMode = 0;
        std::uint8_t _format = 0;
        bool _enable = false;

        std::shared_ptr<NC::SWAV> _source;

        std::uint16_t _timer = 0;

        std::uint16_t _psgx = 0;
        float _psgLast = 0.0F;
        std::uint32_t _psgLastCount = 0;

        double _samplePosition = 0.0;
        double _sampleIncrease = 0.0;

        std::uint32_t _loopStart = 0;
        std::uint32_t _length = 0;
        std::uint32_t _totalLength = 0;
    };

    enum class LFOTarget : std::uint8_t
    {
        Pitch,
        Volume,
        Pan
    };

    class LFOParam
    {
    public:
        LFOParam() noexcept = default;

        LFOParam(const LFOParam&) = delete;
        LFOParam(LFOParam&&) = delete;
        LFOParam& operator=(const LFOParam&) = delete;
        LFOParam& operator=(LFOParam&&) = delete;

        [[nodiscard]] LFOTarget Target() const noexcept;
        void Target(LFOTarget value) noexcept;

        [[nodiscard]] std::uint8_t Speed() const noexcept;
        void Speed(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Depth() const noexcept;
        void Depth(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Range() const noexcept;
        void Range(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint16_t Delay() const noexcept;
        void Delay(std::uint16_t value) noexcept;

        void CopyTo(LFOParam* other) const;

    private:
        LFOTarget _target = LFOTarget::Pitch;
        std::uint8_t _speed = 16;
        std::uint8_t _depth = 0;
        std::uint8_t _range = 1;
        std::uint16_t _delay = 0;
    };

    class LFO
    {
    public:
        LFO();

        LFO(const LFO&) = delete;
        LFO(LFO&&) = delete;
        LFO& operator=(const LFO&) = delete;
        LFO& operator=(LFO&&) = delete;

        [[nodiscard]] const std::shared_ptr<LFOParam>& Param() const noexcept;
        void Param(std::shared_ptr<LFOParam> value) noexcept;

        [[nodiscard]] std::uint16_t DelayCounter() const noexcept;
        void DelayCounter(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint16_t Counter() const noexcept;
        void Counter(std::uint16_t value) noexcept;

        void Start() noexcept;
        void Update();
        [[nodiscard]] std::int32_t GetValue() const;

    private:
        static const std::array<std::int8_t, 33> SinTable;

        [[nodiscard]] static std::int8_t SinIndex(std::int32_t x);

        std::shared_ptr<LFOParam> _param;
        std::uint16_t _delayCounter = 0;
        std::uint16_t _counter = 0;
    };

    enum class ChannelType : std::uint8_t
    {
        PCM,
        PSG,
        Noise
    };

    enum class ChannelState : std::uint8_t
    {
        Attack,
        Decay,
        Sustain,
        Release
    };

    enum class ChannelFlag : std::uint8_t
    {
        Active = 1U << 0,
        Start = 1U << 1,
        AutoSweep = 1U << 2
    };

    enum class ChannelSyncFlag : std::uint8_t
    {
        Stop = 1U << 0,
        Start = 1U << 1,
        Timer = 1U << 2,
        Volume = 1U << 3,
        Pan = 1U << 4
    };

    [[nodiscard]] constexpr ChannelFlag operator|(ChannelFlag left, ChannelFlag right) noexcept
    {
        return static_cast<ChannelFlag>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr ChannelFlag operator&(ChannelFlag left, ChannelFlag right) noexcept
    {
        return static_cast<ChannelFlag>(
            static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr ChannelFlag operator~(ChannelFlag value) noexcept
    {
        return static_cast<ChannelFlag>(static_cast<std::uint8_t>(
            ~static_cast<std::uint8_t>(value)));
    }

    constexpr ChannelFlag& operator|=(ChannelFlag& left, ChannelFlag right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr ChannelFlag& operator&=(ChannelFlag& left, ChannelFlag right) noexcept
    {
        left = left & right;
        return left;
    }

    [[nodiscard]] constexpr ChannelSyncFlag operator|(ChannelSyncFlag left, ChannelSyncFlag right) noexcept
    {
        return static_cast<ChannelSyncFlag>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr ChannelSyncFlag operator&(ChannelSyncFlag left, ChannelSyncFlag right) noexcept
    {
        return static_cast<ChannelSyncFlag>(
            static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr ChannelSyncFlag operator~(ChannelSyncFlag value) noexcept
    {
        return static_cast<ChannelSyncFlag>(static_cast<std::uint8_t>(
            ~static_cast<std::uint8_t>(value)));
    }

    constexpr ChannelSyncFlag& operator|=(ChannelSyncFlag& left, ChannelSyncFlag right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr ChannelSyncFlag& operator&=(ChannelSyncFlag& left, ChannelSyncFlag right) noexcept
    {
        left = left & right;
        return left;
    }

    class Channel
    {
    public:
        Channel() noexcept = default;
        virtual ~Channel() = default;

        Channel(const Channel&) = delete;
        Channel(Channel&&) = delete;
        Channel& operator=(const Channel&) = delete;
        Channel& operator=(Channel&&) = delete;

        [[nodiscard]] std::uint8_t Id() const noexcept;
        void Id(std::uint8_t value) noexcept;

        [[nodiscard]] ChannelState EnvelopeStatus() const noexcept;
        void EnvelopeStatus(ChannelState value) noexcept;

        [[nodiscard]] ChannelFlag Flags() const noexcept;
        void Flags(ChannelFlag value) noexcept;

        [[nodiscard]] ChannelSyncFlag SyncFlags() const noexcept;
        void SyncFlags(ChannelSyncFlag value) noexcept;

        [[nodiscard]] std::uint8_t PanRange() const noexcept;
        void PanRange(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t MidiKey() const noexcept;
        void MidiKey(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Velocity() const noexcept;
        void Velocity(std::uint8_t value) noexcept;

        [[nodiscard]] std::int8_t UserPan() const noexcept;
        void UserPan(std::int8_t value) noexcept;

        [[nodiscard]] std::int16_t UserDecay() const noexcept;
        void UserDecay(std::int16_t value) noexcept;

        [[nodiscard]] std::int16_t UserPitch() const noexcept;
        void UserPitch(std::int16_t value) noexcept;

        [[nodiscard]] std::int32_t SweepCounter() const noexcept;
        void SweepCounter(std::int32_t value) noexcept;

        [[nodiscard]] std::int32_t SweepLength() const noexcept;
        void SweepLength(std::int32_t value) noexcept;

        [[nodiscard]] std::uint8_t Priority() const noexcept;
        void Priority(std::uint8_t value) noexcept;

        [[nodiscard]] NCSFCommon::LFO& LFO() noexcept;
        [[nodiscard]] const NCSFCommon::LFO& LFO() const noexcept;

        [[nodiscard]] std::int16_t SweepPitch() const noexcept;
        void SweepPitch(std::int16_t value) noexcept;

        [[nodiscard]] std::int32_t Length() const noexcept;
        void Length(std::int32_t value) noexcept;

        [[nodiscard]] const std::function<void(Channel*, bool)>& Callback() const noexcept;
        void Callback(std::function<void(Channel*, bool)> value) noexcept;

        [[nodiscard]] NCSFCommon::Player* Player() const noexcept;
        void Player(NCSFCommon::Player* value) noexcept;

        [[nodiscard]] NDSSoundRegister& Register() noexcept;
        [[nodiscard]] const NDSSoundRegister& Register() const noexcept;

        void Init(std::int32_t id);
        void Update();

        [[nodiscard]] static std::uint16_t CalculateChannelVolume(std::int32_t value);
        [[nodiscard]] static std::int16_t ConvertSustain(std::int32_t sustain);

        void Main();

        [[nodiscard]] bool StartPCM(std::shared_ptr<NC::SWAV> wave, std::int32_t length);
        [[nodiscard]] bool StartPSG(std::int32_t duty, std::int32_t length);
        [[nodiscard]] bool StartNoise(std::int32_t length);

        [[nodiscard]] std::int32_t UpdateEnvelope();

        void SetAttack(std::int32_t attack);
        void SetDecay(std::int32_t decay);
        void SetSustain(std::int32_t sustain) noexcept;
        void SetRelease(std::int32_t release);

        void Release() noexcept;
        [[nodiscard]] bool IsActive() const noexcept;
        void Free() noexcept;

        void Setup(std::function<void(Channel*, bool)> callback, std::int32_t priority);
        void Start(std::int32_t length);

        [[nodiscard]] std::int32_t VolumeCompare(const Channel* other) const;
        [[nodiscard]] std::int32_t UpdateSweep();
        [[nodiscard]] std::int32_t UpdateLFO();

        [[nodiscard]] bool NoteOn(
            std::int32_t midiKey,
            std::int32_t velocity,
            std::int32_t length,
            const NC::SBNKInstrument* instData);

        void Kill();

        [[nodiscard]] virtual float GenerateSample() = 0;
        virtual void IncrementSample() = 0;

    protected:
        static const std::array<std::array<float, 8>, 8> WaveDutyTable;

    private:
        class CallbackStorage
        {
        public:
            CallbackStorage() noexcept = default;
            CallbackStorage(std::function<void(Channel*, bool)> value) noexcept
                : _value(std::move(value))
            {
            }

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return static_cast<bool>(_value);
            }

            void operator()(Channel* channel, bool free) const
            {
                auto callback = _value;
                callback(channel, free);
            }

            [[nodiscard]] operator const std::function<void(Channel*, bool)>&() const noexcept
            {
                return _value;
            }

        private:
            std::function<void(Channel*, bool)> _value;
        };

        static constexpr std::int32_t SoundVolumeDBMin = -723;

        static const std::array<std::uint8_t, 724> GetVolumeTable;
        static const std::array<std::uint16_t, 768> GetPitchTable;
        static const std::array<std::int16_t, 128> convertSustainLookupTable;
        static const std::array<std::uint8_t, 19> AttackCoefficientTable;
        static const std::array<std::uint8_t, 4> SampleDataShiftTable;

        [[nodiscard]] static std::uint16_t CalculateTimer(std::int32_t timer, std::int32_t pitch);
        [[nodiscard]] static std::uint16_t CalculateDecayCoefficient(std::int32_t vol);

        std::uint8_t _id = 0;
        ChannelType _type = ChannelType::PCM;
        ChannelState _envelopeStatus = ChannelState::Attack;

        ChannelFlag _flags = static_cast<ChannelFlag>(0);
        ChannelSyncFlag _syncFlags = static_cast<ChannelSyncFlag>(0);

        std::uint8_t _panRange = 0;
        std::uint8_t _rootMidiKey = 0;

        std::uint8_t _midiKey = 0;
        std::uint8_t _velocity = 0;
        std::int8_t _initialPan = 0;
        std::int8_t _userPan = 0;

        std::int16_t _userDecay = 0;
        std::int16_t _userPitch = 0;

        std::int32_t _envelopeAttenuation = 0;
        std::int32_t _sweepCounter = 0;
        std::int32_t _sweepLength = 0;

        std::uint8_t _envelopeAttack = 0;
        std::uint8_t _envelopeSustain = 0;
        std::uint16_t _envelopeDecay = 0;
        std::uint16_t _envelopeRelease = 0;
        std::uint8_t _priority = 0;
        std::uint8_t _pan = 0;
        std::uint16_t _volume = 0;
        std::uint16_t _timer = 0;

        NCSFCommon::LFO _lfo;

        std::int16_t _sweepPitch = 0;

        std::int32_t _length = 0;

        std::shared_ptr<NC::SWAV> _waveData;
        std::int32_t _dutyCycle = 0;
        std::uint16_t _waveTimer = 0;

        CallbackStorage _callback;

        NCSFCommon::Player* _player = nullptr;
        NDSSoundRegister _register;
    };
}
