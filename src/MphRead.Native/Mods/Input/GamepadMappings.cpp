#include "GamepadMappings.hpp"

#include "GamepadLayout.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"

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

namespace
{
    void AppendUtf8(std::string& output, std::uint32_t value)
    {
        if (value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
        {
            value = 0xFFFDU;
        }
        if (value <= 0x7FU)
        {
            output.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (value >> 6)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else if (value <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (value >> 12)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (value >> 18)));
            output.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    [[nodiscard]] std::string DecodeUtf8(std::string_view input)
    {
        std::string output;
        output.reserve(input.size());
        for (std::size_t index = 0; index < input.size();)
        {
            const auto first = static_cast<unsigned char>(input[index]);
            if (first <= 0x7FU)
            {
                output.push_back(static_cast<char>(first));
                ++index;
                continue;
            }

            std::size_t length = 0;
            if (first >= 0xC2U && first <= 0xDFU)
            {
                length = 2;
            }
            else if (first >= 0xE0U && first <= 0xEFU)
            {
                length = 3;
            }
            else if (first >= 0xF0U && first <= 0xF4U)
            {
                length = 4;
            }
            else
            {
                AppendUtf8(output, 0xFFFDU);
                ++index;
                continue;
            }

            std::size_t available = 1;
            while (available < length && index + available < input.size()
                && (static_cast<unsigned char>(input[index + available]) & 0xC0U) == 0x80U)
            {
                ++available;
            }
            if (available < length)
            {
                AppendUtf8(output, 0xFFFDU);
                index += available;
                continue;
            }

            const auto second = static_cast<unsigned char>(input[index + 1]);
            if ((first == 0xE0U && second < 0xA0U)
                || (first == 0xEDU && second >= 0xA0U)
                || (first == 0xF0U && second < 0x90U)
                || (first == 0xF4U && second > 0x8FU))
            {
                AppendUtf8(output, 0xFFFDU);
                ++index;
                continue;
            }

            output.append(input.substr(index, length));
            index += length;
        }
        return output;
    }

    [[nodiscard]] std::string DecodeUtf16(std::string_view input, bool bigEndian)
    {
        std::string output;
        output.reserve(input.size());
        const auto read = [&](std::size_t offset)
        {
            const auto a = static_cast<unsigned char>(input[offset]);
            const auto b = static_cast<unsigned char>(input[offset + 1]);
            return bigEndian
                ? static_cast<std::uint16_t>((a << 8) | b)
                : static_cast<std::uint16_t>(a | (b << 8));
        };

        std::size_t index = 0;
        while (index + 1 < input.size())
        {
            const std::uint16_t first = read(index);
            index += 2;
            if (first >= 0xD800U && first <= 0xDBFFU)
            {
                if (index + 1 < input.size())
                {
                    const std::uint16_t second = read(index);
                    if (second >= 0xDC00U && second <= 0xDFFFU)
                    {
                        index += 2;
                        AppendUtf8(output, 0x10000U
                            + ((static_cast<std::uint32_t>(first) - 0xD800U) << 10)
                            + (static_cast<std::uint32_t>(second) - 0xDC00U));
                        continue;
                    }
                }
                AppendUtf8(output, 0xFFFDU);
            }
            else if (first >= 0xDC00U && first <= 0xDFFFU)
            {
                AppendUtf8(output, 0xFFFDU);
            }
            else
            {
                AppendUtf8(output, first);
            }
        }
        if (index < input.size())
        {
            AppendUtf8(output, 0xFFFDU);
        }
        return output;
    }

    [[nodiscard]] std::string DecodeUtf32(std::string_view input, bool bigEndian)
    {
        std::string output;
        output.reserve(input.size());
        std::size_t index = 0;
        while (index + 3 < input.size())
        {
            const auto a = static_cast<unsigned char>(input[index]);
            const auto b = static_cast<unsigned char>(input[index + 1]);
            const auto c = static_cast<unsigned char>(input[index + 2]);
            const auto d = static_cast<unsigned char>(input[index + 3]);
            index += 4;
            const std::uint32_t value = bigEndian
                ? (static_cast<std::uint32_t>(a) << 24)
                    | (static_cast<std::uint32_t>(b) << 16)
                    | (static_cast<std::uint32_t>(c) << 8)
                    | d
                : a
                    | (static_cast<std::uint32_t>(b) << 8)
                    | (static_cast<std::uint32_t>(c) << 16)
                    | (static_cast<std::uint32_t>(d) << 24);
            AppendUtf8(output, value);
        }
        if (index < input.size())
        {
            AppendUtf8(output, 0xFFFDU);
        }
        return output;
    }

    [[nodiscard]] std::pair<std::uint32_t, std::size_t> DecodeCodePoint(
        std::string_view text, std::size_t offset) noexcept
    {
        const auto first = static_cast<unsigned char>(text[offset]);
        if (first <= 0x7FU)
        {
            return {first, 1};
        }
        if ((first & 0xE0U) == 0xC0U)
        {
            return {
                ((first & 0x1FU) << 6)
                    | (static_cast<unsigned char>(text[offset + 1]) & 0x3FU),
                2
            };
        }
        if ((first & 0xF0U) == 0xE0U)
        {
            return {
                ((first & 0x0FU) << 12)
                    | ((static_cast<unsigned char>(text[offset + 1]) & 0x3FU) << 6)
                    | (static_cast<unsigned char>(text[offset + 2]) & 0x3FU),
                3
            };
        }
        return {
            ((first & 0x07U) << 18)
                | ((static_cast<unsigned char>(text[offset + 1]) & 0x3FU) << 12)
                | ((static_cast<unsigned char>(text[offset + 2]) & 0x3FU) << 6)
                | (static_cast<unsigned char>(text[offset + 3]) & 0x3FU),
            4
        };
    }

    [[nodiscard]] bool IsDotNetWhitespace(std::uint32_t value) noexcept
    {
        if (value >= 0x0009U && value <= 0x000DU)
        {
            return true;
        }
        switch (value)
        {
        case 0x0020U:
        case 0x0085U:
        case 0x00A0U:
        case 0x1680U:
        case 0x2000U:
        case 0x2001U:
        case 0x2002U:
        case 0x2003U:
        case 0x2004U:
        case 0x2005U:
        case 0x2006U:
        case 0x2007U:
        case 0x2008U:
        case 0x2009U:
        case 0x200AU:
        case 0x2028U:
        case 0x2029U:
        case 0x202FU:
        case 0x205FU:
        case 0x3000U:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] std::string_view TrimDotNetWhitespace(
        std::string_view text) noexcept
    {
        std::size_t first = 0;
        std::size_t last = text.size();
        while (first < last)
        {
            const auto [value, length] = DecodeCodePoint(text, first);
            if (!IsDotNetWhitespace(value))
            {
                break;
            }
            first += length;
        }
        while (last > first)
        {
            std::size_t start = last - 1;
            while (start > first
                && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U)
            {
                --start;
            }
            const auto [value, length] = DecodeCodePoint(text, start);
            if (start + length != last || !IsDotNetWhitespace(value))
            {
                break;
            }
            last = start;
        }
        return text.substr(first, last - first);
    }

    [[nodiscard]] std::filesystem::path FromUtf8(std::string_view text)
    {
#if defined(__cpp_char8_t)
        std::u8string value;
        value.reserve(text.size());
        for (const unsigned char character : text)
        {
            value.push_back(static_cast<char8_t>(character));
        }
        return std::filesystem::path(value);
#else
        return std::filesystem::u8path(text);
#endif
    }

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

    [[nodiscard]] std::optional<std::filesystem::path> ProcessPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::nullopt;
            }
            if (length < buffer.size())
            {
                return std::filesystem::path(
                    std::wstring(buffer.data(), static_cast<std::size_t>(length)));
            }
            if (buffer.size()
                > static_cast<std::size_t>(std::numeric_limits<DWORD>::max()) / 2U)
            {
                return std::nullopt;
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 1;
        char probe = 0;
        if (_NSGetExecutablePath(&probe, &size) == 0 || size == 0)
        {
            return std::nullopt;
        }
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            return std::nullopt;
        }
        std::error_code error;
        const std::filesystem::path canonical
            = std::filesystem::canonical(FromUtf8(buffer.data()), error);
        return error ? std::optional<std::filesystem::path>{}
            : std::optional<std::filesystem::path>{canonical};
#elif defined(__linux__)
        std::error_code error;
        const std::filesystem::path procPath
            = std::filesystem::canonical("/proc/self/exe", error);
        if (!error)
        {
            return procPath;
        }
#if defined(AT_EXECFN)
        const auto executable
            = reinterpret_cast<const char*>(getauxval(AT_EXECFN));
        if (executable != nullptr)
        {
            error.clear();
            const std::filesystem::path execPath
                = std::filesystem::canonical(FromUtf8(executable), error);
            if (!error)
            {
                return execPath;
            }
        }
#endif
        return std::nullopt;
#else
        return std::nullopt;
#endif
    }

    [[nodiscard]] std::string BaseDirectory()
    {
        if (const std::optional<std::filesystem::path> process = ProcessPath())
        {
            return ToUtf8(process->parent_path());
        }
        return ToUtf8(std::filesystem::current_path());
    }

    [[nodiscard]] bool IsSeparator(char value) noexcept
    {
#if defined(_WIN32)
        return value == '\\' || value == '/';
#else
        return value == '/';
#endif
    }

    [[nodiscard]] std::string Combine(
        std::string_view directory, std::string_view file)
    {
        if (directory.empty())
        {
            return std::string(file);
        }
        std::string result(directory);
        if (!IsSeparator(result.back())
#if defined(_WIN32)
            && result.back() != ':'
#endif
        )
        {
#if defined(_WIN32)
            result.push_back('\\');
#else
            result.push_back('/');
#endif
        }
        result.append(file);
        return result;
    }

    [[nodiscard]] bool FileExists(std::string_view path) noexcept
    {
        if (path.empty() || path.find('\0') != std::string_view::npos)
        {
            return false;
        }
        try
        {
            std::error_code error;
            const std::filesystem::file_status status
                = std::filesystem::status(FromUtf8(path), error);
            return !error
                && std::filesystem::exists(status)
                && !std::filesystem::is_directory(status);
        }
        catch (...)
        {
            return false;
        }
    }

    [[nodiscard]] std::string ReadAllText(std::string_view path)
    {
        std::ifstream stream(FromUtf8(path), std::ios::binary);
        if (!stream.is_open())
        {
            throw std::ios_base::failure("Could not open gamepad mappings.");
        }
        stream.exceptions(std::ios::badbit);
        const std::string bytes{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        const auto byte = [&](std::size_t index)
        {
            return static_cast<unsigned char>(bytes[index]);
        };
        if (bytes.size() >= 4
            && byte(0) == 0xFFU && byte(1) == 0xFEU
            && byte(2) == 0 && byte(3) == 0)
        {
            return DecodeUtf32(std::string_view(bytes).substr(4), false);
        }
        if (bytes.size() >= 4
            && byte(0) == 0 && byte(1) == 0
            && byte(2) == 0xFEU && byte(3) == 0xFFU)
        {
            return DecodeUtf32(std::string_view(bytes).substr(4), true);
        }
        if (bytes.size() >= 3
            && byte(0) == 0xEFU && byte(1) == 0xBBU && byte(2) == 0xBFU)
        {
            return DecodeUtf8(std::string_view(bytes).substr(3));
        }
        if (bytes.size() >= 2 && byte(0) == 0xFFU && byte(1) == 0xFEU)
        {
            return DecodeUtf16(std::string_view(bytes).substr(2), false);
        }
        if (bytes.size() >= 2 && byte(0) == 0xFEU && byte(1) == 0xFFU)
        {
            return DecodeUtf16(std::string_view(bytes).substr(2), true);
        }
        return DecodeUtf8(bytes);
    }

    [[nodiscard]] std::optional<std::string> EnvironmentVariable()
    {
#if defined(_WIN32)
        constexpr wchar_t Name[] = L"SDL_GAMECONTROLLERCONFIG";
        SetLastError(ERROR_SUCCESS);
        DWORD required = GetEnvironmentVariableW(Name, nullptr, 0);
        if (required == 0)
        {
            return GetLastError() == ERROR_ENVVAR_NOT_FOUND
                ? std::nullopt
                : std::optional<std::string>{""};
        }

        for (;;)
        {
            std::vector<wchar_t> buffer(required);
            SetLastError(ERROR_SUCCESS);
            const DWORD length = GetEnvironmentVariableW(
                Name, buffer.data(), required);
            if (length == 0)
            {
                return GetLastError() == ERROR_ENVVAR_NOT_FOUND
                    ? std::nullopt
                    : std::optional<std::string>{""};
            }
            if (length >= required)
            {
                required = length;
                continue;
            }
            if (length > static_cast<DWORD>(std::numeric_limits<int>::max()))
            {
                throw std::length_error("Environment variable is too long.");
            }
            const int bytes = WideCharToMultiByte(
                CP_UTF8, 0, buffer.data(), static_cast<int>(length),
                nullptr, 0, nullptr, nullptr);
            if (bytes <= 0)
            {
                return std::string{};
            }
            std::string result(static_cast<std::size_t>(bytes), '\0');
            (void)WideCharToMultiByte(
                CP_UTF8, 0, buffer.data(), static_cast<int>(length),
                result.data(), bytes, nullptr, nullptr);
            return result;
        }
#else
        const char* value = std::getenv("SDL_GAMECONTROLLERCONFIG");
        return value == nullptr
            ? std::nullopt
            : std::optional<std::string>{DecodeUtf8(value)};
#endif
    }

    class GlfwBindingUnavailable final : public std::runtime_error
    {
    public:
        explicit GlfwBindingUnavailable(const char* procedure)
            : std::runtime_error(
                std::string("GLFW binding unavailable: ") + procedure)
        {
        }
    };

    using UpdateMappings = int (*)(const char*);
    using JoystickString = const char* (*)(int);

#if defined(_WIN32)
    [[nodiscard]] HMODULE GlfwModule() noexcept
    {
        static HMODULE module = []() noexcept -> HMODULE
        {
            for (const wchar_t* name : {L"glfw3.3.dll", L"glfw3.dll", L"glfw.dll"})
            {
                if (HMODULE handle = GetModuleHandleW(name))
                {
                    return handle;
                }
                if (HMODULE handle = LoadLibraryW(name))
                {
                    return handle;
                }
            }
            return nullptr;
        }();
        return module;
    }

    template <typename T>
    [[nodiscard]] T GlfwProc(const char* name) noexcept
    {
        const HMODULE module = GlfwModule();
        return module == nullptr
            ? nullptr
            : reinterpret_cast<T>(GetProcAddress(module, name));
    }
#elif defined(__APPLE__) || (defined(__unix__) && !defined(__ANDROID__))
    [[nodiscard]] std::optional<std::filesystem::path>
        GlfwExecutableDirectory() noexcept
    {
        try
        {
#if defined(__APPLE__)
            std::uint32_t size = 1;
            char probe = 0;
            if (_NSGetExecutablePath(&probe, &size) == 0 || size == 0)
            {
                return std::nullopt;
            }
            std::vector<char> buffer(size);
            if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            {
                return std::nullopt;
            }
            return std::filesystem::path(buffer.data()).parent_path();
#elif defined(__linux__)
            std::vector<char> buffer(256);
            for (;;)
            {
                const ssize_t length = readlink(
                    "/proc/self/exe", buffer.data(), buffer.size());
                if (length < 0)
                {
                    return std::nullopt;
                }
                if (static_cast<std::size_t>(length) < buffer.size())
                {
                    return std::filesystem::path(std::string(
                        buffer.data(), static_cast<std::size_t>(length))).parent_path();
                }
                buffer.resize(buffer.size() * 2U);
            }
#else
            return std::nullopt;
#endif
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    [[nodiscard]] void* GlfwModule() noexcept
    {
        static void* module = []() noexcept -> void*
        {
#if defined(__APPLE__)
            constexpr const char* names[] = {
                "glfw.3.3.dylib", "libglfw.3.3.dylib",
                "glfw.3.dylib", "libglfw.3.dylib",
                "glfw.dylib", "libglfw.dylib", "glfw"};
#else
            constexpr const char* names[] = {
                "glfw.so.3.3", "libglfw.so.3.3",
                "glfw.so.3", "libglfw.so.3",
                "glfw.so", "libglfw.so", "glfw"};
#endif
            if (const auto directory = GlfwExecutableDirectory())
            {
                for (const char* name : names)
                {
                    try
                    {
                        const std::string local = (*directory / name).string();
                        if (void* handle = dlopen(
                            local.c_str(), RTLD_LAZY | RTLD_LOCAL))
                        {
                            return handle;
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }
            for (const char* name : names)
            {
                if (void* handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL))
                {
                    return handle;
                }
            }
            return nullptr;
        }();
        return module;
    }

    template <typename T>
    [[nodiscard]] T GlfwProc(const char* name) noexcept
    {
        void* module = GlfwModule();
        return module == nullptr
            ? nullptr
            : reinterpret_cast<T>(dlsym(module, name));
    }
#else
    template <typename T>
    [[nodiscard]] T GlfwProc(const char*) noexcept
    {
        return nullptr;
    }
#endif

    [[nodiscard]] UpdateMappings UpdateMappingsApi() noexcept
    {
        static const auto value
            = GlfwProc<UpdateMappings>("glfwUpdateGamepadMappings");
        return value;
    }

    [[nodiscard]] JoystickString GuidApi() noexcept
    {
        static const auto value
            = GlfwProc<JoystickString>("glfwGetJoystickGUID");
        return value;
    }

    [[nodiscard]] JoystickString NameApi() noexcept
    {
        static const auto value
            = GlfwProc<JoystickString>("glfwGetJoystickName");
        return value;
    }

    [[nodiscard]] std::optional<std::string> ReadJoystickString(
        JoystickString function, std::int32_t slot, const char* procedure)
    {
        if (function == nullptr)
        {
            throw GlfwBindingUnavailable(procedure);
        }
        const char* value = function(slot);
        return value == nullptr
            ? std::nullopt
            : std::optional<std::string>{DecodeUtf8(value)};
    }

    [[nodiscard]] std::int32_t AddUnchecked(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(left)
            + std::bit_cast<std::uint32_t>(right));
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
                files = AddUnchecked(files, 1);
                lines = AddUnchecked(lines, Count(*text));
                std::cout << "[input] gamepad mappings: "
                    << Count(*text) << " from " << path << '\n';
            }
        }

        const std::optional<std::string> config = EnvironmentVariable();
        if (config.has_value()
            && !TrimDotNetWhitespace(*config).empty()
            && Apply(*config))
        {
            files = AddUnchecked(files, 1);
            lines = AddUnchecked(lines, Count(*config));
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
        const std::string beside = Combine(BaseDirectory(), FileName);
        const std::string settings = Combine(
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
            return ReadAllText(path);
        }
        catch (const std::ios_base::failure&)
        {
            return std::nullopt;
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return std::nullopt;
        }
    }

    bool GamepadMappings::Apply(const std::string& text)
    {
        try
        {
            const UpdateMappings function = UpdateMappingsApi();
            if (function == nullptr)
            {
                throw GlfwBindingUnavailable("glfwUpdateGamepadMappings");
            }
            return function(text.c_str()) != 0;
        }
        catch (const GlfwBindingUnavailable&)
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
            const std::string_view trimmed = TrimDotNetWhitespace(line);
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
        std::string guid = ReadJoystickString(
            GuidApi(), slot, "glfwGetJoystickGUID").value_or(
                "00000000000000000000000000000000");
        std::string name = ReadJoystickString(
            NameApi(), slot, "glfwGetJoystickName").value_or("gamepad");
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
