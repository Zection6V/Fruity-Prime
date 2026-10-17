#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../Features.hpp"
#include "../../Formats/Collision.hpp"
#include "../../Formats/Effects.hpp"
#include "../../Formats/Types.hpp"
#include "../../HUD/HudInfo.hpp"
#include "../../Scene.hpp"
#include "../Render/AnimationClock.hpp"
#include "../Render/EffectPlacement.hpp"
#include "../Render/FrameTiming.hpp"
#include "../Render/MotionCarry.hpp"
#include "../Render/ScenePresentation.hpp"
#include "../SimulationRate.hpp"
#include "../Network/Gameplay/Replication/NetSmoothing.hpp"
#include "../Network/Session/NetSession.hpp"
#include "../SpectatorMode.hpp"
#include "AimNumericPolicy.hpp"
#include "MouseAimTurn.hpp"
#include "PointerDevice.hpp"
#include "PointerInput.hpp"

#include <algorithm>
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

    void PlayerEntity::ModPlaceGunEffects()
    {
        if (!IsMainPlayer() && _aimFrame.Owner != Mods::Input::AimOwner::Local) return;
        Mods::Render::PlaceEffect(_muzzleEffect, _gunVec2, _gunVec1, _muzzlePos);
        Mods::Render::PlaceEffect(_chargeEffect, _gunVec2, _gunVec1, _muzzlePos);
        // The double damage glow round the gun, and the flames on it while
        // burning, placed as the step places them (on a ball, or on a body
        // seen from outside, the flames are the body's).
        Mods::Render::PlaceEffect(_doubleDmgEffect, _upVector, _gunVec1, _muzzlePos);
        if (IsMainPlayer() && !IsAltForm() && !IsMorphing())
        {
            Mods::Render::PlaceEffect(_burnEffect, _upVector, _gunVec1, _muzzlePos);
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
        ModRestoreFrameAim();
        const bool idleSway = ModIdleSwayPresentable(live);
        bool shown = false;
        // Dead too: the death camera flies at the step's rate, and is carried
        // on between steps like any other (a respawn is a jump, not carried).
        if (Mods::Input::FrameAim::Enabled() && !Mods::SpectatorMode::IsSpectating()
            && _aimFrame.Owner == Mods::Input::AimOwner::Local && Formats::CameraSequence::Current() == nullptr)
        {
            // What it looks at is carried with the eye -- a ball's camera
            // swings round it, Trace's turns with the mouse, a pad turns the
            // view a step at a time -- unless this frame's own mouse turns the
            // sight line (FrameAim), when the target goes with the eye.
            const bool mouseTurns = live != nullptr && (ModFrameAimAllowed(true) || ModFrameAimAllowed(false));
            shown = ModCarryStepCamera(!mouseTurns && !idleSway);
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

    bool PlayerEntity::ModCarryStepCamera(bool turnWithStep)
    {
        using ::OpenTK::Mathematics::Vector3;
        if (_cameraInfo == nullptr) return false;
        const ::MphRead::Entities::CameraInfo& camera = *_cameraInfo;
        const Mods::Render::PlayerPoseHistory& pose = _modPose;
        if (pose.Step != Mods::Render::ScenePresentation::CurrentStep()
            || !Same(camera.Position, pose.CurrentEye) || !Same(camera.Target, pose.CurrentTarget + pose.CurrentShake))
        {
            return false;
        }
        // The step moved the eye with the body, and the picture is a
        // fraction of a step on from it: carried on along that step's own
        // motion, the eye moves every frame where it otherwise jumps 60
        // times a second under a view that turns every frame. Nothing has
        // happened yet that could say otherwise, and the next step puts
        // the eye where the body really goes. A jump no body makes in a
        // step -- a teleport, a respawn -- is not carried. The body is
        // carried with it where it is drawn (ModPresentationCarry).
        constexpr float MaxStep = Mods::Render::MotionCarry::JumpDistance;
        const Vector3 stepped = pose.CurrentEye - pose.PreviousEye;
        const Vector3 looked = pose.CurrentTarget - pose.PreviousTarget;
        Vector3 eye{};
        Vector3 turn{};
        if (pose.HasPrevious && stepped.LengthSquared() < MaxStep * MaxStep && looked.LengthSquared() < MaxStep * MaxStep)
        {
            const float phase = Mods::Render::ScenePresentation::Moving()
                ? static_cast<float>(Mods::Render::ScenePresentation::Phase()) : 0.0F;
            eye = ::OpenTK::Mathematics::Multiply(stepped, phase);
            // A ball's camera, or any that trails, held off a wall goes to and
            // fro every step (PlayerEntity::UpdateCameraThird1, the ROM's own
            // pull-in and slide, run at 60 Hz): carried on along each step,
            // the swing was drawn twice over. The picture shows the middle of
            // it, moving on as it does, on the step's own instant too; a
            // camera that only moves steadily is shown where it was.
            const bool trails = _cameraType != CameraType::First || IsAltForm();
            if (trails && pose.HasEarlier && (pose.PreviousEye - pose.EarlierEye).LengthSquared() < MaxStep * MaxStep)
            {
                eye = Mods::Render::MotionCarry::Settled(pose.EarlierEye, pose.PreviousEye, pose.CurrentEye, phase)
                    - pose.CurrentEye;
            }
            // What it looks at is carried on along the step's own motion.
            if (turnWithStep) turn = ::OpenTK::Mathematics::Multiply(looked, phase) - eye;
        }
        return ModPresentCamera(eye, turn);
    }

    bool PlayerEntity::ModPresentCamera(::OpenTK::Mathematics::Vector3 eye, ::OpenTK::Mathematics::Vector3 turn)
    {
        using ::OpenTK::Mathematics::Vector3;
        const ::MphRead::Entities::CameraInfo& camera = *_cameraInfo;
        const Mods::Render::PlayerPoseHistory& pose = _modPose;
        // The hit shake is a fresh random offset on every other step
        // (CameraInfo::Update), held for one and gone for the next:
        // drawn as it is, a 30 Hz flicker. The camera is carried
        // without it, and the picture eases from the shake's previous
        // offset to its newest over the steps between them, applied to
        // the finished view (FrameAim::View) so that it tilts the
        // picture and never turns its heading or its roll -- which, as
        // a world offset on a sight line looking straight down, it does.
        // A picture on the step's own instant too: drawn raw there, the
        // shake jumped to its newest offset whenever a frame met a step.
        Vector3 shake{};
        if (pose.Step == Mods::Render::ScenePresentation::CurrentStep()
            && Same(camera.Target, pose.CurrentTarget + pose.CurrentShake))
        {
            const float phase = Mods::Render::ScenePresentation::Moving()
                ? static_cast<float>(Mods::Render::ScenePresentation::Phase()) : 0.0F;
            const float age = std::clamp((static_cast<float>(pose.Step - pose.ShakeStep) + phase)
                / static_cast<float>(Mods::SimulationRate::StepsPerDsFrame), 0.0F, 1.0F);
            shake = pose.ShakeFrom + ::OpenTK::Mathematics::Multiply(pose.ShakeTo - pose.ShakeFrom, age);
            turn = turn - pose.CurrentShake;
        }
        if (eye.LengthSquared() == 0 && turn.LengthSquared() == 0 && shake.LengthSquared() == 0) return false;
        ModSaveFrameView();
        ModCarryCamera(eye, turn);
        _modFrameView.Shaken = shake.LengthSquared() > 0;
        _modFrameView.Shake = shake;
        return true;
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
            view.CameraNode = _cameraInfo->NodeRef;
        }
    }

    void PlayerEntity::ModNoteCameraSource()
    {
        _modCameraSource = {true, CurrentStep(), static_cast<::OpenTK::Mathematics::Vector3>(Position), _facingVector};
    }

    bool PlayerEntity::ModPresentWatchedView(std::optional<::OpenTK::Mathematics::Vector3> playoutShift)
    {
        using ::OpenTK::Mathematics::Vector3;
        ModRestoreFrameAim();
        if (Health() <= 0 || _cameraInfo == nullptr) return false;
        bool shown = false;
        if (!playoutShift)
        {
            // Simulated here (offline, or on the authority): the watched
            // player's camera is carried on from its step as the own view's is.
            shown = ModCarryStepCamera(true);
        }
        else
        {
            const Mods::Input::CameraSource& source = _modCameraSource;
            if (!source.Valid || source.Step != CurrentStep()) return false;
            // The step built this camera, then the snapshot moved the player and
            // turned them (ApplyState runs after the entities): the eye goes where
            // the body is, and on along the playout to this picture's instant.
            const Vector3 eye = *playoutShift + (static_cast<Vector3>(Position) - source.Position);
            // Only a first-person sight line is the facing. The ball's camera
            // trails, swinging round it a step at a time: on along that swing.
            const Mods::Render::PlayerPoseHistory& pose = _modPose;
            const Vector3 swing = (pose.CurrentTarget - pose.PreviousTarget) - (pose.CurrentEye - pose.PreviousEye);
            const Vector3 turn = _cameraType == CameraType::First && !IsAltForm() ? _facingVector - source.Facing
                : Mods::Render::ScenePresentation::Moving() && pose.HasPrevious
                    && pose.Step == Mods::Render::ScenePresentation::CurrentStep()
                    && swing.LengthSquared() < Mods::Render::MotionCarry::JumpDistance * Mods::Render::MotionCarry::JumpDistance
                ? ::OpenTK::Mathematics::Multiply(swing, static_cast<float>(Mods::Render::ScenePresentation::Phase()))
                : Vector3{};
            shown = ModPresentCamera(eye, turn);
        }
        if (!shown) return false;
        Mods::Input::FrameAimView& view = _modFrameView;
        view.Watched = true;
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
        // The room draws the parts seen from the camera's part (RoomEntity::
        // UpdateRoomParts): an eye carried through a doorway is in the next
        // part, and tested from the old one the far side is culled -- black.
        // The step's own lookup, for the picture's eye; put back with the rest.
        if (eye.LengthSquared() > 0 && camera.NodeRef.PartIndex != -1 && _scene != nullptr)
        {
            camera.NodeRef = _scene->UpdateNodeRef(camera.NodeRef, camera.Position, camera.Position + eye);
        }
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
            _cameraInfo->NodeRef = view.CameraNode;
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

    std::optional<::OpenTK::Mathematics::Vector3> PlayerEntity::ModFrameShake() const
    {
        if (!_modFrameView.Active || !_modFrameView.Shaken) return std::nullopt;
        return _modFrameView.Shake;
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
