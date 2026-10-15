#include "Mods/Network/Gameplay/Weapons/HomingTargetSelection.hpp"

#include "Entities/EnemyInstanceEntity.hpp"
#include "Entities/PlatformEntity.hpp"
#include "Mods/Multiplayer/TeamLayout.hpp"
#include "Mods/Network/Protocol/Packets/ShotRayValidation.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace MphRead::Mods::Network
{
    namespace
    {
        using ::OpenTK::Mathematics::Vector3;
        using TargetKind = IntentPacket::ShotTargetKind;

        [[nodiscard]] bool HasFlag(std::uint32_t value, std::uint32_t flag) noexcept
        {
            return (value & flag) == flag;
        }

        [[nodiscard]] bool IsValidShooterAtFire(const HomingTargetSnapshot* shooterAtFire,
            std::int32_t shooterSlot) noexcept
        {
            return shooterAtFire != nullptr && shooterAtFire->Targetable
                && shooterAtFire->Reference.Kind == IntentPacket::ShotTargetKind::Player
                && shooterAtFire->OwnerSlot == shooterSlot;
        }

        [[nodiscard]] bool MatchesType(const HomingTargetSnapshot& candidate, bool requireReference) noexcept
        {
            if (!candidate.Reference.IsValid() || candidate.Reference.Kind == TargetKind::None)
            {
                return !requireReference && (candidate.Type == EntityType::Player
                    || candidate.Type == EntityType::Halfturret || candidate.Type == EntityType::EnemyInstance
                    || candidate.Type == EntityType::Door || candidate.Type == EntityType::Platform);
            }
            switch (candidate.Reference.Kind)
            {
            case TargetKind::Player:
                return candidate.Type == EntityType::Player;
            case TargetKind::Halfturret:
                return candidate.Type == EntityType::Halfturret;
            case TargetKind::SceneEntity:
                return candidate.Type == EntityType::Door || candidate.Type == EntityType::Platform;
            case TargetKind::EnemyInstance:
                return candidate.Type == EntityType::EnemyInstance;
            default:
                return false;
            }
        }

        [[nodiscard]] std::optional<float> AcquisitionCosine(const HomingTargetSnapshot& candidate,
            const HomingTargetSelectionRequest& request) noexcept
        {
            using ::OpenTK::Mathematics::Vector3;
            const float directionLengthSquared = Vector3::Dot(request.Direction, request.Direction);
            if (!(directionLengthSquared > 0.0F) || !std::isfinite(directionLengthSquared)
                || !std::isfinite(request.Range) || !std::isfinite(request.Tolerance))
            {
                return std::nullopt;
            }
            const Vector3 between = candidate.Position - request.Origin;
            const float distanceSquared = Vector3::Dot(between, between);
            if (!(distanceSquared > 0.0F) || !std::isfinite(distanceSquared))
            {
                return std::nullopt;
            }
            const bool ignoresRange = request.Continuous && request.Beam != BeamType::Platform;
            if (!ignoresRange && distanceSquared > request.Range * request.Range)
            {
                return std::nullopt;
            }
            const float distance = std::sqrt(distanceSquared);
            const Vector3 direction = ::OpenTK::Mathematics::Scale(
                request.Direction, 1.0F / std::sqrt(directionLengthSquared));
            const float cosine = Vector3::Dot(between, direction) / distance;
            if (cosine < request.Tolerance)
            {
                return std::nullopt;
            }
            if (request.Continuous)
            {
                if (candidate.Type == EntityType::Player && (candidate.AltForm || candidate.Morphing)
                    && cosine < 4006.0F / 4096.0F)
                {
                    return std::nullopt;
                }
                const float distanceRatio = request.Range > 0.0F
                    ? std::min(distance / request.Range, 1.0F) : 1.0F;
                const float threshold = request.Tolerance
                    + distanceRatio * (4094.0F / 4096.0F - request.Tolerance);
                if (cosine < threshold)
                {
                    return std::nullopt;
                }
            }
            return cosine;
        }
    }

    HomingFireTargetValidation HomingTargetSelection::ValidateFireRay(
        const HomingTargetSnapshot* shooterAtFire, std::int32_t shooterSlot,
        const OpenTK::Mathematics::Vector3& rayOrigin,
        const OpenTK::Mathematics::Vector3& rayDirection) noexcept
    {
        HomingFireTargetValidation result{};
        result.ShooterValid = IsValidShooterAtFire(shooterAtFire, shooterSlot);
        result.RayValid = result.ShooterValid && ShotRayValidation::IsPlausibleRay(
            rayOrigin, rayDirection, shooterAtFire->Position);
        return result;
    }

    HomingFireTargetValidation HomingTargetSelection::ValidateFireTarget(
        const HomingTargetSnapshot* shooterAtFire, std::int32_t shooterSlot,
        const HomingTargetSnapshot* targetAtFire, HomingTargetSelectionRequest request,
        bool hasRay, const OpenTK::Mathematics::Vector3& rayOrigin,
        const OpenTK::Mathematics::Vector3& rayDirection) noexcept
    {
        HomingFireTargetValidation result = hasRay
            ? ValidateFireRay(shooterAtFire, shooterSlot, rayOrigin, rayDirection)
            : HomingFireTargetValidation{};
        if (!hasRay)
        {
            result.ShooterValid = IsValidShooterAtFire(shooterAtFire, shooterSlot);
            result.RayValid = result.ShooterValid;
        }
        if (!result.ShooterValid || !result.RayValid || targetAtFire == nullptr)
        {
            return result;
        }
        request.ExcludedTarget = {};
        request.OwnerTeam = shooterAtFire->TeamIndex;
        if (!hasRay)
        {
            result.Accepted = IsEligible(*targetAtFire, request);
        }
        else
        {
            request.Origin = rayOrigin;
            request.Direction = rayDirection;
            result.Accepted = CanAcquire(*targetAtFire, request);
        }
        return result;
    }

    bool HomingTargetSelection::IsEligible(const HomingTargetSnapshot& candidate,
        const HomingTargetSelectionRequest& request) noexcept
    {
        const bool hasReference = candidate.Reference.IsValid()
            && candidate.Reference.Kind != IntentPacket::ShotTargetKind::None;
        const bool hasExcludedReference = request.ExcludedTarget.IsValid()
            && request.ExcludedTarget.Kind != IntentPacket::ShotTargetKind::None;
        if ((request.RequireReference && !hasReference)
            || (hasExcludedReference && candidate.Reference == request.ExcludedTarget)
            || !candidate.Targetable || !MatchesType(candidate, request.RequireReference))
        {
            return false;
        }
        if (candidate.Type == EntityType::Player)
        {
            return request.OwnerType != EntityType::Player
                || (candidate.OwnerSlot != request.OwnerSlot
                    && !Multiplayer::TeamRules::AreAllies(candidate.TeamIndex, request.OwnerTeam));
        }
        if (candidate.Type == EntityType::Halfturret)
        {
            if (request.OwnerSlot >= 0 && candidate.OwnerSlot == request.OwnerSlot)
            {
                return false;
            }
            return request.OwnerType != EntityType::Player
                || !Multiplayer::TeamRules::AreAllies(candidate.TeamIndex, request.OwnerTeam);
        }
        if (candidate.Type == EntityType::EnemyInstance)
        {
            if (request.OwnerType == EntityType::EnemyInstance || request.OwnerType == EntityType::Platform)
            {
                return false;
            }
            const auto has = [&candidate](Entities::EnemyFlags flag)
            {
                return HasFlag(candidate.Flags, static_cast<std::uint32_t>(flag));
            };
            return has(Entities::EnemyFlags::CollideBeam)
                && (!has(Entities::EnemyFlags::NoHomingNc) || request.Continuous)
                && (!has(Entities::EnemyFlags::NoHomingCo) || !request.Continuous);
        }
        if (candidate.Type == EntityType::Platform)
        {
            return HasFlag(candidate.Flags, static_cast<std::uint32_t>(Entities::PlatformFlags::BeamTarget));
        }
        return candidate.Type == EntityType::Door;
    }

    bool HomingTargetSelection::CanAcquire(const HomingTargetSnapshot& candidate,
        const HomingTargetSelectionRequest& request) noexcept
    {
        return IsEligible(candidate, request) && AcquisitionCosine(candidate, request).has_value();
    }

    IntentPacket::ShotTargetReference HomingTargetSelection::Select(
        std::span<const HomingTargetSnapshot> candidates,
        const HomingTargetSelectionRequest& request) noexcept
    {
        const std::optional<std::size_t> selected = SelectIndex(candidates, request);
        return selected.has_value() ? candidates[*selected].Reference : IntentPacket::ShotTargetReference{};
    }

    std::optional<std::size_t> HomingTargetSelection::SelectIndex(
        std::span<const HomingTargetSnapshot> candidates,
        const HomingTargetSelectionRequest& request) noexcept
    {
        float bestCosine = request.Tolerance;
        std::optional<std::size_t> selected{};
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const HomingTargetSnapshot& candidate = candidates[i];
            if (!IsEligible(candidate, request))
            {
                continue;
            }
            const std::optional<float> cosine = AcquisitionCosine(candidate, request);
            if (!cosine.has_value() || *cosine < bestCosine)
            {
                continue;
            }
            bestCosine = *cosine;
            selected = i;
        }
        return selected;
    }
}
