#pragma once

#include "Formats/effects.hpp"
#include "Utility/rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace fruityprime::effects {

// The native renderer receives effect events from gameplay and expands them
// into the same element/particle lifetime graph used by Formats/Effects.cs.
// The resource files remain immutable; all mutable state lives in Runtime so a
// headless replay can use the exact same update boundary as the Win32 host.
struct SpawnRequest {
    std::uint32_t instance_id = 0;
    std::uint16_t effect_id = 0;
    std::uint32_t source_entity_id = 0;
    formats::Matrix4 transform{};
    float owner_lifespan = 0.0F;
    bool child = false;
    bool element_extension = false;
};

struct FunctionIds {
    std::uint8_t set_vecs = 0;
    std::uint8_t draw = 0;
};

[[nodiscard]] FunctionIds function_ids(formats::EffElemFlags flags,
                                        std::int32_t draw_type) noexcept;

struct ParticleState {
    std::uint32_t id = 0;
    std::size_t particle_id = 0;
    formats::Vector3 position;
    formats::Vector3 speed;
    float creation_time = 0.0F;
    float expiration_time = 0.0F;
    float lifespan = 0.0F;
    float scale = 0.0F;
    float rotation = 0.0F;
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
    float alpha = 1.0F;
    float portion_total = 0.0F;
    std::array<float, 4> read_only_fields{};
    std::array<float, 4> read_write_fields{};
    FunctionIds functions;
};

struct ElementState {
    std::uint32_t instance_id = 0;
    std::uint16_t effect_id = 0;
    std::size_t definition_index = 0;
    const File* file = nullptr;
    const Element* definition = nullptr;
    formats::Matrix4 own_transform{};
    formats::Matrix4 transform{};
    formats::Vector3 acceleration;
    formats::EffElemFlags flags = formats::EffElemFlags::None;
    float creation_time = 0.0F;
    float expiration_time = 0.0F;
    float lifespan = 0.0F;
    float drain_time = 0.0F;
    float buffer_time = 0.0F;
    float particle_amount = 0.0F;
    std::array<float, 4> read_only_fields{};
    bool function39_called = false;
    bool expired = false;
    std::uint8_t parity = 0;
    std::vector<ParticleState> particles;
};

struct InstanceState {
    std::uint32_t id = 0;
    std::uint16_t effect_id = 0;
    std::uint32_t source_entity_id = 0;
    const File* file = nullptr;
    formats::Matrix4 transform{};
    float creation_time = 0.0F;
    float owner_lifespan = 0.0F;
    std::vector<ElementState> elements;
};

class Runtime final {
public:
    using EffectResolver = std::function<const File*(std::uint32_t)>;
    using CollisionResolver = std::function<
        std::optional<formats::Vector3>(formats::Vector3, formats::Vector3)>;

    explicit Runtime(EffectResolver resolver = {}, utility::Rng rng = {});

    [[nodiscard]] bool spawn(const File& file, SpawnRequest request,
                             float global_time);
    void set_transform(std::uint32_t instance_id,
                       formats::Matrix4 transform) noexcept;
    void update(float delta_seconds, float global_time,
                const CollisionResolver& collision = {});
    void detach(std::uint32_t instance_id, bool set_expired) noexcept;
    void clear() noexcept;

    [[nodiscard]] bool has_instance(std::uint32_t instance_id) const noexcept;
    [[nodiscard]] std::size_t particle_count() const noexcept;
    [[nodiscard]] const std::vector<InstanceState>& instances() const noexcept {
        return instances_;
    }
    [[nodiscard]] std::uint64_t tick_count() const noexcept {
        return tick_count_;
    }

private:
    struct PendingSpawn {
        const File* file = nullptr;
        SpawnRequest request;
    };

    [[nodiscard]] EvaluationState element_evaluation_state(
        const ElementState& element) const noexcept;
    [[nodiscard]] EvaluationState particle_evaluation_state(
        const ElementState& element, const ParticleState& particle,
        bool use_element_creation_time) const noexcept;
    [[nodiscard]] const Function* action_function(
        const ElementState& element, FuncAction action) const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> action_offset(
        const ElementState& element, FuncAction action) const noexcept;
    [[nodiscard]] bool evaluate_element_float(
        ElementState& element, std::uint32_t function_offset,
        TimeValues times, float& result) noexcept;
    [[nodiscard]] bool evaluate_particle_float(
        const ElementState& element, const ParticleState& particle,
        std::uint32_t function_offset, TimeValues times, float& result,
        bool use_element_creation_time = false) noexcept;
    [[nodiscard]] bool evaluate_particle_vector(
        const ElementState& element, const ParticleState& particle,
        std::uint32_t function_offset, TimeValues times,
        formats::Vector3& result,
        bool use_element_creation_time = false) noexcept;
    [[nodiscard]] bool spawn_particle(ElementState& element,
                                      float global_time, float portion_total,
                                      ParticleState& result);
    void update_element(InstanceState& instance, ElementState& element,
                        float delta_seconds, float global_time,
                        const CollisionResolver& collision,
                        std::vector<PendingSpawn>& pending);
    void process_pending(float global_time,
                         std::vector<PendingSpawn>& pending);

    EffectResolver resolver_;
    utility::Rng rng_;
    std::vector<InstanceState> instances_;
    std::uint32_t next_instance_id_ = 0xc0000000u;
    std::uint32_t next_particle_id_ = 0xd0000000u;
    std::uint64_t tick_count_ = 0;
    float current_time_ = 0.0F;
    bool have_time_ = false;
};

} // namespace fruityprime::effects
