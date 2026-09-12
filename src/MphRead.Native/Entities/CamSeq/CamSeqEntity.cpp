#include "CamSeqEntity.hpp"

#include "../../Features.hpp"
#include "../../GameState.hpp"
#include "../../MemoryArrays.hpp"
#include "../../Messaging.hpp"
#include "../../Scene.hpp"
#include "../../Sound/Music.hpp"
#include "../../Sound/Sfx.hpp"
#include "../Players/PlayerEntity.hpp"
#include "CameraSequence.hpp"

#include <any>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace
{
    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return (static_cast<Underlying>(value) & static_cast<Underlying>(flag))
            == static_cast<Underlying>(flag);
    }

    template <typename TEnum>
    [[nodiscard]] constexpr TEnum SetFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return static_cast<TEnum>(
            static_cast<Underlying>(value) | static_cast<Underlying>(flag));
    }

    template <typename TEnum>
    [[nodiscard]] constexpr TEnum ClearFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return static_cast<TEnum>(
            static_cast<Underlying>(value) & ~static_cast<Underlying>(flag));
    }

    [[nodiscard]] MphRead::MessageObject BoxInt32(std::int32_t value)
    {
        return std::make_shared<const std::any>(value);
    }

    [[nodiscard]] std::int32_t UnboxInt32(const MphRead::MessageObject& value)
    {
        if (!value || !value->has_value())
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        try
        {
            return std::any_cast<std::int32_t>(*value);
        }
        catch (const std::bad_any_cast&)
        {
            throw MphRead::Memory::Detail::InvalidCastException();
        }
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(T* value)
    {
        if (value == nullptr)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] std::size_t CheckedSequenceIndex(std::uint8_t sequenceId)
    {
        const std::size_t index = sequenceId;
        if (index >= 199)
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return index;
    }

    [[nodiscard]] std::int32_t ArithmeticShiftRight(std::int32_t value, std::int32_t count) noexcept
    {
        const std::uint32_t shift = static_cast<std::uint32_t>(count) & 0x1FU;
        if (shift == 0)
        {
            return value;
        }
        const std::uint32_t bits = static_cast<std::uint32_t>(value);
        std::uint32_t shifted = bits >> shift;
        if (value < 0)
        {
            shifted |= UINT32_MAX << (32U - shift);
        }
        return std::bit_cast<std::int32_t>(shifted);
    }
}

namespace MphRead::Entities
{
    CamSeqEntity* CamSeqEntity::_current = nullptr;

    std::array<std::shared_ptr<Formats::CameraSequence>, 199>
        CamSeqEntity::_sequenceData{};

    CamSeqEntity::CamSeqEntity(CameraSequenceEntityData data, Scene* scene)
        : EntityBase(EntityType::CameraSequence, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(
            data.Header.FacingVector,
            data.Header.UpVector,
            data.Header.Position);
        AddPlaceholderModel();

        const std::uint8_t sequenceId = data.SequenceId;
        const std::size_t index = CheckedSequenceIndex(sequenceId);
        std::shared_ptr<Formats::CameraSequence> sequence = _sequenceData[index];
        if (!sequence)
        {
            sequence = Formats::CameraSequence::Load(sequenceId, scene);
            _sequenceData[index] = sequence;
        }
        _sequence = std::move(sequence);
    }

    CameraSequenceEntityData CamSeqEntity::Data() const
    {
        return _data;
    }

    std::shared_ptr<Formats::CameraSequence> CamSeqEntity::Sequence() const noexcept
    {
        return _sequence;
    }

    std::string CamSeqEntity::Name() const
    {
        return RequireReference(_sequence).Name();
    }

    CamSeqEntity* CamSeqEntity::Current() noexcept
    {
        return _current;
    }

    void CamSeqEntity::Current(CamSeqEntity* value) noexcept
    {
        _current = value;
    }

    std::optional<::OpenTK::Mathematics::Vector4> CamSeqEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void CamSeqEntity::ClearData()
    {
        for (std::size_t i = 0; i < _sequenceData.size(); ++i)
        {
            _sequenceData[i].reset();
        }
    }

    void CamSeqEntity::Initialize()
    {
        EntityBase::Initialize();

        assert(_data.PlayerId1 == 0);
        assert(_data.PlayerId2 == 0);
        assert(_data.Entity1 == -1);
        assert(_data.Entity2 == -1);

        if (_data.EndMessageTargetId != -1)
        {
            RequireReference(_scene).TryGetEntity(
                _data.EndMessageTargetId, _endMessageTarget);
        }

        RequireReference(_sequence).Initialize();
    }

    bool CamSeqEntity::Process()
    {
        if (!_active)
        {
            return EntityBase::Process();
        }

        if (_handoffTimer > 0)
        {
            --_handoffTimer;
            if (_handoffTimer == 0)
            {
                Cancel();
                return EntityBase::Process();
            }
        }

        const std::int32_t delayFrames
            = static_cast<std::int32_t>(_data.DelayFrames) * 2;
        if (static_cast<std::int32_t>(_delayTimer) <= delayFrames)
        {
            TryStart();
        }

        if (static_cast<std::int32_t>(_delayTimer) > delayFrames)
        {
            Formats::CameraSequence& sequence = RequireReference(_sequence);
            sequence.Process();

            if (TestFlag(sequence.Flags(), Formats::CamSeqFlags::CanEnd))
            {
                if (_data.Loop != 0)
                {
                    if (Bugfixes::SmoothCamSeqHandoff)
                    {
                        sequence.Restart(
                            sequence.TransitionTimer(),
                            sequence.TransitionTime());
                    }
                    else
                    {
                        const std::uint16_t transitionTimer
                            = sequence.TransitionTimer();
                        sequence.Restart();
                        sequence.TransitionTimer(transitionTimer);
                    }
                }
                else
                {
                    const std::size_t index
                        = static_cast<std::size_t>(_data.SequenceId);
                    const std::int32_t sfxData
                        = Formats::CameraSequence::SfxData[index];

                    if ((sfxData & 0x4000) != 0
                        && Sound::Sfx::ForceFieldSfxMute > 0)
                    {
                        --Sound::Sfx::ForceFieldSfxMute;
                    }

                    if ((sfxData & 0x8000) != 0)
                    {
                        RequireReference(PlayerEntity::Main()).RestartLongSfx();
                    }
                    else
                    {
                        RequireReference(PlayerEntity::Main()).RestartTimedSfx();
                    }

                    const std::int32_t musicValue
                        = Formats::CameraSequence::MusicData[index];
                    bool playPausedMusic = musicValue != 0;
                    if (playPausedMusic && (musicValue & 0x2000) != 0)
                    {
                        StorySave* storySave = GameState::StorySave;
                        if (storySave == nullptr)
                        {
                            throw Memory::Detail::NullReferenceException();
                        }
                        const std::int32_t areaId = RequireReference(_scene).AreaId;
                        const std::int32_t shift
                            = Memory::Detail::UncheckedMultiply(2, areaId);
                        const std::int32_t bossFlags
                            = static_cast<std::int32_t>(storySave->BossFlags);
                        playPausedMusic
                            = (ArithmeticShiftRight(bossFlags, shift) & 3) == 0;
                    }
                    if (playPausedMusic && (musicValue & 0x400) != 0)
                    {
                        playPausedMusic = GameState::EscapeTimer == -1
                            || GameState::EscapeState != EscapeState::Escape;
                    }
                    if (playPausedMusic
                        && (musicValue & 0x4000) == 0
                        && (musicValue & 0x8000) == 0)
                    {
                        Music::PlayPausedMusic();
                    }

                    _active = false;
                    sequence.End();
                    _current = nullptr;
                    RequireReference(PlayerEntity::Main()).RefreshExternalCamera();
                    SendEndMessage();
                }
            }
        }

        return EntityBase::Process();
    }

    void CamSeqEntity::TryStart()
    {
        PlayerEntity& player = RequireReference(PlayerEntity::Main());
        if ((player.Health() == 0
                && player.DeathCountdown() > 0
                && _data.BlockInput != 0)
            || GameState::DialogPause)
        {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(_data.SequenceId);
        const std::int32_t musicValue
            = Formats::CameraSequence::MusicData[index];

        bool hasMusic = musicValue != 0;
        if (hasMusic && (musicValue & 0x2000) != 0)
        {
            StorySave* storySave = GameState::StorySave;
            if (storySave == nullptr)
            {
                throw Memory::Detail::NullReferenceException();
            }
            const std::int32_t areaId = RequireReference(_scene).AreaId;
            const std::int32_t shift
                = Memory::Detail::UncheckedMultiply(2, areaId);
            const std::int32_t bossFlags
                = static_cast<std::int32_t>(storySave->BossFlags);
            hasMusic = (ArithmeticShiftRight(bossFlags, shift) & 3) == 0;
        }
        if (hasMusic && (musicValue & 0x400) != 0)
        {
            hasMusic = GameState::EscapeTimer == -1
                || GameState::EscapeState != EscapeState::Escape;
        }

        const std::int32_t sfxData
            = Formats::CameraSequence::SfxData[index];

        if (_delayTimer == 0)
        {
            if (_data.Loop == 0)
            {
                if ((sfxData & 0x2000) != 0)
                {
                    RequireReference(Sound::Sfx::Instance()).StopSoundById(
                        static_cast<std::int32_t>(SfxId::CHIME1));
                }
                if ((sfxData & 0x4000) != 0)
                {
                    Sound::Sfx::ForceFieldSfxMute
                        = Memory::Detail::UncheckedAdd(
                            Sound::Sfx::ForceFieldSfxMute, 1);
                }
                if ((sfxData & 0x8000) != 0)
                {
                    RequireReference(PlayerEntity::Main()).StopLongSfx();
                }
                else
                {
                    RequireReference(PlayerEntity::Main()).StopTimedSfx();
                }
            }

            if (hasMusic && (musicValue & 0x4000) == 0)
            {
                Music::Pause();
            }
        }

        _delayTimer = static_cast<std::uint8_t>(_delayTimer + 1);
        const std::int32_t delayFrames
            = static_cast<std::int32_t>(_data.DelayFrames) * 2;
        if (static_cast<std::int32_t>(_delayTimer) > delayFrames)
        {
            if (hasMusic)
            {
                const std::int32_t musicOrSeqId = musicValue & 0x3FF;
                if ((musicValue & 0x800) != 0)
                {
                    Music::MusicToResume(
                        static_cast<MusicId>(musicOrSeqId));
                }
                else if ((musicValue & 0x1000) != 0
                    && GameState::EscapeTimer != -1
                    && GameState::EscapeState == EscapeState::Escape)
                {
                    Music::PlayMusic(MusicId::SEQ_OREGANO_M55);
                    Music::UpdateEscapeMusic();
                }
                else if ((musicValue & 0x4000) != 0)
                {
                    Music::PlayMusic(
                        static_cast<MusicId>(musicOrSeqId));
                }
                else
                {
                    Music::PlaySeq(
                        static_cast<SeqId>(musicOrSeqId));
                }
            }

            const std::int32_t scriptId = sfxData & 0x1FFF;
            if (scriptId != 0)
            {
                Sound::SfxInstanceBase& sfx
                    = RequireReference(Sound::Sfx::Instance());
                sfx.StopFreeSfxScripts();
                sfx.PlayScript(
                    scriptId | 0x4000,
                    nullptr,
                    false,
                    -1.0F,
                    false,
                    false);
            }

            Start();
        }
    }

    void CamSeqEntity::Start()
    {
        Formats::CameraSequence& sequence = RequireReference(_sequence);

        sequence.Flags(ClearFlag(
            sequence.Flags(), Formats::CamSeqFlags::BlockInput));
        sequence.Flags(ClearFlag(
            sequence.Flags(), Formats::CamSeqFlags::ForceAlt));
        sequence.Flags(ClearFlag(
            sequence.Flags(), Formats::CamSeqFlags::ForceBiped));

        if (_data.BlockInput != 0)
        {
            sequence.Flags(SetFlag(
                sequence.Flags(), Formats::CamSeqFlags::BlockInput));
        }
        if (_data.ForceAltForm != 0)
        {
            sequence.Flags(SetFlag(
                sequence.Flags(), Formats::CamSeqFlags::ForceAlt));
        }
        else if (_data.ForceBipedForm != 0 && _data.BlockInput != 0)
        {
            sequence.Flags(SetFlag(
                sequence.Flags(), Formats::CamSeqFlags::ForceBiped));
        }

        const std::uint16_t transitionTime
            = static_cast<std::uint16_t>(_handoff ? 60 * 2 : 0);
        PlayerEntity& player = RequireReference(PlayerEntity::Main());
        sequence.SetUp(player.CameraInfo(), transitionTime);
        RequireReference(PlayerEntity::Main()).RefreshExternalCamera();
    }

    void CamSeqEntity::Cancel()
    {
        PlayerEntity& player = RequireReference(PlayerEntity::Main());
        player.RestartLongSfx();

        const bool currentSeq
            = Formats::CameraSequence::Current() == _sequence.get();
        Formats::CameraSequence& sequence = RequireReference(_sequence);
        const bool playerCam
            = sequence.CamInfoRef() == &player.CameraInfo();

        SendEndMessage();
        sequence.End();
        _active = false;

        if (currentSeq)
        {
            player.RefreshExternalCamera();
            if (playerCam
                && (player.IsAltForm()
                    || player.IsMorphing()
                    || player.IsUnmorphing()))
            {
                player.ResumeOwnCamera();
            }
        }

        if (_current == this)
        {
            _current = nullptr;
        }
    }

    void CamSeqEntity::SendEndMessage()
    {
        if (_data.EndMessage != Message::None)
        {
            RequireReference(_scene).SendMessage(
                _data.EndMessage,
                this,
                _endMessageTarget.get(),
                BoxInt32(_data.EndMessageParam),
                BoxInt32(0));
        }
    }

    void CamSeqEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Activate
            || (info.Message == Message::SetActive
                && UnboxInt32(info.Param1) != 0))
        {
            PlayerEntity& player = RequireReference(PlayerEntity::Main());
            if (player.Health() == 0
                && player.DeathCountdown() > 0
                && _data.BlockInput != 0)
            {
                return;
            }

            bool activate = true;
            bool handoff = false;
            if (_current != nullptr)
            {
                if (_current->_data.BlockInput != 0)
                {
                    activate = false;
                }
                if (_current->_data.Handoff != 0
                    && _data.Handoff != 0)
                {
                    handoff = true;
                    if (_current->_handoffTimer == 0)
                    {
                        activate = false;
                    }
                }
            }

            Formats::CameraSequence* cameraCurrent
                = Formats::CameraSequence::Current();
            if (cameraCurrent != nullptr
                && TestFlag(
                    cameraCurrent->Flags(),
                    Formats::CamSeqFlags::BlockInput))
            {
                activate = false;
            }

            if (activate)
            {
                if (Cheats::SkipPlanetIntros
                    && (Name() == "unit2_land_intro"
                        || Name() == "unit1_land_intro"
                        || Name() == "unit3_land_intro"
                        || Name() == "unit4_land_intro"
                        || Name() == "gorea_land_intro"))
                {
                    return;
                }

                if (_current != nullptr && _current != this)
                {
                    CamSeqEntity* current = _current;
                    if (handoff)
                    {
                        RequireReference(current->_sequence).CamInfoRef(nullptr);
                    }
                    current->Cancel();
                }

                cameraCurrent = Formats::CameraSequence::Current();
                if (cameraCurrent != nullptr
                    && cameraCurrent != _sequence.get())
                {
                    cameraCurrent->End();
                }

                if (!_active)
                {
                    const bool quickStart
                        = Name() == "unit2_b1_octolith_intro"
                        || Name() == "bigeye_octolith_intro";
                    _active = true;
                    _delayTimer = static_cast<std::uint8_t>(
                        quickStart ? 7 : 0);
                    _handoffTimer = 0;
                    _handoff = handoff;
                    _current = this;

                    if (_data.DelayFrames == 0 || quickStart)
                    {
                        TryStart();
                    }
                }
            }
            else
            {
                const auto& keyframes
                    = RequireReference(_sequence).Keyframes();
                for (std::size_t i = 0; i < keyframes.size(); ++i)
                {
                    const auto& keyframe = RequireReference(keyframes[i]);
                    const Message message
                        = static_cast<Message>(keyframe.MessageId());
                    if (message != Message::None)
                    {
                        RequireReference(_scene).SendMessage(
                            message,
                            nullptr,
                            keyframe.MessageTarget(),
                            BoxInt32(static_cast<std::int32_t>(
                                keyframe.MessageParam())),
                            BoxInt32(0));
                    }
                }
            }
        }
        else if (info.Message == Message::SetActive
            && UnboxInt32(info.Param1) == 0)
        {
            if (_handoffTimer == 0)
            {
                _handoffTimer = 2 * 2;
            }
        }
    }

    void CamSeqEntity::CancelCurrent()
    {
        CamSeqEntity* current = _current;
        if (current != nullptr)
        {
            current->Cancel();
        }
    }
}
