#include "../Entities/Enemies/enemy_catalog.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void write_u32(std::array<std::uint8_t, 400>& bytes, std::size_t offset,
               std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
}

void write_fixed(std::array<std::uint8_t, 400>& bytes, std::size_t offset,
                 float value) {
    write_u32(bytes, offset, static_cast<std::uint32_t>(
        static_cast<std::int32_t>(value * 4096.0F)));
}

void write_vector(std::array<std::uint8_t, 400>& bytes, std::size_t offset,
                  float x, float y, float z) {
    write_fixed(bytes, offset, x);
    write_fixed(bytes, offset + 4, y);
    write_fixed(bytes, offset + 8, z);
}

} // namespace

int main() {
    try {
        const auto& profiles = fruityprime::enemy::profiles();
        require(profiles.size() == fruityprime::metadata::EnemyCount,
                "enemy profile count does not match the cartridge table");

        const auto& wasp = fruityprime::enemy::profile(0);
        require(wasp.source_file == "00_WarWasp.cs"
                    && wasp.managed_class == "Enemy00Entity"
                    && wasp.family == fruityprime::enemy::BehaviorFamily::Flying,
                "War Wasp profile was not mapped to its managed class");

        const auto& gorea = fruityprime::enemy::profile(31);
        require(gorea.family == fruityprime::enemy::BehaviorFamily::Boss
                    && fruityprime::enemy::has_flag(
                        gorea.flags,
                        fruityprime::enemy::ProfileFlags::HasLinkedParts),
                "Gorea profile did not retain linked-part behavior");

        const auto& fire_spawn = fruityprime::enemy::profile(39);
        require(fruityprime::enemy::has_flag(
                    fire_spawn.flags,
                    fruityprime::enemy::ProfileFlags::UsesHitZone),
                "Fire Spawn profile did not retain its hit-zone behavior");

        const auto& plant = fruityprime::enemy::profile(51);
        require(plant.family == fruityprime::enemy::BehaviorFamily::Plant
                    && fruityprime::enemy::has_flag(
                        plant.flags,
                        fruityprime::enemy::ProfileFlags::IgnoresPlayerRange),
                "Carnivorous Plant profile did not retain range behavior");

        const auto& clamped = fruityprime::enemy::profile(255);
        require(clamped.id == 51,
                "out-of-range enemy profile lookup was not clamped safely");

        require(fruityprime::enemy::model_name(0) == "warwasp_lod0",
                "War Wasp model name was not mapped");
        require(fruityprime::enemy::model_name(31) == "Gorea2_lod0",
                "Gorea model name was not mapped");
        require(fruityprime::enemy::model_name(48).empty()
                    && fruityprime::enemy::model_name(255).empty(),
                "support enemy model IDs should remain empty");

        std::array<std::uint8_t, 400> fields{};
        write_u32(fields, 384, 2);
        write_u32(fields, 388, 2);
        write_vector(fields, 192, 1.0F, 0.0F, 0.0F);
        write_vector(fields, 204, 3.0F, 0.0F, 0.0F);
        const auto vector_motion = fruityprime::enemy::decode_authored_motion(
            0, fields, {10.0F, 2.0F, 20.0F});
        require(vector_motion.supported
                    && vector_motion.movement_type == 2
                    && vector_motion.path_count == 2
                    && vector_motion.path_positions[0].x == 11.0F
                    && vector_motion.path_positions[1].x == 13.0F,
                "War Wasp vector patrol was not decoded");

        fields.fill(0);
        constexpr std::size_t barbed_base = 8;
        write_u32(fields, barbed_base + 64, 0); // VolumeType.Box
        write_vector(fields, barbed_base + 64 + 28, 0.0F, 0.0F, 1.0F);
        write_vector(fields, barbed_base + 64 + 40, 2.0F, 0.0F, 3.0F);
        write_fixed(fields, barbed_base + 64 + 52, 2.0F);
        write_fixed(fields, barbed_base + 64 + 60, 4.0F);
        write_u32(fields, barbed_base + 388, 1);
        const auto box_motion = fruityprime::enemy::decode_authored_motion(
            10, fields, {10.0F, 2.0F, 20.0F});
        require(box_motion.supported
                    && box_motion.movement_type == 1
                    && box_motion.path_count == 4
                    && box_motion.path_positions[0].x == 12.0F
                    && box_motion.path_positions[0].z == 23.0F,
                "Barbed War Wasp box patrol was not decoded");

        fields.fill(0);
        write_u32(fields, 0, 2); // Zoomer hurt sphere
        write_vector(fields, 4, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 16, 0.25F);
        write_u32(fields, 64, 2); // Zoomer home sphere
        write_vector(fields, 68, 2.0F, 1.0F, 3.0F);
        write_fixed(fields, 80, 5.0F);
        const auto zoomer = fruityprime::enemy::decode_zoomer_profile(
            1, fields, {10.0F, 2.0F, 20.0F});
        require(zoomer.supported
                    && zoomer.hurt_volume.kind
                        == fruityprime::scene::VolumeKind::Sphere
                    && zoomer.home_volume.kind
                        == fruityprime::scene::VolumeKind::Sphere
                    && zoomer.home_volume.center().x == 12.0F
                    && zoomer.home_volume.center().y == 3.0F
                    && zoomer.home_volume.center().z == 23.0F,
                "Zoomer S00 home volume was not decoded");
        const auto geemer = fruityprime::enemy::decode_zoomer_profile(
            12, fields, {10.0F, 2.0F, 20.0F});
        require(geemer.supported,
                "Geemer S00 surface volume was not decoded");

        fields.fill(0);
        write_u32(fields, 0, 1); // Voldrum2 hurt cylinder
        write_vector(fields, 4, 0.0F, 1.0F, 0.0F);
        write_fixed(fields, 28, 0.5F);
        write_fixed(fields, 32, 1.0F);
        write_u32(fields, 64, 1); // Voldrum2 home cylinder
        write_vector(fields, 68, 0.0F, 1.0F, 0.0F);
        write_vector(fields, 80, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 92, 4.0F);
        write_fixed(fields, 96, 2.0F);
        const auto voldrum2 = fruityprime::enemy::decode_voldrum_profile(
            35, fields, {10.0F, 2.0F, 20.0F});
        require(voldrum2.supported && !voldrum2.ranged
                    && voldrum2.health == 42
                    && voldrum2.contact_damage == 2
                    && voldrum2.home_volume.center().y == 3.0F
                    && voldrum2.home_volume.contains({10.0F, 2.0F, 20.0F}),
                "Voldrum2 S00 volumes were not decoded");

        fields.fill(0);
        write_u32(fields, 0, 3); // Magma Voldrum subtype
        write_u32(fields, 4, 1); // Volt enemy weapon version
        write_u32(fields, 8, 2); // Voldrum1 hurt sphere
        write_vector(fields, 12, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 24, 0.5F);
        write_u32(fields, 72, 1); // Voldrum1 home cylinder
        write_vector(fields, 76, 0.0F, 1.0F, 0.0F);
        write_vector(fields, 88, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 100, 6.0F);
        write_fixed(fields, 104, 2.0F);
        const auto voldrum1 = fruityprime::enemy::decode_voldrum_profile(
            36, fields, {10.0F, 2.0F, 20.0F});
        require(voldrum1.supported && voldrum1.ranged
                    && voldrum1.variant == 3 && voldrum1.version == 1
                    && voldrum1.health == 150
                    && voldrum1.beam_damage == 8
                    && voldrum1.min_shots == 1
                    && voldrum1.max_shots == 2
                    && voldrum1.projectile_draw_func == 2
                    && voldrum1.projectile_collision_effect == 89,
                "Voldrum1 S06 combat profile was not decoded");

        fields.fill(0);
        write_u32(fields, 0, 3); // PsychoBit v3.0 subtype
        write_u32(fields, 4, 2); // enemy weapon version
        write_u32(fields, 8, 2); // hurt sphere
        write_vector(fields, 12, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 24, 1.0F);
        write_u32(fields, 72, 1); // home cylinder
        write_vector(fields, 76, 0.0F, 1.0F, 0.0F);
        write_vector(fields, 88, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 100, 6.0F);
        write_fixed(fields, 104, 3.0F);
        write_u32(fields, 200, 2); // target range sphere
        write_vector(fields, 204, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 216, 12.0F);
        const auto psychobit = fruityprime::enemy::decode_psychobit_profile(
            23, fields, {10.0F, 2.0F, 20.0F});
        require(psychobit.supported && psychobit.variant == 3
                    && psychobit.version == 2
                    && psychobit.health == 120
                    && psychobit.beam_damage == 8
                    && psychobit.contact_damage == 10
                    && psychobit.min_shots == 1
                    && psychobit.max_shots == 2
                    && psychobit.aim_steps == 30
                    && psychobit.range_volume.center().x == 10.0F
                    && psychobit.projectile_draw_func == 7
                    && psychobit.projectile_collision_effect == 8,
                "PsychoBit S06 combat profile was not decoded");

        fields.fill(0);
        write_u32(fields, 0, 1); // Arctic Spawn subtype
        write_u32(fields, 4, 0); // enemy weapon version
        write_u32(fields, 72, 1); // authored location cylinder
        write_vector(fields, 76, 0.0F, 1.0F, 0.0F);
        write_vector(fields, 88, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 100, 4.0F);
        write_fixed(fields, 104, 2.0F);
        write_u32(fields, 136, 2); // activation sphere
        write_vector(fields, 140, 1.0F, 2.0F, 3.0F);
        write_fixed(fields, 152, 7.0F);
        const auto firespawn = fruityprime::enemy::decode_firespawn_profile(
            39, fields, {10.0F, 2.0F, 20.0F});
        require(firespawn.supported && firespawn.subtype == 1
                    && firespawn.health == 600
                    && firespawn.splash_damage == 0
                    && firespawn.contact_damage == 12
                    && firespawn.attack_count_min == 2
                    && firespawn.attack_count_max == 5
                    && firespawn.active_volume.center().x == 11.0F
                    && firespawn.projectile_draw_func == 21,
                "Fire Spawn S06 combat profile was not decoded");

        fields.fill(0);
        write_u32(fields, 0, 2); // Temroid hurt sphere
        write_vector(fields, 4, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 16, 1.0F);
        write_vector(fields, 92, 0.0F, 0.0F, 1.0F);
        write_vector(fields, 104, 1.0F, 2.0F, 3.0F);
        write_vector(fields, 116, 4.0F, 0.0F, 5.0F);
        const auto temroid = fruityprime::enemy::decode_temroid_profile(
            2, fields, {10.0F, 2.0F, 20.0F});
        require(temroid.supported && temroid.facing.z == 1.0F
                    && temroid.position_offset.x == 1.0F
                    && temroid.position_offset.y == 2.0F
                    && temroid.idle_range.x == 4.0F
                    && temroid.idle_range.z == 5.0F,
                "Temroid S03 movement fields were not decoded");

        fields.fill(0);
        write_u32(fields, 0, 2); // Petrasyl1 hurt sphere
        write_vector(fields, 4, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 16, 0.5F);
        write_vector(fields, 92, 1.0F, 0.0F, 0.0F);
        write_vector(fields, 104, 2.0F, 3.0F, 4.0F);
        write_vector(fields, 116, 5.0F, 0.0F, 6.0F);
        const auto petrasyl1 = fruityprime::enemy::decode_petrasyl_profile(
            3, fields, {10.0F, 2.0F, 20.0F});
        require(petrasyl1.supported && petrasyl1.variant == 3
                    && petrasyl1.facing.x == 1.0F
                    && petrasyl1.position_offset.y == 3.0F
                    && petrasyl1.idle_range.z == 6.0F,
                "Petrasyl1 S03 movement fields were not decoded");

        fields.fill(0);
        write_u32(fields, 0, 2); // Petrasyl3 hurt sphere
        write_vector(fields, 4, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 16, 0.5F);
        write_vector(fields, 80, 2.0F, 3.0F, 4.0F);
        write_fixed(fields, 92, 7.0F);
        write_fixed(fields, 96, 8.0F);
        const auto petrasyl3 = fruityprime::enemy::decode_petrasyl_profile(
            5, fields, {10.0F, 2.0F, 20.0F});
        require(petrasyl3.supported && petrasyl3.variant == 5
                    && petrasyl3.position_offset.x == 2.0F
                    && petrasyl3.weave_offset == 7.0F
                    && petrasyl3.vertical_range == 8.0F,
                "Petrasyl3 S04 movement fields were not decoded");

        fields.fill(0);
        fields[0] = 120;
        fields[2] = 22;
        write_u32(fields, 4, 37); // PlantCarnivarous_Branched
        write_u32(fields, 8, 2); // hurt sphere
        write_vector(fields, 12, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 24, 0.45F);
        const auto decoded_plant =
            fruityprime::enemy::decode_carnivorous_plant_profile(
                51, fields, {10.0F, 2.0F, 20.0F});
        require(decoded_plant.supported && decoded_plant.health == 120
                    && decoded_plant.damage == 22
                    && decoded_plant.subtype == 37
                    && decoded_plant.model_name
                        == "PlantCarnivarous_Branched"
                    && decoded_plant.hurt_volume.center().x == 10.0F
                    && decoded_plant.hurt_volume.center().y
                        == 2.0F + 409.0F / 4096.0F
                    && decoded_plant.hurt_volume.sphere_radius
                        == 1843.0F / 4096.0F,
                "Carnivorous Plant S07 fields were not decoded");

        fields.fill(0);
        write_u32(fields, 0, 2); // Shriekbat hurt sphere
        write_fixed(fields, 16, 1.0F);
        write_vector(fields, 64, 0.0F, -2.0F, 3.0F);
        write_u32(fields, 76, 2); // S02 range sphere
        write_vector(fields, 80, 4.0F, 0.0F, 0.0F);
        write_fixed(fields, 92, 8.0F);
        write_u32(fields, 140, 2); // S02 active sphere
        write_vector(fields, 144, 5.0F, 0.0F, 0.0F);
        write_fixed(fields, 156, 3.0F);
        const auto shriekbat = fruityprime::enemy::decode_shriekbat_profile(
            11, fields, {10.0F, 2.0F, 20.0F});
        require(shriekbat.supported
                    && shriekbat.path_vector.y == -2.0F
                    && shriekbat.path_vector.z == 3.0F
                    && shriekbat.range_volume.center().x == 14.0F
                    && shriekbat.active_volume.center().x == 15.0F
                    && shriekbat.active_volume.center().y == 2.0F,
                "Shriekbat S02 attack volumes were not decoded");

        fields.fill(0);
        write_u32(fields, 0, 1); // Enemy18 subtype: v1.4
        write_u32(fields, 4, 1); // Enemy18 version: Volt Driver
        write_u32(fields, 72, 2); // VolumeType.Sphere
        write_vector(fields, 76, 1.0F, 0.0F, 2.0F);
        write_fixed(fields, 88, 12.0F);
        const auto turret = fruityprime::enemy::decode_turret_profile(
            18, fields, {10.0F, 2.0F, 20.0F});
        require(turret.supported && turret.subtype == 1
                    && turret.version == 1 && turret.health == 80
                    && turret.beam_damage == 4 && turret.splash_damage == 2
                    && turret.min_shots == 3 && turret.max_shots == 5
                    && turret.range_volume.kind
                        == fruityprime::scene::VolumeKind::Sphere
                    && turret.range_volume.contains({16.0F, 2.0F, 22.0F})
                    && !turret.range_volume.contains({23.1F, 2.0F, 22.0F}),
                "Alimbic Turret S06 profile was not decoded");

        fields.fill(0);
        write_u32(fields, 0, 3); // Slench v4 subtype
        write_u32(fields, 4, 2); // Slench turret weapon version
        write_u32(fields, 8, 2); // S10 target sphere
        write_vector(fields, 12, 1.0F, 0.0F, 2.0F);
        write_fixed(fields, 24, 9.0F);
        write_u32(fields, 72, 2); // S10 local hurt sphere
        write_vector(fields, 76, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 88, 1.0F);
        write_u32(fields, 136, 2); // S10 synapse/turret index
        const auto slench_turret = fruityprime::enemy::decode_turret_profile(
            45, fields, {10.0F, 2.0F, 20.0F});
        require(slench_turret.supported && slench_turret.subtype == 3
                    && slench_turret.version == 2
                    && slench_turret.health == 30
                    && slench_turret.contact_damage == 5
                    && slench_turret.delay_frames == 250
                    && slench_turret.index == 2
                    && slench_turret.range_volume.center().x == 11.0F
                    && slench_turret.range_volume.center().z == 22.0F,
                "Slench turret S10 profile was not decoded");

        fields.fill(0);
        // Lesser Ithrak reads the four common S00 volumes directly.
        for (std::size_t i = 0; i < 4; ++i) {
            write_u32(fields, i * 64, 2); // sphere
            write_vector(fields, i * 64 + 4,
                         static_cast<float>(i), 0.0F, 0.0F);
            write_fixed(fields, i * 64 + 16, 3.0F);
        }
        const auto lesser_ithrak = fruityprime::enemy::decode_ithrak_profile(
            46, fields, {10.0F, 2.0F, 20.0F});
        require(lesser_ithrak.supported && lesser_ithrak.variant == 46
                    && lesser_ithrak.health == 85
                    && lesser_ithrak.contact_damage == 15
                    && lesser_ithrak.home_volume.center().x == 12.0F
                    && lesser_ithrak.range_volume.center().x == 11.0F
                    && lesser_ithrak.warn_volume.center().x == 13.0F,
                "Lesser Ithrak S00 volumes were not decoded");

        fields.fill(0);
        write_u32(fields, 0, 2); // S05 subtype prefix
        for (std::size_t i = 0; i < 4; ++i) {
            const std::size_t offset = 4 + i * 64;
            write_u32(fields, offset, 2); // sphere
            write_vector(fields, offset + 4,
                         static_cast<float>(i), 1.0F, 0.0F);
            write_fixed(fields, offset + 16, 4.0F);
        }
        const auto greater_ithrak = fruityprime::enemy::decode_ithrak_profile(
            47, fields, {10.0F, 2.0F, 20.0F});
        require(greater_ithrak.supported && greater_ithrak.variant == 47
                    && greater_ithrak.hurt_volume.center().x == 10.0F
                    && greater_ithrak.hurt_volume.center().y == 3.0F
                    && greater_ithrak.range_volume.center().x == 12.0F,
                "Greater Ithrak S05 volumes were not decoded");

        fields.fill(0);
        for (std::size_t i = 0; i < 2; ++i) {
            write_u32(fields, i * 64, 2); // sphere
            write_vector(fields, i * 64 + 4,
                         static_cast<float>(i + 1), 0.0F, 2.0F);
            write_fixed(fields, i * 64 + 16, 5.0F);
        }
        const auto quadtroid = fruityprime::enemy::decode_quadtroid_profile(
            37, fields, {10.0F, 2.0F, 20.0F});
        require(quadtroid.supported
                    && quadtroid.hurt_volume.center().x == 11.0F
                    && quadtroid.encounter_volume.center().x == 12.0F
                    && quadtroid.encounter_volume.contains({12.0F, 2.0F,
                                                            22.0F}),
                "Quadtroid S00 volumes were not decoded");

        fields.fill(0);
        for (std::size_t i = 0; i < 3; ++i) {
            write_u32(fields, i * 64, 2); // sphere
            write_vector(fields, i * 64 + 4,
                         static_cast<float>(i + 2), 1.0F, 3.0F);
            write_fixed(fields, i * 64 + 16, 4.0F);
        }
        const auto crash_pillar =
            fruityprime::enemy::decode_crash_pillar_profile(
                38, fields, {10.0F, 2.0F, 20.0F});
        require(crash_pillar.supported
                    && crash_pillar.hurt_volume.center().x == 12.0F
                    && crash_pillar.leash_volume.center().x == 13.0F
                    && crash_pillar.activation_volume.center().x == 14.0F
                    && crash_pillar.activation_volume.contains({14.0F, 3.0F,
                                                                  23.0F}),
                "CrashPillar S00 volumes were not decoded");

        fields.fill(0);
        write_u32(fields, 0, 3); // Cretaphid v4 subtype
        write_u32(fields, 4, 2); // S05 Volume0 hurt sphere
        write_vector(fields, 8, 1.0F, 0.0F, 2.0F);
        write_fixed(fields, 20, 2.0F);
        const auto cretaphid = fruityprime::enemy::decode_cretaphid_profile(
            19, fields, {10.0F, 2.0F, 20.0F});
        require(cretaphid.supported && cretaphid.subtype == 3
                    && cretaphid.crystal_health == 550
                    && cretaphid.eye_health == 4
                    && cretaphid.hurt_volume.center().x == 11.0F
                    && cretaphid.phases[0].crystal_health == 385
                    && cretaphid.phases[0].eye_state[7] == 4
                    && cretaphid.phases[1].eye_beam_type[3] == 2
                    && cretaphid.phases[2].eye_beam_cooldown[11] == 5
                    && cretaphid.phases[2].eye_state_timer2[11] == 260,
                "Cretaphid S05 profile was not decoded");

        fields.fill(0);
        write_u32(fields, 0, 2); // Slench S00 hurt sphere
        write_vector(fields, 4, 0.0F, 0.0F, 0.0F);
        write_fixed(fields, 16, 2.9F);
        const auto slench_v1 = fruityprime::enemy::decode_slench_profile(
            41, fields, {10.0F, 2.0F, 20.0F}, "UNIT1_B1");
        const auto slench_v2 = fruityprime::enemy::decode_slench_profile(
            41, fields, {10.0F, 2.0F, 20.0F}, "UNIT4_B1");
        const auto slench_v3 = fruityprime::enemy::decode_slench_profile(
            41, fields, {10.0F, 2.0F, 20.0F}, "UNIT2_B2");
        const auto slench_v4 = fruityprime::enemy::decode_slench_profile(
            41, fields, {10.0F, 2.0F, 20.0F}, "UNIT3_B2");
        require(slench_v1.supported && slench_v1.subtype == 0
                    && slench_v1.health == 200
                    && slench_v1.phases[0].static_shot_count == 1
                    && slench_v1.phases[2].roam_time == 300
                    && slench_v1.projectile_weapon == 0
                    && slench_v1.projectile_damage == 3
                    && slench_v2.subtype == 1 && slench_v2.health == 1200
                    && slench_v2.phases[2].static_shot_count == 3
                    && slench_v3.subtype == 2 && slench_v3.health == 900
                    && slench_v3.phases[0].slam_range == 10.0F
                    && slench_v4.subtype == 3 && slench_v4.health == 800
                    && slench_v4.phases[2].rolling_speed == 6.0F
                    && slench_v4.projectile_weapon == 5,
                "Slench room variant and phase profile was not decoded");

        const auto slench_shield =
            fruityprime::enemy::slench_shield_profile();
        const auto slench_nest = fruityprime::enemy::slench_nest_profile();
        const auto slench_synapse =
            fruityprime::enemy::slench_synapse_profile(3, 2);
        const auto invalid_synapse =
            fruityprime::enemy::slench_synapse_profile(4, 0);
        require(slench_shield.supported && slench_shield.health == 255
                    && slench_shield.hurt_radius == 1.0F
                    && slench_shield.follow_offset == 2.9F
                    && slench_nest.supported && slench_nest.health == 100
                    && slench_synapse.supported
                    && slench_synapse.phases[0].health == 90
                    && slench_synapse.phases[2].reappear_timer == 480
                    && slench_synapse.phases[2].collision_radius == 1.5F
                    && !invalid_synapse.supported,
                "Slench shield, nest, or synapse profile was not decoded");

        std::cout << "native enemy catalog tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
