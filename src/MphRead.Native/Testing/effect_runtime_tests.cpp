#include "Formats/effect_runtime.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    assert(offset + 4 <= bytes.size());
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
}

void put_i32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::int32_t value) {
    put_u32(bytes, offset, static_cast<std::uint32_t>(value));
}

void put_text(std::vector<std::uint8_t>& bytes, std::size_t offset,
              const std::string& value) {
    assert(offset + value.size() + 1 <= bytes.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value[index]);
    }
    bytes[offset + value.size()] = 0;
}

fruityprime::effects::File make_effect() {
    // Header 0, function table 28, element table 60, element 64.
    // The two function records use the same fixed-value function with
    // different parameters: one particle per eligible frame and scale 2.
    std::vector<std::uint8_t> bytes(224, 0);
    put_u32(bytes, 0, 0);       // field0
    put_u32(bytes, 4, 2);       // func_count
    put_u32(bytes, 8, 28);      // func_offset
    put_u32(bytes, 12, 0);      // count2
    put_u32(bytes, 16, 0);      // offset2
    put_u32(bytes, 20, 1);      // element_count
    put_u32(bytes, 24, 60);     // element_offset

    put_u32(bytes, 28, 44);     // amount function offset
    put_u32(bytes, 32, 52);     // scale function offset
    put_i32(bytes, 36, 4096);   // amount parameter = 1
    put_i32(bytes, 40, 8192);   // scale parameter = 2
    put_u32(bytes, 44, 42);     // amount function ID
    put_u32(bytes, 48, 36);     // amount parameter offset
    put_u32(bytes, 52, 42);     // scale function ID
    put_u32(bytes, 56, 40);     // scale parameter offset

    put_u32(bytes, 60, 64);     // element record offset
    const std::size_t element = 64;
    put_text(bytes, element, "runtime");
    put_text(bytes, element + 32, "runtimeModel");
    put_u32(bytes, element + 64, 1);   // particle_count
    put_u32(bytes, element + 68, 180); // particle table
    put_u32(bytes, element + 72, 0x2); // UseAcceleration
    put_i32(bytes, element + 76, 0);
    put_i32(bytes, element + 80, 4096); // acceleration y = 1
    put_i32(bytes, element + 84, 0);
    put_u32(bytes, element + 88, 0);   // child effect
    put_i32(bytes, element + 92, 4096); // lifespan = 1
    put_i32(bytes, element + 96, 0);
    put_i32(bytes, element + 100, 0);
    put_i32(bytes, element + 104, 1);  // draw type
    put_u32(bytes, element + 108, 2);  // two actions
    put_u32(bytes, element + 112, 196); // action table
    put_u32(bytes, 180, 184);
    put_text(bytes, 184, "particle");
    put_u32(bytes, 196, 14);  // IncreaseParticleAmount
    put_u32(bytes, 200, 44);
    put_u32(bytes, 204, 23);  // SetParticleScale
    put_u32(bytes, 208, 52);
    return fruityprime::effects::File::parse(bytes, 321, "runtime");
}

} // namespace

int main() {
    using fruityprime::effects::FunctionIds;
    using fruityprime::effects::Runtime;
    using fruityprime::effects::SpawnRequest;

    const FunctionIds billboard = fruityprime::effects::function_ids(
        static_cast<fruityprime::formats::EffElemFlags>(
            static_cast<std::uint32_t>(fruityprime::formats::EffElemFlags::UseTransform)),
        4);
    assert(billboard.set_vecs == 1 && billboard.draw == 4);

    const auto effect = make_effect();
    Runtime runtime;
    SpawnRequest request;
    request.instance_id = 7;
    request.effect_id = 321;
    request.transform.m41 = 1.0F;
    request.transform.m42 = 2.0F;
    request.transform.m43 = 3.0F;
    assert(runtime.spawn(effect, request, 0.0F));
    assert(runtime.has_instance(7));

    runtime.update(1.0F / 60.0F, 1.0F / 60.0F);
    assert(runtime.particle_count() == 0);
    runtime.update(1.0F / 60.0F, 2.0F / 60.0F);
    assert(runtime.particle_count() == 1);
    const auto& particle = runtime.instances().front().elements.front()
        .particles.front();
    assert(particle.scale == 2.0F);
    assert(particle.position.x == 1.0F);
    assert(particle.position.y > 2.0F);
    assert(particle.position.z == 3.0F);

    runtime.detach(7, true);
    assert(runtime.particle_count() == 1);
    runtime.update(1.0F, 1.1F);
    assert(runtime.particle_count() == 0);
    assert(!runtime.has_instance(7));
    return 0;
}
