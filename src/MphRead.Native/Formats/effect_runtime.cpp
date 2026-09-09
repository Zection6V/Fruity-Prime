#include "Formats/effect_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace fruityprime::effects {
namespace {

constexpr float FixedFrameSeconds = 1.0F / 60.0F;
constexpr std::size_t MaxParticleCount = 200;
constexpr std::size_t MaxEffectCount = 64;
constexpr std::size_t MaxElementCount = 96;

[[nodiscard]] bool has_flag(formats::EffElemFlags value,
                             formats::EffElemFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value)
            & static_cast<std::uint32_t>(flag)) != 0;
}

[[nodiscard]] formats::Vector3 translation(
    const formats::Matrix4& matrix) noexcept {
    return {matrix.m41, matrix.m42, matrix.m43};
}

[[nodiscard]] formats::Vector3 safe_normalized(formats::Vector3 value,
                                                formats::Vector3 fallback) {
    if (value.length_squared() <= std::numeric_limits<float>::epsilon()) {
        return fallback;
    }
    return value.normalized();
}

[[nodiscard]] float fixed_to_float(std::int32_t value) noexcept {
    return static_cast<float>(value) / 4096.0F;
}

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

} // namespace

FunctionIds function_ids(formats::EffElemFlags flags,
                         std::int32_t draw_type) noexcept {
    FunctionIds result;
    if (has_flag(flags, formats::EffElemFlags::UseMesh)) {
        result.draw = 7;
        result.set_vecs = draw_type == 3 ? 4 : 5;
        return result;
    }

    const bool alternate = has_flag(flags, formats::EffElemFlags::UseTransform);
    switch (draw_type) {
    case 1:
        result.set_vecs = 1;
        result.draw = alternate ? 1 : 2;
        break;
    case 2:
        result.set_vecs = 2;
        result.draw = alternate ? 1 : 2;
        break;
    case 3:
        result.set_vecs = 3;
        result.draw = 3;
        break;
    case 4:
        result.set_vecs = 1;
        result.draw = alternate ? 4 : 5;
        break;
    case 5:
        result.set_vecs = 2;
        result.draw = alternate ? 4 : 5;
        break;
    case 6:
        result.set_vecs = 3;
        result.draw = 6;
        break;
    default:
        break;
    }
    return result;
}

Runtime::Runtime(EffectResolver resolver, utility::Rng rng)
    : resolver_(std::move(resolver)), rng_(rng) {
    instances_.reserve(MaxEffectCount);
}

const Function* Runtime::action_function(const ElementState& element,
                                         FuncAction action) const noexcept {
    if (element.file == nullptr || element.definition == nullptr) {
        return nullptr;
    }
    const auto action_it = element.definition->actions.find(
        static_cast<std::uint32_t>(action));
    if (action_it == element.definition->actions.end()) {
        return nullptr;
    }
    const auto function_it = element.file->functions().find(action_it->second);
    return function_it == element.file->functions().end()
        ? nullptr : &function_it->second;
}

std::optional<std::uint32_t> Runtime::action_offset(
    const ElementState& element, FuncAction action) const noexcept {
    if (element.definition == nullptr) {
        return std::nullopt;
    }
    const auto found = element.definition->actions.find(
        static_cast<std::uint32_t>(action));
    if (found == element.definition->actions.end()) {
        return std::nullopt;
    }
    return found->second;
}

EvaluationState Runtime::element_evaluation_state(
    const ElementState& element) const noexcept {
    EvaluationState state;
    state.position = translation(element.transform);
    state.transform_position = state.position;
    state.creation_time = element.creation_time;
    state.owner_lifespan = element.lifespan;
    state.read_only_fields = element.read_only_fields;
    state.element_context = true;
    return state;
}

EvaluationState Runtime::particle_evaluation_state(
    const ElementState& element, const ParticleState& particle,
    bool use_element_creation_time) const noexcept {
    EvaluationState state;
    state.position = particle.position;
    state.speed = particle.speed;
    state.transform_position = translation(element.transform);
    state.portion_total = particle.portion_total;
    state.creation_time = use_element_creation_time
        ? element.creation_time : particle.creation_time;
    state.owner_lifespan = element.lifespan;
    state.alpha = particle.alpha;
    state.red = particle.red;
    state.green = particle.green;
    state.blue = particle.blue;
    state.scale = particle.scale;
    state.rotation = particle.rotation;
    state.read_only_fields = particle.read_only_fields;
    state.read_write_fields = particle.read_write_fields;
    return state;
}

bool Runtime::evaluate_element_float(ElementState& element,
                                     std::uint32_t function_offset,
                                     TimeValues times, float& result) noexcept {
    if (element.file == nullptr) {
        return false;
    }
    const auto found = element.file->functions().find(function_offset);
    if (found == element.file->functions().end()) {
        return false;
    }
    if (found->second.id == 39) {
        if (element.function39_called) {
            result = 0.0F;
            return true;
        }
        if (found->second.parameters.empty()) {
            return false;
        }
        element.function39_called = true;
        result = fixed_to_float(found->second.parameters.front());
        return true;
    }
    Evaluator evaluator(*element.file, rng_);
    return evaluator.evaluate_float(function_offset, times,
                                    element_evaluation_state(element), result);
}

bool Runtime::evaluate_particle_float(
    const ElementState& element, const ParticleState& particle,
    std::uint32_t function_offset, TimeValues times, float& result,
    bool use_element_creation_time) noexcept {
    if (element.file == nullptr) {
        return false;
    }
    const auto found = element.file->functions().find(function_offset);
    if (found == element.file->functions().end() || found->second.id == 39) {
        // FxFunc39 is owned by the element in the managed implementation and
        // is handled during element-side evaluation, never as a per-particle
        // expression.
        return false;
    }
    Evaluator evaluator(*element.file, rng_);
    return evaluator.evaluate_float(
        function_offset, times,
        particle_evaluation_state(element, particle,
                                  use_element_creation_time), result);
}

bool Runtime::evaluate_particle_vector(
    const ElementState& element, const ParticleState& particle,
    std::uint32_t function_offset, TimeValues times, formats::Vector3& result,
    bool use_element_creation_time) noexcept {
    if (element.file == nullptr) {
        return false;
    }
    Evaluator evaluator(*element.file, rng_);
    return evaluator.evaluate_vector(
        function_offset, times,
        particle_evaluation_state(element, particle,
                                  use_element_creation_time), result);
}

bool Runtime::spawn(const File& file, SpawnRequest request, float global_time) {
    if (file.elements().empty() || instances_.size() >= MaxEffectCount
        || file.elements().size() > MaxElementCount) {
        return false;
    }
    if (request.instance_id == 0) {
        request.instance_id = next_instance_id_++;
    }
    if (has_instance(request.instance_id)) {
        return false;
    }
    if (request.effect_id == 0) {
        request.effect_id = file.id() > 0
            ? static_cast<std::uint16_t>(file.id()) : 0;
    }

    InstanceState instance;
    instance.id = request.instance_id;
    instance.effect_id = request.effect_id;
    instance.source_entity_id = request.source_entity_id;
    instance.file = &file;
    instance.transform = request.transform;
    instance.creation_time = global_time;
    instance.owner_lifespan = request.owner_lifespan;
    instance.elements.reserve(file.elements().size());

    for (std::size_t index = 0; index < file.elements().size(); ++index) {
        const Element& definition = file.elements()[index];
        ElementState element;
        element.instance_id = instance.id;
        element.effect_id = instance.effect_id;
        element.definition_index = index;
        element.file = &file;
        element.definition = &definition;
        auto element_flags = static_cast<std::uint32_t>(definition.flags);
        // InitEffectElement sets this bit after copying the cartridge flags;
        // draw submission uses it as the lifetime-visible enable state.
        element_flags |= static_cast<std::uint32_t>(
            formats::EffElemFlags::DrawEnabled);
        if (request.element_extension) {
            element_flags |= static_cast<std::uint32_t>(
                formats::EffElemFlags::ElementExtension);
        }
        element.flags = static_cast<formats::EffElemFlags>(element_flags);
        element.own_transform = request.transform;
        element.transform = request.transform;
        if (has_flag(definition.flags, formats::EffElemFlags::SpawnUnitVecs)) {
            element.own_transform = formats::MatrixOps::get_transform4(
                {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F},
                translation(request.transform));
            element.transform = element.own_transform;
        }
        element.acceleration = definition.acceleration;
        element.creation_time = global_time
            + (request.child ? FixedFrameSeconds : 0.0F);
        element.expiration_time = element.creation_time + definition.lifespan;
        element.lifespan = definition.lifespan;
        element.drain_time = definition.drain_time;
        element.buffer_time = definition.buffer_time;
        element.parity = static_cast<std::uint8_t>(tick_count_ & 1u);
        instance.elements.push_back(std::move(element));
    }

    instances_.push_back(std::move(instance));
    return true;
}

void Runtime::set_transform(std::uint32_t instance_id,
                            formats::Matrix4 transform) noexcept {
    for (auto& instance : instances_) {
        if (instance.id != instance_id) {
            continue;
        }
        instance.transform = transform;
        for (auto& element : instance.elements) {
            element.own_transform = transform;
            element.transform = transform;
        }
        return;
    }
}

bool Runtime::spawn_particle(ElementState& element, float global_time,
                             float portion_total, ParticleState& result) {
    if (element.definition == nullptr || element.file == nullptr
        || element.definition->particle_names.empty()
        || particle_count() >= MaxParticleCount) {
        return false;
    }

    result = {};
    result.id = next_particle_id_++;
    result.creation_time = global_time;
    result.particle_id = 0;
    result.portion_total = portion_total;
    result.functions = function_ids(element.flags,
                                    element.definition->draw_type);
    const TimeValues element_times{
        global_time,
        std::max(0.0F, global_time - element.creation_time),
        element.lifespan};

    const auto evaluate_vector_action =
        [&](FuncAction action, formats::Vector3& value) {
            const auto offset = action_offset(element, action);
            return offset.has_value()
                && evaluate_particle_vector(
                    element, result, *offset, element_times, value,
                    true);
        };
    formats::Vector3 value;
    if (evaluate_vector_action(FuncAction::SetNewParticlePosition, value)) {
        result.position = value;
    }
    if (evaluate_vector_action(FuncAction::SetNewParticleSpeed, value)) {
        result.speed = value;
    }
    if (!has_flag(element.flags,
                  formats::EffElemFlags::UseTransform)) {
        result.position = formats::MatrixOps::vec3_mult_mtx4(
            result.position, element.transform);
        result.speed = formats::MatrixOps::vec3_mult_mtx3(
            result.speed, element.transform);
    }

    const auto evaluate_float_action =
        [&](FuncAction action, float& destination, TimeValues times) {
            const auto offset = action_offset(element, action);
            return offset.has_value()
                && evaluate_particle_float(
                    element, result, *offset, times, destination, true);
        };
    for (std::size_t index = 0; index < result.read_only_fields.size();
         ++index) {
        const auto action = static_cast<FuncAction>(
            static_cast<std::uint32_t>(FuncAction::SetParticleRoField1)
            + static_cast<std::uint32_t>(index));
        if (!evaluate_float_action(action, result.read_only_fields[index],
                                   element_times)) {
            result.read_only_fields[index] = element.read_only_fields[index];
        }
    }

    const TimeValues new_lifespan_times{global_time, 1.0F, element.lifespan};
    if (!evaluate_float_action(FuncAction::SetNewParticleLifespan,
                               result.lifespan, new_lifespan_times)
        || !finite(result.lifespan)) {
        result.lifespan = element.lifespan;
    }
    result.expiration_time = result.creation_time + result.lifespan;

    if (const Function* function = action_function(
            element, FuncAction::UpdateParticleSpeed);
        function != nullptr && function->id == 4) {
        const auto offset = action_offset(element, FuncAction::UpdateParticleSpeed);
        if (offset.has_value()
            && evaluate_particle_vector(element, result, *offset,
                                         element_times, value, true)) {
            result.speed = value;
        }
    }

    const auto evaluate_fixed_action = [&](FuncAction action,
                                           float& destination) {
        const Function* function = action_function(element, action);
        if (function == nullptr || function->id != 42) {
            return false;
        }
        const auto offset = action_offset(element, action);
        return offset.has_value()
            && evaluate_particle_float(element, result, *offset,
                                       element_times, destination, true);
    };
    if (!evaluate_fixed_action(FuncAction::SetParticleRed, result.red)) {
        result.red = 1.0F;
    }
    if (!evaluate_fixed_action(FuncAction::SetParticleGreen, result.green)) {
        result.green = 1.0F;
    }
    if (!evaluate_fixed_action(FuncAction::SetParticleBlue, result.blue)) {
        result.blue = 1.0F;
    }
    if (!evaluate_fixed_action(FuncAction::SetParticleAlpha, result.alpha)
        || !finite(result.alpha)) {
        result.alpha = 1.0F;
    }
    result.alpha = std::max(0.0F, result.alpha);
    if (!evaluate_fixed_action(FuncAction::SetParticleScale, result.scale)) {
        result.scale = 0.0F;
    }
    if (!evaluate_fixed_action(FuncAction::SetParticleRotation,
                               result.rotation)) {
        result.rotation = 0.0F;
    }

    for (std::size_t index = 0; index < result.read_write_fields.size();
         ++index) {
        const auto action = static_cast<FuncAction>(
            static_cast<std::uint32_t>(FuncAction::SetParticleRwField1)
            + static_cast<std::uint32_t>(index));
        static_cast<void>(evaluate_float_action(
            action, result.read_write_fields[index], element_times));
    }
    return true;
}

void Runtime::update_element(InstanceState& instance, ElementState& element,
                             float delta_seconds, float global_time,
                             const CollisionResolver& collision,
                             std::vector<PendingSpawn>& pending) {
    if (element.definition == nullptr) {
        element.expired = true;
        return;
    }
    const auto flags = element.flags;
    if (!element.expired && global_time > element.expiration_time) {
        if (!has_flag(flags, formats::EffElemFlags::KeepAlive)) {
            element.expired = true;
            element.particles.clear();
            return;
        }
        element.expired = true;
    }

    if (!element.expired) {
        if (has_flag(flags, formats::EffElemFlags::ElementExtension)
            && global_time - element.creation_time > element.buffer_time) {
            element.creation_time += element.buffer_time - element.drain_time;
            element.expiration_time += element.buffer_time - element.drain_time;
        }
        element.transform = element.own_transform;
        const TimeValues times{
            global_time,
            global_time - element.creation_time,
            element.lifespan};
        if ((tick_count_ & 1u) == element.parity) {
            const auto offset = action_offset(
                element, FuncAction::IncreaseParticleAmount);
            if (offset.has_value()) {
                float amount = 0.0F;
                if (evaluate_element_float(element, *offset, times, amount)
                    && finite(amount)) {
                    element.particle_amount += amount;
                }
            }
        }

        const int spawn_count = std::max(
            0, static_cast<int>(std::floor(element.particle_amount)));
        element.particle_amount -= static_cast<float>(spawn_count);
        const float portion_step = spawn_count > 0
            ? 1.0F / static_cast<float>(spawn_count) : 0.0F;
        float portion = 0.0F;
        for (int index = 0; index < spawn_count; ++index) {
            ParticleState particle;
            if (!spawn_particle(element, global_time, portion, particle)) {
                break;
            }
            element.particles.push_back(std::move(particle));
            portion += portion_step;
        }
    } else {
        element.transform = element.own_transform;
    }

    for (std::size_t index = 0; index < element.particles.size();) {
        ParticleState& particle = element.particles[index];
        if (has_flag(flags, formats::EffElemFlags::ElementExtension)
            && has_flag(flags, formats::EffElemFlags::ParticleExtension)
            && global_time - particle.creation_time > element.buffer_time) {
            particle.creation_time += element.buffer_time - element.drain_time;
            particle.expiration_time += element.buffer_time - element.drain_time;
        }

        if (global_time < particle.expiration_time) {
            const TimeValues times{
                global_time,
                global_time - particle.creation_time,
                particle.lifespan};
            const auto evaluate_float_action =
                [&](FuncAction action, float& destination) {
                    const auto offset = action_offset(element, action);
                    return offset.has_value()
                        && evaluate_particle_float(
                            element, particle, *offset, times, destination);
                };
            for (std::size_t field = 0;
                 field < particle.read_write_fields.size(); ++field) {
                const auto action = static_cast<FuncAction>(
                    static_cast<std::uint32_t>(FuncAction::SetParticleRwField1)
                    + static_cast<std::uint32_t>(field));
                static_cast<void>(evaluate_float_action(
                    action, particle.read_write_fields[field]));
            }
            float value = 0.0F;
            if (evaluate_float_action(FuncAction::SetParticleId, value)) {
                if (value < 0.0F) {
                    particle.particle_id = 0;
                } else {
                    particle.particle_id = std::min<std::size_t>(
                        static_cast<std::size_t>(value),
                        element.definition->particle_names.size() - 1);
                }
            }

            const auto speed_offset = action_offset(
                element, FuncAction::UpdateParticleSpeed);
            if (speed_offset.has_value()) {
                formats::Vector3 speed;
                if (evaluate_particle_vector(element, particle, *speed_offset,
                                              times, speed)) {
                    particle.speed = speed;
                }
            }
            static_cast<void>(evaluate_float_action(
                FuncAction::SetParticleRed, particle.red));
            static_cast<void>(evaluate_float_action(
                FuncAction::SetParticleGreen, particle.green));
            static_cast<void>(evaluate_float_action(
                FuncAction::SetParticleBlue, particle.blue));
            if (evaluate_float_action(FuncAction::SetParticleAlpha,
                                       particle.alpha)) {
                particle.alpha = std::max(0.0F, particle.alpha);
            }
            static_cast<void>(evaluate_float_action(
                FuncAction::SetParticleScale, particle.scale));
            static_cast<void>(evaluate_float_action(
                FuncAction::SetParticleRotation, particle.rotation));

            if (has_flag(flags, formats::EffElemFlags::UseAcceleration)) {
                particle.speed += element.acceleration * delta_seconds;
            }
            const formats::Vector3 previous = particle.position;
            particle.position += particle.speed * delta_seconds;
            if (has_flag(flags, formats::EffElemFlags::CheckCollision)
                && collision) {
                const auto contact = collision(previous, particle.position);
                if (contact.has_value()) {
                    particle.position = *contact;
                    particle.expiration_time = global_time;
                }
            }
            ++index;
            continue;
        }

        if (has_flag(flags, formats::EffElemFlags::SpawnChildEffect)
            && element.definition->child_effect_id != 0
            && resolver_) {
            const formats::Vector3 reverse = safe_normalized(
                -particle.speed, {0.0F, 0.0F, 1.0F});
            formats::Vector3 second =
                (reverse.z <= -0.9F || reverse.z >= 0.9F)
                ? formats::Vector3{1.0F, 0.0F, 0.0F}
                : formats::Vector3{0.0F, 0.0F, 1.0F};
            second = safe_normalized(formats::cross(reverse, second),
                                     {1.0F, 0.0F, 0.0F});
            PendingSpawn child;
            child.file = resolver_(element.definition->child_effect_id);
            child.request.effect_id = static_cast<std::uint16_t>(
                element.definition->child_effect_id);
            child.request.source_entity_id = instance.source_entity_id;
            child.request.transform = formats::MatrixOps::get_transform4(
                second, reverse, particle.position);
            child.request.owner_lifespan = element.lifespan;
            child.request.child = true;
            if (child.file != nullptr) {
                pending.push_back(std::move(child));
            }
        }
        element.particles.erase(element.particles.begin()
                                + static_cast<std::ptrdiff_t>(index));
    }
}

void Runtime::process_pending(float global_time,
                              std::vector<PendingSpawn>& pending) {
    for (auto& child : pending) {
        if (child.file == nullptr) {
            continue;
        }
        child.request.instance_id = next_instance_id_++;
        if (instances_.size() >= MaxEffectCount) {
            break;
        }
        static_cast<void>(spawn(*child.file, child.request, global_time));
    }
    pending.clear();
}

void Runtime::update(float delta_seconds, float global_time,
                     const CollisionResolver& collision) {
    if (!finite(delta_seconds) || delta_seconds <= 0.0F
        || !finite(global_time)) {
        return;
    }
    // The Win32 host can render more than once for one fixed simulation tick.
    // Effects are simulation-owned, so an equal timestamp must not advance
    // particles a second time just because the back buffer was redrawn.
    if (have_time_ && global_time <= current_time_) {
        return;
    }
    current_time_ = global_time;
    have_time_ = true;
    ++tick_count_;

    std::vector<PendingSpawn> pending;
    pending.reserve(instances_.size());
    for (auto& instance : instances_) {
        for (std::size_t index = 0; index < instance.elements.size();) {
            ElementState& element = instance.elements[index];
            update_element(instance, element, delta_seconds, global_time,
                           collision, pending);
            if (element.expired && element.particles.empty()) {
                instance.elements.erase(instance.elements.begin()
                                        + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            ++index;
        }
    }
    instances_.erase(
        std::remove_if(instances_.begin(), instances_.end(),
                       [](const InstanceState& instance) {
                           return instance.elements.empty();
                       }),
        instances_.end());
    process_pending(global_time, pending);
}

void Runtime::detach(std::uint32_t instance_id, bool set_expired) noexcept {
    for (auto& instance : instances_) {
        if (instance.id != instance_id) {
            continue;
        }
        for (auto& element : instance.elements) {
            if (element.definition == nullptr) {
                continue;
            }
            if (has_flag(element.definition->flags,
                         formats::EffElemFlags::DestroyOnDetach)) {
                element.expired = true;
                element.particles.clear();
                continue;
            }
            auto flags = static_cast<std::uint32_t>(element.flags);
            flags &= ~static_cast<std::uint32_t>(
                formats::EffElemFlags::ElementExtension);
            flags |= static_cast<std::uint32_t>(formats::EffElemFlags::KeepAlive);
            element.flags = static_cast<formats::EffElemFlags>(flags);
            if (set_expired) {
                element.expired = true;
            }
        }
        return;
    }
}

void Runtime::clear() noexcept {
    instances_.clear();
    current_time_ = 0.0F;
    have_time_ = false;
}

bool Runtime::has_instance(std::uint32_t instance_id) const noexcept {
    return std::any_of(instances_.begin(), instances_.end(),
                       [instance_id](const InstanceState& instance) {
                           return instance.id == instance_id;
                       });
}

std::size_t Runtime::particle_count() const noexcept {
    std::size_t count = 0;
    for (const auto& instance : instances_) {
        for (const auto& element : instance.elements) {
            count += element.particles.size();
        }
    }
    return count;
}

} // namespace fruityprime::effects
