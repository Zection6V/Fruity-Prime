#include "Entities/room_catalog.hpp"
#include "Mods/thumbnail_generator.hpp"

#include "../Mods/MapGen/custom_rooms.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <string>

int main() {
    fruityprime::mapgen::MapDefinition named;
    named.name = "My Test.Room";
    assert(fruityprime::mapgen::file_prefix(named) == "my test.room");
    const auto& story = fruityprime::scene::story_rooms();
    assert(story.size() == 66);
    for (std::size_t i = 0; i < story.size(); ++i) {
        assert(story[i].id == 27 + static_cast<int>(i));
        assert(!story[i].name.empty());
        assert(story[i].definition.name == story[i].name);
        assert(story[i].definition.model_archive.rfind(
            "archives/", 0) == 0);
        assert(!story[i].definition.animation_entry.empty());
        assert(!story[i].definition.entity_path.empty());
    }

    const auto& rooms = fruityprime::scene::multiplayer_rooms();
    assert(rooms.size() >= 27);
    for (std::size_t i = 0; i < 27; ++i) {
        assert(rooms[i].id == 93 + static_cast<int>(i));
        assert(!rooms[i].name.empty());
        assert(!rooms[i].in_game_name.empty());
        assert(rooms[i].definition.name == rooms[i].name);
        assert(rooms[i].definition.model_archive.rfind(
            "archives/", 0) == 0);
        assert(rooms[i].definition.model_entry.size() > 4);
        assert(rooms[i].definition.collision_entry.size() > 4);
        assert(rooms[i].definition.texture_path.rfind(
            "levels/textures/", 0) == 0);
        assert(rooms[i].definition.entity_path.rfind(
            "levels/entities/", 0) == 0);
    }
    if (rooms.size() > 27) {
        const auto* custom = fruityprime::scene::find_multiplayer_room(
            "TEST ARENA");
        assert(custom != nullptr);
        assert(custom->id >= 138);
        assert(custom->definition.model_archive.empty());
        assert(!custom->definition.external_root.empty());
    }

    const auto* exact = fruityprime::scene::find_multiplayer_room(
        "MP1 SANCTORUS");
    assert(exact != nullptr);
    assert(exact->id == 93);
    const auto* case_insensitive = fruityprime::scene::find_multiplayer_room(
        "mp1 sanctorus");
    assert(case_insensitive == exact);
    const auto* story_exact = fruityprime::scene::find_room("unit1_c0");
    assert(story_exact != nullptr && story_exact->id == 28);
    assert(fruityprime::scene::find_room("MP1 SANCTORUS") == exact);
    assert(fruityprime::scene::find_multiplayer_room("missing") == nullptr);
    const auto without_first_hunt = fruityprime::mods::thumbnail::multiplayer_rooms(false);
    const auto with_first_hunt = fruityprime::mods::thumbnail::multiplayer_rooms(true);
    assert(with_first_hunt.size() == without_first_hunt.size() + 1);
    assert(std::find(with_first_hunt.begin(), with_first_hunt.end(),
                     "E3 FIRST HUNT") != with_first_hunt.end());
    assert(std::find(without_first_hunt.begin(), without_first_hunt.end(),
                     "E3 FIRST HUNT") == without_first_hunt.end());
    assert(std::is_sorted(with_first_hunt.begin(), with_first_hunt.end(),
                          [](const std::string& left,
                             const std::string& right) {
                              std::string left_lower = left;
                              std::string right_lower = right;
                              std::transform(left_lower.begin(), left_lower.end(),
                                             left_lower.begin(), [](unsigned char c) {
                                                 return static_cast<char>(std::tolower(c));
                                             });
                              std::transform(right_lower.begin(), right_lower.end(),
                                             right_lower.begin(), [](unsigned char c) {
                                                 return static_cast<char>(std::tolower(c));
                                             });
                              return left_lower < right_lower;
                          }));
    assert(fruityprime::scene::classify_entity_type(2)
           == fruityprime::scene::EntityKind::PlayerSpawn);
    assert(fruityprime::scene::classify_entity_type(1, true)
           == fruityprime::scene::EntityKind::PlayerSpawn);
    assert(fruityprime::scene::classify_entity_type(20)
           == fruityprime::scene::EntityKind::Unknown);
    assert(fruityprime::scene::classify_entity_type(101)
           == fruityprime::scene::EntityKind::Unknown);

    using fruityprime::scene::EntityVolume;
    using fruityprime::scene::VolumeKind;
    EntityVolume box;
    box.kind = VolumeKind::Box;
    box.box_vector1 = {1.0F, 0.0F, 0.0F};
    box.box_vector2 = {0.0F, 1.0F, 0.0F};
    box.box_vector3 = {0.0F, 0.0F, 1.0F};
    box.box_position = {1.0F, 2.0F, 3.0F};
    box.box_dot1 = 2.0F;
    box.box_dot2 = 4.0F;
    box.box_dot3 = 6.0F;
    assert(box.contains(box.center()));
    assert(box.contains({1.0F, 2.0F, 3.0F}));
    assert(!box.contains({3.1F, 2.0F, 3.0F}));
    EntityVolume cylinder;
    cylinder.kind = VolumeKind::Cylinder;
    cylinder.cylinder_vector = {0.0F, 1.0F, 0.0F};
    cylinder.cylinder_position = {0.0F, 0.0F, 0.0F};
    cylinder.cylinder_radius = 2.0F;
    cylinder.cylinder_dot = 4.0F;
    assert(cylinder.contains(cylinder.center()));
    assert(cylinder.contains({1.0F, 2.0F, 0.0F}));
    assert(!cylinder.contains({2.1F, 2.0F, 0.0F}));
    EntityVolume sphere;
    sphere.kind = VolumeKind::Sphere;
    sphere.sphere_position = {4.0F, 5.0F, 6.0F};
    sphere.sphere_radius = 2.0F;
    assert(sphere.contains(sphere.center()));
    assert(sphere.contains({5.0F, 5.0F, 6.0F}));
    assert(!sphere.contains({6.1F, 5.0F, 6.0F}));
    return 0;
}
