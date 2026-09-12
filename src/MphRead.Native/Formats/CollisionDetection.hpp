#pragma once

#include "Collision.hpp"
#include "Types.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

namespace MphRead
{
    class CollisionVolume;
    class Scene;
}

namespace MphRead::Formats
{
    class CollisionCandidate
    {
    public:
        std::shared_ptr<MphRead::Formats::Collision::EntityCollision> EntityCollision{};
        std::shared_ptr<MphRead::Formats::Collision::CollisionInstance> Collision{};
        MphRead::Formats::Collision::CollisionEntry Entry{};

        CollisionCandidate(
            std::shared_ptr<MphRead::Formats::Collision::CollisionInstance> collision,
            MphRead::Formats::Collision::CollisionEntry entry);
    };

    enum class TestFlags : std::int32_t
    {
        None = 0x0,
        Players = 0x2000,
        Beams = 0x4000,
        Scan = 0x8000
    };

    struct CollisionResult
    {
        std::uint8_t Field0 = 0;
        MphRead::Formats::Collision::CollisionFlags Flags
            = MphRead::Formats::Collision::CollisionFlags::None;
        OpenTK::Mathematics::Vector4 Plane{};
        float Field14 = 0.0F;
        OpenTK::Mathematics::Vector3 Position{};
        float Distance = 0.0F;
        std::shared_ptr<MphRead::Formats::Collision::EntityCollision> EntityCollision{};
        OpenTK::Mathematics::Vector3 EdgePoint1{};
        OpenTK::Mathematics::Vector3 EdgePoint2{};

        [[nodiscard]] std::int32_t Slipperiness() const noexcept;
        [[nodiscard]] MphRead::Terrain Terrain() const noexcept;
    };

    class CollisionDetection final
    {
    public:
        CollisionDetection() = delete;

        static void Init();

        [[nodiscard]] static bool CheckBetweenPoints(
            const std::vector<std::shared_ptr<CollisionCandidate>>* candidates,
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            TestFlags flags,
            MphRead::Scene* scene,
            CollisionResult& result);

        [[nodiscard]] static bool CheckBetweenPoints(
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            TestFlags flags,
            MphRead::Scene* scene,
            CollisionResult& result);

        [[nodiscard]] static std::int32_t CheckSphereBetweenPoints(
            const std::vector<std::shared_ptr<CollisionCandidate>>* candidates,
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            float radius,
            std::int32_t limit,
            bool includeOffset,
            TestFlags flags,
            MphRead::Scene* scene,
            MphRead::ManagedArray<CollisionResult>* results);

        [[nodiscard]] static std::int32_t CheckSphereBetweenPoints(
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            float radius,
            std::int32_t limit,
            bool includeOffset,
            TestFlags flags,
            MphRead::Scene* scene,
            MphRead::ManagedArray<CollisionResult>* results);

        [[nodiscard]] static bool CheckCylinderBetweenPoints(
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            OpenTK::Mathematics::Vector3 cylPos,
            float cylHeight,
            float radii,
            CollisionResult& result);

        [[nodiscard]] static std::int32_t CheckInRadius(
            OpenTK::Mathematics::Vector3 point,
            float radius,
            std::int32_t limit,
            bool getSimpleNormal,
            TestFlags flags,
            MphRead::Scene* scene,
            MphRead::ManagedArray<CollisionResult>* results);

        [[nodiscard]] static const std::vector<std::shared_ptr<CollisionCandidate>>&
        GetCandidatesForLimits(
            std::optional<OpenTK::Mathematics::Vector3> point1,
            OpenTK::Mathematics::Vector3 point2,
            float margin,
            std::optional<OpenTK::Mathematics::Vector3> limitMin,
            OpenTK::Mathematics::Vector3 limitMax,
            bool includeEntities,
            MphRead::Scene* scene);

        [[nodiscard]] static const std::vector<std::shared_ptr<CollisionCandidate>>&
        GetCandidatesForPoints(
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            float margin,
            bool includeEntities,
            MphRead::Scene* scene);

        [[nodiscard]] static bool CheckSphereOverlapVolume(
            const MphRead::CollisionVolume* other,
            OpenTK::Mathematics::Vector3 position,
            float radius,
            CollisionResult& result);

        [[nodiscard]] static bool CheckCylinderOverlapVolume(
            const MphRead::CollisionVolume* other,
            OpenTK::Mathematics::Vector3 cylinderBottom,
            OpenTK::Mathematics::Vector3 cylinderTop,
            float radius,
            CollisionResult& result);

        [[nodiscard]] static bool CheckVolumesOverlap(
            const MphRead::CollisionVolume* one,
            const MphRead::CollisionVolume* two,
            CollisionResult& result);

        [[nodiscard]] static bool CheckCylinderOverlapSphere(
            OpenTK::Mathematics::Vector3 cylinderBottom,
            OpenTK::Mathematics::Vector3 cylinderTop,
            OpenTK::Mathematics::Vector3 spherePosition,
            float radii,
            CollisionResult& result);

        [[nodiscard]] static bool CheckCylinderIntersectPlane(
            OpenTK::Mathematics::Vector3 cylinderBottom,
            OpenTK::Mathematics::Vector3 cylinderTop,
            OpenTK::Mathematics::Vector4 plane,
            CollisionResult& result);

        [[nodiscard]] static bool CheckPortBetweenPoints(
            const MphRead::Formats::Collision::Portal* portal,
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            bool otherSide);

    private:
        [[nodiscard]] static bool CheckBetweenPoints(
            const std::vector<std::shared_ptr<CollisionCandidate>>* candidates,
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            TestFlags flags,
            MphRead::Scene* scene,
            CollisionResult& result,
            bool hasCandidates);

        [[nodiscard]] static bool CheckPointOnFace(
            OpenTK::Mathematics::Vector3 point,
            const MphRead::Formats::Collision::MphCollisionInfo& info,
            MphRead::Formats::Collision::CollisionData data);

        static void ClearCandidates();

        [[nodiscard]] static std::int32_t CheckSphereBetweenPoints(
            const std::vector<std::shared_ptr<CollisionCandidate>>* candidates,
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            float radius,
            std::int32_t limit,
            bool includeOffset,
            TestFlags flags,
            MphRead::Scene* scene,
            MphRead::ManagedArray<CollisionResult>* results,
            bool hasCandidates);

        static void GetRoomCandidatesForLimits(
            OpenTK::Mathematics::Vector3 limitMin,
            OpenTK::Mathematics::Vector3 limitMax,
            MphRead::Scene* scene);

        static void GetEntityCandidates(
            OpenTK::Mathematics::Vector3 limitMin,
            OpenTK::Mathematics::Vector3 limitMax,
            MphRead::Scene* scene);

        static void GetRoomCandidatesForPoints(
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            MphRead::Scene* scene);

        [[nodiscard]] static bool CheckCylindersOverlap(
            OpenTK::Mathematics::Vector3 oneBottom,
            OpenTK::Mathematics::Vector3 oneTop,
            OpenTK::Mathematics::Vector3 twoBottom,
            OpenTK::Mathematics::Vector3 twoVector,
            float twoDot,
            float radii,
            CollisionResult& result);

        [[nodiscard]] static bool CheckCylinderOverlapVolumeHelper(
            const MphRead::CollisionVolume* other,
            OpenTK::Mathematics::Vector3 cylinderBottom,
            OpenTK::Mathematics::Vector3 cylinderVector,
            float cylinderDot,
            float radius,
            CollisionResult& result);

        static std::vector<std::shared_ptr<CollisionCandidate>> _activeItems;
        static std::deque<std::shared_ptr<CollisionCandidate>> _inactiveItems;
        static std::vector<std::shared_ptr<CollisionCandidate>> _tempItems;
        static std::vector<MphRead::Formats::Collision::CollisionData> _seenData;
    };
}
