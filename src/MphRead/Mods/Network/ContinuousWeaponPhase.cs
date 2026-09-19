using System;

namespace MphRead.Mods.Network
{
    /// <summary>Per-slot firing clocks for continuous player weapons.</summary>
    internal sealed class ContinuousWeaponPhase
    {
        internal const uint MaxIntentAge = 30;

        private struct Clock
        {
            public ulong Phase;
            public ulong SceneFrame;
            public bool Valid;
        }

        private readonly Clock[] _clocks;

        internal ContinuousWeaponPhase(int slots) => _clocks = new Clock[slots];

        internal void Reset() => Array.Clear(_clocks);

        internal void ResetSlot(int slot)
        {
            if ((uint)slot < (uint)_clocks.Length)
            {
                _clocks[slot] = default;
            }
        }

        // Called on every player simulation step, including steps without a Spawn.
        // A missing packet does not end a held stream; a release, weapon change,
        // or stale intent does. The next fresh shot then seeds a new phase.
        internal void Observe(int slot, ulong sceneFrame, bool continuousHeld, bool intentFresh)
        {
            if (!continuousHeld || !intentFresh)
            {
                ResetSlot(slot);
                return;
            }
            if ((uint)slot < (uint)_clocks.Length)
            {
                Advance(ref _clocks[slot], sceneFrame);
            }
        }

        private static void Advance(ref Clock clock, ulong sceneFrame)
        {
            if (!clock.Valid) return;
            if (sceneFrame < clock.SceneFrame)
            {
                clock = default;
            }
            else if (sceneFrame > clock.SceneFrame)
            {
                clock.Phase++;
                clock.SceneFrame = sceneFrame;
            }
        }

        internal ulong Resolve(int slot, ulong sceneFrame, bool networked, bool localOwner,
            uint netFrame, bool intentValid, uint intentFrame, uint intentAge, out bool shared)
        {
            if (networked && localOwner && netFrame != 0)
            {
                shared = true;
                return netFrame;
            }
            if (networked && !localOwner && (uint)slot < (uint)_clocks.Length
                && intentValid && intentFrame != 0 && intentAge <= MaxIntentAge)
            {
                ref Clock clock = ref _clocks[slot];
                Advance(ref clock, sceneFrame);
                if (!clock.Valid)
                {
                    clock.Phase = (ulong)intentFrame + intentAge;
                    clock.SceneFrame = sceneFrame;
                    clock.Valid = true;
                }
                // Never re-anchor to a later packet: its arrival jitter can
                // repeat or skip a parity even when the trigger never lifted.
                shared = true;
                return clock.Phase;
            }
            ResetSlot(slot);
            shared = false;
            return sceneFrame;
        }
    }
}
