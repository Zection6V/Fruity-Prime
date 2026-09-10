#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace MphRead
{
    namespace Mods
    {
        class ConsoleWindow final
        {
        public:
            static bool OwnsItsConsole();
            static void Prepare(const std::vector<std::string>& args);
            static void Show();

            ConsoleWindow() = delete;
            ConsoleWindow(const ConsoleWindow&) = delete;
            ConsoleWindow& operator=(const ConsoleWindow&) = delete;

        private:
            inline static constexpr int _attachParentProcess = -1;

            static void Rebind();
            static bool HasFlag(
                const std::vector<std::string>& args, std::string_view name);
        };
    }
}
