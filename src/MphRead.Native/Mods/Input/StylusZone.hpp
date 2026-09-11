#pragma once

#include <cstdint>
#include <new>
#include <optional>
#include <string>

namespace MphRead::Mods::Input
{
    enum class StylusRegion : std::int32_t
    {
        None = 0,
        Aim = 1,
        PowerBeam = 2,
        Missile = 3,
        Weapons = 4,
        WeaponSelect = 5,
        AltForm = 6
    };

    class StylusZone final
    {
    public:
        struct Button final
        {
            const StylusRegion Region = StylusRegion::None;
            const float X = 0.0F;
            const float Y = 0.0F;
            const float Radius = 0.0F;
            const std::optional<std::string> Label{};

            Button() noexcept = default;
            Button(StylusRegion region, float x, float y, float radius,
                std::optional<std::string> label);
            Button(const Button&) = default;

            Button& operator=(const Button& other)
            {
                if (this != &other)
                {
                    this->~Button();
                    ::new (static_cast<void*>(this)) Button(other);
                }
                return *this;
            }
        };

        StylusZone() = delete;
        StylusZone(const StylusZone&) = delete;
        StylusZone(StylusZone&&) = delete;
        StylusZone& operator=(const StylusZone&) = delete;
        StylusZone& operator=(StylusZone&&) = delete;

        inline static constexpr float DsWidth = 256.0F;
        inline static constexpr float DsHeight = 192.0F;

        static Button (&Buttons)[5];

        [[nodiscard]] static bool Enabled() noexcept;
        static void Enabled(bool value) noexcept;

        [[nodiscard]] static float Left() noexcept;
        [[nodiscard]] static float Top() noexcept;
        [[nodiscard]] static float Width() noexcept;
        [[nodiscard]] static float Height() noexcept;

        [[nodiscard]] static float AspectCorrection() noexcept;
        static void AspectCorrection(float value) noexcept;

        [[nodiscard]] static float Opacity() noexcept;
        static void Opacity(float value) noexcept;

        static void SetRect(float left, float top, float width) noexcept;

        [[nodiscard]] static bool Placing() noexcept;
        static void BeginPlacement() noexcept;
        static void CancelPlacement() noexcept;
        static void PlacementDown(float x, float y) noexcept;
        static void PlacementDrag(float x, float y) noexcept;
        static void PlacementUp() noexcept;

        [[nodiscard]] static StylusRegion Region() noexcept;
        [[nodiscard]] static bool Contact() noexcept;
        [[nodiscard]] static StylusRegion Pressed() noexcept;

        static void Update(float x, float y, bool contact) noexcept;
        [[nodiscard]] static StylusRegion RegionAt(float x, float y) noexcept;
        [[nodiscard]] static bool OnButton() noexcept;
        static void Reset() noexcept;

    private:
        static Button _buttons[5];

        static bool _enabled;
        static float _left;
        static float _top;
        static float _width;
        static float _aspectCorrection;
        static float _opacity;

        static bool _placing;
        static float _placeAnchorX;
        static float _placeAnchorY;
        static bool _placeAnchored;

        static StylusRegion _region;
        static bool _contact;
        static StylusRegion _pressed;
        static StylusRegion _lastRegion;
        static bool _lastContact;
    };
}
