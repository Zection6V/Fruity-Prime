#pragma once

#include "GamepadManager.hpp"
#include "GamepadState.hpp"
#include "../../NativeRuntime/System/Event.hpp"

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Mods::Input
{
    enum class UiAction : std::int32_t { Up, Down, Left, Right, Accept, Back, PreviousTab, NextTab, PageUp, PageDown };
    enum class GamepadContext : std::int32_t { Gameplay, Menu, BindingCapture, TextEntry, Results };

    // UiAction.ToString().
    [[nodiscard]] std::string ToString(UiAction value);

    class GamepadContexts final
    {
    public:
        GamepadContexts() = delete;

        [[nodiscard]] static std::int64_t Revision() noexcept { return _revision.load(); }
        [[nodiscard]] static bool MenuVisible() noexcept { return _menu.load(); }
        static void MenuVisible(bool value);
        [[nodiscard]] static bool Capturing() noexcept { return _capture.load(); }
        static void Capturing(bool value);
        [[nodiscard]] static bool Focused() noexcept { return _focused.load(); }
        static void Focused(bool value);
        [[nodiscard]] static GamepadContext Current() noexcept { return _current; }
        static void Current(GamepadContext value) noexcept { _current = value; }
        [[nodiscard]] static GamepadContext Resolve(bool textEntry = false, bool results = false);

    private:
        inline static std::atomic_bool _menu{false};
        inline static std::atomic_bool _capture{false};
        inline static std::atomic_bool _focused{true};
        inline static std::atomic<std::int64_t> _revision{0};
        inline static GamepadContext _current = GamepadContext::Gameplay;
    };

    class GamepadEdges final
    {
    public:
        GamepadButtons Update(const GamepadSnapshot& snapshot);

    private:
        GamepadButtons _previous = GamepadButtons::None;
        std::int64_t _revision = -1;
    };

    class GamepadUiRouter final
    {
    public:
        ::MphRead::NativeRuntime::Event<UiAction> Action{};

        void Reset();
        void Update(GamepadSnapshot snapshot, GamepadContext context, std::int64_t milliseconds);

    private:
        GamepadEdges _edges{};
        std::optional<UiAction> _direction{};
        std::int64_t _started = 0;
        std::int64_t _next = 0;
        std::int64_t _revision = -1;
        std::int64_t _contextRevision = -1;
        GamepadContext _context = GamepadContext::Gameplay;
        bool _neutralRequired = false;
        bool _leftTrigger = false;
        bool _rightTrigger = false;
    };
}
