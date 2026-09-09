#pragma once

#include "Mods/Network/net_protocol.hpp"
#include "Entities/scene.hpp"

#include <span>

namespace fruityprime::players {

// Runtime light data exposed by LightSourceEntity.  It is intentionally a
// value type so PlayerEntity, HalfturretEntity, and a renderer can share the
// same interpolation input without depending on one another.
struct LightSource {
    scene::EntityVolume volume;
    net::Vec3 light1_vector{0.0F, 1.0F, 0.0F};
    net::Vec3 light1_color{1.0F, 1.0F, 1.0F};
    net::Vec3 light2_vector{0.0F, 1.0F, 0.0F};
    net::Vec3 light2_color{1.0F, 1.0F, 1.0F};
    bool light1_enabled = false;
    bool light2_enabled = false;
};

// Native counterpart of DynamicLightEntityBase.cs.  The update rule mirrors
// the managed per-channel eight-step interpolation and its room-light
// fallback.  No renderer API is involved here.
class DynamicLightEntityBase {
public:
    void reset_lights(net::Vec3 light1_vector, net::Vec3 light1_color,
                      net::Vec3 light2_vector, net::Vec3 light2_color) noexcept;
    void update_light_sources(net::Vec3 position,
                              std::span<const LightSource> sources,
                              net::Vec3 room_light1_vector,
                              net::Vec3 room_light1_color,
                              net::Vec3 room_light2_vector,
                              net::Vec3 room_light2_color,
                              float frame_seconds) noexcept;

    void set_use_room_lights(bool enabled) noexcept {
        use_room_lights_ = enabled;
    }
    [[nodiscard]] bool use_room_lights() const noexcept {
        return use_room_lights_;
    }
    [[nodiscard]] const net::Vec3& light1_vector() const noexcept {
        return light1_vector_;
    }
    [[nodiscard]] const net::Vec3& light1_color() const noexcept {
        return light1_color_;
    }
    [[nodiscard]] const net::Vec3& light2_vector() const noexcept {
        return light2_vector_;
    }
    [[nodiscard]] const net::Vec3& light2_color() const noexcept {
        return light2_color_;
    }

private:
    net::Vec3 light1_vector_{0.0F, 1.0F, 0.0F};
    net::Vec3 light1_color_{1.0F, 1.0F, 1.0F};
    net::Vec3 light2_vector_{0.0F, 1.0F, 0.0F};
    net::Vec3 light2_color_{1.0F, 1.0F, 1.0F};
    bool use_room_lights_ = false;
};

} // namespace fruityprime::players
