// The Morph Ball's touch roll and its two boosts (EU1.1 02021C28): which
// branch runs, and what each does to the ball. ProcessAlt calls in; the
// arithmetic itself is MorphBallTouchRules's and the touch state is the input
// adapter's.
#include "PlayerEntity.hpp"
#include "MorphBallTouchRules.hpp"
#include "../../Features.hpp"
#include "../../Scene.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Metadata/Player.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Mods/Input/GamepadHaptics.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestFlag;
using ::OpenTK::Mathematics::AddX;
using ::OpenTK::Mathematics::AddZ;
using ::OpenTK::Mathematics::Vector3;

namespace MphRead::Entities
{
    // 02021DD0-02021F0C. The control mode's 0x10 bit (Touch 0x76, Dual 0x7C)
    // has no byte here; its compatibility mapping is the touch adapter, which
    // is the only thing that raises Down.
    void PlayerEntity::ApplyTouchRoll(Vector3& speedDelta)
    {
        const Mods::Input::NativeTouchState& touch = _input.Touch();
        if (!touch.Down || IsMorphing() || TestFlag(_flags1, PlayerFlags1::NoAimInput))
        {
            return;
        }
        float scale = MorphBallTouchRules::TouchRollPerDsPixel;
        if (_jumpPadControlLockMin > 0)
        {
            scale *= Fixed::ToFloat(_values.JumpPadSlideFactor);
        }
        const auto roll = MorphBallTouchRules::TouchRoll(touch.Delta4X, touch.Delta4Y, scale,
            _altRollFbX, _altRollFbZ, _altRollLrX, _altRollLrZ);
        speedDelta.X += roll.X;
        speedDelta.Z += roll.Z;
    }

    // 0202360C-02023A24: the touch boost is decided before R, and an armed
    // continued contact that does not clear the threshold skips R this frame.
    void PlayerEntity::ProcessBoost(Vector3& speedDelta)
    {
        const Mods::Input::NativeTouchState& touch = _input.Touch();
        if (!touch.Down)
        {
            _flags1 |= PlayerFlags1::CanTouchBoost;
        }
        switch (MorphBallTouchRules::Arbitrate(TestFlag(_flags1, PlayerFlags1::Boosting),
            TestFlag(_flags1, PlayerFlags1::CanTouchBoost), touch.Continued, touch.Delta4X, touch.Delta4Y))
        {
        case MorphBallTouchRules::BoostBranch::TouchBoost:
            FireNativeTouchBoost(touch.Delta4X, touch.Delta4Y, speedDelta);
            break;
        case MorphBallTouchRules::BoostBranch::Shoulder:
            ProcessShoulderBoost(speedDelta);
            break;
        case MorphBallTouchRules::BoostBranch::SkipShoulder:
            break;
        }
    }

    // EU1.1 0202366C-02023840. Full strength whatever R holds, and R's charge
    // is left exactly where it was: this branch never reaches 02023A1C.
    void PlayerEntity::FireNativeTouchBoost(std::int32_t dx, std::int32_t dy, Vector3& speedDelta)
    {
        const auto& camera = RequireReference(_cameraInfo);
        const auto impulse = MorphBallTouchRules::TouchBoostImpulse(dx, dy,
            Fixed::ToFloat(_values.BoostSpeedMax), camera.Field48, camera.Field4C, camera.Field50, camera.Field54);
        const float boostHCap = Fixed::ToFloat(_values.BoostSpeedCap);
        if (_hSpeedCap < boostHCap) _hSpeedCap = boostHCap;
        PlayBoostSideEffects();
        _altAttackCooldown = static_cast<std::uint16_t>(_values.AltAttackCooldown * 2);
        _flags1 |= PlayerFlags1::Boosting;
        _flags1 &= ~PlayerFlags1::CanTouchBoost;
        _boostDamage = static_cast<std::uint16_t>(_values.AltAttackDamage);
        speedDelta = AddZ(AddX(speedDelta, impulse.X), impulse.Z);
    }

    // EU1.1 02023844-02023A20, unchanged from MphRead.
    void PlayerEntity::ProcessShoulderBoost(Vector3& speedDelta)
    {
        if (_controls.Boost().IsDown())
        {
            if (_boostCharge < _values.BoostChargeMax * 2)
            {
                ++_boostCharge;
            }
            return;
        }
        if (_boostCharge > _values.BoostChargeMin * 2)
        {
            if (Features::FullBoostCharge())
            {
                _boostCharge = static_cast<std::uint16_t>(_values.BoostChargeMax * 2);
            }
            const float boostHCap = Fixed::ToFloat(_values.BoostSpeedCap) * _boostCharge
                / static_cast<float>(_values.BoostChargeMax * 2);
            if (_hSpeedCap < boostHCap) _hSpeedCap = boostHCap;
            const float factor = Fixed::ToFloat(_values.BoostSpeedMin)
                + _boostCharge * (Fixed::ToFloat(_values.BoostSpeedMax)
                    - Fixed::ToFloat(_values.BoostSpeedMin))
                / static_cast<float>(_values.BoostChargeMax * 2);
            speedDelta = AddZ(AddX(speedDelta, _field70 * factor), _field74 * factor);
            _altAttackCooldown = static_cast<std::uint16_t>(_values.AltAttackCooldown * 2);
            _flags1 |= PlayerFlags1::Boosting;
            _boostDamage = static_cast<std::uint16_t>(
                _values.AltAttackDamage * _boostCharge / (_values.BoostChargeMax * 2));
            PlayBoostSideEffects();
        }
        _boostCharge = 0;
    }

    // What both boosts do besides move the ball: the sound, the pad's kick,
    // the HUD animation and effect 136 along the player's own orientation.
    void PlayerEntity::PlayBoostSideEffects()
    {
        const auto hunterSfx = Metadata::HunterSfx();
        const auto& hunterSounds = ManagedAt(RequireReference(hunterSfx), static_cast<std::int32_t>(_hunter));
        _soundSource.PlaySfx(ManagedAt(hunterSounds, static_cast<std::int32_t>(HunterSfx::Boost)));
        ModControllerFeedback(Mods::Input::GamepadFeedback::Boost);
        if (IsMainPlayer())
        {
            RequireReference(_boostInst).SetAnimation(0, 10, 11, 0);
        }
        if (_boostEffect != nullptr)
        {
            RequireReference(_scene).UnlinkEffectEntry(_boostEffect);
            _boostEffect.reset();
        }
        _boostEffect = RequireReference(_scene).SpawnEffectGetEntry(136, _gunVec2, _facingVector, static_cast<Vector3>(Position));
        if (_boostEffect != nullptr)
        {
            _boostEffect->SetElementExtension(true);
        }
    }
}
