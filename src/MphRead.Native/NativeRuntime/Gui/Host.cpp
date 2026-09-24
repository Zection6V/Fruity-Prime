#include "Host.hpp"

#include "Backend.hpp"

#include "../OpenTK/GL.hpp"
#include "../System/Heartbeat.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <utility>

namespace MphRead::NativeRuntime::Gui
{
    namespace Detail
    {
        // HostTaskbar.cpp, which is where <windows.h> is allowed.
        void HideFromTaskbar(GLFWwindow* handle);
    }

    namespace
    {
        // GLFW is initialized once for the process, as the renderer's own
        // platform layer does. It is never terminated, because the game's
        // window may still be using it.
        [[nodiscard]] bool EnsureGlfw()
        {
            static const bool ready = ::glfwInit() == GLFW_TRUE;
            return ready;
        }

        // Every toolkit window shares one context, so a texture made while
        // drawing one -- the glyph atlas, above all -- is the same texture in
        // the next. Without it the pause menu opened over a match drew every
        // letter as a blank box, because its own context had never seen the
        // atlas. The group is rooted in a window that is never shown and never
        // destroyed, so the textures outlive any window that made them.
        [[nodiscard]] GLFWwindow* ShareRoot()
        {
            static GLFWwindow* root = []() -> GLFWwindow*
            {
                if (!EnsureGlfw())
                {
                    return nullptr;
                }
                ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
                ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
                ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
                ::glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                return ::glfwCreateWindow(1, 1, "", nullptr, nullptr);
            }();
            return root;
        }

        // The context in force on the way in, put back on the way out: the
        // game's renderer owns one of its own and goes on drawing into it the
        // moment the pause menu's pump returns.
        class BorrowedContext final
        {
        public:
            explicit BorrowedContext(GLFWwindow* handle) noexcept
                : _previous(::glfwGetCurrentContext())
            {
                ::glfwMakeContextCurrent(handle);
            }

            ~BorrowedContext()
            {
                ::glfwMakeContextCurrent(_previous);
            }

            BorrowedContext(const BorrowedContext&) = delete;
            BorrowedContext& operator=(const BorrowedContext&) = delete;
            BorrowedContext(BorrowedContext&&) = delete;
            BorrowedContext& operator=(BorrowedContext&&) = delete;

        private:
            GLFWwindow* _previous = nullptr;
        };

        GLFWcursor*& CursorFor(Cursor kind)
        {
            static GLFWcursor* arrow = nullptr;
            static GLFWcursor* hand = nullptr;
            static GLFWcursor* beam = nullptr;
            switch (kind)
            {
            case Cursor::Hand:
                return hand;
            case Cursor::IBeam:
                return beam;
            default:
                return arrow;
            }
        }

        [[nodiscard]] Key Translate(int key) noexcept
        {
            switch (key)
            {
            case GLFW_KEY_ESCAPE: return Key::Escape;
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER: return Key::Enter;
            case GLFW_KEY_SPACE: return Key::Space;
            case GLFW_KEY_TAB: return Key::Tab;
            case GLFW_KEY_BACKSPACE: return Key::Backspace;
            case GLFW_KEY_DELETE: return Key::Delete;
            case GLFW_KEY_LEFT: return Key::Left;
            case GLFW_KEY_RIGHT: return Key::Right;
            case GLFW_KEY_UP: return Key::Up;
            case GLFW_KEY_DOWN: return Key::Down;
            case GLFW_KEY_HOME: return Key::Home;
            case GLFW_KEY_END: return Key::End;
            default: return Key::Other;
            }
        }

        [[nodiscard]] GLFWcursor* StandardCursor(Cursor kind)
        {
            GLFWcursor*& slot = CursorFor(kind);
            if (slot == nullptr)
            {
                const int shape = kind == Cursor::Hand ? GLFW_HAND_CURSOR
                    : kind == Cursor::IBeam            ? GLFW_IBEAM_CURSOR
                                                       : GLFW_ARROW_CURSOR;
                slot = ::glfwCreateStandardCursor(shape);
            }
            return slot;
        }
    }

    // --- Timer ------------------------------------------------------------

    void Timer::IntervalSeconds(double seconds) noexcept
    {
        _interval = seconds > 0.0 ? seconds : 0.0;
    }

    void Timer::Tick(std::function<void()> action)
    {
        _tick = std::move(action);
    }

    void Timer::Start()
    {
        _running = true;
        _next = ::glfwGetTime() + _interval;
    }

    void Timer::Stop() noexcept
    {
        _running = false;
    }

    void Timer::Service(double now)
    {
        if (!_running || now < _next)
        {
            return;
        }
        _next = now + _interval;
        if (_tick)
        {
            _tick();
        }
    }

    // --- Dispatcher -------------------------------------------------------

    Dispatcher& Dispatcher::Instance()
    {
        static Dispatcher instance;
        return instance;
    }

    void Dispatcher::Post(std::function<void()> action, Priority priority)
    {
        if (!action)
        {
            return;
        }
        const std::lock_guard<std::mutex> guard(_lock);
        if (priority == Priority::Background)
        {
            _background.push_back(std::move(action));
            return;
        }
        _normal.push_back(std::move(action));
    }

    std::shared_ptr<Timer> Dispatcher::CreateTimer()
    {
        auto timer = std::make_shared<Timer>();
        _timers.push_back(timer);
        return timer;
    }

    void Dispatcher::Register(Window* window)
    {
        if (std::find(_windows.begin(), _windows.end(), window) == _windows.end())
        {
            _windows.push_back(window);
        }
    }

    void Dispatcher::Unregister(Window* window)
    {
        _windows.erase(
            std::remove(_windows.begin(), _windows.end(), window), _windows.end());
    }

    bool Dispatcher::AnyWindowOpen() const noexcept
    {
        return std::any_of(_windows.begin(), _windows.end(),
            [](const Window* window) { return window->Open(); });
    }

    bool Dispatcher::Idle() const
    {
        const std::lock_guard<std::mutex> guard(_lock);
        return _normal.empty() && _background.empty();
    }

    Window* Dispatcher::WindowOf(const Element* root) const
    {
        for (Window* window : _windows)
        {
            if (window->Content().get() == root)
            {
                return window;
            }
        }
        return nullptr;
    }

    void Dispatcher::DrainQueue()
    {
        // Everything queued when the pass started, and nothing queued by it:
        // a callback that posts itself must not spin the frame. The queues are
        // taken under the lock and run outside it, since a callback may post.
        std::deque<std::function<void()>> normal;
        std::deque<std::function<void()>> background;
        {
            const std::lock_guard<std::mutex> guard(_lock);
            normal.swap(_normal);
            background.swap(_background);
        }
        for (const std::function<void()>& action : normal)
        {
            action();
        }
        for (const std::function<void()>& action : background)
        {
            action();
        }
    }

    void Dispatcher::ServiceTimers()
    {
        const double now = ::glfwGetTime();
        // A tick may stop or start a timer, so the list is copied first.
        const std::vector<std::shared_ptr<Timer>> timers = _timers;
        for (const std::shared_ptr<Timer>& timer : timers)
        {
            timer->Service(now);
        }
    }

    void Dispatcher::PumpOnce()
    {
        ::MphRead::NativeRuntime::FrameHeartbeat();
        if (EnsureGlfw())
        {
            ::glfwPollEvents();
        }
        DrainQueue();
        ServiceTimers();
        const std::vector<Window*> windows = _windows;
        for (Window* window : windows)
        {
            if (window->Open())
            {
                window->Service();
            }
        }
    }

    void Dispatcher::PushFrame(const std::function<bool()>& shouldContinue)
    {
        while (shouldContinue())
        {
            PumpOnce();
            if (!AnyWindowOpen() && Idle())
            {
                // Nothing left to draw and nothing left to run: a frame that
                // waited on a window whose close was missed would hang here.
                break;
            }
        }
    }

    // --- Window -----------------------------------------------------------

    bool Window::Available()
    {
        return EnsureGlfw();
    }

    Window::Window(WindowOptions options)
        : _transparent(options.Transparent)
    {
        if (!EnsureGlfw())
        {
            return;
        }
        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
        ::glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        ::glfwWindowHint(GLFW_DECORATED, options.Decorated ? GLFW_TRUE : GLFW_FALSE);
        ::glfwWindowHint(GLFW_FLOATING, options.Topmost ? GLFW_TRUE : GLFW_FALSE);
        ::glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER,
            options.Transparent ? GLFW_TRUE : GLFW_FALSE);
        _handle = ::glfwCreateWindow(940, 560, "", nullptr, ShareRoot());
        if (_handle == nullptr)
        {
            // Nothing above this can tell an unopenable window from one that
            // was never asked for, so it is said here.
            const char* description = nullptr;
            const int code = ::glfwGetError(&description);
            std::cout << "[launcher] the window could not be created (" << code
                << (description != nullptr ? std::string(": ") + description
                                           : std::string())
                << ")\n";
            return;
        }
        if (!options.ShowInTaskbar)
        {
            Detail::HideFromTaskbar(_handle);
        }
        ::glfwSetWindowUserPointer(_handle, this);
        ::glfwSetKeyCallback(_handle, &Window::OnKey);
        ::glfwSetCharCallback(_handle, &Window::OnChar);
        ::glfwSetMouseButtonCallback(_handle, &Window::OnMouseButton);
        ::glfwSetCursorPosCallback(_handle, &Window::OnCursorPos);
        ::glfwSetScrollCallback(_handle, &Window::OnScroll);
        ::glfwSetWindowCloseCallback(_handle, &Window::OnClose);
        ::glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        ::glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
        ::glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
        Dispatcher::Instance().Register(this);
    }

    Window::~Window()
    {
        Dispatcher::Instance().Unregister(this);
        if (_handle != nullptr)
        {
            ::glfwDestroyWindow(_handle);
            _handle = nullptr;
        }
    }

    void Window::Title(std::string_view value)
    {
        if (_handle != nullptr)
        {
            ::glfwSetWindowTitle(_handle, std::string(value).c_str());
        }
    }

    void Window::Icon(std::int32_t width, std::int32_t height, const std::uint8_t* rgba)
    {
        if (_handle == nullptr || width <= 0 || height <= 0 || rgba == nullptr)
        {
            return;
        }
        GLFWimage image{};
        image.width = width;
        image.height = height;
        // GLFW reads the pixels during the call and keeps nothing.
        image.pixels = const_cast<unsigned char*>(rgba);
        ::glfwSetWindowIcon(_handle, 1, &image);
    }

    void Window::ClientSize(double width, double height)
    {
        if (_handle != nullptr)
        {
            ::glfwSetWindowSize(
                _handle, static_cast<int>(width), static_cast<int>(height));
        }
    }

    void Window::MinimumSize(double width, double height)
    {
        if (_handle != nullptr)
        {
            ::glfwSetWindowSizeLimits(_handle, static_cast<int>(width),
                static_cast<int>(height), GLFW_DONT_CARE, GLFW_DONT_CARE);
        }
    }

    void Window::CenterOnScreen()
    {
        if (_handle == nullptr)
        {
            return;
        }
        GLFWmonitor* const monitor = ::glfwGetPrimaryMonitor();
        if (monitor == nullptr)
        {
            return;
        }
        int areaX = 0;
        int areaY = 0;
        int areaWidth = 0;
        int areaHeight = 0;
        ::glfwGetMonitorWorkarea(monitor, &areaX, &areaY, &areaWidth, &areaHeight);
        int width = 0;
        int height = 0;
        ::glfwGetWindowSize(_handle, &width, &height);
        ::glfwSetWindowPos(_handle, areaX + (areaWidth - width) / 2,
            areaY + (areaHeight - height) / 2);
    }

    void Window::Position(std::int32_t x, std::int32_t y)
    {
        if (_handle != nullptr)
        {
            ::glfwSetWindowPos(_handle, x, y);
        }
    }

    void Window::Decorated(bool value)
    {
        if (_handle != nullptr)
        {
            ::glfwSetWindowAttrib(
                _handle, GLFW_DECORATED, value ? GLFW_TRUE : GLFW_FALSE);
        }
    }

    void Window::Topmost(bool value)
    {
        if (_handle != nullptr)
        {
            ::glfwSetWindowAttrib(
                _handle, GLFW_FLOATING, value ? GLFW_TRUE : GLFW_FALSE);
        }
    }

    void Window::Show()
    {
        if (_handle == nullptr)
        {
            return;
        }
        _open = true;
        _shown = true;
        ::glfwShowWindow(_handle);
        ::glfwFocusWindow(_handle);
    }

    void Window::Close()
    {
        if (!_open)
        {
            return;
        }
        _open = false;
        if (_handle != nullptr)
        {
            ::glfwHideWindow(_handle);
        }
        // A handler may close another window, so the list is copied first.
        const std::vector<std::function<void()>> handlers = _closed;
        for (const std::function<void()>& handler : handlers)
        {
            handler();
        }
    }

    void Window::Closed(std::function<void()> handler)
    {
        if (handler)
        {
            _closed.push_back(std::move(handler));
        }
    }

    void Window::SizeChanged(std::function<void(double)> handler)
    {
        if (!handler)
        {
            return;
        }
        // Avalonia raises this once the control is in a laid-out tree; a
        // subscriber added after the first layout is told straight away.
        if (_reportedWidth >= 0.0)
        {
            handler(_reportedWidth);
        }
        _sizeChanged.push_back(std::move(handler));
    }

    void Window::KeyDown(std::function<void(std::int32_t, bool&)> handler)
    {
        _keyDown = std::move(handler);
    }

    void Window::TextInput(std::function<void(char32_t)> handler)
    {
        _textInput = std::move(handler);
    }

    void Window::Layout()
    {
        if (_content == nullptr)
        {
            return;
        }
        _content->Measure(Size{_width, _height});
        _content->Arrange(Rect{0.0, 0.0, _width, _height});
    }

    void Window::Draw()
    {
        Backend().BeginFrame(
            static_cast<std::int32_t>(_width), static_cast<std::int32_t>(_height));
        // Cleared rather than filled, so a transparent window writes its own
        // alpha instead of blending over whatever the buffer held.
        Backend().Clear(_background);
        Backend().EndFrame();

        if (_content != nullptr)
        {
            _renderer.Begin(static_cast<std::int32_t>(_width),
                static_cast<std::int32_t>(_height));
            _renderer.DrawElement(*_content);
            _renderer.End();
        }
    }

    void Window::Service()
    {
        if (_handle == nullptr || !_open)
        {
            return;
        }
        int width = 0;
        int height = 0;
        ::glfwGetFramebufferSize(_handle, &width, &height);
        _width = std::max(1, width);
        _height = std::max(1, height);

        const BorrowedContext context(_handle);
        Layout();

        if (_reportedWidth != _width)
        {
            _reportedWidth = _width;
            const std::vector<std::function<void(double)>> handlers = _sizeChanged;
            for (const std::function<void(double)>& handler : handlers)
            {
                handler(_width);
            }
            // A handler may have rebuilt the tree.
            Layout();
        }

        Draw();
        ApplyCursor();
        ::glfwSwapBuffers(_handle);
    }

    bool Window::Capture(
        std::int32_t width, std::int32_t height, std::vector<std::uint8_t>& rgba)
    {
        namespace GL = ::OpenTK::Graphics::OpenGL::GL;

        if (_handle == nullptr || _content == nullptr || width <= 0 || height <= 0)
        {
            return false;
        }
        const BorrowedContext context(_handle);

        const std::int32_t texture = GL::GenTexture();
        GL::BindTexture(GL::TextureTarget::Texture2D, texture);
        GL::TexImage2D(GL::TextureTarget::Texture2D, 0,
            GL::PixelInternalFormat::Rgba, width, height, 0, GL::PixelFormat::Rgba,
            GL::PixelType::UnsignedByte, nullptr);
        GL::TexParameter(GL::TextureTarget::Texture2D,
            GL::TextureParameterName::TextureMinFilter, 0x2601);
        GL::TexParameter(GL::TextureTarget::Texture2D,
            GL::TextureParameterName::TextureMagFilter, 0x2601);

        const std::int32_t buffer = GL::GenFramebuffer();
        GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, buffer);
        GL::FramebufferTexture2D(GL::FramebufferTarget::Framebuffer,
            GL::FramebufferAttachment::ColorAttachment0,
            GL::TextureTarget::Texture2D, texture, 0);
        const bool ready
            = GL::CheckFramebufferStatus(GL::FramebufferTarget::Framebuffer)
            == ::OpenTK::Graphics::OpenGL::FramebufferErrorCode::FramebufferComplete;
        if (ready)
        {
            _width = width;
            _height = height;
            Layout();
            Draw();
            rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);
            GL::ReadPixels(0, 0, width, height, GL::PixelFormat::Rgba,
                GL::PixelType::UnsignedByte, rgba.data());
        }

        GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, 0);
        GL::DeleteFramebuffer(buffer);
        GL::DeleteTexture(texture);
        return ready;
    }

    void Window::ApplyCursor()
    {
        const Cursor wanted = _hovered != nullptr ? _hovered->CursorKind : Cursor::Arrow;
        if (wanted == _cursor)
        {
            return;
        }
        _cursor = wanted;
        ::glfwSetCursor(_handle, StandardCursor(wanted));
    }

    namespace
    {
        // A routed event, bubbling from the element outwards as Avalonia's do.
        template <typename Get, typename Args>
        void Bubble(Element* element, Get get, Args& args)
        {
            for (Element* node = element; node != nullptr; node = node->Parent())
            {
                const auto& handler = get(*node);
                if (handler)
                {
                    handler(args);
                    if (args.Handled)
                    {
                        return;
                    }
                }
            }
        }
    }

    Window* Window::Of(const Element& element)
    {
        const Element* root = &element;
        while (root->Parent() != nullptr)
        {
            root = root->Parent();
        }
        return Dispatcher::Instance().WindowOf(root);
    }

    void Window::Capture(Element* element) noexcept
    {
        if (_captured == element)
        {
            return;
        }
        Element* const previous = _captured;
        _captured = element;
        if (previous != nullptr && previous->PointerCaptureLost)
        {
            previous->PointerCaptureLost();
        }
    }

    void Window::Focus(Element* element)
    {
        if (_focused == element)
        {
            return;
        }
        Element* const previous = _focused;
        _focused = element;
        if (previous != nullptr)
        {
            previous->IsFocused = false;
            if (previous->LostFocus)
            {
                previous->LostFocus();
            }
        }
        if (element != nullptr)
        {
            element->IsFocused = true;
            if (element->GotFocus)
            {
                element->GotFocus();
            }
        }
    }

    void Window::Move(double x, double y)
    {
        _pointerX = x;
        _pointerY = y;
        Element* const hit = _content != nullptr ? _content->HitTest(x, y) : nullptr;
        if (hit != _hovered)
        {
            Element* const previous = _hovered;
            _hovered = hit;
            if (previous != nullptr)
            {
                previous->IsPointerOver = false;
                if (previous->PointerExited)
                {
                    PointerEvent e{x, y, false};
                    previous->PointerExited(e);
                }
            }
            if (hit != nullptr)
            {
                hit->IsPointerOver = true;
                if (hit->PointerEntered)
                {
                    PointerEvent e{x, y, false};
                    hit->PointerEntered(e);
                }
            }
        }
        Element* const target = _captured != nullptr ? _captured : _hovered;
        if (target != nullptr && target->PointerMoved)
        {
            PointerEvent e{x, y, false};
            target->PointerMoved(e);
        }
    }

    void Window::Press(double x, double y)
    {
        Move(x, y);
        _pressed = _captured != nullptr ? _captured : _hovered;
        // Focus follows the click, onto the nearest focusable ancestor; a
        // click on nothing takes it away, which is what closes an editor.
        Element* wants = _pressed;
        while (wants != nullptr && !wants->Focusable)
        {
            wants = wants->Parent();
        }
        Focus(wants);
        if (_pressed == nullptr)
        {
            return;
        }
        PointerEvent e{x, y, false};
        Bubble(_pressed,
            [](Element& node) -> const std::function<void(PointerEvent&)>&
            { return node.PointerPressed; },
            e);
    }

    void Window::Release(double x, double y)
    {
        Move(x, y);
        Element* const target = _captured != nullptr ? _captured : _pressed;
        _pressed = nullptr;
        if (target == nullptr)
        {
            return;
        }
        PointerEvent e{x, y, false};
        Bubble(target,
            [](Element& node) -> const std::function<void(PointerEvent&)>&
            { return node.PointerReleased; },
            e);
    }

    void Window::RaiseKey(KeyEvent& e)
    {
        if (_focused != nullptr)
        {
            Bubble(_focused,
                [](Element& node) -> const std::function<void(KeyEvent&)>&
                { return node.KeyDown; },
                e);
        }
        if (!e.Handled && _keyDown)
        {
            bool handled = false;
            _keyDown(e.Code, handled);
            e.Handled = handled;
        }
    }

    void Window::RaiseText(char32_t code)
    {
        if (_focused != nullptr && _focused->TextInput)
        {
            _focused->TextInput(code);
            return;
        }
        if (_textInput)
        {
            _textInput(code);
        }
    }

    void Window::OnKey(GLFWwindow* handle, int key, int scancode, int action, int mods)
    {
        (void)scancode;
        if (action != GLFW_PRESS && action != GLFW_REPEAT)
        {
            return;
        }
        auto* const window = static_cast<Window*>(::glfwGetWindowUserPointer(handle));
        if (window == nullptr)
        {
            return;
        }
        KeyEvent e;
        e.Code = static_cast<std::int32_t>(key);
        e.Which = Translate(key);
        e.Shift = (mods & GLFW_MOD_SHIFT) != 0;
        e.Control = (mods & GLFW_MOD_CONTROL) != 0;
        window->RaiseKey(e);
    }

    void Window::OnChar(GLFWwindow* handle, unsigned int codepoint)
    {
        auto* const window = static_cast<Window*>(::glfwGetWindowUserPointer(handle));
        if (window != nullptr)
        {
            window->RaiseText(static_cast<char32_t>(codepoint));
        }
    }

    void Window::OnMouseButton(GLFWwindow* handle, int button, int action, int mods)
    {
        (void)mods;
        if (button != GLFW_MOUSE_BUTTON_LEFT)
        {
            return;
        }
        auto* const window = static_cast<Window*>(::glfwGetWindowUserPointer(handle));
        if (window == nullptr)
        {
            return;
        }
        double x = 0.0;
        double y = 0.0;
        ::glfwGetCursorPos(handle, &x, &y);
        if (action == GLFW_PRESS)
        {
            window->Press(x, y);
            return;
        }
        if (action == GLFW_RELEASE)
        {
            window->Release(x, y);
        }
    }

    void Window::OnCursorPos(GLFWwindow* handle, double x, double y)
    {
        auto* const window = static_cast<Window*>(::glfwGetWindowUserPointer(handle));
        if (window != nullptr)
        {
            window->Move(x, y);
        }
    }

    void Window::OnScroll(GLFWwindow* handle, double x, double y)
    {
        (void)x;
        auto* const window = static_cast<Window*>(::glfwGetWindowUserPointer(handle));
        if (window == nullptr)
        {
            return;
        }
        // The nearest scroll viewer over the pointer takes it.
        for (Element* element = window->_hovered; element != nullptr;
            element = element->Parent())
        {
            if (element->Kind() != ElementKind::ScrollViewer)
            {
                continue;
            }
            const double content = element->Children().empty()
                ? 0.0
                : element->Children().front()->Desired().Height;
            const double limit = std::max(0.0, content - element->Bounds().Height);
            element->ScrollOffset
                = std::clamp(element->ScrollOffset - y * 48.0, 0.0, limit);
            break;
        }
    }

    void Window::OnClose(GLFWwindow* handle)
    {
        auto* const window = static_cast<Window*>(::glfwGetWindowUserPointer(handle));
        if (window == nullptr)
        {
            return;
        }
        ::glfwSetWindowShouldClose(handle, GLFW_FALSE);
        window->Close();
    }
}
