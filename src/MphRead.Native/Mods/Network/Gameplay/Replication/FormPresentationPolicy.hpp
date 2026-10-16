#pragma once

#include "Mods/Network/Gameplay/Replication/ReplicatedFormState.hpp"

#include <cstdint>

namespace MphRead::Mods::Network
{
    // Decides when a sampled stable form owns the displayed form state.
    // Gameplay reconciliation still uses FormReconciliation and its grace.
    class FormPresentationPolicy final
    {
    public:
        FormPresentationPolicy() = delete;

        [[nodiscard]] static bool RequiresStableCorrection(const ReplicatedFormState& sample,
            bool actualAlt, bool morphing, bool unmorphing) noexcept
        {
            return sample.IsValid()
                && sample.Direction == ReplicatedFormState::Transition::None
                && (sample.AltForm != actualAlt || morphing || unmorphing);
        }

        // How an alt-form action relayed from the owner meets the replica's
        // presented form. Inputs replay on the intent clock; the form is read
        // a few frames behind it on the playout clock, so a bomb or an attack
        // can arrive while the presented morph clip is still playing.
        enum class AltAction : std::uint8_t
        {
            None,     // already settled in alt form
            Resume,   // let the clip run on from where it is (bomb: spawns alongside)
            Complete  // finish the clip now: the native attack needs a settled alt form
        };
        [[nodiscard]] static AltAction ForAltAction(bool completeRequired,
            bool actualAlt, bool morphing, bool unmorphing) noexcept
        {
            if (actualAlt && !morphing && !unmorphing) return AltAction::None;
            return completeRequired ? AltAction::Complete : AltAction::Resume;
        }

        // After a completed alt action, samples still behind it (biped, or the
        // morph clip on its way) must not pull the replica back into the clip.
        // A settled alt sample is the clock catching up; an unmorph sample is
        // the owner's next transition. Either ends the hold.
        static constexpr std::uint32_t AltCommitCeilingFrames = 120;
        [[nodiscard]] static bool AltCommitHolds(const ReplicatedFormState& sample,
            std::uint32_t framesSinceCommit) noexcept
        {
            if (!sample.IsValid() || framesSinceCommit > AltCommitCeilingFrames) return false;
            if (sample.Direction == ReplicatedFormState::Transition::Unmorph) return false;
            return !(sample.Direction == ReplicatedFormState::Transition::None && sample.AltForm);
        }
    };
}
