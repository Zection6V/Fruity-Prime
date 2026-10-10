#pragma once

namespace MphRead::Mods::Network
{
    // The trigger a remote player respawned with.
    //
    // A player respawns by pressing fire, and the press can still be held as
    // the new life begins. The owner's machine does not necessarily fire it --
    // the gun is not up yet -- but their copy here, spawned a moment apart,
    // read the held trigger as Power Beam autofire and fired a shot the owner
    // never did. From a new life until the trigger is first let go, the copy
    // fires only for a shot the owner's machine actually reports (a shot
    // event, NetShotEvents); once released, the trigger is the copy's again.
    class RespawnTriggerGuard final
    {
    public:
        void Arm() noexcept { _armed = true; }

        // Whether the trigger reaches the copy this frame. While the player is
        // dead the trigger is the respawn press itself and always goes through.
        [[nodiscard]] bool TriggerAllowed(bool alive, bool triggerDown, bool triggerPressed, bool shotReported) noexcept
        {
            if (!_armed || !alive)
            {
                return true;
            }
            if (!triggerDown && !triggerPressed)
            {
                _armed = false;
                return true;
            }
            return shotReported;
        }

        [[nodiscard]] bool Armed() const noexcept { return _armed; }

    private:
        bool _armed = false;
    };
}
