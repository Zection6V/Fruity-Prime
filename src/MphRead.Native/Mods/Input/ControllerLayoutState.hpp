#pragma once

#include <memory>
#include <string>

namespace MphRead::Mods::Input
{
    class GamepadOptionState;
    class PadBindingState;

    class ControllerLayoutState final
    {
    public:
        ControllerLayoutState(std::shared_ptr<GamepadOptionState> options, std::shared_ptr<PadBindingState> bindings);

        [[nodiscard]] const std::shared_ptr<PadBindingState>& Bindings() const noexcept { return _bindings; }
        [[nodiscard]] const std::string& Name() const;
        [[nodiscard]] bool Southpaw() const;
        void Southpaw(bool value);
        void Apply(const std::string& name);

    private:
        std::shared_ptr<GamepadOptionState> _options;
        std::shared_ptr<PadBindingState> _bindings;
    };
}
