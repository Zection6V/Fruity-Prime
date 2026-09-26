#include "GamepadRuntimeConfig.hpp"

#include "ControllerLayoutState.hpp"
#include "GamepadOptionState.hpp"
#include "PadBindingState.hpp"

namespace MphRead::Mods::Input
{
    GamepadRuntimeConfig::GamepadRuntimeConfig()
        : GamepadRuntimeConfig(std::make_shared<GamepadOptionState>(), std::make_shared<PadBindingState>())
    {
    }

    GamepadRuntimeConfig::GamepadRuntimeConfig(std::shared_ptr<GamepadOptionState> options,
        std::shared_ptr<PadBindingState> bindings)
        : _options(std::move(options)), _bindings(std::move(bindings)),
          _layout(std::make_shared<ControllerLayoutState>(_options, _bindings))
    {
    }

    std::shared_ptr<GamepadRuntimeConfig>& GamepadRuntimeConfig::Fallback()
    {
        static std::shared_ptr<GamepadRuntimeConfig> fallback = std::make_shared<GamepadRuntimeConfig>();
        return fallback;
    }

    ::MphRead::NativeRuntime::AtomicSharedPtr<GamepadRuntimeConfig>& GamepadRuntimeConfig::Selected()
    {
        static ::MphRead::NativeRuntime::AtomicSharedPtr<GamepadRuntimeConfig> selected{Fallback()};
        return selected;
    }

    std::shared_ptr<GamepadRuntimeConfig>& GamepadRuntimeConfig::Frame()
    {
        thread_local std::shared_ptr<GamepadRuntimeConfig> frame{};
        return frame;
    }

    std::shared_ptr<GamepadRuntimeConfig> GamepadRuntimeConfig::Current()
    {
        const std::shared_ptr<GamepadRuntimeConfig>& frame = Frame();
        return frame != nullptr ? frame : Selected().load();
    }

    void GamepadRuntimeConfig::Current(std::shared_ptr<GamepadRuntimeConfig> value)
    {
        Selected().store(std::move(value));
        Frame() = nullptr;
    }

    std::shared_ptr<GamepadRuntimeConfig> GamepadRuntimeConfig::Clone() const
    {
        std::shared_ptr<GamepadRuntimeConfig> copy(new GamepadRuntimeConfig(_options->Clone(), _bindings->Clone()));
        copy->_profileName = _profileName;
        return copy;
    }
}
