#include "UiCommands.hpp"

#include "CommandLine.hpp"

#include "../../NativeRuntime/System/Runtime.hpp"
#include "../ClassicMenu/Frames.hpp"
#if defined(MPHREAD_QT) && !defined(__ANDROID__)
#include "../../../MphRead.Native.Qt/Shell/UiCapture.hpp"
#include "../../../MphRead.Native.Qt/Shell/UiChecks.hpp"
#endif
#if defined(MPHREAD_SHELL)
#include "../Launcher/GuiLauncher.hpp"
#include "../Launcher/Shell.hpp"
#endif

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

namespace
{
    using MphRead::Mods::Entry::HasFlag;
    using MphRead::Mods::Entry::ValueAfter;

    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }

    void SetExitCode(std::int32_t value) noexcept
    {
        MphRead::NativeRuntime::SetEnvironmentExitCode(value);
    }

#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    int RunUiCapture(const std::string& directory)
    {
#if defined(MPHREAD_QT) && !defined(__ANDROID__)
        return MphRead::Qt::UiCapture::Run(directory);
#else
        (void)directory;
        WriteLine("[uishot] this head has no desktop Qt launcher");
        return 1;
#endif
    }

#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    int RunUiBench(const std::vector<std::string>& args)
    {
#if defined(MPHREAD_QT) && !defined(__ANDROID__)
        // Qt's GPU benchmark measures completed render passes, excluding PNG
        // encoding/readback. The old CPU-copy rig's modes are not comparable.
        for (const char* option : {"uibenchslow", "uibenchandroid", "uibenchfree",
                "uibenchonly", "uibenchscale"})
        {
            if (HasFlag(args, option))
            {
                WriteLine(std::string("[uibench] the CPU-raster option is unavailable with Qt: ") + option);
                return 1;
            }
        }
        if (ValueAfter(args, "uibenchsize"))
        {
            WriteLine("[uibench] select resolution with FP_QT_UISHOT_SIZE=WxH");
            return 1;
        }
        const std::string output = ValueAfter(args, "uibenchshot").value_or("qt-uibench-shots");
        const std::string report = ValueAfter(args, "uibench").value_or("qt-uibench.csv");
        return MphRead::Qt::UiCapture::Run(output, 120, report);
#else
        (void)args;
        WriteLine("[uibench] this head has no desktop Qt render-control surface");
        return 1;
#endif
    }

#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    int RunUiDesigns(const std::string& directory)
    {
        (void)directory;
        WriteLine("[uidesign] the six experimental CPU layouts were retired; use -uishot for the Qt screens");
        return 1;
    }

#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    int RunShellCapture(const std::string& directory)
    {
#if defined(MPHREAD_SHELL)
        using namespace MphRead::Mods::Launcher::Gui;
        Shell::RequestShots(directory);
        if (!GuiLauncher::TryRun())
        {
            return 1;
        }
        const std::int32_t misses = Shell::ShotMisses();
        if (misses > 0)
        {
            WriteLine("[shellshot] " + std::to_string(misses) + " step(s) found nothing to press");
            return 1;
        }
        return 0;
#else
        (void)directory;
        WriteLine("[shellshot] this build has no launcher");
        return 1;
#endif
    }

#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    int RunTapCheck()
    {
#if defined(MPHREAD_QT) && !defined(__ANDROID__)
        return MphRead::Qt::RunTapChecks();
#else
        WriteLine("[tapcheck] this build has no launcher");
        return 1;
#endif
    }
}

namespace MphRead::Mods::Entry
{
    bool TryRunUiCommands(const std::vector<std::string>& args)
    {
        const std::optional<std::string> uiShot = ValueAfter(args, "uishot");
        if (uiShot.has_value())
        {
            SetExitCode(RunUiCapture(*uiShot));
            return true;
        }

        if (HasFlag(args, "uibench"))
        {
            SetExitCode(RunUiBench(args));
            return true;
        }

        const std::optional<std::string> uiDesign = ValueAfter(args, "uidesign");
        if (uiDesign.has_value())
        {
            SetExitCode(RunUiDesigns(*uiDesign));
            return true;
        }

        const std::optional<std::string> classicFrames = ValueAfter(args, "classicframes");
        if (classicFrames.has_value())
        {
            // The script is the token *after* the directory, wherever that
            // falls: not a value of its own flag.
            const int index = IndexOfFlag(args, "classicframes");
            const std::size_t script = static_cast<std::size_t>(index) + 2;
            SetExitCode(ClassicMenu::RunFrames(*classicFrames, script < args.size() ? args[script] : std::string()));
            return true;
        }

        const std::optional<std::string> shellShot = ValueAfter(args, "shellshot");
        if (shellShot.has_value())
        {
            SetExitCode(RunShellCapture(*shellShot));
            return true;
        }

        if (HasFlag(args, "tapcheck"))
        {
            SetExitCode(RunTapCheck());
            return true;
        }
        return false;
    }
}
