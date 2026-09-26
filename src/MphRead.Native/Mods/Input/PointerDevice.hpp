#pragma once

#include <cstdint>
#include <utility>

namespace MphRead::Entities
{
    class Keybind;
}

namespace MphRead::Mods::Input
{
    enum class PointerDeviceType : std::int32_t { Unknown, Mouse, Pen, Touch };

    struct PointerSample
    {
        PointerDeviceType Device = PointerDeviceType::Unknown;
        std::uint32_t Id = 0;
        float X = 0;
        float Y = 0;
        bool PrimaryDown = false;
        bool InContact = false;
        bool InRange = false;
        float Pressure = 0;
        float TiltX = 0;
        float TiltY = 0;

        friend bool operator==(const PointerSample&, const PointerSample&) = default;
    };

    class PointerDevice final
    {
    public:
        PointerDevice() = delete;

        [[nodiscard]] static const PointerSample& Current() noexcept { return _current; }
        [[nodiscard]] static bool Active() noexcept { return _active; }
        [[nodiscard]] static bool PrimaryDown() noexcept { return _primaryDown; }
        static void Update(const PointerSample& sample, std::int32_t width, std::int32_t height,
            bool independentPrimaryDown = false, bool acceptsInput = true);
        [[nodiscard]] static bool ResolvePrimary(bool tipDown, bool independentDown, bool captured) noexcept;
        [[nodiscard]] static std::pair<float, float> TakeDelta() noexcept;
        static void Reset() noexcept;

    private:
        inline static PointerSample _current{};
        inline static bool _active = false;
        inline static bool _primaryDown = false;
        inline static bool _acceptingInput = false;
        inline static float _pendingX = 0;
        inline static float _pendingY = 0;
    };

    class PointerBindings final
    {
    public:
        [[nodiscard]] bool Down() const noexcept { return _down; }
        [[nodiscard]] bool PreviousDown() const noexcept { return _previousDown; }
        void Update(bool rawDown, bool captured, bool independentDown = false) noexcept;
        bool Resolve(Entities::Keybind& control) const;

    private:
        bool _down = false;
        bool _previousDown = false;
    };
}
