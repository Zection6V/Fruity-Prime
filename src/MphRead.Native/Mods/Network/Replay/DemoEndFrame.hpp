#pragma once

namespace MphRead::Mods::Network
{
    // Terminal playback state: reading EOF alone is not sufficient. The last
    // packets must pass through one full simulation step before holding the
    // world's final image. Reset for any new playback or explicit rewind.
    class DemoEndFrame final
    {
    public:
        void AfterWorldStep(bool active, bool exhausted) noexcept
        {
            if (active && exhausted) _held = true;
        }

        [[nodiscard]] bool Held() const noexcept { return _held; }
        void Reset() noexcept { _held = false; }

    private:
        bool _held = false;
    };
}
