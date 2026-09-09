// Native counterpart of src/MphRead/Entities/Players/PlayerDraw.cs.
#include "PlayerDraw.hpp"

#include <algorithm>

namespace fruityprime::players {

DrawPacket PlayerDraw::build(const net::PlayerState& player,
                             const Profile& profile,
                             CameraType camera_type, bool is_main_player,
                             bool camera_sequence_active,
                             float squared_distance,
                             bool max_detail) noexcept {
    DrawPacket packet;
    const bool spectating = (player.flags & net::PlayerState::FlagSpectating) != 0;
    const bool hidden = spectating
        || (player.flags & net::PlayerState::FlagActive) == 0;
    if (hidden) {
        return packet;
    }
    const bool alt_form = (player.flags & net::PlayerState::FlagAltForm) != 0;
    const bool first_person = is_main_player
        && camera_type == CameraType::First
        && !camera_sequence_active;
    packet.mode = alt_form ? DrawMode::AltForm
        : first_person ? DrawMode::FirstPerson : DrawMode::Biped;
    packet.lod = !max_detail && !is_main_player
        && squared_distance >= 9.0F ? 1 : 0;
    const std::uint32_t form = alt_form ? 1u : 0u;
    packet.model.resource_id = 0x1000u
        + static_cast<std::uint32_t>(profile.hunter) * 2u + form
        + static_cast<std::uint32_t>(packet.lod) * 0x100u;
    packet.model.layer = renderer::Layer::Player;
    packet.model.x = player.position.x;
    packet.model.y = player.position.y;
    packet.model.z = player.position.z;
    packet.model.textured = true;
    packet.draw_shadow = true;
    packet.shadow.resource_id = 0x10u;
    packet.shadow.layer = renderer::Layer::Player;
    packet.shadow.x = player.position.x;
    packet.shadow.y = player.position.y;
    packet.shadow.z = player.position.z;
    packet.shadow.scale = alt_form ? 0.7F : 1.0F;
    packet.shadow.textured = true;
    packet.draw_frozen_overlay =
        (player.flags & net::PlayerState::FlagFrozen) != 0;
    return packet;
}

} // namespace fruityprime::players
