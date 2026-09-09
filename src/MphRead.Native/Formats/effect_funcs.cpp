#include "Formats/effect_funcs.hpp"

#include <cmath>
#include <stdexcept>

namespace fruityprime::formats {
namespace {

constexpr float Pi = 3.14159265358979323846F;

[[nodiscard]] float to_float(std::int32_t raw) noexcept {
    return static_cast<float>(raw) / 4096.0F;
}

[[nodiscard]] float degrees_to_radians(float degrees) noexcept {
    return degrees * Pi / 180.0F;
}

// Rng.GetRandomInt1: the cartridge's own generator, so an effect replays the
// same way from the same seed.  The state is per-process in the managed code.
std::uint32_t g_rng_state = 1;

[[nodiscard]] std::int32_t random_int1(std::int32_t range) noexcept {
    if (range <= 0) {
        return 0;
    }
    g_rng_state = g_rng_state * 1103515245u + 12345u;
    return static_cast<std::int32_t>((g_rng_state >> 16)
                                     % static_cast<std::uint32_t>(range));
}

[[nodiscard]] std::int32_t param_at(std::span<const std::int32_t> parameters,
                                    std::size_t index) noexcept {
    return index < parameters.size() ? parameters[index] : 0;
}

// The managed code indexes Funcs by the parameter's value, which is the
// function's file offset.
[[nodiscard]] const FxFuncInfo* func_at(const EffectFuncBase& owner,
                                        std::int32_t offset) {
    const auto found = owner.Funcs.find(static_cast<std::uint32_t>(offset));
    return found == owner.Funcs.end() ? nullptr : &found->second;
}

[[nodiscard]] float call_float(const EffectFuncBase& owner,
                               std::span<const std::int32_t> parameters,
                               std::size_t index, TimeValues times) {
    const FxFuncInfo* info = func_at(owner, param_at(parameters, index));
    return info == nullptr ? 0.0F : invoke_float_func(owner, *info, times);
}

[[nodiscard]] Vector3 call_vec(const EffectFuncBase& owner,
                               std::span<const std::int32_t> parameters,
                               std::size_t index, TimeValues times) {
    Vector3 result{};
    const FxFuncInfo* info = func_at(owner, param_at(parameters, index));
    if (info != nullptr) {
        invoke_vec_func(owner, *info, times, result);
    }
    return result;
}

} // namespace

void invoke_vec_func(const EffectFuncBase& owner, const FxFuncInfo& info,
                     TimeValues times, Vector3& vec) {
    const std::span<const std::int32_t> p(info.Parameters);
    switch (info.FuncId) {
    case 1:
    case 2:
    case 3:
    case 11:
        // FxFunc01, FxFunc03 and FxFunc11 are abstract in the managed base:
        // the element entry supplies them from its own state, so there is
        // nothing to compute here.
        break;
    case 4:
        vec = {to_float(param_at(p, 0)), to_float(param_at(p, 1)),
               to_float(param_at(p, 2))};
        break;
    case 5:
        vec = {to_float(random_int1(4096)), to_float(random_int1(4096)),
               to_float(random_int1(4096))};
        break;
    case 6:
        vec = {to_float(random_int1(4096)), 0.0F, to_float(random_int1(4096))};
        break;
    case 7:
        vec = {to_float(random_int1(4096)), 1.0F, to_float(random_int1(4096))};
        break;
    case 8:
        vec = {to_float(random_int1(4096)) - 0.5F,
               to_float(random_int1(4096)) - 0.5F,
               to_float(random_int1(4096)) - 0.5F};
        break;
    case 9:
        vec = {to_float(random_int1(4096)) - 0.5F, 0.0F,
               to_float(random_int1(4096)) - 0.5F};
        break;
    case 10:
        vec = {to_float(random_int1(4096)) - 0.5F, 1.0F,
               to_float(random_int1(4096)) - 0.5F};
        break;
    case 13: {
        // The managed code walks forward from the stored offset until it
        // finds a function, because fx13's second parameter points into the
        // middle of the table rather than at an entry.
        std::uint32_t offset = static_cast<std::uint32_t>(param_at(p, 1));
        const FxFuncInfo* target = nullptr;
        for (int step = 0; step < 64; ++step) {
            const auto found = owner.Funcs.find(offset);
            if (found != owner.Funcs.end()) {
                target = &found->second;
                break;
            }
            offset += 4;
        }
        float value = 0.0F;
        if (target != nullptr) {
            value = invoke_float_func(
                owner, static_cast<std::uint32_t>(param_at(p, 0)),
                target->Parameters, times);
        }
        float percent = value == 0.0F ? 0.0F : times.Elapsed / value;
        if (value < 0.0F) {
            percent *= -1.0F;
        }
        const float angle = degrees_to_radians(360.0F * percent);
        vec = {std::sin(angle), 0.0F, std::cos(angle)};
        break;
    }
    case 14: {
        const Vector3 temp = call_vec(owner, p, 0, times);
        const float value = call_float(owner, p, 1, times);
        float div = value == 0.0F ? 0.0F : times.Elapsed / value;
        if (value < 0.0F) {
            div *= -1.0F;
        }
        vec = {temp.x * div, temp.y * div, temp.z * div};
        break;
    }
    case 15: {
        const float value1 = call_float(owner, p, 0, times);
        const float value2 = call_float(owner, p, 1, times);
        const float angle = degrees_to_radians(
            static_cast<float>(random_int1(0xFFFF) >> 4) * (360.0F / 4096.0F));
        vec = {std::sin(angle) * value1, value2, std::cos(angle) * value1};
        break;
    }
    case 16: {
        const float value1 = call_float(owner, p, 0, times);
        const float value2 = call_float(owner, p, 1, times);
        vec = {(to_float(random_int1(4096)) - 0.5F) * value1, 0.0F,
               (to_float(random_int1(4096)) - 0.5F) * value2};
        break;
    }
    case 17: {
        const Vector3 a = call_vec(owner, p, 0, times);
        const Vector3 b = call_vec(owner, p, 1, times);
        vec = {a.x + b.x, a.y + b.y, a.z + b.z};
        break;
    }
    case 18: {
        const Vector3 a = call_vec(owner, p, 0, times);
        const Vector3 b = call_vec(owner, p, 1, times);
        vec = {a.x - b.x, a.y - b.y, a.z - b.z};
        break;
    }
    case 19: {
        const Vector3 a = call_vec(owner, p, 0, times);
        const Vector3 b = call_vec(owner, p, 1, times);
        vec = {a.x * b.x, a.y * b.y, a.z * b.z};
        break;
    }
    case 20: {
        const float value = call_float(owner, p, 0, times);
        const Vector3 temp = call_vec(owner, p, 1, times);
        vec = {temp.x * value, temp.y * value, temp.z * value};
        break;
    }
    default:
        throw std::runtime_error("effect vector function id is not known: "
                                 + std::to_string(info.FuncId));
    }
}

float invoke_float_func(const EffectFuncBase& owner, const FxFuncInfo& info,
                        TimeValues times) {
    return invoke_float_func(owner, info.FuncId, info.Parameters, times);
}

float invoke_float_func(const EffectFuncBase& owner, std::uint32_t func_id,
                        std::span<const std::int32_t> parameters,
                        TimeValues times) {
    switch (func_id) {
    case 21:
    case 28:
        // FxFunc21 returns zero; id 28 shares it in the managed switch.
        return 0.0F;
    case 22: case 23: case 24: case 25: case 26: case 27:
    case 29: case 30: case 31: case 32: case 33: case 34:
    case 35: case 36: case 37: case 38: case 39:
        // Abstract in the managed base: supplied by the element entry from
        // its own state.
        return 0.0F;
    case 40:
        return times.Lifespan != 0.0F
                && times.Elapsed / times.Lifespan
                    <= to_float(param_at(parameters, 0))
            ? to_float(param_at(parameters, 1))
            : to_float(param_at(parameters, 2));
    case 41: {
        // A keyframe ramp: pairs of (percent, value) terminated by INT_MIN.
        const float percent =
            times.Lifespan == 0.0F ? 0.0F : times.Elapsed / times.Lifespan;
        if (percent < to_float(param_at(parameters, 0))) {
            return to_float(param_at(parameters, 1));
        }
        bool none = true;
        std::size_t i = 0;
        do {
            if (to_float(param_at(parameters, i)) > percent) {
                break;
            }
            none = false;
            i += 2;
        } while (i < parameters.size()
                 && parameters[i] != (std::numeric_limits<std::int32_t>::min)());
        if (none) {
            return 0.0F;
        }
        i -= 2;
        if (i + 2 >= parameters.size()
            || parameters[i + 2]
                == (std::numeric_limits<std::int32_t>::min)()) {
            return to_float(param_at(parameters, i + 1));
        }
        const float low = to_float(param_at(parameters, i));
        const float high = to_float(param_at(parameters, i + 2));
        const float from = to_float(param_at(parameters, i + 1));
        const float to = to_float(param_at(parameters, i + 3));
        if (high == low) {
            return from;
        }
        return from + (to - from) * ((percent - low) / (high - low));
    }
    case 42:
        return to_float(param_at(parameters, 0));
    case 43:
        return to_float(random_int1(4096));
    case 44:
        return to_float(random_int1(4096)) - 0.5F;
    case 45:
        // A random angle in [0, 360) as fx32.
        return to_float(random_int1(0x168000));
    case 46:
        return call_float(owner, parameters, 0, times)
            + call_float(owner, parameters, 1, times);
    case 47:
        return call_float(owner, parameters, 0, times)
            - call_float(owner, parameters, 1, times);
    case 48:
        return call_float(owner, parameters, 0, times)
            * call_float(owner, parameters, 1, times);
    case 49:
        return call_float(owner, parameters, 0, times)
                >= call_float(owner, parameters, 1, times)
            ? call_float(owner, parameters, 2, times)
            : call_float(owner, parameters, 3, times);
    default:
        throw std::runtime_error("effect float function id is not known: "
                                 + std::to_string(func_id));
    }
}

} // namespace fruityprime::formats
