#include "Mods/Network/Gameplay/Weapons/HomingTargetAcquisition.hpp"

#include "Entities/BeamProjectileEntity.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Formats/Types.hpp"
#include "Metadata/Metadata.hpp"
#include "Mods/Network/Gameplay/Weapons/HomingTargetHistory.hpp"
#include "Mods/Network/Gameplay/Weapons/HomingTargetReference.hpp"
#include "Mods/Network/Gameplay/Weapons/HomingTargetSelection.hpp"
#include "Mods/Network/Gameplay/Weapons/NetShotEvents.hpp"
#include "Mods/Network/Gameplay/Weapons/ShotOwnership.hpp"
#include "Mods/Network/Diagnostics/NetShotDiagnostics.hpp"
#include "Mods/Network/Diagnostics/NetLog.hpp"
#include "Mods/Network/Session/NetSession.hpp"
#include "NativeRuntime/System/Managed.hpp"
#include "Scene.hpp"

#include <optional>
#include <vector>

namespace MphRead::Mods::Network
{
    using Entities::BeamFlags;
    using Entities::BeamProjectileEntity;
    using Entities::EntityBase;
    using Entities::PlayerEntity;
    using OpenTK::Mathematics::Vector3;
    using TargetReference = IntentPacket::ShotTargetReference;

    namespace
    {
        // What the shot's own record says about its target, when this copy
        // is not the one that chooses.
        struct TargetDecision final
        {
            bool Received = false;
            bool Remote = false;
            bool Trusted = false;
            std::optional<TargetReference> Target{};
            bool HasRay = false;
            Vector3 RayOrigin{};
            Vector3 RayDirection{};
            std::uint32_t Frame = 0;
            std::uint32_t Sequence = 0;
        };

        [[nodiscard]] TargetDecision ReadDecision(const BeamProjectileEntity& beam, const PlayerEntity* owner)
        {
            TargetDecision decision{};
            if (!NetSession::Active() || owner == nullptr)
            {
                return decision;
            }
            decision.Remote = !ShotOwnership::IsOwner(*owner);
            decision.Trusted = decision.Remote && !ShotOwnership::Judges();
            if (::MphRead::TestFlag(beam.Flags(), BeamFlags::Continuous))
            {
                if (decision.Remote && NetSession::HasRemoteIntent(owner->SlotIndex()))
                {
                    const IntentPacket& intent = NetSession::RemoteIntent(static_cast<std::size_t>(owner->SlotIndex()));
                    decision.Received = true;
                    decision.Frame = intent.AckFrame;
                    decision.HasRay = intent.HasShot;
                    decision.RayOrigin = intent.ShotOrigin;
                    decision.RayDirection = intent.ShotDirection;
                    if (intent.HasContinuousTarget)
                    {
                        decision.Target = intent.ContinuousTarget;
                    }
                }
            }
            else if (const auto event = NetShotEvents::CurrentShotEvent(*owner); event.has_value())
            {
                decision.Received = decision.Remote || event->HasHomingDecision();
                decision.Frame = event->AckFrame;
                decision.Sequence = event->Sequence;
                decision.HasRay = true;
                decision.RayOrigin = event->Origin;
                decision.RayDirection = event->Direction;
                if (event->HasHomingDecision())
                {
                    decision.Target = event->Target;
                }
            }
            return decision;
        }

        // The authority's check of a remote shooter's choice, against where
        // shooter and target were in the frame the shot was fired from.
        [[nodiscard]] bool ValidAtFire(const TargetDecision& decision, const PlayerEntity& owner,
            HomingTargetSelectionRequest request)
        {
            const auto ownerAtFire = HomingTargetHistory::FindPlayer(decision.Frame, owner.SlotIndex());
            const auto targetAtFire = HomingTargetHistory::Find(decision.Frame, *decision.Target);
            const HomingFireTargetValidation validation = HomingTargetSelection::ValidateFireTarget(
                ownerAtFire.has_value() ? &*ownerAtFire : nullptr, owner.SlotIndex(),
                targetAtFire.has_value() ? &*targetAtFire : nullptr, request,
                decision.HasRay, decision.RayOrigin, decision.RayDirection);
            const ShotKey key = ShotKey::For(owner.SlotIndex(), decision.Frame);
            NetShotDiagnostics::TraceRayValidation("homing-ray-validation",
                key, request.Beam,
                decision.Sequence, decision.Frame, decision.RayOrigin, decision.RayDirection,
                ownerAtFire.has_value() ? &ownerAtFire->Position : nullptr,
                ownerAtFire.has_value() ? ownerAtFire->Reference.Generation : 0,
                ownerAtFire.has_value() ? ownerAtFire->Reference.LifeId : 0,
                validation.RayValid, validation.Accepted);
            const IntentPacket::ShotTargetReference& target = *decision.Target;
            NetShotDiagnostics::Trace("homing-target-validation", key, request.Beam,
                "shotSequence=" + std::to_string(decision.Sequence)
                + " ackFrame=" + std::to_string(decision.Frame)
                + " targetKind=" + std::to_string(static_cast<std::uint8_t>(target.Kind))
                + " targetSlot=" + std::to_string(target.Slot)
                + " targetGeneration=" + std::to_string(target.Generation)
                + " targetLifeId=" + std::to_string(target.LifeId)
                + " targetEntityId=" + std::to_string(target.EntityId)
                + " targetStableId=" + std::to_string(target.StableId)
                + " targetAtFire=" + (targetAtFire.has_value() ? "true" : "false")
                + " acquired=" + (validation.Accepted ? "true" : "false"));
            return validation.Accepted;
        }

        void RecordChoice(const BeamProjectileEntity& beam, const PlayerEntity& owner, const TargetReference& selected)
        {
            if (::MphRead::TestFlag(beam.Flags(), BeamFlags::Continuous))
            {
                NetShotEvents::RecordContinuousTarget(owner.SlotIndex(), selected);
            }
            else if (NetShotEvents::CurrentShotEvent(owner).has_value())
            {
                NetShotEvents::RecordHomingTarget(owner, selected);
            }
        }
    }

    std::string ToString(HomingTargetAcquisition::Outcome value)
    {
        using Outcome = HomingTargetAcquisition::Outcome;
        switch (value)
        {
        case Outcome::NotAttempted: return "-";
        case Outcome::Chosen: return "chosen";
        case Outcome::NoneInCone: return "none";
        case Outcome::Received: return "received";
        case Outcome::ReceivedNone: return "received-none";
        case Outcome::ReceivedIneligible: return "received-ineligible";
        case Outcome::Refused: return "refused";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    HomingTargetAcquisition::Outcome HomingTargetAcquisition::ConsumeOutcome() noexcept
    {
        const Outcome outcome = _outcome;
        _outcome = Outcome::NotAttempted;
        return outcome;
    }

    bool HomingTargetAcquisition::Finish(Outcome outcome, bool result) noexcept
    {
        _outcome = outcome;
        return result;
    }

    bool HomingTargetAcquisition::Acquire(BeamProjectileEntity& beam, const EquipInfo& equip, Scene& scene)
    {
        const std::shared_ptr<EntityBase> owner = beam.Owner();
        const PlayerEntity* ownerPlayer = owner != nullptr && owner->Type == EntityType::Player
            ? static_cast<const PlayerEntity*>(owner.get()) : nullptr;
        const bool continuous = ::MphRead::TestFlag(beam.Flags(), BeamFlags::Continuous);
        const TargetDecision decision = ReadDecision(beam, ownerPlayer);
        if (decision.Received && (!decision.Target.has_value()
            || decision.Target->Kind == IntentPacket::ShotTargetKind::None))
        {
            return Finish(Outcome::ReceivedNone, false);
        }

        const WeaponInfo& weapon = NativeRuntime::RequireReference(equip.Weapon);
        HomingTargetSelectionRequest request{};
        request.ExcludedTarget = HomingTargetReference::Encode(owner.get());
        request.Origin = static_cast<Vector3>(beam.Position);
        request.Direction = beam.Velocity();
        request.OwnerType = NativeRuntime::RequireReference(owner).Type;
        request.Beam = beam.BeamKind();
        request.OwnerSlot = ownerPlayer != nullptr ? ownerPlayer->SlotIndex() : -1;
        request.OwnerTeam = ownerPlayer != nullptr ? ownerPlayer->TeamIndex() : -1;
        request.Range = Fixed::ToFloat(weapon.HomingRange);
        request.Tolerance = Fixed::ToFloat(equip.HomingTolerance());
        request.Continuous = ::MphRead::TestFlag(weapon.Flags, WeaponFlags::Continuous);
        request.RequireReference = NetSession::Active();

        bool validated = false;
        if (decision.Remote && decision.Target.has_value() && ShotOwnership::Judges())
        {
            if (!ValidAtFire(decision, *ownerPlayer, request))
            {
                return Finish(Outcome::Refused, false);
            }
            validated = true;
        }

        std::vector<HomingTargetSnapshot> snapshots{};
        std::vector<std::shared_ptr<EntityBase>> entities{};
        for (const EntityType type : HomingTargetHistory::TargetTypes)
        {
            auto enumerator = scene.Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                const std::shared_ptr<EntityBase> entity = enumerator.Current();
                if (!entity || entity->Type != type)
                {
                    continue;
                }
                const HomingTargetSnapshot snapshot = HomingTargetHistory::Capture(*entity);
                if ((request.RequireReference && (!snapshot.Reference.IsValid()
                        || snapshot.Reference.Kind == IntentPacket::ShotTargetKind::None))
                    || (decision.Received && snapshot.Reference != *decision.Target))
                {
                    continue;
                }
                if (decision.Received && (decision.Trusted || validated))
                {
                    // Somebody else chose: take that entity as it stands now.
                    if (HomingTargetSelection::IsEligible(snapshot, request))
                    {
                        beam.SetTarget(entity);
                        return Finish(Outcome::Received, continuous);
                    }
                    continue;
                }
                snapshots.push_back(snapshot);
                entities.push_back(entity);
            }
        }

        if (decision.Received && (decision.Trusted || validated))
        {
            return Finish(Outcome::ReceivedIneligible, false);
        }
        const std::optional<std::size_t> selectedIndex = HomingTargetSelection::SelectIndex(snapshots, request);
        const TargetReference selected = selectedIndex.has_value()
            ? snapshots[*selectedIndex].Reference : TargetReference{};
        if (selectedIndex.has_value())
        {
            beam.SetTarget(entities[*selectedIndex]);
        }
        if (!decision.Received && ownerPlayer != nullptr)
        {
            RecordChoice(beam, *ownerPlayer, selected);
        }
        const bool acquired = selectedIndex.has_value();
        return Finish(acquired ? Outcome::Chosen : decision.Received ? Outcome::ReceivedIneligible
            : Outcome::NoneInCone, acquired && continuous);
    }
}
