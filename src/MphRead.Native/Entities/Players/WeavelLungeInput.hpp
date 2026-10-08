#pragma once

#include <utility>

namespace MphRead::Entities
{
    // Capture 60 Hz input edges; consume once at the common 30 Hz decision boundary.
    class WeavelLungeInput final
    {
    public:
        void Capture(bool pressed) noexcept { _pending = _pending || pressed; }
        [[nodiscard]] bool Consume(bool nativeTick) noexcept
        {
            return nativeTick && std::exchange(_pending, false);
        }
        void Reset() noexcept { _pending = false; }
    private:
        bool _pending = false;
    };
}
