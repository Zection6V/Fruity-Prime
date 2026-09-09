#pragma once

// Native counterpart of the effect function dispatch in Formats/Effects.cs.
//
// An effect element is driven by small numbered functions the cartridge
// stores as (id, parameters) pairs.  There are four kinds, and which one a
// call site wants is fixed by where the id came from:
//
//   * a vector function writes a Vector3 in place  (InvokeVecFunc)
//   * a float function returns one number          (InvokeFloatFunc)
//   * a set-vecs function orients the element      (InvokeSetVecsFunc)
//   * a draw function builds its geometry          (InvokeDrawFunc)
//
// The ids are cartridge data, so the dispatch is a switch over them rather
// than a table of pointers: an unknown id has to be visible, not silently do
// nothing.

#include "Formats/effect_records.hpp"
#include "Formats/Types.hpp"

#include <cstdint>
#include <span>

namespace fruityprime::formats {

// Effects.TimeValues
struct TimeValues {
    // Global elapsed time.
    float Global = 0.0F;
    // Time since the current element entry or particle was created.
    float Elapsed = 0.0F;
    // Lifespan of the current element entry or particle.
    float Lifespan = 0.0F;
};

// EffectFuncBase.InvokeVecFunc: writes `vec` in place, as the managed `ref`
// parameter does.
void invoke_vec_func(const EffectFuncBase& owner, const FxFuncInfo& info,
                     TimeValues times, Vector3& vec);

// EffectFuncBase.InvokeFloatFunc.  The two-argument managed overload resolves
// the id from the info first; both spellings are kept because fx15 stores
// function ids as its own parameters and calls the id form directly.
[[nodiscard]] float invoke_float_func(const EffectFuncBase& owner,
                                      const FxFuncInfo& info,
                                      TimeValues times);
[[nodiscard]] float invoke_float_func(const EffectFuncBase& owner,
                                      std::uint32_t func_id,
                                      std::span<const std::int32_t> parameters,
                                      TimeValues times);

// EffectElementEntry.InvokeSetVecsFunc and InvokeDrawFunc are not declared
// here: they read and write the element entry's own orientation and geometry,
// which the effect runtime port does not carry yet.  Declaring them before
// they exist would hide that.

} // namespace fruityprime::formats
