#include "Mods/Network/Gameplay/Player/NetPlayerLifecycle.hpp"
#include "Mods/Network/Gameplay/Replication/NetSmoothing.hpp"
#include "Mods/Network/Protocol/NetProtocol.hpp"
#include "Mods/Network/Testing/NetSessionFixture.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>

using namespace MphRead::Mods::Network;
using OpenTK::Mathematics::Vector3;

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
        }
        return condition;
    }

    PlayerState StateAt(std::uint32_t frame)
    {
        PlayerState state{};
        state.SlotIndex = 1;
        state.SlotGeneration = 1;
        state.LifeId = 1;
        state.Flags = PlayerState::FlagActive | PlayerState::FlagSpawned;
        state.Position = Vector3(static_cast<float>(frame), 0, 0);
        state.Health = 100;
        if (frame <= 29)
        {
            state.WeavelFlags = PlayerState::WeavelFlagMorphing;
            state.SetFormTransitionProgress(frame <= 28 ? 0 : 15);
        }
        else
        {
            state.Flags |= PlayerState::FlagAltForm;
        }
        return state;
    }

    PlayerState UnmorphStateAt(std::uint32_t frame)
    {
        PlayerState state = StateAt(frame);
        state.SlotIndex = 2;
        if (frame <= 29)
        {
            state.Flags |= PlayerState::FlagAltForm;
            state.WeavelFlags = PlayerState::WeavelFlagUnmorphing;
            state.SetFormTransitionProgress(frame <= 28 ? 0 : 15);
        }
        else
        {
            state.Flags = static_cast<std::uint8_t>(state.Flags & ~PlayerState::FlagAltForm);
            state.WeavelFlags = 0;
        }
        return state;
    }
}

int main()
{
    const NetSessionFixture::State previousSession = NetSessionFixture::Capture();
    const bool previousEnabled = NetSmoothing::Enabled();
    NetSessionFixture::SetRole(NetRole::Client);
    NetSessionFixture::SetLocalSlot(0);
    NetSessionFixture::RevokeAuthority();
    NetPlayerLifecycle::Reset();
    NetPlayerLifecycle::SetOccupant(1, 1);
    NetPlayerLifecycle::SetOccupant(2, 1);
    NetSmoothing::Reset();
    NetSmoothing::Enabled(true);

    bool passed = true;
    passed &= Check(NetSmoothing::PresentationOwnsRemoteReplicas(),
        "an interpolated client owns remote form presentation before its sample ring starts");
    NetSmoothing::Enabled(false);
    passed &= Check(!NetSmoothing::PresentationOwnsRemoteReplicas(),
        "disabling interpolation restores the immediate remote form-input path");
    NetSmoothing::Enabled(true);
    NetSessionFixture::SetRole(NetRole::Host);
    passed &= Check(!NetSmoothing::PresentationOwnsRemoteReplicas(),
        "the authoritative host retains remote form-input simulation");
    NetSessionFixture::SetRole(NetRole::Client);

    for (std::uint32_t frame = 1; frame <= 30; ++frame)
    {
        const std::array<PlayerState, 2> states{ StateAt(frame), UnmorphStateAt(frame) };
        for (const PlayerState& state : states)
        {
            passed &= Check(NetPlayerLifecycle::AcceptState(state, frame),
                "the test state belongs to the active slot generation and life");
        }
        NetSmoothing::Record(frame, std::span<const PlayerState>(states));

        NetSmoothing::TrackedPosition bomb{};
        bomb.Sequence = 7;
        bomb.Position = Vector3(static_cast<float>(frame * 10), 0, 0);
        bomb.Present = frame < 29;
        NetSmoothing::RecordTrackedPositions(frame, states[0].SlotIndex,
            std::span<const NetSmoothing::TrackedPosition>(&bomb, 1));
    }

    // Snap to newest-delay (frame 28), then let all visual tracks read the same fractional frame.
    NetSmoothing::Tick();
    NetSmoothing::Tick();
    Vector3 playerPosition{};
    bool altForm = true;
    ReplicatedFormState form{};
    Vector3 bombPosition{};
    bool bombPresent = false;
    passed &= Check(NetSmoothing::Sample(1, playerPosition, altForm)
        && std::abs(playerPosition.X - 28.95F) < 0.02F && !altForm,
        "player presentation samples the shared fractional playout frame");
    passed &= Check(NetSmoothing::SampleForm(1, form)
        && form.Direction == ReplicatedFormState::Transition::Morph && form.Progress == 14
        && form.HeadingAlt(),
        "a remote form transition samples direction and progress at the shared presentation frame");
    ReplicatedFormState unmorph{};
    passed &= Check(NetSmoothing::SampleForm(2, unmorph)
        && unmorph.Direction == ReplicatedFormState::Transition::Unmorph
        && unmorph.Progress == 14 && !unmorph.HeadingAlt() && unmorph.AltForm,
        "an unmorph clip samples the same presentation clock while its authority alt-form bit remains set");
    passed &= Check(NetSmoothing::SampleTrackedPosition(1, 7, bombPosition, bombPresent)
        && bombPresent && std::abs(bombPosition.X - 280.0F) < 0.02F,
        "a moving bomb holds its last visible pose until playout reaches its gone frame");

    NetSmoothing::Tick();
    passed &= Check(NetSmoothing::SampleTrackedPosition(1, 7, bombPosition, bombPresent)
        && !bombPresent && std::abs(bombPosition.X - 290.0F) < 0.02F,
        "the bomb disappears when the shared playout reaches the gone frame");
    passed &= Check(NetSmoothing::SampleForm(1, form)
        && form.Direction == ReplicatedFormState::Transition::Morph && form.Progress == 15,
        "the remote animation reaches its final sampled phase before the stable-form snapshot");
    passed &= Check(NetSmoothing::SampleForm(2, unmorph)
        && unmorph.Direction == ReplicatedFormState::Transition::Unmorph && unmorph.Progress == 15,
        "the unmorph animation reaches its final sampled phase before the stable-form snapshot");

    NetSmoothing::Tick();
    passed &= Check(NetSmoothing::SampleForm(1, form)
        && form.Direction == ReplicatedFormState::Transition::None && form.AltForm,
        "morph commits to the stable alt form only when the shared read clock reaches it");
    passed &= Check(NetSmoothing::SampleForm(2, unmorph)
        && unmorph.Direction == ReplicatedFormState::Transition::None && !unmorph.AltForm,
        "unmorph commits to the stable biped form only when the shared read clock reaches it");

    NetSmoothing::Reset();
    PlayerState sparseBefore = StateAt(100);
    sparseBefore.Position = Vector3(0, 0, 0);
    sparseBefore.Flags = static_cast<std::uint8_t>(sparseBefore.Flags & ~PlayerState::FlagAltForm);
    sparseBefore.WeavelFlags = PlayerState::WeavelFlagMorphing;
    sparseBefore.SetFormTransitionProgress(0);
    PlayerState sparseAfter = sparseBefore;
    sparseAfter.Position = Vector3(3, 0, 0);
    sparseAfter.SetFormTransitionProgress(12);
    const std::array<PlayerState, 1> beforeStates{ sparseBefore };
    const std::array<PlayerState, 1> afterStates{ sparseAfter };
    passed &= Check(NetPlayerLifecycle::AcceptState(sparseBefore, 100)
        && NetPlayerLifecycle::AcceptState(sparseAfter, 103),
        "sparse timeline snapshots retain the active slot generation and life");
    NetSmoothing::Record(100, std::span<const PlayerState>(beforeStates));
    NetSmoothing::Record(103, std::span<const PlayerState>(afterStates));
    NetSmoothing::Tick();
    NetSmoothing::Tick();
    NetSmoothing::Tick();
    passed &= Check(NetSmoothing::Sample(1, playerPosition, altForm)
        && std::abs(playerPosition.X - 1.14F) < 0.05F && !altForm,
        "a dropped snapshot frame is interpolated from the nearest received positions");
    passed &= Check(NetSmoothing::SampleForm(1, form)
        && form.Direction == ReplicatedFormState::Transition::Morph && form.Progress == 5,
        "form animation progress uses the same sparse-frame interpolation as its position");

    NetPlayerLifecycle::SetOccupant(1, 0);
    NetPlayerLifecycle::Reset();
    NetSmoothing::Reset();
    NetSmoothing::Enabled(previousEnabled);
    NetSessionFixture::Restore(previousSession);
    if (!passed)
    {
        return 1;
    }
    std::cout << "Network smoothing timeline checks passed.\n";
    return 0;
}
