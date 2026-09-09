#include "Utility/rng.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fruityprime::utility {

std::uint32_t Rng::call(std::uint32_t& state, std::uint32_t value) noexcept {
    state = state * 0x7FF8A3EDU + 0x2AA01D31U;
    const std::uint64_t product = static_cast<std::uint64_t>(state >> 16)
        * static_cast<std::uint64_t>(value);
    return static_cast<std::uint32_t>(product / 0x10000ULL);
}

std::uint32_t Rng::random1(std::uint32_t value) noexcept {
    return call(rng1_, value);
}

std::uint32_t Rng::random2(std::uint32_t value) noexcept {
    return call(rng2_, value);
}

CameraShakeResult Rng::camera_shake(int shake) noexcept {
    CameraShakeResult result;
    result.before = rng2_;
    while (shake > 0) {
        ++result.frames;
        static_cast<void>(random2(1));
        static_cast<void>(random2(1));
        static_cast<void>(random2(1));
        const std::int64_t next = (3481LL * shake + 2048LL) >> 12;
        shake = next > 0x7fffffffLL ? 0x7fffffff : static_cast<int>(next);
        if (shake < 41) {
            shake = 0;
        }
    }
    result.calls = result.frames * 3;
    result.after = rng2_;
    return result;
}

CameraShakeResult Rng::damage_shake(int damage) noexcept {
    // Keep the conversion order used by C#: damage is first converted to a
    // single-precision value, multiplied by the float literal 40.96f, then
    // truncated to int.  The old native code used integer fixed-point
    // division, which only happens to agree for ordinary small damages.
    const float scaled = static_cast<float>(damage) * 40.96F;
    const int shake = static_cast<int>(std::max(204.0F, scaled));
    return camera_shake(shake);
}

Rng& global_rng() noexcept {
    static Rng instance;
    return instance;
}

std::uint32_t get_random_int1(std::uint32_t value) noexcept {
    return global_rng().random1(value);
}

std::uint32_t get_random_int2(std::uint32_t value) noexcept {
    return global_rng().random2(value);
}

std::uint32_t get_random_int1(std::int32_t value) noexcept {
    return get_random_int1(static_cast<std::uint32_t>(value));
}

std::uint32_t get_random_int2(std::int32_t value) noexcept {
    return get_random_int2(static_cast<std::uint32_t>(value));
}

void set_rng1(std::uint32_t value) noexcept {
    global_rng().set_rng1(value);
}

void set_rng2(std::uint32_t value) noexcept {
    global_rng().set_rng2(value);
}

CameraShakeResult do_camera_shake(int shake) noexcept {
    const auto result = global_rng().camera_shake(shake);
    std::ostringstream output;
    output << "shake " << shake << '\n'
           << result.frames << " frame"
           << (result.frames == 1 ? "" : "s") << ", "
           << result.calls << " calls\n"
           << "rng " << std::uppercase << std::hex << std::setw(8)
           << std::setfill('0') << result.before << " --> "
           << std::setw(8) << result.after;
    std::cout << output.str() << '\n';
    return result;
}

CameraShakeResult do_damage_shake(int damage) noexcept {
    const float scaled = static_cast<float>(damage) * 40.96F;
    const int shake = static_cast<int>(std::max(204.0F, scaled));
    return do_camera_shake(shake);
}

} // namespace fruityprime::utility
