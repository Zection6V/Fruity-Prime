#include "Formats/effects.hpp"
#include "Utility/rng.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>

int main() {
    using fruityprime::effects::EvaluationState;
    using fruityprime::effects::Evaluator;
    using fruityprime::effects::Function;
    using fruityprime::effects::FunctionTable;
    using fruityprime::effects::TimeValues;

    FunctionTable functions;
    functions.emplace(4, Function{42, {4096}});
    functions.emplace(8, Function{42, {8192}});
    functions.emplace(12, Function{46, {4, 8}});
    functions.emplace(16, Function{47, {8, 4}});
    functions.emplace(20, Function{49, {8, 4, 4, 8}});
    functions.emplace(24, Function{4, {4096, 8192, 12288}});
    functions.emplace(28, Function{17, {24, 24}});
    functions.emplace(32, Function{20, {4, 24}});
    functions.emplace(36, Function{40, {2048, 4096, 8192}});
    functions.emplace(40, Function{41,
                                    {0, 0, 2048, 4096, 4096, 8192,
                                     INT32_MIN}});
    functions.emplace(44, Function{13, {42, 52}});
    functions.emplace(48, Function{1, {}});
    functions.emplace(52, Function{42, {8192}});
    functions.emplace(56, Function{3, {}});
    // The three arithmetic pairs, scalar and componentwise.  These
    // were one body branching on the identifier before the functions
    // were split apart, so each is checked separately: multiply
    // reading as add is the shape of mistake that split invites.
    functions.emplace(60, Function{48, {8, 4}});
    functions.emplace(64, Function{18, {24, 24}});
    functions.emplace(68, Function{19, {24, 24}});
    // The four read-only and four read-write fields.
    functions.emplace(72, Function{31, {}});
    functions.emplace(76, Function{34, {}});
    functions.emplace(80, Function{35, {}});
    functions.emplace(84, Function{38, {}});

    fruityprime::utility::Rng rng;
    Evaluator evaluator(functions, rng);
    EvaluationState state;
    state.position = {4.0F, 5.0F, 6.0F};
    state.transform_position = {7.0F, 8.0F, 9.0F};
    state.alpha = 0.75F;
    state.portion_total = 0.125F;
    const TimeValues times{0.75F, 0.75F, 1.0F};

    float scalar = 0.0F;
    assert(evaluator.evaluate_float(4, times, state, scalar));
    assert(std::abs(scalar - 1.0F) < 0.0001F);
    assert(evaluator.evaluate_float(12, times, state, scalar));
    assert(std::abs(scalar - 3.0F) < 0.0001F);
    assert(evaluator.evaluate_float(16, times, state, scalar));
    assert(std::abs(scalar - 1.0F) < 0.0001F);
    assert(evaluator.evaluate_float(20, times, state, scalar));
    assert(std::abs(scalar - 1.0F) < 0.0001F);
    assert(evaluator.evaluate_float(36, times, state, scalar));
    assert(std::abs(scalar - 2.0F) < 0.0001F);
    assert(evaluator.evaluate_float(40, times, state, scalar));
    assert(std::abs(scalar - 1.5F) < 0.0001F);

    fruityprime::formats::Vector3 vector;
    assert(evaluator.evaluate_vector(24, times, state, vector));
    assert(vector.x == 1.0F && vector.y == 2.0F && vector.z == 3.0F);
    assert(evaluator.evaluate_vector(28, times, state, vector));
    assert(vector.x == 2.0F && vector.y == 4.0F && vector.z == 6.0F);
    assert(evaluator.evaluate_vector(32, times, state, vector));
    assert(vector.x == 1.0F && vector.y == 2.0F && vector.z == 3.0F);
    const TimeValues angle_times{0.25F, 0.25F, 1.0F};
    assert(evaluator.evaluate_vector(44, angle_times, state, vector));
    assert(std::abs(vector.x - 0.7071067F) < 0.001F);
    assert(std::abs(vector.z - 0.7071067F) < 0.001F);

    // 8192 times 4096 in fixed point is two.
    assert(evaluator.evaluate_float(60, times, state, scalar));
    assert(std::abs(scalar - 2.0F) < 0.0001F);
    // Subtracting a vector from itself is zero; multiplying squares it.
    assert(evaluator.evaluate_vector(64, times, state, vector));
    assert(vector.x == 0.0F && vector.y == 0.0F && vector.z == 0.0F);
    assert(evaluator.evaluate_vector(68, times, state, vector));
    assert(vector.x == 1.0F && vector.y == 4.0F && vector.z == 9.0F);

    // The numbered fields are read by index, first and last of each.
    state.read_only_fields = {1.0F, 2.0F, 3.0F, 4.0F};
    state.read_write_fields = {5.0F, 6.0F, 7.0F, 8.0F};
    assert(evaluator.evaluate_float(72, times, state, scalar));
    assert(scalar == 1.0F);
    assert(evaluator.evaluate_float(76, times, state, scalar));
    assert(scalar == 4.0F);
    assert(evaluator.evaluate_float(80, times, state, scalar));
    assert(scalar == 5.0F);
    assert(evaluator.evaluate_float(84, times, state, scalar));
    assert(scalar == 8.0F);

    state.element_context = true;
    // An element has no numbered fields of its own -- they are set on
    // a particle when it is created -- so asking is a question with no
    // answer rather than a zero.
    assert(!evaluator.evaluate_float(72, times, state, scalar));
    assert(!evaluator.evaluate_float(84, times, state, scalar));
    assert(evaluator.evaluate_vector(48, times, state, vector));
    assert(vector.x == 7.0F && vector.y == 8.0F && vector.z == 9.0F);
    assert(!evaluator.evaluate_vector(56, times, state, vector));
    assert(!evaluator.evaluate_float(999, times, state, scalar));

    return 0;
}
