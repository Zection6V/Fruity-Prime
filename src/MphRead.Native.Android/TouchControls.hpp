#pragma once

#include "../MphRead.Native/Mods/Input/TouchSettings.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace MphRead::Droid
{
    enum class TouchAction : std::int32_t
    {
        Shoot = 0,
        Jump = 1,
        Morph = 2,
        ScanVisor = 3,
        Scan = 4,
        Missile = 5,
        WeaponMenu = 6,
        Zoom = 7,
        Pause = 8,
        Scoreboard = 9,
        Chat = 10
    };

    class TouchButton final
    {
    public:
        TouchButton(TouchAction action, std::string label);
        TouchButton(const TouchButton&) = delete;
        TouchButton& operator=(const TouchButton&) = delete;
        TouchButton(TouchButton&&) = delete;
        TouchButton& operator=(TouchButton&&) = delete;
        ~TouchButton() = default;

        [[nodiscard]] TouchAction Action() const noexcept;
        [[nodiscard]] std::string Label() const;
        void Relabel(std::optional<std::string_view> label);

        [[nodiscard]] float CentreX() const;
        void CentreX(float value);
        [[nodiscard]] float CentreY() const;
        void CentreY(float value);
        [[nodiscard]] float Radius() const;
        void Radius(float value);
        [[nodiscard]] bool Visible() const;
        void Visible(bool value);

        [[nodiscard]] bool Contains(float x, float y) const;

    private:
        const TouchAction _action;
        mutable std::mutex _lock;
        std::string _label;
        const std::string _defaultLabel;
        float _centreX = 0.0F;
        float _centreY = 0.0F;
        float _radius = 0.0F;
        bool _visible = true;
    };

    class TouchControls final
    {
    public:
        enum class Dir : std::int32_t
        {
            None = 0,
            Up = 1,
            Down = 2,
            Left = 4,
            Right = 8
        };

        friend constexpr Dir operator|(Dir left, Dir right) noexcept
        {
            return static_cast<Dir>(
                static_cast<std::int32_t>(left) | static_cast<std::int32_t>(right)
            );
        }

        friend constexpr Dir operator&(Dir left, Dir right) noexcept
        {
            return static_cast<Dir>(
                static_cast<std::int32_t>(left) & static_cast<std::int32_t>(right)
            );
        }

        friend constexpr Dir& operator|=(Dir& left, Dir right) noexcept
        {
            left = left | right;
            return left;
        }

        struct TapResult
        {
            bool Got;
            float X;
            float Y;
        };

        struct AimDelta
        {
            float X;
            float Y;
        };

        struct SwipeBoostResult
        {
            bool Fired;
            float X;
            float Y;
        };

        struct PositionResult
        {
            bool Down;
            float X;
            float Y;
        };

        TouchControls();
        TouchControls(const TouchControls&) = delete;
        TouchControls& operator=(const TouchControls&) = delete;
        TouchControls(TouchControls&&) = delete;
        TouchControls& operator=(TouchControls&&) = delete;
        ~TouchControls() = default;

        [[nodiscard]] const std::vector<std::shared_ptr<TouchButton>>& Buttons() const noexcept;

        [[nodiscard]] float Width() const;
        [[nodiscard]] float Height() const;
        [[nodiscard]] float Density() const;

        [[nodiscard]] bool StickActive() const;
        [[nodiscard]] float StickX() const;
        [[nodiscard]] float StickY() const;
        [[nodiscard]] float StickKnobX() const;
        [[nodiscard]] float StickKnobY() const;
        [[nodiscard]] float StickRadius() const;
        [[nodiscard]] float StickKnobRadius() const;

        [[nodiscard]] bool SwipeBoostEnabled() const;
        void SwipeBoostEnabled(bool value);

        [[nodiscard]] bool ScanVisorActive() const;
        void ScanVisorActive(bool value);

        [[nodiscard]] bool ChatEnabled() const;
        void ChatEnabled(bool value);

        void SetSpectator(bool spectating, bool freeCamera);
        void SetEndScreen(bool active);
        void SetTapTargets(std::shared_ptr<std::vector<float>> targets);

        [[nodiscard]] TapResult TakeTap();

        void ReloadSettings();

        [[nodiscard]] std::function<void()> Invalidated() const;
        void Invalidated(std::function<void()> value);

        [[nodiscard]] bool PadDriving() const;

        [[nodiscard]] bool ForceVisible() const;
        void ForceVisible(bool value);

        void NotePadActivity();

        void Layout(float width, float height, float density);

        [[nodiscard]] bool IsHeld(TouchAction action) const;
        [[nodiscard]] AimDelta TakeAimDelta();
        [[nodiscard]] SwipeBoostResult TakeSwipeBoost();
        [[nodiscard]] bool TakeDoubleTapJump();

        [[nodiscard]] bool PointerIsAbsolute() const;
        void PointerIsAbsolute(bool value);

        [[nodiscard]] PositionResult AimPosition() const;
        [[nodiscard]] PositionResult WeaponWheelPosition() const;

        [[nodiscard]] Dir Direction() const;

        void PointerDown(std::int32_t pointerId, float x, float y);
        void PointerMove(std::int32_t pointerId, float x, float y);
        void PointerUp(std::int32_t pointerId);
        void ReleaseEverything();

    private:
        struct DisplacementResult
        {
            float Distance;
            float X;
            float Y;
        };

        class SwipeTracker final
        {
        public:
            void Reset() noexcept;
            void Add(float x, float y, std::int64_t now) noexcept;
            [[nodiscard]] DisplacementResult Displacement(
                std::int64_t now,
                std::int64_t windowMs
            ) const;

        private:
            static constexpr std::int32_t Capacity = 12;
            std::array<float, Capacity> _x{};
            std::array<float, Capacity> _y{};
            std::array<std::int64_t, Capacity> _time{};
            std::int32_t _count = 0;
            std::int32_t _newest = -1;
        };

        static constexpr float SwipeBoostDistanceDp = 50.0F;
        static constexpr std::int64_t SwipeBoostWindowMs = 120;
        static constexpr std::int64_t SwipeBoostCooldownMs = 350;
        static constexpr std::int64_t TapMaxMs = 250;
        static constexpr std::int64_t DoubleTapGapMs = 300;
        static constexpr float TapSlopDp = 16.0F;
        static constexpr float DoubleTapSpreadDp = 70.0F;

        void Change(const std::function<bool()>& mutate);
        void ApplyLayoutLocked();
        [[nodiscard]] static Mods::Input::TouchControl SettingOf(TouchAction action) noexcept;

        void NoteTapLocked(float x, float y) noexcept;
        [[nodiscard]] bool TapHitsTargetLocked(float x, float y) const;
        [[nodiscard]] bool TapIsForHudLocked(float x, float y) const noexcept;

        [[nodiscard]] bool HiddenLocked() const noexcept;
        void Settle(bool wasHidden, bool isHidden);
        void InvokeInvalidated();

        void Place(TouchAction action, float x, float y, float radius);
        void PointerDownLocked(std::int32_t pointerId, float x, float y);
        void CheckSwipeBoost(SwipeTracker& tracker, float x, float y);
        void ReleaseAction(TouchAction action);

        mutable std::mutex _lock;

        std::vector<std::shared_ptr<TouchButton>> _buttons;

        float _width = 0.0F;
        float _height = 0.0F;
        float _density = 1.0F;

        bool _stickActive = false;
        float _stickX = 0.0F;
        float _stickY = 0.0F;
        float _stickKnobX = 0.0F;
        float _stickKnobY = 0.0F;
        float _stickRadius = 0.0F;
        float _stickKnobRadius = 0.0F;

        bool _padDriving = false;
        bool _forceVisible = false;

        std::unordered_set<TouchAction> _held;
        std::unordered_map<std::int32_t, TouchAction> _buttonPointers;
        std::int32_t _stickPointer = -1;
        std::int32_t _aimPointer = -1;
        float _aimLastX = 0.0F;
        float _aimLastY = 0.0F;
        float _aimDeltaX = 0.0F;
        float _aimDeltaY = 0.0F;
        float _aimAbsX = 0.0F;
        float _aimAbsY = 0.0F;
        bool _aimDown = false;
        Dir _direction = Dir::None;

        std::int32_t _fireAimPointer = -1;
        float _fireAimLastX = 0.0F;
        float _fireAimLastY = 0.0F;

        std::int32_t _wheelPointer = -1;
        float _wheelX = 0.0F;
        float _wheelY = 0.0F;

        SwipeTracker _aimSwipe;
        SwipeTracker _fireAimSwipe;
        std::int64_t _lastSwipeBoostTime = 0;
        bool _swipeBoostPending = false;
        bool _swipeBoostEnabled = false;
        float _swipeBoostX = 0.0F;
        float _swipeBoostY = 0.0F;

        std::int64_t _tapDownTime = 0;
        float _tapDownX = 0.0F;
        float _tapDownY = 0.0F;
        bool _tapMoved = false;
        std::int64_t _lastTapTime = 0;
        float _lastTapX = 0.0F;
        float _lastTapY = 0.0F;
        bool _doubleTapJumpPending = false;

        bool _scanVisorActive = false;
        bool _chatEnabled = false;
        bool _spectating = false;
        bool _spectatorFreeCam = false;
        bool _endScreen = false;

        std::shared_ptr<std::vector<float>> _tapTargets = std::make_shared<std::vector<float>>();
        bool _tapPending = false;
        float _tapX = 0.0F;
        float _tapY = 0.0F;

        std::function<void()> _invalidated;

        bool _pointerIsAbsolute = false;
    };
}
