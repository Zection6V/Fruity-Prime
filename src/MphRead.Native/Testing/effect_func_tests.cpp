// Formats/Effects.cs's effect function dispatch.
//
// The cartridge drives an effect through small numbered functions; a wrong id
// mapping or a wrong parameter index produces an effect that still animates,
// just incorrectly, so the checks below pin the arithmetic rather than only
// that a call returns.
#include "Formats/effect_funcs.hpp"
#include "Formats/effect_particle_funcs.hpp"
#include <cstdio>
int main() {
    using namespace fruityprime::formats;
    EffectFuncBase owner;
    // fx42 returns its one parameter as a fixed-point float.
    owner.Funcs[100] = FxFuncInfo{42, {4096}};        // 1.0
    owner.Funcs[200] = FxFuncInfo{42, {2048}};        // 0.5
    // fx46 adds the two functions its parameters point at.
    FxFuncInfo add{46, {100, 200}};
    const float sum = invoke_float_func(owner, add, TimeValues{});
    // fx04 is a literal vector.
    FxFuncInfo lit{4, {4096, -4096, 2048}};
    Vector3 v{};
    invoke_vec_func(owner, lit, TimeValues{}, v);
    // fx20 scales a vector by a float.
    owner.Funcs[300] = lit;
    FxFuncInfo scaled{20, {200, 300}};
    Vector3 s{};
    invoke_vec_func(owner, scaled, TimeValues{}, s);
    // fx40 picks by elapsed/lifespan against its first parameter.
    FxFuncInfo pick{40, {2048, 4096, 8192}};
    const float early = invoke_float_func(owner, pick, TimeValues{0, 1, 4});
    const float late = invoke_float_func(owner, pick, TimeValues{0, 3, 4});
    std::printf("native effect funcs: sum=%.3f lit=(%.1f,%.1f,%.1f) scaled=(%.2f,%.2f,%.2f) "
                "early=%.1f late=%.1f\n",
                sum, v.x, v.y, v.z, s.x, s.y, s.z, early, late);

    // The set-vecs dispatch clears the billboard mode first, so an id that
    // does not set one leaves the particle unbillboarded rather than keeping
    // the previous frame's.
    EffectParticle particle;
    particle.BillboardMode = BillboardMode::Sphere;
    invoke_set_vecs_func(particle, 2, Matrix4{});
    if (particle.BillboardMode != BillboardMode::None
        || particle.EffectVec1.x != 1.0F || particle.EffectVec2.z != 1.0F) {
        std::printf("set-vecs BC mismatch\n");
        return 1;
    }
    invoke_set_vecs_func(particle, 1, Matrix4{});
    if (particle.BillboardMode != BillboardMode::Sphere
        || particle.EffectVec2.y != -1.0F) {
        std::printf("set-vecs B0 mismatch\n");
        return 1;
    }
    // A particle whose alpha has reached zero stops being drawn.
    particle.Alpha = 0.0F;
    particle.ShouldDraw = true;
    invoke_draw_func(particle, 1, 1.0F);
    if (particle.ShouldDraw) {
        std::printf("a zero-alpha particle was still drawn\n");
        return 1;
    }
    // Otherwise the quad is built around the particle's own position.
    particle.Alpha = 1.0F;
    particle.Scale = 2.0F;
    particle.EffectVec1 = {1.0F, 0.0F, 0.0F};
    particle.EffectVec2 = {0.0F, 1.0F, 0.0F};
    invoke_draw_func(particle, 1, 1.0F);
    if (!particle.ShouldDraw || particle.Vertex0.x != -1.0F
        || particle.Vertex0.y != 1.0F || particle.Vertex1.x != 1.0F
        || particle.Vertex2.y != -1.0F || particle.Texcoord2.x != 1.0F) {
        std::printf("draw quad mismatch\n");
        return 1;
    }
    return (sum == 1.5F && v.x == 1.0F && v.y == -1.0F && v.z == 0.5F
            && s.x == 0.5F && s.z == 0.25F && early == 1.0F && late == 2.0F)
        ? 0 : 1;
}
