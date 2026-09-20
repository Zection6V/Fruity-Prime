#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>

namespace MphRead::Mods::Update
{
    class Version;
}

namespace MphRead::Mods::Launcher
{
    class GameFiles final
    {
    public:
        GameFiles() = delete;

        [[nodiscard]] static std::string Root();
        static void Root(std::string value);

        [[nodiscard]] static bool Ready();
        [[nodiscard]] static std::optional<std::string> Problem();
        [[nodiscard]] static std::string Describe();
        // Desktop setup keeps its redirected-output readers scope-bound so
        // exceptional unwinding reaches RunSetup's managed-equivalent catch.
        [[nodiscard]] static bool RunSetup(
            const std::string& romPath,
            const std::function<void(const std::string&)>& report);
        static void ApplyPaths();
        [[nodiscard]] static bool InProcessSetup() noexcept;

    private:
        class ReportWriter;

        [[nodiscard]] static std::string PathsFile();
        [[nodiscard]] static bool RunSetupHere(
            const std::string& romPath,
            const std::function<void(const std::string&)>& report);

        static std::string _root;
        static const MphRead::Mods::Update::Version _minExtractVersion;
    };
}
