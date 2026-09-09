#pragma once

#include "Formats/fixed.hpp"
#include "Formats/raw_formats.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <vector>

namespace fruityprime::utility::analyzer {

// These are the fixed records read from the disassembler dumps by
// Utility/Analyzer.cs.  The pointer-shaped fields remain 32-bit values: they
// are addresses in the captured DS process, not native host pointers.
struct ParticleDefinition {
    std::int32_t model = 0;
    std::int32_t node = 0;
    std::int32_t material_id = 0;
};

struct EffectParticle {
    formats::Fixed creation_time;
    formats::Fixed expiration_time;
    formats::Fixed lifespan;
    formats::Vector3Fx position;
    formats::Vector3Fx speed;
    formats::Fixed scale;
    formats::Fixed rotation;
    formats::Fixed red;
    formats::Fixed green;
    formats::Fixed blue;
    formats::Fixed alpha;
    std::int32_t particle_id = 0;
    formats::Fixed portion_total;
    std::int32_t ro_field_1 = 0;
    std::int32_t ro_field_2 = 0;
    std::int32_t ro_field_3 = 0;
    std::int32_t ro_field_4 = 0;
    std::int32_t rw_field_1 = 0;
    std::int32_t rw_field_2 = 0;
    std::int32_t rw_field_3 = 0;
    std::int32_t rw_field_4 = 0;
    std::int32_t func_180 = 0;
    std::int32_t func_188 = 0;
    std::int32_t func_18c = 0;
    std::int32_t func_190 = 0;
    std::int32_t func_194 = 0;
    std::int32_t func_198 = 0;
    std::int32_t func_19c = 0;
    std::int32_t func_1b0 = 0;
    std::int32_t func_1b4 = 0;
    std::int32_t func_1b8 = 0;
    std::int32_t func_1bc = 0;
    std::int32_t prev = 0;
    std::int32_t next = 0;
};

struct EffectElementEntry {
    std::int32_t effect_entry = 0;
    std::int32_t effect_id = 0;
    std::int32_t matrix_pointer = 0;
    formats::Matrix43Fx transform;
    std::int32_t state = 0;
    std::int32_t element = 0;
    std::int32_t creation_time = 0;
    std::int32_t expiration_time = 0;
    std::int32_t drain_time = 0;
    std::int32_t buffer_time = 0;
    std::int32_t func_39_called = 0;
    std::int32_t field_58 = 0;
    std::int32_t field_5c = 0;
    std::int32_t field_60 = 0;
    std::int32_t field_64 = 0;
    std::int32_t flags = 0;
    std::int32_t child_effect = 0;
    std::int32_t particle_amount = 0;
    std::int32_t lifespan = 0;
    std::int32_t draw_type = 0;
    std::int32_t particle_count = 0;
    std::array<ParticleDefinition, 16> particles{};
    formats::Vector3Fx position;
    formats::Vector3Fx vector_1;
    formats::Vector3Fx vector_2;
    formats::Vector3Fx acceleration;
    std::array<std::int32_t, 20> functions{};
    std::int32_t set_vectors = 0;
    std::int32_t draw = 0;
    std::int32_t particle_free_pointer = 0;
    EffectParticle particle_head;
    std::int32_t prev = 0;
    std::int32_t next = 0;
};

static_assert(sizeof(ParticleDefinition) == 0x0c);
static_assert(sizeof(EffectParticle) == 0x98);
static_assert(sizeof(EffectElementEntry) == 0x26c);
static_assert(sizeof(raw::RawEffectElement) == 0x74);

struct AnalysisPaths {
    std::filesystem::path matrix_dump =
        "D:/Cdrv/MPH/Disassembly/mtx.bin";
    std::filesystem::path process_dump =
        "D:/Cdrv/MPH/Disassembly/dump.bin";
};

struct EffectElementSnapshot {
    std::uint32_t offset = 0;
    EffectElementEntry entry;
    raw::RawEffectElement definition;
    std::vector<EffectParticle> particles;
};

struct AnalysisResult {
    float elapsed_seconds = 0.0F;
    bool effect_vectors_match = false;
    std::vector<EffectElementSnapshot> elements;
};

[[nodiscard]] AnalysisResult read_thing(
    const AnalysisPaths& paths = {}, std::ostream* output = nullptr);

void print_report(const AnalysisResult& result, std::ostream& output);

} // namespace fruityprime::utility::analyzer
