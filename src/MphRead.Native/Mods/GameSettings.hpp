#pragma once

#include <memory>
#include <optional>
#include <string>

namespace MphRead
{
    class MenuSettings;

    namespace Mods
    {
        class GameSettings final
        {
        public:
            [[nodiscard]] static const std::shared_ptr<MenuSettings>& Current() noexcept;

            static void Apply(const std::shared_ptr<MenuSettings>& settings);
            static void ApplyMatchRules();

            GameSettings() = delete;
            GameSettings(const GameSettings&) = delete;
            GameSettings& operator=(const GameSettings&) = delete;

        private:
            [[nodiscard]] static bool TryVolume(
                const std::optional<std::string>& value, float& volume);
            [[nodiscard]] static bool TryTime(
                const std::optional<std::string>& value, float& seconds);

            static std::shared_ptr<MenuSettings> _current;
        };
    }
}
