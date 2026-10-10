#pragma once

namespace MphRead::Mods::Network
{
    // The order a remote player's presses take effect in, when several arrive
    // at once.
    //
    // Presses the press history replays land in the same intent, and the game
    // reads the morph before the trigger: a shot fired just before morphing
    // came out of a copy already morphing, which cannot fire, and was lost.
    // While a shot event is still unspent, the morph waits one frame behind it.
    class ReplayedPressOrder final
    {
    public:
        // Whether the morph press takes effect this frame.
        [[nodiscard]] bool MorphPressed(bool morphPressed, bool shootPressed, bool shotPending) noexcept
        {
            if (_morphDeferred && !morphPressed)
            {
                _morphDeferred = false;
                return true;
            }
            if (morphPressed && shootPressed && shotPending)
            {
                _morphDeferred = true;
                return false;
            }
            return morphPressed;
        }

        void Reset() noexcept { _morphDeferred = false; }

    private:
        bool _morphDeferred = false;
    };
}
