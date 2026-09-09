#include "Formats/effect_funcs.hpp"
#include "Formats/effect_records.hpp"
#include "Formats/single_particle.hpp"
#include "Formats/effects.hpp"

#include "Formats/raw_formats.hpp"
#include "Read.hpp"
#include "Utility/rng.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fruityprime::effects {
namespace {

using Bytes = std::span<const std::uint8_t>;

[[noreturn]] void invalid(const std::string& message) {
    throw std::runtime_error("invalid effect data: " + message);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        invalid(message);
    }
}

[[nodiscard]] std::uint32_t read_u32(Bytes bytes, std::size_t offset) {
    return read::read_struct<std::uint32_t>(bytes, offset);
}

[[nodiscard]] std::int32_t read_i32(Bytes bytes, std::size_t offset) {
    return read::read_struct<std::int32_t>(bytes, offset);
}

[[nodiscard]] std::string fixed_name(const std::uint8_t* data,
                                     std::size_t size) {
    std::size_t length = 0;
    while (length < size && data[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(data), length);
}

[[nodiscard]] std::size_t checked_table_size(std::uint32_t count,
                                             std::size_t record_size,
                                             std::string_view what) {
    if (count > std::numeric_limits<std::size_t>::max() / record_size) {
        invalid(std::string(what) + " count overflows the host size");
    }
    return static_cast<std::size_t>(count) * record_size;
}

} // namespace

File File::parse(std::span<const std::uint8_t> bytes, std::int32_t id,
                 std::string name) {
    File result;
    result.id_ = id;
    result.name_ = std::move(name);
    result.header_ = read::read_struct<raw::RawEffect>(bytes);

    const auto function_table_size = checked_table_size(
        result.header_.func_count, sizeof(std::uint32_t), "effect function");
    const auto list2_table_size = checked_table_size(
        result.header_.count2, sizeof(std::uint32_t), "effect list2");
    const auto element_table_size = checked_table_size(
        result.header_.element_count, sizeof(std::uint32_t), "effect element");
    if (result.header_.func_count != 0) {
        static_cast<void>(read::read_struct<std::uint8_t>(
            bytes, result.header_.func_offset + function_table_size - 1));
    }
    if (result.header_.count2 != 0) {
        static_cast<void>(read::read_struct<std::uint8_t>(
            bytes, result.header_.offset2 + list2_table_size - 1));
    }
    if (result.header_.element_count != 0) {
        static_cast<void>(read::read_struct<std::uint8_t>(
            bytes, result.header_.element_offset + element_table_size - 1));
    }

    for (std::uint32_t index = 0; index < result.header_.func_count; ++index) {
        const std::size_t table_offset = static_cast<std::size_t>(
            result.header_.func_offset) + index * sizeof(std::uint32_t);
        const std::uint32_t function_offset = read_u32(bytes, table_offset);
        require(function_offset <= bytes.size()
                    && sizeof(std::uint32_t) * 2
                        <= bytes.size() - function_offset,
                "effect function offset is invalid");
        const std::uint32_t function_id = read_u32(bytes, function_offset);
        const std::uint32_t parameter_offset = read_u32(
            bytes, static_cast<std::size_t>(function_offset) + 4);
        Function function;
        function.id = function_id;
        if (parameter_offset != 0) {
            require(parameter_offset < function_offset,
                    "effect parameter offset is not before its function");
            const std::uint32_t distance = function_offset - parameter_offset;
            require(distance % sizeof(std::int32_t) == 0,
                    "effect parameter table is not aligned");
            const std::size_t count = distance / sizeof(std::int32_t);
            function.parameters.reserve(count);
            for (std::size_t parameter = 0; parameter < count; ++parameter) {
                function.parameters.push_back(read_i32(
                    bytes, static_cast<std::size_t>(parameter_offset)
                        + parameter * sizeof(std::int32_t)));
            }
        }
        const auto [_, inserted] = result.functions_.emplace(
            function_offset, std::move(function));
        require(inserted, "effect function offsets contain a duplicate");
    }

    result.list2_.reserve(result.header_.count2);
    for (std::uint32_t index = 0; index < result.header_.count2; ++index) {
        result.list2_.push_back(read_u32(bytes,
            static_cast<std::size_t>(result.header_.offset2)
                + index * sizeof(std::uint32_t)));
    }

    result.elements_.reserve(result.header_.element_count);
    for (std::uint32_t index = 0; index < result.header_.element_count; ++index) {
        const std::uint32_t element_offset = read_u32(
            bytes, static_cast<std::size_t>(result.header_.element_offset)
                + index * sizeof(std::uint32_t));
        const auto raw_element = read::read_struct<raw::RawEffectElement>(
            bytes, element_offset);
        Element element;
        element.name = fixed_name(raw_element.name.data(),
                                  raw_element.name.size());
        element.model_name = fixed_name(raw_element.model_name.data(),
                                        raw_element.model_name.size());
        element.flags = raw_element.flags;
        element.acceleration = {
            raw_element.acceleration.x.to_float(),
            raw_element.acceleration.y.to_float(),
            raw_element.acceleration.z.to_float()
        };
        element.child_effect_id = raw_element.child_effect_id;
        element.lifespan = raw_element.lifespan.to_float();
        element.drain_time = raw_element.drain_time.to_float();
        element.buffer_time = raw_element.buffer_time.to_float();
        element.draw_type = raw_element.draw_type;

        const auto particle_table_size = checked_table_size(
            raw_element.particle_count, sizeof(std::uint32_t),
            "effect particle");
        if (raw_element.particle_count != 0) {
            static_cast<void>(read::read_struct<std::uint8_t>(
                bytes, raw_element.particle_offset + particle_table_size - 1));
        }
        element.particle_names.reserve(raw_element.particle_count);
        for (std::uint32_t particle = 0;
             particle < raw_element.particle_count; ++particle) {
            const std::uint32_t particle_offset = read_u32(
                bytes, static_cast<std::size_t>(raw_element.particle_offset)
                    + particle * sizeof(std::uint32_t));
            element.particle_names.push_back(read::string(bytes,
                                                           particle_offset, 16));
        }

        const auto action_table_size = checked_table_size(
            raw_element.func_count, sizeof(std::uint32_t) * 2,
            "effect action");
        if (raw_element.func_count != 0) {
            static_cast<void>(read::read_struct<std::uint8_t>(
                bytes, raw_element.func_offset + action_table_size - 1));
        }
        for (std::uint32_t action = 0; action < raw_element.func_count;
             ++action) {
            const std::size_t offset = static_cast<std::size_t>(
                raw_element.func_offset)
                + static_cast<std::size_t>(action) * sizeof(std::uint32_t) * 2;
            const std::uint32_t action_id = read_u32(bytes, offset);
            const std::uint32_t function_offset = read_u32(bytes, offset + 4);
            if (function_offset == 0) {
                continue;
            }
            require(result.functions_.contains(function_offset),
                    "effect action references an unknown function");
            const auto [_, inserted] = element.actions.emplace(
                action_id, function_offset);
            require(inserted, "effect element has a duplicate action");
        }
        result.elements_.push_back(std::move(element));
    }
    return result;
}

File File::read_file(const std::filesystem::path& path, std::int32_t id,
                     std::string name) {
    const auto bytes = read::file(path);
    if (name.empty()) {
        name = path.filename().string();
    }
    return parse(bytes, id, std::move(name));
}

namespace {

constexpr std::size_t MaxEvaluationDepth = 64;
constexpr float Pi = 3.14159265358979323846F;
constexpr std::int32_t Sentinel = std::numeric_limits<std::int32_t>::min();

[[nodiscard]] float fixed_to_float(std::int32_t value) noexcept {
    return static_cast<float>(value) / 4096.0F;
}

[[nodiscard]] bool parameter_at(
    std::span<const std::int32_t> parameters, std::size_t index,
    std::int32_t& result) noexcept {
    if (index >= parameters.size()) {
        return false;
    }
    result = parameters[index];
    return true;
}

[[nodiscard]] bool offset_at(
    std::span<const std::int32_t> parameters, std::size_t index,
    std::uint32_t& result) noexcept {
    std::int32_t value = 0;
    if (!parameter_at(parameters, index, value) || value < 0) {
        return false;
    }
    result = static_cast<std::uint32_t>(value);
    return true;
}

[[nodiscard]] float random_fixed(utility::Rng& rng,
                                 std::uint32_t range) noexcept {
    return static_cast<float>(rng.random1(range)) / 4096.0F;
}

} // namespace

Evaluator::Evaluator(const FunctionTable& functions, utility::Rng& rng) noexcept
    : functions_(&functions), rng_(&rng) {}

Evaluator::Evaluator(const File& file, utility::Rng& rng) noexcept
    : Evaluator(file.functions(), rng) {}

const Function* Evaluator::function_at_or_after(
    std::uint32_t offset) const noexcept {
    if (functions_ == nullptr) {
        return nullptr;
    }
    // FxFunc13 follows the managed implementation's four-byte offset probe;
    // malformed resources must not turn that probe into an unbounded loop.
    for (std::size_t probe = 0; probe <= 256; ++probe) {
        if (offset > std::numeric_limits<std::uint32_t>::max() - probe * 4u) {
            break;
        }
        const auto found = functions_->find(offset + probe * 4u);
        if (found != functions_->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

bool Evaluator::evaluate_float(std::uint32_t function_offset,
                               TimeValues times,
                               const EvaluationState& state,
                               float& result) noexcept {
    result = 0.0F;
    return evaluate_float_at(function_offset, times, state, result, 0);
}

bool Evaluator::evaluate_vector(std::uint32_t function_offset,
                                TimeValues times,
                                const EvaluationState& state,
                                formats::Vector3& result) noexcept {
    result = {};
    return evaluate_vector_at(function_offset, times, state, result, 0);
}

bool Evaluator::evaluate_float_at(std::uint32_t function_offset,
                                  TimeValues times,
                                  const EvaluationState& state,
                                  float& result,
                                  std::size_t depth) noexcept {
    if (functions_ == nullptr || depth > MaxEvaluationDepth) {
        return false;
    }
    const auto found = functions_->find(function_offset);
    if (found == functions_->end()) {
        return false;
    }
    const auto& function = found->second;
    return evaluate_float_function(
        function.id, function.parameters, times, state, result, depth + 1);
}

bool Evaluator::evaluate_vector_at(std::uint32_t function_offset,
                                   TimeValues times,
                                   const EvaluationState& state,
                                   formats::Vector3& result,
                                   std::size_t depth) noexcept {
    if (functions_ == nullptr || depth > MaxEvaluationDepth) {
        return false;
    }
    const auto found = functions_->find(function_offset);
    if (found == functions_->end()) {
        return false;
    }
    const auto& function = found->second;
    return evaluate_vector_function(
        function.id, function.parameters, times, state, result, depth + 1);
}

bool Evaluator::evaluate_vector_function(
    std::uint32_t function_id, std::span<const std::int32_t> parameters,
    TimeValues times, const EvaluationState& state,
    formats::Vector3& result, std::size_t depth) noexcept {
    if (depth > MaxEvaluationDepth || rng_ == nullptr) {
        return false;
    }
    switch (function_id) {
    case 1:
    case 2:
        result = state.element_context ? state.transform_position
                                       : state.position;
        return true;
    case 3:
        if (state.element_context) {
            return false;
        }
        result = state.speed;
        return true;
    case 4: {
        if (parameters.size() < 3) {
            return false;
        }
        result = {fixed_to_float(parameters[0]), fixed_to_float(parameters[1]),
                  fixed_to_float(parameters[2])};
        return true;
    }
    case 5:
        result = {random_fixed(*rng_, 4096), random_fixed(*rng_, 4096),
                  random_fixed(*rng_, 4096)};
        return true;
    case 6:
        result = {random_fixed(*rng_, 4096), 0.0F,
                  random_fixed(*rng_, 4096)};
        return true;
    case 7:
        result = {random_fixed(*rng_, 4096), 1.0F,
                  random_fixed(*rng_, 4096)};
        return true;
    case 8:
        result = {random_fixed(*rng_, 4096) - 0.5F,
                  random_fixed(*rng_, 4096) - 0.5F,
                  random_fixed(*rng_, 4096) - 0.5F};
        return true;
    case 9:
        result = {random_fixed(*rng_, 4096) - 0.5F, 0.0F,
                  random_fixed(*rng_, 4096) - 0.5F};
        return true;
    case 10:
        result = {random_fixed(*rng_, 4096) - 0.5F, 1.0F,
                  random_fixed(*rng_, 4096) - 0.5F};
        return true;
    case 11:
        if (state.element_context) {
            return false;
        }
        {
            const float angle = 2.0F * Pi * state.portion_total;
            result = {std::sin(angle), 0.0F, std::cos(angle)};
        }
        return true;
    case 13: {
        std::int32_t function_id_value = 0;
        std::uint32_t parameter_offset = 0;
        if (!parameter_at(parameters, 0, function_id_value)
            || function_id_value < 0
            || !offset_at(parameters, 1, parameter_offset)) {
            return false;
        }
        const auto* function = function_at_or_after(parameter_offset);
        if (function == nullptr) {
            return false;
        }
        float value = 0.0F;
        if (!evaluate_float_function(
                static_cast<std::uint32_t>(function_id_value),
                function->parameters, times, state, value, depth + 1)
            || std::abs(value) <= std::numeric_limits<float>::epsilon()) {
            return false;
        }
        float percent = times.elapsed / value;
        if (value < 0.0F) {
            percent *= -1.0F;
        }
        const float angle = 2.0F * Pi * percent;
        result = {std::sin(angle), 0.0F, std::cos(angle)};
        return true;
    }
    case 14: {
        std::uint32_t vector_offset = 0;
        std::uint32_t scalar_offset = 0;
        if (!offset_at(parameters, 0, vector_offset)
            || !offset_at(parameters, 1, scalar_offset)) {
            return false;
        }
        formats::Vector3 temp;
        float value = 0.0F;
        if (!evaluate_vector_at(vector_offset, times, state, temp, depth + 1)
            || !evaluate_float_at(scalar_offset, times, state, value,
                                  depth + 1)
            || std::abs(value) <= std::numeric_limits<float>::epsilon()) {
            return false;
        }
        float divisor = times.elapsed / value;
        if (value < 0.0F) {
            divisor *= -1.0F;
        }
        result = temp * divisor;
        return true;
    }
    case 15: {
        std::uint32_t first_offset = 0;
        std::uint32_t second_offset = 0;
        if (!offset_at(parameters, 0, first_offset)
            || !offset_at(parameters, 1, second_offset)) {
            return false;
        }
        float first = 0.0F;
        float second = 0.0F;
        if (!evaluate_float_at(first_offset, times, state, first, depth + 1)
            || !evaluate_float_at(second_offset, times, state, second,
                                  depth + 1)) {
            return false;
        }
        const float angle = static_cast<float>(rng_->random1(0xffffU) >> 4)
            * (360.0F / 4096.0F) * Pi / 180.0F;
        result = {std::sin(angle) * first, second,
                  std::cos(angle) * first};
        return true;
    }
    case 16: {
        std::uint32_t first_offset = 0;
        std::uint32_t second_offset = 0;
        if (!offset_at(parameters, 0, first_offset)
            || !offset_at(parameters, 1, second_offset)) {
            return false;
        }
        float first = 0.0F;
        float second = 0.0F;
        if (!evaluate_float_at(first_offset, times, state, first, depth + 1)
            || !evaluate_float_at(second_offset, times, state, second,
                                  depth + 1)) {
            return false;
        }
        result = {(random_fixed(*rng_, 4096) - 0.5F) * first, 0.0F,
                  (random_fixed(*rng_, 4096) - 0.5F) * second};
        return true;
    }
    case 17:
    case 18:
    case 19: {
        std::uint32_t first_offset = 0;
        std::uint32_t second_offset = 0;
        if (!offset_at(parameters, 0, first_offset)
            || !offset_at(parameters, 1, second_offset)) {
            return false;
        }
        formats::Vector3 first;
        formats::Vector3 second;
        if (!evaluate_vector_at(first_offset, times, state, first, depth + 1)
            || !evaluate_vector_at(second_offset, times, state, second,
                                   depth + 1)) {
            return false;
        }
        if (function_id == 17) {
            result = first + second;
        } else if (function_id == 18) {
            result = first - second;
        } else {
            result = {first.x * second.x, first.y * second.y,
                      first.z * second.z};
        }
        return true;
    }
    case 20: {
        std::uint32_t scalar_offset = 0;
        std::uint32_t vector_offset = 0;
        if (!offset_at(parameters, 0, scalar_offset)
            || !offset_at(parameters, 1, vector_offset)) {
            return false;
        }
        float scalar = 0.0F;
        formats::Vector3 vector;
        if (!evaluate_float_at(scalar_offset, times, state, scalar, depth + 1)
            || !evaluate_vector_at(vector_offset, times, state, vector,
                                   depth + 1)) {
            return false;
        }
        result = vector * scalar;
        return true;
    }
    default:
        return false;
    }
}

bool Evaluator::evaluate_float_function(
    std::uint32_t function_id, std::span<const std::int32_t> parameters,
    TimeValues times, const EvaluationState& state, float& result,
    std::size_t depth) noexcept {
    if (depth > MaxEvaluationDepth || rng_ == nullptr) {
        return false;
    }
    switch (function_id) {
    case 21:
    case 28:
        result = 0.0F;
        return true;
    case 22:
        result = state.owner_lifespan;
        return true;
    case 23:
        result = times.global - state.creation_time;
        return true;
    case 24:
        result = state.alpha;
        return true;
    case 25:
        result = state.red;
        return true;
    case 26:
        result = state.green;
        return true;
    case 27:
        result = state.blue;
        return true;
    case 29:
        result = state.scale;
        return true;
    case 30:
        result = state.rotation;
        return true;
    case 31:
    case 32:
    case 33:
    case 34:
        result = state.read_only_fields[function_id - 31];
        return true;
    case 35:
    case 36:
    case 37:
    case 38:
        result = state.read_write_fields[function_id - 35];
        return true;
    case 40: {
        if (parameters.size() < 3
            || std::abs(times.lifespan)
                <= std::numeric_limits<float>::epsilon()) {
            return false;
        }
        result = times.elapsed / times.lifespan <= fixed_to_float(parameters[0])
            ? fixed_to_float(parameters[1]) : fixed_to_float(parameters[2]);
        return true;
    }
    case 41: {
        if (parameters.size() < 2
            || std::abs(times.lifespan)
                <= std::numeric_limits<float>::epsilon()) {
            return false;
        }
        const float percent = times.elapsed / times.lifespan;
        if (percent < fixed_to_float(parameters[0])) {
            result = fixed_to_float(parameters[1]);
            return true;
        }
        bool found = false;
        std::size_t last = 0;
        for (std::size_t index = 0; index + 1 < parameters.size();
             index += 2) {
            if (parameters[index] == Sentinel) {
                break;
            }
            if (fixed_to_float(parameters[index]) > percent) {
                break;
            }
            found = true;
            last = index;
            if (index + 2 >= parameters.size()
                || parameters[index + 2] == Sentinel) {
                break;
            }
        }
        if (!found) {
            result = 0.0F;
            return true;
        }
        if (last + 3 >= parameters.size()
            || parameters[last + 2] == Sentinel) {
            result = fixed_to_float(parameters[last + 1]);
            return true;
        }
        const float start = fixed_to_float(parameters[last]);
        const float end = fixed_to_float(parameters[last + 2]);
        if (std::abs(end - start) <= std::numeric_limits<float>::epsilon()) {
            result = fixed_to_float(parameters[last + 1]);
            return true;
        }
        result = fixed_to_float(parameters[last + 1])
            + (fixed_to_float(parameters[last + 3])
               - fixed_to_float(parameters[last + 1]))
                * ((percent - start) / (end - start));
        return true;
    }
    case 42:
        if (parameters.empty()) {
            return false;
        }
        result = fixed_to_float(parameters[0]);
        return true;
    case 43:
        result = random_fixed(*rng_, 4096);
        return true;
    case 44:
        result = random_fixed(*rng_, 4096) - 0.5F;
        return true;
    case 45:
        result = random_fixed(*rng_, 0x168000U);
        return true;
    case 46:
    case 47:
    case 48: {
        std::uint32_t first_offset = 0;
        std::uint32_t second_offset = 0;
        if (!offset_at(parameters, 0, first_offset)
            || !offset_at(parameters, 1, second_offset)) {
            return false;
        }
        float first = 0.0F;
        float second = 0.0F;
        if (!evaluate_float_at(first_offset, times, state, first, depth + 1)
            || !evaluate_float_at(second_offset, times, state, second,
                                  depth + 1)) {
            return false;
        }
        if (function_id == 46) {
            result = first + second;
        } else if (function_id == 47) {
            result = first - second;
        } else {
            result = first * second;
        }
        return true;
    }
    case 49: {
        std::array<std::uint32_t, 4> offsets{};
        for (std::size_t index = 0; index < offsets.size(); ++index) {
            if (!offset_at(parameters, index, offsets[index])) {
                return false;
            }
        }
        float left = 0.0F;
        float right = 0.0F;
        if (!evaluate_float_at(offsets[0], times, state, left, depth + 1)
            || !evaluate_float_at(offsets[1], times, state, right,
                                  depth + 1)) {
            return false;
        }
        const std::size_t chosen = left >= right ? 2 : 3;
        return evaluate_float_at(offsets[chosen], times, state, result,
                                 depth + 1);
    }
    default:
        return false;
    }
}

} // namespace fruityprime::effects
