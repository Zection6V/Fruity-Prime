#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../Features.hpp"
#include "../../Formats/Types.hpp"
#include "../../HUD/HudInfo.hpp"
#include "../../Scene.hpp"
#include "../Render/AnimationClock.hpp"
#include "../Render/FrameTiming.hpp"
#include "../SpectatorMode.hpp"
#include "AimNumericPolicy.hpp"
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

        bool Same(::OpenTK::Mathematics::Vector3 left, ::OpenTK::Mathematics::Vector3 right) noexcept
        {
            return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
        }
    }

    bool PlayerEntity::ModFrameAimAllowed(bool fixedCrosshair) const
    {
        // Where the aim is the mouse's, by the game's exact rules, with the
        // crosshair asked for, and nothing this frame would block the next step's.
        return Mods::Input::FrameAim::Enabled() && !Mods::SpectatorMode::IsSpectating() && Health() > 0
            && _aimFrame.Owner == Mods::Input::AimOwner::Local
            && _aimFrame.Source == Mods::Input::AimSource::Mouse && _aimFrame.Exact && !_aimFrame.Native
            && Features::FixedCrosshair() == fixedCrosshair && _controls.MouseAim()
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
        if (!ModFrameAimAllowed(true)) return;
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
        ModRebuildAimFromFacing();
    }

    void PlayerEntity::ModRebuildAimFromFacing()
    {
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

    bool PlayerEntity::ModPresentFrameAim(const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& live)
    {
        using ::OpenTK::Mathematics::Multiply;
        using ::OpenTK::Mathematics::Vector3;
        ModRestoreFrameAim();
        if (!ModFrameAimAllowed(false)) return false;
        ModSaveFrameView();
        Mods::Input::FrameAimView& view = _modFrameView;
        // The aim the next step turns to, by its own UpdateAimY/UpdateAimX,
        // for the motion since the snapshot it will consume: the crosshair
        // is exactly where the mouse has put it.
        const auto [rawX, rawY] = live.DeltaFrom(*_input.MouseState);
        const auto [x, y] = Mods::Input::PointerInput::Filter(rawX, rawY);
        const Mods::Input::MouseTurn turn = Mods::Input::MouseAimTurn(x, y);
        UpdateAimY(turn.Pitch);
        UpdateAimX(turn.Yaw);
        // The view the next step makes of that aim is where this picture's
        // view is going: UpdateAimFacing's two eases of a tenth, each inside
        // the 15 degree envelope. It is reached by the time the step runs,
        // along the ease's own curve, from the last step's view held inside
        // the envelope of the aim on screen -- which the step's first turn
        // does at once. Zoomed, the step puts the view on the aim.
        const Vector3 next = _facingVector;
        Vector3 facing = next;
        if (!_equipInfo->Zoomed)
        {
            facing = view.Facing;
            const float dot = Vector3::Dot(_gunVec1, facing);
            if (dot < Mods::Input::AimEnvelopeCos)
            {
                const Vector3 side = (facing - Multiply(_gunVec1, dot)).Normalized();
                facing = Multiply(_gunVec1, Mods::Input::AimEnvelopeCos) + Multiply(side, Mods::Input::AimEnvelopeSin);
            }
            const double kept = 1.0 - Mods::Input::AimFollow;
            const double phase = Mods::Render::FrameTiming::StepPhase();
            const float eased = static_cast<float>((1.0 - std::pow(kept, 2.0 * phase)) / (1.0 - kept * kept));
            facing = (facing + Multiply(next - facing, eased)).Normalized();
        }
        _facingVector = facing;
        ModRebuildAimFromFacing();
        view.ShownGun = _gunVec1;
        view.ShownFacing = _facingVector;
        return true;
    }

    void PlayerEntity::ModSaveFrameView()
    {
        Mods::Input::FrameAimView& view = _modFrameView;
        view = {};
        view.Active = true;
        view.Gun = _gunVec1;
        view.Facing = _facingVector;
        view.AimPosition = _aimPosition;
        view.GunSide = _gunVec2;
        view.Up = _upVector;
        view.GunDrawPos = _gunDrawPos;
        view.AimVec = _aimVec;
        view.Muzzle = _muzzlePos;
        view.AimY = _aimY;
        view.Trace = _aimTrace.Calls;
    }

    void PlayerEntity::ModNoteCameraSource()
    {
        _modCameraSource = {true, CurrentStep(), static_cast<::OpenTK::Mathematics::Vector3>(Position), _facingVector};
    }

    bool PlayerEntity::ModPresentWatchedView(::OpenTK::Mathematics::Vector3 playoutShift)
    {
        using ::OpenTK::Mathematics::Matrix4;
        using ::OpenTK::Mathematics::Vector3;
        ModRestoreFrameAim();
        const Mods::Input::CameraSource& source = _modCameraSource;
        if (!source.Valid || source.Step != CurrentStep() || Health() <= 0 || _cameraInfo == nullptr) return false;
        ::MphRead::Entities::CameraInfo& camera = *_cameraInfo;
        // The step built this camera, then the snapshot moved the player and
        // turned them (ApplyState runs after the entities): the eye goes where
        // the body is, and on along the playout to this picture's instant.
        const Vector3 eye = playoutShift + (static_cast<Vector3>(Position) - source.Position);
        // Only a first-person sight line is the facing; the ball's camera trails.
        const Vector3 turn = _cameraType == CameraType::First && !IsAltForm()
            ? _facingVector - source.Facing : Vector3{};
        if (eye.LengthSquared() == 0 && turn.LengthSquared() == 0) return false;
        ModSaveFrameView();
        Mods::Input::FrameAimView& view = _modFrameView;
        view.Camera = true;
        view.CameraPosition = camera.Position;
        view.CameraTarget = camera.Target;
        view.CameraFacing = camera.Facing;
        view.CameraTrueUp = camera.TrueUp;
        view.CameraView = camera.ViewMatrix;
        const Matrix4 shown = Mods::Input::FrameAim::Moved(camera, eye, turn);
        // The gun, the muzzle and their effects ride with the view: whatever
        // the step's camera had where, the picture's has in the same place.
        const Matrix4 carry = camera.ViewMatrix * ::OpenTK::Mathematics::Inverted(shown);
        const auto point = [&carry](Vector3 value) { return Matrix::Vec3MultMtx4(value, carry); };
        const auto direction = [&carry](Vector3 value) { return Matrix::Vec3MultMtx3(value, carry); };
        _gunDrawPos = point(_gunDrawPos);
        _muzzlePos = point(_muzzlePos);
        _aimPosition = point(_aimPosition);
        _aimVec = direction(_aimVec);
        _upVector = direction(_upVector);
        _gunVec1 = direction(_gunVec1);
        _gunVec2 = direction(_gunVec2);
        camera.Position = camera.Position + eye;
        camera.Target = camera.Target + eye + turn;
        camera.Facing = (camera.Target - camera.Position).Normalized();
        camera.TrueUp = direction(camera.TrueUp);
        camera.ViewMatrix = shown;
        if (IsMainPlayer())
        {
            if (_muzzleEffect) _muzzleEffect->Transform(_gunVec2, _gunVec1, _muzzlePos);
            if (_chargeEffect) _chargeEffect->Transform(_gunVec2, _gunVec1, _muzzlePos);
        }
        view.ShownGun = _gunVec1;
        view.ShownFacing = _facingVector;
        return true;
    }

    void PlayerEntity::ModPresentFrameReticle()
    {
        Mods::Input::FrameAimView& view = _modFrameView;
        if (!view.Active || Features::FixedWeapon() || _targetCircleInst == nullptr || !_targetCircleInst->Enabled)
        {
            return;
        }
        // UpdateReticle's projection, through this picture's camera.
        view.Reticle = true;
        view.ReticleX = _targetCircleInst->PositionX;
        view.ReticleY = _targetCircleInst->PositionY;
        ::OpenTK::Mathematics::Vector2 pos{};
        (void)Matrix::ProjectPosition(_aimPosition, _scene->ViewMatrix(), _scene->PerspectiveMatrix(), pos);
        _targetCircleInst->PositionX = NativeRuntime::RoundToEven(pos.X * 100000.0F) / 100000.0F;
        _targetCircleInst->PositionY = NativeRuntime::RoundToEven(pos.Y * 100000.0F) / 100000.0F;
    }

    void PlayerEntity::ModRestoreFrameAim()
    {
        Mods::Input::FrameAimView& view = _modFrameView;
        if (!view.Active) return;
        view.Active = false;
        // A respawn or a room loaded while the picture was made has set an
        // aim of its own, which is the one to keep.
        if (!Same(_gunVec1, view.ShownGun) || !Same(_facingVector, view.ShownFacing)) return;
        _gunVec1 = view.Gun;
        _facingVector = view.Facing;
        _aimPosition = view.AimPosition;
        _gunVec2 = view.GunSide;
        _upVector = view.Up;
        _gunDrawPos = view.GunDrawPos;
        _aimVec = view.AimVec;
        _muzzlePos = view.Muzzle;
        _aimY = view.AimY;
        _aimTrace.Calls = view.Trace;
        if (view.Camera && _cameraInfo != nullptr)
        {
            _cameraInfo->Position = view.CameraPosition;
            _cameraInfo->Target = view.CameraTarget;
            _cameraInfo->Facing = view.CameraFacing;
            _cameraInfo->TrueUp = view.CameraTrueUp;
            _cameraInfo->ViewMatrix = view.CameraView;
        }
        if (IsMainPlayer())
        {
            if (_muzzleEffect) _muzzleEffect->Transform(_gunVec2, _gunVec1, _muzzlePos);
            if (_chargeEffect) _chargeEffect->Transform(_gunVec2, _gunVec1, _muzzlePos);
        }
        if (view.Reticle && _targetCircleInst != nullptr)
        {
            _targetCircleInst->PositionX = view.ReticleX;
            _targetCircleInst->PositionY = view.ReticleY;
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
        if (_modFrameView.Active)
        {
            // A watched view is already the camera's own matrix.
            if (_modFrameView.Camera || !Same(_facingVector, _modFrameView.ShownFacing)) return std::nullopt;
            return _facingVector - _modFrameView.Facing;
        }
        const Mods::Input::FrameAimState& state = _modFrameAim;
        if (!state.Active || state.Step != CurrentStep()) return std::nullopt;
        return _facingVector - state.CameraFacing;
    }
}
