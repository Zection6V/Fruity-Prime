#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../Features.hpp"
#include "../../Formats/Collision.hpp"
#include "../../Formats/Effects.hpp"
#include "../../Formats/Types.hpp"
#include "../../HUD/HudInfo.hpp"
#include "../../Scene.hpp"
#include "../Render/AnimationClock.hpp"
#include "../Render/FrameTiming.hpp"
#include "../Render/MotionCarry.hpp"
#include "../Render/ScenePresentation.hpp"
#include "../Network/Gameplay/Replication/NetSmoothing.hpp"
#include "../Network/Session/NetSession.hpp"
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

        // EffectEntry::Transform sets the elements' own transform; the one
        // their attached particles are drawn with is copied from it by
        // Scene::ProcessEffects, once a step. Placed by a picture between
        // steps, the effect is copied here too, or its glow trails the gun.
        void Place(const std::shared_ptr<Effects::EffectEntry>& effect,
            ::OpenTK::Mathematics::Vector3 side, ::OpenTK::Mathematics::Vector3 gun,
            ::OpenTK::Mathematics::Vector3 muzzle)
        {
            if (effect == nullptr) return;
            effect->Transform(side, gun, muzzle);
            for (const auto& element : *effect->Elements)
            {
                if (element == nullptr) continue;
                element->Transform = element->EntityCollision != nullptr && !element->Expired
                    ? ::OpenTK::Mathematics::Multiply(element->OwnTransform, element->EntityCollision->Transform)
                    : element->OwnTransform;
            }
        }
    }

    void PlayerEntity::ModPlaceGunEffects()
    {
        if (!IsMainPlayer() && _aimFrame.Owner != Mods::Input::AimOwner::Local) return;
        Place(_muzzleEffect, _gunVec2, _gunVec1, _muzzlePos);
        Place(_chargeEffect, _gunVec2, _gunVec1, _muzzlePos);
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
        ModPlaceGunEffects();
    }

    bool PlayerEntity::ModPresentFrameAim(const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& live)
    {
        using ::OpenTK::Mathematics::Multiply;
        using ::OpenTK::Mathematics::Vector3;
        if (!ModFrameAimAllowed(false)) return false;
        ModSaveFrameView();
        Mods::Input::FrameAimView& view = _modFrameView;
        view.Aimed = true;
        // The aim the next step turns to, by its own UpdateAimY/UpdateAimX,
        // for the motion since the snapshot it will consume: the crosshair
        // is exactly where the mouse has put it.
        const auto [rawX, rawY] = live.DeltaFrom(*_input.MouseState);
        const auto [x, y] = Mods::Input::PointerInput::Filter(rawX, rawY);
        const Mods::Input::MouseTurn turn = Mods::Input::MouseAimTurn(x, y);
        UpdateAimY(turn.Pitch);
        UpdateAimX(turn.Yaw);
        // The view the next step makes of that aim is the step's own answer,
        // and every picture before it reads the same ease in continuous time.
        // A step keeps k = 0.81 of the view's offset from the aim (each of
        // UpdateAimY and UpdateAimX eases a tenth of the way), and lags an
        // aim that moved by d by b d: b = 0.81 for a pitch, which both eases
        // see, and 0.9 for a yaw, which only UpdateAimX's does. Over a
        // fraction p of a step the ease keeps k^p of the last offset and lags
        // c(p) d, c(p) = b (1 - k^p) / ((1 - k) p): a steady turn keeps a
        // steady offset and so moves the view evenly, and at p = 1 this is
        // the step's view for that aim -- written as that view plus what
        // vanishes there, so the two meet exactly. Zoomed, the step puts the
        // view on the aim.
        const Vector3 next = _facingVector;
        Vector3 facing = next;
        if (!_equipInfo->Zoomed)
        {
            const double follow = 1.0 - Mods::Input::AimFollow;
            const double kept = follow * follow;
            const double phase = Mods::Render::ScenePresentation::Phase();
            const double offsetKept = std::pow(kept, phase);
            // c(p) / b, which tends to -ln k / (1 - k) as p does to 0.
            const double lagShape = phase > 1e-6 ? (1.0 - offsetKept) / ((1.0 - kept) * phase)
                : -std::log(kept) / (1.0 - kept);
            const Vector3 moved = _gunVec1 - view.Gun;
            const float horizontal = std::sqrt(_gunVec1.X * _gunVec1.X + _gunVec1.Z * _gunVec1.Z);
            const Vector3 side = horizontal > 0
                ? Vector3(_gunVec1.Z / horizontal, 0.0F, -_gunVec1.X / horizontal) : Vector3{};
            const Vector3 yawed = Multiply(side, Vector3::Dot(moved, side));
            const Vector3 pitched = moved - yawed;
            facing = (next + Multiply(view.Facing - view.Gun, static_cast<float>(offsetKept - kept))
                - Multiply(yawed, static_cast<float>(follow * (lagShape - 1.0)))
                - Multiply(pitched, static_cast<float>(kept * (lagShape - 1.0)))).Normalized();
            // Never further behind the aim than the steps either side of this
            // picture leave it: inside the envelope that is the ease above,
            // and against it the step clamps and eases back every time, which
            // a picture must not draw as a 60 Hz wobble.
            const float cosLimit = std::min(Vector3::Dot(view.Facing, view.Gun), Vector3::Dot(next, _gunVec1));
            const float dot = Vector3::Dot(_gunVec1, facing);
            if (dot < cosLimit)
            {
                const Vector3 away = (facing - Multiply(_gunVec1, dot)).Normalized();
                facing = Multiply(_gunVec1, cosLimit) + Multiply(away, std::sqrt(std::max(0.0F, 1.0F - cosLimit * cosLimit)));
            }
        }
        _facingVector = facing;
        ModRebuildAimFromFacing();
        view.ShownGun = _gunVec1;
        view.ShownFacing = _facingVector;
        return true;
    }

    bool PlayerEntity::ModPresentOwnView(const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState* live)
    {
        using ::OpenTK::Mathematics::Vector3;
        ModRestoreFrameAim();
        const bool idleSway = ModIdleSwayPresentable(live);
        bool shown = false;
        // Dead too: the death camera flies at the step's rate, and is carried
        // on between steps like any other (a respawn is a jump, not carried).
        if (Mods::Input::FrameAim::Enabled() && !Mods::SpectatorMode::IsSpectating()
            && _aimFrame.Owner == Mods::Input::AimOwner::Local && Formats::CameraSequence::Current() == nullptr
            && _cameraInfo != nullptr)
        {
            // The step moved the eye with the body, and the picture is a
            // fraction of a step on from it: carried on along that step's own
            // motion, the eye moves every frame where it otherwise jumps 60
            // times a second under a view that turns every frame. Nothing has
            // happened yet that could say otherwise, and the next step puts
            // the eye where the body really goes. A jump no body makes in a
            // step -- a teleport, a respawn -- is not carried. The body is
            // carried with it where it is drawn (ModPresentationCarry).
            //
            // What it looks at is carried the same way -- a ball's camera
            // swings round it, Trace's turns with the mouse, a pad turns the
            // view a step at a time -- unless this frame's own mouse turns the
            // sight line (FrameAim), when the target goes with the eye.
            constexpr float MaxStep = Mods::Render::MotionCarry::JumpDistance;
            const ::MphRead::Entities::CameraInfo& camera = *_cameraInfo;
            const Mods::Render::PlayerPoseHistory& pose = _modPose;
            const Vector3 stepped = pose.CurrentEye - pose.PreviousEye;
            const Vector3 looked = pose.CurrentTarget - pose.PreviousTarget;
            const float phase = static_cast<float>(Mods::Render::ScenePresentation::Phase());
            if (Mods::Render::ScenePresentation::Moving() && pose.HasPrevious
                && pose.Step == Mods::Render::ScenePresentation::CurrentStep()
                && Same(camera.Position, pose.CurrentEye) && Same(camera.Target, pose.CurrentTarget)
                && stepped.LengthSquared() < MaxStep * MaxStep && looked.LengthSquared() < MaxStep * MaxStep)
            {
                const Vector3 eye = ::OpenTK::Mathematics::Multiply(stepped, phase);
                const bool mouseTurns = live != nullptr && (ModFrameAimAllowed(true) || ModFrameAimAllowed(false));
                const Vector3 turn = mouseTurns || idleSway ? Vector3{} : ::OpenTK::Mathematics::Multiply(looked - stepped, phase);
                if (eye.LengthSquared() > 0 || turn.LengthSquared() > 0)
                {
                    ModSaveFrameView();
                    ModCarryCamera(eye, turn);
                    shown = true;
                }
            }
        }
        // The DS-style aim, from the eye this picture has.
        if (idleSway)
        {
            ModPresentIdleSway();
            shown = true;
        }
        else if (live != nullptr && ModPresentFrameAim(*live)) shown = true;
        if (shown)
        {
            _modFrameView.ShownGun = _gunVec1;
            _modFrameView.ShownFacing = _facingVector;
        }
        return shown;
    }

    void PlayerEntity::ModSaveFrameView()
    {
        Mods::Input::FrameAimView& view = _modFrameView;
        if (view.Active) return;
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
        if (_cameraInfo != nullptr)
        {
            view.Camera = true;
            view.CameraPosition = _cameraInfo->Position;
            view.CameraTarget = _cameraInfo->Target;
            view.CameraFacing = _cameraInfo->Facing;
            view.CameraTrueUp = _cameraInfo->TrueUp;
            view.CameraView = _cameraInfo->ViewMatrix;
        }
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
        view.Watched = true;
        ModCarryCamera(eye, turn);
        view.ShownGun = _gunVec1;
        view.ShownFacing = _facingVector;
        return true;
    }

    void PlayerEntity::ModCarryCamera(::OpenTK::Mathematics::Vector3 eye, ::OpenTK::Mathematics::Vector3 turn)
    {
        using ::OpenTK::Mathematics::Matrix4;
        using ::OpenTK::Mathematics::Vector3;
        ::MphRead::Entities::CameraInfo& camera = *_cameraInfo;
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
        ModPlaceGunEffects();
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
        ModPlaceGunEffects();
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
        if (_modFrameView.Active && (_modFrameView.Watched || _modFrameView.Aimed))
        {
            // A watched view is already the camera's own matrix.
            if (_modFrameView.Watched || !Same(_facingVector, _modFrameView.ShownFacing)) return std::nullopt;
            return _facingVector - _modFrameView.Facing;
        }
        const Mods::Input::FrameAimState& state = _modFrameAim;
        if (!state.Active || state.Step != CurrentStep()) return std::nullopt;
        return _facingVector - state.CameraFacing;
    }
}
