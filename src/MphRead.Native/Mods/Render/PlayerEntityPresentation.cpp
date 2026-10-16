#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../Headless.hpp"
#include "../Network/Gameplay/Replication/NetSmoothing.hpp"
#include "../Network/Session/NetSession.hpp"
#include "MotionCarry.hpp"
#include "ScenePresentation.hpp"

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
            pose.CurrentTarget = _cameraInfo->Target;
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
        if (motion.LengthSquared() > Mods::Render::MotionCarry::JumpDistance * Mods::Render::MotionCarry::JumpDistance)
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

    std::optional<Matrix4> PlayerEntity::ModPresentationCarry() const
    {
        if (!ModPresentationMoving() || ModCarriesKandenSegments()) return std::nullopt;
        const Mods::Render::PlayerPoseHistory& pose = _modPose;
        const float phase = static_cast<float>(Mods::Render::ScenePresentation::Phase());
        const auto carried = Mods::Render::MotionCarry::Between(pose.Previous, pose.Current, phase);
        // A replica on a client is read off the playout, which holds what has
        // arrived past the step's read point: its position is no guess. The
        // turn it is drawn with is simulated here, and carried as any other.
        if (Mods::Network::NetSmoothing::Active() && SlotIndex() != Mods::Network::NetSession::LocalSlot())
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
