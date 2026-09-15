#pragma once

#include <cstdint>
#include <optional>

namespace MphRead::Mods
{
    class SpectatorMode final
    {
    public:
        SpectatorMode() = delete;

        [[nodiscard]] static bool IsSpectating() noexcept;
        [[nodiscard]] static bool FreeCamera() noexcept;
        [[nodiscard]] static bool CanSpectate();

        static void Start(bool watchSomeone = false);
        static void CycleNext();
        static void ToggleView();

        [[nodiscard]] static bool ShowScoreboard() noexcept;
        static void NoteScoreboard(bool down) noexcept;
        static void NoteFreeCamera(bool on) noexcept;
        [[nodiscard]] static std::optional<bool> TakeCameraRequest() noexcept;

        static void Rejoin();
        static void Reset() noexcept;

    private:
        static void Switch(std::int32_t slot);
        [[nodiscard]] static std::int32_t FindNextActiveSlot(std::int32_t fromSlot);

        static bool _isSpectating;
        static bool _freeCamera;
        static std::optional<bool> _cameraRequest;
        static bool _showScoreboard;
    };
}
