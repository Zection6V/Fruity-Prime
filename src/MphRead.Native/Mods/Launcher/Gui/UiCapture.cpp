#include "UiCapture.hpp"

#include "../../LogShare.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace
{
    using MphRead::Mods::Launcher::Gui::UiCaptureAdapter;
    using MphRead::Mods::Launcher::Gui::UiCaptureControlHandle;
    using MphRead::Mods::Launcher::Gui::UiCaptureRoomMetadataEnumerator;
    using MphRead::Mods::Launcher::Gui::UiCaptureSize;

    constexpr UiCaptureSize WindowSize{940.0, 560.0};
    constexpr std::string_view Endpoint = "203.0.113.7:27888";

    struct SampleServer final
    {
        std::string_view Name;
        std::string_view Room;
        std::int32_t Mode;
        std::int32_t Players;
        std::int32_t Ping;
    };

    // Immutable values from the C# GameMode : byte declaration:
    // Battle = 3, Bounty = 8, PrimeHunter = 14.
    constexpr std::array<SampleServer, 3> SampleServers{{
        {"net.livetek.fr", "MP3 PROVING GROUND", 3, 3, 41},
        {"A very long server name indeed", "MP7 PROCESSOR CORE", 14, 8, 152},
        {"lan", "MP2 HARVESTER", 8, 1, 2}
    }};

    void DisposeEnumeratorLikeForeach(
        const std::shared_ptr<UiCaptureRoomMetadataEnumerator>& enumerator,
        std::exception_ptr& pending)
    {
        if (!enumerator)
        {
            return;
        }
        try
        {
            enumerator->Dispose();
        }
        catch (...)
        {
            // A C# foreach finally replaces an exception already in flight if
            // IDisposable.Dispose itself throws.
            pending = std::current_exception();
        }
    }

    class CaptureLogShare final : public MphRead::Mods::ILogShare
    {
    public:
        [[nodiscard]] std::u16string StagingPath(std::u16string_view fileName) override
        {
            const std::u16string temp
                = MphRead::Mods::Launcher::Gui::Detail::UiCapturePathGetTempPath();
            return MphRead::Mods::Launcher::Gui::Detail::UiCapturePathCombine(
                temp, fileName);
        }

        [[nodiscard]] bool Share(std::u16string_view path,
            std::u16string_view subject, std::u16string& error) override
        {
            (void)path;
            (void)subject;
            error = u"there is nothing to share to on this platform";
            return false;
        }
    };

    void CloseAfterCapture(UiCaptureAdapter& adapter,
        const MphRead::Mods::Launcher::Gui::UiCaptureWindowHandle& window,
        std::exception_ptr pending)
    {
        if (window)
        {
            try
            {
                adapter.CloseWindow(window);
            }
            catch (...)
            {
                // C# finally semantics: Close() replaces a pending body/catch
                // exception or a pending return value.
                throw;
            }
        }
        if (pending)
        {
            std::rethrow_exception(pending);
        }
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    std::int32_t UiCapture::Run(UiCaptureAdapter& adapter, std::nullptr_t)
    {
        if (!adapter.EnsureSetup())
        {
            adapter.ConsoleWriteLine(
                "[uishot] no Avalonia backend on this machine; nothing captured");
            return 1;
        }

        // Directory.CreateDirectory(directory) is the first use of the C#
        // parameter after setup and throws ArgumentNullException("path") for null.
        throw System::ArgumentNullException("path");
    }

    std::int32_t UiCapture::Run(UiCaptureAdapter& adapter, std::string directory)
    {
        if (!adapter.EnsureSetup())
        {
            adapter.ConsoleWriteLine(
                "[uishot] no Avalonia backend on this machine; nothing captured");
            return 1;
        }

        adapter.CreateDirectory(directory);
        if (!Mods::LogShare::Current())
        {
            Mods::LogShare::Current(std::make_shared<CaptureLogShare>());
        }

        std::int32_t written = 0;
        adapter.InvokeUiThread([&]
        {
            const UiCaptureMenuSettingsHandle settings = adapter.ConstructMenuSettings();
            const UiCaptureRoomListRef rooms = RoomList(adapter);

            UiCaptureControlHandle iteratorCurrent;
            const auto capture = [&](std::string_view name,
                UiCaptureControlHandle view, UiCaptureSize size)
            {
                // Screens() is a C# iterator. Its state-machine Current keeps
                // the previous yielded Control alive while the next Control is
                // constructed, then replaces it before the foreach body runs.
                iteratorCurrent = std::move(view);
                const std::string path = adapter.PathCombine(
                    directory, std::string(name) + ".png");
                if (Capture(adapter, iteratorCurrent, path, size))
                {
                    ++written;
                    adapter.ConsoleWriteLine("[uishot] " + path);
                }
            };

            capture("home", adapter.ConstructHomeView(settings, rooms), WindowSize);
            capture("settings", adapter.ConstructSettingsView(settings), WindowSize);

            UiCaptureControlHandle credits = adapter.ConstructSettingsView(settings);
            adapter.SettingsViewShowSection(credits, "Credits");
            capture("settings-credits", std::move(credits), WindowSize);

            if (!rooms->empty())
            {
                capture("mappicker",
                    adapter.ConstructMapPickerView(rooms, (*rooms)[0]), WindowSize);
            }

            // C# argument evaluation is left-to-right: create the sample array
            // before evaluating DemoLibrary.Directory for the first picker.
            std::shared_ptr<const std::vector<Network::DemoRecording>> demos = SampleDemos();
            std::string demoDirectory = Network::DemoLibrary::Directory();
            capture("demopicker",
                adapter.ConstructDemoPickerView(std::move(demos), std::move(demoDirectory)),
                WindowSize);

            // Array.Empty<T>() is a reusable empty array; keep the native empty
            // list singleton stable for the same reference behavior.
            static const std::shared_ptr<const std::vector<Network::DemoRecording>> EmptyDemos
                = std::make_shared<const std::vector<Network::DemoRecording>>();
            demoDirectory = Network::DemoLibrary::Directory();
            capture("demopicker-empty",
                adapter.ConstructDemoPickerView(EmptyDemos, std::move(demoDirectory)),
                WindowSize);

            capture("pausemenu", adapter.ConstructPauseMenuView(true), WindowSize);
            capture("pausemenu-small", adapter.ConstructPauseMenuView(true),
                UiCaptureSize{560.0, 320.0});
            capture("serverbrowser", ServerList(adapter), WindowSize);
        });

        adapter.ConsoleWriteLine("[uishot] " + adapter.FormatCurrentInt32(written)
            + " screen(s) written to " + directory);
        return written > 0 ? 0 : 1;
    }

    UiCaptureRoomListRef UiCapture::RoomList(UiCaptureAdapter& adapter)
    {
        auto rooms = std::make_shared<UiCaptureRoomList>();
        try
        {
            const std::shared_ptr<UiCaptureRoomMetadataEnumerator> enumerator
                = adapter.GetRoomMetadataValuesEnumerator();
            std::exception_ptr pending;
            try
            {
                if (!enumerator)
                {
                    throw std::runtime_error(
                        "Object reference not set to an instance of an object.");
                }
                while (enumerator->MoveNext())
                {
                    const UiCaptureRoomMetadataHandle meta = enumerator->Current();
                    if (adapter.RoomMetadataMultiplayer(meta))
                    {
                        rooms->push_back(adapter.RoomMetadataName(meta));
                    }
                }
            }
            catch (...)
            {
                pending = std::current_exception();
            }
            DisposeEnumeratorLikeForeach(enumerator, pending);
            if (pending)
            {
                std::rethrow_exception(pending);
            }
        }
        catch (...)
        {
            // C# catches Exception and keeps any rooms added before the failure.
        }

        adapter.SortOrdinalIgnoreCase(*rooms);
        return rooms;
    }

    std::shared_ptr<const std::vector<Network::DemoRecording>> UiCapture::SampleDemos()
    {
        constexpr std::int64_t NowTicks = 639241429270000000LL;
        constexpr std::int64_t TwoDaysTicks = 2LL * 24LL * 60LL * 60LL * 10'000'000LL;
        constexpr std::int64_t NineDaysTicks = 9LL * 24LL * 60LL * 60LL * 10'000'000LL;

        auto demos = std::make_shared<std::vector<Network::DemoRecording>>();
        demos->reserve(3);
        demos->emplace_back(
            "MP3 PROVING GROUND_2026-09-04_18-22-07.fpdemo",
            "MP3 PROVING GROUND", ::MphRead::NativeRuntime::ManagedDateTime{NowTicks, 2}, 1'512'320);
        demos->emplace_back(
            "COMBAT HALL_2026-09-02_21-04-55.fpdemo",
            "COMBAT HALL", ::MphRead::NativeRuntime::ManagedDateTime{NowTicks - TwoDaysTicks, 2}, 402'112);
        demos->emplace_back(
            "sent-to-me.fpdemo", "",
            ::MphRead::NativeRuntime::ManagedDateTime{NowTicks - NineDaysTicks, 2}, 88'400);
        return demos;
    }

    UiCaptureControlHandle UiCapture::ServerList(UiCaptureAdapter& adapter)
    {
        UiCaptureControlHandle stack = adapter.ConstructStackPanel();
        adapter.SetStackPanelSpacing(stack, 18.0);
        adapter.SetControlMargin(stack, UiCaptureThickness::Uniform(12.0));

        for (const double width : {600.0, 400.0})
        {
            UiCaptureControlHandle list = adapter.ConstructStackPanel();
            adapter.SetStackPanelSpacing(list, 2.0);
            adapter.SetControlWidth(list, width);
            adapter.AddPanelChild(list, adapter.ConstructServerHeader());

            for (const SampleServer& sample : SampleServers)
            {
                UiCaptureControlHandle row = adapter.ConstructServerRow(sample.Name, Endpoint);
                Network::ServerStatus status;
                status.Online = true;
                status.RoomKey = sample.Room;
                status.Mode = static_cast<GameMode>(sample.Mode);
                status.Players = sample.Players;
                status.MaxPlayers = 8;
                status.Latency = sample.Ping;
                adapter.SetServerRowStatus(row, std::move(status));
                adapter.AddPanelChild(list, row);
            }
            adapter.AddPanelChild(stack, list);
        }
        return stack;
    }

    bool UiCapture::Capture(UiCaptureAdapter& adapter,
        const UiCaptureControlHandle& view, std::string_view path, UiCaptureSize size)
    {
        UiCaptureWindowHandle window;
        bool result = false;
        std::exception_ptr pending;

        try
        {
            // C# assigns the local only after the object initializer completes.
            // Keep a temporary handle so a setter failure leaves `window` null
            // and therefore does not call Close() in finally.
            UiCaptureWindowHandle created = adapter.ConstructWindow();
            adapter.SetWindowWidth(created, size.Width);
            adapter.SetWindowHeight(created, size.Height);
            adapter.SetWindowBackground(created, UiCaptureBrush::GuiThemePanelBrush);
            adapter.SetWindowRequestedThemeVariant(created, UiCaptureThemeVariant::Dark);
            adapter.SetWindowSystemDecorations(created, UiCaptureSystemDecorations::None);
            adapter.SetWindowShowInTaskbar(created, false);
            adapter.SetWindowShowActivated(created, false);
            adapter.SetWindowStartupLocation(created, UiCaptureWindowStartupLocation::Manual);
            adapter.SetWindowPosition(created, UiCapturePixelPoint{-4000, -4000});
            adapter.SetWindowContent(created, view);
            window = std::move(created);

            adapter.ShowWindow(window);
            for (std::int32_t i = 0; i < 8; ++i)
            {
                adapter.RunUiThreadJobs();
            }
            adapter.MeasureWindow(window, size);
            adapter.ArrangeWindow(window, UiCaptureRect{0.0, 0.0, size.Width, size.Height});
            adapter.RunUiThreadJobs();

            const UiCaptureBitmapHandle bitmap = adapter.ConstructRenderTargetBitmap(
                UiCapturePixelSize{
                    static_cast<std::int32_t>(size.Width),
                    static_cast<std::int32_t>(size.Height)},
                UiCaptureVector{96.0, 96.0});
            adapter.RenderBitmap(bitmap, window);
            adapter.SaveBitmap(bitmap, path);
            result = true;
        }
        catch (...)
        {
            const std::exception_ptr bodyException = std::current_exception();
            try
            {
                const std::string fileName = adapter.PathGetFileName(path);
                const std::string message = adapter.ExceptionMessage(bodyException);
                adapter.ConsoleWriteLine(
                    "[uishot] " + fileName + " could not be rendered: " + message);
                result = false;
            }
            catch (...)
            {
                pending = std::current_exception();
            }
        }

        CloseAfterCapture(adapter, window, pending);
        return result;
    }
}
