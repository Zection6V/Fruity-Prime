#pragma once

#include "../../NativeRuntime/System/AtomicSharedPtr.hpp"

#include <memory>
#include <string>

namespace MphRead::Mods::Input
{
    class ControllerLayoutState;
    class GamepadOptionState;
    class PadBindingState;

    // One controller's settings and bindings. The one in force is the active
    // device's, or the fallback when there is none; a frame pins the one it
    // began with on its own thread.
    class GamepadRuntimeConfig final
    {
    public:
        GamepadRuntimeConfig();

        [[nodiscard]] static std::shared_ptr<GamepadRuntimeConfig>& Fallback();
        // [ThreadStatic] Frame.
        [[nodiscard]] static std::shared_ptr<GamepadRuntimeConfig>& Frame();
        [[nodiscard]] static std::shared_ptr<GamepadRuntimeConfig> Current();
        static void Current(std::shared_ptr<GamepadRuntimeConfig> value);

        [[nodiscard]] const std::shared_ptr<GamepadOptionState>& Options() const noexcept { return _options; }
        [[nodiscard]] const std::shared_ptr<PadBindingState>& Bindings() const noexcept { return _bindings; }
        [[nodiscard]] const std::shared_ptr<ControllerLayoutState>& Layout() const noexcept { return _layout; }
        [[nodiscard]] const std::string& ProfileName() const noexcept { return _profileName; }
        void ProfileName(std::string value) { _profileName = std::move(value); }

        [[nodiscard]] std::shared_ptr<GamepadRuntimeConfig> Clone() const;

    private:
        GamepadRuntimeConfig(std::shared_ptr<GamepadOptionState> options, std::shared_ptr<PadBindingState> bindings);

        [[nodiscard]] static ::MphRead::NativeRuntime::AtomicSharedPtr<GamepadRuntimeConfig>& Selected();

        std::shared_ptr<GamepadOptionState> _options;
        std::shared_ptr<PadBindingState> _bindings;
        std::shared_ptr<ControllerLayoutState> _layout;
        std::string _profileName = "Custom settings";
    };
}
