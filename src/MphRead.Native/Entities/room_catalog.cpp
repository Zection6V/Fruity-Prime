#include "Entities/room_catalog.hpp"

#include "Formats/paths.hpp"
#include "../Mods/MapGen/custom_rooms.hpp"

#include <cctype>
#include <filesystem>
#include <utility>

namespace fruityprime::scene {
namespace {

RoomCatalogEntry make_entry(int id, std::string name,
                             std::string in_game_name,
                             std::string archive, std::string model,
                             std::string collision, std::string texture,
                             std::string entity,
                             std::string animation = {},
                             std::string node = {}) {
    return RoomCatalogEntry{
        id,
        name,
        in_game_name,
        RoomDefinition{
            name,
            "archives/" + archive + ".arc",
            std::move(model),
            "levels/textures/" + std::move(texture),
            std::move(collision),
            "levels/entities/" + std::move(entity),
            std::move(animation),
            node.empty() ? std::string{} : "levels/nodeData/" + std::move(node),
            {}
        }
    };
}

RoomCatalogEntry make_custom_entry(
    int id, const fruityprime::mapgen::MapDefinition& definition) {
    const std::string prefix = fruityprime::mapgen::file_prefix(definition);
    std::filesystem::path root = fruityprime::formats::global_paths()
        .file_system();
    if (root.empty()) {
        root = std::filesystem::current_path();
    }
    return RoomCatalogEntry{
        id,
        definition.name,
        definition.in_game_name.empty() ? definition.name
                                        : definition.in_game_name,
        RoomDefinition{
            definition.name,
            {},
            "_archives/" + prefix + "/" + prefix + "_Model.bin",
            {},
            "_archives/" + prefix + "/" + prefix + "_Collision.bin",
            "levels/entities/" + prefix + "_Ent.bin",
            "_archives/" + prefix + "/" + prefix + "_Anim.bin",
            "levels/nodeData/" + prefix + "_Node.bin",
            root
        }
    };
}

bool equal_ascii_insensitive(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto left_char = static_cast<unsigned char>(left[i]);
        const auto right_char = static_cast<unsigned char>(right[i]);
        if (std::tolower(left_char) != std::tolower(right_char)) {
            return false;
        }
    }
    return true;
}

} // namespace

const std::vector<RoomCatalogEntry>& story_rooms() {
    static const std::vector<RoomCatalogEntry> rooms{
        make_entry(27, "UNIT1_LAND", "Alinos Gateway", "unit1_Land",
                   "unit1_land_model.bin", "unit1_land_collision.bin",
                   "unit1_land_tex.bin", "Unit1_Land_Ent.bin",
                   "unit1_land_anim.bin", "unit1_Land_node.bin"),
        make_entry(28, "UNIT1_C0", "Echo Hall", "unit1_C0",
                   "unit1_c0_model.bin", "unit1_c0_collision.bin",
                   "unit1_c0_tex.bin", "Unit1_C0_Ent.bin",
                   "unit1_c0_anim.bin", "unit1_C0_node.bin"),
        make_entry(29, "UNIT1_RM1", "High Ground", "unit1_RM1",
                   "unit1_RM1_model.bin", "unit1_RM1_collision.bin",
                   "unit1_rm1_tex.bin", "unit1_RM1_Ent.bin",
                   "unit1_RM1_anim.bin", "unit1_RM1_node.bin"),
        make_entry(30, "UNIT1_C4", "Magma Drop", "unit1_C4",
                   "unit1_c4_model.bin", "unit1_c4_collision.bin",
                   "unit1_c4_tex.bin", "Unit1_C4_Ent.bin",
                   "unit1_c4_anim.bin"),
        make_entry(31, "UNIT1_RM6", "Elder Passage", "unit1_RM6",
                   "unit1_rm6_model.bin", "unit1_rm6_collision.bin",
                   "unit1_rm6_tex.bin", "unit1_RM6_Ent.bin",
                   "unit1_rm6_anim.bin", "unit1_RM6_node.bin"),
        make_entry(32, "CRYSTALROOM", "Alimbic Cannon Control Room",
                   "crystalroom", "crystalroom_model.bin",
                   "crystalroom_collision.bin", "crystalroom_tex.bin",
                   "crystalroom_Ent.bin", "crystalroom_anim.bin",
                   "crystalroom_node.bin"),
        make_entry(33, "UNIT1_RM4", "Combat Hall", "mp3",
                   "mp3_Model.bin", "mp3_Collision.bin", "mp3_Tex.bin",
                   "unit1_rm4_Ent.bin", "mp3_Anim.bin",
                   "unit1_RM4_Node.bin"),
        make_entry(34, "UNIT1_TP1", "Stronghold Void A", "TeleportRoom",
                   "TeleportRoom_model.bin", "TeleportRoom_collision.bin",
                   "teleportroom_tex.bin", "Unit1_TP1_Ent.bin",
                   "TeleportRoom_anim.bin"),
        make_entry(35, "UNIT1_B1", "Biodefense Chamber 02", "bigeyeroom",
                   "bigeyeroom_model.bin", "bigeyeroom_collision.bin",
                   "bigeyeroom_tex.bin", "Unit1_b1_Ent.bin",
                   "bigeyeroom_anim.bin", "unit2_b2_node.bin"),
        make_entry(36, "UNIT1_C1", "Alimbic Gardens", "unit1_C1",
                   "unit1_c1_model.bin", "unit1_c1_collision.bin",
                   "unit1_c1_tex.bin", "Unit1_C1_Ent.bin",
                   "unit1_c1_anim.bin", "unit1_C1_node.bin"),
        make_entry(37, "UNIT1_C2", "Thermal Vast", "unit1_C2",
                   "unit1_c2_model.bin", "unit1_c2_collision.bin",
                   "unit1_c2_tex.bin", "Unit1_C2_Ent.bin",
                   "unit1_c2_anim.bin", "unit1_C2_node.bin"),
        make_entry(38, "UNIT1_C5", "Piston Cave", "unit1_C5",
                   "unit1_c5_model.bin", "unit1_c5_collision.bin",
                   "unit1_c5_tex.bin", "Unit1_C5_Ent.bin",
                   "unit1_c5_anim.bin", "unit1_RM5_node.bin"),
        make_entry(39, "UNIT1_RM2", "Alinos Perch", "unit1_RM2",
                   "unit1_rm2_model.bin", "unit1_rm2_collision.bin",
                   "unit1_rm2_tex.bin", "unit1_RM2_ent.bin",
                   "unit1_rm2_anim.bin", "unit1_RM2_node.bin"),
        make_entry(40, "UNIT1_RM3", "Council Chamber", "unit1_RM3",
                   "unit1_rm3_model.bin", "unit1_rm3_collision.bin",
                   "unit1_rm3_tex.bin", "unit1_rm3_Ent.bin",
                   "unit1_rm3_anim.bin", "unit1_RM3_Node.bin"),
        make_entry(41, "UNIT1_RM5", "Processor Core", "mp7",
                   "mp7_model.bin", "mp7_collision.bin", "mp7_tex.bin",
                   "unit1_rm5_Ent.bin", "mp7_anim.bin",
                   "unit1_RM5_node.bin"),
        make_entry(42, "UNIT1_C3", "Crash Site", "unit1_C3",
                   "unit1_c3_model.bin", "unit1_c3_collision.bin",
                   "unit1_c3_tex.bin", "Unit1_C3_Ent.bin",
                   "unit1_c3_anim.bin", "unit1_C3_node.bin"),
        make_entry(43, "UNIT1_TP2", "Stronghold Void B", "TeleportRoom",
                   "TeleportRoom_model.bin", "TeleportRoom_collision.bin",
                   "teleportroom_tex.bin", "Unit1_TP2_Ent.bin",
                   "TeleportRoom_anim.bin"),
        make_entry(44, "UNIT1_B2", "Biodefense Chamber 06", "cylinderroom",
                   "cylinderroom_model.bin", "cylinderroom_collision.bin",
                   "cylinderroom_tex.bin", "Unit1_b2_Ent.bin",
                   "cylinderroom_anim.bin", "unit2_b1_node.bin"),
        make_entry(45, "UNIT2_LAND", "Celestial Gateway", "unit2_Land",
                   "unit2_Land_model.bin", "unit2_Land_collision.bin",
                   "unit2_land_tex.bin", "unit2_Land_Ent.bin",
                   "unit2_Land_anim.bin"),
        make_entry(46, "UNIT2_C0", "Helm Room", "unit2_C0",
                   "unit2_c0_model.bin", "unit2_c0_collision.bin",
                   "unit2_c0_tex.bin", "unit2_C0_Ent.bin",
                   "unit2_c0_anim.bin", "unit2_C0_Node.bin"),
        make_entry(47, "UNIT2_C1", "Meditation Room", "unit2_C1",
                   "unit2_c1_model.bin", "unit2_c1_collision.bin",
                   "unit2_c1_tex.bin", "unit2_C1_Ent.bin",
                   "unit2_c1_anim.bin", "unit2_C1_Node.bin"),
        make_entry(48, "UNIT2_RM1", "Data Shrine 01", "mp1",
                   "mp1_Model.bin", "mp1_Collision.bin", "mp1_tex.bin",
                   "unit2_RM1_Ent.bin", "mp1_Anim.bin",
                   "unit2_RM1_Node.bin"),
        make_entry(49, "UNIT2_C2", "Fan Room Alpha", "unit2_C2",
                   "unit2_c2_model.bin", "unit2_c2_collision.bin",
                   "unit2_c2_tex.bin", "unit2_C2_Ent.bin",
                   "unit2_c2_anim.bin", "unit2_C2_Node.bin"),
        make_entry(50, "UNIT2_RM2", "Data Shrine 02", "mp1",
                   "mp1_Model.bin", "mp1_Collision.bin", "mp1_tex.bin",
                   "unit2_RM2_Ent.bin", "mp1_Anim.bin",
                   "unit2_RM2_Node.bin"),
        make_entry(51, "UNIT2_C3", "Fan Room Beta", "unit2_C3",
                   "unit2_c3_model.bin", "unit2_c3_collision.bin",
                   "unit2_c3_tex.bin", "unit2_C3_Ent.bin",
                   "unit2_c3_anim.bin", "unit2_C3_Node.bin"),
        make_entry(52, "UNIT2_RM3", "Data Shrine 03", "unit2_RM3",
                   "unit2_RM3_model.bin", "unit2_RM3_collision.bin",
                   "unit2_rm3_tex.bin", "unit2_RM3_Ent.bin",
                   "unit2_RM3_anim.bin", "unit2_RM3_Node.bin"),
        make_entry(53, "UNIT2_C4", "Synergy Core", "unit2_C4",
                   "unit2_c4_model.bin", "unit2_c4_collision.bin",
                   "unit2_c4_tex.bin", "unit2_C4_Ent.bin",
                   "unit2_c4_anim.bin", "unit2_C4_Node.bin"),
        make_entry(54, "UNIT2_TP1", "Stronghold Void A", "TeleportRoom",
                   "TeleportRoom_model.bin", "TeleportRoom_collision.bin",
                   "teleportroom_tex.bin", "Unit2_TP1_Ent.bin",
                   "TeleportRoom_anim.bin"),
        make_entry(55, "UNIT2_B1", "Biodefense Chamber 01", "cylinderroom",
                   "cylinderroom_model.bin", "cylinderroom_collision.bin",
                   "cylinderroom_tex.bin", "Unit2_b1_Ent.bin",
                   "cylinderroom_anim.bin", "unit2_b1_node.bin"),
        make_entry(56, "UNIT2_C6", "Tetra Vista", "unit2_C6",
                   "unit2_c6_model.bin", "unit2_c6_collision.bin",
                   "unit2_c6_tex.bin", "Unit2_C6_Ent.bin",
                   "unit2_c6_anim.bin", "unit2_C6_Node.bin"),
        make_entry(57, "UNIT2_C7", "New Arrival Registration", "unit2_C7",
                   "unit2_c7_model.bin", "unit2_c7_collision.bin",
                   "unit2_c7_tex.bin", "Unit2_C7_Ent.bin",
                   "unit2_c7_anim.bin", "unit2_C7_Node.bin"),
        make_entry(58, "UNIT2_RM4", "Transfer Lock", "unit2_RM4",
                   "unit2_rm4_model.bin", "unit2_rm4_collision.bin",
                   "unit2_rm4_tex.bin", "Unit2_RM4_Ent.bin",
                   "unit2_rm4_anim.bin", "unit2_RM4_node.bin"),
        make_entry(59, "UNIT2_RM5", "Incubation Vault 01", "mp10",
                   "mp10_model.bin", "mp10_collision.bin", "mp10_tex.bin",
                   "Unit2_RM5_Ent.bin", "mp10_anim.bin",
                   "unit2_RM5_node.bin"),
        make_entry(60, "UNIT2_RM6", "Incubation Vault 02", "mp10",
                   "mp10_model.bin", "mp10_collision.bin", "mp10_tex.bin",
                   "Unit2_RM6_Ent.bin", "mp10_anim.bin",
                   "unit2_RM6_node.bin"),
        make_entry(61, "UNIT2_RM7", "Incubation Vault 03", "mp10",
                   "mp10_model.bin", "mp10_collision.bin", "mp10_tex.bin",
                   "Unit2_RM7_Ent.bin", "mp10_anim.bin",
                   "unit2_RM7_node.bin"),
        make_entry(62, "UNIT2_RM8", "Docking Bay", "unit2_RM8",
                   "unit2_rm8_model.bin", "unit2_rm8_collision.bin",
                   "unit2_rm8_tex.bin", "unit2_RM8_Ent.bin",
                   "unit2_rm8_anim.bin", "unit2_RM8_node.bin"),
        make_entry(63, "UNIT2_TP2", "Stronghold Void B", "TeleportRoom",
                   "TeleportRoom_model.bin", "TeleportRoom_collision.bin",
                   "teleportroom_tex.bin", "Unit2_TP2_Ent.bin",
                   "TeleportRoom_anim.bin"),
        make_entry(64, "UNIT2_B2", "Biodefense Chamber 05", "bigeyeroom",
                   "bigeyeroom_model.bin", "bigeyeroom_collision.bin",
                   "bigeyeroom_tex.bin", "Unit2_b2_Ent.bin",
                   "bigeyeroom_anim.bin", "unit2_b2_node.bin"),
        make_entry(65, "UNIT3_LAND", "VDO Gateway", "unit3_Land",
                   "unit3_land_model.bin", "unit3_land_collision.bin",
                   "unit3_land_tex.bin", "unit3_Land_Ent.bin",
                   "unit3_land_anim.bin", "unit3_Land_node.bin"),
        make_entry(66, "UNIT3_C0", "Bioweaponry Lab", "unit3_C0",
                   "unit3_c0_model.bin", "unit3_c0_collision.bin",
                   "unit3_c0_tex.bin", "unit3_C0_Ent.bin",
                   "unit3_c0_anim.bin", "unit3_C0_node.bin"),
        make_entry(67, "UNIT3_C2", "Cortex CPU", "unit3_C2",
                   "unit3_c2_model.bin", "unit3_c2_collision.bin",
                   "unit3_c2_tex.bin", "Unit3_C2_Ent.bin",
                   "unit3_c2_anim.bin"),
        make_entry(68, "UNIT3_RM1", "Weapons Complex", "unit3_RM1",
                   "unit3_rm1_model.bin", "unit3_rm1_collision.bin",
                   "unit3_rm1_Tex.bin", "Unit3_RM1_Ent.bin",
                   "unit3_rm1_anim.bin", "unit3_RM1_node.bin"),
        make_entry(69, "UNIT3_RM4", "Compression Chamber", "mp5",
                   "mp5_Model.bin", "mp5_Collision.bin", "mp5_tex.bin",
                   "unit3_rm4_Ent.bin", "mp5_Anim.bin",
                   "Unit3_RM4_node.bin"),
        make_entry(70, "UNIT3_TP1", "Stronghold Void A", "TeleportRoom",
                   "TeleportRoom_model.bin", "TeleportRoom_collision.bin",
                   "teleportroom_tex.bin", "Unit3_TP1_Ent.bin",
                   "TeleportRoom_anim.bin"),
        make_entry(71, "UNIT3_B1", "Biodefense Chamber 03", "cylinderroom",
                   "cylinderroom_model.bin", "cylinderroom_collision.bin",
                   "cylinderroom_tex.bin", "Unit3_b1_Ent.bin",
                   "cylinderroom_anim.bin", "unit2_b1_node.bin"),
        make_entry(72, "UNIT3_C1", "Ascension", "unit3_C1",
                   "unit3_c1_model.bin", "unit3_c1_collision.bin",
                   "unit3_c1_tex.bin", "unit3_C1_Ent.bin",
                   "unit3_c1_anim.bin", "unit3_C1_node.bin"),
        make_entry(73, "UNIT3_RM2", "Fuel Stack", "unit3_RM2",
                   "unit3_rm2_model.bin", "unit3_rm2_collision.bin",
                   "unit3_rm2_tex.bin", "Unit3_RM2_Ent.bin",
                   "unit3_rm2_anim.bin", "unit3_RM2_node.bin"),
        make_entry(74, "UNIT3_RM3", "Stasis Bunker", "e3Level",
                   "e3Level_Model.bin", "e3Level_Collision.bin",
                   "e3level_tex.bin", "Unit3_RM3_Ent.bin",
                   "e3Level_Anim.bin", "unit3_rm3_Node.bin"),
        make_entry(75, "UNIT3_TP2", "Stronghold Void B", "TeleportRoom",
                   "TeleportRoom_model.bin", "TeleportRoom_collision.bin",
                   "teleportroom_tex.bin", "Unit3_TP2_Ent.bin",
                   "TeleportRoom_anim.bin"),
        make_entry(76, "UNIT3_B2", "Biodefense Chamber 08", "bigeyeroom",
                   "bigeyeroom_model.bin", "bigeyeroom_collision.bin",
                   "bigeyeroom_tex.bin", "Unit3_b2_Ent.bin",
                   "bigeyeroom_anim.bin", "unit2_b2_node.bin"),
        make_entry(77, "UNIT4_LAND", "Arcterra Gateway", "unit4_Land",
                   "unit4_land_model.bin", "unit4_land_collision.bin",
                   "unit4_land_tex.bin", "unit4_Land_Ent.bin",
                   "unit4_land_anim.bin", "unit4_Land_node.bin"),
        make_entry(78, "UNIT4_RM1", "Ice Hive", "unit4_rm1",
                   "unit4_rm1_model.bin", "unit4_rm1_collision.bin",
                   "unit4_rm1_Tex.bin", "Unit4_RM1_Ent.bin",
                   "unit4_rm1_anim.bin", "unit4_RM1_node.bin"),
        make_entry(79, "UNIT4_RM3", "Sic Transit", "mp12",
                   "mp12_model.bin", "mp12_collision.bin", "mp12_Tex.bin",
                   "unit4_rm3_Ent.bin", "mp12_anim.bin",
                   "Unit4_RM3_node.bin"),
        make_entry(80, "UNIT4_C0", "Frost Labyrinth", "unit4_C0",
                   "unit4_c0_model.bin", "unit4_c0_collision.bin",
                   "unit4_c0_tex.bin", "unit4_C0_Ent.bin",
                   "unit4_c0_anim.bin", "unit4_C0_node.bin"),
        make_entry(81, "UNIT4_TP1", "Stronghold Void A", "TeleportRoom",
                   "TeleportRoom_model.bin", "TeleportRoom_collision.bin",
                   "teleportroom_tex.bin", "Unit4_TP1_Ent.bin",
                   "TeleportRoom_anim.bin"),
        make_entry(82, "UNIT4_B1", "Biodefense Chamber 04", "bigeyeroom",
                   "bigeyeroom_model.bin", "bigeyeroom_collision.bin",
                   "bigeyeroom_tex.bin", "unit4_b1_Ent.bin",
                   "bigeyeroom_anim.bin", "unit2_b2_node.bin"),
        make_entry(83, "UNIT4_C1", "Drip Moat", "unit4_C1",
                   "unit4_c1_model.bin", "unit4_c1_collision.bin",
                   "unit4_c1_tex.bin", "unit4_C1_Ent.bin",
                   "unit4_c1_anim.bin"),
        make_entry(84, "UNIT4_RM2", "Subterranean", "unit4_rm2",
                   "unit4_rm2_model.bin", "unit4_rm2_collision.bin",
                   "unit4_rm2_tex.bin", "Unit4_RM2_Ent.bin",
                   "unit4_rm2_anim.bin", "unit4_RM2_node.bin"),
        make_entry(85, "UNIT4_RM4", "Sanctorus", "mp11",
                   "mp11_model.bin", "mp11_collision.bin", "mp11_tex.bin",
                   "unit4_rm4_Ent.bin", "mp11_anim.bin",
                   "Unit4_RM4_node.bin"),
        make_entry(86, "UNIT4_RM5", "Fault Line", "unit4_rm5",
                   "unit4_rm5_model.bin", "unit4_rm5_collision.bin",
                   "unit4_rm5_tex.bin", "Unit4_RM5_Ent.bin",
                   "unit4_rm5_anim.bin", "Unit4_RM5_node.bin"),
        make_entry(87, "UNIT4_TP2", "Stronghold Void B", "TeleportRoom",
                   "TeleportRoom_model.bin", "TeleportRoom_collision.bin",
                   "teleportroom_tex.bin", "Unit4_TP2_Ent.bin",
                   "TeleportRoom_anim.bin"),
        make_entry(88, "UNIT4_B2", "Biodefense Chamber 07", "cylinderroom",
                   "cylinderroom_model.bin", "cylinderroom_collision.bin",
                   "cylinderroom_tex.bin", "unit4_b2_Ent.bin",
                   "cylinderroom_anim.bin", "unit2_b1_node.bin"),
        make_entry(89, "Gorea_Land", "", "Gorea_Land",
                   "Gorea_Land_Model.bin", "Gorea_Land_collision.bin",
                   "Gorea_Land_tex.bin", "Gorea_Land_Ent.bin",
                   "Gorea_Land_Anim.bin"),
        make_entry(90, "Gorea_Peek", "", "Gorea_b2",
                   "gorea_b2_Model.bin", "Gorea_b2_collision.bin",
                   "gorea_b2_tex.bin", "Gorea_Peek_Ent.bin",
                   "gorea_b2_Anim.bin", "gorea_b2_node.bin"),
        make_entry(91, "Gorea_b1", "", "Gorea_b1",
                   "Gorea_b1_Model.bin", "Gorea_b1_collision.bin",
                   "Gorea_b1_tex.bin", "Gorea_b1_Ent.bin",
                   "Gorea_b1_Anim.bin"),
        make_entry(92, "Gorea_b2", "", "Gorea_b2",
                   "gorea_b2_Model.bin", "Gorea_b2_collision.bin",
                   "gorea_b2_tex.bin", "gorea_b2_Ent.bin",
                   "gorea_b2_Anim.bin", "gorea_b2_node.bin")
    };
    return rooms;
}

const std::vector<RoomCatalogEntry>& multiplayer_rooms() {
    static const std::vector<RoomCatalogEntry> rooms = [] {
        std::vector<RoomCatalogEntry> rooms{
        make_entry(93, "MP1 SANCTORUS", "Data Shrine", "mp1",
                   "mp1_Model.bin", "mp1_Collision.bin", "mp1_tex.bin",
                   "mp1_Ent.bin"),
        make_entry(94, "MP2 HARVESTER", "Harvester", "mp2",
                   "mp2_model.bin", "mp2_collision.bin", "mp2_tex.bin",
                   "mp2_Ent.bin"),
        make_entry(95, "MP3 PROVING GROUND", "Combat Hall", "mp3",
                   "mp3_Model.bin", "mp3_Collision.bin", "mp3_Tex.bin",
                   "mp3_Ent.bin"),
        make_entry(96, "MP4 HIGHGROUND - EXPANDED", "Elder Passage", "mp4",
                   "mp4_model.bin", "mp4_collision.bin", "mp4_Tex.bin",
                   "mp4_Ent.bin"),
        make_entry(97, "MP4 HIGHGROUND", "High Ground", "unit1_RM1",
                   "unit1_RM1_model.bin", "unit1_RM1_collision.bin",
                   "unit1_rm1_tex.bin", "mp4_dm1_Ent.bin"),
        make_entry(98, "MP5 FUEL SLUICE", "Compression Chamber", "mp5",
                   "mp5_Model.bin", "mp5_Collision.bin", "mp5_tex.bin",
                   "mp5_Ent.bin"),
        make_entry(99, "MP6 HEADSHOT", "Head Shot", "mp6",
                   "mp6_model.bin", "mp6_collision.bin", "mp6_tex.bin",
                   "mp6_Ent.bin"),
        make_entry(100, "MP7 PROCESSOR CORE", "Processor Core", "mp7",
                   "mp7_model.bin", "mp7_collision.bin", "mp7_tex.bin",
                   "mp7_Ent.bin"),
        make_entry(101, "MP8 FIRE CONTROL", "Weapons Complex", "mp8",
                   "mp8_model.bin", "mp8_collision.bin", "mp8_Tex.bin",
                   "mp8_Ent.bin"),
        make_entry(102, "MP9 CRYOCHASM", "Ice Hive", "mp9",
                   "mp9_model.bin", "mp9_collision.bin", "mp9_tex.bin",
                   "mp9_Ent.bin"),
        make_entry(103, "MP10 OVERLOAD", "Incubation Vault", "mp10",
                   "mp10_model.bin", "mp10_collision.bin", "mp10_tex.bin",
                   "mp10_Ent.bin"),
        make_entry(104, "MP11 BREAKTHROUGH", "Sanctorus", "mp11",
                   "mp11_model.bin", "mp11_collision.bin", "mp11_tex.bin",
                   "mp11_Ent.bin"),
        make_entry(105, "MP12 SIC TRANSIT", "Sic Transit", "mp12",
                   "mp12_model.bin", "mp12_collision.bin", "mp12_Tex.bin",
                   "mp12_Ent.bin"),
        make_entry(106, "MP13 ACCELERATOR", "Fuel Stack", "mp13",
                   "mp13_model.bin", "mp13_collision.bin", "mp13_tex.bin",
                   "mp13_Ent.bin"),
        make_entry(107, "MP14 OUTER REACH", "Outer Reach", "mp14",
                   "mp14_model.bin", "mp14_collision.bin", "mp14_tex.bin",
                   "mp14_Ent.bin"),
        make_entry(108, "CTF1 FAULT LINE - EXPANDED", "Fault Line", "ctf1",
                   "ctf1_model.bin", "ctf1_collision.bin", "ctf1_tex.bin",
                   "ctf1_Ent.bin"),
        make_entry(109, "CTF1_FAULT LINE", "Subterranean", "unit4_rm5",
                   "unit4_rm5_model.bin", "unit4_rm5_collision.bin",
                   "unit4_rm5_tex.bin", "ctf1_dm1_Ent.bin"),
        make_entry(110, "AD1 TRANSFER LOCK BT", "Transfer Lock", "ad1",
                   "ad1_model.bin", "ad1_collision.bin", "ad1_tex.bin",
                   "ad1_Ent.bin"),
        make_entry(111, "AD1 TRANSFER LOCK DM", "Transfer Lock", "unit2_RM4",
                   "unit2_rm4_model.bin", "unit2_rm4_collision.bin",
                   "unit2_rm4_tex.bin", "ad1_dm1_Ent.bin"),
        make_entry(112, "AD2 MAGMA VENTS", "Council Chamber", "ad2",
                   "ad2_model.bin", "ad2_collision.bin", "ad2_tex.bin",
                   "ad2_Ent.bin"),
        make_entry(113, "AD2 ALINOS PERCH", "Alinos Perch", "unit1_RM2",
                   "unit1_rm2_model.bin", "unit1_rm2_collision.bin",
                   "unit1_rm2_tex.bin", "ad2_dm1_Ent.bin"),
        make_entry(114, "UNIT1 ALINOS LANDFALL", "Alinos Gateway", "unit1_Land",
                   "unit1_land_model.bin", "unit1_land_collision.bin",
                   "unit1_land_tex.bin", "Unit1_Land_dm1_Ent.bin"),
        make_entry(115, "UNIT2 LANDING BAY", "Celestial Gateway", "unit2_Land",
                   "unit2_Land_model.bin", "unit2_Land_collision.bin",
                   "unit2_land_tex.bin", "unit2_land_dm1_Ent.bin"),
        make_entry(116, "UNIT 3 VESPER STARPORT", "VDO Gateway", "unit3_Land",
                   "unit3_land_model.bin", "unit3_land_collision.bin",
                   "unit3_land_tex.bin", "unit3_Land_dm1_Ent.bin"),
        make_entry(117, "UNIT 4 ARCTERRA BASE", "Arcterra Gateway", "unit4_Land",
                   "unit4_land_model.bin", "unit4_land_collision.bin",
                   "unit4_land_tex.bin", "unit4_Land_dm1_Ent.bin"),
        make_entry(118, "Gorea Prison", "Oubliette", "Gorea_b2",
                   "gorea_b2_Model.bin", "Gorea_b2_collision.bin",
                   "gorea_b2_tex.bin", "gorea_b2_dm_Ent.bin"),
        make_entry(119, "E3 FIRST HUNT", "Stasis Bunker", "e3Level",
                   "e3Level_Model.bin", "e3Level_Collision.bin",
                   "e3level_tex.bin", "e3Level_Ent.bin")
        };
        // CustomRooms.AppendIds appends after all 138 cartridge/FH slots,
        // including the unused and unreferenced entries at 120..137.
        int id = 138;
        for (const auto& definition
             : fruityprime::mapgen::custom_rooms::definitions()) {
            rooms.push_back(make_custom_entry(id++, definition));
        }
        return rooms;
    }();
    return rooms;
}

const RoomCatalogEntry* find_room(std::string_view name) noexcept {
    for (const auto& room : story_rooms()) {
        if (room.name == name || equal_ascii_insensitive(room.name, name)) {
            return &room;
        }
    }
    return find_multiplayer_room(name);
}

const RoomCatalogEntry* find_multiplayer_room(std::string_view name) noexcept {
    for (const auto& room : multiplayer_rooms()) {
        if (room.name == name || equal_ascii_insensitive(room.name, name)) {
            return &room;
        }
    }
    return nullptr;
}

} // namespace fruityprime::scene
