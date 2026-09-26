#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace MphRead::Mods::Input
{
    enum class StylusRegion : std::int32_t
    {
        None,
        Aim,
        PowerBeam,
        Missile,
        Weapons,
        WeaponSelect,
        AltForm
    };

    // StylusRegion.ToString().
    [[nodiscard]] const char* ToString(StylusRegion value) noexcept;

    class StylusZone final
    {
    public:
        StylusZone() = delete;

        inline static constexpr float DsWidth = 256;
        inline static constexpr float DsHeight = 192;

        struct Button
        {
            StylusRegion Region = StylusRegion::None;
            float X = 0;
            float Y = 0;
            float Radius = 0;
            std::string Label{};
        };

        static const std::array<Button, 5> Buttons;

        [[nodiscard]] static bool Enabled() noexcept;
        static void Enabled(bool value) noexcept { _wanted = value; }
        [[nodiscard]] static bool Wanted() noexcept { return _wanted; }
        [[nodiscard]] static float Left() noexcept { return _left; }
        [[nodiscard]] static float Top() noexcept { return _top; }
        [[nodiscard]] static float Width() noexcept { return _width; }
        [[nodiscard]] static float Height() noexcept { return _width * (DsHeight / DsWidth) * _aspectCorrection; }
        [[nodiscard]] static float AspectCorrection() noexcept { return _aspectCorrection; }
        static void AspectCorrection(float value) noexcept { _aspectCorrection = value; }
        [[nodiscard]] static float Opacity() noexcept { return _opacity; }
        static void Opacity(float value) noexcept { _opacity = value; }
        static void SetRect(float left, float top, float width) noexcept;

        [[nodiscard]] static bool Placing() noexcept { return _placing; }
        static void BeginPlacement() noexcept;
        static void CancelPlacement() noexcept;
        static void PlacementDown(float x, float y) noexcept;
        static void PlacementDrag(float x, float y) noexcept;
        static void Nudge(float dx, float dy) noexcept;
        static void Resize(float by) noexcept;
        static void PlacementUp() noexcept;
        static void CommitPlacement() noexcept;

        [[nodiscard]] static StylusRegion Region() noexcept { return _region; }
        [[nodiscard]] static bool Contact() noexcept { return _contact; }
        [[nodiscard]] static StylusRegion Held() noexcept { return _held; }
        [[nodiscard]] static StylusRegion Pressed() noexcept { return _pressed; }
        [[nodiscard]] static StylusRegion TakePressed() noexcept;
        [[nodiscard]] static bool Aiming() noexcept;
        [[nodiscard]] static bool MenuHeld() noexcept;
        [[nodiscard]] static bool CapturingPointer() noexcept;
        [[nodiscard]] static bool CapturingPrimaryButton() noexcept;

        static void Update(float x, float y, bool contact);
        [[nodiscard]] static StylusRegion RegionAt(float x, float y) noexcept;
        static void Reset() noexcept;

    private:
        static void LogTransitions();

        inline static bool _wanted = false;
        inline static float _left = 0.62F;
        inline static float _top = 0.60F;
        inline static float _width = 0.34F;
        inline static float _aspectCorrection = 16.0F / 9.0F;
        inline static float _opacity = 0.22F;
        inline static bool _placing = false;
        inline static float _placeAnchorX = 0;
        inline static float _placeAnchorY = 0;
        inline static bool _placeAnchored = false;
        inline static StylusRegion _region = StylusRegion::None;
        inline static bool _contact = false;
        inline static StylusRegion _held = StylusRegion::None;
        inline static StylusRegion _pressed = StylusRegion::None;
        inline static bool _loggedContact = false;
        inline static bool _loggedCapture = false;
        inline static bool _lastContact = false;
        inline static bool _aimReady = false;
    };
}
