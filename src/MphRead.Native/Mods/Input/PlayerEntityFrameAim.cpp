#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../Features.hpp"
#include "../Render/AnimationClock.hpp"
#include "../SpectatorMode.hpp"
#include "MouseAimTurn.hpp"
#include "PointerDevice.hpp"
#include "PointerInput.hpp"

#include <cmath>

namespace MphRead::Entities
{
    namespace
    {
        bool From(const Mods::Input::FrameAimState& state,
            const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& snapshot) noexcept
        {
            return state.BaselineX == snapshot.X && state.BaselineY == snapshot.Y
                && state.BaselineEpoch == snapshot.PositionEpoch;
        }

        double CurrentStep() noexcept
        {
            return Mods::Render::AnimationClock::Simulation().TickTime();
        }
    }

    bool PlayerEntity::ModFrameAimAllowed() const
    {
        // Where the view is the aim and the aim is the mouse's, by the game's
        // exact rules, and nothing this frame would block the next step's.
        return Mods::Input::FrameAim::Enabled() && !Mods::SpectatorMode::IsSpectating() && Health() > 0
            && _aimFrame.Owner == Mods::Input::AimOwner::Local
            && _aimFrame.Source == Mods::Input::AimSource::Mouse && _aimFrame.Exact && !_aimFrame.Native
            && Features::FixedCrosshair() && _controls.MouseAim()
            && _cameraType == CameraType::First && !IsAltForm() && _frozenTimer == 0
            && !TestFlag(_flags1, PlayerFlags1::NoAimInput)
            && _camSwitchTimer >= Values().CamSwitchTime * 2
            && Formats::CameraSequence::Current() == nullptr
            && !Mods::Input::PointerDevice::Active() && _input.MouseState.has_value();
    }

    void PlayerEntity::ModApplyFrameAim(const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& live)
    {
        Mods::Input::FrameAimState& state = _modFrameAim;
        if (state.Active && state.Step != CurrentStep()) state = {};
        if (!ModFrameAimAllowed()) return;
        const auto& baseline = *_input.MouseState;
        if (state.Active && !From(state, baseline)) state = {};
        const auto [rawX, rawY] = live.DeltaFrom(baseline);
        const auto [x, y] = Mods::Input::PointerInput::Filter(rawX, rawY);
        if (x == state.AppliedX && y == state.AppliedY) return;
        if (!state.Active)
        {
            state.Active = true;
            state.Step = CurrentStep();
            state.BaselineX = baseline.X;
            state.BaselineY = baseline.Y;
            state.BaselineEpoch = baseline.PositionEpoch;
            state.CameraFacing = _facingVector;
        }
        // ApplyMouseAim's turn, for the motion this frame added.
        const Mods::Input::MouseTurn turn = Mods::Input::MouseAimTurn(x - state.AppliedX, y - state.AppliedY);
        UpdateAimY(turn.Pitch);
        UpdateAimX(turn.Yaw);
        state.AppliedX = x;
        state.AppliedY = y;
        // What ProcessMovement and UpdateAimVecs build from an aim, so the gun,
        // the muzzle and their effects are this aim's and not the last step's.
        const float horizontal = std::sqrt(_facingVector.X * _facingVector.X + _facingVector.Z * _facingVector.Z);
        if (horizontal > 0)
        {
            _gunVec2 = ::OpenTK::Mathematics::Vector3(_facingVector.Z / horizontal, 0.0F, -_facingVector.X / horizontal);
            _upVector = ::OpenTK::Mathematics::Vector3::Cross(_facingVector, _gunVec2).Normalized();
        }
        UpdateAimVecs();
        if (IsMainPlayer())
        {
            if (_muzzleEffect) _muzzleEffect->Transform(_gunVec2, _gunVec1, _muzzlePos);
            if (_chargeEffect) _chargeEffect->Transform(_gunVec2, _gunVec1, _muzzlePos);
        }
    }

    std::pair<float, float> PlayerEntity::ModStepMouseAim(float deltaX, float deltaY)
    {
        Mods::Input::FrameAimState& state = _modFrameAim;
        std::pair<float, float> share{deltaX, deltaY};
        // The frames measured from the same snapshot this step's delta is.
        if (state.Active && _input.PrevMouseState.has_value() && From(state, *_input.PrevMouseState))
        {
            share = {deltaX - state.AppliedX, deltaY - state.AppliedY};
        }
        state = {};
        return share;
    }

    std::optional<::OpenTK::Mathematics::Vector3> PlayerEntity::ModFrameFacingShift() const
    {
        const Mods::Input::FrameAimState& state = _modFrameAim;
        if (!state.Active || state.Step != CurrentStep()) return std::nullopt;
        return _facingVector - state.CameraFacing;
    }
}
