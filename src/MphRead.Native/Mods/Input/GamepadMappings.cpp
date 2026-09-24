#include "GamepadMappings.hpp"

#include "GamepadLayout.hpp"
#include "../../NativeRuntime/OpenTK/GLFW.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <sys/auxv.h>
#if !defined(__ANDROID__)
#include <dlfcn.h>
#include <unistd.h>
#endif
#elif defined(__unix__)
#if !defined(__ANDROID__)
#include <dlfcn.h>
#endif
#endif

using ::MphRead::NativeRuntime::AppContextBaseDirectory;
using ::MphRead::NativeRuntime::EnvironmentGetVariable;
using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::FileReadAllText;
using ::MphRead::NativeRuntime::PathCombine;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::StringTrimView;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::Utf8GetString;

namespace Glfw = ::OpenTK::Windowing::GraphicsLibraryFramework;

namespace
{
    [[nodiscard]] std::string ToUtf8(const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        return std::string(
            reinterpret_cast<const char*>(value.data()), value.size());
#else
        return path.u8string();
#endif
    }

}

namespace MphRead::Mods::Input
{
    std::atomic_bool GamepadMappings::_loaded{false};
    std::string GamepadMappings::_summary = "no extra mappings loaded";

    std::string GamepadMappings::Summary()
    {
        return _summary;
    }

    void GamepadMappings::EnsureLoaded()
    {
#if defined(__ANDROID__)
        return;
#else
        if (_loaded.load(std::memory_order_relaxed))
        {
            return;
        }
        _loaded.store(true, std::memory_order_relaxed);

        std::int32_t files = 0;
        std::int32_t lines = 0;
        for (const std::string& path : Paths())
        {
            const std::optional<std::string> text = TryRead(path);
            if (!text.has_value())
            {
                continue;
            }
            if (Apply(*text))
            {
                files = UncheckedAdd(files, 1);
                lines = UncheckedAdd(lines, Count(*text));
                std::cout << "[input] gamepad mappings: "
                    << Count(*text) << " from " << path << '\n';
            }
        }

        const std::optional<std::string> config = EnvironmentGetVariable("SDL_GAMECONTROLLERCONFIG");
        if (config.has_value()
            && !StringTrimView(*config).empty()
            && Apply(*config))
        {
            files = UncheckedAdd(files, 1);
            lines = UncheckedAdd(lines, Count(*config));
            std::cout << "[input] gamepad mappings: "
                << Count(*config) << " from SDL_GAMECONTROLLERCONFIG\n";
        }

        _summary = files == 0
            ? "no extra mappings loaded"
            : std::to_string(lines) + " extra mapping(s) from "
                + std::to_string(files) + " source(s)";
#endif
    }

    std::vector<std::string> GamepadMappings::Paths()
    {
        const std::string beside = PathCombine(AppContextBaseDirectory(), FileName);
        const std::string settings = PathCombine(
            Launcher::LauncherPrefs::Directory(), FileName);
        return beside == settings
            ? std::vector<std::string>{beside}
            : std::vector<std::string>{beside, settings};
    }

    std::optional<std::string> GamepadMappings::TryRead(
        const std::string& path)
    {
        if (!FileExists(path))
        {
            return std::nullopt;
        }
        try
        {
            return FileReadAllText(path);
        }
        catch (const System::IO::IOException&)
        {
            return std::nullopt;
        }
        catch (const System::UnauthorizedAccessException&)
        {
            return std::nullopt;
        }
    }

    bool GamepadMappings::Apply(const std::string& text)
    {
        try
        {
            return Glfw::GLFW::UpdateGamepadMappings(text);
        }
        catch (const Glfw::GlfwUnavailableException&)
        {
            return false;
        }
    }

    std::int32_t GamepadMappings::Count(std::string_view text)
    {
        std::uint32_t count = 0;
        std::size_t start = 0;
        for (;;)
        {
            const std::size_t newline = text.find('\n', start);
            const std::string_view line = newline == std::string_view::npos
                ? text.substr(start)
                : text.substr(start, newline - start);
            const std::string_view trimmed = StringTrimView(line);
            if (!trimmed.empty() && trimmed.front() != '#')
            {
                ++count;
            }
            if (newline == std::string_view::npos)
            {
                break;
            }
            start = newline + 1;
        }
        return std::bit_cast<std::int32_t>(count);
    }

    std::string GamepadMappings::Suggest(std::int32_t slot)
    {
        std::string guid = Glfw::GLFW::GetJoystickGUID(slot).value_or(
                "00000000000000000000000000000000");
        std::string name = Glfw::GLFW::GetJoystickName(slot).value_or("gamepad");
        for (char& character : name)
        {
            if (character == ',')
            {
                character = ' ';
            }
        }

        const GamepadLayout layout = GamepadLayout::For(slot);
        std::string text;
        text.reserve(guid.size() + name.size() + 256);
        text.append(guid).push_back(',');
        text.append(name).push_back(',');
        text.append("a:b").append(std::to_string(layout.ButtonA)).append(",");
        text.append("b:b").append(std::to_string(layout.ButtonB)).append(",");
        text.append("x:b").append(std::to_string(layout.ButtonX)).append(",");
        text.append("y:b").append(std::to_string(layout.ButtonY)).append(",");
        text.append("leftshoulder:b").append(
            std::to_string(layout.ButtonLeftBumper)).append(",");
        text.append("rightshoulder:b").append(
            std::to_string(layout.ButtonRightBumper)).append(",");
        text.append("back:b").append(std::to_string(layout.ButtonBack)).append(",");
        text.append("start:b").append(std::to_string(layout.ButtonStart)).append(",");
        text.append("leftstick:b").append(
            std::to_string(layout.ButtonLeftThumb)).append(",");
        text.append("rightstick:b").append(
            std::to_string(layout.ButtonRightThumb)).append(",");
        text.append("leftx:a").append(std::to_string(layout.AxisLeftX)).append(",");
        text.append("lefty:a").append(std::to_string(layout.AxisLeftY)).append(",");
        text.append("rightx:a").append(std::to_string(layout.AxisRightX)).append(",");
        text.append("righty:a").append(std::to_string(layout.AxisRightY)).append(",");
        if (layout.AxisLeftTrigger >= 0)
        {
            text.append("lefttrigger:a").append(
                std::to_string(layout.AxisLeftTrigger)).append(",");
        }
        else
        {
            text.append("lefttrigger:b").append(
                std::to_string(layout.ButtonLeftTrigger)).append(",");
        }
        if (layout.AxisRightTrigger >= 0)
        {
            text.append("righttrigger:a").append(
                std::to_string(layout.AxisRightTrigger)).append(",");
        }
        else
        {
            text.append("righttrigger:b").append(
                std::to_string(layout.ButtonRightTrigger)).append(",");
        }
        text.append("dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,");
        text.append("platform:").append(Platform()).append(",");
        return text;
    }

    std::string GamepadMappings::Platform()
    {
#if defined(_WIN32)
        return "Windows";
#elif defined(__APPLE__) && defined(TARGET_OS_OSX) && TARGET_OS_OSX
        return "Mac OS X";
#elif defined(__ANDROID__)
        return "Android";
#else
        return "Linux";
#endif
    }
}
