#pragma once

#include <cstdint>

namespace fruityprime::utility {

struct CameraShakeResult {
    int frames = 0;
    int calls = 0;
    std::uint32_t before = 0;
    std::uint32_t after = 0;
};

// Native counterpart of Utility/Rng.cs.  The arithmetic is deliberately
// performed in unsigned 32-bit space so MinGW, MSVC, and the managed uint
// implementation produce the same sequence.
class Rng {
public:
    static constexpr std::uint32_t Rng1StartValue = 0x3DE9179BU;
    static constexpr std::uint32_t Rng2StartValue = 0U;

    constexpr Rng(std::uint32_t rng1 = Rng1StartValue,
                  std::uint32_t rng2 = Rng2StartValue) noexcept
        : rng1_(rng1), rng2_(rng2) {}

    [[nodiscard]] static std::uint32_t call(std::uint32_t& state,
                                            std::uint32_t value) noexcept;
    [[nodiscard]] static std::uint32_t call(std::uint32_t& state,
                                            std::int32_t value) noexcept {
        return call(state, static_cast<std::uint32_t>(value));
    }
    [[nodiscard]] std::uint32_t random1(std::uint32_t value) noexcept;
    [[nodiscard]] std::uint32_t random2(std::uint32_t value) noexcept;
    void set_rng1(std::uint32_t value) noexcept { rng1_ = value; }
    void set_rng2(std::uint32_t value) noexcept { rng2_ = value; }
    [[nodiscard]] std::uint32_t rng1() const noexcept { return rng1_; }
    [[nodiscard]] std::uint32_t rng2() const noexcept { return rng2_; }

    [[nodiscard]] CameraShakeResult camera_shake(int shake) noexcept;
    [[nodiscard]] CameraShakeResult damage_shake(int damage) noexcept;

private:
    std::uint32_t rng1_;
    std::uint32_t rng2_;
};

// Process-global entry points mirror the static C# API for callers that are
// porting a gameplay path directly.  Tests and replays should prefer an
// explicit Rng instance so state is isolated.
[[nodiscard]] Rng& global_rng() noexcept;
[[nodiscard]] std::uint32_t get_random_int1(std::uint32_t value) noexcept;
[[nodiscard]] std::uint32_t get_random_int2(std::uint32_t value) noexcept;
[[nodiscard]] std::uint32_t get_random_int1(std::int32_t value) noexcept;
[[nodiscard]] std::uint32_t get_random_int2(std::int32_t value) noexcept;
void set_rng1(std::uint32_t value) noexcept;
void set_rng2(std::uint32_t value) noexcept;
[[nodiscard]] CameraShakeResult do_camera_shake(int shake) noexcept;
[[nodiscard]] CameraShakeResult do_damage_shake(int damage) noexcept;

} // namespace fruityprime::utility
