#pragma once

namespace MphRead::Mods
{
    class SpectatorCameraPolicy final
    {
    public:
        SpectatorCameraPolicy() = delete;

        // Do not leave the multiplayer intro active behind the demo camera.
        // The normal game controls the intro lifecycle independently.
        [[nodiscard]] static constexpr bool EndIntroForPlayback(
            bool playback, bool matchInProgress, bool introActive) noexcept
        {
            return playback && matchInProgress && introActive;
        }

        [[nodiscard]] static constexpr bool RetryPlaybackFollow(
            bool playback, bool pending, bool hasTarget) noexcept
        {
            return playback && pending && !hasTarget;
        }

        [[nodiscard]] static constexpr bool CanFollowPlayer(bool spectating,
            bool hasMainPlayer, bool hasTarget, bool targetIsCurrent) noexcept
        {
            return hasMainPlayer && (!spectating || (hasTarget && targetIsCurrent));
        }

        [[nodiscard]] static constexpr bool CanBorrowLocalPose(bool spectating,
            bool hasTarget, bool mainIsLocal, bool hasLocalPlayer) noexcept
        {
            return spectating && !hasTarget && mainIsLocal && hasLocalPlayer;
        }
    };
}
