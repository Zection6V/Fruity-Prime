#pragma once

#include "Formats/Enums.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace MphRead
{
    class RoomMetadata;
    class Scene;
    class StorySave;
    enum class BossFlags : std::uint32_t;
    enum class GameMode : std::int32_t;

    namespace Formats
    {
        class NodeData;
        namespace Collision
        {
            class CollisionInstance;
        }
    }

    namespace Entities
    {
        class BeamProjectileEntity;
        class EnemySpawnEntity;
        class EntityBase;
        class ItemSpawnEntity;
        class ObjectEntity;
        class PlatformEntity;
        class RoomEntity;
    }

    class SceneSetup final
    {
    public:
        SceneSetup() = delete;
        SceneSetup(const SceneSetup&) = delete;
        SceneSetup(SceneSetup&&) = delete;
        SceneSetup& operator=(const SceneSetup&) = delete;
        SceneSetup& operator=(SceneSetup&&) = delete;

        [[nodiscard]] static std::tuple<
            std::shared_ptr<Entities::RoomEntity>,
            const RoomMetadata*,
            std::shared_ptr<Formats::Collision::CollisionInstance>,
            std::shared_ptr<const std::vector<std::shared_ptr<Entities::EntityBase>>>>
            LoadGame(const std::string& name, Scene* scene, std::int32_t playerCount = 0,
                BossFlags bossFlags = static_cast<BossFlags>(-1),
                std::int32_t nodeLayerMask = 0, std::int32_t entityLayerId = -1);

        [[nodiscard]] static std::shared_ptr<Formats::NodeData> LoadNodeData(
            const std::optional<std::string>& nodePath, std::int32_t roomId, GameMode mode,
            const std::shared_ptr<const std::vector<std::shared_ptr<Entities::EntityBase>>>& entities,
            bool firstHunt);

        static void UpdateAreaHunters(StorySave* save = nullptr);
        static void InitHunterSpawns(Scene* scene,
            const std::shared_ptr<const std::vector<std::shared_ptr<Entities::EntityBase>>>& entities,
            bool initialize);

        [[nodiscard]] static std::pair<
            std::shared_ptr<Formats::Collision::CollisionInstance>,
            std::shared_ptr<const std::vector<std::shared_ptr<Entities::EntityBase>>>>
            SetUpRoom(GameMode mode, std::int32_t playerCount, BossFlags bossFlags,
                std::int32_t nodeLayerMask, std::int32_t entityLayerId,
                const RoomMetadata* metadata, const std::shared_ptr<Entities::RoomEntity>& room,
                Scene* scene, bool isRoomTransition);

        [[nodiscard]] static std::int32_t GetNodeLayer(
            GameMode mode, std::int32_t roomLayer, std::int32_t playerCount);

        static void LoadCommonHunterResources(Scene* scene);
        static void LoadHunterResources(Hunter hunter, Scene* scene);
        static void LoadEntityResources(const std::shared_ptr<Entities::EntityBase>& entity, Scene* scene);
        static void LoadObjectResources(Scene* scene);
        static void LoadObjectResources(const std::shared_ptr<Entities::ObjectEntity>& obj, Scene* scene);
        static void LoadPlatformResources(Scene* scene);
        static void LoadPlatformResources(const std::shared_ptr<Entities::PlatformEntity>& platform, Scene* scene);
        static void LoadEnemyResources(Scene* scene);
        static void LoadEnemyResources(const std::shared_ptr<Entities::EnemySpawnEntity>& spawner, Scene* scene);
        static void LoadItemResources(Scene* scene);
        static void LoadItemResources(const std::shared_ptr<Entities::ItemSpawnEntity>& itemSpawner, Scene* scene);

        [[nodiscard]] static std::shared_ptr<std::vector<std::shared_ptr<Entities::BeamProjectileEntity>>>
            CreateBeamList(std::int32_t size, Scene* scene);

    private:
        [[nodiscard]] static std::shared_ptr<const std::vector<std::shared_ptr<Entities::EntityBase>>>
            LoadEntities(const RoomMetadata* metadata, std::int32_t layerId, Scene* scene);
        static void LoadResources(Scene* scene);
        static void LoadBombResources(Scene* scene);
        static void LoadBeamEffectResources(Scene* scene);
        static void LoadBeamProjectileResources(Scene* scene);
        static void LoadRoomResources(Scene* scene);
        static void LoadEnemy(EnemyType enemy, Scene* scene);
        static void LoadItem(ItemType item, Scene* scene);

        [[nodiscard]] static std::shared_ptr<const std::vector<std::shared_ptr<Entities::EntityBase>>>
            GetExtraEntities(std::int32_t roomId,
                const std::shared_ptr<const std::vector<std::shared_ptr<Entities::EntityBase>>>& entities,
                Scene* scene);
    };
}
