#pragma once

#include "../../../Formats/Types.hpp"
#include "../../Network/DemoLibrary.hpp"
#include "../../Network/NetStatus.hpp"

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Detail
    {
        // Platform boundary for the exact .NET 9 System.IO.Path operations used
        // by CaptureLogShare. Implementations must preserve Path.GetTempPath()
        // and Path.Combine(string,string), including platform rooting,
        // normalization, encoding, exceptions and messages.
        [[nodiscard]] std::u16string UiCapturePathGetTempPath();
        [[nodiscard]] std::u16string UiCapturePathCombine(
            std::u16string_view left, std::u16string_view right);
    }

    struct UiCaptureSize final
    {
        double Width;
        double Height;

        friend constexpr bool operator==(
            const UiCaptureSize&, const UiCaptureSize&) noexcept = default;
    };

    struct UiCaptureRect final
    {
        double X;
        double Y;
        double Width;
        double Height;

        friend constexpr bool operator==(
            const UiCaptureRect&, const UiCaptureRect&) noexcept = default;
    };

    struct UiCapturePixelSize final
    {
        std::int32_t Width;
        std::int32_t Height;

        friend constexpr bool operator==(
            const UiCapturePixelSize&, const UiCapturePixelSize&) noexcept = default;
    };

    struct UiCaptureVector final
    {
        double X;
        double Y;

        friend constexpr bool operator==(
            const UiCaptureVector&, const UiCaptureVector&) noexcept = default;
    };

    struct UiCapturePixelPoint final
    {
        std::int32_t X;
        std::int32_t Y;

        friend constexpr bool operator==(
            const UiCapturePixelPoint&, const UiCapturePixelPoint&) noexcept = default;
    };

    struct UiCaptureThickness final
    {
        double Left;
        double Top;
        double Right;
        double Bottom;

        [[nodiscard]] static constexpr UiCaptureThickness Uniform(double value) noexcept
        {
            return UiCaptureThickness{value, value, value, value};
        }

        friend constexpr bool operator==(
            const UiCaptureThickness&, const UiCaptureThickness&) noexcept = default;
    };

    enum class UiCaptureThemeVariant : std::uint8_t
    {
        Dark
    };

    enum class UiCaptureSystemDecorations : std::uint8_t
    {
        None
    };

    enum class UiCaptureWindowStartupLocation : std::uint8_t
    {
        Manual
    };

    enum class UiCaptureBrush : std::uint8_t
    {
        GuiThemePanelBrush
    };

    using UiCaptureControlHandle = std::shared_ptr<void>;
    using UiCaptureWindowHandle = std::shared_ptr<void>;
    using UiCaptureBitmapHandle = std::shared_ptr<void>;
    using UiCaptureMenuSettingsHandle = std::shared_ptr<void>;
    using UiCaptureRoomMetadataHandle = std::shared_ptr<void>;
    using UiCaptureRoomList = std::vector<std::string>;
    using UiCaptureRoomListRef = std::shared_ptr<UiCaptureRoomList>;

    class UiCaptureRoomMetadataEnumerator
    {
    public:
        virtual ~UiCaptureRoomMetadataEnumerator() = default;

        [[nodiscard]] virtual bool MoveNext() = 0;
        [[nodiscard]] virtual UiCaptureRoomMetadataHandle Current() const = 0;
        virtual void Dispose() = 0;
    };

    class UiCaptureAdapter
    {
    public:
        virtual ~UiCaptureAdapter() = default;

        [[nodiscard]] virtual bool EnsureSetup() = 0;
        virtual void CreateDirectory(std::string_view directory) = 0;
        [[nodiscard]] virtual std::string PathCombine(
            std::string_view left, std::string_view right) const = 0;
        [[nodiscard]] virtual std::string PathGetFileName(std::string_view path) const = 0;
        [[nodiscard]] virtual std::string FormatCurrentInt32(std::int32_t value) const = 0;
        [[nodiscard]] virtual std::string ExceptionMessage(
            std::exception_ptr exception) const = 0;
        virtual void ConsoleWriteLine(std::string_view text) = 0;

        virtual void InvokeUiThread(const std::function<void()>& action) = 0;
        virtual void RunUiThreadJobs() = 0;

        [[nodiscard]] virtual UiCaptureMenuSettingsHandle ConstructMenuSettings() = 0;
        [[nodiscard]] virtual std::shared_ptr<UiCaptureRoomMetadataEnumerator>
            GetRoomMetadataValuesEnumerator() = 0;
        [[nodiscard]] virtual bool RoomMetadataMultiplayer(
            const UiCaptureRoomMetadataHandle& room) const = 0;
        [[nodiscard]] virtual std::string RoomMetadataName(
            const UiCaptureRoomMetadataHandle& room) const = 0;
        virtual void SortOrdinalIgnoreCase(UiCaptureRoomList& rooms) const = 0;

        [[nodiscard]] virtual UiCaptureControlHandle ConstructHomeView(
            const UiCaptureMenuSettingsHandle& settings,
            const UiCaptureRoomListRef& rooms) = 0;
        [[nodiscard]] virtual UiCaptureControlHandle ConstructSettingsView(
            const UiCaptureMenuSettingsHandle& settings) = 0;
        virtual void SettingsViewShowSection(
            const UiCaptureControlHandle& view, std::string_view section) = 0;
        [[nodiscard]] virtual UiCaptureControlHandle ConstructMapPickerView(
            const UiCaptureRoomListRef& rooms, std::string_view current) = 0;
        [[nodiscard]] virtual UiCaptureControlHandle ConstructDemoPickerView(
            std::shared_ptr<const std::vector<Network::DemoRecording>> demos,
            std::string directory) = 0;
        [[nodiscard]] virtual UiCaptureControlHandle ConstructPauseMenuView(
            bool offerWindowMode) = 0;

        [[nodiscard]] virtual UiCaptureControlHandle ConstructStackPanel() = 0;
        virtual void SetStackPanelSpacing(
            const UiCaptureControlHandle& panel, double spacing) = 0;
        virtual void SetControlMargin(
            const UiCaptureControlHandle& control, UiCaptureThickness margin) = 0;
        virtual void SetControlWidth(
            const UiCaptureControlHandle& control, double width) = 0;
        virtual void AddPanelChild(const UiCaptureControlHandle& panel,
            const UiCaptureControlHandle& child) = 0;

        [[nodiscard]] virtual UiCaptureControlHandle ConstructServerHeader() = 0;
        [[nodiscard]] virtual UiCaptureControlHandle ConstructServerRow(
            std::string_view name, std::string_view endpoint) = 0;
        virtual void SetServerRowStatus(const UiCaptureControlHandle& row,
            Network::ServerStatus status) = 0;
        [[nodiscard]] virtual UiCaptureWindowHandle ConstructWindow() = 0;
        virtual void SetWindowWidth(const UiCaptureWindowHandle& window, double width) = 0;
        virtual void SetWindowHeight(const UiCaptureWindowHandle& window, double height) = 0;
        virtual void SetWindowBackground(
            const UiCaptureWindowHandle& window, UiCaptureBrush brush) = 0;
        virtual void SetWindowRequestedThemeVariant(
            const UiCaptureWindowHandle& window, UiCaptureThemeVariant variant) = 0;
        virtual void SetWindowSystemDecorations(
            const UiCaptureWindowHandle& window, UiCaptureSystemDecorations decorations) = 0;
        virtual void SetWindowShowInTaskbar(
            const UiCaptureWindowHandle& window, bool showInTaskbar) = 0;
        virtual void SetWindowShowActivated(
            const UiCaptureWindowHandle& window, bool showActivated) = 0;
        virtual void SetWindowStartupLocation(const UiCaptureWindowHandle& window,
            UiCaptureWindowStartupLocation location) = 0;
        virtual void SetWindowPosition(
            const UiCaptureWindowHandle& window, UiCapturePixelPoint position) = 0;
        virtual void SetWindowContent(const UiCaptureWindowHandle& window,
            const UiCaptureControlHandle& content) = 0;
        virtual void ShowWindow(const UiCaptureWindowHandle& window) = 0;
        virtual void MeasureWindow(
            const UiCaptureWindowHandle& window, UiCaptureSize size) = 0;
        virtual void ArrangeWindow(
            const UiCaptureWindowHandle& window, UiCaptureRect rect) = 0;
        virtual void CloseWindow(const UiCaptureWindowHandle& window) = 0;

        [[nodiscard]] virtual UiCaptureBitmapHandle ConstructRenderTargetBitmap(
            UiCapturePixelSize size, UiCaptureVector dpi) = 0;
        virtual void RenderBitmap(const UiCaptureBitmapHandle& bitmap,
            const UiCaptureWindowHandle& window) = 0;
        virtual void SaveBitmap(
            const UiCaptureBitmapHandle& bitmap, std::string_view path) = 0;
    };

    // Native equivalent of C# internal static class UiCapture. The adapter is
    // the unavoidable platform boundary for Avalonia/System.IO operations;
    // sequencing and state remain pair-local here.
    namespace Detail
    {
        // The platform's own capture adapter, supplied by the GUI host.
        [[nodiscard]] UiCaptureAdapter& UiCaptureAdapterInstance();
    }

    class UiCapture final
    {
    public:
        UiCapture() = delete;
        UiCapture(const UiCapture&) = delete;
        UiCapture& operator=(const UiCapture&) = delete;

        [[nodiscard]] static std::int32_t Run(
            UiCaptureAdapter& adapter, std::string directory);
        [[nodiscard]] static std::int32_t Run(
            UiCaptureAdapter& adapter, std::nullptr_t directory);

    private:
        [[nodiscard]] static UiCaptureRoomListRef RoomList(UiCaptureAdapter& adapter);
        [[nodiscard]] static std::shared_ptr<const std::vector<Network::DemoRecording>>
            SampleDemos();
        [[nodiscard]] static UiCaptureControlHandle ServerList(UiCaptureAdapter& adapter);
        [[nodiscard]] static bool Capture(UiCaptureAdapter& adapter,
            const UiCaptureControlHandle& view, std::string_view path, UiCaptureSize size);
    };
}
