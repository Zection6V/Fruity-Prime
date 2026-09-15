#pragma once

#include "../Formats/Types.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace MphRead
{
    class CollisionPlane;

    namespace Entities
    {
        class CameraInfo;
        class PlayerEntity;
    }

    class SpectatorMode final
    {
    public:
        SpectatorMode() = delete;

        [[nodiscard]] static bool IsSpectator() noexcept;
        [[nodiscard]] static std::int32_t SpectatorTarget() noexcept;

        static void EnableSpectator() noexcept;
        static void CancelSpectator() noexcept;
        static void EndSpectating();
        [[nodiscard]] static bool TrySelectSpectatorTarget(std::int32_t slotIndex);
        static void UpdateSpectatorTarget();
        static void ClearPlayerPointers();
        static void ResetPlayerPointers();
        static void CheckForNextTarget(const std::shared_ptr<Entities::PlayerEntity>& player);
        [[nodiscard]] static std::shared_ptr<Entities::CameraInfo> GetCameraInfo(
            const std::shared_ptr<Entities::PlayerEntity>& player,
            std::shared_ptr<Entities::CameraInfo> cameraInfo);
        static void MoveReflectedCamera(const std::shared_ptr<Entities::PlayerEntity>& player);
        static void DrawSpectated(const std::shared_ptr<Entities::PlayerEntity>& player);
        static void DrawIceOverlay(const std::shared_ptr<Entities::PlayerEntity>& player);
        static void DrawHud();

    private:
        [[nodiscard]] static bool ShouldDrawPlayer(
            const std::shared_ptr<Entities::PlayerEntity>& player);
        static void DrawFusionBanned();

        static std::int32_t _spectatorEnabled;
        static std::int32_t _spectatorTarget;
        static std::shared_ptr<Entities::CameraInfo> _prevCameraInfo;
        static std::int32_t _prevPlayerTarget;

        static const OpenTK::Mathematics::Vector3 _fbPosition;
        static const OpenTK::Mathematics::Vector3 _fbFacing;
        static const OpenTK::Mathematics::Vector3 _fbUp;
        static const std::shared_ptr<const std::vector<CollisionPlane>> _fbCollision;
    };
}
