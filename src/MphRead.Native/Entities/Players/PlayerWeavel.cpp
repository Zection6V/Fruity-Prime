#include "PlayerEntity.hpp"
#include "HalfturretEntity.hpp"
#include "../../Scene.hpp"
#include "../../Mods/Gameplay/NativeGameplayClock.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Mods/Network/NetLog.hpp"

#include <string>

#include <limits>

namespace MphRead::Entities
{
    using OpenTK::Mathematics::Vector3;

    void PlayerEntity::AdvanceNativeWeaponTimers()
    {
        const auto frame = NativeRuntime::RequireReference(_scene).FrameCount();
        if (!Mods::Gameplay::NativeGameplayClock::IsNativeTick(frame) || _nativeWeaponTimerFrame == frame) return;
        _nativeWeaponTimerFrame = frame;
        if (_nativeTimeSinceShot < std::numeric_limits<std::uint8_t>::max()) ++_nativeTimeSinceShot;
        if (_hunter == Hunter::Weavel && _altAttackCooldown > 0) --_altAttackCooldown;
    }

    void PlayerEntity::FinalizeWeavelForm(bool desiredAlt)
    {
        if (!desiredAlt && TypeExtensions::TestFlag(_flags2, PlayerFlags2::AltAttack))
        {
            EndAltAttack();
        }
        const bool transitioning = IsMorphing() || IsUnmorphing();
        if (IsAltForm() == desiredAlt && !transitioning) return;
        if (IsAltForm() != desiredAlt) UpdateForm(desiredAlt);
        _flags1 &= ~(PlayerFlags1::Morphing | PlayerFlags1::Unmorphing);
        if (desiredAlt)
        {
            SwitchCamera(_values.AltFormStrafe != 0 ? CameraType::Third2 : CameraType::Third1,
                Vector3(_field70, 0.0F, _field74));
        }
        else
        {
            SwitchCamera(CameraType::First, _facingVector);
            SetBipedAnimation(PlayerAnimation::Idle, AnimFlags::None);
        }
        _weavelLungeInput.Reset();
        _weavelNativeAttackPress = false;
    }

    void PlayerEntity::ModForceWeavelState(bool desiredAlt, bool desiredTurretActive,
        std::optional<std::int32_t> desiredTurretHealth)
    {
        if (_hunter != Hunter::Weavel) return;
        const bool active = TypeExtensions::TestFlag(_flags2, PlayerFlags2::Halfturret)
            && _halfturret && _halfturret->Health() > 0;
        if (!desiredAlt)
        {
            _weavelLungeInput.Reset();
            _weavelNativeAttackPress = false;
            _flags2 &= ~PlayerFlags2::Halfturret;
            if (active) GainHealth(_halfturret->Health());
            if (_halfturret) _halfturret->Die();
            _weavelAltLife = false;
        }
        else if (desiredTurretActive && !_weavelAltLife)
        {
            EnterAltForm(); // local unsplit life: create and split exactly once
        }
        else if (!desiredTurretActive)
        {
            if (_halfturret) _halfturret->Die();
            _weavelAltLife = true; // a dead turret remains dead throughout this Alt life
        }
        if (desiredAlt && desiredTurretActive && desiredTurretHealth
            && TypeExtensions::TestFlag(_flags2, PlayerFlags2::Halfturret))
        {
            _halfturret->SetHealth(*desiredTurretHealth);
        }
        FinalizeWeavelForm(desiredAlt);
    }

    void PlayerEntity::ModApplyWeavelState(bool desiredAlt, bool turretActive, std::int32_t turretHealth,
        Vector3 turretPosition, bool turretGrounded)
    {
        if (_hunter != Hunter::Weavel) return;
        // A replica going to Alt plays the morph like every other hunter's
        // puppet does (ProcessPlayer applies the form when the animation
        // ends) instead of snapping into it, which drew no animation at all
        // for anyone watching. _weavelAltLife is set first so EnterAltForm
        // spawns no turret: the turret below is the authority's.
        const auto frame = static_cast<std::uint64_t>(
            ::MphRead::NativeRuntime::RequireReference(_scene).FrameCount());
        bool morphing = false;
        if (desiredAlt && !IsAltForm() && !IsUnmorphing() && _health > 0)
        {
            if (!IsMorphing())
            {
                _weavelAltLife = true;
                EnterAltForm();
                _weavelReplicaMorphFrame = frame;
                Mods::Network::NetLog::Event("slot " + std::to_string(SlotIndex()) + " replica Weavel morph started");
            }
            // Snap only if the animation never finishes.
            morphing = frame - _weavelReplicaMorphFrame < 90;
        }
        else if (!desiredAlt && IsAltForm() && !IsMorphing() && _health > 0)
        {
            // The way back, the same: ExitAltForm plays the unmorph. The
            // turret flag goes first so it merges no turret health -- the
            // snapshot's health already holds the authority's merge.
            _flags2 &= ~PlayerFlags2::Halfturret;
            ExitAltForm();
            _weavelReplicaMorphFrame = frame;
            morphing = true;
            Mods::Network::NetLog::Event("slot " + std::to_string(SlotIndex()) + " replica Weavel unmorph started");
        }
        else if (!desiredAlt && IsUnmorphing())
        {
            morphing = frame - _weavelReplicaMorphFrame < 90;
        }
        if (!morphing)
        {
            if (_weavelReplicaMorphFrame != 0)
            {
                Mods::Network::NetLog::Event("slot " + std::to_string(SlotIndex()) + " replica Weavel "
                    + (IsMorphing() || IsUnmorphing() ? "transition stalled, snapped" : "transition finished")
                    + " after " + std::to_string(frame - _weavelReplicaMorphFrame) + " frames");
                _weavelReplicaMorphFrame = 0;
            }
            FinalizeWeavelForm(desiredAlt);
        }
        _weavelAltLife = desiredAlt;
        if (!desiredAlt)
        {
            _weavelLungeInput.Reset();
            _weavelNativeAttackPress = false;
        }
        const bool wanted = desiredAlt && turretActive && turretHealth > 0;
        const bool active = TypeExtensions::TestFlag(_flags2, PlayerFlags2::Halfturret)
            && _halfturret && _halfturret->Health() > 0;
        Scene& scene = ::MphRead::NativeRuntime::RequireReference(_scene);
        if (wanted)
        {
            if (!active)
            {
                // Snapshot health already includes the authority's split. Never AddEntity,
                // whose Initialize would subtract half of that health a second time.
                scene.RemoveEntity(_halfturret);
                _halfturret->InitializeFromNetworkState(turretHealth, turretPosition, turretGrounded);
                scene.InsertEntity(_halfturret);
                scene.InitEntity(_halfturret);
                _flags2 |= PlayerFlags2::Halfturret;
            }
            else
            {
                _halfturret->ApplyNetworkState(turretHealth, turretPosition, turretGrounded);
            }
        }
        else if (_halfturret)
        {
            _halfturret->Die(); // no merge of authority HP during replica reconciliation
        }
    }

}
