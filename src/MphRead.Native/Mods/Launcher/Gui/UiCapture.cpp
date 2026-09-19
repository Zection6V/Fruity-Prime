#include "UiCapture.hpp"

#include "../../LogShare.hpp"

#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
    using MphRead::Mods::Launcher::Gui::UiCaptureAdapter;
    using MphRead::Mods::Launcher::Gui::UiCaptureControlHandle;
    using MphRead::Mods::Launcher::Gui::UiCaptureRoomMetadataEnumerator;
    using MphRead::Mods::Launcher::Gui::UiCaptureSize;

    constexpr UiCaptureSize WindowSize{940.0, 560.0};
    constexpr std::string_view Endpoint = "203.0.113.7:27888";

    enum class SampleMode : std::uint8_t
    {
        Battle,
        PrimeHunter,
        Bounty
    };

    struct SampleServer final
    {
        std::string_view Name;
        std::string_view Room;
        SampleMode Mode;
        std::int32_t Players;
        std::int32_t Ping;
    };

    constexpr std::array<SampleServer, 3> SampleServers{{
        {"net.livetek.fr", "MP3 PROVING GROUND", SampleMode::Battle, 3, 41},
        {"A very long server name indeed", "MP7 PROCESSOR CORE",
            SampleMode::PrimeHunter, 8, 152},
        {"lan", "MP2 HARVESTER", SampleMode::Bounty, 1, 2}
    }};

    [[nodiscard]] MphRead::GameMode ResolveMode(
        UiCaptureAdapter& adapter, SampleMode mode)
    {
        switch (mode)
        {
        case SampleMode::Battle:
            return adapter.BattleGameMode();
        case SampleMode::PrimeHunter:
            return adapter.PrimeHunterGameMode();
        case SampleMode::Bounty:
            return adapter.BountyGameMode();
        }
        throw std::logic_error("Unknown UiCapture sample game mode.");
    }

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

    [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view text)
    {
        std::u16string result;
        result.reserve(text.size());
        std::size_t index = 0;
        while (index < text.size())
        {
            const auto first = static_cast<unsigned char>(text[index]);
            char32_t value = 0xFFFDU;
            std::size_t consumed = 1;

            if (first <= 0x7FU)
            {
                value = first;
            }
            else if (first >= 0xC2U && first <= 0xDFU && index + 1 < text.size())
            {
                const auto second = static_cast<unsigned char>(text[index + 1]);
                if ((second & 0xC0U) == 0x80U)
                {
                    value = static_cast<char32_t>(((first & 0x1FU) << 6)
                        | (second & 0x3FU));
                    consumed = 2;
                }
            }
            else if (first >= 0xE0U && first <= 0xEFU && index + 2 < text.size())
            {
                const auto second = static_cast<unsigned char>(text[index + 1]);
                const auto third = static_cast<unsigned char>(text[index + 2]);
                const bool secondValid = (second & 0xC0U) == 0x80U
                    && !(first == 0xE0U && second < 0xA0U)
                    && !(first == 0xEDU && second >= 0xA0U);
                if (secondValid && (third & 0xC0U) == 0x80U)
                {
                    value = static_cast<char32_t>(((first & 0x0FU) << 12)
                        | ((second & 0x3FU) << 6) | (third & 0x3FU));
                    consumed = 3;
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U && index + 3 < text.size())
            {
                const auto second = static_cast<unsigned char>(text[index + 1]);
                const auto third = static_cast<unsigned char>(text[index + 2]);
                const auto fourth = static_cast<unsigned char>(text[index + 3]);
                const bool secondValid = (second & 0xC0U) == 0x80U
                    && !(first == 0xF0U && second < 0x90U)
                    && !(first == 0xF4U && second > 0x8FU);
                if (secondValid && (third & 0xC0U) == 0x80U
                    && (fourth & 0xC0U) == 0x80U)
                {
                    value = static_cast<char32_t>(((first & 0x07U) << 18)
                        | ((second & 0x3FU) << 12)
                        | ((third & 0x3FU) << 6) | (fourth & 0x3FU));
                    consumed = 4;
                }
            }

            if (value <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(value));
            }
            else
            {
                value -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (value >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00U + (value & 0x3FFU)));
            }
            index += consumed;
        }
        return result;
    }

    [[nodiscard]] std::u16string ManagedTempPath()
    {
#ifdef _WIN32
        using GetTempPathFunction = DWORD (WINAPI*)(DWORD, LPWSTR);
        GetTempPathFunction getTempPath = &::GetTempPathW;
        if (HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll"); kernel32 != nullptr)
        {
            if (FARPROC procedure = ::GetProcAddress(kernel32, "GetTempPath2W"); procedure != nullptr)
            {
                getTempPath = reinterpret_cast<GetTempPathFunction>(procedure);
            }
        }

        DWORD capacity = MAX_PATH + 1U;
        for (;;)
        {
            std::wstring buffer(static_cast<std::size_t>(capacity), L'\0');
            const DWORD length = getTempPath(capacity, buffer.data());
            if (length == 0U)
            {
                throw std::runtime_error("The temporary path could not be determined.");
            }
            if (length < capacity)
            {
                buffer.resize(static_cast<std::size_t>(length));
                static_assert(sizeof(wchar_t) == sizeof(char16_t));
                return std::u16string(
                    reinterpret_cast<const char16_t*>(buffer.data()), buffer.size());
            }
            if (length == std::numeric_limits<DWORD>::max())
            {
                throw std::length_error("The temporary path is too long.");
            }
            capacity = length + 1U;
        }
#else
        const char* environment = std::getenv("TMPDIR");
        std::string path = environment != nullptr && *environment != '\0'
            ? std::string(environment)
            : std::string("/tmp");
        if (path.empty() || path.back() != '/')
        {
            path.push_back('/');
        }
        return Utf8ToUtf16(path);
#endif
    }

    [[nodiscard]] bool IsManagedPathRooted(std::u16string_view path) noexcept
    {
        if (path.empty())
        {
            return false;
        }
#ifdef _WIN32
        if (path[0] == u'\\' || path[0] == u'/')
        {
            return true;
        }
        return path.size() >= 2 && path[1] == u':';
#else
        return path[0] == u'/';
#endif
    }

    [[nodiscard]] std::u16string ManagedPathCombine(
        std::u16string_view left, std::u16string_view right)
    {
        if (left.empty())
        {
            return std::u16string(right);
        }
        if (right.empty())
        {
            return std::u16string(left);
        }
        if (IsManagedPathRooted(right))
        {
            return std::u16string(right);
        }

        std::u16string result(left);
        const char16_t last = result.back();
#ifdef _WIN32
        if (last != u'\\' && last != u'/' && last != u':')
        {
            result.push_back(u'\\');
        }
#else
        if (last != u'/')
        {
            result.push_back(u'/');
        }
#endif
        result.append(right);
        return result;
    }

    class CaptureLogShare final : public MphRead::Mods::ILogShare
    {
    public:
        [[nodiscard]] std::u16string StagingPath(std::u16string_view fileName) override
        {
            return ManagedPathCombine(ManagedTempPath(), fileName);
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
    UiCaptureArgumentNullException::UiCaptureArgumentNullException(std::string parameterName)
        : std::invalid_argument(
            "Value cannot be null. (Parameter '" + parameterName + "')"),
          _parameterName(std::move(parameterName))
    {
    }

    const std::string& UiCaptureArgumentNullException::ParameterName() const noexcept
    {
        return _parameterName;
    }

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
        throw UiCaptureArgumentNullException("path");
    }

    std::int32_t UiCapture::Run(UiCaptureAdapter& adapter, std::string_view directory)
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

            const auto capture = [&](std::string_view name,
                UiCaptureControlHandle view, UiCaptureSize size)
            {
                const std::string path = adapter.PathCombine(
                    directory, std::string(name) + ".png");
                if (Capture(adapter, view, path, size))
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
            + " screen(s) written to " + std::string(directory));
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
            "MP3 PROVING GROUND", Network::DemoLibraryDateTime(NowTicks), 1'512'320);
        demos->emplace_back(
            "COMBAT HALL_2026-09-02_21-04-55.fpdemo",
            "COMBAT HALL", Network::DemoLibraryDateTime(NowTicks - TwoDaysTicks), 402'112);
        demos->emplace_back(
            "sent-to-me.fpdemo", "",
            Network::DemoLibraryDateTime(NowTicks - NineDaysTicks), 88'400);
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
                status.Mode = ResolveMode(adapter, sample.Mode);
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
