#include "Formats/effect_particle_funcs.hpp"

#include <cmath>
#include <stdexcept>

namespace fruityprime::formats {
namespace {

[[nodiscard]] Vector3 vec3_mult_mtx4_no_translation(
    Vector3 value, const Matrix4& matrix) noexcept {
    // Matrix4.ClearTranslation followed by Vec3MultMtx4 is the rotation and
    // scale only.
    return {value.x * matrix.m11 + value.y * matrix.m21 + value.z * matrix.m31,
            value.x * matrix.m12 + value.y * matrix.m22 + value.z * matrix.m32,
            value.x * matrix.m13 + value.y * matrix.m23 + value.z * matrix.m33};
}

} // namespace

void set_vecs_b0(EffectParticle& particle) noexcept {
    // The game uses the view matrix here; the renderer's sphere billboard
    // supplies the same orientation, so the axes stay in model space.
    particle.EffectVec1 = {1.0F, 0.0F, 0.0F};
    particle.EffectVec2 = {0.0F, -1.0F, 0.0F};
    particle.BillboardMode = BillboardMode::Sphere;
}

void set_vecs_bc(EffectParticle& particle) noexcept {
    particle.EffectVec1 = {1.0F, 0.0F, 0.0F};
    particle.EffectVec2 = {0.0F, 0.0F, 1.0F};
}

void set_vecs_c0(EffectParticle& particle,
                 const Matrix4& view_matrix) noexcept {
    particle.EffectVec1 = {0.0F, -1.0F, 0.0F};
    particle.EffectVec2 = {view_matrix.m13, view_matrix.m23, view_matrix.m33};
}

void set_vecs_d4(EffectParticle& particle) noexcept {
    // The managed implementation asserts if this is reached and builds its
    // basis against an identity view matrix, which is what is reproduced.
    const Matrix4 view{};
    Vector3 vec1 = particle.Speed.normalized();
    Vector3 vec2{view.m13, view.m23, view.m33};
    Vector3 vec3 = cross(vec2, vec1);
    if (vec3.length_squared() < 64.0F / 4096.0F) {
        vec2 = {view.m11, view.m21, view.m31};
        vec3 = cross(vec2, vec1);
    }
    vec3 = vec3.normalized();
    vec1 = cross(vec3, vec2);
    particle.EffectVec1 = vec1;
    particle.EffectVec2 = vec2;
    particle.EffectVec3 = vec3;
}

void set_vecs_d8(EffectParticle& particle) noexcept {
    particle.BillboardMode = BillboardMode::Sphere;
}

void invoke_set_vecs_func(EffectParticle& particle, int set_vecs_id,
                          const Matrix4& view_matrix) {
    particle.BillboardMode = BillboardMode::None;
    switch (set_vecs_id) {
    case 1: set_vecs_b0(particle); break;
    case 2: set_vecs_bc(particle); break;
    case 3: set_vecs_c0(particle, view_matrix); break;
    case 4: set_vecs_d4(particle); break;
    case 5: set_vecs_d8(particle); break;
    default:
        throw std::runtime_error("effect set-vecs function id is not known: "
                                 + std::to_string(set_vecs_id));
    }
}

void draw_b8(EffectParticle& particle, float scale_factor) noexcept {
    if (particle.Alpha <= 0.0F) {
        return;
    }
    particle.ShouldDraw = true;
    particle.Color = {particle.Red, particle.Green, particle.Blue};
    const Vector3 ev1 = particle.EffectVec1 * particle.Scale;
    const Vector3 ev2 = particle.EffectVec2 * particle.Scale;

    const Matrix4 owner = particle.Owner == nullptr ? Matrix4{}
                                                    : particle.Owner->Transform;
    const Vector3 position =
        vec3_mult_mtx4_no_translation(particle.Position, owner);

    // The corner order is top left, top right, bottom right, bottom left, and
    // each vertex is relative to the particle's own position.
    const float base_x = position.x - ev1.x / 2.0F + ev2.x / 2.0F;
    const float base_y = position.y - ev1.y / 2.0F + ev2.y / 2.0F;
    const float base_z = position.z - ev1.z / 2.0F + ev2.z / 2.0F;

    const auto corner = [&](float x, float y, float z) {
        return Vector3{x / scale_factor, y / scale_factor, z / scale_factor}
            - position;
    };

    particle.Vertex0 = corner(base_x, base_y, base_z);
    particle.Texcoord0 = {0.0F, 1.0F};

    const float right_x = base_x + ev1.x;
    const float right_y = base_y + ev1.y;
    const float right_z = base_z + ev1.z;
    particle.Vertex1 = corner(right_x, right_y, right_z);
    particle.Texcoord1 = {1.0F, 1.0F};

    const float down_x = right_x - ev2.x;
    const float down_y = right_y - ev2.y;
    const float down_z = right_z - ev2.z;
    particle.Vertex2 = corner(down_x, down_y, down_z);
    particle.Texcoord2 = {1.0F, 0.0F};

    particle.Vertex3 = corner(down_x - ev1.x, down_y - ev1.y, down_z - ev1.z);
    particle.Texcoord3 = {0.0F, 0.0F};
}

void invoke_draw_func(EffectParticle& particle, int draw_id,
                      float scale_factor) {
    particle.ShouldDraw = false;
    particle.DrawNode = false;
    switch (draw_id) {
    case 1:
    case 2:
        draw_b8(particle, scale_factor);
        break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        // DrawC4, DrawCC, DrawD0 and DrawDC differ from DrawB8 only in which
        // axes they use and whether they draw the element's node; the quad
        // build is the same, so the shared one runs until each is ported.
        draw_b8(particle, scale_factor);
        break;
    default:
        throw std::runtime_error("effect draw function id is not known: "
                                 + std::to_string(draw_id));
    }
}

void reset_elements(EffectEntry& entry, float elapsed_time) noexcept {
    for (EffectElementEntry* element : entry.Elements) {
        if (element == nullptr) {
            continue;
        }
        element->Expired = false;
        element->Func39Called = false;
        element->CreationTime = elapsed_time;
        element->ExpirationTime = element->CreationTime + element->Lifespan;
    }
}

void set_read_only_field(EffectEntry& entry, int index, float value) noexcept {
    for (EffectElementEntry* element : entry.Elements) {
        if (element == nullptr) {
            continue;
        }
        switch (index) {
        case 0: element->RoField1 = value; break;
        case 1: element->RoField2 = value; break;
        case 2: element->RoField3 = value; break;
        case 3: element->RoField4 = value; break;
        default: break;
        }
    }
}

} // namespace fruityprime::formats
