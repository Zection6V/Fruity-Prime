#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../Headless.hpp"
#include "../Network/Gameplay/Replication/NetSmoothing.hpp"
#include "../Network/Session/NetSession.hpp"
#include "../../Formats/Effects.hpp"
#include "AnimationBlend.hpp"
#include "AnimationClock.hpp"
#include "EffectPlacement.hpp"
#include "MotionCarry.hpp"
#include "ScenePresentation.hpp"
#include <algorithm>

namespace MphRead::Entities
{
    using ::MphRead::NativeRuntime::ManagedAt;
    using ::MphRead::NativeRuntime::RequireReference;
    using ::OpenTK::Mathematics::Matrix4;
    using ::OpenTK::Mathematics::Vector3;

    void PlayerEntity::ModNotePresentationStep(std::uint64_t step)
    {
        Mods::Render::PlayerPoseHistory& pose = _modPose;
        const bool alt = IsAltForm();
        // The pose the draw builds the body from: in alt form the ball's own
        // transform (its roll, Noxus's spin, Trace's tilt) at the position,
        // which is what PlayerDraw puts in _modelTransform; on foot, where
        // the body is, its turn being the facing's.
        Matrix4 now = ::OpenTK::Mathematics::CreateTranslation(static_cast<Vector3>(Position));
        if (alt)
        {
            now = _modelTransform;
            now.M41 = Position.X;
            now.M42 = Position.Y;
            now.M43 = Position.Z;
        }
        pose.HasPrevious = pose.Step + 1 == step;
        pose.SameForm = pose.HasPrevious && pose.AltForm == alt;
        pose.Previous = pose.Current;
        pose.Current = now;
        pose.PreviousSegments = pose.CurrentSegments;
        pose.CurrentSegments = _kandenSegMtx;
        pose.PreviousEye = pose.CurrentEye;
        pose.PreviousTarget = pose.CurrentTarget;
        if (_cameraInfo != nullptr)
        {
            pose.CurrentEye = _cameraInfo->Position;
            pose.CurrentShake = _cameraInfo->ModShakeOffset;
            pose.CurrentTarget = _cameraInfo->Target - pose.CurrentShake;
            if (_cameraInfo->ModShakeSlot)
            {
                pose.ShakeFrom = pose.ShakeTo;
                pose.ShakeTo = pose.CurrentShake;
                pose.ShakeStep = step;
            }
        }
        if (_cameraInfo != nullptr)
        {
            // The trailing camera's place against the body, a step at a time.
            // A form or camera that changed moved what the camera is held
            // against (the body's point drops as a morph ends), not the
            // camera: what came before is no history of it.
            const auto cameraType = static_cast<std::int32_t>(_cameraType);
            if (!pose.SameForm || cameraType != pose.TrailCamera) pose.TrailSteps = 0;
            pose.TrailCamera = cameraType;
            std::rotate(pose.TrailEye.begin(), pose.TrailEye.begin() + 1, pose.TrailEye.end());
            // A replica's camera was built where the body was before the
            // snapshot moved it (ModNoteCameraSource).
            const auto& source = _modCameraSource;
            const Vector3 from = source.Valid && source.Step == Mods::Render::AnimationClock::Simulation().TickTime()
                ? source.Position : static_cast<Vector3>(Position);
            pose.TrailEye.back() = pose.CurrentEye - from;
            pose.TrailSteps = std::min<std::int32_t>(pose.TrailSteps + 1, static_cast<std::int32_t>(pose.TrailEye.size()));
        }
        pose.AltForm = alt;
        pose.Step = step;
        ModLookAheadKandenSegments();
    }

    void PlayerEntity::ModLookAheadKandenSegments()
    {
        Mods::Render::PlayerPoseHistory& pose = _modPose;
        pose.HasNextSegments = false;
        if (_hunter != Hunter::Kanden || !IsAltForm() || Mods::Headless::Active()) return;
        // The segments wiggle by the frame number on a loop of the DS's own
        // making -- it holds them still for a step as it wraps -- so their
        // next pose is asked of the game's own update, one frame on, with the
        // body moved on by one step. Everything it touches is put back.
        const auto segPos = _kandenSegPos;
        const auto segMtx = _kandenSegMtx;
        const Vector3 at = Position;
        const Vector3 motion = Mods::Network::NetSmoothing::Active() && SlotIndex() != Mods::Network::NetSession::LocalSlot()
            ? Mods::Network::NetSmoothing::AheadShift(SlotIndex(), 1.0)
            : at - PrevPosition();
        if (!Mods::Render::MotionCarry::Within(motion))
        {
            return;
        }
        Position = at + motion;
        _modSegmentLookAhead = 1;
        UpdateStinglarvaSegments();
        _modSegmentLookAhead = 0;
        pose.NextSegments = _kandenSegMtx;
        pose.HasNextSegments = true;
        _kandenSegPos = segPos;
        _kandenSegMtx = segMtx;
        Position = at;
    }

    bool PlayerEntity::ModPresentationMoving() const
    {
        if (!Mods::Render::ScenePresentation::Moving() || !_modPose.SameForm
            || _modPose.Step != Mods::Render::ScenePresentation::CurrentStep())
        {
            return false;
        }
        // In first person the body is not drawn, and the eye and the gun are
        // presented with the camera (ModPresentOwnView, ModPresentWatchedView)
        // -- except while the camera switches, through a morph or an unmorph,
        // when PlayerDraw draws the body too (its drawBiped).
        return !(IsMainPlayer() && _cameraType == CameraType::First && !IsAltForm()
            && Formats::CameraSequence::Current() == nullptr
            && _camSwitchTimer >= Values().CamSwitchTime * 2);
    }

    bool PlayerEntity::ModCarriesKandenSegments() const
    {
        // Kanden's segments wiggle on phases of their own and trail the head
        // round a turn: each is carried on by its own motion, not the head's.
        return _hunter == Hunter::Kanden && IsAltForm() && _modPose.AltForm;
    }

    std::optional<Vector3> PlayerEntity::ModTrailShift() const
    {
        // A ball's camera, or any that trails, held off a wall fights it
        // (PlayerEntity::UpdateCameraThird1, the ROM's own pull-in and slide,
        // run at 60 Hz): to and fro every step, or snapped in and easing out
        // again. Carried on along each step, the swing was drawn twice over
        // and a snap dove through the body and back; against the body, it
        // moves as MotionCarry::Trail reads its last steps.
        const Mods::Render::PlayerPoseHistory& pose = _modPose;
        if ((_cameraType == CameraType::First && !IsAltForm())
            || pose.Step != Mods::Render::ScenePresentation::CurrentStep()
            || pose.TrailSteps != static_cast<std::int32_t>(pose.TrailEye.size()))
        {
            return std::nullopt;
        }
        return Mods::Render::MotionCarry::Trail(pose.TrailEye, Mods::Render::ScenePresentation::ShownPhase())
            - pose.TrailEye.back();
    }

    std::optional<Matrix4> PlayerEntity::ModPresentationCarry() const
    {
        if (ModCarriesKandenSegments()) return std::nullopt;
        const Mods::Render::PlayerPoseHistory& pose = _modPose;
        const bool moving = ModPresentationMoving();
        const float phase = moving ? Mods::Render::ScenePresentation::ShownPhase() : 0.0F;
        // A ball's roll, Noxus's spin and tilt are turned by the game every
        // step, but they are what the Animation setting is about on a ball:
        // they are shown on its grid (30 Hz holds each for two steps, back to
        // the previous step's turn on the second), while the ball itself moves
        // on at the picture's instant like everything else.
        float turn = phase;
        const bool rolls = _hunter == Hunter::Samus || _hunter == Hunter::Spire || _hunter == Hunter::Noxus;
        if (rolls && pose.AltForm && pose.SameForm && IsAltForm()
            && pose.Step == Mods::Render::ScenePresentation::CurrentStep())
        {
            turn = static_cast<float>(Mods::Render::AnimationBlend::ShownStepFraction(pose.Step, phase));
        }
        if (!moving && !(turn < 0.0F)) return std::nullopt;
        const auto carried = Mods::Render::MotionCarry::BetweenShown(pose.Previous, pose.Current, phase, turn);
        // A replica on a client is read off the playout, which holds what has
        // arrived past the step's read point: its position is no guess. The
        // turn it is drawn with is simulated here, and carried as any other.
        if (moving && Mods::Network::NetSmoothing::Active() && SlotIndex() != Mods::Network::NetSession::LocalSlot())
        {
            const Vector3 ahead = Mods::Network::NetSmoothing::AheadShift(SlotIndex(), phase);
            Matrix4 drawn = carried ? pose.Current * *carried : pose.Current;
            drawn.M41 = pose.Current.M41 + ahead.X;
            drawn.M42 = pose.Current.M42 + ahead.Y;
            drawn.M43 = pose.Current.M43 + ahead.Z;
            const Matrix4 carry = ::OpenTK::Mathematics::Inverted(pose.Current) * drawn;
            if (!carried && ahead.LengthSquared() <= 0.0F) return std::nullopt;
            return carry;
        }
        return carried;
    }

    std::optional<std::pair<Vector3, Vector3>> PlayerEntity::ModPresentedMuzzle() const
    {
        if (!Mods::Render::ScenePresentation::Moving()) return std::nullopt;
        // The gun a shot leaves from (PlayerEntity::TryFireWeapon: the muzzle,
        // towards the aim). A local view already holds this picture's gun
        // (ModPresentOwnView, ModPresentWatchedView); a body drawn on from its
        // step carries its gun with it.
        Vector3 muzzle = _muzzlePos;
        Vector3 aimed = _aimPosition;
        if (const auto carry = ModPresentationCarry())
        {
            muzzle = Mods::Render::MotionCarry::Point(muzzle, *carry);
            aimed = Mods::Render::MotionCarry::Point(aimed, *carry);
        }
        return std::pair{muzzle, aimed - muzzle};
    }

    Vector3 PlayerEntity::ModPresentedPoint(Vector3 point) const
    {
        if (!Mods::Render::ScenePresentation::Moving()) return point;
        // A local or watched view carried its camera with the body
        // (ModCarryCamera): the body is where that eye has gone.
        const Mods::Input::FrameAimView& view = _modFrameView;
        if (view.Active && view.Camera && _cameraInfo != nullptr)
        {
            return point + (_cameraInfo->Position - view.CameraPosition);
        }
        if (const auto carry = ModPresentationCarry()) return Mods::Render::MotionCarry::Point(point, *carry);
        return point;
    }

    void PlayerEntity::ModGunEffectsDrawn(bool gunDrawn)
    {
        // A flash fired just before a morph lives on its own lifetime (a
        // continuous weapon's, across shots) and nothing in the game ends it:
        // through the morph the body is drawn and turns it back on at the
        // body's gun, and in alt form it hangs where the gun had been --
        // carried with the camera, on the screen. Yours is drawn only while
        // your first-person gun is; anybody else's keeps PlayerDraw's own
        // rule. Drawing only: the effects run their lives as before.
        if (!IsMainPlayer()) return;
        if (_muzzleEffect != nullptr) _muzzleEffect->SetDrawEnabled(gunDrawn);
        if (_chargeEffect != nullptr) _chargeEffect->SetDrawEnabled(gunDrawn);
    }

    void PlayerEntity::ModPlaceBodyEffectsDrawn()
    {
        // Process anchors these to the body at the step's pose. Between steps
        // the body is drawn on -- carried by its motion, or a replica read off
        // the playout at the picture's instant -- and an effect left at the
        // step's pose trails the ball it belongs to, a boost by most of a step.
        // Same anchors as Process, moved with the body this picture draws.
        if (!Mods::Render::ScenePresentation::Moving()) return;
        if (_boostEffect != nullptr && !_boostEffect->Elements->empty() && _boostEffect->Elements->front() != nullptr)
        {
            const Matrix4& own = _boostEffect->Elements->front()->OwnTransform;
            const Vector3 from(own.M41, own.M42, own.M43);
            const Vector3 to = ModPresentedPoint(static_cast<Vector3>(Position));
            Mods::Render::BodyEffectDraws::Note(::OpenTK::Mathematics::Length(to - from));
        }
        const Vector3 body = ModPresentedPoint(static_cast<Vector3>(Position));
        const Vector3 sphere = ModPresentedPoint(_volume.SpherePosition);
        Mods::Render::PlaceEffect(_boostEffect, _gunVec2, _facingVector, body);
        Mods::Render::PlaceEffect(_furlEffect, _gunVec2, _facingVector, body);
        Mods::Render::PlaceEffect(_deathaltEffect, Vector3(0.0F, 1.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F), sphere);
        if (!IsMainPlayer() || IsAltForm() || IsMorphing())
        {
            Mods::Render::PlaceEffect(_burnEffect, Vector3(_field70, 0.0F, _field74), Vector3(0.0F, 1.0F, 0.0F), sphere);
        }
    }

    Matrix4 PlayerEntity::ModKandenSegmentDrawn(std::int32_t index) const
    {
        const Matrix4& now = _kandenSegMtx.at(static_cast<std::size_t>(index));
        if (!ModPresentationMoving() || !ModCarriesKandenSegments()) return now;
        const auto at = static_cast<std::size_t>(index);
        const float phase = static_cast<float>(Mods::Render::ScenePresentation::Phase());
        // Toward the pose the game's own update gives next, where it was asked.
        const auto carry = _modPose.HasNextSegments
            ? Mods::Render::MotionCarry::Toward(_modPose.CurrentSegments[at], _modPose.NextSegments[at], phase)
            : Mods::Render::MotionCarry::Between(_modPose.PreviousSegments[at], _modPose.CurrentSegments[at], phase);
        return carry ? now * *carry : now;
    }

    void PlayerEntity::ModRestoreKandenSegments()
    {
        // The alt model's node matrices are the step's again, whatever the
        // picture drew, for anything that reads them before the next draw.
        if (_altModel == nullptr) return;
        Model& model = RequireReference(_altModel->Model());
        for (std::size_t i = 0; i < _kandenSegMtx.size(); ++i)
        {
            RequireReference(ManagedAt(model.Nodes, static_cast<std::int32_t>(i))).Animation = _kandenSegMtx[i];
        }
    }
}
