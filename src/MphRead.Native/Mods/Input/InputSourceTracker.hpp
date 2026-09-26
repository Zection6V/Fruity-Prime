#pragma once

#include <cstdint>

namespace MphRead::Mods::Input
{
    enum class InputSource : std::int32_t { KeyboardMouse, Gamepad, Touch };

    class InputSourceTracker final
    {
    public:
        InputSourceTracker() = delete;

        [[nodiscard]] static InputSource Current() noexcept { return _current; }
        static void Reset() noexcept;
        static void Note(InputSource source);
        static void Note(InputSource source, std::int64_t milliseconds) noexcept;

    private:
        inline static InputSource _current = InputSource::KeyboardMouse;
        inline static std::int64_t _changed = 0;
    };
}
