#pragma once

#include "Formats/Types.hpp"

#include <array>
#include <string>

namespace fruityprime::culling {

// Renderer-side references are compared by indices in the managed code. The
// room name remains available for diagnostics but intentionally does not
// participate in equality, so a reference can be matched after a room is
// reloaded under another display name.
struct NodeRef {
    std::string room_name;
    int part_index = 0;
    int node_index = 0;
    int model_index = 0;

    [[nodiscard]] static NodeRef none() {
        return NodeRef{{}, -1, -1, -1};
    }

    [[nodiscard]] friend bool operator==(const NodeRef& left,
                                        const NodeRef& right) noexcept {
        return left.part_index == right.part_index
            && left.node_index == right.node_index
            && left.model_index == right.model_index;
    }

    [[nodiscard]] friend bool operator!=(const NodeRef& left,
                                        const NodeRef& right) noexcept {
        return !(left == right);
    }
};

struct RoomPartVisInfo {
    NodeRef node_ref;
    float view_min_x = 0.0F;
    float view_max_x = 0.0F;
    float view_min_y = 0.0F;
    float view_max_y = 0.0F;
    RoomPartVisInfo* next = nullptr;
};

struct FrustumPlane {
    int x_index1 = 0;
    int x_index2 = 0;
    int y_index1 = 0;
    int y_index2 = 0;
    int z_index1 = 0;
    int z_index2 = 0;
    formats::Vector4 plane;
};

struct FrustumInfo {
    int index = 0;
    int count = 0;
    std::array<FrustumPlane, 10> planes{};
};

struct RoomFrustumItem {
    NodeRef node_ref = NodeRef::none();
    FrustumInfo info;
    RoomFrustumItem* next = nullptr;
};

} // namespace fruityprime::culling

namespace MphReadNative {
namespace Culling = ::fruityprime::culling;
}
