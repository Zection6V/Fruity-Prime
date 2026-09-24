// OpenTK.Windowing.Desktop.GameWindow, over the same GLFW the managed build
// runs on. What this file owns is only the toolkit binding: every frame, every
// event and every state read is handed straight to the Scene's own members,
// which is what GameWindow does for the C# renderer.

#include "../../Renderer.hpp"

#include "../../Mods/Chat/ChatBox.hpp"
#include "../System/Console.hpp"

#include <chrono>
#include <iostream>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <GLFW/glfw3.h>

namespace
{
    using MphRead::RendererPlatform::CursorState;
    using MphRead::RendererPlatform::FrameEventArgs;
    using MphRead::RendererPlatform::GLFWException;
    using MphRead::RendererPlatform::MouseButtonEventArgs;
    using MphRead::RendererPlatform::MouseMoveEventArgs;
    using MphRead::RendererPlatform::MouseWheelEventArgs;
    using MphRead::RendererPlatform::ResizeEventArgs;
    using MphRead::RendererPlatform::TextInputEventArgs;
    using MphRead::RendererPlatform::VSyncMode;
    using MphRead::RendererPlatform::WindowEvents;
    using MphRead::RendererPlatform::WindowSettings;
    using ::OpenTK::Windowing::Common::KeyboardKeyEventArgs;

    std::function<void(std::int32_t, std::string)>& ErrorCallback()
    {
        static std::function<void(std::int32_t, std::string)> callback;
        return callback;
    }

    void ForwardGlfwError(int code, const char* description)
    {
        if (ErrorCallback())
        {
            ErrorCallback()(static_cast<std::int32_t>(code),
                description == nullptr ? std::string() : std::string(description));
        }
    }

    // GLFW is initialized once for the process, as OpenTK's toolkit is.
    void EnsureGlfw()
    {
        static const bool ready = []()
        {
            ::glfwSetErrorCallback(&ForwardGlfwError);
            if (::glfwInit() == GLFW_FALSE)
            {
                const char* description = nullptr;
                const int code = ::glfwGetError(&description);
                throw GLFWException(
                    description == nullptr ? std::string("GLFW could not start.")
                                           : std::string(description),
                    static_cast<std::int32_t>(code));
            }
            return true;
        }();
        (void)ready;
    }

    // CurrentMonitor: the one whose work area the window's origin is inside.
    [[nodiscard]] GLFWmonitor* MonitorForWindow(GLFWwindow* handle)
    {
        GLFWmonitor* monitor = ::glfwGetPrimaryMonitor();
        if (handle == nullptr)
        {
            return monitor;
        }
        int windowX = 0;
        int windowY = 0;
        ::glfwGetWindowPos(handle, &windowX, &windowY);
        int count = 0;
        GLFWmonitor** const monitors = ::glfwGetMonitors(&count);
        for (int i = 0; i < count; ++i)
        {
            int areaX = 0;
            int areaY = 0;
            int areaWidth = 0;
            int areaHeight = 0;
            ::glfwGetMonitorWorkarea(monitors[i], &areaX, &areaY, &areaWidth, &areaHeight);
            if (windowX >= areaX && windowX < areaX + areaWidth
                && windowY >= areaY && windowY < areaY + areaHeight)
            {
                return monitors[i];
            }
        }
        return monitor;
    }

    class GlfwWindow final : public MphRead::RendererPlatform::Window
    {
    public:
        explicit GlfwWindow(const WindowSettings& settings)
        {
            EnsureGlfw();
            ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, settings.ApiMajor);
            ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, settings.ApiMinor);
            // ContextProfile.Compatability, and it has to be asked for by
            // name: the renderer is written in immediate mode, which a core
            // profile does not have, and a driver handed ANY_PROFILE with a
            // version of 3.2 or above gives a core one -- every frame then
            // comes out black with nothing in any log to say why.
            ::glfwWindowHint(GLFW_OPENGL_PROFILE,
                settings.Profile == WindowSettings::ContextProfile::Compatability
                    ? GLFW_OPENGL_COMPAT_PROFILE
                    : GLFW_OPENGL_ANY_PROFILE);
            ::glfwWindowHint(GLFW_VISIBLE, settings.StartVisible ? GLFW_TRUE : GLFW_FALSE);
            _handle = ::glfwCreateWindow(
                settings.ClientSize.X, settings.ClientSize.Y,
                settings.Title.c_str(), nullptr, nullptr);
            if (_handle == nullptr)
            {
                const char* description = nullptr;
                const int code = ::glfwGetError(&description);
                throw GLFWException(
                    description == nullptr ? std::string("The window could not be created.")
                                           : std::string(description),
                    static_cast<std::int32_t>(code));
            }
            _updateFrequency = settings.UpdateFrequency;
            ::glfwMakeContextCurrent(_handle);
            ::glfwSwapInterval(1);
            ::glfwSetWindowUserPointer(_handle, this);
            ::glfwSetFramebufferSizeCallback(_handle, &OnFramebufferSize);
            ::glfwSetKeyCallback(_handle, &OnKey);
            ::glfwSetCharCallback(_handle, &OnChar);
            ::glfwSetMouseButtonCallback(_handle, &OnMouseButton);
            ::glfwSetCursorPosCallback(_handle, &OnCursorPos);
            ::glfwSetScrollCallback(_handle, &OnScroll);
        }

        ~GlfwWindow() override
        {
            if (_handle != nullptr)
            {
                ::glfwDestroyWindow(_handle);
                _handle = nullptr;
            }
        }

        [[nodiscard]] GLFWwindow* Handle() const noexcept { return _handle; }

        void Run(WindowEvents& events) override
        {
            _events = &events;
            ::glfwMakeContextCurrent(_handle);
            events.OnLoad();
            // The size the window actually got, which OnLoad's callers read.
            ResizeEventArgs resize;
            resize.Size = Size();
            events.OnResize(resize);

            auto previous = std::chrono::steady_clock::now();
            while (::glfwWindowShouldClose(_handle) == GLFW_FALSE)
            {
                ::glfwPollEvents();
                const auto now = std::chrono::steady_clock::now();
                const double elapsed
                    = std::chrono::duration<double>(now - previous).count();
                // UpdateFrequency caps how often the loop runs; zero is
                // OpenTK's "as fast as the frames arrive".
                if (_updateFrequency > 0.0 && elapsed < 1.0 / _updateFrequency)
                {
                    std::this_thread::sleep_for(std::chrono::duration<double>(
                        1.0 / _updateFrequency - elapsed));
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

        [[nodiscard]] OpenTK::Mathematics::Vector2i Size() const override
        {
            int width = 0;
            int height = 0;
            ::glfwGetFramebufferSize(_handle, &width, &height);
            return OpenTK::Mathematics::Vector2i(width, height);
        }

        [[nodiscard]] MphRead::RendererPlatform::KeyboardState& Keyboard() override
        {
            return _keyboard;
        }

        [[nodiscard]] MphRead::RendererPlatform::MouseState& Mouse() override
        {
            return _mouse;
        }

        void Title(std::string value) override
        {
            ::glfwSetWindowTitle(_handle, value.c_str());
        }

        void MinimumSize(OpenTK::Mathematics::Vector2i value) override
        {
            ::glfwSetWindowSizeLimits(
                _handle, value.X, value.Y, GLFW_DONT_CARE, GLFW_DONT_CARE);
        }

        void Cursor(CursorState value) override
        {
            ::glfwSetInputMode(_handle, GLFW_CURSOR,
                value == CursorState::Grabbed ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }

        void VSync(VSyncMode value) override
        {
            ::glfwSwapInterval(value == VSyncMode::On ? 1 : 0);
        }

        void UpdateFrequency(double value) override
        {
            _updateFrequency = value;
        }

        void Visible(bool value) override
        {
            if (value)
            {
                ::glfwShowWindow(_handle);
            }
            else
            {
                ::glfwHideWindow(_handle);
            }
        }

        void Close() override
        {
            ::glfwSetWindowShouldClose(_handle, GLFW_TRUE);
        }

        void SwapBuffers() override
        {
            ::glfwSwapBuffers(_handle);
        }

        // GameWindow's own handlers raise events nothing here subscribes to.
        void BaseOnClosing() override {}
        void BaseOnLoad() override {}
        void BaseOnRenderFrame(const FrameEventArgs& args) override { (void)args; }
        void BaseOnResize(const ResizeEventArgs& e) override { (void)e; }
        void BaseOnMouseDown(const MouseButtonEventArgs& e) override { (void)e; }
        void BaseOnMouseUp(const MouseButtonEventArgs& e) override { (void)e; }
        void BaseOnMouseMove(const MouseMoveEventArgs& e) override { (void)e; }
        void BaseOnMouseWheel(const MouseWheelEventArgs& e) override { (void)e; }
        void BaseOnTextInput(const TextInputEventArgs& e) override { (void)e; }
        void BaseOnKeyDown(const KeyboardKeyEventArgs& e) override { (void)e; }

        std::int32_t WindowBorder() const override
        {
            if (::glfwGetWindowAttrib(_handle, GLFW_DECORATED) == GLFW_FALSE)
            {
                return static_cast<std::int32_t>(
                    MphRead::RendererPlatform::WindowBorderValue::Hidden);
            }
            if (::glfwGetWindowAttrib(_handle, GLFW_RESIZABLE) == GLFW_FALSE)
            {
                return static_cast<std::int32_t>(
                    MphRead::RendererPlatform::WindowBorderValue::Fixed);
            }
            return static_cast<std::int32_t>(
                MphRead::RendererPlatform::WindowBorderValue::Resizable);
        }

        void WindowBorder(std::int32_t value) override
        {
            const auto border
                = static_cast<MphRead::RendererPlatform::WindowBorderValue>(value);
            ::glfwSetWindowAttrib(_handle, GLFW_DECORATED,
                border == MphRead::RendererPlatform::WindowBorderValue::Hidden
                    ? GLFW_FALSE : GLFW_TRUE);
            ::glfwSetWindowAttrib(_handle, GLFW_RESIZABLE,
                border == MphRead::RendererPlatform::WindowBorderValue::Resizable
                    ? GLFW_TRUE : GLFW_FALSE);
        }

        OpenTK::Mathematics::Vector2i Location() const override
        {
            int x = 0;
            int y = 0;
            ::glfwGetWindowPos(_handle, &x, &y);
            return OpenTK::Mathematics::Vector2i(x, y);
        }

        void Location(OpenTK::Mathematics::Vector2i value) override
        {
            ::glfwSetWindowPos(_handle, value.X, value.Y);
        }

        OpenTK::Mathematics::Vector2i ClientSize() const override
        {
            int width = 0;
            int height = 0;
            ::glfwGetWindowSize(_handle, &width, &height);
            return OpenTK::Mathematics::Vector2i(width, height);
        }

        void ClientSize(OpenTK::Mathematics::Vector2i value) override
        {
            ::glfwSetWindowSize(_handle, value.X, value.Y);
        }

        MphRead::RendererPlatform::MonitorArea CurrentMonitorClientArea() const override
        {
            MphRead::RendererPlatform::MonitorArea area;
            GLFWmonitor* monitor = MonitorForWindow(_handle);
            if (monitor == nullptr)
            {
                return area;
            }
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            ::glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
            area.Min = OpenTK::Mathematics::Vector2i(x, y);
            area.Size = OpenTK::Mathematics::Vector2i(width, height);
            return area;
        }

        void WindowStateNormal() override
        {
            ::glfwRestoreWindow(_handle);
        }

        void Floating(bool value) override
        {
            ::glfwSetWindowAttrib(_handle, GLFW_FLOATING, value ? GLFW_TRUE : GLFW_FALSE);
        }

        bool IsFocused() const override
        {
            return ::glfwGetWindowAttrib(_handle, GLFW_FOCUSED) != GLFW_FALSE;
        }

        OpenTK::Mathematics::Vector2i ClientLocation() const override
        {
            // The client area's own origin, which is the window position plus
            // the frame GLFW reports around it.
            int x = 0;
            int y = 0;
            ::glfwGetWindowPos(_handle, &x, &y);
            return OpenTK::Mathematics::Vector2i(x, y);
        }

        void Focus() override
        {
            ::glfwFocusWindow(_handle);
        }

    private:
        [[nodiscard]] static GlfwWindow* From(GLFWwindow* handle)
        {
            return static_cast<GlfwWindow*>(::glfwGetWindowUserPointer(handle));
        }

        static void OnFramebufferSize(GLFWwindow* handle, int width, int height)
        {
            GlfwWindow* const self = From(handle);
            if (self == nullptr || self->_events == nullptr)
            {
                return;
            }
            ResizeEventArgs args;
            args.Size = OpenTK::Mathematics::Vector2i(width, height);
            self->_events->OnResize(args);
        }

        static void OnKey(GLFWwindow* handle, int key, int scancode, int action, int mods)
        {
            (void)scancode;
            GlfwWindow* const self = From(handle);
            if (self == nullptr || key < 0)
            {
                return;
            }
            const bool down = action != GLFW_RELEASE;
            self->_keyboard.SetKeyDown(
                static_cast<MphRead::RendererPlatform::Key>(key), down);
            if (action != GLFW_PRESS || self->_events == nullptr)
            {
                return;
            }
            KeyboardKeyEventArgs args;
            args.Key = static_cast<MphRead::RendererPlatform::Key>(key);
            args.Shift = (mods & GLFW_MOD_SHIFT) != 0;
            args.Control = (mods & GLFW_MOD_CONTROL) != 0;
            args.Alt = (mods & GLFW_MOD_ALT) != 0;
            args.Command = (mods & GLFW_MOD_SUPER) != 0;
            self->_events->OnKeyDown(args);
        }

        static void OnChar(GLFWwindow* handle, unsigned int codepoint)
        {
            GlfwWindow* const self = From(handle);
            if (self == nullptr || self->_events == nullptr)
            {
                return;
            }
            TextInputEventArgs args;
            args.Unicode = static_cast<std::uint32_t>(codepoint);
            self->_events->OnTextInput(args);
        }

        static void OnMouseButton(GLFWwindow* handle, int button, int action, int mods)
        {
            (void)mods;
            GlfwWindow* const self = From(handle);
            if (self == nullptr || button < 0)
            {
                return;
            }
            const auto value = static_cast<MphRead::RendererPlatform::MouseButton>(button);
            self->_mouse.SetButtonDown(value, action != GLFW_RELEASE);
            if (self->_events == nullptr)
            {
                return;
            }
            MouseButtonEventArgs args;
            args.Button = value;
            if (action == GLFW_PRESS)
            {
                self->_events->OnMouseDown(args);
            }
            else if (action == GLFW_RELEASE)
            {
                self->_events->OnMouseUp(args);
            }
        }

        static void OnCursorPos(GLFWwindow* handle, double x, double y)
        {
            GlfwWindow* const self = From(handle);
            if (self == nullptr)
            {
                return;
            }
            const float newX = static_cast<float>(x);
            const float newY = static_cast<float>(y);
            // MouseMoveEventArgs carries the delta, which OpenTK computes from
            // the previous position.
            MouseMoveEventArgs args;
            args.DeltaX = newX - self->_mouse.X;
            args.DeltaY = newY - self->_mouse.Y;
            self->_mouse.X = newX;
            self->_mouse.Y = newY;
            if (self->_events != nullptr)
            {
                self->_events->OnMouseMove(args);
            }
        }

        static void OnScroll(GLFWwindow* handle, double offsetX, double offsetY)
        {
            GlfwWindow* const self = From(handle);
            if (self == nullptr)
            {
                return;
            }
            self->_mouse.Scroll.X += static_cast<float>(offsetX);
            self->_mouse.Scroll.Y += static_cast<float>(offsetY);
            if (self->_events == nullptr)
            {
                return;
            }
            MouseWheelEventArgs args;
            args.OffsetY = static_cast<float>(offsetY);
            self->_events->OnMouseWheel(args);
        }

        GLFWwindow* _handle = nullptr;
        WindowEvents* _events = nullptr;
        double _updateFrequency = 0.0;
        MphRead::RendererPlatform::KeyboardState _keyboard{};
        MphRead::RendererPlatform::MouseState _mouse{};
    };
}

namespace MphRead::RendererPlatform
{
    std::shared_ptr<Window> CreateWindow(const WindowSettings& settings)
    {
        return std::make_shared<GlfwWindow>(settings);
    }

    bool IsLinux()
    {
#if defined(__linux__)
        return true;
#else
        return false;
#endif
    }

    std::optional<std::string> EnvironmentVariable(std::string_view name)
    {
        return NativeRuntime::EnvironmentGetVariable(std::string(name));
    }

    OpenTK::Mathematics::Vector2i WorkAreaForWindow(Window& window)
    {
        EnsureGlfw();
        const MonitorArea area = window.CurrentMonitorClientArea();
        return area.Size;
    }

    void ProcessEvents()
    {
        EnsureGlfw();
        ::glfwPollEvents();
    }

    void InstallGlfwErrorCallback(std::function<void(std::int32_t, std::string)> callback)
    {
        ErrorCallback() = std::move(callback);
        EnsureGlfw();
    }

    std::int32_t GlfwFeatureUnavailableCode()
    {
        return static_cast<std::int32_t>(GLFW_FEATURE_UNAVAILABLE);
    }

    void ConsoleClear()
    {
        // Console.Clear: the terminal's own erase-and-home, which is what the
        // runtime writes on every platform that has one.
        NativeRuntime::ConsoleWrite("\x1B[2J\x1B[H");
    }

    void ConsoleWrite(std::string_view text)
    {
        NativeRuntime::ConsoleWrite(text);
    }

    void ConsoleWriteLine(std::string_view text)
    {
        NativeRuntime::ConsoleWriteLine(text);
    }

    std::optional<std::string> ConsoleReadLine()
    {
        // Console.ReadLine returns null at end of input.
        std::string line;
        if (!std::getline(std::cin, line))
        {
            return std::nullopt;
        }
        // The runtime strips the terminator, including a CR left by CRLF.
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        {
            line.pop_back();
        }
        return line;
    }
}
