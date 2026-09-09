#include "Entities/Players/player_statics.hpp"

#include "Formats/model_instance.hpp"
#include "Metadata/player_values.hpp"

#include <cmath>

namespace fruityprime::players {
namespace {

[[nodiscard]] float to_float(std::int32_t raw) noexcept {
    return static_cast<float>(raw) / 4096.0F;
}

[[nodiscard]] formats::CollisionVolume sphere(formats::Vector3 center,
                                              float radius) noexcept {
    formats::CollisionVolume volume;
    volume.Type = formats::VolumeType::Sphere;
    volume.SpherePosition = center;
    volume.SphereRadius = radius;
    return volume;
}

} // namespace

void PlayerStatics::generate_player_volumes() noexcept {
    for (std::size_t i = 0; i < volumes_.size()
                            && i < metadata::PlayerValuesTable.size(); ++i) {
        const auto& values = metadata::PlayerValuesTable[i];
        // The low sphere sits so its bottom is at the minimum pickup height,
        // and the high one so its top is at the maximum: between them they
        // cover exactly the reach the game allows, with no gap in the middle.
        float radius = to_float(values.BipedColRadius);
        volumes_[i][0] = sphere(
            {0.0F, to_float(values.MinPickupHeight) + radius, 0.0F}, radius);
        volumes_[i][1] = sphere(
            {0.0F, to_float(values.MaxPickupHeight) - radius, 0.0F}, radius);
        // The alt-form sphere sits so its bottom is at ground level.
        radius = to_float(values.AltColRadius);
        volumes_[i][2] =
            sphere({0.0F, to_float(values.AltColYPos), 0.0F}, radius);
    }
}

void PlayerStatics::generate_kanden_alt_node_distances(
    const model::File& kanden_alt) {
    // The managed version measures once, while every distance is still zero.
    if (kanden_distances_[0] != 0.0F || kanden_distances_[1] != 0.0F
        || kanden_distances_[2] != 0.0F || kanden_distances_[3] != 0.0F) {
        return;
    }
    model::ModelInstance instance(kanden_alt);
    instance.compute_node_matrices();
    const auto& nodes = instance.node_states();
    for (std::size_t i = 0; i < kanden_distances_.size(); ++i) {
        if (i + 1 >= nodes.size()) {
            break;
        }
        // The translation row of each node's bind-pose transform.
        const formats::Matrix4& a = nodes[i].transform;
        const formats::Matrix4& b = nodes[i + 1].transform;
        const float dx = a.m41 - b.m41;
        const float dy = a.m42 - b.m42;
        const float dz = a.m43 - b.m43;
        kanden_distances_[i] = std::sqrt(dx * dx + dy * dy + dz * dz);
    }
}

} // namespace fruityprime::players
