#include "GamepadMappingWizard.hpp"

#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>

namespace MphRead::Mods::Input
{
    namespace
    {
        constexpr std::array<std::pair<std::string_view, std::string_view>, 20> Steps{{
            {"a", "Press the south face button (A / Cross)."}, {"b", "Press the east face button (B / Circle)."},
            {"x", "Press the west face button (X / Square)."}, {"y", "Press the north face button (Y / Triangle)."},
            {"leftshoulder", "Press the left bumper."}, {"rightshoulder", "Press the right bumper."},
            {"back", "Press Back / Share."}, {"start", "Press Start / Options."},
            {"leftstick", "Click the left stick."}, {"rightstick", "Click the right stick."},
            {"dpup", "Press D-pad up."}, {"dpright", "Press D-pad right."},
            {"dpdown", "Press D-pad down."}, {"dpleft", "Press D-pad left."},
            {"leftx", "Push the left stick fully right."}, {"lefty", "Push the left stick fully up."},
            {"rightx", "Push the right stick fully right."}, {"righty", "Push the right stick fully up."},
            {"lefttrigger", "Fully squeeze the left trigger."}, {"righttrigger", "Fully squeeze the right trigger."}}};
    }

    GamepadMappingWizard::GamepadMappingWizard(GamepadRawSample rest) : _rest(std::move(rest))
    {
    }

    bool GamepadMappingWizard::Complete() const noexcept
    {
        return Step() == static_cast<std::int32_t>(Steps.size()) && !_release;
    }

    std::string GamepadMappingWizard::Prompt() const
    {
        if (_release)
        {
            return "Release the control and center both sticks.";
        }
        if (Step() == static_cast<std::int32_t>(Steps.size()))
        {
            return "Mapping complete. Apply to save it for this controller.";
        }
        return std::to_string(Step() + 1) + "/" + std::to_string(Steps.size()) + ": "
            + std::string(Steps[static_cast<std::size_t>(Step())].second);
    }

    void GamepadMappingWizard::Sample(const GamepadRawSample& sample)
    {
        if (sample.DeviceId != _rest.DeviceId || sample.Axes.size() != _rest.Axes.size()
            || sample.Buttons.size() != _rest.Buttons.size() || sample.Hats.size() != _rest.Hats.size())
        {
            throw System::InvalidOperationException("Controller changed. Restart mapping.");
        }
        if (_release)
        {
            bool resting = true;
            for (std::size_t i = 0; i < sample.Axes.size(); i++)
            {
                resting &= std::abs(sample.Axes[i] - _rest.Axes[i]) < .2F;
            }
            for (std::size_t i = 0; i < sample.Buttons.size(); i++)
            {
                resting &= sample.Buttons[i] == _rest.Buttons[i];
            }
            for (std::size_t i = 0; i < sample.Hats.size(); i++)
            {
                resting &= sample.Hats[i] == _rest.Hats[i];
            }
            if (resting)
            {
                _release = false;
            }
            return;
        }
        if (Complete())
        {
            return;
        }
        const std::string key(Steps[static_cast<std::size_t>(Step())].first);
        std::optional<std::string> binding;
        const bool stick = (key.ends_with("x") && key.size() > 1) || (key.ends_with("y") && key.size() > 1);
        if (!stick)
        {
            for (std::size_t i = 0; i < sample.Buttons.size(); i++)
            {
                if (sample.Buttons[i] && !_rest.Buttons[i])
                {
                    binding = "b" + std::to_string(i);
                    break;
                }
            }
            if (key.starts_with("dp"))
            {
                for (std::size_t i = 0; i < sample.Hats.size(); i++)
                {
                    const std::uint8_t hat = sample.Hats[i];
                    if (hat == 1 || hat == 2 || hat == 4 || hat == 8)
                    {
                        binding = "h" + std::to_string(i) + "." + std::to_string(hat);
                        break;
                    }
                }
            }
        }
        if (stick || key.ends_with("trigger"))
        {
            for (std::size_t i = 0; i < sample.Axes.size(); i++)
            {
                const float delta = sample.Axes[i] - _rest.Axes[i];
                if (!std::isfinite(delta) || std::abs(delta) < .65F)
                {
                    continue;
                }
                if (stick)
                {
                    binding = "a" + std::to_string(i) + ((key.ends_with("y") ? delta > 0 : delta < 0) ? "~" : "");
                }
                else
                {
                    binding = std::abs(_rest.Axes[i]) < .2F
                        ? std::string(delta > 0 ? "+a" : "-a") + std::to_string(i)
                        : "a" + std::to_string(i) + (delta < 0 ? "~" : "");
                }
                break;
            }
        }
        if (!binding.has_value())
        {
            return;
        }
        std::string trimmed = *binding;
        while (!trimmed.empty() && trimmed.back() == '~')
        {
            trimmed.pop_back();
        }
        if (!_used.insert(trimmed).second)
        {
            return;
        }
        _bindings.push_back(key + ":" + *binding);
        _release = true;
    }

    std::string GamepadMappingWizard::Mapping() const
    {
        if (!Complete())
        {
            throw System::InvalidOperationException("Finish every mapping step first.");
        }
        if (_rest.Guid.size() != 32 || !std::all_of(_rest.Guid.begin(), _rest.Guid.end(),
            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }))
        {
            throw System::InvalidOperationException("Invalid controller identifier.");
        }
        std::string name;
        for (const char c : _rest.Name)
        {
            if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7f && c != ',')
            {
                if (name.size() == 100)
                {
                    break;
                }
                name += c;
            }
        }
        const std::string platform = ::MphRead::NativeRuntime::IsMacOS() ? "Mac OS X"
            : ::MphRead::NativeRuntime::IsWindows() ? "Windows" : "Linux";
        std::string joined;
        for (std::size_t i = 0; i < _bindings.size(); i++)
        {
            joined += (i == 0 ? "" : ",") + _bindings[i];
        }
        return _rest.Guid + "," + name + "," + joined + ",platform:" + platform + ",";
    }
}
