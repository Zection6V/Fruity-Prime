#include "ScreenCapture.hpp"

#include "../NativeRuntime/OpenTK/GL.hpp"
#include "../NativeRuntime/System/Console.hpp"
#include "../NativeRuntime/System/Globalization.hpp"

#include <typeinfo>

#include "ThumbnailLog.hpp"
#include "../Formats/Types.hpp"
#include "../Scene.hpp"
#include "../NativeRuntime/System/ExceptionText.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Managed.hpp"

#include <bit>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::ExceptionTypeName;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;

namespace MphRead::Export::ImagesInterop
{
    void SetFlipVerticallyOnSave(bool value);
    void WritePngRgb(
        std::span<const std::uint8_t> buffer,
        std::int32_t width,
        std::int32_t height,
        std::ostream& stream);
}

namespace
{
    constexpr std::int32_t GlDebugOutput = 0x92E0;
    constexpr std::int32_t GlDebugOutputSynchronous = 0x8242;
    constexpr std::int32_t GlDebugSeverityNotification = 0x826B;
    constexpr std::int32_t GlVendor = 0x1F00;
    constexpr std::int32_t GlRenderer = 0x1F01;
    constexpr std::int32_t GlVersion = 0x1F02;
    constexpr std::int32_t GlContextFlags = 0x821E;
    constexpr std::int32_t GlContextProfileMask = 0x9126;
    constexpr std::int32_t GlContextFlagForwardCompatibleBit = 0x00000001;
    constexpr std::int32_t GlContextCoreProfileBit = 0x00000001;
    constexpr std::int32_t GlContextCompatibilityProfileBit = 0x00000002;

    [[nodiscard]] std::ofstream CreateFile(const std::string& path)
    {
        std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        return stream;
    }

    [[nodiscard]] std::string DebugSourceName(std::int32_t value)
    {
        switch (value)
        {
        case 0x8246: return "DebugSourceApi";
        case 0x8247: return "DebugSourceWindowSystem";
        case 0x8248: return "DebugSourceShaderCompiler";
        case 0x8249: return "DebugSourceThirdParty";
        case 0x824A: return "DebugSourceApplication";
        case 0x824B: return "DebugSourceOther";
        default: return std::to_string(value);
        }
    }

    [[nodiscard]] std::string DebugTypeName(std::int32_t value)
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
        default: return std::to_string(value);
        }
    }

    [[nodiscard]] std::string DebugSeverityName(std::int32_t value)
    {
        switch (value)
        {
        case 0x9146: return "DebugSeverityHigh";
        case 0x9147: return "DebugSeverityMedium";
        case 0x9148: return "DebugSeverityLow";
        case 0x826B: return "DebugSeverityNotification";
        default: return std::to_string(value);
        }
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

namespace
{
    // GL_KHR_debug and the two glGet forms this file reads, which the managed
    // build reaches through OpenTK's own GL class.
    using GlDebugProc = void (*)(
        std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
        const char*, const void*);

    void GlEnable(std::int32_t capability)
    {
        ::OpenTK::Graphics::OpenGL::GL::Enable(
            static_cast<::OpenTK::Graphics::OpenGL::GL::EnableCap>(capability));
    }

    void GlDebugMessageCallback(GlDebugProc callback, const void* userParam)
    {
        ::OpenTK::Graphics::OpenGL::GL::DebugMessageCallback(
            reinterpret_cast<void*>(callback), userParam);
    }

    [[nodiscard]] std::optional<std::string> GlGetString(std::int32_t name)
    {
        std::string value = ::OpenTK::Graphics::OpenGL::GL::GetString(
            static_cast<::OpenTK::Graphics::OpenGL::GL::StringName>(name));
        if (value.empty())
        {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] std::int32_t GlGetInteger(std::int32_t pname)
    {
        return ::OpenTK::Graphics::OpenGL::GL::GetInteger(pname);
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
                std::string first = PathToUtf8(PathFromUtf8(path).filename());
                first += " came out black ";

                std::string second = "(";
                second += NativeRuntime::DoubleToStringFixed2(
                    LitFraction(*pixels) * 100.0);
                second += "% lit, ";
                second += std::to_string(width);
                second += "x";
                second += std::to_string(height);
                second += "); not saving it. ";

                std::string third = "The scene rendered nothing -- ";
                third += DescribeContext();

                const std::string why = first + second + third;
                NativeRuntime::ConsoleWriteLine("[capture] " + why);
                ThumbnailLog::Write(why);
                return false;
            }

            const std::filesystem::path pathValue = PathFromUtf8(path);
            const std::filesystem::path directory
                = pathValue == pathValue.root_path()
                    ? std::filesystem::path{}
                    : pathValue.parent_path();
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
            try
            {
                Export::ImagesInterop::SetFlipVerticallyOnSave(true);
                Export::ImagesInterop::WritePngRgb(
                    std::span<const std::uint8_t>(*pixels), width, height, stream);
            }
            catch (...)
            {
                try
                {
                    stream.close();
                }
                catch (...)
                {
                    throw;
                }
                throw;
            }
            stream.close();
            return true;
        }
        catch (const std::exception& exception)
        {
            NativeRuntime::ConsoleWriteLine(
                "[capture] could not save " + path + ": "
                    + std::string(exception.what()));
            return false;
        }
    }

    double ScreenCapture::LitFraction(const std::vector<std::uint8_t>& pixels)
    {
        const std::int32_t length = static_cast<std::int32_t>(pixels.size());
        const auto channel = [&](std::int32_t index) -> std::uint8_t
        {
            if (index < 0 || index >= length)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return pixels[static_cast<std::size_t>(index)];
        };

        std::int32_t lit = 0;
        std::int32_t total = 0;
        for (std::int32_t i = 0; UncheckedAdd(i, 2) < length; i = UncheckedAdd(i, 3))
        {
            total = UncheckedAdd(total, 1);
            if (channel(i) > 8
                || channel(UncheckedAdd(i, 1)) > 8
                || channel(UncheckedAdd(i, 2)) > 8)
            {
                lit = UncheckedAdd(lit, 1);
            }
        }
        return total == 0 ? 0.0 : static_cast<double>(lit) / static_cast<double>(total);
    }

    void ScreenCapture::DebugThunk(
        std::int32_t source,
        std::int32_t type,
        std::int32_t id,
        std::int32_t severity,
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
                std::int32_t source,
                std::int32_t type,
                std::int32_t id,
                std::int32_t severity,
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
                _messagesLogged = UncheckedAdd(_messagesLogged, 1);
                const std::string text
                    = std::string(message, static_cast<std::size_t>(length));
                const std::string severityText = DebugSeverityName(severity);
                const std::string typeText = DebugTypeName(type);
                const std::string sourceText = DebugSourceName(source);
                InvokeReport(
                    report,
                    "GL says: [" + severityText + "] " + typeText
                        + " from " + sourceText + ": " + text);
            };
            GlEnable(GlDebugOutput);
            GlEnable(GlDebugOutputSynchronous);
            GlDebugMessageCallback(&ScreenCapture::DebugThunk, nullptr);
        }
        catch (const std::exception& exception)
        {
            InvokeReport(
                report,
                "could not turn on GL debug output ("
                    + ExceptionTypeName(exception)
                    + "); this driver may not have KHR_debug");
        }
    }

    std::string ScreenCapture::DescribeContext()
    {
        try
        {
            const std::string vendor = GlGetString(GlVendor).value_or("?");
            const std::string renderer = GlGetString(GlRenderer).value_or("?");
            const std::string version = GlGetString(GlVersion).value_or("?");
            const std::int32_t flags = GlGetInteger(GlContextFlags);
            const std::string forward = (flags & GlContextFlagForwardCompatibleBit) != 0
                ? ", FORWARD-COMPATIBLE (deprecated entry points removed, which is all of immediate mode)"
                : "";
            const std::int32_t mask = GlGetInteger(GlContextProfileMask);
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
            return "could not query the GL context: "
                + std::string(exception.what());
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
        for (std::int32_t i = 0; i < length; i = UncheckedAdd(i, 3))
        {
            if (channel(i) > 8
                || channel(UncheckedAdd(i, 1)) > 8
                || channel(UncheckedAdd(i, 2)) > 8)
            {
                lit = UncheckedAdd(lit, 1);
            }
        }
        const std::int32_t area = UncheckedMultiply(width, height);
        return static_cast<double>(lit) / static_cast<double>(area);
    }
}
