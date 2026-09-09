#include "Entities/enemy_helpers.hpp"

#include <cmath>

namespace fruityprime::entities {
namespace {

constexpr float Pi = 3.14159265358979323846F;

[[nodiscard]] float radians(float degrees) noexcept {
    return degrees * Pi / 180.0F;
}

} // namespace

formats::Vector3 rotate_vector(formats::Vector3 value, formats::Vector3 axis,
                               float degrees) noexcept {
    // Matrix3.CreateFromAxisAngle applied on the right, which is the managed
    // `vec * matrix` order.
    const formats::Vector3 unit = axis.normalized();
    const float angle = radians(degrees);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0F - c;
    const float x = unit.x;
    const float y = unit.y;
    const float z = unit.z;
    // OpenTK treats the vector as a row: result[j] = sum_i v[i] * M[i][j],
    // so the sign of each cross term is the transpose of the column-vector
    // form.  Getting this backwards turns every enemy the wrong way.
    return {value.x * (t * x * x + c)
                + value.y * (t * x * y - s * z)
                + value.z * (t * x * z + s * y),
            value.x * (t * x * y + s * z)
                + value.y * (t * y * y + c)
                + value.z * (t * y * z - s * x),
            value.x * (t * x * z - s * y)
                + value.y * (t * y * z + s * x)
                + value.z * (t * z * z + c)};
}

bool seek_target_vector(formats::Vector3 target, formats::Vector3& current,
                        formats::Vector3 axis, std::uint16_t& steps,
                        float degrees) noexcept {
    // While there are steps left and the target is more than one step away,
    // turn by one step.  Otherwise snap, which is what stops the enemy
    // oscillating around a target it cannot land on exactly.
    if (steps > 0 && formats::dot(target, current) < std::cos(radians(degrees))) {
        current = rotate_vector(current, axis, degrees).normalized();
        --steps;
        return false;
    }
    current = target;
    return true;
}

formats::Vector3 fix_parallel_vectors(formats::Vector3 facing,
                                      formats::Vector3 up) noexcept {
    formats::Vector3 right = formats::cross(up, facing);
    if (right.x != 0.0F || right.y != 0.0F || right.z != 0.0F) {
        return up;
    }
    // The two are parallel, so pick an axis that is not.
    if (facing.y != 0.0F || facing.z != 0.0F) {
        right = formats::cross(facing, formats::Vector3{1.0F, 0.0F, 0.0F});
    } else {
        right = formats::cross(facing, formats::Vector3{0.0F, 1.0F, 0.0F});
    }
    // The managed code normalises the right vector, then takes up as
    // facing x right -- not right x facing.
    right = right.normalized();
    return formats::cross(facing, right);
}

} // namespace fruityprime::entities
