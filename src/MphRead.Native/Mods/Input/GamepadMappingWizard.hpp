#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace MphRead::Mods::Input
{
    struct GamepadRawSample
    {
        std::string DeviceId{};
        std::string Guid{};
        std::string Name{};
        std::vector<float> Axes{};
        std::vector<bool> Buttons{};
        std::vector<std::uint8_t> Hats{};
    };

    class GamepadMappingWizard final
    {
    public:
        explicit GamepadMappingWizard(GamepadRawSample rest);

        inline static std::optional<std::string> RequestedDevice{};
        inline static std::shared_ptr<GamepadRawSample> Latest{};

        [[nodiscard]] std::int32_t Step() const noexcept { return static_cast<std::int32_t>(_bindings.size()); }
        [[nodiscard]] bool Complete() const noexcept;
        [[nodiscard]] std::string Prompt() const;
        void Sample(const GamepadRawSample& sample);
        [[nodiscard]] std::string Mapping() const;

    private:
        GamepadRawSample _rest;
        std::vector<std::string> _bindings{};
        std::set<std::string> _used{};
        bool _release = false;
    };
}
