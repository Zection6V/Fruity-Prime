#include "AimAssistWorld.hpp"

#include "AimAssist.hpp"
#include "AimAssistDebug.hpp"
#include "AimAssistMath.hpp"
#include "AimAssistTelemetry.hpp"
#include "../AimInputSourceTracker.hpp"
#include "../GamepadAnalog.hpp"
#include "../GamepadInput.hpp"
#include "../GamepadOptions.hpp"
#include "../GamepadUiRouter.hpp"
#include "../PointerDevice.hpp"
#include "../../SpectatorMode.hpp"
#include "../../../Entities/Players/PlayerEntity.hpp"
#include "../../../Formats/CollisionDetection.hpp"
#include "../../../GameState.hpp"
#include "../../../Scene.hpp"
#include "../../../NativeRuntime/OpenTK/Mathematics.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"

#include <array>
#include <cmath>
#include <numbers>

namespace MphRead::Entities
{
    using ::MphRead::Mods::Input::AimAssist::AimAssistPointType;
    using ::MphRead::Mods::Input::AimAssist::AimAssistResult;
    using ::MphRead::Mods::Input::AimAssist::AimAssistTarget;
    using ::MphRead::Mods::Input::AimAssist::AimAssistWeaponClass;
    using ::MphRead::Mods::Input::AimAssist::AimAssistWeaponProfile;
    using ::OpenTK::Mathematics::Vector3;
    using NVector2 = ::System::Numerics::Vector2;
    namespace Input = ::MphRead::Mods::Input;

    NVector2 PlayerEntity::AssistAngles(Vector3 point) const
    {
        const Vector3 direction = point - CameraInfo()->Position;
        const float desiredYaw = std::atan2(direction.X, direction.Z);
        const float currentYaw = std::atan2(_gunVec1.X, _gunVec1.Z);
        const float yaw = std::remainder(desiredYaw - currentYaw, std::numbers::pi_v<float> * 2);
        const float pitch = std::atan2(direction.Y, std::sqrt(direction.X * direction.X + direction.Z * direction.Z))
            - std::atan2(_gunVec1.Y, std::sqrt(_gunVec1.X * _gunVec1.X + _gunVec1.Z * _gunVec1.Z));
        return {::OpenTK::Mathematics::MathHelper::RadiansToDegrees(yaw),
            ::OpenTK::Mathematics::MathHelper::RadiansToDegrees(pitch)};
    }

    bool PlayerEntity::AssistVisible(Vector3 point) const
    {
        Formats::CollisionResult result{};
        const auto& candidates = Formats::CollisionDetection::GetCandidatesForLimits(CameraInfo()->Position, point, 0,
            std::nullopt, Vector3(), true, _scene);
        return !Formats::CollisionDetection::CheckBetweenPoints(&candidates, CameraInfo()->Position, point,
            Formats::TestFlags::Beams, _scene, result);
    }

    AimAssistResult PlayerEntity::ApplyControllerAssist(float x, float y)
    {
        const Input::GamepadSnapshot& snapshot = Input::GamepadInput::FrameSnapshot();
        const std::int64_t context = Input::GamepadContexts::Revision();
        const void* room = ::MphRead::NativeRuntime::RequireReference(_scene).Room().get();
        if (_assistDeviceRevision != snapshot.Revision || _assistContextRevision != context
            || _aimSourceRevision != Input::AimInputSourceTracker::Revision() || _assistRoom != room)
        {
            _controllerAssist.Reset();
            _assistDeviceRevision = snapshot.Revision;
            _assistContextRevision = context;
        }
        _aimSourceRevision = Input::AimInputSourceTracker::Revision();
        _assistRoom = room;
        const std::int64_t now = ::MphRead::NativeRuntime::EnvironmentTickCount64();
        Input::AimInputSourceTracker::Pointer(_input.MouseDeltaX(), _input.MouseDeltaY(),
            Input::PointerDevice::Active() && Input::PointerDevice::Current().Device != Input::PointerDeviceType::Mouse, now);
        const auto aim = Input::GamepadInput::AimStick();
        Input::AimInputSourceTracker::Stick(aim.first, aim.second, now);
        const bool eligible = snapshot.State.Connected && Input::GamepadContexts::Focused() && !Input::GamepadContexts::MenuVisible()
            && Input::GamepadContexts::Current() == Input::GamepadContext::Gameplay && !Input::GamepadInput::WheelHeld()
            && Input::AimInputSourceTracker::Current() == Input::AimInputSource::Gamepad && _health > 0
            && TestFlag(_loadFlags, LoadFlags::Spawned) && !IsAltForm() && !Mods::SpectatorMode::IsSpectating();
        AimAssistWeaponClass weapon = AimAssistWeaponClass::Standard;
        switch (_currentWeapon)
        {
        case BeamType::ShockCoil: weapon = AimAssistWeaponClass::Tracking; break;
        case BeamType::Imperialist: weapon = AimAssistWeaponClass::Precision; break;
        case BeamType::Missile:
        case BeamType::Magmaul:
        case BeamType::OmegaCannon: weapon = AimAssistWeaponClass::Splash; break;
        case BeamType::Judicator:
        case BeamType::Battlehammer: weapon = AimAssistWeaponClass::Projectile; break;
        default: break;
        }
        const AimAssistWeaponProfile profile = AimAssistWeaponProfile::For(weapon, ::MphRead::NativeRuntime::RequireReference(_equipInfo).Zoomed);
        std::array<AimAssistTarget, SlotCapacity> candidates{};
        std::size_t count = 0;
        const bool observe = Mods::Input::AimAssist::AimAssistTelemetry::Enabled() && Input::GamepadContexts::Focused()
            && !Input::GamepadContexts::MenuVisible() && Input::GamepadContexts::Current() == Input::GamepadContext::Gameplay
            && _health > 0 && !Mods::SpectatorMode::IsSpectating();
        if (eligible || observe)
        {
            for (const std::shared_ptr<PlayerEntity>& entry : Players())
            {
                PlayerEntity* target = entry.get();
                if (target == nullptr || target == this || !target->ModInPlay()
                    || !TestFlag(target->_loadFlags, LoadFlags::Active)
                    || !TestFlag(target->_loadFlags, LoadFlags::Spawned) || target->CurAlpha() < .95F
                    || (GameState::Teams() && TeamIndex() == target->TeamIndex()))
                {
                    continue;
                }
                const MphRead::CollisionVolume& volume = PlayerVolumes[static_cast<std::size_t>(target->Hunter())]
                    [target->IsAltForm() ? 2 : 0];
                const Vector3 position = target->Position;
                const Vector3 center = position + volume.SpherePosition;
                const float height = Fixed::ToFloat(target->Values().MaxPickupHeight);
                const Vector3 chest = target->IsAltForm() ? center
                    : center + ::OpenTK::Mathematics::Multiply(position + Vector3(0, height - .3F, 0) - center, .65F);
                const Vector3 head = position + Vector3(0, height - .15F, 0);
                const float distance = ::OpenTK::Mathematics::Length(chest - CameraInfo()->Position);
                const NVector2 bodyError = AssistAngles(chest);
                if (!Mods::Input::AimAssist::AimAssistMath::Finite(bodyError) || !std::isfinite(distance) || distance > 60
                    || bodyError.Length() > profile.ReleaseCone)
                {
                    continue;
                }
                const bool visible = AssistVisible(chest);
                if (!visible)
                {
                    continue;
                }
                const NVector2 headError = AssistAngles(head);
                const bool headVisible = !target->IsAltForm() && profile.Head && headError.Length() < 1.5F && AssistVisible(head);
                const std::int64_t targetLife = 0;
                candidates[count++] = AimAssistTarget{target->SlotIndex(), targetLife, bodyError, headError, distance, visible,
                    headVisible, true, target->IsAltForm() ? AimAssistPointType::CenterMass : AimAssistPointType::UpperChest};
                if (count == candidates.size())
                {
                    break;
                }
            }
        }
        const Input::GamepadState& pad = snapshot.State;
        const auto movement = Input::GamepadOptions::Southpaw()
            ? Input::GamepadAnalog::ApplyRadialDeadZone(pad.RightX, pad.RightY, Input::GamepadOptions::RightInner(), Input::GamepadOptions::RightOuter())
            : Input::GamepadAnalog::ApplyRadialDeadZone(pad.LeftX, pad.LeftY, Input::GamepadOptions::LeftInner(), Input::GamepadOptions::LeftOuter());
        const float move = std::sqrt(movement.first * movement.first + movement.second * movement.second);
        const std::span<const AimAssistTarget> found(candidates.data(), count);
        AimAssistResult result = Mods::Input::AimAssist::AimAssist::Apply(_controllerAssist, found, NVector2(x, y),
            std::sqrt(aim.first * aim.first + aim.second * aim.second), move, 1.0F / 60, eligible, profile);
        AimAssistTarget chosen{};
        for (const AimAssistTarget& candidate : found)
        {
            if (candidate.Slot == result.TargetSlot)
            {
                chosen = candidate;
            }
        }
        using Mods::Input::AimAssist::AimAssistDebug;
        if (AimAssistDebug::UnassistedArm)
        {
            result.X = x;
            result.Y = y;
            result.Friction = 1;
            result.RotationStrength = 0;
            result.HeadBlend = 0;
            result.PointType = chosen.BodyPointType;
            _controllerAssist.PreviousOutput = NVector2(x, y);
        }
        AimAssistDebug::Result = result;
        AimAssistDebug::Target = chosen;
        AimAssistDebug::Raw = NVector2(x, y);
        AimAssistDebug::Velocity = _controllerAssist.AngularVelocity;
        AimAssistResult observation = result;
        if (!eligible && observe)
        {
            float nearest = profile.Cone;
            for (const AimAssistTarget& candidate : found)
            {
                if (candidate.BodyError.Length() < nearest)
                {
                    chosen = candidate;
                    nearest = candidate.BodyError.Length();
                    observation = result;
                    observation.TargetSlot = candidate.Slot;
                }
            }
        }
        Mods::Input::AimAssist::AimAssistTelemetry::Record(_currentWeapon, chosen, observation,
            std::sqrt((result.X - x) * (result.X - x) + (result.Y - y) * (result.Y - y)),
            _controllerAssist.AngularVelocity.Length());
        return result;
    }
}
