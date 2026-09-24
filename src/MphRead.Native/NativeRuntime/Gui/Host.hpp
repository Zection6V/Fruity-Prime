#pragma once

// The window and the UI thread beneath the launcher's screens: what Avalonia
// is on the managed side. A window owns an element tree, a GL (or other)
// surface and nothing else; the dispatcher owns the queue of work posted to
// the thread that draws them, the nested frames a modal wait is made of, and
// the timers.
//
// Nothing here knows what a launcher is. The launcher's adapter interfaces are
// bound to this in Mods/Launcher/Gui's host files.

#include "Element.hpp"
#include "Renderer.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct GLFWwindow;

namespace MphRead::NativeRuntime::Gui
{
    class Window;

    // Work queued for the thread that draws. Background work runs after
    // everything else, which is what Avalonia's priority does for the one
    // place the launcher uses it.
    enum class Priority : std::uint8_t
    {
        Normal,
        Background
    };

    // A repeating tick, serviced between frames.
    class Timer final
    {
    public:
        void IntervalSeconds(double seconds) noexcept;
        void Tick(std::function<void()> action);
        void Start();
        void Stop() noexcept;
        [[nodiscard]] bool Running() const noexcept { return _running; }

        // The dispatcher calls this; it fires when enough time has passed.
        void Service(double now);

    private:
        std::function<void()> _tick;
        double _interval = 1.0;
        double _next = 0.0;
        bool _running = false;
    };

    class Dispatcher final
    {
    public:
        [[nodiscard]] static Dispatcher& Instance();

        // Safe to call from any thread: background work settles its task by
        // posting here, and the loop picks it up on the next pass.
        void Post(std::function<void()> action, Priority priority = Priority::Normal);

        // Runs the loop until the predicate says to stop, or until every
        // window has closed. A nested call is a nested frame, which is how a
        // modal wait is written without a thread of its own.
        void PushFrame(const std::function<bool()>& shouldContinue);

        // One pass: events, queued work, timers, and a picture per window.
        void PumpOnce();

        [[nodiscard]] std::shared_ptr<Timer> CreateTimer();

        void Register(Window* window);
        void Unregister(Window* window);
        [[nodiscard]] bool AnyWindowOpen() const noexcept;
        // The window whose content is this root, or null.
        [[nodiscard]] Window* WindowOf(const Element* root) const;

    private:
        [[nodiscard]] bool Idle() const;
        void DrainQueue();
        void ServiceTimers();

        mutable std::mutex _lock;
        std::deque<std::function<void()>> _normal;
        std::deque<std::function<void()>> _background;
        std::vector<std::shared_ptr<Timer>> _timers;
        std::vector<Window*> _windows;
    };

    // What a window has to be told before it exists, because the platform
    // fixes these when the surface is created.
    struct WindowOptions final
    {
        bool Decorated = true;
        // A surface whose alpha reaches the desktop: the pause menu is an
        // overlay with the match showing through it.
        bool Transparent = false;
        bool Topmost = false;
    };

    class Window final
    {
    public:
        explicit Window(WindowOptions options = {});
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        // True once a surface exists. False means no toolkit here, which is
        // what sends the launcher to its text screen.
        [[nodiscard]] static bool Available();

        void Title(std::string_view value);
        // The picture the desktop shows for this window, as RGBA pixels.
        void Icon(std::int32_t width, std::int32_t height,
            const std::uint8_t* rgba);
        void ClientSize(double width, double height);
        void MinimumSize(double width, double height);
        void CenterOnScreen();
        // Where the window sits, in screen pixels.
        void Position(std::int32_t x, std::int32_t y);
        // A window with no frame, and one that stays over the others: what an
        // overlay such as the pause menu is.
        void Decorated(bool value);
        void Topmost(bool value);
        void Background(Color value) noexcept { _background = value; }
        void Content(ElementPtr content) { _content = std::move(content); }
        [[nodiscard]] const ElementPtr& Content() const noexcept { return _content; }

        void Show();
        void Close();
        [[nodiscard]] bool Open() const noexcept { return _open; }

        void Closed(std::function<void()> handler);
        // Called with the client width whenever it changes, and once at first
        // layout, which is what a SizeChanged subscriber is told.
        void SizeChanged(std::function<void(double)> handler);
        // A key the window did not consume itself. Escape is the only one the
        // launcher reads.
        void KeyDown(std::function<void(std::int32_t, bool&)> handler);
        void TextInput(std::function<void(char32_t)> handler);

        // One event pass and one picture.
        void Service();

        // The content, laid out at this size and drawn into a buffer of its
        // own rather than onto the screen: what a RenderTargetBitmap is. The
        // window needs no be shown, because nothing is read back from the
        // surface -- only from the framebuffer object this makes.
        [[nodiscard]] bool Capture(std::int32_t width, std::int32_t height,
            std::vector<std::uint8_t>& rgba);

        // The element under the pointer, for the cursor and for hit testing.
        [[nodiscard]] Element* Hovered() const noexcept { return _hovered; }
        // Whatever last took focus by click or by Focus(), with the got/lost
        // notifications Avalonia raises.
        void Focus(Element* element);
        [[nodiscard]] Element* Focused() const noexcept { return _focused; }

        // Avalonia's pointer capture: while an element holds it, every
        // pointer event goes to it wherever the pointer is.
        void Capture(Element* element) noexcept;
        [[nodiscard]] Element* Captured() const noexcept { return _captured; }

        // The window this element is in, or null for one not in a tree.
        [[nodiscard]] static Window* Of(const Element& element);

        [[nodiscard]] double Width() const noexcept { return _width; }
        [[nodiscard]] double Height() const noexcept { return _height; }

    private:
        void Layout();
        void Draw();
        void Press(double x, double y);
        void Release(double x, double y);
        void Move(double x, double y);
        void ApplyCursor();
        void RaiseKey(KeyEvent& e);
        void RaiseText(char32_t code);

        static void OnKey(GLFWwindow* handle, int key, int scancode, int action, int mods);
        static void OnChar(GLFWwindow* handle, unsigned int codepoint);
        static void OnMouseButton(GLFWwindow* handle, int button, int action, int mods);
        static void OnCursorPos(GLFWwindow* handle, double x, double y);
        static void OnScroll(GLFWwindow* handle, double x, double y);
        static void OnClose(GLFWwindow* handle);

        GLFWwindow* _handle = nullptr;
        ElementPtr _content;
        Renderer _renderer;
        Color _background = Color::FromArgb(0xFF101014);
        std::vector<std::function<void()>> _closed;
        std::vector<std::function<void(double)>> _sizeChanged;
        std::function<void(std::int32_t, bool&)> _keyDown;
        std::function<void(char32_t)> _textInput;
        Element* _hovered = nullptr;
        Element* _focused = nullptr;
        Element* _pressed = nullptr;
        Element* _captured = nullptr;
        double _pointerX = 0.0;
        double _pointerY = 0.0;
        double _width = 0.0;
        double _height = 0.0;
        double _reportedWidth = -1.0;
        bool _open = false;
        bool _shown = false;
        bool _transparent = false;
        Cursor _cursor = Cursor::Arrow;
    };
}
