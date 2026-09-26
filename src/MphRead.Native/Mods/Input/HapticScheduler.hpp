#pragma once

#include <array>
#include <cstdint>

namespace MphRead::Mods::Input
{
    enum class GamepadFeedback : std::int32_t;

    class HapticScheduler final
    {
    public:
        HapticScheduler();

        void Reset() noexcept;
        [[nodiscard]] bool Accept(GamepadFeedback feedback, std::int64_t now, std::int32_t duration) noexcept;

    private:
        std::array<std::int64_t, 7> _last{};
        std::int64_t _until = 0;
        std::int32_t _priority = 0;
    };
}
