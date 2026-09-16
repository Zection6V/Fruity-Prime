#include "ScreenCapture.hpp"

#include "ThumbnailLog.hpp"
#include "../Formats/Types.hpp"
#include "../Scene.hpp"

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#if defined(__GNUG__)
#include <cxxabi.h>
#include <cstdlib>
#include <memory>
#endif

namespace MphRead::Export::ImagesInterop
{
    void SetFlipVerticallyOnSave(bool value);
    void WritePngRgb(
        std::span<const std::uint8_t> buffer,
        std::int32_t width,
        std::int32_t height,
        std::ostream& stream);
}

namespace MphRead::Mods::ScreenCaptureInterop
{
    using DebugCallback = void(*)(
        std::uint32_t source,
        std::uint32_t type,
        std::uint32_t id,
        std::uint32_t severity,
        std::int32_t length,
        const char* message,
        const void* userParam);

    void Enable(std::uint32_t capability);
    void DebugMessageCallback(DebugCallback callback, const void* userParam);
    [[nodiscard]] std::optional<std::string> GetString(std::uint32_t name);
    [[nodiscard]] std::int32_t GetInteger(std::uint32_t pname);
    [[nodiscard]] std::string PtrToStringAnsi(const char* message, std::int32_t length);
}

namespace
{
    constexpr std::uint32_t GlDebugOutput = 0x92E0;
    constexpr std::uint32_t GlDebugOutputSynchronous = 0x8242;
    constexpr std::uint32_t GlDebugSeverityNotification = 0x826B;
    constexpr std::uint32_t GlVendor = 0x1F00;
    constexpr std::uint32_t GlRenderer = 0x1F01;
    constexpr std::uint32_t GlVersion = 0x1F02;
    constexpr std::uint32_t GlContextFlags = 0x821E;
    constexpr std::uint32_t GlContextProfileMask = 0x9126;
    constexpr std::int32_t GlContextFlagForwardCompatibleBit = 0x00000001;
    constexpr std::int32_t GlContextCoreProfileBit = 0x00000001;
    constexpr std::int32_t GlContextCompatibilityProfileBit = 0x00000002;

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
    {
#if defined(__cpp_char8_t)
        std::u8string converted;
        converted.reserve(value.size());
        for (unsigned char ch : value)
        {
            converted.push_back(static_cast<char8_t>(ch));
        }
        return std::filesystem::path(converted);
#else
        return std::filesystem::u8path(value.begin(), value.end());
#endif
    }

    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& value)
    {
#if defined(__cpp_char8_t)
        const std::u8string converted = value.u8string();
        return std::string(
            reinterpret_cast<const char*>(converted.data()), converted.size());
#else
        return value.u8string();
#endif
    }

    [[nodiscard]] std::ofstream CreateFile(const std::string& path)
    {
        std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        return stream;
    }

    [[nodiscard]] std::int32_t WrappedAdd(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t WrappedMultiply(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::string FixedTwo(double value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << value;
        return stream.str();
    }

    [[nodiscard]] std::string DebugSourceName(std::uint32_t value)
    {
        switch (value)
        {
        case 0x8246: return "DebugSourceApi";
        case 0x8247: return "DebugSourceWindowSystem";
        case 0x8248: return "DebugSourceShaderCompiler";
        case 0x8249: return "DebugSourceThirdParty";
        case 0x824A: return "DebugSourceApplication";
        case 0x824B: return "DebugSourceOther";
        default: return std::to_string(std::bit_cast<std::int32_t>(value));
        }
    }

    [[nodiscard]] std::string DebugTypeName(std::uint32_t value)
    {
        switch (value)
        {
        case 0x824C: return "DebugTypeError";
        case 0x824D: return "DebugTypeDeprecatedBehavior";
        case 0x824E: return "DebugTypeUndefinedBehavior";
        case 0x824F: return "DebugTypePortability";
        case 0x8250: return "DebugTypePerformance";
        case 0x8251: return "DebugTypeOther";
        case 0x8268: return "DebugTypeMarker";
        case 0x8269: return "DebugTypePushGroup";
        case 0x826A: return "DebugTypePopGroup";
        default: return std::to_string(std::bit_cast<std::int32_t>(value));
        }
    }

    [[nodiscard]] std::string DebugSeverityName(std::uint32_t value)
    {
        switch (value)
        {
        case 0x9146: return "DebugSeverityHigh";
        case 0x9147: return "DebugSeverityMedium";
        case 0x9148: return "DebugSeverityLow";
        case 0x826B: return "DebugSeverityNotification";
        default: return std::to_string(std::bit_cast<std::int32_t>(value));
        }
    }

    [[nodiscard]] std::string ExceptionTypeName(const std::exception& exception)
    {
        std::string name;
#if defined(__GNUG__)
        int status = 0;
        std::unique_ptr<char, void(*)(void*)> demangled(
            abi::__cxa_demangle(typeid(exception).name(), nullptr, nullptr, &status),
            std::free);
        name = status == 0 && demangled ? demangled.get() : typeid(exception).name();
#else
        name = typeid(exception).name();
#endif
        constexpr std::string_view classPrefix = "class ";
        constexpr std::string_view structPrefix = "struct ";
        if (name.starts_with(classPrefix))
        {
            name.erase(0, classPrefix.size());
        }
        else if (name.starts_with(structPrefix))
        {
            name.erase(0, structPrefix.size());
        }
        const std::size_t namespacePos = name.rfind("::");
        if (namespacePos != std::string::npos)
        {
            name.erase(0, namespacePos + 2);
        }
        const std::size_t templatePos = name.find('<');
        if (templatePos != std::string::npos)
        {
            name.erase(templatePos);
        }
        return name;
    }

    void InvokeReport(
        const std::function<void(const std::string&)>& report,
        const std::string& line)
    {
        if (!report)
        {
            throw System::NullReferenceException();
        }
        report(line);
    }
}

namespace MphRead::Mods
{
    ScreenCapture::PngWriterAction ScreenCapture::_pngWriter{};
    ScreenCapture::DebugProc ScreenCapture::_debugCallback{};
    std::int32_t ScreenCapture::_messagesLogged = 0;

    ScreenCapture::PngWriterAction ScreenCapture::PngWriter()
    {
        return _pngWriter;
    }

    void ScreenCapture::PngWriter(PngWriterAction value)
    {
        _pngWriter = std::move(value);
    }

    bool ScreenCapture::SaveWindow(Scene* scene, const std::string& path)
    {
        if (scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        ReadPixels read = [scene](std::int32_t& width, std::int32_t& height)
        {
            return scene->ReadWindowBuffer(width, height);
        };
        return Save(scene, path, read);
    }

    bool ScreenCapture::Save(Scene* scene, const std::string& path)
    {
        if (scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        ReadPixels read = [scene](std::int32_t& width, std::int32_t& height)
        {
            return scene->ReadSceneTarget(width, height);
        };
        return Save(scene, path, read);
    }

    bool ScreenCapture::Save(
        Scene* scene,
        const std::string& path,
        const ReadPixels& read)
    {
        (void)scene;
        try
        {
            std::int32_t width = 0;
            std::int32_t height = 0;
            PixelBuffer pixels = read(width, height);
            if (!pixels || width <= 0 || height <= 0)
            {
                return false;
            }

            if (LitFraction(*pixels) < MinLitFraction)
            {
                const std::string fileName = PathToUtf8(PathFromUtf8(path).filename());
                const std::string litPercent = FixedTwo(LitFraction(*pixels) * 100.0);
                const std::string context = DescribeContext();
                const std::string why = fileName + " came out black (" + litPercent
                    + "% lit, " + std::to_string(width) + "x" + std::to_string(height)
                    + "); not saving it. The scene rendered nothing -- " + context;
                std::cout << "[capture] " << why << std::endl;
                ThumbnailLog::Write(why);
                return false;
            }

            const std::filesystem::path directory = PathFromUtf8(path).parent_path();
            if (!directory.empty())
            {
                std::filesystem::create_directories(directory);
            }

            PngWriterAction writer = PngWriter();
            if (writer)
            {
                writer(*pixels, width, height, path);
                return true;
            }

            std::ofstream stream = CreateFile(path);
            Export::ImagesInterop::SetFlipVerticallyOnSave(true);
            Export::ImagesInterop::WritePngRgb(
                std::span<const std::uint8_t>(*pixels), width, height, stream);
            stream.close();
            return true;
        }
        catch (const std::exception& exception)
        {
            std::cout << "[capture] could not save " << path
                      << ": " << exception.what() << std::endl;
            return false;
        }
    }

    double ScreenCapture::LitFraction(const std::vector<std::uint8_t>& pixels)
    {
        if (pixels.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::overflow_error("Array dimensions exceeded supported range.");
        }

        const std::int32_t length = static_cast<std::int32_t>(pixels.size());
        std::int32_t lit = 0;
        std::int32_t total = 0;
        for (std::int32_t i = 0; WrappedAdd(i, 2) < length; i = WrappedAdd(i, 3))
        {
            const std::int32_t i1 = WrappedAdd(i, 1);
            const std::int32_t i2 = WrappedAdd(i, 2);
            if (i < 0 || i1 < 0 || i2 < 0
                || i >= length || i1 >= length || i2 >= length)
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            total = WrappedAdd(total, 1);
            if (pixels[static_cast<std::size_t>(i)] > 8
                || pixels[static_cast<std::size_t>(i1)] > 8
                || pixels[static_cast<std::size_t>(i2)] > 8)
            {
                lit = WrappedAdd(lit, 1);
            }
        }
        return total == 0 ? 0.0 : static_cast<double>(lit) / static_cast<double>(total);
    }

    void ScreenCapture::DebugThunk(
        std::uint32_t source,
        std::uint32_t type,
        std::uint32_t id,
        std::uint32_t severity,
        std::int32_t length,
        const char* message,
        const void* param)
    {
        if (_debugCallback)
        {
            _debugCallback(source, type, id, severity, length, message, param);
        }
    }

    void ScreenCapture::EnableDebugOutput(ReportAction report)
    {
        try
        {
            _debugCallback = [report](
                std::uint32_t source,
                std::uint32_t type,
                std::uint32_t id,
                std::uint32_t severity,
                std::int32_t length,
                const char* message,
                const void* param)
            {
                (void)id;
                (void)param;
                if (severity == GlDebugSeverityNotification || _messagesLogged >= 12)
                {
                    return;
                }
                _messagesLogged = WrappedAdd(_messagesLogged, 1);
                const std::string text
                    = ScreenCaptureInterop::PtrToStringAnsi(message, length);
                const std::string severityText = DebugSeverityName(severity);
                const std::string typeText = DebugTypeName(type);
                const std::string sourceText = DebugSourceName(source);
                InvokeReport(
                    report,
                    "GL says: [" + severityText + "] " + typeText
                        + " from " + sourceText + ": " + text);
            };
            ScreenCaptureInterop::Enable(GlDebugOutput);
            ScreenCaptureInterop::Enable(GlDebugOutputSynchronous);
            ScreenCaptureInterop::DebugMessageCallback(&ScreenCapture::DebugThunk, nullptr);
        }
        catch (const std::exception& exception)
        {
            InvokeReport(
                report,
                "could not turn on GL debug output (" + ExceptionTypeName(exception)
                    + "); this driver may not have KHR_debug");
        }
    }

    std::string ScreenCapture::DescribeContext()
    {
        try
        {
            const std::string vendor = ScreenCaptureInterop::GetString(GlVendor).value_or("?");
            const std::string renderer = ScreenCaptureInterop::GetString(GlRenderer).value_or("?");
            const std::string version = ScreenCaptureInterop::GetString(GlVersion).value_or("?");
            const std::int32_t flags = ScreenCaptureInterop::GetInteger(GlContextFlags);
            const std::string forward = (flags & GlContextFlagForwardCompatibleBit) != 0
                ? ", FORWARD-COMPATIBLE (deprecated entry points removed, which is all of immediate mode)"
                : "";
            const std::int32_t mask = ScreenCaptureInterop::GetInteger(GlContextProfileMask);
            const std::string profile = (mask & GlContextCoreProfileBit) != 0
                ? "CORE (immediate mode is unavailable, which renders everything black)"
                : (mask & GlContextCompatibilityProfileBit) != 0
                    ? "compatibility"
                    : "unreported";
            return "GL " + version + ", profile " + profile + forward
                + ", " + vendor + " / " + renderer;
        }
        catch (const std::exception& exception)
        {
            return "could not query the GL context: " + std::string(exception.what());
        }
    }

    double ScreenCapture::NonBlackFraction(Scene* scene)
    {
        if (scene == nullptr)
        {
            throw System::NullReferenceException();
        }

        std::int32_t width = 0;
        std::int32_t height = 0;
        PixelBuffer pixels = scene->ReadSceneTarget(width, height);
        if (!pixels || width <= 0)
        {
            return 0.0;
        }
        if (pixels->size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::overflow_error("Array dimensions exceeded supported range.");
        }

        const std::int32_t length = static_cast<std::int32_t>(pixels->size());
        const auto channel = [&](std::int32_t index) -> std::uint8_t
        {
            if (index < 0 || index >= length)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return (*pixels)[static_cast<std::size_t>(index)];
        };

        std::int32_t lit = 0;
        for (std::int32_t i = 0; i < length; i = WrappedAdd(i, 3))
        {
            if (channel(i) > 8
                || channel(WrappedAdd(i, 1)) > 8
                || channel(WrappedAdd(i, 2)) > 8)
            {
                lit = WrappedAdd(lit, 1);
            }
        }
        const std::int32_t area = WrappedMultiply(width, height);
        return static_cast<double>(lit) / static_cast<double>(area);
    }
}
