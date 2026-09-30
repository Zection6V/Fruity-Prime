#include "AndroidUiSurface.hpp"

#include "MainActivity.hpp"

#include "../MphRead.Native/Mods/DebugLog.hpp"
#include "../MphRead.Native/Mods/Render/HunterShot.hpp"
#include "../MphRead.Native/Mods/Launcher/Gui/UiScaleHost.hpp"
#include "../MphRead.Native/NativeRuntime/Avalonia/Threading.hpp"
#include "../MphRead.Native/NativeRuntime/System/AtomicSharedPtr.hpp"
#include "../MphRead.Native/NativeRuntime/System/ExceptionText.hpp"
#include "../MphRead.Native/NativeRuntime/System/Exceptions.hpp"
#include "../MphRead.Native/NativeRuntime/System/Number.hpp"
#include "../MphRead.Native/NativeRuntime/System/Console.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <exception>
#include <string>
#include <utility>

namespace
{
    MphRead::NativeRuntime::AtomicSharedPtr<
        MphRead::Droid::AndroidUiSurface> CurrentSurface{};
    std::mutex CurrentSurfaceGate;

    [[nodiscard]] std::int32_t IncrementVersion(std::int32_t value) noexcept
    {
        const std::uint32_t incremented =
            static_cast<std::uint32_t>(value) + 1u;
        return std::bit_cast<std::int32_t>(incremented);
    }
}

namespace MphRead::Droid
{
    std::shared_ptr<AndroidUiSurface> AndroidUiSurface::Current() noexcept
    {
        return CurrentSurface.load(std::memory_order_acquire);
    }

    std::shared_ptr<AndroidUiSurface> AndroidUiSurface::Ensure()
    {
        std::lock_guard lock(CurrentSurfaceGate);
        std::shared_ptr<AndroidUiSurface> current = Current();
        if (current != nullptr)
        {
            return current;
        }

        try
        {
            current = std::shared_ptr<AndroidUiSurface>(
                new AndroidUiSurface());
            CurrentSurface.store(current, std::memory_order_release);
        }
        catch (const std::exception& ex)
        {
            ::MphRead::NativeRuntime::ConsoleWriteLine(
                std::string("[ui] the in-frame surface could not be built: ")
                + ::MphRead::NativeRuntime::ExceptionToString(ex));
            ::MphRead::Mods::DebugLog::Exception("ui", ex);
        }
        return current;
    }

    AndroidUiSurface::AndroidUiSurface()
        : _impl(std::make_unique<Mods::Launcher::Gui::UiTopLevelImpl>()),
          _host(std::make_shared<Av::Controls::LayoutTransformControl>())
    {
        // The Java LauncherView consumes premultiplied RGBA pixels. Android's
        // game rendering still uses GLES; only this launcher top level uses
        // the in-tree CPU canvas instead of desktop Skia Ganesh/GLFW.
        _impl->GpuRendering(false);

        _host->LayoutTransform(
            std::make_shared<Av::Media::ScaleTransform>(1.0, 1.0));
        _host->HorizontalAlignment(Av::Layout::HorizontalAlignment::Stretch);
        _host->VerticalAlignment(Av::Layout::VerticalAlignment::Stretch);

        _impl->SetClientSize(Av::Size{
            static_cast<double>(_width),
            static_cast<double>(_height)
        });
        _impl->Painted([this]()
        {
            Painted();
        });

        Av::EmbeddableControlRoot& window = _impl->Root();
        window.Background(Av::Media::Brushes::Transparent());
        window.TransparencyLevelHint = std::vector<
            Av::Controls::WindowTransparencyLevel>{
                Av::Controls::WindowTransparencyLevel::Transparent
            };
        window.RequestedThemeVariant = Av::Styling::ThemeVariant::Dark;
        window.Content(_host);
        _impl->Prepare();
        _impl->StartRendering();
        Mods::Launcher::Gui::UiRenderTimer::Install();
    }

    bool AndroidUiSurface::Visible() const noexcept
    {
        std::lock_guard lock(_gate);
        return _view != nullptr;
    }

    void AndroidUiSurface::Resize(
        std::int32_t width,
        std::int32_t height
    )
    {
        if (width <= 0 || height <= 0
            || (width == _width && height == _height))
        {
            return;
        }

        _width = width;
        _height = height;
        _impl->SetClientSize(Av::Size{
            static_cast<double>(width),
            static_cast<double>(height)
        });
        Mods::Render::HunterShot::FrameWidth = width;
        Mods::Render::HunterShot::FrameHeight = height;
        ApplyScale();
    }

    void AndroidUiSurface::ApplyScale()
    {
        MainActivity* activity = MainActivity::Instance();
        double density = activity != nullptr
            ? activity->DisplayDensity()
            : 1.0;
        density = density <= 0.0 ? 1.0 : density;

        const double points = Mods::Launcher::Gui::UiScaleHost::FactorFor(
            _width / density,
            _height / density
        );
        const double factor = points * density;
        Mods::Render::HunterShot::FrameScale = factor;

        const Av::Media::TransformPtr transform = _host->LayoutTransform();
        const auto* current =
            dynamic_cast<const Av::Media::ScaleTransform*>(transform.get());
        if (current != nullptr
            && std::abs(current->ScaleY - factor) < 0.0001)
        {
            return;
        }

        _host->LayoutTransform(
            std::make_shared<Av::Media::ScaleTransform>(factor, factor));
        ::MphRead::Mods::DebugLog::Line(
            "ui",
            "the in-frame surface is at "
                + ::MphRead::NativeRuntime::ToString(factor, "0.###")
                + "x (" + ::MphRead::NativeRuntime::ToString(_width)
                + "x" + ::MphRead::NativeRuntime::ToString(_height)
                + " pixels)"
        );
    }

    void AndroidUiSurface::Show(const Av::Controls::ControlPtr& view)
    {
        {
            std::lock_guard lock(_gate);
            _view = view;
        }
        _host->Child(view);
        Mods::Render::HunterShot::InFrame = true;
        ApplyScale();
        Av::Controls::ControlPtr focusView = view;
        Av::Threading::Dispatcher::UIThread().Post(
            [focusView]()
            {
                if (focusView == nullptr)
                {
                    throw ::System::NullReferenceException();
                }
                (void)focusView->Focus();
            },
            Av::Threading::DispatcherPriority::Background
        );
    }

    void AndroidUiSurface::Hide()
    {
        {
            std::lock_guard lock(_gate);
            _view.reset();
        }
        _host->Child(nullptr);
        Mods::Render::HunterShot::InFrame = false;
        Mods::Render::HunterShot::HoleWanted = false;
        {
            std::lock_guard lock(_gate);
            _version = IncrementVersion(_version);
            _frameWidth = 0;
            _frameHeight = 0;
        }
    }

    void AndroidUiSurface::Tick()
    {
        if (!Visible())
        {
            return;
        }
        Av::Threading::Dispatcher::UIThread().RunJobs();
        Mods::Launcher::Gui::UiRenderTimer::Pump(*_impl);
    }

    void AndroidUiSurface::Painted()
    {
        const std::uint8_t* address = _impl->Pixels();
        const std::int32_t width = _impl->PixelWidth();
        const std::int32_t height = _impl->PixelHeight();
        if (address == nullptr || width <= 0 || height <= 0)
        {
            return;
        }

        const std::size_t bytes =
            static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height)
            * 4u;
        std::lock_guard lock(_gate);
        if (_frame.size() < bytes)
        {
            _frame.resize(bytes);
        }
        std::copy_n(address, bytes, _frame.data());
        _frameWidth = width;
        _frameHeight = height;
        _version = IncrementVersion(_version);
    }

    bool AndroidUiSurface::TakeFrame(
        std::vector<std::uint8_t>& into,
        std::int32_t& version,
        std::int32_t& width,
        std::int32_t& height
    )
    {
        std::lock_guard lock(_gate);
        width = _frameWidth;
        height = _frameHeight;
        if (_view == nullptr || width <= 0 || height <= 0
            || version == _version)
        {
            return false;
        }

        const std::size_t bytes =
            static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height)
            * 4u;
        if (into.size() < bytes)
        {
            into.resize(bytes);
        }
        std::copy_n(_frame.data(), bytes, into.data());
        version = _version;
        return true;
    }

    void AndroidUiSurface::TouchDown(double x, double y)
    {
        _touchId = std::bit_cast<std::int64_t>(
            std::bit_cast<std::uint64_t>(_touchId) + 1u);
        _impl->TouchBegin(Av::Point{x, y}, _touchId);
    }

    void AndroidUiSurface::TouchMove(double x, double y)
    {
        _impl->TouchUpdate(Av::Point{x, y}, _touchId);
    }

    void AndroidUiSurface::TouchUp(double x, double y)
    {
        _impl->TouchEnd(Av::Point{x, y}, _touchId);
    }
}
