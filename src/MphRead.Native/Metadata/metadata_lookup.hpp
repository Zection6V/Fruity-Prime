#pragma once

// Native counterpart of the lookup helpers in Metadata/Metadata.cs and
// Metadata/Weapons.cs.  These read the tables the rest of the metadata port
// already carries; keeping them beside those tables means a caller never has
// to reimplement the entity-layer arithmetic or the editor's event palette.

#include "Formats/enum_tables.hpp"
#include "Metadata/entity_metadata.hpp"
#include "GameState.hpp"
#include "Metadata/room_metadata.hpp"
#include "Formats/Types.hpp"
#include "Metadata/weapon_metadata.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace fruityprime::metadata {

// Metadata._modeLayers: the entity layer ids each multiplayer mode uses.  A
// mode with three entries picks by player count; one with a single entry
// always uses it.
[[nodiscard]] std::span<const int> mode_layers(game::Mode mode) noexcept;

// Metadata.GetMultiplayerEntityLayer
[[nodiscard]] int multiplayer_entity_layer(game::Mode mode,
                                           int player_count) noexcept;

// Metadata.GetLayerName
[[nodiscard]] std::string layer_name(int layer_id, bool multiplayer);

// Metadata.GetLayerNames
[[nodiscard]] std::string layer_names(int layer_mask, bool multiplayer);

// Metadata.GetEventColor: the editor's colour for a message, used to draw
// trigger and area volumes.
[[nodiscard]] formats::Vector3 event_color(formats::Message event_id) noexcept;
[[nodiscard]] formats::Vector3 event_color(
    formats::FhMessage event_id) noexcept;

// Metadata.GetRoomById
[[nodiscard]] const RoomMetadata* room_by_id(int id) noexcept;

// Weapons.GetAffinityBeam
[[nodiscard]] formats::BeamType affinity_beam(formats::Hunter hunter) noexcept;

// Weapons.GetAmmo: which ammo pool a beam draws from, or -1 for none.
[[nodiscard]] int weapon_ammo_type(formats::BeamType beam) noexcept;

// Enemies.GetEnemyModelName: empty when the enemy has no model of its own.
[[nodiscard]] std::string_view enemy_model_name(
    formats::EnemyType type) noexcept;

// Enemies.GetEnemyDeathEffect
[[nodiscard]] int enemy_death_effect(formats::EnemyType type) noexcept;

// Metadata.DoubleDamageImg / InvisiblePlat: two singleton records the game
// looks up by name rather than from a table.
[[nodiscard]] std::string_view double_damage_img_name() noexcept;
[[nodiscard]] std::string_view invisible_plat_name() noexcept;

// Metadata.GetModelByName / GetFirstHuntModelByName / GetObjectById /
// GetEntityByPath.  The native tree already resolves each of these; these
// wrappers carry the managed names so both trees grep alike.
//
// GetModelByName is assets::try_load_named_model, which owns the `_Model` /
// `_model` / `_mdl_Model` spelling variants.  First Hunt models live under
// the same resolver with the First Hunt asset root selected on the store.
[[nodiscard]] std::string model_path_by_name(std::string_view name);
[[nodiscard]] std::string first_hunt_model_path_by_name(std::string_view name);

// Metadata.GetObjectById is entity_metadata's object_info.
[[nodiscard]] const ObjectInfo* object_by_id(std::uint32_t id) noexcept;

// Metadata.GetEntityByPath: an entity file is named for its room, under
// levels/entities.
[[nodiscard]] std::string entity_path_for(std::string_view filename);

} // namespace fruityprime::metadata
