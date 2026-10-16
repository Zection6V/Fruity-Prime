#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../Features.hpp"
#include "../../Utility/Rng.hpp"
#include "../Render/ScenePresentation.hpp"
#include "../SpectatorMode.hpp"
#include "PointerInput.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Entities
{
    using ::MphRead::NativeRuntime::UncheckedMultiply;
    using ::OpenTK::Mathematics::Add;
    using ::OpenTK::Mathematics::ScaleVector;
    using ::OpenTK::Mathematics::Subtract;
    using ::OpenTK::Mathematics::Normalize;
    using ::OpenTK::Mathematics::Vector3;
    using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;

    bool PlayerEntity::ModIdleSwayEnabled() const
    {
        return _aimY < 60.0F && _aimY > -60.0F && !_equipInfo->Zoomed && _health > 0
            && !Features::NoIdleSway();
    }

    std::int32_t PlayerEntity::ModIdleSwayStartTime() const
    {
        const auto start = UncheckedMultiply(_values.SwayStartTime, 2);
        return Features::DelayedIdleSway() ? UncheckedMultiply(start, 4) : start;
    }

    Vector3 PlayerEntity::ModIdleSwayFacing(float phase) const
    {
        const float angle = 180.0F * phase + 180.0F;
        const float factor = (std::cos(DegreesToRadians(angle)) + 1.0F) / 2.0F;
        return Normalize(Add(_field410, ScaleVector(Subtract(_field41C, _field410), factor)));
    }

    void PlayerEntity::ModUpdateIdleSway()
    {
        const auto nextTarget = [this](Vector3 origin)
        {
            const float horizontal = static_cast<float>(
                static_cast<std::int64_t>(Rng::GetRandomInt2(_values.SwayLimit))
                - static_cast<std::int64_t>(_values.SwayLimit / 2)) / 4096.0F;
            const float vertical = static_cast<float>(
                static_cast<std::int64_t>(Rng::GetRandomInt2(_values.SwayLimit))
                - static_cast<std::int64_t>(_values.SwayLimit / 2)) / 4096.0F;
            return Add(origin, Add(ScaleVector(_gunVec2, horizontal), ScaleVector(_upVector, vertical)));
        };
        if (ModIdleSwayEnabled())
        {
            const std::int32_t swayStart = ModIdleSwayStartTime();
            if (_timeSinceInput == swayStart)
            {
                _field40C = 0.0F;
                _field410 = _facingVector;
                _field428 = _field410;
                _field41C = nextTarget(_field410);
            }
            else if (_timeSinceInput > swayStart)
            {
                _field40C += 1.0F / static_cast<float>(_values.SwayIncrement) / 2.0F;
                if (_field40C >= 1.0F)
                {
                    _field40C = 0.0F;
                    _field410 = _field41C;
                    _field41C = nextTarget(_field428);
                }
                _facingVector = ModIdleSwayFacing(_field40C);
            }
        }
    }

    bool PlayerEntity::ModIdleSwayPresentable(
        const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState* live) const
    {
        if (!Mods::Input::FrameAim::Enabled() || Mods::SpectatorMode::IsSpectating()
            || _aimFrame.Owner != Mods::Input::AimOwner::Local || !ModIdleSwayEnabled()
            || _timeSinceInput <= ModIdleSwayStartTime() || _values.SwayIncrement <= 0
            || _cameraInfo == nullptr || _cameraType != CameraType::First || IsAltForm()
            || _frozenTimer != 0 || _camSwitchTimer < Values().CamSwitchTime * 2
            || Formats::CameraSequence::Current() != nullptr
            || !_modPose.HasPrevious || _modPose.Step != Mods::Render::ScenePresentation::CurrentStep())
        {
            return false;
        }
        // Fresh mouse motion takes over immediately, before the next step
        // resets the idle timer. Its frame aim must not be replaced by sway.
        if (_modFrameAim.Active && (_modFrameAim.AppliedX != 0 || _modFrameAim.AppliedY != 0)) return false;
        if (live != nullptr && _input.MouseState)
        {
            const auto [x, y] = live->DeltaFrom(*_input.MouseState);
            const auto [filteredX, filteredY] = Mods::Input::PointerInput::Filter(x, y);
            if (filteredX != 0 || filteredY != 0) return false;
        }
        return true;
    }

    void PlayerEntity::ModPresentIdleSway()
    {
        const float phase = static_cast<float>(Mods::Render::ScenePresentation::Phase());
        if (phase <= 0) return;
        // Ask the current cosine segment at the picture's time. Its end is
        // known; choosing the following random target belongs to the step.
        const float next = std::min(1.0F, _field40C
            + phase * (1.0F / static_cast<float>(_values.SwayIncrement) / 2.0F));
        const Vector3 turn = ModIdleSwayFacing(next) - _facingVector;
        ModSaveFrameView();
        ModCarryCamera(Vector3{}, turn);
    }
}
