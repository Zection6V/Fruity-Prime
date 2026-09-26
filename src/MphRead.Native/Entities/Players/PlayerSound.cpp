#include "PlayerSound.hpp"

#include "PlayerEntity.hpp"
#include "../CamSeq/CameraSequence.hpp"
#include "../../GameState.hpp"
#include "../../Messaging.hpp"
#include "../../MemoryArrays.hpp"
#include "../../Scene.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Sound/Music.hpp"
#include "../../Sound/Sfx.hpp"
#include "../../Utility/Rng.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <any>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::ManagedListAt;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::UncheckedSubtract;
using ::MphRead::TestFlag;

namespace
{
    [[nodiscard]] std::int32_t SoundTableValue(
        const std::shared_ptr<std::vector<std::vector<std::int32_t>>>& values,
        std::int32_t row, std::int32_t column)
    {
        const auto& table = RequireReference(values);
        return ManagedAt(ManagedAt(table, row), column);
    }

    [[nodiscard]] std::int32_t HunterSfxValue(MphRead::Hunter hunter, MphRead::HunterSfx sfx)
    {
        return SoundTableValue(
            MphRead::Metadata::HunterSfx(),
            static_cast<std::int32_t>(hunter),
            static_cast<std::int32_t>(sfx));
    }

    [[nodiscard]] std::int32_t BeamSfxValue(MphRead::BeamType beam, MphRead::BeamSfx sfx)
    {
        return SoundTableValue(
            MphRead::Metadata::BeamSfx(),
            static_cast<std::int32_t>(beam),
            static_cast<std::int32_t>(sfx));
    }

    [[nodiscard]] std::int32_t TerrainSfxValue(MphRead::Terrain terrain, MphRead::TerrainSfx sfx)
    {
        return SoundTableValue(
            MphRead::Metadata::TerrainSfx(),
            static_cast<std::int32_t>(terrain),
            static_cast<std::int32_t>(sfx));
    }

}


namespace MphRead::Entities
{
    void PlayerEntity::PlayHunterSfx(HunterSfx sfx)
    {
        std::int32_t id = HunterSfxValue(_hunter, sfx);
        if (id == -1)
        {
            if (!GameState::Multiplayer() || _hunter != Hunter::Guardian || sfx != HunterSfx::Spawn)
            {
                return;
            }
            id = HunterSfxValue(Hunter::Samus, sfx);
        }
        if (sfx == HunterSfx::Death && IsMainPlayer())
        {
            _soundSource.PlayFreeSfx(id);
        }
        else
        {
            float recency = -1.0F;
            if (sfx == HunterSfx::Damage)
            {
                recency = 5.0F / 30.0F;
            }
            if (!IsMainPlayer())
            {
                if (sfx == HunterSfx::Damage)
                {
                    id = HunterSfxValue(_hunter, HunterSfx::DamageEnemy);
                }
                else if (sfx == HunterSfx::Death)
                {
                    id = HunterSfxValue(_hunter, HunterSfx::DeathEnemy);
                }
            }
            _soundSource.PlaySfx(id, false, false, recency, true);
        }
    }

    std::int32_t PlayerEntity::PlayMissileSfx(HunterSfx sfx)
    {
        const std::int32_t id = HunterSfxValue(_hunter, sfx);
        if (id == -1 || Sound::Sfx::TimedSfxMute > 0)
        {
            return -1;
        }
        return _soundSource.PlayFreeSfx(id);
    }

    void PlayerEntity::PlayRandomDamageSfx()
    {
        assert(IsMainPlayer());
        if (_hunter == Hunter::Samus && _damageSfxTimer == 0.0F)
        {
            const std::uint32_t sfx = Rng::GetRandomInt1(3) + 369U;
            _soundSource.PlaySfx(static_cast<std::int32_t>(sfx));
            _damageSfxTimer = 90.0F / 30.0F;
        }
    }

    void PlayerEntity::PlayBeamEmptySfx(BeamType beam)
    {
        const std::int32_t sfx = BeamSfxValue(beam, BeamSfx::Empty);
        if (sfx != -1)
        {
            _soundSource.PlaySfx(sfx, false, true);
        }
    }

    void PlayerEntity::PlayBeamShotSfx(
        BeamType beam, bool charged, bool continuous, bool homing, float amountA)
    {
        StopBeamChargeSfx(beam);
        if (continuous)
        {
            amountA = homing ? amountA + 0x3FFF : 0.0F;
            _soundSource.PlaySfx(
                BeamSfxValue(beam, BeamSfx::Shot), true, false, -1.0F, false, false, amountA);
            return;
        }

        BeamSfx sfx;
        if (charged)
        {
            sfx = beam == ManagedListAt(
                Weapons::AffinityWeapons, static_cast<std::int32_t>(_hunter))
                ? BeamSfx::AffinityChargeShot
                : BeamSfx::ChargeShot;
        }
        else
        {
            sfx = _hunter == Hunter::Weavel && beam == BeamType::Battlehammer
                ? BeamSfx::AffinityChargeShot
                : BeamSfx::Shot;
        }
        const std::int32_t id = BeamSfxValue(beam, sfx);
        if (id != -1)
        {
            _soundSource.PlaySfx(id);
        }
    }

    std::int32_t PlayerEntity::GetBeamChargeSfx(BeamType beam)
    {
        if (beam == BeamType::Missile)
        {
            return HunterSfxValue(_hunter, HunterSfx::MissileCharge);
        }
        if (beam == BeamType::Judicator && _hunter == Hunter::Noxus)
        {
            return static_cast<std::int32_t>(SfxId::SHOTGUN_CHARGE1_NOX);
        }
        return BeamSfxValue(beam, BeamSfx::Charge);
    }

    void PlayerEntity::PlayBeamChargeSfx(BeamType beam)
    {
        const std::int32_t sfx = GetBeamChargeSfx(beam);
        if (sfx != -1)
        {
            _soundSource.PlaySfx(sfx, true);
        }
    }

    void PlayerEntity::StopBeamChargeSfx(BeamType beam)
    {
        const std::int32_t sfx = GetBeamChargeSfx(beam);
        if (sfx != -1)
        {
            _soundSource.StopSfx(sfx);
        }
    }

    void PlayerEntity::StopContinuousBeamSfx(BeamType beam)
    {
        std::int32_t sfx = BeamSfxValue(beam, BeamSfx::Shot);
        _soundSource.StopSfx(sfx);
        sfx = BeamSfxValue(beam, BeamSfx::AffinityChargeShot);
        _soundSource.StopSfx(sfx);
    }

    void PlayerEntity::UpdateHealthSfx(std::int32_t health)
    {
        if (Sound::Sfx::TimedSfxMute > 0)
        {
            return;
        }
        if (health > 0 && health < 25)
        {
            if (!_soundSource.IsHandlePlaying(_healthSfxHandle))
            {
                _healthSfxHandle = _soundSource.PlayFreeSfx(SfxId::ENERGY_ALARM);
            }
        }
        else if (_healthSfxHandle != -1)
        {
            _soundSource.StopSfxByHandle(_healthSfxHandle);
            _healthSfxHandle = -1;
        }
    }

    void PlayerEntity::UpdateWalkingSfx()
    {
        if (!TestFlag(_flags1, PlayerFlags1::MovingBiped) || _hSpeedMag <= 0.0F)
        {
            _walkSfxTimer = 10 / 30;
            _walkSfxIndex = 0;
            return;
        }

        auto& scene = RequireReference(_scene);
        _walkSfxTimer += scene.FrameTime();
        std::int32_t sfxId = -1;
        if (_walkSfxTimer >= 15.0F / 30.0F)
        {
            if (_walkSfxIndex == 0)
            {
                sfxId = TerrainSfxValue(_standTerrain, TerrainSfx::Walk1);
                _walkSfxIndex = 1;
            }
        }
        if (_walkSfxTimer >= 25.0F / 30.0F)
        {
            assert(_walkSfxIndex == 1);
            sfxId = TerrainSfxValue(_standTerrain, TerrainSfx::Walk2);
            _walkSfxTimer = 5.0F / 30.0F;
            _walkSfxIndex = 0;
        }
        if (_standTerrain == Terrain::Lava && _hunter != Hunter::Spire)
        {
            sfxId = -1;
        }
        const float amountB = static_cast<float>(Rng::GetRandomInt1(0x7FFF) * 2U);
        if (sfxId != -1)
        {
            _soundSource.PlaySfx(
                sfxId, false, false, -1.0F, false, false, 0xFFFF, amountB);
        }
    }

    std::int32_t PlayerEntity::GetAltMovementSfx()
    {
        std::int32_t sfxId;
        if (_hunter == Hunter::Samus)
        {
            sfxId = TerrainSfxValue(_standTerrain, TerrainSfx::Roll);
        }
        else if (_hunter == Hunter::Trace)
        {
            sfxId = TerrainSfxValue(_standTerrain, TerrainSfx::TraceAlt);
        }
        else
        {
            sfxId = HunterSfxValue(_hunter, HunterSfx::Roll);
        }
        return sfxId;
    }

    void PlayerEntity::UpdateAltMovementSfx()
    {
        const float newAmount
            = 0xFFFF * _hSpeedMag / Fixed::ToFloat(_values.AltMinHSpeed);
        UpdateMovementSfxAmount(newAmount);
        const std::int32_t sfxId = GetAltMovementSfx();
        if (sfxId != -1)
        {
            _soundSource.PlaySfx(
                sfxId, true, false, -1.0F, false, false, _moveSfxAmount);
        }
    }

    void PlayerEntity::UpdateSlidingSfx(float newAmount)
    {
        UpdateMovementSfxAmount(newAmount);
        const std::int32_t sfxId = TerrainSfxValue(_standTerrain, TerrainSfx::Slide);
        if (sfxId != -1)
        {
            _soundSource.PlaySfx(
                sfxId, true, false, -1.0F, false, false, _moveSfxAmount);
        }
    }

    void PlayerEntity::UpdateMovementSfxAmount(float newAmount)
    {
        const float prevAmount = _moveSfxAmount;
        if (!TestFlag(_flags1, PlayerFlags1::Grounded))
        {
            newAmount = ExponentialDecay(0.5F, prevAmount);
        }
        else if (RequireReference(_scene).FrameCount() % 2 == 0)
        {
            if (newAmount < prevAmount)
            {
                newAmount = prevAmount + (newAmount - prevAmount) / 4.0F;
            }
            else
            {
                newAmount = prevAmount + (newAmount - prevAmount) / 2.0F;
            }
        }
        else
        {
            newAmount = _moveSfxAmount;
        }
        if (newAmount < 1000.0F)
        {
            newAmount = 0.0F;
        }
        _moveSfxAmount = newAmount;
    }

    void PlayerEntity::StopTerrainSfx(Terrain prevTerrain)
    {
        std::int32_t curSfx;
        std::int32_t prevSfx;
        if (_hunter == Hunter::Samus)
        {
            curSfx = TerrainSfxValue(_standTerrain, TerrainSfx::Roll);
            prevSfx = TerrainSfxValue(prevTerrain, TerrainSfx::Roll);
        }
        else if (_hunter == Hunter::Trace)
        {
            curSfx = TerrainSfxValue(_standTerrain, TerrainSfx::TraceAlt);
            prevSfx = TerrainSfxValue(prevTerrain, TerrainSfx::TraceAlt);
        }
        else
        {
            curSfx = HunterSfxValue(_hunter, HunterSfx::Roll);
            prevSfx = curSfx;
        }
        if (curSfx != prevSfx && prevSfx != -1)
        {
            _soundSource.StopSfx(prevSfx);
        }

        curSfx = TerrainSfxValue(_standTerrain, TerrainSfx::Slide);
        prevSfx = TerrainSfxValue(prevTerrain, TerrainSfx::Slide);
        if (curSfx != prevSfx && prevSfx != -1)
        {
            _soundSource.StopSfx(prevSfx);
        }
    }

    void PlayerEntity::StopAltFormSfx()
    {
        if (IsAltForm())
        {
            const std::int32_t sfxId = TerrainSfxValue(_standTerrain, TerrainSfx::Slide);
            if (sfxId != -1)
            {
                _soundSource.StopSfx(sfxId);
            }
        }
        else
        {
            _soundSource.StopSfx(SfxId::NOX_TOP_ATTACK2);
            _soundSource.StopSfx(SfxId::NOX_TOP_ENERGY_DRAIN2);
            const std::int32_t sfxId = GetAltMovementSfx();
            if (sfxId != -1)
            {
                _soundSource.StopSfx(sfxId);
            }
        }
    }

    void PlayerEntity::PlayLandingSfx()
    {
        if (_timeBeforeLanding > 30)
        {
            ModControllerFeedback(Mods::Input::GamepadFeedback::Landing);
        }
        const std::int32_t sfxId = TerrainSfxValue(_standTerrain, TerrainSfx::Land);
        const float amountA = static_cast<float>(
            UncheckedMultiply(0xFFFF, static_cast<std::int32_t>(_timeBeforeLanding)))
            / (90.0F * 2.0F);
        _soundSource.PlaySfx(
            sfxId, false, false, -1.0F, false, false, amountA);
    }

    void PlayerEntity::UpdateBurningSfx(bool burning)
    {
        const float prevAmount = _burnSfxAmount;
        float newAmount = 0xFFFF;
        if (!burning)
        {
            newAmount = ExponentialDecay(0.875F, prevAmount);
            if (newAmount < 50.0F)
            {
                newAmount = 0.0F;
            }
        }
        if (newAmount > 0.0F)
        {
            _burnSfxAmount = newAmount;
            _soundSource.PlaySfx(
                SfxId::DGN_LAVA_DAMAGE, true, false, -1.0F, false, false, newAmount);
        }
        else if (prevAmount > 0.0F)
        {
            _burnSfxAmount = 0.0F;
            _soundSource.StopSfx(SfxId::DGN_LAVA_DAMAGE);
        }
    }

    void PlayerEntity::UpdateDoubleDamageSfx(std::int32_t index, bool play)
    {
        if (index != -1)
        {
            if (play)
            {
                if (_dblDamageSfxHandle != -1)
                {
                    _soundSource.StopSfxByHandle(_dblDamageSfxHandle);
                    _dblDamageSfxHandle = -1;
                }
                _dblDamageSfxId = ManagedListAt(_dblDamageIds, index);
            }
            else
            {
                if (_dblDamageSfxHandle != -1)
                {
                    _soundSource.StopSfxByHandle(_dblDamageSfxHandle);
                }
                _dblDamageSfxHandle = -1;
                _dblDamageSfxId = SfxId::None;
            }
        }
        else if (_dblDamageSfxHandle != -1)
        {
            _soundSource.StopSfxByHandle(_dblDamageSfxHandle);
        }
    }

    void PlayerEntity::UpdateCloakSfx(std::int32_t index, bool play)
    {
        if (index != -1)
        {
            if (play)
            {
                if (_cloakSfxHandle != -1)
                {
                    _soundSource.StopSfxByHandle(_cloakSfxHandle);
                    _cloakSfxHandle = -1;
                }
                _cloakSfxId = ManagedListAt(_cloakSfxIds, index);
            }
            else
            {
                if (_cloakSfxHandle != -1)
                {
                    _soundSource.StopSfxByHandle(_cloakSfxHandle);
                }
                _cloakSfxHandle = -1;
                _cloakSfxId = SfxId::None;
            }
        }
        else if (_cloakSfxHandle != -1)
        {
            _soundSource.StopSfxByHandle(_cloakSfxHandle);
        }
    }

    void PlayerEntity::StartFlagCarrySfx()
    {
        _flagCarrySfxOn = true;
    }

    void PlayerEntity::StopFlagCarrySfx()
    {
        _soundSource.StopSfxByHandle(_flagCarrySfxHandle);
        _flagCarrySfxHandle = -1;
        _flagCarrySfxOn = false;
    }

    void PlayerEntity::UpdateScanSfx(std::int32_t index, bool enable)
    {
        if (index == -1)
        {
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(_scanSfxOn.size()); ++i)
            {
                if (ManagedAt(_scanSfxOn, i))
                {
                    _soundSource.StopSfxByHandle(ManagedAt(_scanSfxHandles, i));
                    ManagedAt(_scanSfxHandles, i) = -1;
                }
            }
            if (!enable)
            {
                for (std::int32_t i = 0; i < static_cast<std::int32_t>(_scanSfxOn.size()); ++i)
                {
                    ManagedAt(_scanSfxOn, i) = false;
                }
            }
        }
        else if (enable)
        {
            ManagedAt(_scanSfxOn, index) = true;
        }
        else
        {
            _soundSource.StopSfxByHandle(ManagedAt(_scanSfxHandles, index));
            ManagedAt(_scanSfxHandles, index) = -1;
            ManagedAt(_scanSfxOn, index) = false;
        }
    }

    void PlayerEntity::StopAllSfx()
    {
        StopFlagCarrySfx();
        UpdateHealthSfx(0);
        UpdateDoubleDamageSfx(0, false);
        UpdateCloakSfx(0, false);
        UpdateScanSfx(-1, false);
        _soundSource.StopFreeSfxScripts();
        _soundSource.StopFreeSfx(SfxId::FAST_SCROLL_UP_LOOP);
        RequireReference(Sound::Sfx::Instance()).StopEnvironmentSfx();
        RequireReference(Sound::Sfx::Instance()).StopAllSound(false);
    }

    void PlayerEntity::PlayTimedSfx(SfxId id)
    {
        _timedSfxSource.PlaySfx(id, false, false, 0.0F, true);
    }

    void PlayerEntity::StopTimedSfx(SfxId id)
    {
        _timedSfxSource.StopSfx(id);
    }

    void PlayerEntity::StopTimedSfx()
    {
        if (Sound::Sfx::TimedSfxMute == 0)
        {
            _dblDamageSfxMuted = true;
            _cloakSfxMuted = true;
            if (_flagCarrySfxOn)
            {
                _flagCarrySfxOn = false;
                _flagCarrySfxMuted = true;
            }
            UpdateHealthSfx(0);
            UpdateScanSfx(-1, true);
        }
        Sound::Sfx::TimedSfxMute = UncheckedAdd(Sound::Sfx::TimedSfxMute, 1);
    }

    void PlayerEntity::RestartTimedSfx(bool force)
    {
        const bool restart = force
            || (Sound::Sfx::TimedSfxMute
                = UncheckedSubtract(Sound::Sfx::TimedSfxMute, 1)) <= 0;
        if (restart)
        {
            Sound::Sfx::TimedSfxMute = 0;
            _dblDamageSfxMuted = false;
            _cloakSfxMuted = false;
            if (_flagCarrySfxMuted)
            {
                _flagCarrySfxMuted = false;
                _flagCarrySfxOn = true;
            }
        }
    }

    void PlayerEntity::StopLongSfx()
    {
        StopTimedSfx();
        if (Sound::Sfx::LongSfxMute == 0)
        {
            Sound::Sfx::SfxMute = true;
            RequireReference(Sound::Sfx::Instance()).StopEnvironmentSfx();
        }
        Sound::Sfx::LongSfxMute = UncheckedAdd(Sound::Sfx::LongSfxMute, 1);
    }

    void PlayerEntity::RestartLongSfx(bool force)
    {
        RestartTimedSfx(force);
        const bool restart = force
            || (Sound::Sfx::LongSfxMute
                = UncheckedSubtract(Sound::Sfx::LongSfxMute, 1)) <= 0;
        if (restart)
        {
            Sound::Sfx::LongSfxMute = 0;
            Sound::Sfx::SfxMute = false;
        }
    }

    void PlayerEntity::UpdateTimedSounds()
    {
        const auto scene = [&]() -> Scene&
        {
            return RequireReference(_scene);
        };
        _timedSfxSource.Update(Position, -1);

        if (_sfxStopTimer > 0.0F)
        {
            _sfxStopTimer -= scene().FrameTime();
            if (_sfxStopTimer <= 0.0F)
            {
                _sfxStopTimer = 0.0F;
                StopLongSfx();
            }
        }
        if (_damageSfxTimer > 0.0F)
        {
            _damageSfxTimer -= scene().FrameTime();
            if (_damageSfxTimer < 0.0F)
            {
                _damageSfxTimer = 0.0F;
            }
        }
        if (_dblDamageSfxId != SfxId::None
            && !_soundSource.IsHandlePlaying(_dblDamageSfxHandle)
            && !_dblDamageSfxMuted)
        {
            _dblDamageSfxHandle = _soundSource.PlayFreeSfx(_dblDamageSfxId);
        }
        if (_cloakSfxId != SfxId::None
            && !_soundSource.IsHandlePlaying(_cloakSfxHandle)
            && !_cloakSfxMuted)
        {
            _cloakSfxHandle = _soundSource.PlayFreeSfx(_cloakSfxId);
        }
        if (_flagCarrySfxOn && !_soundSource.IsHandlePlaying(_flagCarrySfxHandle))
        {
            _flagCarrySfxHandle = _soundSource.PlayFreeSfx(SfxId::FLAG_CARRIED);
        }

        MusicId musicId = MusicId::Invalid;
        for (std::int32_t i = 0; i < scene().MessageQueue().Count(); ++i)
        {
            const MessageInfo message = scene().MessageQueue()[i];
            if (message.ExecuteFrame != scene().FrameCount())
            {
                continue;
            }
            if (message.Message == Message::PlaySfxScript)
            {
                const std::int32_t id = UnboxInt32(message.Param1);
                if (id == -1)
                {
                    DoorChimeSfxTimer = 2.0F / 30.0F;
                }
                else if (id <= 104)
                {
                    _timedSfxSource.PlaySfx(id | 0x4000, false, false, 0.0F, true);
                }
            }
            else if (message.Message == Message::UpdateMusic)
            {
                const std::int32_t param1 = UnboxInt32(message.Param1);
                const std::int32_t param2 = UnboxInt32(message.Param2);
                musicId = CheckMusicUpdate(musicId, param1, param2);
            }
        }

        if (musicId != MusicId::Invalid
            && RequireReference(PlayerEntity::Main()).Health() > 0
            && (GameState::EscapeTimer() == -1 || GameState::EscapeState() != EscapeState::Escape))
        {
            if (Music::MusicEncounterSuspension() != 0)
            {
                Music::MusicToResume(musicId);
            }
            else if (Sound::Sfx::TimedSfxMute > 0)
            {
                Music::UpdateMusicIdIfPaused(musicId);
            }
            else
            {
                Music::PlayMusic(musicId);
            }
        }

        if (Sound::Sfx::TimedSfxMute == 0)
        {
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(_scanSfxOn.size()); ++i)
            {
                if (ManagedAt(_scanSfxOn, i) && ManagedAt(_scanSfxHandles, i) == -1)
                {
                    ManagedAt(_scanSfxHandles, i)
                        = _soundSource.PlayFreeSfx(ManagedListAt(_scanSfxIds, i));
                }
            }
        }

        if (Sound::Sfx::LongSfxMute == 0 && DoorUnlockSfxTimer > 0.0F)
        {
            DoorUnlockSfxTimer -= scene().FrameTime();
            if (DoorUnlockSfxTimer <= 1.0F / 30.0F)
            {
                DoorUnlockSfxTimer = 0.0F;
                if (_soundSource.CountPlayingSfx(SfxId::UNLOCK_ANIM) == 0)
                {
                    _soundSource.PlayFreeSfx(SfxId::UNLOCK_ANIM);
                }
            }
        }

        if (DoorChimeSfxTimer > 0.0F)
        {
            DoorChimeSfxTimer -= scene().FrameTime();
            if (DoorChimeSfxTimer <= 1.0F / 30.0F)
            {
                DoorChimeSfxTimer = 0.0F;
                if (Sound::Sfx::TimedSfxMute == 0
                    && (Formats::CameraSequence::Current() == nullptr
                        || !RequireReference(Formats::CameraSequence::Current()).BlockInput())
                    && _soundSource.CountPlayingSfx(SfxId::DOOR_UNLOCK) == 0)
                {
                    _soundSource.PlayFreeSfx(SfxId::DOOR_UNLOCK);
                }
            }
        }

        if (ForceFieldSfxTimer > 0.0F)
        {
            ForceFieldSfxTimer -= scene().FrameTime();
            if (ForceFieldSfxTimer <= 0.0F)
            {
                ForceFieldSfxTimer = 0.0F;
                if (Sound::Sfx::TimedSfxMute == 0)
                {
                    _timedSfxSource.PlaySfx(
                        SfxId::GEN_OFF, false, false, 5.0F / 30.0F, true);
                }
            }
        }

        if (_scrollSfxTimer > 0.0F)
        {
            if (_scrollSfxTimer < 2.0F / 30.0F)
            {
                _soundSource.StopFreeSfx(SfxId::FAST_SCROLL_UP_LOOP);
            }
            else if (_soundSource.CountPlayingSfx(SfxId::FAST_SCROLL_UP_LOOP) == 0)
            {
                _soundSource.PlayFreeSfx(SfxId::FAST_SCROLL_UP_LOOP);
            }
            _scrollSfxTimer -= scene().FrameTime();
        }
    }

    MusicId PlayerEntity::CheckMusicUpdate(
        MusicId currentMusicId, std::int32_t param1, std::int32_t param2)
    {
        const MusicId newMusicId = static_cast<MusicId>(param1);
        if (param2 == 0)
        {
            return newMusicId;
        }

        const std::int32_t idValue = param2 & 0x7F;
        const bool negation = ((param2 & 0x80) >> 7) != 0;

        if (idValue > 70)
        {
            const bool hasArtifact
                = RequireReference(GameState::StorySave).CheckFoundArtifact(idValue - 71, 0)
                    ^ negation;
            if (hasArtifact)
            {
                return newMusicId;
            }
        }
        else if (((param2 & 0x100) >> 8) != 0)
        {
            std::shared_ptr<MphRead::StorySave> storySave = GameState::StorySave;
            const std::int32_t roomId = RequireReference(_scene).RoomId();
            const bool hasRoomState
                = (RequireReference(storySave).GetRoomState(roomId, idValue) != 0) ^ negation;
            if (hasRoomState)
            {
                return newMusicId;
            }
        }
        else
        {
            const std::int32_t byteIndex = idValue >> 3;
            const std::int32_t bitmask
                = static_cast<std::uint8_t>(1 << (idValue & 7));
            StorySave& storySave = RequireReference(GameState::StorySave);
            const std::int32_t areaId = RequireReference(_scene).AreaId();
            const bool hasEncounterState
                = ((ManagedAt(RequireReference(ManagedAt(
                    RequireReference(storySave.EnemyEncounters), areaId)), byteIndex)
                    & bitmask) == 0) ^ negation;
            if (hasEncounterState)
            {
                return newMusicId;
            }
        }
        return currentMusicId;
    }
}
