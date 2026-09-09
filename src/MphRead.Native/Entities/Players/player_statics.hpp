#pragma once

// Native counterpart of the per-process player statics in
// Entities/Players/PlayerEntity.cs.
//
// These are the pieces of player state that are shared by every slot rather
// than owned by one: which slot the local player is, how many have been
// created, the three pickup/collision spheres per hunter, and the segment
// lengths of Kanden's alt form.

#include "Formats/formats_layouts.hpp"
#include "Formats/model_format.hpp"
#include "Formats/Types.hpp"

#include <array>
#include <cstdint>

namespace fruityprime::players {

// PlayerEntity.SlotCapacity: every per-slot array is this long, so raising it
// is what makes a match of more than four possible.
inline constexpr std::size_t SlotCapacity = 8;

class PlayerStatics final {
public:
    // PlayerEntity.MainPlayerIndex.  Only the networked paths move this, and
    // nothing moved it back: a client that joined on slot 1 kept pointing at
    // slot 1, so the next offline match made Main a bot and the player's
    // input went to its controls.  reset() puts it back.
    [[nodiscard]] int main_player_index() const noexcept {
        return main_player_index_;
    }
    void set_main_player_index(int index) noexcept {
        main_player_index_ = index;
    }

    // PlayerEntity.PlayersCreated / PlayerCount
    [[nodiscard]] int players_created() const noexcept {
        return players_created_;
    }
    [[nodiscard]] int player_count() const noexcept { return player_count_; }
    void set_player_count(int count) noexcept { player_count_ = count; }

    // PlayerEntity.Create returns nothing once the match's cap is reached.
    [[nodiscard]] bool create_player(int max_players) noexcept {
        if (players_created_ >= max_players) {
            return false;
        }
        ++players_created_;
        return true;
    }

    // PlayerEntity.ResetReferences
    void reset_references() noexcept {
        players_created_ = 0;
        main_player_index_ = 0;
    }

    // PlayerEntity.PlayerVolumes: three spheres per hunter -- the low and
    // high pickup reach, and the alt-form body.
    enum class Volume : std::size_t {
        PickupLow = 0,
        PickupHigh = 1,
        AltForm = 2
    };

    [[nodiscard]] const formats::CollisionVolume& player_volume(
        std::size_t hunter, Volume which) const noexcept {
        return volumes_[hunter][static_cast<std::size_t>(which)];
    }

    // PlayerEntity.GeneratePlayerVolumes
    void generate_player_volumes() noexcept;

    // PlayerEntity.KandenAltNodeDistances: the length of each segment of
    // Kanden's alt form, measured from the model's own bind pose so the tail
    // follows at the right spacing.
    [[nodiscard]] const std::array<float, 4>& kanden_alt_node_distances()
        const noexcept {
        return kanden_distances_;
    }

    // PlayerEntity.GenerateKandenAltNodeDistances.  The managed version only
    // measures once, when every distance is still zero.
    void generate_kanden_alt_node_distances(const model::File& kanden_alt);

private:
    int main_player_index_ = 0;
    int players_created_ = 0;
    int player_count_ = 0;
    std::array<std::array<formats::CollisionVolume, 3>, 8> volumes_{};
    std::array<float, 4> kanden_distances_{};
};

} // namespace fruityprime::players
