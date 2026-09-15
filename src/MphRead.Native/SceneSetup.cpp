#include "SceneSetup.hpp"

#include "Scene.hpp"
#include "Read.hpp"
#include "Formats/Entity.hpp"
#include "Formats/Formats.hpp"
#include "Formats/Types.hpp"
#include "Metadata/Metadata.hpp"
#include "Metadata/Rooms.hpp"
#include "Entities/AreaVolumeEntity.hpp"
#include "Entities/ArtifactEntity.hpp"
#include "Entities/BeamProjectileEntity.hpp"
#include "Entities/CamSeq/CamSeqEntity.hpp"
#include "Entities/DoorEntity.hpp"
#include "Entities/EnemySpawnEntity.hpp"
#include "Entities/EntityBase.hpp"
#include "Entities/FlagBaseEntity.hpp"
#include "Entities/ForceFieldEntity.hpp"
#include "Entities/ItemSpawnEntity.hpp"
#include "Entities/JumpPadEntity.hpp"
#include "Entities/LightSourceEntity.hpp"
#include "Entities/MorphCameraEntity.hpp"
#include "Entities/NodeDefenseEntity.hpp"
#include "Entities/ObjectEntity.hpp"
#include "Entities/OctolithFlagEntity.hpp"
#include "Entities/PlatformEntity.hpp"
#include "Entities/PlayerSpawnEntity.hpp"
#include "Entities/PointModuleEntity.hpp"
#include "Entities/RoomEntity.hpp"
#include "Entities/TeleporterEntity.hpp"
#include "Entities/TriggerVolumeEntity.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace MphRead
{
    enum class AreaState : std::int32_t;
}

namespace MphRead::SceneSetupInterop
{
    // Later-owner seams. These declarations deliberately carry no behavior;
    // SceneSetup keeps the ordering and branching owned by SceneSetup.cs.

    [[nodiscard]] GameMode GameModeNone();
    [[nodiscard]] GameMode GameModeSinglePlayer();
    [[nodiscard]] GameMode GameModeBattle();
    [[nodiscard]] GameMode GameModeBounty();
    [[nodiscard]] GameMode GameModeCapture();
    [[nodiscard]] GameMode GameModeNodes();
    [[nodiscard]] GameMode GameModeNodesTeams();
    [[nodiscard]] GameMode GameModeDefender();
    [[nodiscard]] GameMode GameModeDefenderTeams();
    [[nodiscard]] AreaState AreaStateClear();
    [[nodiscard]] std::int32_t NodeLayerMultiplayerU();
    [[nodiscard]] std::int32_t NodeLayerMultiplayerLod0();
    [[nodiscard]] std::int32_t NodeLayerMultiplayerLod1();
    [[nodiscard]] std::int32_t NodeLayerCaptureTheFlag();

    [[nodiscard]] GameMode GetGameMode();
    void SetGameMode(GameMode mode);
    [[nodiscard]] bool Multiplayer();
    [[nodiscard]] bool SinglePlayer();
    [[nodiscard]] StorySave* CurrentStorySave();
    [[nodiscard]] AreaState GetAreaState(std::int32_t areaId, StorySave* save = nullptr);
    [[nodiscard]] std::span<std::int32_t> EncounterState();
    [[nodiscard]] std::span<bool> CompletedRandomEncounterRooms();

    [[nodiscard]] std::uint32_t BossFlagsValue(const StorySave& save);
    [[nodiscard]] std::uint32_t LostOctoliths(const StorySave& save);
    [[nodiscard]] std::span<std::uint8_t> AreaHunters(StorySave& save);
    [[nodiscard]] std::uint8_t DefeatedHunters(const StorySave& save);
    [[nodiscard]] std::int32_t GetEnemyOctolithDrop(const StorySave& save, std::int32_t hunter);
    void SetCheckpointRoomId(StorySave& save, std::int32_t roomId);

    void SetSceneAreaId(Scene* scene, std::int32_t areaId);
    [[nodiscard]] std::int32_t SceneAreaId(const Scene* scene);
    [[nodiscard]] std::int32_t SceneRoomId(const Scene* scene);
    void SceneLoadModel(Scene* scene, const std::string& name);
    void SceneLoadEffect(Scene* scene, std::int32_t effectId, bool persistent);

    void ApplyAdventureSettings();
    [[nodiscard]] std::int32_t SaveSlot();
    [[nodiscard]] std::int32_t PreviousSaveSlot();
    void SetPreviousSaveSlot(std::int32_t value);
    void LoadRuntimeData();

    void SetWeaponsForRoom(bool multiplayerMetadata);
    void SetWeaponsForGameStateMultiplayer(bool multiplayer);

    [[nodiscard]] std::uint32_t Rng1StartValue();
    [[nodiscard]] std::uint32_t Rng2StartValue();
    void SetRng1(std::uint32_t value);
    void SetRng2(std::uint32_t value);
    [[nodiscard]] std::uint32_t GetRandomInt2(std::uint32_t max);

    void ClearCamSeqData();
    void SetCamSeqCurrentNull();
    void SetCameraSequenceCurrentNull();
    void SetCameraSequenceIntroNull();
    void LoadCameraSequenceIntro(std::int32_t sequenceId, Scene* scene);
    void LoadSfx(Scene* scene);
    void TryPlayRoomMusic(std::int32_t roomId, std::int32_t variant);
    void PlayEncounterMusic(Hunter hunter);
    void LoadAiPersonality(GameMode mode);

    [[nodiscard]] std::int32_t PlayerCount();
    void SetPlayerCount(std::int32_t count);
    void SetPlayersCreated(std::int32_t count);
    [[nodiscard]] std::int32_t MaxPlayers();
    [[nodiscard]] std::shared_ptr<Entities::PlayerEntity> PlayerAt(std::int32_t index);
    [[nodiscard]] std::shared_ptr<Entities::PlayerEntity> MainPlayer();
    [[nodiscard]] bool PlayerIsBot(const Entities::PlayerEntity& player);
    void SetPlayerIsBot(Entities::PlayerEntity& player, bool value);
    void ClearPlayerActiveFlags(Entities::PlayerEntity& player);
    void ClearPlayerSlotActive(Entities::PlayerEntity& player);
    void SetPlayerSlotActive(Entities::PlayerEntity& player);
    void SetPlayerBotLevel(Entities::PlayerEntity& player, std::int32_t level);
    void ResetAdventureModeBotWeapon(Entities::PlayerEntity& player);
    void SetEnemySpawner(Entities::PlayerEntity& player,
        const std::shared_ptr<Entities::EnemySpawnEntity>& spawner);
    [[nodiscard]] Hunter PlayerHunter(const Entities::PlayerEntity& player);
    [[nodiscard]] std::int32_t PlayerRecolor(const Entities::PlayerEntity& player);
    [[nodiscard]] bool PlayerHasWeapon(const Entities::PlayerEntity& player, BeamType weapon);
    void CreatePlayer(Hunter hunter, std::int32_t recolor);
    void SetPlayerInitialized(Entities::PlayerEntity& player, bool initialized);

    [[nodiscard]] bool NoRandomEncounters();
    [[nodiscard]] bool NoRepeatEncounters();
    [[nodiscard]] bool AlternateHunters1P();
    [[nodiscard]] bool MaxRoomDetail();
    [[nodiscard]] bool ThumbnailModeActive();

    [[nodiscard]] std::shared_ptr<Formats::Collision::CollisionInstance>
        GetCollision(const RoomMetadata* metadata, std::int32_t layerMask);
    void SetCollisionActive(Formats::Collision::CollisionInstance& collision, bool active);

    [[nodiscard]] std::string CombinePath(const std::string& root, const std::string& path);
    [[nodiscard]] const std::string& FileSystem();
    [[nodiscard]] const std::string& FhFileSystem();
    [[nodiscard]] bool IsMphJapan();
    [[nodiscard]] bool IsMphKorea();

    [[nodiscard]] std::shared_ptr<Formats::NodeData> ReadNodeData(
        const std::string& path, bool firstHunt);
    [[nodiscard]] bool NodeDataSimple(const Formats::NodeData& nodeData);
    [[nodiscard]] std::int32_t FindClosestNode(
        const Formats::NodeData& nodeData, OpenTK::Mathematics::Vector3 position, bool useMaxDist);
    void SetJumpPadClosestNode(Entities::JumpPadEntity& entity, std::int32_t node);
    void SetOctolithClosestNode(Entities::OctolithFlagEntity& entity, std::int32_t node);
    void SetOctolithBaseClosestNode(Entities::OctolithFlagEntity& entity, std::int32_t node);
    void SetFlagBaseClosestNode(Entities::FlagBaseEntity& entity, std::int32_t node);
    void SetNodeDefenseClosestNode(Entities::NodeDefenseEntity& entity, std::int32_t node);

    [[nodiscard]] std::pair<std::int32_t, std::vector<std::uint8_t>>
        ReadKanjiFont(bool singlePlayer);
    void SetKanjiFontData(std::vector<std::uint8_t> widths,
        std::vector<std::uint8_t> offsets, std::vector<std::uint8_t> charData,
        std::int32_t minChar);

    [[nodiscard]] std::int32_t MuzzleEffectId(std::int32_t index);
    [[nodiscard]] std::int32_t ChargeEffectId(std::int32_t index);
    [[nodiscard]] std::int32_t ChargeLoopEffectId(std::int32_t index);
    void LoadWeaponNames();
    void GeneratePlayerVolumes();
    void ReadStringTableCommon();
    void ReadStringTableSp();
    void ReadStringTableMp();
    void ReadStringTableScanLog();
    [[nodiscard]] std::span<const std::string> HunterModels(Hunter hunter);
    void GenerateKandenAltNodeDistances();
    [[nodiscard]] std::string SingleParticleModel(SingleType type);
    [[nodiscard]] std::string HudIconsModel();
    [[nodiscard]] std::optional<std::string> EnemyModelName(EnemyType type);
    [[nodiscard]] std::int32_t EnemyDeathEffect(EnemyType type);
    [[nodiscard]] std::string ObjectModelName(std::int32_t id);
    [[nodiscard]] std::size_t ItemModelCount();
    [[nodiscard]] const std::string& ItemModelName(std::size_t index);

    [[nodiscard]] bool EnemySpawnIsHunter(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] EnemyType EnemySpawnType(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] std::int32_t EnemySpawnerHealth(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] ItemType EnemySpawnerItemType(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] std::int32_t EnemySpawnerItemChance(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] std::int32_t EnemyHunterId(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] std::int32_t EnemyHunterChance(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] std::int32_t EnemyHunterColor(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] std::int32_t EnemyEncounterType(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] std::int32_t EnemySubtypeS06(const Entities::EnemySpawnEntity& spawner);
    [[nodiscard]] std::int32_t EnemySubtypeS07(const Entities::EnemySpawnEntity& spawner);

    [[nodiscard]] std::int32_t ObjectEffectId(const Entities::ObjectEntity& obj);
    [[nodiscard]] std::int32_t PlatformResistEffectId(const Entities::PlatformEntity& platform);
    [[nodiscard]] std::int32_t PlatformDamageEffectId(const Entities::PlatformEntity& platform);
    [[nodiscard]] std::int32_t PlatformDeadEffectId(const Entities::PlatformEntity& platform);
    [[nodiscard]] bool PlatformSamusShip(const Entities::PlatformEntity& platform);
    [[nodiscard]] bool PlatformBeamSpawner(const Entities::PlatformEntity& platform);
    [[nodiscard]] std::int32_t PlatformBeamId(const Entities::PlatformEntity& platform);
    [[nodiscard]] std::int32_t PlatformItemChance(const Entities::PlatformEntity& platform);
    [[nodiscard]] ItemType PlatformItemType(const Entities::PlatformEntity& platform);
    [[nodiscard]] ItemType ItemSpawnerType(const Entities::ItemSpawnEntity& itemSpawner);
    [[nodiscard]] std::int32_t ItemSpawnerHasBase(const Entities::ItemSpawnEntity& itemSpawner);

    void SetAreaVolumeActive(Entities::AreaVolumeEntity& entity, bool active);
    [[noreturn]] void ThrowProgramException(const std::string& message);
}

namespace MphRead
{
    namespace
    {
        using EntityList = std::shared_ptr<const std::vector<std::shared_ptr<Entities::EntityBase>>>;

        [[nodiscard]] bool IsMode(GameMode value, GameMode expected)
        {
            return static_cast<std::int32_t>(value) == static_cast<std::int32_t>(expected);
        }

        [[nodiscard]] OpenTK::Mathematics::Vector3 AddY(
            OpenTK::Mathematics::Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] OpenTK::Mathematics::Vector3 WithY(
            OpenTK::Mathematics::Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] constexpr OpenTK::Mathematics::Vector3 UnitX() noexcept
        {
            return {1.0F, 0.0F, 0.0F};
        }

        [[nodiscard]] constexpr OpenTK::Mathematics::Vector3 UnitY() noexcept
        {
            return {0.0F, 1.0F, 0.0F};
        }

        [[nodiscard]] constexpr OpenTK::Mathematics::Vector3 UnitZ() noexcept
        {
            return {0.0F, 0.0F, 1.0F};
        }

        [[nodiscard]] constexpr OpenTK::Mathematics::Vector3 NegativeUnitX() noexcept
        {
            return {-1.0F, 0.0F, 0.0F};
        }

        [[nodiscard]] constexpr OpenTK::Mathematics::Vector3 NegativeUnitZ() noexcept
        {
            return {0.0F, 0.0F, -1.0F};
        }
    }

    std::tuple<std::shared_ptr<Entities::RoomEntity>, const RoomMetadata*,
        std::shared_ptr<Formats::Collision::CollisionInstance>, EntityList>
        SceneSetup::LoadGame(const std::string& name, Scene* scene, std::int32_t playerCount,
            BossFlags bossFlags, std::int32_t nodeLayerMask, std::int32_t entityLayerId)
    {
        auto [metadata, roomId] = Metadata::GetRoomByName(name);
        SceneSetupInterop::SetSceneAreaId(scene, Metadata::GetAreaInfo(roomId));
        if (metadata == nullptr)
        {
            SceneSetupInterop::ThrowProgramException("No room with this name is known.");
        }

        GameMode mode = SceneSetupInterop::GetGameMode();
        if (IsMode(mode, SceneSetupInterop::GameModeNone()))
        {
            mode = metadata->Multiplayer
                ? SceneSetupInterop::GameModeBattle()
                : SceneSetupInterop::GameModeSinglePlayer();
            if (IsMode(mode, SceneSetupInterop::GameModeBattle())
                && metadata->Name == "AD1 TRANSFER LOCK BT")
            {
                mode = SceneSetupInterop::GameModeBounty();
            }
            SceneSetupInterop::SetWeaponsForRoom(metadata->Multiplayer);
        }
        else
        {
            SceneSetupInterop::SetWeaponsForGameStateMultiplayer(SceneSetupInterop::Multiplayer());
        }

        SceneSetupInterop::SetGameMode(mode);
        if (IsMode(mode, SceneSetupInterop::GameModeSinglePlayer()))
        {
            SceneSetupInterop::ApplyAdventureSettings();
        }

        SceneSetupInterop::LoadRuntimeData();
        LoadResources(scene);

        if (SceneSetupInterop::SaveSlot() != SceneSetupInterop::PreviousSaveSlot())
        {
            SceneSetupInterop::SetRng1(SceneSetupInterop::Rng1StartValue());
            SceneSetupInterop::SetRng2(SceneSetupInterop::Rng2StartValue());
            SceneSetupInterop::SetPreviousSaveSlot(SceneSetupInterop::SaveSlot());
        }

        SceneSetupInterop::ClearCamSeqData();
        SceneSetupInterop::SetCamSeqCurrentNull();
        SceneSetupInterop::SetCameraSequenceCurrentNull();
        SceneSetupInterop::SetCameraSequenceIntroNull();
        if (SceneSetupInterop::Multiplayer() && SceneSetupInterop::PlayerCount() > 0)
        {
            const std::int32_t seqId = roomId - 93 + 172;
            if (seqId >= 172 && seqId < 199)
            {
                SceneSetupInterop::LoadCameraSequenceIntro(seqId, scene);
            }
        }

        SceneSetupInterop::LoadSfx(scene);
        auto room = std::make_shared<Entities::RoomEntity>(scene);
        auto [collision, entities] = SetUpRoom(mode, playerCount, bossFlags,
            nodeLayerMask, entityLayerId, metadata, room, scene, false);

        StorySave* save = SceneSetupInterop::CurrentStorySave();
        const bool bossDone = SceneSetupInterop::SinglePlayer()
            && ((SceneSetupInterop::BossFlagsValue(*save)
                >> (2 * SceneSetupInterop::SceneAreaId(scene))) & 3U) != 0;
        SceneSetupInterop::TryPlayRoomMusic(room->RoomId(), bossDone ? 1 : 0);

        if (SceneSetupInterop::SinglePlayer())
        {
            UpdateAreaHunters();
            InitHunterSpawns(scene, entities, false);
            scene->LoadMapSymbolEntities(SceneSetupInterop::SceneAreaId(scene));
        }

        SceneSetupInterop::LoadAiPersonality(mode);
        room->SetNodeData(LoadNodeData(metadata->NodePath, room->RoomId(), mode,
            entities, metadata->FirstHunt));
        SceneSetupInterop::SetCheckpointRoomId(*save, room->RoomId());
        return {room, metadata, collision, entities};
    }

    std::shared_ptr<Formats::NodeData> SceneSetup::LoadNodeData(
        const std::optional<std::string>& requestedNodePath, std::int32_t roomId,
        GameMode mode, const EntityList& entities, bool firstHunt)
    {
        std::optional<std::string> nodePath = requestedNodePath;
        if (IsMode(mode, SceneSetupInterop::GameModeSinglePlayer()))
        {
            const std::int32_t count = SceneSetupInterop::PlayerCount();
            auto encounterState = SceneSetupInterop::EncounterState();
            for (std::int32_t i = 0; i < count; ++i)
            {
                const auto player = SceneSetupInterop::PlayerAt(i);
                const std::int32_t state = encounterState[static_cast<std::size_t>(i)];
                if (SceneSetupInterop::PlayerIsBot(*player) && state >= 1 && state <= 4)
                {
                    auto overridePath = Metadata::EncounterNodeDataOverrides.find(roomId);
                    if (overridePath != Metadata::EncounterNodeDataOverrides.end())
                    {
                        nodePath = overridePath->second;
                    }
                    break;
                }
            }
        }
        else if (IsMode(mode, SceneSetupInterop::GameModeCapture()))
        {
            auto overridePath = Metadata::CtfNodeDataOverrides.find(roomId);
            if (overridePath != Metadata::CtfNodeDataOverrides.end())
            {
                nodePath = overridePath->second;
            }
        }
        else if ((IsMode(mode, SceneSetupInterop::GameModeNodes())
            || IsMode(mode, SceneSetupInterop::GameModeNodesTeams())
            || IsMode(mode, SceneSetupInterop::GameModeDefender())
            || IsMode(mode, SceneSetupInterop::GameModeDefenderTeams())) && roomId == 107)
        {
            nodePath = R"(levels\nodeData\mp14_KOTH_node.bin)";
        }

        if (nodePath.has_value())
        {
            const std::string root = firstHunt
                ? SceneSetupInterop::FhFileSystem()
                : SceneSetupInterop::FileSystem();
            if (!std::filesystem::exists(SceneSetupInterop::CombinePath(root, *nodePath)))
            {
                std::cout << "[nodes] " << *nodePath
                    << " is missing; bots in this room will not navigate." << std::endl;
                nodePath.reset();
            }
        }

        std::shared_ptr<Formats::NodeData> nodeData;
        if (nodePath.has_value())
        {
            nodeData = SceneSetupInterop::ReadNodeData(
                SceneSetupInterop::CombinePath("", *nodePath), firstHunt);
            if (SceneSetupInterop::NodeDataSimple(*nodeData))
            {
                for (const auto& entity : *entities)
                {
                    if (entity->Type == EntityType::JumpPad)
                    {
                        auto jumpPad = std::static_pointer_cast<Entities::JumpPadEntity>(entity);
                        SceneSetupInterop::SetJumpPadClosestNode(*jumpPad,
                            SceneSetupInterop::FindClosestNode(*nodeData, jumpPad->Position, true));
                    }
                    else if (entity->Type == EntityType::OctolithFlag)
                    {
                        auto flag = std::static_pointer_cast<Entities::OctolithFlagEntity>(entity);
                        SceneSetupInterop::SetOctolithClosestNode(*flag,
                            SceneSetupInterop::FindClosestNode(*nodeData, flag->Position, false));
                        SceneSetupInterop::SetOctolithBaseClosestNode(*flag,
                            SceneSetupInterop::FindClosestNode(*nodeData, flag->BasePosition(), false));
                    }
                    else if (entity->Type == EntityType::FlagBase)
                    {
                        auto flagBase = std::static_pointer_cast<Entities::FlagBaseEntity>(entity);
                        SceneSetupInterop::SetFlagBaseClosestNode(*flagBase,
                            SceneSetupInterop::FindClosestNode(*nodeData, flagBase->Position, false));
                    }
                    else if (entity->Type == EntityType::NodeDefense)
                    {
                        auto defense = std::static_pointer_cast<Entities::NodeDefenseEntity>(entity);
                        SceneSetupInterop::SetNodeDefenseClosestNode(*defense,
                            SceneSetupInterop::FindClosestNode(*nodeData, defense->Position, false));
                    }
                }
            }
        }
        return nodeData;
    }

    void SceneSetup::UpdateAreaHunters(StorySave* save)
    {
        if (save == nullptr)
        {
            save = SceneSetupInterop::CurrentStorySave();
            auto completed = SceneSetupInterop::CompletedRandomEncounterRooms();
            std::fill(completed.begin(), completed.end(), false);
        }

        auto areaHunters = SceneSetupInterop::AreaHunters(*save);
        std::fill(areaHunters.begin(), areaHunters.end(), std::uint8_t{0});
        std::uint8_t chance = 0;
        std::array<std::uint8_t, 4> chances{};
        std::array<std::uint8_t, 4> counts{};

        for (std::int32_t i = 0; i < 4; ++i)
        {
            const std::int32_t area1 = i * 2;
            if (SceneSetupInterop::GetAreaState(area1, save) == SceneSetupInterop::AreaStateClear())
            {
                const std::uint32_t lostOctoliths = SceneSetupInterop::LostOctoliths(*save);
                if (((lostOctoliths >> (8 * i)) & 15U) == 15U
                    || ((lostOctoliths >> (4 * (2 * i + 1))) & 15U) == 15U)
                {
                    chance = static_cast<std::uint8_t>(chance + 2);
                }
                else
                {
                    chance = static_cast<std::uint8_t>(chance + 1);
                }
                chances[static_cast<std::size_t>(i)] = chance;
            }
        }

        for (std::int32_t i = 0; i < 8; ++i)
        {
            if ((SceneSetupInterop::DefeatedHunters(*save) & (1 << i)) == 0)
            {
                continue;
            }
            const std::uint32_t rand = SceneSetupInterop::GetRandomInt2(chance);
            for (std::int32_t j = 0; j < 4; ++j)
            {
                if (rand < chances[static_cast<std::size_t>(j)])
                {
                    areaHunters[static_cast<std::size_t>(j)]
                        |= static_cast<std::uint8_t>(1 << i);
                    counts[static_cast<std::size_t>(j)]
                        = static_cast<std::uint8_t>(counts[static_cast<std::size_t>(j)] + 1);
                    if (counts[static_cast<std::size_t>(j)] >= 3)
                    {
                        for (std::int32_t k = 3; k > j; --k)
                        {
                            chances[static_cast<std::size_t>(k)]
                                = chances[static_cast<std::size_t>(k - 1)];
                        }
                        chances[static_cast<std::size_t>(j)] = 0;
                        chance = chances[3];
                    }
                    break;
                }
            }
        }
    }

    void SceneSetup::InitHunterSpawns(Scene* scene, const EntityList& entities, bool initialize)
    {
        for (std::int32_t i = 1; i < SceneSetupInterop::MaxPlayers(); ++i)
        {
            auto player = SceneSetupInterop::PlayerAt(i);
            SceneSetupInterop::ClearPlayerActiveFlags(*player);
            SceneSetupInterop::ClearPlayerSlotActive(*player);
            SceneSetupInterop::SetPlayerIsBot(*player, false);
            SceneSetupInterop::SetPlayerBotLevel(*player, 0);
            SceneSetupInterop::ResetAdventureModeBotWeapon(*player);
        }

        SceneSetupInterop::SetPlayerCount(1);
        SceneSetupInterop::SetPlayersCreated(1);
        auto encounterState = SceneSetupInterop::EncounterState();
        std::fill(encounterState.begin(), encounterState.end(), 0);

        const std::int32_t areaId = SceneSetupInterop::SceneAreaId(scene);
        if (areaId >= 8)
        {
            return;
        }

        const auto mainPlayer = SceneSetupInterop::MainPlayer();
        if (SceneSetupInterop::GetAreaState(areaId) != SceneSetupInterop::AreaStateClear()
            || SceneSetupInterop::SceneRoomId(scene) != 50
            || SceneSetupInterop::PlayerHasWeapon(*mainPlayer, BeamType::Battlehammer))
        {
            StorySave* save = SceneSetupInterop::CurrentStorySave();
            auto areaHunters = SceneSetupInterop::AreaHunters(*save);
            std::int32_t randomHunters
                = areaHunters[static_cast<std::size_t>(areaId / 2)] & 0x7E;
            std::int32_t randomHunterCount
                = std::popcount(static_cast<std::uint32_t>(randomHunters));
            std::int32_t extraCount = 0;

            for (const auto& entity : *entities)
            {
                if (SceneSetupInterop::PlayerCount() >= SceneSetupInterop::MaxPlayers())
                {
                    break;
                }
                if (entity->Type != EntityType::EnemySpawn)
                {
                    continue;
                }
                auto spawner = std::static_pointer_cast<Entities::EnemySpawnEntity>(entity);
                if (!SceneSetupInterop::EnemySpawnIsHunter(*spawner))
                {
                    continue;
                }

                const std::int32_t hunterId = SceneSetupInterop::EnemyHunterId(*spawner);
                const std::int32_t roomId = SceneSetupInterop::SceneRoomId(scene);
                auto completed = SceneSetupInterop::CompletedRandomEncounterRooms();
                if (hunterId == 8
                    && (SceneSetupInterop::NoRandomEncounters()
                        || (SceneSetupInterop::NoRepeatEncounters()
                            && roomId >= 27 && roomId <= 92
                            && completed[static_cast<std::size_t>(roomId - 27)])))
                {
                    return;
                }
                if (SceneSetupInterop::GetRandomInt2(100)
                    >= static_cast<std::uint32_t>(SceneSetupInterop::EnemyHunterChance(*spawner)))
                {
                    continue;
                }

                auto player = SceneSetupInterop::PlayerAt(SceneSetupInterop::PlayerCount());
                SceneSetupInterop::SetPlayerIsBot(*player, true);
                SceneSetupInterop::SetEnemySpawner(*player, spawner);

                Hunter hunter{};
                if (hunterId == 8)
                {
                    const std::uint32_t rand = SceneSetupInterop::GetRandomInt2(
                        static_cast<std::uint32_t>(randomHunterCount + extraCount));
                    if (rand < static_cast<std::uint32_t>(randomHunterCount))
                    {
                        std::int32_t index = 0;
                        std::int32_t j = 0;
                        for (; j < 8; ++j)
                        {
                            if ((randomHunters & (1 << j)) != 0)
                            {
                                if (index++ == static_cast<std::int32_t>(rand))
                                {
                                    break;
                                }
                            }
                        }
                        hunter = static_cast<Hunter>(j);
                        if (hunter != Hunter::Samus && hunter != Hunter::Guardian)
                        {
                            SceneSetupInterop::PlayEncounterMusic(hunter);
                        }
                    }
                    else
                    {
                        hunter = Hunter::Guardian;
                    }
                }
                else
                {
                    hunter = static_cast<Hunter>(hunterId);
                }

                if (hunter != Hunter::Guardian)
                {
                    extraCount = 1;
                }
                if ((randomHunters & (1 << static_cast<std::int32_t>(hunter))) != 0)
                {
                    randomHunters &= ~(1 << static_cast<std::int32_t>(hunter));
                    --randomHunterCount;
                }

                std::int32_t suitColor = SceneSetupInterop::EnemyHunterColor(*spawner);
                if (hunter == SceneSetupInterop::PlayerHunter(*mainPlayer)
                    && suitColor == SceneSetupInterop::PlayerRecolor(*mainPlayer)
                    && SceneSetupInterop::AlternateHunters1P())
                {
                    suitColor = SceneSetupInterop::PlayerRecolor(*mainPlayer) == 0 ? 1 : 0;
                }

                SceneSetupInterop::CreatePlayer(hunter, suitColor);
                if (initialize)
                {
                    SceneSetupInterop::SetPlayerSlotActive(*player);
                    SceneSetupInterop::SetPlayerInitialized(*player, false);
                    scene->AddEntity(player);
                }
                SceneSetupInterop::EncounterState()[
                    static_cast<std::size_t>(SceneSetupInterop::PlayerCount())]
                    = SceneSetupInterop::EnemyEncounterType(*spawner);
                SceneSetupInterop::SetPlayerBotLevel(*player, 1);
                SceneSetupInterop::SetPlayerCount(SceneSetupInterop::PlayerCount() + 1);
            }
        }

        StorySave* save = SceneSetupInterop::CurrentStorySave();
        for (std::int32_t i = 1; i < SceneSetupInterop::MaxPlayers(); ++i)
        {
            auto player = SceneSetupInterop::PlayerAt(i);
            if (SceneSetupInterop::PlayerIsBot(*player))
            {
                const std::int32_t dropId = SceneSetupInterop::GetEnemyOctolithDrop(
                    *save, static_cast<std::int32_t>(SceneSetupInterop::PlayerHunter(*player)));
                if (dropId < 8)
                {
                    const EntityDataHeader header(
                        static_cast<std::uint16_t>(EntityType::Artifact), -1,
                        OpenTK::Mathematics::Vector3{}, UnitY(), UnitX());
                    const ArtifactEntityData data(header, 8,
                        static_cast<std::uint8_t>(dropId), 0, 0,
                        0, Message::None, 0, Message::None, 0, Message::None, -1);
                    auto artifact = std::make_shared<Entities::ArtifactEntity>(data, "", scene);
                    scene->AddEntity(artifact);
                }
            }
        }
    }

    std::pair<std::shared_ptr<Formats::Collision::CollisionInstance>, EntityList>
        SceneSetup::SetUpRoom(GameMode mode, std::int32_t playerCount, BossFlags bossFlags,
            std::int32_t nodeLayerMask, std::int32_t entityLayerId,
            const RoomMetadata* metadata, const std::shared_ptr<Entities::RoomEntity>& room,
            Scene* scene, bool isRoomTransition)
    {
        if (playerCount == 0)
        {
            playerCount = SceneSetupInterop::PlayerCount();
        }
        if (entityLayerId < 0 || entityLayerId > 15)
        {
            if (IsMode(mode, SceneSetupInterop::GameModeSinglePlayer()))
            {
                if (static_cast<std::uint32_t>(bossFlags) == 0xFFFFFFFFU)
                {
                    bossFlags = static_cast<BossFlags>(
                        SceneSetupInterop::BossFlagsValue(*SceneSetupInterop::CurrentStorySave()));
                }
                entityLayerId = (static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(bossFlags))
                    >> (2 * SceneSetupInterop::SceneAreaId(scene))) & 3;
            }
            else
            {
                entityLayerId = Metadata::GetMultiplayerEntityLayer(mode, playerCount);
            }
        }
        if (nodeLayerMask == 0)
        {
            const std::int32_t nodePlayerCount
                = SceneSetupInterop::MaxRoomDetail() ? 2 : playerCount;
            nodeLayerMask = GetNodeLayer(mode, metadata->NodeLayer, nodePlayerCount);
        }

        auto collision = SceneSetupInterop::GetCollision(metadata, nodeLayerMask);
        if (isRoomTransition)
        {
            SceneSetupInterop::SetCollisionActive(*collision, false);
        }
        room->Setup(metadata->Name, metadata, collision, nodeLayerMask, metadata->Id);
        EntityList entities = LoadEntities(metadata, entityLayerId, scene);
        entities = GetExtraEntities(room->RoomId(), entities, scene);
        return {collision, entities};
    }

    std::int32_t SceneSetup::GetNodeLayer(
        GameMode mode, std::int32_t roomLayer, std::int32_t playerCount)
    {
        std::int32_t nodeLayerMask = 0;
        if (IsMode(mode, SceneSetupInterop::GameModeSinglePlayer()))
        {
            if (roomLayer > 0)
            {
                nodeLayerMask = nodeLayerMask & 0xC03F
                    | (((1 << roomLayer) & 0xFF) << 6);
            }
        }
        else
        {
            nodeLayerMask |= SceneSetupInterop::NodeLayerMultiplayerU();
            if (playerCount <= 2)
            {
                nodeLayerMask |= SceneSetupInterop::NodeLayerMultiplayerLod0();
            }
            else
            {
                nodeLayerMask |= SceneSetupInterop::NodeLayerMultiplayerLod1();
            }
            if (IsMode(mode, SceneSetupInterop::GameModeCapture()))
            {
                nodeLayerMask |= SceneSetupInterop::NodeLayerCaptureTheFlag();
            }
        }
        return nodeLayerMask;
    }

    EntityList SceneSetup::LoadEntities(
        const RoomMetadata* metadata, std::int32_t layerId, Scene* scene)
    {
        auto results = std::make_shared<std::vector<std::shared_ptr<Entities::EntityBase>>>();
        if (!metadata->EntityPath.has_value())
        {
            return results;
        }

        auto entities = Read::GetEntities(*metadata->EntityPath, layerId,
            metadata->FirstHunt, true);
        for (const auto& entity : *entities)
        {
            const std::string& nodeName = *entity->NodeName;
            if (entity->Type == EntityType::Platform)
            {
                const auto raw = std::static_pointer_cast<EntityOf<PlatformEntityData>>(entity);
                results->push_back(std::make_shared<Entities::PlatformEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::FhPlatform)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FhPlatformEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FhPlatformEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::Object)
            {
                const auto raw = std::static_pointer_cast<EntityOf<ObjectEntityData>>(entity);
                results->push_back(std::make_shared<Entities::ObjectEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::PlayerSpawn || entity->Type == EntityType::FhPlayerSpawn)
            {
                const auto raw = std::static_pointer_cast<EntityOf<PlayerSpawnEntityData>>(entity);
                results->push_back(std::make_shared<Entities::PlayerSpawnEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::Door)
            {
                const auto raw = std::static_pointer_cast<EntityOf<DoorEntityData>>(entity);
                results->push_back(std::make_shared<Entities::DoorEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::FhDoor)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FhDoorEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FhDoorEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::ItemSpawn)
            {
                const auto raw = std::static_pointer_cast<EntityOf<ItemSpawnEntityData>>(entity);
                results->push_back(std::make_shared<Entities::ItemSpawnEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::FhItemSpawn)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FhItemSpawnEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FhItemSpawnEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::EnemySpawn)
            {
                const auto raw = std::static_pointer_cast<EntityOf<EnemySpawnEntityData>>(entity);
                results->push_back(std::make_shared<Entities::EnemySpawnEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::FhEnemySpawn)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FhEnemySpawnEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FhEnemySpawnEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::TriggerVolume)
            {
                const auto raw = std::static_pointer_cast<EntityOf<TriggerVolumeEntityData>>(entity);
                results->push_back(std::make_shared<Entities::TriggerVolumeEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::FhTriggerVolume)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FhTriggerVolumeEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FhTriggerVolumeEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::AreaVolume)
            {
                const auto raw = std::static_pointer_cast<EntityOf<AreaVolumeEntityData>>(entity);
                results->push_back(std::make_shared<Entities::AreaVolumeEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::FhAreaVolume)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FhAreaVolumeEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FhAreaVolumeEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::JumpPad)
            {
                const auto raw = std::static_pointer_cast<EntityOf<JumpPadEntityData>>(entity);
                results->push_back(std::make_shared<Entities::JumpPadEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::FhJumpPad)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FhJumpPadEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FhJumpPadEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::PointModule || entity->Type == EntityType::FhPointModule)
            {
                const auto raw = std::static_pointer_cast<EntityOf<PointModuleEntityData>>(entity);
                results->push_back(std::make_shared<Entities::PointModuleEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::MorphCamera)
            {
                const auto raw = std::static_pointer_cast<EntityOf<MorphCameraEntityData>>(entity);
                results->push_back(std::make_shared<Entities::MorphCameraEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::FhMorphCamera)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FhMorphCameraEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FhMorphCameraEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::OctolithFlag)
            {
                const auto raw = std::static_pointer_cast<EntityOf<OctolithFlagEntityData>>(entity);
                results->push_back(std::make_shared<Entities::OctolithFlagEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::FlagBase)
            {
                const auto raw = std::static_pointer_cast<EntityOf<FlagBaseEntityData>>(entity);
                results->push_back(std::make_shared<Entities::FlagBaseEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::Teleporter)
            {
                const auto raw = std::static_pointer_cast<EntityOf<TeleporterEntityData>>(entity);
                results->push_back(std::make_shared<Entities::TeleporterEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::NodeDefense)
            {
                const auto raw = std::static_pointer_cast<EntityOf<NodeDefenseEntityData>>(entity);
                results->push_back(std::make_shared<Entities::NodeDefenseEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::LightSource)
            {
                const auto raw = std::static_pointer_cast<EntityOf<LightSourceEntityData>>(entity);
                results->push_back(std::make_shared<Entities::LightSourceEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::Artifact)
            {
                const auto raw = std::static_pointer_cast<EntityOf<ArtifactEntityData>>(entity);
                results->push_back(std::make_shared<Entities::ArtifactEntity>(raw->Data, nodeName, scene));
            }
            else if (entity->Type == EntityType::CameraSequence)
            {
                const auto raw = std::static_pointer_cast<EntityOf<CameraSequenceEntityData>>(entity);
                results->push_back(std::make_shared<Entities::CamSeqEntity>(raw->Data, scene));
            }
            else if (entity->Type == EntityType::ForceField)
            {
                const auto raw = std::static_pointer_cast<EntityOf<ForceFieldEntityData>>(entity);
                results->push_back(std::make_shared<Entities::ForceFieldEntity>(raw->Data, nodeName, scene));
            }
            else
            {
                SceneSetupInterop::ThrowProgramException(
                    "Invalid entity type " + std::to_string(static_cast<std::uint16_t>(entity->Type)));
            }
        }
        return results;
    }

    void SceneSetup::LoadResources(Scene* scene)
    {
        if (scene != nullptr)
        {
            if (SceneSetupInterop::IsMphJapan() || SceneSetupInterop::IsMphKorea())
            {
                auto [count, charData] = SceneSetupInterop::ReadKanjiFont(
                    SceneSetupInterop::SinglePlayer());
                std::vector<std::uint8_t> widths(static_cast<std::size_t>(count));
                if (SceneSetupInterop::IsMphJapan())
                {
                    std::fill(widths.begin(), widths.end(), std::uint8_t{10});
                }
                else
                {
                    std::fill(widths.begin(), widths.end(), std::uint8_t{11});
                    widths[1] = 2;
                    widths[32] = 6;
                }
                std::vector<std::uint8_t> offsets(static_cast<std::size_t>(count));
                SceneSetupInterop::SetKanjiFontData(
                    std::move(widths), std::move(offsets), std::move(charData), 0);
            }

            LoadBombResources(scene);
            LoadBeamEffectResources(scene);
            LoadBeamProjectileResources(scene);
            LoadRoomResources(scene);
            LoadHunterResources(Hunter::Samus, scene);
            if (!SceneSetupInterop::ThumbnailModeActive())
            {
                LoadHunterResources(Hunter::Kanden, scene);
                LoadHunterResources(Hunter::Trace, scene);
                LoadHunterResources(Hunter::Sylux, scene);
                LoadHunterResources(Hunter::Noxus, scene);
                LoadHunterResources(Hunter::Spire, scene);
                LoadHunterResources(Hunter::Weavel, scene);
                LoadHunterResources(Hunter::Guardian, scene);
            }
            LoadCommonHunterResources(scene);
        }
    }

    void SceneSetup::LoadCommonHunterResources(Scene* scene)
    {
        for (const char* name : {"doubleDamage_img", "alt_ice", "gunSmoke",
            "trail", "octolith_simple", "Octolith"})
        {
            SceneSetupInterop::SceneLoadModel(scene, name);
        }
        for (std::int32_t id : {10, 216, 187, 188, 189})
        {
            SceneSetupInterop::SceneLoadEffect(scene, id, true);
        }
        for (std::int32_t i = 0; i < 9; ++i)
        {
            SceneSetupInterop::SceneLoadEffect(scene, SceneSetupInterop::MuzzleEffectId(i), true);
            SceneSetupInterop::SceneLoadEffect(scene, SceneSetupInterop::ChargeEffectId(i), true);
            SceneSetupInterop::SceneLoadEffect(scene, SceneSetupInterop::ChargeLoopEffectId(i), true);
        }
        SceneSetupInterop::LoadWeaponNames();
        SceneSetupInterop::GeneratePlayerVolumes();
        SceneSetupInterop::ReadStringTableCommon();
        SceneSetupInterop::ReadStringTableSp();
        SceneSetupInterop::ReadStringTableMp();
        if (SceneSetupInterop::SinglePlayer())
        {
            SceneSetupInterop::ReadStringTableScanLog();
        }
    }

    void SceneSetup::LoadHunterResources(Hunter hunter, Scene* scene)
    {
        SceneSetupInterop::SceneLoadModel(scene,
            hunter == Hunter::Noxus || hunter == Hunter::Trace ? "nox_ice" : "samus_ice");
        for (const std::string& modelName : SceneSetupInterop::HunterModels(hunter))
        {
            SceneSetupInterop::SceneLoadModel(scene, modelName);
        }
        if (hunter == Hunter::Samus)
        {
            SceneSetupInterop::SceneLoadEffect(scene, 30, true);
            SceneSetupInterop::SceneLoadEffect(scene, 136, true);
        }
        else if (hunter == Hunter::Kanden)
        {
            SceneSetupInterop::GenerateKandenAltNodeDistances();
        }
        else if (hunter == Hunter::Spire)
        {
            SceneSetupInterop::SceneLoadEffect(scene, 37, true);
        }
        else if (hunter == Hunter::Noxus)
        {
            SceneSetupInterop::SceneLoadEffect(scene, 235, true);
        }
    }

    void SceneSetup::LoadBombResources(Scene* scene)
    {
        for (const char* name : {"KandenAlt_TailBomb", "arcWelder", "arcWelder1"})
        {
            SceneSetupInterop::SceneLoadModel(scene, name);
        }
        for (std::int32_t id : {9, 113, 119, 128, 129, 145, 146,
            149, 150, 151, 152, 153})
        {
            SceneSetupInterop::SceneLoadEffect(scene, id, true);
        }
    }

    void SceneSetup::LoadBeamEffectResources(Scene* scene)
    {
        SceneSetupInterop::SceneLoadModel(scene, "iceWave");
        SceneSetupInterop::SceneLoadModel(scene, "sniperBeam");
        SceneSetupInterop::SceneLoadModel(scene, "cylBossLaserBurn");
    }

    void SceneSetup::LoadBeamProjectileResources(Scene* scene)
    {
        for (const char* name : {"iceShard", "energyBeam", "trail", "electroTrail", "arcWelder"})
        {
            SceneSetupInterop::SceneLoadModel(scene, name);
        }
        for (std::int32_t id : {57, 58, 59, 60, 61, 62, 63, 78, 85, 86,
            92, 98, 99, 100, 121, 122, 123, 124, 125, 126, 130, 134, 137,
            140, 141, 142, 171, 211, 237, 238, 246})
        {
            SceneSetupInterop::SceneLoadEffect(scene, id, true);
        }
    }

    void SceneSetup::LoadRoomResources(Scene* scene)
    {
        for (std::int32_t id : {1, 2, 5, 6, 7, 8, 11, 12, 13, 14, 15, 16,
            17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 31, 33, 99,
            115, 154, 155, 156, 157, 158, 159, 160, 161, 173, 190, 191,
            192, 231, 239})
        {
            SceneSetupInterop::SceneLoadEffect(scene, id, true);
        }
        SceneSetupInterop::SceneLoadModel(
            scene, SceneSetupInterop::SingleParticleModel(SingleType::Death));
        SceneSetupInterop::SceneLoadModel(
            scene, SceneSetupInterop::SingleParticleModel(SingleType::Fuzzball));
        if (SceneSetupInterop::SinglePlayer())
        {
            SceneSetupInterop::SceneLoadModel(scene, SceneSetupInterop::HudIconsModel());
        }
        SceneSetupInterop::SceneLoadEffect(scene, 209, true);
        SceneSetupInterop::SceneLoadEffect(scene, 245, true);
    }

    void SceneSetup::LoadEntityResources(
        const std::shared_ptr<Entities::EntityBase>& entity, Scene* scene)
    {
        if (auto obj = std::dynamic_pointer_cast<Entities::ObjectEntity>(entity))
        {
            LoadObjectResources(obj, scene);
        }
        else if (auto platform = std::dynamic_pointer_cast<Entities::PlatformEntity>(entity))
        {
            LoadPlatformResources(platform, scene);
        }
        else if (auto spawner = std::dynamic_pointer_cast<Entities::EnemySpawnEntity>(entity))
        {
            LoadEnemyResources(spawner, scene);
        }
        else if (auto itemSpawner = std::dynamic_pointer_cast<Entities::ItemSpawnEntity>(entity))
        {
            LoadItemResources(itemSpawner, scene);
        }
    }

    void SceneSetup::LoadObjectResources(Scene* scene)
    {
        for (const auto& obj : scene->GetObjectEntities())
        {
            LoadObjectResources(obj, scene);
        }
    }

    void SceneSetup::LoadObjectResources(
        const std::shared_ptr<Entities::ObjectEntity>& obj, Scene* scene)
    {
        const std::int32_t effectId = SceneSetupInterop::ObjectEffectId(*obj);
        if (effectId != 0)
        {
            SceneSetupInterop::SceneLoadEffect(scene, effectId, false);
        }
    }

    void SceneSetup::LoadPlatformResources(Scene* scene)
    {
        for (const auto& platform : scene->GetPlatformEntities())
        {
            LoadPlatformResources(platform, scene);
        }
    }

    void SceneSetup::LoadPlatformResources(
        const std::shared_ptr<Entities::PlatformEntity>& platform, Scene* scene)
    {
        const std::array<std::int32_t, 3> effects{
            SceneSetupInterop::PlatformResistEffectId(*platform),
            SceneSetupInterop::PlatformDamageEffectId(*platform),
            SceneSetupInterop::PlatformDeadEffectId(*platform)
        };
        for (std::int32_t effectId : effects)
        {
            if (effectId != 0)
            {
                SceneSetupInterop::SceneLoadEffect(scene, effectId, false);
            }
        }
        if (SceneSetupInterop::PlatformSamusShip(*platform))
        {
            SceneSetupInterop::SceneLoadEffect(scene, 182, false);
        }
        if (SceneSetupInterop::PlatformBeamSpawner(*platform)
            && SceneSetupInterop::PlatformBeamId(*platform) == 0)
        {
            SceneSetupInterop::SceneLoadEffect(scene, 183, false);
            SceneSetupInterop::SceneLoadEffect(scene, 184, false);
            SceneSetupInterop::SceneLoadEffect(scene, 185, false);
        }
        if (SceneSetupInterop::PlatformItemChance(*platform) > 0)
        {
            LoadItem(SceneSetupInterop::PlatformItemType(*platform), scene);
        }
    }

    void SceneSetup::LoadEnemyResources(Scene* scene)
    {
        for (const auto& spawner : scene->GetEnemySpawnEntities())
        {
            LoadEnemyResources(spawner, scene);
        }
    }

    void SceneSetup::LoadEnemyResources(
        const std::shared_ptr<Entities::EnemySpawnEntity>& spawner, Scene* scene)
    {
        const EnemyType type = SceneSetupInterop::EnemySpawnType(*spawner);
        LoadEnemy(type, scene);
        if (SceneSetupInterop::EnemySpawnerHealth(*spawner) > 0)
        {
            SceneSetupInterop::SceneLoadModel(scene,
                type == EnemyType::WarWasp || type == EnemyType::BarbedWarWasp
                    ? "PlantCarnivarous_Pod" : "EnemySpawner");
        }
        if (SceneSetupInterop::EnemySpawnerItemChance(*spawner) > 0)
        {
            LoadItem(SceneSetupInterop::EnemySpawnerItemType(*spawner), scene);
        }

        switch (type)
        {
        case EnemyType::Cretaphid:
            LoadEnemy(EnemyType::CretaphidEye, scene);
            for (std::int32_t id : {65, 66, 67, 73, 74, 116, 117, 138, 139})
                SceneSetupInterop::SceneLoadEffect(scene, id, false);
            LoadItem(ItemType::HealthMedium, scene);
            LoadItem(ItemType::UASmall, scene);
            LoadItem(ItemType::MissileSmall, scene);
            break;
        case EnemyType::Gorea1A:
            LoadEnemy(EnemyType::Gorea1B, scene);
            for (std::int32_t id : {48, 46, 49, 47, 50, 41, 42, 43, 44, 45,
                54, 51, 55, 53, 56, 52, 71, 72, 104, 148, 175, 179, 180})
                SceneSetupInterop::SceneLoadEffect(scene, id, false);
            LoadItem(ItemType::HealthBig, scene);
            LoadItem(ItemType::UABig, scene);
            LoadItem(ItemType::MissileBig, scene);
            break;
        case EnemyType::Trocra:
            SceneSetupInterop::SceneLoadEffect(scene, 164, false);
            SceneSetupInterop::SceneLoadEffect(scene, 75, false);
            LoadItem(ItemType::HealthSmall, scene);
            LoadItem(ItemType::UASmall, scene);
            LoadItem(ItemType::MissileSmall, scene);
            break;
        case EnemyType::Gorea2:
            LoadEnemy(EnemyType::GoreaMeteor, scene);
            for (std::int32_t id : {104, 224, 79, 176, 177, 178, 80, 225, 44, 72, 174, 210})
                SceneSetupInterop::SceneLoadEffect(scene, id, false);
            LoadItem(ItemType::HealthSmall, scene);
            LoadItem(ItemType::UASmall, scene);
            LoadItem(ItemType::MissileSmall, scene);
            break;
        case EnemyType::Slench:
            LoadEnemy(EnemyType::SlenchNest, scene);
            LoadEnemy(EnemyType::SlenchSynapse, scene);
            for (std::int32_t id : {64, 81, 68, 82, 70, 69, 83, 109, 135,
                201, 202, 203, 204, 205, 206})
                SceneSetupInterop::SceneLoadEffect(scene, id, false);
            LoadItem(ItemType::HealthMedium, scene);
            LoadItem(ItemType::UASmall, scene);
            LoadItem(ItemType::MissileSmall, scene);
            break;
        case EnemyType::Blastcap:
            SceneSetupInterop::SceneLoadEffect(scene, 3, false);
            SceneSetupInterop::SceneLoadEffect(scene, 4, false);
            break;
        case EnemyType::PsychoBit1:
            SceneSetupInterop::SceneLoadEffect(scene, 240, false);
            break;
        case EnemyType::AlimbicTurret:
            SceneSetupInterop::SceneLoadEffect(scene, 207, false);
            SceneSetupInterop::SceneLoadEffect(scene, 208, false);
            break;
        case EnemyType::FireSpawn:
            if (SceneSetupInterop::EnemySubtypeS06(*spawner) == 1)
            {
                for (std::int32_t id : {96, 132, 133, 131, 217})
                    SceneSetupInterop::SceneLoadEffect(scene, id, false);
            }
            else
            {
                for (std::int32_t id : {94, 95, 93, 110, 218})
                    SceneSetupInterop::SceneLoadEffect(scene, id, false);
            }
            break;
        case EnemyType::GreaterIthrak:
            for (std::int32_t id : {102, 101, 103})
                SceneSetupInterop::SceneLoadEffect(scene, id, false);
            break;
        case EnemyType::Shriekbat:
            SceneSetupInterop::SceneLoadEffect(scene, 29, false);
            SceneSetupInterop::SceneLoadEffect(scene, 108, false);
            break;
        case EnemyType::CarnivorousPlant:
            SceneSetupInterop::SceneLoadModel(
                scene, SceneSetupInterop::ObjectModelName(
                    SceneSetupInterop::EnemySubtypeS07(*spawner)));
            break;
        default:
            break;
        }
    }

    void SceneSetup::LoadEnemy(EnemyType enemy, Scene* scene)
    {
        if (enemy == EnemyType::SlenchSynapse)
        {
            const std::int32_t roomId = SceneSetupInterop::SceneRoomId(scene);
            if (roomId == 76)
                SceneSetupInterop::SceneLoadModel(scene, "BigEyeSynapse_04");
            else if (roomId == 64)
                SceneSetupInterop::SceneLoadModel(scene, "BigEyeSynapse_03");
            else if (roomId == 82)
                SceneSetupInterop::SceneLoadModel(scene, "BigEyeSynapse_02");
            else
                SceneSetupInterop::SceneLoadModel(scene, "BigEyeSynapse_01");
        }
        else
        {
            if (auto model = SceneSetupInterop::EnemyModelName(enemy); model.has_value())
            {
                SceneSetupInterop::SceneLoadModel(scene, *model);
            }
            if (enemy == EnemyType::Gorea1A)
            {
                SceneSetupInterop::SceneLoadModel(scene, "Gorea1B_lod0");
                SceneSetupInterop::SceneLoadModel(scene, "goreaArmRegen");
                SceneSetupInterop::SceneLoadModel(scene, "goreaMindTrick");
                SceneSetupInterop::SceneLoadModel(scene, "goreaMindTrick");
            }
            else if (enemy == EnemyType::Gorea2)
            {
                SceneSetupInterop::SceneLoadModel(scene, "goreaMeteor");
                SceneSetupInterop::SceneLoadModel(scene, "goreaLaser");
                SceneSetupInterop::SceneLoadModel(scene, "goreaLaserColl");
            }
        }

        const std::int32_t effectId = SceneSetupInterop::EnemyDeathEffect(enemy);
        if (effectId > 0)
        {
            SceneSetupInterop::SceneLoadEffect(scene, effectId, false);
        }
    }

    void SceneSetup::LoadItemResources(Scene* scene)
    {
        if (SceneSetupInterop::Multiplayer())
        {
            LoadItem(ItemType::UASmall, scene);
            LoadItem(ItemType::UABig, scene);
            LoadItem(ItemType::MissileSmall, scene);
            LoadItem(ItemType::MissileBig, scene);
        }
        for (const auto& itemSpawner : scene->GetItemSpawnEntities())
        {
            LoadItemResources(itemSpawner, scene);
        }
    }

    void SceneSetup::LoadItemResources(
        const std::shared_ptr<Entities::ItemSpawnEntity>& itemSpawner, Scene* scene)
    {
        LoadItem(SceneSetupInterop::ItemSpawnerType(*itemSpawner), scene);
        if (SceneSetupInterop::ItemSpawnerHasBase(*itemSpawner) != 0)
        {
            SceneSetupInterop::SceneLoadModel(scene, "items_base");
        }
    }

    void SceneSetup::LoadItem(ItemType item, Scene* scene)
    {
        if (item == ItemType::None)
        {
            return;
        }
        const std::int32_t index = static_cast<std::int32_t>(item);
        assert(index < static_cast<std::int32_t>(SceneSetupInterop::ItemModelCount()));
        SceneSetupInterop::SceneLoadModel(
            scene, SceneSetupInterop::ItemModelName(static_cast<std::size_t>(index)));
        if (item == ItemType::ArtifactKey)
            SceneSetupInterop::SceneLoadEffect(scene, 144, false);
        else if (item == ItemType::Deathalt)
            SceneSetupInterop::SceneLoadEffect(scene, 181, true);
        else if (item == ItemType::OmegaCannon)
        {
            SceneSetupInterop::SceneLoadEffect(scene, 209, true);
            SceneSetupInterop::SceneLoadEffect(scene, 245, true);
        }
        else if (item == ItemType::DoubleDamage)
            SceneSetupInterop::SceneLoadEffect(scene, 244, true);
    }

    std::shared_ptr<std::vector<std::shared_ptr<Entities::BeamProjectileEntity>>>
        SceneSetup::CreateBeamList(std::int32_t size, Scene* scene)
    {
        assert(size > 0);
        auto beams = std::make_shared<std::vector<std::shared_ptr<Entities::BeamProjectileEntity>>>();
        beams->reserve(static_cast<std::size_t>(size));
        for (std::int32_t i = 0; i < size; ++i)
        {
            beams->push_back(std::make_shared<Entities::BeamProjectileEntity>(scene));
        }
        return beams;
    }

    EntityList SceneSetup::GetExtraEntities(
        std::int32_t roomId, const EntityList& entities, Scene* scene)
    {
        const auto mainPlayer = SceneSetupInterop::MainPlayer();
        const Hunter hunter = SceneSetupInterop::PlayerHunter(*mainPlayer);
        if (!SceneSetupInterop::SinglePlayer() || hunter == Hunter::Samus
            || !SceneSetupInterop::AlternateHunters1P())
        {
            return entities;
        }

        const auto getEntity = [&](EntityType type, std::int32_t id)
            -> std::shared_ptr<Entities::EntityBase>
        {
            for (const auto& entity : *entities)
            {
                if (entity->Type == type && entity->Id == id)
                {
                    return entity;
                }
            }
            SceneSetupInterop::ThrowProgramException("Could not find entity to update.");
        };

        std::int16_t nextId = 30000;
        const auto createJumpPad =
            [&](const std::string& nodeName, OpenTK::Mathematics::Vector3 position,
                float speed, float radius = 1.0F, float height = 1.0F,
                float offset = 0.0F, std::uint16_t frames = 20,
                Entities::TriggerFlags flags = Entities::TriggerFlags::PlayerAlt)
                -> std::shared_ptr<Entities::JumpPadEntity>
        {
            const Fixed radiusFx(Fixed::ToInt(radius));
            const Fixed heightFx(Fixed::ToInt(height));
            const EntityDataHeader header(
                static_cast<std::uint16_t>(EntityType::JumpPad), nextId++,
                position, UnitY(), UnitZ());
            const Vector3Fx cylPos(0, Fixed::ToInt(offset), 0);
            const RawCollisionVolume volume(TypeExtensions::ToVector3Fx(UnitY()), cylPos, radiusFx, heightFx);
            const JumpPadEntityData data(header, -1, volume, TypeExtensions::ToVector3Fx(UnitY()),
                Fixed(Fixed::ToInt(speed)), 0, frames, 1, 0, 0, flags);
            return std::make_shared<Entities::JumpPadEntity>(data, nodeName, scene);
        };

        const auto createTeleporter =
            [&](OpenTK::Mathematics::Vector3 position,
                OpenTK::Mathematics::Vector3 targetPos,
                OpenTK::Mathematics::Vector3 facing,
                const std::string& nodeName, const std::string& targetNode)
                -> std::shared_ptr<Entities::TeleporterEntity>
        {
            const EntityDataHeader header(
                static_cast<std::uint16_t>(EntityType::Teleporter), nextId++,
                position, UnitY(), facing);
            const TeleporterEntityData data(header, 0, 0, 8, 1, 1,
                std::nullopt, TypeExtensions::ToVector3Fx(targetPos), targetNode);
            return std::make_shared<Entities::TeleporterEntity>(
                data, nodeName, scene, true);
        };

        auto list = std::make_shared<std::vector<std::shared_ptr<Entities::EntityBase>>>(
            entities->begin(), entities->end());

        if (roomId == 27 && (hunter == Hunter::Noxus || hunter == Hunter::Trace))
        {
            getEntity(EntityType::ItemSpawn, 13)->Position
                = OpenTK::Mathematics::Vector3(-60.0F, 6.5F, 33.6F);
        }
        else if (roomId == 28 && (hunter == Hunter::Noxus || hunter == Hunter::Trace))
        {
            list->push_back(createJumpPad("rmC0B", {-14.3F, 0.0F, -8.9F}, 0.25F, 0.75F));
        }
        else if (roomId == 67 && (hunter == Hunter::Noxus || hunter == Hunter::Spire
            || hunter == Hunter::Trace || hunter == Hunter::Weavel))
        {
            if (hunter != Hunter::Weavel)
                list->push_back(createJumpPad("rmMain", {8.7F, -2.0F, 12.4F}, 0.3F, 0.5F));
            if (hunter == Hunter::Noxus || hunter == Hunter::Trace || hunter == Hunter::Weavel)
                list->push_back(createJumpPad("rmMain", {8.7F, 3.4F, 0.0F}, 0.5F, 0.5F));
        }
        else if (roomId == 79 && (hunter == Hunter::Noxus || hunter == Hunter::Trace))
        {
            auto entity = getEntity(EntityType::ItemSpawn, 29);
            entity->Position = AddY(entity->Position, -1.5F);
        }
        else if (roomId == 78 && hunter == Hunter::Noxus)
        {
            list->push_back(createJumpPad("rmChamberE", {18.5F, 33.5F, -38.6F}, 0.3F, 0.4F, 0.0F, 35));
            list->push_back(createJumpPad("rmChamberE", {16.4F, 35.5F, -38.6F}, 0.3F, 0.4F, 0.0F, 35));
            list->push_back(createJumpPad("rmChamberE", {19.1F, 40.5F, -38.6F}, 0.3F, 0.4F, 0.0F, 35));
        }
        else if (roomId == 78 && hunter == Hunter::Weavel)
        {
            list->push_back(createJumpPad("rmChamberE", {18.5F, 33.5F, -38.6F}, 0.3F, 0.4F, 0.0F, 35));
        }
        else if (roomId == 78 && (hunter == Hunter::Trace || hunter == Hunter::Sylux))
        {
            auto position = OpenTK::Mathematics::Vector3(18.5F, 33.0F, -38.6F);
            auto targetPos = OpenTK::Mathematics::Vector3(16.8F, 35.5F, -38.6F);
            list->push_back(createTeleporter(position, targetPos, NegativeUnitZ(), "rmChamberE", "rmChamberE"));
            if (hunter == Hunter::Trace)
            {
                position = {19.1F, 40.0F, -38.6F};
                targetPos = {17.4F, 43.3F, -22.5F};
                list->push_back(createTeleporter(position, targetPos, NegativeUnitZ(), "rmChamberE", "rmChamberE"));
            }
        }
        else if (roomId == 80)
        {
            if (hunter == Hunter::Noxus || hunter == Hunter::Spire)
                list->push_back(createJumpPad("rmC0b", {8.0F, 0.0F, 12.1F}, 0.5F, 0.3F, 0.0F, 60));
            else if (hunter == Hunter::Kanden)
            {
                auto entity = getEntity(EntityType::ItemSpawn, 31);
                entity->Position = AddY(entity->Position, -0.7F);
            }
            else if (hunter == Hunter::Weavel)
            {
                auto entity = getEntity(EntityType::ItemSpawn, 31);
                entity->Position = AddY(entity->Position, -2.0F);
            }
            else if (hunter == Hunter::Trace)
            {
                auto entity = getEntity(EntityType::ItemSpawn, 31);
                entity->Position = WithY(entity->Position, 0.0F);
            }
        }
        else if (roomId == 30)
        {
            if (hunter == Hunter::Noxus || hunter == Hunter::Spire
                || hunter == Hunter::Trace || hunter == Hunter::Weavel)
            {
                list->push_back(createJumpPad(
                    "rmLava", {0.6F, -26.5F, 4.8F}, 0.15F, 0.5F, 1.0F, 2.2F));
                list->push_back(createJumpPad("rmLava", {0.6F, 3.2F, -14.6F}, 0.6F, 0.5F));
                if (hunter != Hunter::Spire)
                {
                    list->push_back(createJumpPad("rmLava", {0.6F, -21.6F, -6.8F}, 0.5F, 0.5F));
                    list->push_back(createJumpPad("rmLava", {0.6F, 73.5F, -9.4F}, 0.5F, 0.5F));
                    list->push_back(createJumpPad("rmLava", {0.6F, 94.2F, -15.5F}, 0.5F, 0.5F));
                }
                else
                {
                    SceneSetupInterop::SetAreaVolumeActive(
                        *std::static_pointer_cast<Entities::AreaVolumeEntity>(
                            getEntity(EntityType::AreaVolume, 4)), false);
                }
            }
            if (hunter == Hunter::Trace || hunter == Hunter::Sylux)
            {
                auto position = OpenTK::Mathematics::Vector3(0.59F, -27.63F, -11.91F);
                auto targetPos = OpenTK::Mathematics::Vector3(0.59F, -20.63F, -6.81F);
                list->push_back(createTeleporter(position, NegativeUnitX(), targetPos, "rmLava", "rmLava"));
                position = {0.59F, -23.03F, -10.61F};
                targetPos = {0.59F, -28.43F, -10.61F};
                list->push_back(createTeleporter(position, NegativeUnitX(), targetPos, "rmLava", "rmLava"));
            }
        }
        else if (roomId == 41 && hunter == Hunter::Spire)
        {
            for (std::int32_t id : {3, 11, 12, 13, 16})
            {
                SceneSetupInterop::SetAreaVolumeActive(
                    *std::static_pointer_cast<Entities::AreaVolumeEntity>(
                        getEntity(EntityType::AreaVolume, id)), false);
            }
        }
        else if (roomId == 38 && (hunter == Hunter::Noxus || hunter == Hunter::Trace))
        {
            list->push_back(createJumpPad("rmMain", {-1.4F, 0.3F, 29.1F}, 0.3F, 0.4F));
            list->push_back(createJumpPad("rmMain", {29.1F, 0.3F, 1.4F}, 0.3F, 0.4F));
            list->push_back(createJumpPad("rmend", {-10.8F, 9.6F, -28.3F}, 0.3F, 0.4F));
            list->push_back(createJumpPad("rmend", {-19.6F, 12.0F, -28.3F}, 0.3F, 0.4F));
            list->push_back(createJumpPad("rmend", {-39.9F, 17.0F, -24.5F}, 0.5F, 0.4F));
        }
        else if (roomId == 38 && hunter == Hunter::Weavel)
        {
            list->push_back(createJumpPad("rmend", {-10.8F, 9.6F, -28.3F}, 0.3F, 0.4F));
            list->push_back(createJumpPad("rmend", {-19.6F, 12.0F, -28.3F}, 0.3F, 0.4F));
        }
        return list;
    }
}
