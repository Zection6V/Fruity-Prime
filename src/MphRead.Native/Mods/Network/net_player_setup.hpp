#pragma once

namespace fruityprime::net {

namespace detail {

struct RuntimeBindings {
    bool (*Active)() noexcept = nullptr;
    int (*LocalSlot)() noexcept = nullptr;
};

void BindRuntime(RuntimeBindings bindings) noexcept;

} // namespace detail

class NetPlayerSetup final {
public:
    static void Reset() noexcept;
    static void ApplyOnce() noexcept;

private:
    inline static bool applied_ = false;
};

} // namespace fruityprime::net
