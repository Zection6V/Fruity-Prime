#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MphRead
{
    namespace Mods
    {
        class ConsoleWindow final
        {
        public:
            static void Prepare(const std::vector<std::string>& args);
            static void Show();
            [[nodiscard]] static bool OwnsItsConsole();

            ConsoleWindow() = delete;
            ConsoleWindow(const ConsoleWindow&) = delete;
            ConsoleWindow& operator=(const ConsoleWindow&) = delete;

        private:
            inline static constexpr std::uint32_t EnableVirtualTerminalProcessing = 0x0004U;
            inline static constexpr int StdOutputHandle = -11;
            inline static constexpr int SwShow = 5;
            inline static constexpr int AttachParentProcess = -1;

            static void Rebind();
            static bool HasFlag(
                const std::vector<std::string>& args, const std::string& flag);
        };
    }
}
