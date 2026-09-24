#pragma once

#include <string>

namespace MphRead::Mods::Platform
{
    // Installation resources and writable desktop state have different roots.
    class AppPaths final
    {
    public:
        AppPaths() = delete;
        ~AppPaths() = delete;
        AppPaths(const AppPaths&) = delete;
        AppPaths& operator=(const AppPaths&) = delete;
        AppPaths(AppPaths&&) = delete;
        AppPaths& operator=(AppPaths&&) = delete;

        [[nodiscard]] static std::string ExecutableDirectory();
        [[nodiscard]] static std::string ResourceDirectory();
        [[nodiscard]] static std::string Maps();

        // Other desktop platforms keep their existing portable layout. Android
        // sets GameFiles.Root and LauncherPrefs.Directory from its activity.
        [[nodiscard]] static std::string UserDataDirectory();
        [[nodiscard]] static std::string PathsFile();

        static void PrepareUserData();
    };
}
