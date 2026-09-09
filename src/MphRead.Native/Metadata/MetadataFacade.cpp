#include "MetadataFacade.hpp"

#include <limits>
#include <stdexcept>

namespace fruityprime::metadata {
namespace {

[[nodiscard]] std::vector<int> animations(
    const std::array<std::int8_t, 4>& values) {
    return {values[0], values[1], values[2], values[3]};
}

[[nodiscard]] const std::vector<ObjectMetadata>& object_metadata() {
    static const std::vector<ObjectMetadata> result = [] {
        std::vector<ObjectMetadata> values;
        values.reserve(ObjectCount);
        for (const ObjectInfo& value : objects()) {
            std::optional<std::vector<int>> animation_ids;
            if (value.has_animation) {
                animation_ids = animations(value.animation_ids);
            }
            values.emplace_back(std::string(value.name), value.lighting,
                                value.recolor_id, value.ignore_animation,
                                std::move(animation_ids));
        }
        return values;
    }();
    return result;
}

[[nodiscard]] const std::vector<std::optional<PlatformMetadata>>&
platform_metadata() {
    static const std::vector<std::optional<PlatformMetadata>> result = [] {
        std::vector<std::optional<PlatformMetadata>> values;
        values.reserve(PlatformCount);
        for (const PlatformInfo& value : platforms()) {
            if (value.id == 1 || value.id == 2) {
                values.emplace_back(std::nullopt);
                continue;
            }
            std::optional<std::vector<int>> animation_ids;
            if (value.has_animation) {
                animation_ids = animations(value.animation_ids);
            }
            values.emplace_back(std::in_place, std::string(value.name),
                                value.lighting, std::move(animation_ids));
        }
        return values;
    }();
    return result;
}

[[nodiscard]] const std::vector<DoorMetadata>& door_metadata() {
    static const std::vector<DoorMetadata> result = [] {
        std::vector<DoorMetadata> values;
        values.reserve(DoorCount);
        for (const DoorInfo& value : doors()) {
            values.emplace_back(std::string(value.name),
                                std::string(value.lock_name), value.lock_offset,
                                value.radius);
        }
        return values;
    }();
    return result;
}

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> data,
                                     std::size_t offset) {
    if (offset + 2 > data.size()) {
        throw std::out_of_range("SFX data");
    }
    return static_cast<std::uint16_t>(data[offset])
        | static_cast<std::uint16_t>(data[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> data,
                                     std::size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::out_of_range("SFX data");
    }
    return static_cast<std::uint32_t>(data[offset])
        | static_cast<std::uint32_t>(data[offset + 1]) << 8
        | static_cast<std::uint32_t>(data[offset + 2]) << 16
        | static_cast<std::uint32_t>(data[offset + 3]) << 24;
}

template <typename Table>
void parse_sfx_data2(std::span<const std::uint8_t> data, Table& table) {
    constexpr std::size_t Rows = std::tuple_size_v<Table>;
    constexpr std::size_t Columns =
        std::tuple_size_v<typename Table::value_type>;
    if (data.size() != Rows * Columns * sizeof(std::uint16_t)) {
        throw std::invalid_argument("SFX data size");
    }
    for (std::size_t row = 0; row < Rows; ++row) {
        for (std::size_t column = 0; column < Columns; ++column) {
            const std::uint16_t value = read_u16(
                data, (row * Columns + column) * sizeof(std::uint16_t));
            table[row][column] = value == 0xffffU
                ? -1 : static_cast<std::int32_t>(value);
        }
    }
}

template <typename Table>
void parse_sfx_data4(std::span<const std::uint8_t> data, Table& table) {
    if (data.size() != table.size() * sizeof(std::uint32_t)) {
        throw std::invalid_argument("SFX data size");
    }
    for (std::size_t index = 0; index < table.size(); ++index) {
        const std::uint32_t value = read_u32(
            data, index * sizeof(std::uint32_t));
        table[index] = value == 0xffffffffU
            ? -1 : static_cast<std::int32_t>(value);
    }
}

} // namespace

const std::vector<DoorMetadata>& Metadata::Doors = door_metadata();
const PlatformMetadata Metadata::InvisiblePlat("N/A");
sound::SoundMetadata::PlatformTable Metadata::PlatformSfx{};
sound::SoundMetadata::HunterTable Metadata::HunterSfx{};
sound::SoundMetadata::BeamTable Metadata::BeamSfx{};
sound::SoundMetadata::TerrainTable Metadata::TerrainSfx{};
sound::SoundMetadata::EnemyTable Metadata::EnemyDamageSfx{};
sound::SoundMetadata::EnemyTable Metadata::EnemyDeathSfx{};

int Metadata::GetMultiplayerEntityLayer(game::Mode mode,
                                        int player_count) noexcept {
    return multiplayer_entity_layer(mode, player_count);
}

std::string Metadata::GetLayerName(int layer_id, bool multiplayer) {
    return layer_name(layer_id, multiplayer);
}

std::string Metadata::GetLayerNames(int layer_mask, bool multiplayer) {
    return layer_names(layer_mask, multiplayer);
}

const ModelMetadata* Metadata::GetModelByName(std::string_view name,
                                               MetaDir dir) noexcept {
    return get_model_by_name(name, dir);
}

const ModelMetadata* Metadata::GetFirstHuntModelByName(
    std::string_view name) noexcept {
    return get_first_hunt_model_by_name(name);
}

const ModelMetadata* Metadata::GetEntityByPath(std::string_view path) noexcept {
    return get_entity_by_path(path);
}

const ObjectMetadata& Metadata::GetObjectById(int id) {
    const auto& values = object_metadata();
    if (id < 0 || id > static_cast<int>(values.size())) {
        throw std::invalid_argument("id");
    }
    return values.at(static_cast<std::size_t>(id));
}

const ObjectMetadata& Metadata::GetObjectById(std::uint32_t id) {
    if (id > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("id");
    }
    return GetObjectById(static_cast<int>(id));
}

const PlatformMetadata* Metadata::GetPlatformById(int id) {
    const auto& values = platform_metadata();
    if (id < 0 || id > static_cast<int>(values.size())) {
        throw std::invalid_argument("id");
    }
    if (id == 1) {
        id = 0;
    }
    const auto& value = values.at(static_cast<std::size_t>(id));
    return value ? &*value : nullptr;
}

formats::Vector3 Metadata::GetEventColor(formats::Message event_id) noexcept {
    return event_color(event_id);
}

formats::Vector3 Metadata::GetEventColor(
    formats::FhMessage event_id) noexcept {
    return event_color(event_id);
}

std::pair<const RoomMetadata*, int> Metadata::GetRoomByName(
    std::string_view name) {
    return get_room_by_name(name);
}

const RoomMetadata* Metadata::GetRoomById(int id, bool no_throw) {
    return get_room_by_id(id, no_throw);
}

int Metadata::GetAreaInfo(int room_id) noexcept {
    return area_info(room_id);
}

std::optional<std::string_view> Metadata::GetEnemyModelName(
    formats::EnemyType type) {
    const int index = static_cast<int>(type);
    if (index < 0 || index > static_cast<int>(EnemyModelNames.size())) {
        throw std::out_of_range("EnemyModelNames");
    }
    const std::string_view value = EnemyModelNames.at(
        static_cast<std::size_t>(index));
    return value.empty() ? std::nullopt
                         : std::optional<std::string_view>(value);
}

int Metadata::GetEnemyDeathEffect(formats::EnemyType type) {
    const int index = static_cast<int>(type);
    if (index < 0 || index > static_cast<int>(EnemyDeathEffects.size())) {
        throw std::out_of_range("EnemyDeathEffects");
    }
    return EnemyDeathEffects.at(static_cast<std::size_t>(index));
}

float Metadata::GetDamageMultiplier(Effectiveness value) {
    const int index = static_cast<int>(value);
    if (index < 0 || index > static_cast<int>(DamageMultipliers.size())) {
        throw std::out_of_range("DamageMultipliers");
    }
    return DamageMultipliers.at(static_cast<std::size_t>(index));
}

void Metadata::LoadEffectiveness(formats::EnemyType type,
                                 std::span<Effectiveness> destination) {
    const int index = static_cast<int>(type);
    if (index < 0 || index > static_cast<int>(EnemyEffectiveness.size())) {
        throw std::out_of_range("EnemyEffectiveness");
    }
    LoadEffectiveness(EnemyEffectiveness.at(static_cast<std::size_t>(index)),
                      destination);
}

void Metadata::LoadEffectiveness(std::int32_t value,
                                 std::span<Effectiveness> destination) {
    LoadEffectiveness(static_cast<std::uint32_t>(value), destination);
}

void Metadata::LoadEffectiveness(std::uint32_t value,
                                 std::span<Effectiveness> destination) {
    if (destination.size() != 9) {
        throw std::invalid_argument("destination");
    }
    for (std::size_t index = 0; index < destination.size(); ++index) {
        destination[index] = static_cast<Effectiveness>(
            (value >> (index * 2)) & 3U);
    }
}

void Metadata::SetHunterSfxData(std::span<const std::uint8_t> data) {
    parse_sfx_data2(data, HunterSfx);
}

void Metadata::SetBeamSfxData(std::span<const std::uint8_t> data) {
    parse_sfx_data2(data, BeamSfx);
}

void Metadata::SetTerrainSfxData(std::span<const std::uint8_t> data) {
    parse_sfx_data2(data, TerrainSfx);
}

void Metadata::SetPlatformSfxData(std::span<const std::uint8_t> data) {
    parse_sfx_data2(data, PlatformSfx);
}

void Metadata::SetEnemyDamageSfxData(std::span<const std::uint8_t> data) {
    parse_sfx_data4(data, EnemyDamageSfx);
}

void Metadata::SetEnemyDeathSfxData(std::span<const std::uint8_t> data) {
    parse_sfx_data4(data, EnemyDeathSfx);
}

void Metadata::InstallSoundMetadata(const sound::SoundMetadata& value) {
    PlatformSfx = value.platform_sfx;
    HunterSfx = value.hunter_sfx;
    BeamSfx = value.beam_sfx;
    TerrainSfx = value.terrain_sfx;
    EnemyDamageSfx = value.enemy_damage_sfx;
    EnemyDeathSfx = value.enemy_death_sfx;
}

} // namespace fruityprime::metadata
