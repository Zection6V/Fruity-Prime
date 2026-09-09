#include "Formats/editor_entity.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string_view>

namespace {

const fruityprime::editor::FieldValue& find_field(
    const std::vector<fruityprime::editor::FieldValue>& fields,
    std::string_view name) {
    const auto it = std::find_if(fields.begin(), fields.end(),
        [name](const auto& field) { return field.name == name; });
    assert(it != fields.end());
    return *it;
}

bool has_difference(const std::vector<fruityprime::editor::Difference>& values,
                    std::string_view name) {
    return std::find_if(values.begin(), values.end(),
        [name](const auto& value) { return value.name == name; })
        != values.end();
}

} // namespace

int main() {
    using namespace fruityprime;

    scene::EntityInstance standard;
    standard.type = 0;
    standard.kind = scene::EntityKind::Platform;
    standard.entity_id = 17;
    standard.layer_mask = 0x1234;
    standard.node_name = "PLAT_A";
    standard.position = {4096, -2048, 8192};
    standard.up_vector = {0, 4096, 0};
    standard.facing_vector = {0, 0, -4096};

    scene::PlatformData platform;
    platform.no_port = 2;
    platform.position_count = 1;
    platform.positions[0] = {1.0F, 2.0F, 3.0F};
    platform.rotations[0] = {0.0F, 0.0F, 0.0F, 1.0F};
    platform.forward_speed = 1.5F;
    platform.portal_name = "NEXT_ROOM";
    platform.health = 99;
    platform.unused_1d0 = 0x10;
    platform.unused_1d4 = 0xffffffffU;
    platform.beam_hit_message = {4, 7, 8, 9};
    platform.lifetime_message_indices[0] = 12;
    platform.lifetime_messages[0] = {5, 18, 20, 21};
    standard.typed_data = platform;

    const editor::EntityEditor converted = editor::make_entity_editor(standard);
    assert(!converted.first_hunt);
    assert(editor::entity_type_name(converted) == "Platform");
    const auto values = editor::field_values(converted);
    assert(find_field(values, "Position").value == "1,-0.5,2");
    assert(find_field(values, "NoPort").value == "2");
    assert(find_field(values, "Unused1D0").value == "16");
    assert(find_field(values, "LifetimeMsg1Index").value == "12");
    assert(find_field(values, "LifetimeMsg1Target").value == "5");
    assert(find_field(values, "LifetimeMessage1").value == "18");

    assert(editor::compare(converted, converted).empty());
    editor::EntityEditor changed = converted;
    std::get<scene::PlatformData>(changed.data).health = 100;
    const auto differences = editor::compare(converted, changed);
    assert(has_difference(differences, "Health"));

    changed = converted;
    changed.base.position.x += 1.0F;
    assert(has_difference(editor::compare(converted, changed), "Position"));

    scene::EntityInstance first_hunt;
    first_hunt.type = 111;
    first_hunt.kind = scene::EntityKind::Platform;
    first_hunt.entity_id = 3;
    first_hunt.node_name = "FH_PLAT";
    first_hunt.typed_data = scene::FhPlatformData{};
    const auto fh_editor = editor::make_entity_editor(first_hunt);
    assert(fh_editor.first_hunt);
    assert(editor::entity_type_name(fh_editor) == "FhPlatform");
    assert(find_field(editor::field_values(fh_editor), "NoPortal").value == "0");

    // Every editor-backed typed record must be safely projectable, including
    // the additional decoded records that do not have a dedicated managed
    // EntityEditor class.
    const scene::TypedEntityData typed_values[] = {
        scene::ObjectData{}, scene::EnemySpawnData{}, scene::PlayerSpawnData{},
        scene::DoorData{}, scene::ItemSpawnData{}, scene::TriggerVolumeData{},
        scene::AreaVolumeData{}, scene::JumpPadData{}, scene::PointModuleData{},
        scene::MorphCameraData{}, scene::OctolithFlagData{}, scene::FlagBaseData{},
        scene::TeleporterData{}, scene::NodeDefenseData{}, scene::LightSourceData{},
        scene::ArtifactData{}, scene::CameraSequenceData{}, scene::ForceFieldData{},
        enemy_spawn::FirstHuntData{}, scene::FhDoorData{}, scene::FhItemSpawnData{},
        scene::FhTriggerVolumeData{}, scene::FhAreaVolumeData{},
        scene::FhPlatformData{}, scene::FhJumpPadData{}, scene::FhMorphCameraData{}
    };
    for (const auto& data : typed_values) {
        scene::EntityInstance instance = standard;
        instance.typed_data = data;
        static_cast<void>(editor::field_values(editor::make_entity_editor(instance)));
    }
    return 0;
}
