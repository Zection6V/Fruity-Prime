#include "Utility/analyzer.hpp"

#include "Read.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <vector>

namespace {

template <typename T>
void write_record(std::vector<std::uint8_t>& bytes,
                  std::size_t offset, const T& value) {
    const auto* source = reinterpret_cast<const std::uint8_t*>(&value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        bytes[offset + index] = source[index];
    }
}

} // namespace

int main() {
    using namespace fruityprime::formats;
    using namespace fruityprime::utility::analyzer;

    constexpr std::uint32_t file_offset = 0x02000000;
    constexpr std::uint32_t element_list = 0x001242ac;
    constexpr std::uint32_t element_offset = 0x00004000;
    constexpr std::uint32_t particle_offset = 0x00003000;
    const auto temp = std::filesystem::temp_directory_path();
    const auto matrix_path = temp / "fruity_prime_analyzer_matrix.bin";
    const auto dump_path = temp / "fruity_prime_analyzer_dump.bin";

    std::vector<std::uint8_t> matrix(0x00124000, 0);
    Matrix43Fx view{};
    view.one = {{4096}, {8192}, {12288}};
    view.two = {{16384}, {20480}, {24576}};
    view.three = {{28672}, {32768}, {36864}};
    write_record(matrix, 0x000da430, view);
    const Vector3Fx vector_1 = {{4096}, {16384}, {28672}};
    const Vector3Fx vector_2 = {{-8192}, {-20480}, {-32768}};
    const Vector3Fx vector_3 = {{1}, {2}, {3}};
    write_record(matrix, 0x00123eec, vector_1);
    write_record(matrix, 0x00123e8c, vector_2);
    write_record(matrix, 0x00123e98, vector_3);
    assert(fruityprime::read::write_file(matrix_path, matrix));

    std::vector<std::uint8_t> dump(element_list + 0x100, 0);
    dump.resize(0x00130000, 0);
    dump[0x00123e00] = 0x00;
    dump[0x00123e01] = 0x18;
    EffectElementEntry head{};
    head.prev = static_cast<std::int32_t>(file_offset + 0x1000);
    write_record(dump, element_list, head);

    EffectElementEntry entry{};
    entry.creation_time = 4096;
    entry.expiration_time = 8192;
    entry.position = {{4096}, {8192}, {12288}};
    entry.element = static_cast<std::int32_t>(file_offset + element_offset);
    entry.prev = static_cast<std::int32_t>(file_offset + element_list);
    entry.particle_head.prev = static_cast<std::int32_t>(
        file_offset + particle_offset);
    write_record(dump, 0x1000, entry);

    fruityprime::raw::RawEffectElement definition{};
    definition.name[0] = 't';
    definition.name[1] = 'e';
    definition.name[2] = 's';
    definition.name[3] = 't';
    write_record(dump, element_offset, definition);

    EffectParticle particle{};
    particle.creation_time = {4096};
    particle.expiration_time = {8192};
    particle.position = {{4096}, {8192}, {12288}};
    particle.scale = {2048};
    particle.rotation = {1024};
    particle.prev = static_cast<std::int32_t>(file_offset + 0x1000 + 0x1cc);
    write_record(dump, particle_offset, particle);
    assert(fruityprime::read::write_file(dump_path, dump));

    AnalysisPaths paths{matrix_path, dump_path};
    std::ostringstream output;
    const AnalysisResult result = read_thing(paths, &output);
    assert(result.effect_vectors_match);
    assert(result.elapsed_seconds == 1.5F);
    assert(result.elements.size() == 1);
    assert(result.elements[0].offset == 0x1000);
    assert(result.elements[0].particles.size() == 1);
    assert(result.elements[0].definition.name[0] == 't');
    assert(result.elements[0].entry.position.x.value == 4096);
    assert(result.elements[0].particles[0].scale.value == 2048);
    assert(output.str().find("0x1000") != std::string::npos);
    assert(output.str().find("x1 (test)") != std::string::npos);
    assert(output.str().find("50%") != std::string::npos);

    std::error_code error;
    std::filesystem::remove(matrix_path, error);
    std::filesystem::remove(dump_path, error);
    return 0;
}
