#include "ControllerLayoutState.hpp"

#include "GamepadOptionState.hpp"
#include "PadBindingState.hpp"

namespace MphRead::Mods::Input
{
    ControllerLayoutState::ControllerLayoutState(std::shared_ptr<GamepadOptionState> options,
        std::shared_ptr<PadBindingState> bindings)
        : _options(std::move(options)), _bindings(std::move(bindings))
    {
    }

    const std::string& ControllerLayoutState::Name() const
    {
        return _bindings->Preset();
    }

    bool ControllerLayoutState::Southpaw() const
    {
        return _options->Southpaw;
    }

    void ControllerLayoutState::Southpaw(bool value)
    {
        _options->Southpaw = value;
        _bindings->Preset("Custom");
    }

    void ControllerLayoutState::Apply(const std::string& name)
    {
        _bindings->ApplyPreset(name);
        if (name != "Custom")
        {
            _options->Southpaw = name == "Southpaw";
        }
    }
}
