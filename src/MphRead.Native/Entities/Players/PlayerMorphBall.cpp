// The Morph Ball's touch roll and its two boosts (EU1.1 02021C28): which
// branch runs, and what each does to the ball. ProcessAlt calls in; the
// arithmetic itself is MorphBallTouchRules's and the touch state is the input
// adapter's.
#include "PlayerEntity.hpp"
#include "MorphBallBoostStateMachine.hpp"
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

    // 0202360C-02023A24. Which boost fires is MorphBallBoostStateMachine's
    // call; this applies it to the ball.
    void PlayerEntity::ProcessBoost(Vector3& speedDelta)
    {
        namespace Boost = MorphBallBoostStateMachine;
        const Mods::Input::NativeTouchState& touch = _input.Touch();
        Boost::State state{TestFlag(_flags1, PlayerFlags1::Boosting),
            TestFlag(_flags1, PlayerFlags1::CanTouchBoost), _boostCharge};
        const Boost::Result result = Boost::Advance(state,
            {touch.Down, touch.Continued, touch.Delta4X, touch.Delta4Y, _controls.Boost().IsDown()},
            {static_cast<std::uint16_t>(_values.BoostChargeMin * 2), static_cast<std::uint16_t>(_values.BoostChargeMax * 2),
                Features::FullBoostCharge()});
        if (state.Boosting) _flags1 |= PlayerFlags1::Boosting;
        else _flags1 &= ~PlayerFlags1::Boosting;
        if (state.CanTouchBoost) _flags1 |= PlayerFlags1::CanTouchBoost;
        else _flags1 &= ~PlayerFlags1::CanTouchBoost;
        _boostCharge = state.Charge;

        const Boost::BoostValues values{Fixed::ToFloat(_values.BoostSpeedMin), Fixed::ToFloat(_values.BoostSpeedMax),
            Fixed::ToFloat(_values.BoostSpeedCap), static_cast<std::uint16_t>(_values.AltAttackDamage)};
        switch (result.Boost)
        {
        case Boost::Fired::TouchBoost:
            ApplyTouchBoost(touch.Delta4X, touch.Delta4Y, Boost::TouchBoostStrength(values), speedDelta);
            break;
        case Boost::Fired::ShoulderBoost:
            ApplyShoulderBoost(Boost::ShoulderBoostStrength(values, result.ChargeSpent,
                static_cast<std::uint16_t>(_values.BoostChargeMax * 2)), speedDelta);
            break;
        case Boost::Fired::None:
            break;
        }
    }

    // 0202366C-02023840: along the swipe, against the current camera basis.
    void PlayerEntity::ApplyTouchBoost(std::int32_t dx, std::int32_t dy,
        const MorphBallBoostStateMachine::Strength& strength, Vector3& speedDelta)
    {
        const auto& camera = RequireReference(_cameraInfo);
        const auto impulse = MorphBallTouchRules::TouchBoostImpulse(dx, dy, strength.Speed,
            camera.Field48, camera.Field4C, camera.Field50, camera.Field54);
        speedDelta = AddZ(AddX(speedDelta, impulse.X), impulse.Z);
        ApplyBoostCommon(strength);
    }

    // 02023844-02023A20: along the way the player faces.
    void PlayerEntity::ApplyShoulderBoost(const MorphBallBoostStateMachine::Strength& strength, Vector3& speedDelta)
    {
        speedDelta = AddZ(AddX(speedDelta, _field70 * strength.Speed), _field74 * strength.Speed);
        ApplyBoostCommon(strength);
    }

    void PlayerEntity::ApplyBoostCommon(const MorphBallBoostStateMachine::Strength& strength)
    {
        if (_hSpeedCap < strength.Cap) _hSpeedCap = strength.Cap;
        _altAttackCooldown = static_cast<std::uint16_t>(_values.AltAttackCooldown * 2);
        _boostDamage = strength.Damage;
        PlayBoostSideEffects();
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
