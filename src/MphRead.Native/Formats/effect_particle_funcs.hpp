#pragma once

// Native counterpart of EffectElementEntry's set-vecs and draw dispatch in
// Formats/Effects.cs.
//
// A particle's shape comes from two numbered functions chosen by the
// element's flags and draw type: the set-vecs function decides the two (or
// three) axes the quad is built on and whether the renderer billboards it,
// and the draw function turns those axes into four corners.
//
// The managed methods are named for the cartridge offsets they came from
// (SetVecsB0, DrawB8, ...); those names are kept so the two trees can be read
// side by side.

#include "Formats/effect_records.hpp"
#include "Formats/Types.hpp"

#include <cstdint>

namespace fruityprime::formats {

// EffectElementEntry.GetFuncIds: which pair of functions an element uses.
struct EffectFuncIds {
    int set_vecs_id = 0;
    int draw_id = 0;
};

// EffectParticle.SetVecsB0 / SetVecsBC / SetVecsC0 / SetVecsD4 / SetVecsD8
void set_vecs_b0(EffectParticle& particle) noexcept;
void set_vecs_bc(EffectParticle& particle) noexcept;
void set_vecs_c0(EffectParticle& particle, const Matrix4& view_matrix) noexcept;
void set_vecs_d4(EffectParticle& particle) noexcept;
void set_vecs_d8(EffectParticle& particle) noexcept;

// EffectParticle.InvokeSetVecsFunc.  The billboard mode is cleared first, so
// an element whose id does not set one is not billboarded.
void invoke_set_vecs_func(EffectParticle& particle, int set_vecs_id,
                          const Matrix4& view_matrix);

// EffectParticle.DrawB8 and the other draw functions build the quad.
void draw_b8(EffectParticle& particle, float scale_factor) noexcept;

// EffectParticle.InvokeDrawFunc.  ShouldDraw and DrawNode are cleared first,
// so a particle whose alpha has reached zero stops being drawn rather than
// repeating its last frame.
void invoke_draw_func(EffectParticle& particle, int draw_id,
                      float scale_factor);

// EffectEntry.ResetElements: restart every element's lifetime from now.  An
// effect that is re-used rather than recreated goes through here, so a
// leftover Expired or Func39Called would make it play only once.
void reset_elements(EffectEntry& entry, float elapsed_time) noexcept;

// EffectEntry.SetReadOnlyField: the four values an entity feeds an effect
// (a beam's colour, a bomb's radius).  They are "read only" from the effect
// script's side; the owner writes them across every element at once.
void set_read_only_field(EffectEntry& entry, int index, float value) noexcept;

} // namespace fruityprime::formats
