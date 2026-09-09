#pragma once

// The managed metadata catalogue is one partial static class spread across
// seven source files.  Native table implementations intentionally remain in
// their focused translation units; this facade restores the managed public
// surface without making a second, independently maintained catalogue.

#include "FrontendMeta.hpp"
#include "MetadataModels.hpp"
#include "MetadataValues.hpp"
#include "Rooms.hpp"
#include "entity_metadata.hpp"
#include "enemy_subroutines.hpp"
#include "enemy_values.hpp"
#include "metadata.hpp"
#include "metadata_lookup.hpp"
#include "metadata_values.hpp"
#include "player_metadata.hpp"
#include "sound_metadata.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::metadata {

class Metadata final {
public:
    Metadata() = delete;

    [[nodiscard]] static int GetMultiplayerEntityLayer(game::Mode mode,
                                                       int player_count) noexcept;
    [[nodiscard]] static std::string GetLayerName(int layer_id,
                                                  bool multiplayer);
    [[nodiscard]] static std::string GetLayerNames(int layer_mask,
                                                   bool multiplayer);

    [[nodiscard]] static const ModelMetadata* GetModelByName(
        std::string_view name, MetaDir dir = MetaDir::Models) noexcept;
    [[nodiscard]] static const ModelMetadata* GetFirstHuntModelByName(
        std::string_view name) noexcept;
    [[nodiscard]] static const ModelMetadata* GetEntityByPath(
        std::string_view path) noexcept;

    [[nodiscard]] static const ObjectMetadata& GetObjectById(int id);
    [[nodiscard]] static const ObjectMetadata& GetObjectById(
        std::uint32_t id);
    [[nodiscard]] static const PlatformMetadata* GetPlatformById(int id);

    [[nodiscard]] static formats::Vector3 GetEventColor(
        formats::Message event_id) noexcept;
    [[nodiscard]] static formats::Vector3 GetEventColor(
        formats::FhMessage event_id) noexcept;

    [[nodiscard]] static std::pair<const RoomMetadata*, int> GetRoomByName(
        std::string_view name);
    [[nodiscard]] static const RoomMetadata* GetRoomById(
        int id, bool no_throw = false);
    [[nodiscard]] static int GetAreaInfo(int room_id) noexcept;
    [[nodiscard]] static std::optional<std::string_view> GetEnemyModelName(
        formats::EnemyType type);
    [[nodiscard]] static int GetEnemyDeathEffect(formats::EnemyType type);
    [[nodiscard]] static float GetDamageMultiplier(Effectiveness value);
    static void LoadEffectiveness(formats::EnemyType type,
                                  std::span<Effectiveness> destination);
    static void LoadEffectiveness(std::int32_t value,
                                  std::span<Effectiveness> destination);
    static void LoadEffectiveness(std::uint32_t value,
                                  std::span<Effectiveness> destination);

    static void SetHunterSfxData(std::span<const std::uint8_t> data);
    static void SetBeamSfxData(std::span<const std::uint8_t> data);
    static void SetTerrainSfxData(std::span<const std::uint8_t> data);
    static void SetPlatformSfxData(std::span<const std::uint8_t> data);
    static void SetEnemyDamageSfxData(std::span<const std::uint8_t> data);
    static void SetEnemyDeathSfxData(std::span<const std::uint8_t> data);
    static void InstallSoundMetadata(const sound::SoundMetadata& value);

    static sound::SoundMetadata::PlatformTable PlatformSfx;
    static sound::SoundMetadata::HunterTable HunterSfx;
    static sound::SoundMetadata::BeamTable BeamSfx;
    static sound::SoundMetadata::TerrainTable TerrainSfx;
    static sound::SoundMetadata::EnemyTable EnemyDamageSfx;
    static sound::SoundMetadata::EnemyTable EnemyDeathSfx;

    static const std::vector<DoorMetadata>& Doors;
    static const PlatformMetadata InvisiblePlat;

    static inline const auto& FhDoors = fh_doors();
    static inline constexpr auto& DoorPalettes =
        fruityprime::metadata::DoorPalettes;
    static inline const auto& JumpPads = jump_pads();
    static inline const auto& Items = items();
    static inline const auto& FhItems = fh_items();
    static inline const auto& Effects = effects();
    static inline constexpr auto& RoomList = fruityprime::metadata::RoomList;
    static inline constexpr auto& EncounterNodeDataOverrides =
        fruityprime::metadata::EncounterNodeDataOverrides;
    static inline constexpr auto& CtfNodeDataOverrides =
        fruityprime::metadata::CtfNodeDataOverrides;

    static inline constexpr auto& ObjectVisPosOffsets =
        fruityprime::metadata::ObjectVisPosOffsets;
    static inline constexpr auto& WeaponNames =
        fruityprime::metadata::WeaponNames;
    static inline constexpr auto& WeaponNamesUpper =
        fruityprime::metadata::WeaponNamesUpper;
    static inline constexpr auto& WeaponMessageIds =
        fruityprime::metadata::WeaponMessageIds;
    static inline constexpr auto& BeamRadiusValues =
        fruityprime::metadata::BeamRadiusValues;
    static inline constexpr auto& SyluxBombEffects =
        fruityprime::metadata::SyluxBombEffects;
    static inline constexpr auto& TeamColors = fruityprime::metadata::TeamColors;
    static inline constexpr auto& MusicSeqs = fruityprime::metadata::MusicSeqs;
    static inline constexpr auto& SequenceFiles =
        fruityprime::metadata::SequenceFiles;
    static inline constexpr auto& DamageLevels =
        fruityprime::metadata::DamageLevels;
    static inline constexpr auto& AdpcmTable = fruityprime::metadata::AdpcmTable;
    static inline constexpr auto& BeamDrawEffects =
        fruityprime::metadata::BeamDrawEffects;
    static inline constexpr auto& EnemyModelNames =
        fruityprime::metadata::EnemyModelNames;
    static inline constexpr auto& EnemyDeathEffects =
        fruityprime::metadata::EnemyDeathEffects;
    static inline constexpr auto& EnemyAudioRangeIndices =
        fruityprime::metadata::EnemyAudioRangeIndices;
    static inline constexpr auto& EnemyScanIds =
        fruityprime::metadata::EnemyScanIds;
    static inline constexpr auto& DamageMultipliers =
        fruityprime::metadata::DamageMultipliers;
    static inline constexpr auto& EnemyEffectiveness =
        fruityprime::metadata::EnemyEffectiveness;
    static inline constexpr auto& SlenchEffectiveness =
        fruityprime::metadata::SlenchEffectiveness;
    static inline constexpr auto& SlenchSynapseEffectiveness =
        fruityprime::metadata::SlenchSynapseEffectiveness;
    static inline constexpr auto& GoreaEffectiveness =
        fruityprime::metadata::GoreaEffectiveness;
    static inline constexpr auto& Enemy24Colors =
        fruityprime::metadata::Enemy24Colors;

    static inline constexpr auto& Enemy00Subroutines =
        fruityprime::metadata::Enemy00Subroutines;
    static inline constexpr auto& Enemy02Subroutines =
        fruityprime::metadata::Enemy02Subroutines;
    static inline constexpr auto& Enemy03Subroutines =
        fruityprime::metadata::Enemy03Subroutines;
    static inline constexpr auto& Enemy04Subroutines =
        fruityprime::metadata::Enemy04Subroutines;
    static inline constexpr auto& Enemy05Subroutines =
        fruityprime::metadata::Enemy05Subroutines;
    static inline constexpr auto& Enemy06Subroutines =
        fruityprime::metadata::Enemy06Subroutines;
    static inline constexpr auto& Enemy10Subroutines =
        fruityprime::metadata::Enemy10Subroutines;
    static inline constexpr auto& Enemy11Subroutines =
        fruityprime::metadata::Enemy11Subroutines;
    static inline constexpr auto& Enemy16Subroutines =
        fruityprime::metadata::Enemy16Subroutines;
    static inline constexpr auto& Enemy18Subroutines =
        fruityprime::metadata::Enemy18Subroutines;
    static inline constexpr auto& Enemy19Subroutines =
        fruityprime::metadata::Enemy19Subroutines;
    static inline constexpr auto& Enemy23Subroutines =
        fruityprime::metadata::Enemy23Subroutines;
    static inline constexpr auto& Enemy24Subroutines =
        fruityprime::metadata::Enemy24Subroutines;
    static inline constexpr auto& Enemy28Subroutines =
        fruityprime::metadata::Enemy28Subroutines;
    static inline constexpr auto& Enemy31Subroutines =
        fruityprime::metadata::Enemy31Subroutines;
    static inline constexpr auto& Enemy33Subroutines =
        fruityprime::metadata::Enemy33Subroutines;
    static inline constexpr auto& Enemy35Subroutines =
        fruityprime::metadata::Enemy35Subroutines;
    static inline constexpr auto& Enemy36Subroutines =
        fruityprime::metadata::Enemy36Subroutines;
    static inline constexpr auto& Enemy38Subroutines =
        fruityprime::metadata::Enemy38Subroutines;
    static inline constexpr auto& Enemy39Subroutines =
        fruityprime::metadata::Enemy39Subroutines;
    static inline constexpr auto& Enemy45Subroutines =
        fruityprime::metadata::Enemy45Subroutines;
    static inline constexpr auto& Enemy46Subroutines =
        fruityprime::metadata::Enemy46Subroutines;
    static inline constexpr auto& Enemy47Subroutines =
        fruityprime::metadata::Enemy47Subroutines;

    static inline constexpr auto& Enemy10Values =
        fruityprime::metadata::Enemy10ValuesTable;
    static inline constexpr auto& Enemy18Values =
        fruityprime::metadata::Enemy18ValuesTable;
    static inline constexpr auto& Enemy19Values =
        fruityprime::metadata::Enemy19ValuesTable;
    static inline constexpr auto& Enemy23Values =
        fruityprime::metadata::Enemy23ValuesTable;
    static inline constexpr auto& Enemy36Values =
        fruityprime::metadata::Enemy36ValuesTable;
    static inline constexpr auto& Enemy39Values =
        fruityprime::metadata::Enemy39ValuesTable;
    static inline constexpr auto& Enemy41Values =
        fruityprime::metadata::Enemy41ValuesTable;
    static inline constexpr auto& Enemy44Values =
        fruityprime::metadata::Enemy44ValuesTable;
    static inline constexpr auto& Enemy45Values =
        fruityprime::metadata::Enemy45ValuesTable;

    static inline constexpr auto& SpireAltVectors =
        fruityprime::metadata::SpireAltVectors;
    static inline constexpr auto& SlipSpeedFactors =
        fruityprime::metadata::SlipSpeedFactors;
    static inline constexpr auto& TractionFactors =
        fruityprime::metadata::TractionFactors;
    static inline constexpr auto& MuzzleOffests =
        fruityprime::metadata::MuzzleOffests;
    static inline constexpr auto& MuzzleEffectIds =
        fruityprime::metadata::MuzzleEffectIds;
    static inline constexpr auto& ChargeEffectIds =
        fruityprime::metadata::ChargeEffectIds;
    static inline constexpr auto& ChargeLoopEffectIds =
        fruityprime::metadata::ChargeLoopEffectIds;
    static inline constexpr auto& GunAnimationIds =
        fruityprime::metadata::GunAnimationIds;
    static inline constexpr auto& PlayerValues =
        fruityprime::metadata::PlayerValues;

    static inline constexpr auto& NavMapModelNames =
        fruityprime::metadata::NavMapModelNames;
    static inline constexpr auto& MovieFiles = fruityprime::metadata::MovieFiles;
    static inline constexpr auto& MovieDisplayInfo =
        fruityprime::metadata::MovieDisplayInfo;
    static inline const auto& Ad2Dm2 = fruityprime::metadata::Ad2Dm2;
    static inline const auto& HudModels = fruityprime::metadata::HudModels;
    static inline const auto& TouchToStartModels =
        fruityprime::metadata::TouchToStartModels;
    static inline const auto& MultiplayerModels =
        fruityprime::metadata::MultiplayerModels;
    static inline const auto& LogoModels = fruityprime::metadata::LogoModels;
    static inline const auto& FrontendModels =
        fruityprime::metadata::FrontendModels;

    static inline constexpr auto EmissionOrange =
        fruityprime::metadata::EmissionOrange;
    static inline constexpr auto EmissionGreen =
        fruityprime::metadata::EmissionGreen;
    static inline constexpr auto EmissionGray =
        fruityprime::metadata::EmissionGray;
    static inline constexpr auto OctolithLight1Vector =
        fruityprime::metadata::OctolithLight1Vector;
    static inline constexpr auto OctolithLight2Vector =
        fruityprime::metadata::OctolithLight2Vector;
    static inline constexpr auto OctolithLightColor =
        fruityprime::metadata::OctolithLightColor;
    static inline constexpr auto RedPalette = fruityprime::metadata::RedPalette;
    static inline constexpr auto WhitePalette =
        fruityprime::metadata::WhitePalette;

    using SingleParticle = std::pair<std::string_view, std::string_view>;
    static inline constexpr std::array<
        std::pair<formats::SingleType, SingleParticle>, 12> SingleParticles{{
        {formats::SingleType::Death, {"deathParticle", "death"}},
        {formats::SingleType::Fuzzball, {"particles", "fuzzBall"}},
        {formats::SingleType::Lore, {"icons", "lore"}},
        {formats::SingleType::LoreDim, {"icons", "lore_dim"}},
        {formats::SingleType::Enemy, {"icons", "enemy"}},
        {formats::SingleType::EnemyDim, {"icons", "enemy_dim"}},
        {formats::SingleType::Object, {"icons", "object"}},
        {formats::SingleType::ObjectDim, {"icons", "object_dim"}},
        {formats::SingleType::Equipment, {"icons", "equipment"}},
        {formats::SingleType::EquipmentDim, {"icons", "equipment_dim"}},
        {formats::SingleType::Red, {"icons", "red"}},
        {formats::SingleType::RedDim, {"icons", "red_dim"}},
    }};
    static inline constexpr std::array<std::pair<std::string_view, bool>, 8>
        PreloadResources{{
            {"deathParticle", true}, {"particles", true},
            {"particles2", true}, {"TearParticle", true},
            {"icons", true}, {"iceWave", true}, {"sniperBeam", true},
            {"cylBossLaserBurn", true},
        }};

    static inline const auto& ToonTable = fruityprime::metadata::ToonTable;
    static inline const auto& PowerPalettes =
        fruityprime::metadata::PowerPalettes;
    static inline const auto& HunterScales =
        fruityprime::metadata::HunterScales;
    static inline const auto& HunterModels =
        fruityprime::metadata::HunterModels;
    static inline const auto& ImaIndexTable =
        fruityprime::metadata::ImaIndexTable;

    static inline const auto& DoubleDamageImg =
        fruityprime::metadata::DoubleDamageImg;
    static inline const auto& ModelMetadata =
        fruityprime::metadata::ModelMetadataTable;
    static inline const auto& FirstHuntModels =
        fruityprime::metadata::FirstHuntModels;
};

} // namespace fruityprime::metadata
