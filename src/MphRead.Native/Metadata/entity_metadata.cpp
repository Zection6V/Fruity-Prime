#include "Metadata/entity_metadata.hpp"

#include <algorithm>

namespace fruityprime::metadata {
namespace {

constexpr std::array<std::int8_t, 4> NoAnimation{{-1, -1, -1, -1}};
constexpr std::array<std::int8_t, 4> ConsoleAnimation{{2, 1, 0, 0}};

constexpr ObjectInfo object(std::uint16_t id, std::string_view name,
                            bool lighting = false,
                            std::uint8_t recolor_id = 0,
                            bool ignore_animation = false,
                            std::array<std::int8_t, 4> animation_ids =
                                NoAnimation,
                            bool has_animation = false) noexcept {
    return {id, name, animation_ids, recolor_id, has_animation, lighting,
            ignore_animation};
}

constexpr std::array<ObjectInfo, ObjectCount> ObjectTable{{
    object(0, "AlimbicGhost_01"),
    object(1, "AlimbicLightPole"),
    object(2, "AlimbicStationShieldControl"),
    object(3, "AlimbicComputerStationControl"),
    object(4, "AlimbicEnergySensor"),
    object(5, "SamusShip"),
    object(6, "Guardbot01_Dead"),
    object(7, "Guardbot02_Dead"),
    object(8, "Guardian_Dead"),
    object(9, "Psychobit_Dead"),
    object(10, "AlimbicLightPole02"),
    object(11, "AlimbicComputerStationControl02"),
    object(12, "Generic_Console", false, 0, false, ConsoleAnimation, true),
    object(13, "Generic_Monitor", false, 0, false, ConsoleAnimation, true),
    object(14, "Generic_Power"),
    object(15, "Generic_Scanner", false, 0, false, ConsoleAnimation, true),
    object(16, "Generic_Switch", true, 0, false, ConsoleAnimation, true),
    object(17, "Alimbic_Console", false, 0, false, ConsoleAnimation, true),
    object(18, "Alimbic_Monitor", false, 0, false, ConsoleAnimation, true),
    object(19, "Alimbic_Power"),
    object(20, "Alimbic_Scanner", false, 0, false, ConsoleAnimation, true),
    object(21, "Alimbic_Switch", true, 0, false, ConsoleAnimation, true),
    object(22, "Lava_Console", false, 0, false, ConsoleAnimation, true),
    object(23, "Lava_Monitor", false, 0, false, ConsoleAnimation, true),
    object(24, "Lava_Power"),
    object(25, "Lava_Scanner", false, 0, false, ConsoleAnimation, true),
    object(26, "Lava_Switch", true, 0, false, ConsoleAnimation, true),
    object(27, "Ice_Console", false, 0, false, ConsoleAnimation, true),
    object(28, "Ice_Monitor", false, 0, false, ConsoleAnimation, true),
    object(29, "Ice_Power"),
    object(30, "Ice_Scanner", false, 0, false, ConsoleAnimation, true),
    object(31, "Ice_Switch", true, 0, false, ConsoleAnimation, true),
    object(32, "Ruins_Console", false, 0, false, ConsoleAnimation, true),
    object(33, "Ruins_Monitor", false, 0, false, ConsoleAnimation, true),
    object(34, "Ruins_Power"),
    object(35, "Ruins_Scanner", false, 0, false, ConsoleAnimation, true),
    object(36, "Ruins_Switch", true, 0, false, ConsoleAnimation, true),
    object(37, "PlantCarnivarous_Branched"),
    object(38, "PlantCarnivarous_Pod"),
    object(39, "PlantCarnivarous_PodLeaves"),
    object(40, "PlantCarnivarous_Vine"),
    object(41, "GhostSwitch"),
    object(42, "Switch", true),
    object(43, "Guardian_Stasis", false, 0, false,
           std::array<std::int8_t, 4>{{-1, 0, 0, 0}}, true),
    object(44, "AlimbicStatue_lod0", false, 0, true,
           std::array<std::int8_t, 4>{{-1, 0, 0, 0}}, true),
    object(45, "AlimbicCapsule"),
    object(46, "SniperTarget", true, 0, false,
           std::array<std::int8_t, 4>{{0, 2, 1, 0}}, true),
    object(47, "SecretSwitch", false, 1, false,
           std::array<std::int8_t, 4>{{1, 2, 0, 0}}, true),
    object(48, "SecretSwitch", false, 2, false,
           std::array<std::int8_t, 4>{{1, 2, 0, 0}}, true),
    object(49, "SecretSwitch", false, 3, false,
           std::array<std::int8_t, 4>{{1, 2, 0, 0}}, true),
    object(50, "SecretSwitch", false, 4, false,
           std::array<std::int8_t, 4>{{1, 2, 0, 0}}, true),
    object(51, "SecretSwitch", false, 5, false,
           std::array<std::int8_t, 4>{{1, 2, 0, 0}}, true),
    object(52, "SecretSwitch", false, 6, false,
           std::array<std::int8_t, 4>{{1, 2, 0, 0}}, true),
    object(53, "WallSwitch", true, 0, false,
           std::array<std::int8_t, 4>{{2, 0, 1, 0}}, true)
}};

constexpr PlatformInfo platform(std::uint16_t id, std::string_view name,
                                bool lighting = false,
                                std::array<std::int8_t, 4> animation_ids =
                                    NoAnimation,
                                bool has_animation = false) noexcept {
    return {id, name, animation_ids, has_animation, lighting};
}

constexpr std::array<PlatformInfo, PlatformCount> PlatformTable{{
    platform(0, "platform"),
    platform(1, {}),
    platform(2, {}),
    platform(3, "Elevator"),
    platform(4, "smasher"),
    platform(5, "Platform_Unit4_C1", true),
    platform(6, "pillar"),
    platform(7, "Door_Unit4_RM1"),
    platform(8, "SyluxShip", false,
             std::array<std::int8_t, 4>{{-1, 1, 0, 2}}, true),
    platform(9, "pistonmp7"),
    platform(10, "unit3_brain", false,
             std::array<std::int8_t, 4>{{0, 0, 0, 0}}, true),
    platform(11, "unit4_mover1", false,
             std::array<std::int8_t, 4>{{0, 0, 0, 0}}, true),
    platform(12, "unit4_mover2", false,
             std::array<std::int8_t, 4>{{0, 0, 0, 0}}, true),
    platform(13, "ElectroField1", false,
             std::array<std::int8_t, 4>{{0, 0, 0, 0}}, true),
    platform(14, "Unit3_platform1"),
    platform(15, "unit3_pipe1", false,
             std::array<std::int8_t, 4>{{0, 0, 0, 0}}, true),
    platform(16, "unit3_pipe2", false,
             std::array<std::int8_t, 4>{{0, 0, 0, 0}}, true),
    platform(17, "cylinderbase"),
    platform(18, "unit3_platform"),
    platform(19, "unit3_platform2"),
    platform(20, "unit3_jar", false,
             std::array<std::int8_t, 4>{{0, 2, 1, 0}}, true),
    platform(21, "SyluxTurret", false,
             std::array<std::int8_t, 4>{{3, 2, 1, 0}}, true),
    platform(22, "unit3_jartop", false,
             std::array<std::int8_t, 4>{{0, 2, 1, 0}}, true),
    platform(23, "SamusShip", false,
             std::array<std::int8_t, 4>{{1, 3, 2, 4}}, true),
    platform(24, "unit1_land_plat1"),
    platform(25, "unit1_land_plat2"),
    platform(26, "unit1_land_plat3"),
    platform(27, "unit1_land_plat4"),
    platform(28, "unit1_land_plat5"),
    platform(29, "unit2_c4_plat"),
    platform(30, "unit2_land_elev"),
    platform(31, "unit4_platform1"),
    platform(32, "Crate01", false,
             std::array<std::int8_t, 4>{{-1, -1, 0, 1}}, true),
    platform(33, "unit1_mover1", false,
             std::array<std::int8_t, 4>{{0, 0, 0, 0}}, true),
    platform(34, "unit1_mover2"),
    platform(35, "unit2_mover1"),
    platform(36, "unit4_mover3"),
    platform(37, "unit4_mover4"),
    platform(38, "unit3_mover1"),
    platform(39, "unit2_c1_mover"),
    platform(40, "unit3_mover2", false,
             std::array<std::int8_t, 4>{{0, 0, 0, 0}}, true),
    platform(41, "piston_gorealand"),
    platform(42, "unit4_tp2_artifact_wo"),
    platform(43, "unit4_tp1_artifact_wo"),
    platform(44, "SamusShip", false,
             std::array<std::int8_t, 4>{{1, 0, 2, 4}}, true)
}};

constexpr std::array<DoorInfo, DoorCount> DoorTable{{
    {0, "AlimbicDoor", "AlimbicDoorLock", 1.4F, 2.4F},
    {1, "AlimbicMorphBallDoor", "AlimbicMorphBallDoorLock", 0.7F, 1.0F},
    {2, "AlimbicBossDoor", "AlimbicBossDoorLock", 3.5F, 3.5F},
    {3, "AlimbicThinDoor", "ThinDoorLock", 1.4F, 2.0F}
}};

constexpr std::array<std::string_view, FhDoorCount> FhDoorTable{{
    "door", "door2", "door2_holo"
}};

constexpr std::array<std::string_view, JumpPadCount> JumpPadTable{{
    "JumpPad", "JumpPad_Alimbic", "JumpPad_Ice", "JumpPad_IceStation",
    "JumpPad_Lava", "JumpPad_Station"
}};

} // namespace

const std::array<ObjectInfo, ObjectCount>& objects() noexcept {
    return ObjectTable;
}

const ObjectInfo* object_info(std::uint32_t id) noexcept {
    return id < ObjectTable.size() ? &ObjectTable[id] : nullptr;
}

const std::array<PlatformInfo, PlatformCount>& platforms() noexcept {
    return PlatformTable;
}

const PlatformInfo* platform_info(std::uint32_t id) noexcept {
    if (id >= PlatformTable.size() || id == 1 || id == 2) {
        return nullptr;
    }
    return &PlatformTable[id];
}

const PlatformInfo* platform_model_info(std::uint32_t id) noexcept {
    if (id == 1) {
        id = 0;
    }
    return platform_info(id);
}

const std::array<DoorInfo, DoorCount>& doors() noexcept {
    return DoorTable;
}

const DoorInfo* door_info(std::uint32_t id) noexcept {
    return id < DoorTable.size() ? &DoorTable[id] : nullptr;
}

const std::array<std::string_view, FhDoorCount>& fh_doors() noexcept {
    return FhDoorTable;
}

const std::array<std::string_view, JumpPadCount>& jump_pads() noexcept {
    return JumpPadTable;
}

std::string_view jump_pad_name(std::uint32_t id) noexcept {
    if (id >= JumpPadTable.size()) {
        return {};
    }
    return JumpPadTable[id];
}

} // namespace fruityprime::metadata
