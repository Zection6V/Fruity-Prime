#include "GamepadMappings.hpp"

#include "GamepadLayout.hpp"
#include "GamepadManager.hpp"
#include "GamepadProfiles.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
#include "../Platform/AppPaths.hpp"
#include "../../NativeRuntime/OpenTK/GLFW.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;
    namespace Glfw = ::OpenTK::Windowing::GraphicsLibraryFramework;

    namespace
    {
        [[nodiscard]] std::string Upper(const std::string& text)
        {
            return Runtime::ToUpperInvariant(text);
        }

        [[nodiscard]] std::string Replace(std::string text, char from, char to)
        {
            std::replace(text.begin(), text.end(), from, to);
            return text;
        }
    }

    GamepadCapabilities GamepadMappings::Capabilities(const std::string& guid, GamepadCapabilities fallback)
    {
        const auto found = MappingCapabilities.find(Upper(guid));
        return found != MappingCapabilities.end() ? found->second : fallback;
    }

    GamepadCapabilities GamepadMappings::ParseCapabilities(const std::string& line)
    {
        bool lx = false, ly = false, rx = false, ry = false, lt = false, rt = false;
        for (const std::string& part : Runtime::StringSplit(line, ','))
        {
            const std::size_t split = part.find(':');
            if (split == std::string::npos)
            {
                continue;
            }
            std::string axis = part.substr(split + 1);
            axis.erase(0, axis.find_first_not_of("+-") == std::string::npos ? axis.size() : axis.find_first_not_of("+-"));
            if (!axis.starts_with('a'))
            {
                continue;
            }
            const std::string name = part.substr(0, split);
            if (name == "leftx") { lx = true; }
            else if (name == "lefty") { ly = true; }
            else if (name == "rightx") { rx = true; }
            else if (name == "righty") { ry = true; }
            else if (name == "lefttrigger") { lt = true; }
            else if (name == "righttrigger") { rt = true; }
        }
        return (lx && ly ? GamepadCapabilities::AnalogLeftStick : GamepadCapabilities::None)
            | (rx && ry ? GamepadCapabilities::AnalogRightStick : GamepadCapabilities::None)
            | (lt && rt ? GamepadCapabilities::AnalogTriggers : GamepadCapabilities::None);
    }

    void GamepadMappings::SaveOverride(const std::string& mapping)
    {
        if (mapping.size() > 4096 || mapping.find('\n') != std::string::npos || mapping.find('\r') != std::string::npos)
        {
            throw System::ArgumentException("Invalid controller mapping.");
        }
        const std::string directory = Launcher::LauncherPrefs::Directory();
        const std::string path = Runtime::PathCombine(directory, std::string(FileName));
        const std::string existing = Runtime::FileExists(path) ? Runtime::FileReadAllText(path) : "";
        Runtime::DirectoryCreateDirectory(directory);
        GamepadProfiles::WriteAtomic(path, ReplaceOverride(existing, mapping));
        _loaded = false;
        ReloadRequested = true;
    }

    std::string GamepadMappings::ReplaceOverride(const std::string& existing, const std::string& mapping)
    {
        const auto key = [](const std::string& line)
        {
            const std::vector<std::string> parts = Runtime::StringSplit(line, ',');
            if (parts.size() < 3 || parts[0].size() != 32)
            {
                return std::string();
            }
            std::string platform;
            for (const std::string& part : parts)
            {
                if (part.starts_with("platform:"))
                {
                    platform = part;
                }
            }
            return Runtime::ToLowerInvariant(parts[0]) + ":" + platform;
        };
        const std::string wanted = key(mapping);
        if (wanted.empty())
        {
            throw System::ArgumentException("Invalid controller mapping GUID.");
        }
        const std::string newLine(Runtime::EnvironmentNewLine());
        std::string output;
        std::string text = existing;
        std::erase(text, '\r');
        for (const std::string& line : Runtime::StringSplit(text, '\n'))
        {
            if (!line.empty() && key(line) != wanted)
            {
                output += line + newLine;
            }
        }
        output += mapping + newLine;
        return output;
    }

    void GamepadMappings::ResetOverrides()
    {
        const std::string path = Runtime::PathCombine(Launcher::LauncherPrefs::Directory(), std::string(FileName));
        if (Runtime::FileExists(path))
        {
            GamepadProfiles::WriteAtomic(path, "# Custom controller mappings reset.\n");
        }
        _loaded = false;
        ReloadRequested = true;
    }

    void GamepadMappings::EnsureLoaded()
    {
        if (_loaded || Runtime::IsAndroid())
        {
            return;
        }
        _loaded = true;
        MappingCapabilities.clear();
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
                files++;
                lines += Count(*text);
                Runtime::ConsoleWriteLine("[input] gamepad mappings: " + std::to_string(Count(*text)) + " from " + path);
            }
        }
        const std::optional<std::string> config = Runtime::EnvironmentGetVariable("SDL_GAMECONTROLLERCONFIG");
        if (config.has_value() && !Runtime::StringIsNullOrWhiteSpace(*config) && Apply(*config))
        {
            files++;
            lines += Count(*config);
            Runtime::ConsoleWriteLine("[input] gamepad mappings: " + std::to_string(Count(*config)) + " from "
                "SDL_GAMECONTROLLERCONFIG");
        }
        _summary = files == 0
            ? "no extra mappings loaded"
            : std::to_string(lines) + " extra mapping(s) from " + std::to_string(files) + " source(s)";
    }

    std::vector<std::string> GamepadMappings::Paths(const std::optional<std::string>& resourceDirectory,
        const std::optional<std::string>& settingsDirectory)
    {
        const std::string beside = Runtime::PathCombine(
            resourceDirectory.has_value() ? *resourceDirectory : Platform::AppPaths::ResourceDirectory(), std::string(FileName));
        const std::string settings = Runtime::PathCombine(
            settingsDirectory.has_value() ? *settingsDirectory : Launcher::LauncherPrefs::Directory(), std::string(FileName));
        return beside == settings ? std::vector<std::string>{beside} : std::vector<std::string>{beside, settings};
    }

    std::optional<std::string> GamepadMappings::TryRead(const std::string& path)
    {
        try
        {
            return Runtime::FileExists(path) ? std::optional<std::string>(Runtime::FileReadAllText(path)) : std::nullopt;
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
            const bool applied = Glfw::GLFW::UpdateGamepadMappings(text);
            if (applied)
            {
                for (const std::string& raw : Runtime::StringSplit(text, '\n'))
                {
                    const std::string line = Runtime::StringTrim(raw);
                    const std::vector<std::string> parts = Runtime::StringSplit(line, ',');
                    if (parts.size() < 3 || parts[0].size() != 32)
                    {
                        continue;
                    }
                    bool compatible = true;
                    for (const std::string& part : parts)
                    {
                        if (part.starts_with("platform:") && part != "platform:" + Platform())
                        {
                            compatible = false;
                        }
                    }
                    if (compatible)
                    {
                        MappingCapabilities[Upper(parts[0])] = ParseCapabilities(raw);
                    }
                }
            }
            return applied;
        }
        catch (const Glfw::GlfwUnavailableException&)
        {
            return false;
        }
    }

    std::int32_t GamepadMappings::Count(const std::string& text)
    {
        std::int32_t count = 0;
        for (const std::string& line : Runtime::StringSplit(text, '\n'))
        {
            const std::string trimmed = Runtime::StringTrim(line);
            if (!trimmed.empty() && !trimmed.starts_with('#'))
            {
                count++;
            }
        }
        return count;
    }

    std::string GamepadMappings::Suggest(std::int32_t slot)
    {
        const std::string guid = Glfw::GLFW::GetJoystickGUID(slot).value_or("00000000000000000000000000000000");
        const std::string name = Replace(Glfw::GLFW::GetJoystickName(slot).value_or("gamepad"), ',', ' ');
        const GamepadLayout layout = GamepadLayout::For(slot);
        return DescribeLayout(guid, name, layout, Platform());
    }

    std::optional<std::string> GamepadMappings::CompatibleMacXboxMapping(const std::string& guid,
        const std::string& name, std::int32_t axes, std::int32_t buttons, std::int32_t hats, bool macOS)
    {
        if (!GamepadLayout::IsMacXboxBluetooth(guid, axes, buttons, hats, macOS))
        {
            return std::nullopt;
        }
        return DescribeLayout(guid, Replace(name, ',', ' '),
            GamepadLayout::Select(guid, axes, buttons, hats, macOS), "Mac OS X");
    }

    bool GamepadMappings::TryMapMacXbox(std::int32_t slot)
    {
        if (!Runtime::IsMacOS() || Glfw::GLFW::JoystickIsGamepad(slot))
        {
            return false;
        }
        const std::optional<std::string> mapping = CompatibleMacXboxMapping(
            Glfw::GLFW::GetJoystickGUID(slot).value_or(""),
            Glfw::GLFW::GetJoystickName(slot).value_or("Xbox controller"),
            static_cast<std::int32_t>(Glfw::GLFW::GetJoystickAxes(slot).size()),
            static_cast<std::int32_t>(Glfw::GLFW::GetJoystickButtons(slot).size()),
            static_cast<std::int32_t>(Glfw::GLFW::GetJoystickHats(slot).size()), true);
        return mapping.has_value() && Apply(*mapping) && Glfw::GLFW::JoystickIsGamepad(slot);
    }

    std::string GamepadMappings::DescribeLayout(const std::string& guid, const std::string& name,
        const GamepadLayout& layout, const std::string& platform)
    {
        const auto n = [](std::int32_t value) { return std::to_string(value); };
        std::string text = guid + "," + name + ",";
        text += "a:b" + n(layout.ButtonA) + ",b:b" + n(layout.ButtonB) + ",";
        text += "x:b" + n(layout.ButtonX) + ",y:b" + n(layout.ButtonY) + ",";
        text += "leftshoulder:b" + n(layout.ButtonLeftBumper) + ",";
        text += "rightshoulder:b" + n(layout.ButtonRightBumper) + ",";
        text += "back:b" + n(layout.ButtonBack) + ",start:b" + n(layout.ButtonStart) + ",";
        text += "leftstick:b" + n(layout.ButtonLeftThumb) + ",";
        text += "rightstick:b" + n(layout.ButtonRightThumb) + ",";
        text += "leftx:a" + n(layout.AxisLeftX) + ",lefty:a" + n(layout.AxisLeftY) + ",";
        text += "rightx:a" + n(layout.AxisRightX) + ",righty:a" + n(layout.AxisRightY) + ",";
        text += layout.AxisLeftTrigger >= 0 ? "lefttrigger:a" + n(layout.AxisLeftTrigger) + ","
            : "lefttrigger:b" + n(layout.ButtonLeftTrigger) + ",";
        text += layout.AxisRightTrigger >= 0 ? "righttrigger:a" + n(layout.AxisRightTrigger) + ","
            : "righttrigger:b" + n(layout.ButtonRightTrigger) + ",";
        text += "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,";
        text += "platform:" + platform + ",";
        return text;
    }

    std::string GamepadMappings::Platform()
    {
        if (Runtime::IsWindows())
        {
            return "Windows";
        }
        if (Runtime::IsMacOS())
        {
            return "Mac OS X";
        }
        if (Runtime::IsAndroid())
        {
            return "Android";
        }
        return "Linux";
    }
}
