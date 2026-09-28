// RendererPlatform on Qt: the game window is a QWindow and the frame loop is
// still the renderer's own (Run pumps Qt's events, then raises one frame), so
// nothing above RendererPlatform changes when FRUITY_UI=qt replaces GLFW.
//
// The surface is OpenGL for as long as the renderer is. When the RHI's Vulkan
// backend lands this window becomes a VulkanSurface whose native handle the RHI
// builds its own swapchain on; the event side below stays as it is.

#include "../../MphRead.Native/Renderer.hpp"

#include "../../MphRead.Native/NativeRuntime/System/Heartbeat.hpp"
#include "QtApp.hpp"
#include "QtKeys.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QOpenGLContext>
#include <QtGui/QScreen>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QWheelEvent>
#include <QtGui/QWindow>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace
{
    using MphRead::RendererPlatform::CursorState;
    using MphRead::RendererPlatform::FrameEventArgs;
    using MphRead::RendererPlatform::GLFWException;
    using MphRead::RendererPlatform::MonitorArea;
    using MphRead::RendererPlatform::MouseButtonEventArgs;
    using MphRead::RendererPlatform::MouseMoveEventArgs;
    using MphRead::RendererPlatform::MouseWheelEventArgs;
    using MphRead::RendererPlatform::ResizeEventArgs;
    using MphRead::RendererPlatform::TextInputEventArgs;
    using MphRead::RendererPlatform::VSyncMode;
    using MphRead::RendererPlatform::WindowBorderValue;
    using MphRead::RendererPlatform::WindowEvents;
    using MphRead::RendererPlatform::WindowPositionEventArgs;
    using MphRead::RendererPlatform::WindowSettings;
    using MphRead::RendererPlatform::WindowStateValue;
    using ::OpenTK::Mathematics::Vector2i;
    using ::OpenTK::Windowing::Common::KeyboardKeyEventArgs;
    using GlfwMouseButton = ::OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;

    // GLFW's own numbering, which Keybind and every saved binding use.
    [[nodiscard]] int GlfwButton(Qt::MouseButton button) noexcept
    {
        switch (button)
        {
        case Qt::LeftButton: return 0;
        case Qt::RightButton: return 1;
        case Qt::MiddleButton: return 2;
        case Qt::BackButton: return 3;
        case Qt::ForwardButton: return 4;
        case Qt::ExtraButton3: return 5;
        case Qt::ExtraButton4: return 6;
        case Qt::ExtraButton5: return 7;
        default: return -1;
        }
    }

    [[nodiscard]] MonitorArea AreaOf(const QRect& rect, qreal scale)
    {
        // GLFW reports monitors in physical pixels on every platform the game
        // ships on; Qt reports device-independent ones.
        MonitorArea area;
        area.Min = Vector2i(static_cast<int>(rect.x() * scale), static_cast<int>(rect.y() * scale));
        area.Size = Vector2i(static_cast<int>(rect.width() * scale),
            static_cast<int>(rect.height() * scale));
        return area;
    }

    class GameQWindow;

    class QtWindow final : public MphRead::RendererPlatform::Window
    {
    public:
        explicit QtWindow(const WindowSettings& settings);
        ~QtWindow() override;

        void Run(WindowEvents& events) override;
        [[nodiscard]] Vector2i Size() const override;
        [[nodiscard]] MphRead::RendererPlatform::KeyboardState& Keyboard() override { return _keyboard; }
        [[nodiscard]] MphRead::RendererPlatform::MouseState& Mouse() override { return _mouse; }
        void Title(std::string value) override;
        void MinimumSize(Vector2i value) override;
        void Cursor(CursorState value) override;
        void VSync(VSyncMode value) override;
        void UpdateFrequency(double value) override { _updateFrequency = value; }
        void Visible(bool value) override;
        void SetIcon(const MphRead::RendererPlatform::WindowIcon& icon) override;
        [[nodiscard]] void* NativeHandle() const override;
        void Close() override { _closeRequested = true; }
        void SwapBuffers() override;

        void BaseOnClosing() override {}
        void BaseOnLoad() override {}
        void BaseOnRenderFrame(const FrameEventArgs& args) override { (void)args; }
        void BaseOnResize(const ResizeEventArgs& e) override { (void)e; }
        void BaseOnMove(const WindowPositionEventArgs& e) override { (void)e; }
        void BaseOnMaximizedChanged(bool maximized) override { (void)maximized; }
        void BaseOnFocusedChanged(bool focused) override { (void)focused; }
        void BaseOnMouseDown(const MouseButtonEventArgs& e) override { (void)e; }
        void BaseOnMouseUp(const MouseButtonEventArgs& e) override { (void)e; }
        void BaseOnMouseMove(const MouseMoveEventArgs& e) override { (void)e; }
        void BaseOnMouseWheel(const MouseWheelEventArgs& e) override { (void)e; }
        void BaseOnTextInput(const TextInputEventArgs& e) override { (void)e; }
        void BaseOnKeyDown(const KeyboardKeyEventArgs& e) override { (void)e; }
        void BaseOnKeyUp(const KeyboardKeyEventArgs& e) override { (void)e; }

        [[nodiscard]] std::int32_t WindowBorder() const override;
        void WindowBorder(std::int32_t value) override;
        [[nodiscard]] Vector2i Location() const override;
        void Location(Vector2i value) override;
        [[nodiscard]] Vector2i ClientSize() const override;
        void ClientSize(Vector2i value) override;
        [[nodiscard]] MonitorArea CurrentMonitorClientArea() const override;
        [[nodiscard]] MonitorArea CurrentMonitorWorkArea() const override;
        [[nodiscard]] std::vector<MonitorArea> MonitorClientAreas() const override;
        [[nodiscard]] WindowStateValue WindowState() const override;
        void WindowStateMaximized() override { _window->showMaximized(); }
        void WindowStateNormal() override { _window->showNormal(); }
        void Floating(bool value) override { _window->setFlag(Qt::WindowStaysOnTopHint, value); }
        [[nodiscard]] bool IsFocused() const override { return _window->isActive(); }
        [[nodiscard]] Vector2i ClientLocation() const override;
        void Focus() override { _window->requestActivate(); }

        // From GameQWindow.
        bool HandleEvent(QEvent* event);

    private:
        [[nodiscard]] qreal Scale() const { return _window->devicePixelRatio(); }
        [[nodiscard]] QScreen* ScreenOf() const;
        void Key(QKeyEvent* event, bool down);
        void MouseButton(QMouseEvent* event, bool down);
        void MouseMove(QMouseEvent* event);
        void Wheel(QWheelEvent* event);
        void RecentreGrabbedCursor();

        std::unique_ptr<GameQWindow> _window;
        std::unique_ptr<QOpenGLContext> _context;
        WindowEvents* _events = nullptr;
        double _updateFrequency = 0.0;
        bool _closeRequested = false;
        bool _grabbed = false;
        bool _maximized = false;
        MphRead::RendererPlatform::KeyboardState _keyboard{};
        MphRead::RendererPlatform::MouseState _mouse{};
        // The cursor position the game sees, in framebuffer-sized window
        // pixels. While grabbed it is virtual and unbounded, as GLFW's
        // CURSOR_DISABLED is: the real cursor is re-centred after each move.
        float _cursorX = 0.0F;
        float _cursorY = 0.0F;
        float _lastReportedMouseX = 0.0F;
        float _lastReportedMouseY = 0.0F;
        QPointF _grabCentre{};
        bool _warping = false;
    };

    class GameQWindow final : public QWindow
    {
    public:
        explicit GameQWindow(QtWindow& owner) : _owner(owner) {}

    protected:
        bool event(QEvent* event) override
        {
            if (_owner.HandleEvent(event))
            {
                return true;
            }
            return QWindow::event(event);
        }

    private:
        QtWindow& _owner;
    };

    QtWindow::QtWindow(const WindowSettings& settings)
    {
        MphRead::Qt::EnsureApplication();

        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(settings.ApiMajor, settings.ApiMinor);
        // ContextProfile.Compatability, by name: the renderer still draws in
        // immediate mode, which a core profile does not have.
        format.setProfile(settings.Profile == WindowSettings::ContextProfile::Compatability
            ? QSurfaceFormat::CompatibilityProfile
            : QSurfaceFormat::NoProfile);
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);
        format.setSwapInterval(1);

        _window = std::make_unique<GameQWindow>(*this);
        _window->setSurfaceType(QSurface::OpenGLSurface);
        _window->setFormat(format);
        _window->setTitle(QString::fromStdString(settings.Title));
        _window->resize(settings.ClientSize.X, settings.ClientSize.Y);
        _window->create();

        _context = std::make_unique<QOpenGLContext>();
        _context->setFormat(format);
        if (!_context->create())
        {
            throw GLFWException("The OpenGL context could not be created.", 0x00010006);
        }
        if (!_context->makeCurrent(_window.get()))
        {
            throw GLFWException("The OpenGL context could not be made current.", 0x00010008);
        }
        _updateFrequency = settings.UpdateFrequency;
        const QPointF cursor = _window->mapFromGlobal(QCursor::pos());
        _cursorX = _lastReportedMouseX = static_cast<float>(cursor.x() * Scale());
        _cursorY = _lastReportedMouseY = static_cast<float>(cursor.y() * Scale());
        _mouse.X = _cursorX;
        _mouse.Y = _cursorY;
        if (settings.StartVisible)
        {
            _window->show();
        }
    }

    QtWindow::~QtWindow()
    {
        if (_context != nullptr)
        {
            _context->doneCurrent();
        }
        _context.reset();
        _window.reset();
    }

    void QtWindow::Run(WindowEvents& events)
    {
        _events = &events;
        _context->makeCurrent(_window.get());
        events.OnLoad();
        ResizeEventArgs resize;
        resize.Size = Size();
        events.OnResize(resize);

        auto previous = std::chrono::steady_clock::now();
        while (!_closeRequested)
        {
            ::MphRead::NativeRuntime::FrameHeartbeat();
            // OpenTK's NewInputFrame: the frame sees the cursor where it was
            // before this frame's events arrived.
            _mouse.X = _cursorX;
            _mouse.Y = _cursorY;
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            if (_closeRequested)
            {
                break;
            }
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - previous).count();
            if (_updateFrequency > 0.0 && elapsed < 1.0 / _updateFrequency)
            {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(1.0 / _updateFrequency - elapsed));
                continue;
            }
            previous = now;
            FrameEventArgs args;
            args.Time = elapsed;
            events.OnRenderFrame(args);
        }
        events.OnClosing();
        _events = nullptr;
    }

    Vector2i QtWindow::Size() const
    {
        const QSize size = _window->size() * Scale();
        return Vector2i(size.width(), size.height());
    }

    void QtWindow::Title(std::string value)
    {
        _window->setTitle(QString::fromStdString(value));
    }

    void QtWindow::MinimumSize(Vector2i value)
    {
        _window->setMinimumSize(QSize(value.X, value.Y));
    }

    void QtWindow::Cursor(CursorState value)
    {
        const bool grab = value == CursorState::Grabbed;
        if (grab == _grabbed)
        {
            return;
        }
        _grabbed = grab;
        if (grab)
        {
            _window->setCursor(Qt::BlankCursor);
            _window->setMouseGrabEnabled(true);
            RecentreGrabbedCursor();
        }
        else
        {
            _window->setMouseGrabEnabled(false);
            _window->unsetCursor();
        }
    }

    void QtWindow::VSync(VSyncMode value)
    {
        // The GLX and WGL contexts apply a changed swap interval on the next
        // makeCurrent against the window's format.
        QSurfaceFormat format = _window->format();
        format.setSwapInterval(value == VSyncMode::On ? 1 : 0);
        _window->setFormat(format);
        _context->makeCurrent(_window.get());
    }

    void QtWindow::Visible(bool value)
    {
        if (value)
        {
            _window->show();
        }
        else
        {
            _window->hide();
        }
    }

    void QtWindow::SetIcon(const MphRead::RendererPlatform::WindowIcon& icon)
    {
        QIcon result;
        for (const MphRead::RendererPlatform::WindowIconImage& image : icon.Images)
        {
            const QImage rgba(image.Pixels.data(), image.Width, image.Height,
                image.Width * 4, QImage::Format_RGBA8888);
            result.addPixmap(QPixmap::fromImage(rgba.copy()));
        }
        _window->setIcon(result);
    }

    void* QtWindow::NativeHandle() const
    {
        return reinterpret_cast<void*>(_window->winId());
    }

    void QtWindow::SwapBuffers()
    {
        _context->swapBuffers(_window.get());
    }

    std::int32_t QtWindow::WindowBorder() const
    {
        const Qt::WindowFlags flags = _window->flags();
        if (flags.testFlag(Qt::FramelessWindowHint))
        {
            return static_cast<std::int32_t>(WindowBorderValue::Hidden);
        }
        if (_window->minimumSize() == _window->maximumSize())
        {
            return static_cast<std::int32_t>(WindowBorderValue::Fixed);
        }
        return static_cast<std::int32_t>(WindowBorderValue::Resizable);
    }

    void QtWindow::WindowBorder(std::int32_t value)
    {
        const auto border = static_cast<WindowBorderValue>(value);
        _window->setFlag(Qt::FramelessWindowHint, border == WindowBorderValue::Hidden);
        if (border == WindowBorderValue::Resizable)
        {
            _window->setMaximumSize(QSize(QWINDOWSIZE_MAX, QWINDOWSIZE_MAX));
        }
        else
        {
            _window->setMinimumSize(_window->size());
            _window->setMaximumSize(_window->size());
        }
    }

    Vector2i QtWindow::Location() const
    {
        const QRect frame = _window->frameGeometry();
        return Vector2i(static_cast<int>(frame.x() * Scale()), static_cast<int>(frame.y() * Scale()));
    }

    void QtWindow::Location(Vector2i value)
    {
        _window->setFramePosition(QPoint(static_cast<int>(value.X / Scale()),
            static_cast<int>(value.Y / Scale())));
    }

    Vector2i QtWindow::ClientSize() const
    {
        // GLFW's window size is in screen coordinates, which are the logical
        // size Qt reports, and the framebuffer is the physical one (Size()).
        return Vector2i(_window->width(), _window->height());
    }

    void QtWindow::ClientSize(Vector2i value)
    {
        _window->resize(value.X, value.Y);
    }

    QScreen* QtWindow::ScreenOf() const
    {
        QScreen* screen = _window->screen();
        return screen != nullptr ? screen : QGuiApplication::primaryScreen();
    }

    MonitorArea QtWindow::CurrentMonitorClientArea() const
    {
        QScreen* screen = ScreenOf();
        return screen == nullptr ? MonitorArea{} : AreaOf(screen->geometry(), screen->devicePixelRatio());
    }

    MonitorArea QtWindow::CurrentMonitorWorkArea() const
    {
        QScreen* screen = ScreenOf();
        return screen == nullptr ? MonitorArea{}
                                 : AreaOf(screen->availableGeometry(), screen->devicePixelRatio());
    }

    std::vector<MonitorArea> QtWindow::MonitorClientAreas() const
    {
        std::vector<MonitorArea> areas;
        for (QScreen* screen : QGuiApplication::screens())
        {
            areas.push_back(AreaOf(screen->geometry(), screen->devicePixelRatio()));
        }
        return areas;
    }

    WindowStateValue QtWindow::WindowState() const
    {
        switch (_window->windowState())
        {
        case Qt::WindowMinimized: return WindowStateValue::Minimized;
        case Qt::WindowMaximized: return WindowStateValue::Maximized;
        case Qt::WindowFullScreen: return WindowStateValue::Fullscreen;
        default: return WindowStateValue::Normal;
        }
    }

    Vector2i QtWindow::ClientLocation() const
    {
        const QPoint origin = _window->position();
        return Vector2i(static_cast<int>(origin.x() * Scale()), static_cast<int>(origin.y() * Scale()));
    }

    bool QtWindow::HandleEvent(QEvent* event)
    {
        switch (event->type())
        {
        case QEvent::Close:
            // The close button asks; the loop ends and the renderer closes.
            _closeRequested = true;
            event->ignore();
            return true;
        case QEvent::Resize:
        case QEvent::Expose:
            if (_events != nullptr && _window->isExposed())
            {
                ResizeEventArgs args;
                args.Size = Size();
                _events->OnResize(args);
            }
            return false;
        case QEvent::Move:
            if (_events != nullptr)
            {
                WindowPositionEventArgs args;
                args.Position = ClientLocation();
                _events->OnMove(args);
            }
            return false;
        case QEvent::WindowStateChange:
        {
            const bool maximized = _window->windowState() == Qt::WindowMaximized;
            if (maximized != _maximized)
            {
                _maximized = maximized;
                if (_events != nullptr)
                {
                    _events->OnMaximizedChanged(maximized);
                }
            }
            return false;
        }
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            if (_events != nullptr)
            {
                _events->OnFocusedChanged(event->type() == QEvent::FocusIn);
            }
            return false;
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            Key(static_cast<QKeyEvent*>(event), event->type() == QEvent::KeyPress);
            return true;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseButtonRelease:
            MouseButton(static_cast<QMouseEvent*>(event), event->type() != QEvent::MouseButtonRelease);
            return true;
        case QEvent::MouseMove:
            MouseMove(static_cast<QMouseEvent*>(event));
            return true;
        case QEvent::Wheel:
            Wheel(static_cast<QWheelEvent*>(event));
            return true;
        default:
            return false;
        }
    }

    void QtWindow::Key(QKeyEvent* event, bool down)
    {
        // Auto-repeat raises OnKeyDown again, as GLFW_REPEAT does.
        const int key = MphRead::Qt::GlfwKey(*event);
        if (key >= 0)
        {
            _keyboard.SetKeyDown(static_cast<MphRead::RendererPlatform::Key>(key), down);
            if (_events != nullptr)
            {
                KeyboardKeyEventArgs args;
                args.Key = static_cast<MphRead::RendererPlatform::Key>(key);
                const Qt::KeyboardModifiers mods = event->modifiers();
                args.Shift = mods.testFlag(Qt::ShiftModifier);
                args.Control = mods.testFlag(Qt::ControlModifier);
                args.Alt = mods.testFlag(Qt::AltModifier);
                args.Command = mods.testFlag(Qt::MetaModifier);
                if (down)
                {
                    _events->OnKeyDown(args);
                }
                else
                {
                    _events->OnKeyUp(args);
                }
            }
        }
        // GLFW's char callback: typed text, once per character, repeats included.
        if (down && _events != nullptr)
        {
            const QString text = event->text();
            for (const char32_t codePoint : text.toUcs4())
            {
                if (codePoint >= 0x20 && codePoint != 0x7F)
                {
                    TextInputEventArgs args;
                    args.Unicode = static_cast<std::uint32_t>(codePoint);
                    _events->OnTextInput(args);
                }
            }
        }
    }

    void QtWindow::MouseButton(QMouseEvent* event, bool down)
    {
        const int button = GlfwButton(event->button());
        if (button < 0)
        {
            return;
        }
        const auto value = static_cast<GlfwMouseButton>(button);
        const bool wasDown = _mouse.IsButtonDown(value);
        _mouse.SetButtonDown(value, down);
        if (_events == nullptr || wasDown == down)
        {
            return;
        }
        MouseButtonEventArgs args;
        args.Button = value;
        if (down)
        {
            _events->OnMouseDown(args);
        }
        else
        {
            _events->OnMouseUp(args);
        }
    }

    void QtWindow::MouseMove(QMouseEvent* event)
    {
        const QPointF local = event->position();
        if (_grabbed)
        {
            if (_warping)
            {
                // The move our own re-centring caused.
                _warping = false;
                if ((local - _grabCentre).manhattanLength() < 1.0)
                {
                    return;
                }
            }
            _cursorX += static_cast<float>((local.x() - _grabCentre.x()) * Scale());
            _cursorY += static_cast<float>((local.y() - _grabCentre.y()) * Scale());
            RecentreGrabbedCursor();
        }
        else
        {
            _cursorX = static_cast<float>(local.x() * Scale());
            _cursorY = static_cast<float>(local.y() * Scale());
        }
        MouseMoveEventArgs args;
        args.X = _cursorX;
        args.Y = _cursorY;
        args.DeltaX = _cursorX - _lastReportedMouseX;
        args.DeltaY = _cursorY - _lastReportedMouseY;
        _lastReportedMouseX = _cursorX;
        _lastReportedMouseY = _cursorY;
        if (_events != nullptr && (args.DeltaX != 0.0F || args.DeltaY != 0.0F))
        {
            _events->OnMouseMove(args);
        }
    }

    void QtWindow::RecentreGrabbedCursor()
    {
        // QCursor::setPos is a no-op on Wayland, where pointer lock needs the
        // relative-pointer protocol; X11, Windows and macOS warp.
        _grabCentre = QPointF(_window->width() / 2.0, _window->height() / 2.0);
        _warping = true;
        QCursor::setPos(_window->screen(), _window->mapToGlobal(_grabCentre.toPoint()));
    }

    void QtWindow::Wheel(QWheelEvent* event)
    {
        // GLFW reports one notch as 1.0; Qt as 120 eighths of a degree.
        const QPoint delta = event->angleDelta();
        const float x = static_cast<float>(delta.x()) / 120.0F;
        const float y = static_cast<float>(delta.y()) / 120.0F;
        _mouse.Scroll.X += x;
        _mouse.Scroll.Y += y;
        if (_events != nullptr)
        {
            MouseWheelEventArgs args;
            args.OffsetX = x;
            args.OffsetY = y;
            _events->OnMouseWheel(args);
        }
    }
}

namespace MphRead::RendererPlatform
{
    std::shared_ptr<Window> CreateWindow(const WindowSettings& settings)
    {
        return std::make_shared<QtWindow>(settings);
    }

    OpenTK::Mathematics::Vector2i WorkAreaForWindow(Window& window)
    {
        return window.CurrentMonitorWorkArea().Size;
    }

    void ProcessEvents()
    {
        MphRead::Qt::EnsureApplication();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }

    void InstallGlfwErrorCallback(std::function<void(std::int32_t, std::string)> callback)
    {
        // Qt reports its own failures through its message handler.
        (void)callback;
    }

    std::int32_t GlfwFeatureUnavailableCode()
    {
        return 0x0001000C;
    }
}
