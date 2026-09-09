#pragma once

#include "PlayerCamera.hpp"
#include "Renderer.hpp"

#include <cstdint>

namespace fruityprime::players {

enum class DrawMode : std::uint8_t {
    Hidden,
    FirstPerson,
    Biped,
    AltForm
};

struct DrawPacket {
    DrawMode mode = DrawMode::Hidden;
    std::uint8_t lod = 0;
    bool draw_shadow = false;
    bool draw_frozen_overlay = false;
    renderer::DrawItem model;
    renderer::DrawItem shadow;
};

// Native counterpart of PlayerDraw.cs.  The packet deliberately stops at a
// renderer-independent draw description; model-node traversal belongs to the
// graphics backend, just as the managed Draw method delegates to GetDrawItems.
class PlayerDraw final {
public:
    [[nodiscard]] static DrawPacket build(
        const net::PlayerState& player, const Profile& profile,
        CameraType camera_type, bool is_main_player,
        bool camera_sequence_active, float squared_distance,
        bool max_detail = false) noexcept;
};

} // namespace fruityprime::players
