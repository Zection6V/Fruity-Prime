#pragma once

#include <future>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Mods::Launcher
{
    // "Open a file", asked of the operating system rather than of the toolkit:
    // the launcher's headless top-level has no storage provider behind it, so
    // comdlg32 on Windows, zenity or kdialog on Linux, and osascript on macOS.
    class NativeFilePicker final
    {
    public:
        NativeFilePicker() = delete;

        // The window a dialog belongs to (an HWND on Windows); zero elsewhere.
        [[nodiscard]] static void* Owner() noexcept { return _owner; }
        static void Owner(void* value) noexcept { _owner = value; }

        // Answer as a machine with no dialog would, for -shellshot.
        [[nodiscard]] static bool Suppressed() noexcept { return _suppressed; }
        static void Suppressed(bool value) noexcept { _suppressed = value; }

        // Whether this machine has a dialog this can open.
        [[nodiscard]] static bool Available();

        // One existing file; nothing when cancelled or when there was nothing to ask.
        [[nodiscard]] static std::shared_future<std::optional<std::string>> OpenFile(
            const std::string& title, const std::string& description, const std::string& extension);

    private:
        static constexpr int MaxPath = 32768;
        // OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER
        static constexpr int OpenFileFlags = 0x800 | 0x1000 | 0x8 | 0x80000;

        static inline void* _owner = nullptr;
        static inline bool _suppressed = false;

        [[nodiscard]] static std::shared_future<std::optional<std::string>> WindowsFile(
            const std::string& title, const std::string& description, const std::string& extension, void* owner);
        [[nodiscard]] static std::optional<std::string> LinuxTool();
        [[nodiscard]] static bool OnPath(const std::string& tool);
        [[nodiscard]] static std::optional<std::string> LinuxFile(
            const std::string& title, const std::string& description, const std::string& extension);
        [[nodiscard]] static std::optional<std::string> MacFile(const std::string& title, const std::string& extension);
        [[nodiscard]] static std::optional<std::string> RunTool(
            const std::string& tool, const std::vector<std::string>& arguments);
    };
}
