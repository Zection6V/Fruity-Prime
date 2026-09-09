#pragma once

#include "Formats/fixed.hpp"
#include "Formats/raw_formats.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::utility {
class Rng;
}

namespace fruityprime::effects {

enum class FuncAction : std::uint32_t {
    SetParticleId = 9,
    IncreaseParticleAmount = 14,
    SetNewParticleSpeed = 15,
    SetNewParticlePosition = 16,
    SetNewParticleLifespan = 17,
    UpdateParticleSpeed = 18,
    SetParticleAlpha = 19,
    SetParticleRed = 20,
    SetParticleGreen = 21,
    SetParticleBlue = 22,
    SetParticleScale = 23,
    SetParticleRotation = 24,
    SetParticleRoField1 = 25,
    SetParticleRoField2 = 26,
    SetParticleRoField3 = 27,
    SetParticleRoField4 = 28,
    SetParticleRwField1 = 29,
    SetParticleRwField2 = 30,
    SetParticleRwField3 = 31,
    SetParticleRwField4 = 32
};

struct Function {
    std::uint32_t id = 0;
    std::vector<std::int32_t> parameters;
};

using FunctionTable = std::map<std::uint32_t, Function>;

struct Element {
    std::string name;
    std::string model_name;
    std::vector<std::string> particle_names;
    formats::EffElemFlags flags = formats::EffElemFlags::None;
    formats::Vector3 acceleration;
    std::uint32_t child_effect_id = 0;
    float lifespan = 0.0F;
    float drain_time = 0.0F;
    float buffer_time = 0.0F;
    std::int32_t draw_type = 0;

    // Keyed by the action ordinal from the cartridge.  Values are offsets in
    // File::functions(), matching the managed dictionary's pointer identity.
    std::map<std::uint32_t, std::uint32_t> actions;
};

class File {
public:
    [[nodiscard]] static File parse(std::span<const std::uint8_t> bytes,
                                    std::int32_t id = -1,
                                    std::string name = {});
    [[nodiscard]] static File read_file(const std::filesystem::path& path,
                                        std::int32_t id = -1,
                                        std::string name = {});

    [[nodiscard]] std::int32_t id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const raw::RawEffect& header() const noexcept {
        return header_;
    }
    [[nodiscard]] const FunctionTable& functions() const noexcept {
        return functions_;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& list2() const noexcept {
        return list2_;
    }
    [[nodiscard]] const std::vector<Element>& elements() const noexcept {
        return elements_;
    }

private:
    std::int32_t id_ = -1;
    std::string name_;
    raw::RawEffect header_{};
    FunctionTable functions_;
    std::vector<std::uint32_t> list2_;
    std::vector<Element> elements_;
};

// The managed Effects.cs evaluator treats the function table as a small
// expression graph.  Keep its time/state inputs explicit so the same decoder
// can be used by headless tests and by a renderer without depending on a
// particular entity class.
struct TimeValues {
    float global = 0.0F;
    float elapsed = 0.0F;
    float lifespan = 0.0F;
};

struct EvaluationState {
    formats::Vector3 position;
    formats::Vector3 speed;
    formats::Vector3 transform_position;
    float portion_total = 0.0F;
    float creation_time = 0.0F;
    float owner_lifespan = 0.0F;
    float alpha = 1.0F;
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
    float scale = 0.0F;
    float rotation = 0.0F;
    std::array<float, 4> read_only_fields{};
    std::array<float, 4> read_write_fields{};
    bool element_context = false;
};

class Evaluator final {
public:
    Evaluator(const FunctionTable& functions, utility::Rng& rng) noexcept;
    Evaluator(const File& file, utility::Rng& rng) noexcept;

    // Returns false for an unknown function, a bad reference, or a malformed
    // parameter list.  The managed implementation throws in those cases;
    // native render paths use false to retain their visual fallback.
    [[nodiscard]] bool evaluate_float(std::uint32_t function_offset,
                                      TimeValues times,
                                      const EvaluationState& state,
                                      float& result) noexcept;
    [[nodiscard]] bool evaluate_vector(std::uint32_t function_offset,
                                       TimeValues times,
                                       const EvaluationState& state,
                                       formats::Vector3& result) noexcept;

private:
    [[nodiscard]] bool evaluate_float_at(std::uint32_t function_offset,
                                         TimeValues times,
                                         const EvaluationState& state,
                                         float& result,
                                         std::size_t depth) noexcept;
    [[nodiscard]] bool evaluate_float_function(
        std::uint32_t function_id, std::span<const std::int32_t> parameters,
        TimeValues times, const EvaluationState& state, float& result,
        std::size_t depth) noexcept;
    [[nodiscard]] bool evaluate_vector_at(std::uint32_t function_offset,
                                          TimeValues times,
                                          const EvaluationState& state,
                                          formats::Vector3& result,
                                          std::size_t depth) noexcept;
    [[nodiscard]] bool evaluate_vector_function(
        std::uint32_t function_id, std::span<const std::int32_t> parameters,
        TimeValues times, const EvaluationState& state,
        formats::Vector3& result, std::size_t depth) noexcept;
    [[nodiscard]] const Function* function_at_or_after(
        std::uint32_t offset) const noexcept;

    const FunctionTable* functions_ = nullptr;
    utility::Rng* rng_ = nullptr;
};

} // namespace fruityprime::effects

namespace MphReadNative {
namespace Effects = ::fruityprime::effects;
}
