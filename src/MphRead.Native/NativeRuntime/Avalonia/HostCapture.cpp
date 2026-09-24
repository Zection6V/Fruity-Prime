// Pictures of the launcher's own screens, taken with nobody looking: what
// Avalonia's RenderTargetBitmap is on the managed side. The screens are the
// same adapters the launcher uses, drawn into a framebuffer of their own.

#include "HostScreens.hpp"
#include "HomeViewHost.hpp"

#include "../Gui/Host.hpp"
#include "../Stb/Image.hpp"

#include "../../GameState.hpp"
#include "../../Metadata/Rooms.hpp"
#include "../../Mods/Launcher/Gui/UiCapture.hpp"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        // One capture's window: the content, the size it is laid out at, and
        // the picture read back from it.
        struct CaptureWindow final
        {
            Toolkit::ElementPtr Content;
            double Width = 0.0;
            double Height = 0.0;
        };

        struct CaptureBitmap final
        {
            std::int32_t Width = 0;
            std::int32_t Height = 0;
            std::vector<std::uint8_t> Rgba;
        };

        // The hidden window that owns the GL context every capture draws in.
        [[nodiscard]] Toolkit::Window& Surface()
        {
            static Toolkit::Window window;
            return window;
        }

        void WritePng(void* context, void* data, int size)
        {
            auto* const file = static_cast<std::ofstream*>(context);
            file->write(static_cast<const char*>(data), size);
        }

        class CaptureHost final : public Launcher::UiCaptureAdapter
        {
        public:
            [[nodiscard]] bool EnsureSetup() override
            {
                return Toolkit::Window::Available();
            }

            void CreateDirectory(std::string_view directory) override
            {
                std::error_code error;
                std::filesystem::create_directories(
                    std::filesystem::path(directory), error);
            }

            [[nodiscard]] std::string PathCombine(
                std::string_view left, std::string_view right) const override
            {
                return (std::filesystem::path(left) / std::filesystem::path(right))
                    .string();
            }

            [[nodiscard]] std::string PathGetFileName(
                std::string_view path) const override
            {
                return std::filesystem::path(path).filename().string();
            }

            [[nodiscard]] std::string FormatCurrentInt32(
                std::int32_t value) const override
            {
                return std::to_string(value);
            }

            [[nodiscard]] std::string ExceptionMessage(
                std::exception_ptr exception) const override
            {
                if (exception == nullptr)
                {
                    return std::string();
                }
                try
                {
                    std::rethrow_exception(exception);
                }
                catch (const std::exception& failure)
                {
                    return failure.what();
                }
                catch (...)
                {
                    return "Exception of type 'System.Object' was thrown.";
                }
            }

            void ConsoleWriteLine(std::string_view text) override
            {
                std::cout << text << '\n';
            }

            void InvokeUiThread(const std::function<void()>& action) override
            {
                // There is one thread here, and it is this one.
                if (action)
                {
                    action();
                }
            }

            void RunUiThreadJobs() override
            {
                Toolkit::Dispatcher::Instance().PumpOnce();
            }

            [[nodiscard]] Launcher::UiCaptureMenuSettingsHandle
                ConstructMenuSettings() override
            {
                return MphRead::GameState::LoadSettings();
            }

            [[nodiscard]] std::shared_ptr<Launcher::UiCaptureRoomMetadataEnumerator>
                GetRoomMetadataValuesEnumerator() override
            {
                return std::make_shared<RoomEnumerator>();
            }

            [[nodiscard]] bool RoomMetadataMultiplayer(
                const Launcher::UiCaptureRoomMetadataHandle& room) const override
            {
                const auto* const meta
                    = static_cast<const MphRead::RoomMetadata*>(room.get());
                return meta != nullptr && meta->Multiplayer;
            }

            [[nodiscard]] std::string RoomMetadataName(
                const Launcher::UiCaptureRoomMetadataHandle& room) const override
            {
                const auto* const meta
                    = static_cast<const MphRead::RoomMetadata*>(room.get());
                return meta != nullptr ? meta->Name : std::string();
            }

            void SortOrdinalIgnoreCase(Launcher::UiCaptureRoomList& rooms) const override
            {
                std::sort(rooms.begin(), rooms.end(),
                    [](const std::string& left, const std::string& right)
                    {
                        const std::size_t count
                            = std::min(left.size(), right.size());
                        for (std::size_t i = 0; i < count; ++i)
                        {
                            const unsigned char a = static_cast<unsigned char>(
                                std::toupper(static_cast<unsigned char>(left[i])));
                            const unsigned char b = static_cast<unsigned char>(
                                std::toupper(static_cast<unsigned char>(right[i])));
                            if (a != b)
                            {
                                return a < b;
                            }
                        }
                        return left.size() < right.size();
                    });
            }

            [[nodiscard]] Launcher::UiCaptureControlHandle ConstructHomeView(
                const Launcher::UiCaptureMenuSettingsHandle& settings,
                const Launcher::UiCaptureRoomListRef& rooms) override
            {
                return HomeViewHost::Create(
                    std::static_pointer_cast<MphRead::MenuSettings>(settings),
                    rooms != nullptr ? *rooms : std::vector<std::string>{});
            }

            [[nodiscard]] Launcher::UiCaptureControlHandle ConstructSettingsView(
                const Launcher::UiCaptureMenuSettingsHandle& settings) override
            {
                return SettingsHost::Create(
                    std::static_pointer_cast<MphRead::MenuSettings>(settings), false);
            }

            void SettingsViewShowSection(const Launcher::UiCaptureControlHandle& view,
                std::string_view section) override
            {
                const std::u16string name = Utf8ToUtf16(section);
                static_cast<SettingsHost*>(Ptr(view)->Tag.get())
                    ->View()
                    .ShowSection(std::u16string_view(name));
            }

            [[nodiscard]] Launcher::UiCaptureControlHandle ConstructMapPickerView(
                const Launcher::UiCaptureRoomListRef& rooms,
                std::string_view current) override
            {
                return MapPickerHost::Create(
                    rooms != nullptr ? *rooms : std::vector<std::string>{},
                    std::string(current));
            }

            [[nodiscard]] Launcher::UiCaptureControlHandle ConstructDemoPickerView(
                std::shared_ptr<const std::vector<MphRead::Mods::Network::DemoRecording>>
                    demos,
                std::string directory) override
            {
                return DemoPickerHost::Create(std::move(demos), std::move(directory));
            }

            [[nodiscard]] Launcher::UiCaptureControlHandle ConstructPauseMenuView(
                bool offerWindowMode) override
            {
                return PauseMenuHost::Create(offerWindowMode);
            }

            [[nodiscard]] Launcher::UiCaptureControlHandle
                ConstructStackPanel() override
            {
                return Toolkit::Element::Create(Toolkit::ElementKind::StackPanel);
            }

            void SetStackPanelSpacing(const Launcher::UiCaptureControlHandle& panel,
                double spacing) override
            {
                El(panel)->Spacing = spacing;
            }

            void SetControlMargin(const Launcher::UiCaptureControlHandle& control,
                Launcher::UiCaptureThickness margin) override
            {
                El(control)->Margin = Toolkit::Thickness{
                    margin.Left, margin.Top, margin.Right, margin.Bottom};
            }

            void SetControlWidth(const Launcher::UiCaptureControlHandle& control,
                double width) override
            {
                El(control)->Width = width;
            }

            void AddPanelChild(const Launcher::UiCaptureControlHandle& panel,
                const Launcher::UiCaptureControlHandle& child) override
            {
                El(panel)->AddChild(Ptr(child));
            }

            [[nodiscard]] Launcher::UiCaptureControlHandle
                ConstructServerHeader() override
            {
                return ServerHeaderHost::Create();
            }

            [[nodiscard]] Launcher::UiCaptureControlHandle ConstructServerRow(
                std::string_view name, std::string_view endpoint) override
            {
                return ServerRowHost::Create(Utf8ToUtf16(name), Utf8ToUtf16(endpoint));
            }

            void SetServerRowStatus(const Launcher::UiCaptureControlHandle& row,
                MphRead::Mods::Network::ServerStatus status) override
            {
                HostOf<ServerRowHost>(Ptr(row))->Row().SetStatus(std::move(status));
            }

            [[nodiscard]] Launcher::UiCaptureWindowHandle ConstructWindow() override
            {
                return std::make_shared<CaptureWindow>();
            }

            void SetWindowWidth(
                const Launcher::UiCaptureWindowHandle& window, double width) override
            {
                static_cast<CaptureWindow*>(window.get())->Width = width;
            }

            void SetWindowHeight(
                const Launcher::UiCaptureWindowHandle& window, double height) override
            {
                static_cast<CaptureWindow*>(window.get())->Height = height;
            }

            void SetWindowBackground(const Launcher::UiCaptureWindowHandle& window,
                Launcher::UiCaptureBrush brush) override
            {
                (void)window;
                (void)brush;
            }

            void SetWindowRequestedThemeVariant(
                const Launcher::UiCaptureWindowHandle& window,
                Launcher::UiCaptureThemeVariant variant) override
            {
                (void)window;
                (void)variant;
            }

            void SetWindowSystemDecorations(
                const Launcher::UiCaptureWindowHandle& window,
                Launcher::UiCaptureSystemDecorations decorations) override
            {
                (void)window;
                (void)decorations;
            }

            void SetWindowShowInTaskbar(
                const Launcher::UiCaptureWindowHandle& window,
                bool showInTaskbar) override
            {
                (void)window;
                (void)showInTaskbar;
            }

            void SetWindowShowActivated(
                const Launcher::UiCaptureWindowHandle& window,
                bool showActivated) override
            {
                (void)window;
                (void)showActivated;
            }

            void SetWindowStartupLocation(
                const Launcher::UiCaptureWindowHandle& window,
                Launcher::UiCaptureWindowStartupLocation location) override
            {
                (void)window;
                (void)location;
            }

            void SetWindowPosition(const Launcher::UiCaptureWindowHandle& window,
                Launcher::UiCapturePixelPoint position) override
            {
                (void)window;
                (void)position;
            }

            void SetWindowContent(const Launcher::UiCaptureWindowHandle& window,
                const Launcher::UiCaptureControlHandle& content) override
            {
                static_cast<CaptureWindow*>(window.get())->Content = Ptr(content);
            }

            void ShowWindow(const Launcher::UiCaptureWindowHandle& window) override
            {
                // Nothing is shown: the picture is taken off a framebuffer.
                (void)window;
            }

            void MeasureWindow(const Launcher::UiCaptureWindowHandle& window,
                Launcher::UiCaptureSize size) override
            {
                auto* const value = static_cast<CaptureWindow*>(window.get());
                if (value->Content != nullptr)
                {
                    value->Content->Measure(Toolkit::Size{size.Width, size.Height});
                }
            }

            void ArrangeWindow(const Launcher::UiCaptureWindowHandle& window,
                Launcher::UiCaptureRect rect) override
            {
                auto* const value = static_cast<CaptureWindow*>(window.get());
                if (value->Content != nullptr)
                {
                    value->Content->Arrange(
                        Toolkit::Rect{rect.X, rect.Y, rect.Width, rect.Height});
                }
            }

            void CloseWindow(const Launcher::UiCaptureWindowHandle& window) override
            {
                static_cast<CaptureWindow*>(window.get())->Content = nullptr;
            }

            [[nodiscard]] Launcher::UiCaptureBitmapHandle ConstructRenderTargetBitmap(
                Launcher::UiCapturePixelSize size, Launcher::UiCaptureVector dpi) override
            {
                (void)dpi;
                auto bitmap = std::make_shared<CaptureBitmap>();
                bitmap->Width = size.Width;
                bitmap->Height = size.Height;
                return bitmap;
            }

            void RenderBitmap(const Launcher::UiCaptureBitmapHandle& bitmap,
                const Launcher::UiCaptureWindowHandle& window) override
            {
                auto* const target = static_cast<CaptureBitmap*>(bitmap.get());
                auto* const source = static_cast<CaptureWindow*>(window.get());
                if (source->Content == nullptr)
                {
                    return;
                }
                Surface().Content(source->Content);
                (void)Surface().Capture(
                    target->Width, target->Height, target->Rgba);
                Surface().Content(nullptr);
            }

            void SaveBitmap(const Launcher::UiCaptureBitmapHandle& bitmap,
                std::string_view path) override
            {
                const auto* const value
                    = static_cast<const CaptureBitmap*>(bitmap.get());
                if (value->Rgba.empty())
                {
                    return;
                }
                std::ofstream file(std::filesystem::path(path), std::ios::binary);
                if (!file)
                {
                    return;
                }
                // GL reads bottom row first; a PNG is written from the top.
                ::stbi_flip_vertically_on_write(1);
                (void)::stbi_write_png_to_func(&WritePng, &file, value->Width,
                    value->Height, 4, value->Rgba.data(), value->Width * 4);
                ::stbi_flip_vertically_on_write(0);
            }

        private:
            class RoomEnumerator final
                : public Launcher::UiCaptureRoomMetadataEnumerator
            {
            public:
                RoomEnumerator()
                    : _current(MphRead::Metadata::RoomMetadata.begin())
                {
                }

                [[nodiscard]] bool MoveNext() override
                {
                    if (_started)
                    {
                        ++_current;
                    }
                    _started = true;
                    return _current != MphRead::Metadata::RoomMetadata.end();
                }

                [[nodiscard]] Launcher::UiCaptureRoomMetadataHandle Current()
                    const override
                {
                    return _current->second;
                }

                void Dispose() override {}

            private:
                std::unordered_map<std::string,
                    std::shared_ptr<MphRead::RoomMetadata>>::const_iterator _current;
                bool _started = false;
            };
        };
    }
}

namespace MphRead::Mods::Launcher::Gui::Detail
{
    UiCaptureAdapter& UiCaptureAdapterInstance()
    {
        static ::MphRead::NativeRuntime::Avalonia::CaptureHost adapter;
        return adapter;
    }

    std::u16string UiCapturePathGetTempPath()
    {
        std::error_code error;
        const std::filesystem::path path = std::filesystem::temp_directory_path(error);
        return ::MphRead::NativeRuntime::Utf8ToUtf16(
            error ? std::string() : path.string());
    }

    std::u16string UiCapturePathCombine(
        std::u16string_view left, std::u16string_view right)
    {
        return ::MphRead::NativeRuntime::Utf8ToUtf16(
            (std::filesystem::path(::MphRead::NativeRuntime::Utf16ToUtf8(left))
                / std::filesystem::path(
                    ::MphRead::NativeRuntime::Utf16ToUtf8(right)))
                .string());
    }
}
