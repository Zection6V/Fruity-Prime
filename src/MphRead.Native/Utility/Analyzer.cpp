#include "Utility/analyzer.hpp"

#include "Read.hpp"

#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fruityprime::utility::analyzer {
namespace {

constexpr std::uint32_t kFileOffset = 0x02000000;
constexpr std::uint32_t kElementList = 0x001242ac;
constexpr std::uint32_t kElapsedGlobal = 0x00123e00;
constexpr std::uint32_t kViewMatrix = 0x000da430;
constexpr std::uint32_t kEffectVector1 = 0x00123eec;
constexpr std::uint32_t kEffectVector2 = 0x00123e8c;
constexpr std::uint32_t kEffectVector3 = 0x00123e98;

[[nodiscard]] float round_three(float value) noexcept {
    return std::round(value * 1000.0F) / 1000.0F;
}

void write_scalar(std::ostream& output, float value) {
    output << std::setprecision(3) << std::defaultfloat << value;
}

[[nodiscard]] std::string marshal_name(
    const std::array<std::uint8_t, 32>& bytes) {
    std::size_t length = 0;
    while (length < bytes.size() && bytes[length] != 0) {
        ++length;
    }
    return {reinterpret_cast<const char*>(bytes.data()), length};
}

[[nodiscard]] std::uint32_t subtract_file_offset(std::int32_t address) {
    return static_cast<std::uint32_t>(address)
         - kFileOffset;
}

} // namespace

AnalysisResult read_thing(const AnalysisPaths& paths, std::ostream* output) {
    const std::vector<std::uint8_t> matrix_bytes = read::file(paths.matrix_dump);
    const formats::Matrix43Fx view_matrix =
        read::do_offset<formats::Matrix43Fx>(matrix_bytes, kViewMatrix);
    const formats::Vector3Fx effect_vector_1 =
        read::do_offset<formats::Vector3Fx>(matrix_bytes, kEffectVector1);
    const formats::Vector3Fx effect_vector_2 =
        read::do_offset<formats::Vector3Fx>(matrix_bytes, kEffectVector2);
    const formats::Vector3Fx effect_vector_3 =
        read::do_offset<formats::Vector3Fx>(matrix_bytes, kEffectVector3);

    const bool vectors_match =
        effect_vector_1.x.value == view_matrix.one.x.value
        && effect_vector_1.y.value == view_matrix.two.x.value
        && effect_vector_1.z.value == view_matrix.three.x.value
        && effect_vector_2.x.value == -view_matrix.one.y.value
        && effect_vector_2.y.value == -view_matrix.two.y.value
        && effect_vector_2.z.value == -view_matrix.three.y.value;

    const std::vector<std::uint8_t> bytes = read::file(paths.process_dump);
    const std::uint32_t elapsed_raw =
        read::span_read_u32_at(bytes, kElapsedGlobal);
    AnalysisResult result;
    result.elapsed_seconds = static_cast<float>(elapsed_raw) / 4096.0F;
    result.effect_vectors_match = vectors_match;

    const EffectElementEntry head =
        read::do_offset<EffectElementEntry>(bytes, kElementList);
    std::uint32_t offset = subtract_file_offset(head.prev);
    while (offset != kElementList) {
        EffectElementSnapshot snapshot;
        snapshot.offset = offset;
        snapshot.entry = read::do_offset<EffectElementEntry>(bytes, offset);
        const std::uint32_t element_offset =
            subtract_file_offset(snapshot.entry.element);
        snapshot.definition =
            read::do_offset<raw::RawEffectElement>(bytes, element_offset);

        std::uint32_t particle_offset =
            subtract_file_offset(snapshot.entry.particle_head.prev);
        const std::uint32_t particle_sentinel = offset + 0x1cc;
        while (particle_offset != particle_sentinel) {
            snapshot.particles.push_back(
                read::do_offset<EffectParticle>(bytes, particle_offset));
            particle_offset = subtract_file_offset(
                snapshot.particles.back().prev);
        }
        result.elements.push_back(std::move(snapshot));
        offset = subtract_file_offset(result.elements.back().entry.prev);
    }

    if (output != nullptr) {
        print_report(result, *output);
    }
    (void)effect_vector_3;
    return result;
}

void print_report(const AnalysisResult& result, std::ostream& output) {
    write_scalar(output, result.elapsed_seconds);
    output << '\n';
    for (const EffectElementSnapshot& snapshot : result.elements) {
        const float creation = round_three(
            snapshot.entry.creation_time / 4096.0F);
        const float expiration = round_three(
            snapshot.entry.expiration_time / 4096.0F);
        output << "0x" << std::uppercase << std::hex << snapshot.offset
               << std::nouppercase << std::dec << ' ';
        write_scalar(output, creation);
        output << " - ";
        write_scalar(output, expiration);
        output << " x" << snapshot.particles.size() << " ("
               << marshal_name(snapshot.definition.name) << ")\n -- ";
        for (std::size_t index = 0; index < snapshot.particles.size(); ++index) {
            if (index != 0) {
                output << ", ";
            }
            write_scalar(output, round_three(
                snapshot.particles[index].rotation.to_float()));
        }
        output << '\n';
    }

    output << '\n';
    for (const EffectElementSnapshot& snapshot : result.elements) {
        const formats::Vector3Fx& position = snapshot.entry.position;
        write_scalar(output, position.x.to_float());
        output << ", ";
        write_scalar(output, position.y.to_float());
        output << ", ";
        write_scalar(output, position.z.to_float());
        output << "\n\n";
        for (const EffectParticle& particle : snapshot.particles) {
            const float creation = particle.creation_time.to_float();
            const float expiration = particle.expiration_time.to_float();
            const float lifespan = expiration - creation;
            const float age = result.elapsed_seconds - creation;
            const float percent = lifespan == 0.0F
                ? 0.0F : age / lifespan;
            write_scalar(output, round_three(percent * 100.0F));
            output << "%\n";
            write_scalar(output, particle.position.x.to_float());
            output << ", ";
            write_scalar(output, particle.position.y.to_float());
            output << ", ";
            write_scalar(output, particle.position.z.to_float());
            output << '\n';
            write_scalar(output, particle.scale.to_float());
            output << '\n';
            write_scalar(output, particle.rotation.to_float());
            output << "\n\n";
        }
        output << "-----------------------------------\n\n";
    }
}

} // namespace fruityprime::utility::analyzer
