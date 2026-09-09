#pragma once

// Native counterpart of Formats/Effects.cs's SingleParticle.
//
// A single particle is one camera-facing quad: the managed Process() builds
// its four corners from the scale, and AddRenderItem hands them to the
// renderer with the particle's material. The corner order and the texture
// coordinates decide which way the sprite faces, so they are reproduced
// exactly rather than re-derived.

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>

namespace fruityprime::effects {

struct SingleParticle {
    // Effects.SingleParticle: the definition supplies the model and material;
    // the renderer resolves those, so the index is kept rather than a pointer.
    std::int32_t particle_definition = -1;
    formats::Vector3 position{};
    formats::Vector3 color{};
    float alpha = 0.0F;
    float scale = 0.0F;

    // Written by process(); a particle with no alpha is not drawn at all.
    bool should_draw = false;
    std::array<formats::Vector2, 4> texcoords{};
    std::array<formats::Vector3, 4> vertices{};

    // SingleParticle.Process
    void process() noexcept {
        should_draw = false;
        if (alpha <= 0.0F) {
            return;
        }
        should_draw = true;
        // bottom left, bottom right, top right, top left -- the managed order,
        // which is what makes the quad wind correctly for the sphere
        // billboard the renderer applies.
        vertices[0] = {-scale, scale, 0.0F};
        texcoords[0] = {0.0F, 0.0F};
        vertices[1] = {scale, scale, 0.0F};
        texcoords[1] = {1.0F, 0.0F};
        vertices[2] = {scale, -scale, 0.0F};
        texcoords[2] = {1.0F, 1.0F};
        vertices[3] = {-scale, -scale, 0.0F};
        texcoords[3] = {0.0F, 1.0F};
    }

    // SingleParticle.AddRenderItem builds the interleaved texcoord/vertex
    // array the render item carries. The renderer supplies the binding and
    // the repeat/scale terms from the material.
    [[nodiscard]] std::array<formats::Vector3, 8> uvs_and_vertices()
        const noexcept {
        return {formats::Vector3{texcoords[0].x, texcoords[0].y, 0.0F},
                vertices[0],
                formats::Vector3{texcoords[1].x, texcoords[1].y, 0.0F},
                vertices[1],
                formats::Vector3{texcoords[2].x, texcoords[2].y, 0.0F},
                vertices[2],
                formats::Vector3{texcoords[3].x, texcoords[3].y, 0.0F},
                vertices[3]};
    }
};

} // namespace fruityprime::effects
