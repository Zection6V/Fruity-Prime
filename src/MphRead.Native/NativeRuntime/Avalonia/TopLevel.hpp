#pragma once

// A top level with no window behind it: Avalonia's EmbeddableControlRoot over
// the launcher's own ITopLevelImpl. It lays its content out at the size it is
// given, draws it with the Skia canvas into a buffer of premultiplied RGBA
// that it keeps across frames, and takes its input as the raw events a
// windowing system would have delivered.

#include "Panels.hpp"
#include "../Skia/Skia.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::NativeRuntime::Avalonia
{
    class TopLevel;

    namespace Input
    {
        class FocusManager final
        {
        public:
            explicit FocusManager(TopLevel& owner)
                : _owner(owner)
            {
            }
            [[nodiscard]] IInputElement* GetFocusedElement() const;
            void ClearFocus();

        private:
            TopLevel& _owner;
        };
    }

    class TopLevel : public Controls::ContentControl
    {
    public:
        TopLevel();
        ~TopLevel() override;

        // TopLevel.GetTopLevel(visual).
        [[nodiscard]] static TopLevel* GetTopLevel(const Visual* visual);

        [[nodiscard]] Size ClientSize() const noexcept { return _clientSize; }
        void SetClientSize(Size size);
        [[nodiscard]] double RenderScaling() const noexcept { return 1.0; }

        // Window.Prepare: the first layout pass. StartRendering: draw from
        // now on.
        void Prepare();
        void StartRendering();

        void InvalidateRender() noexcept { _renderDirty = true; }
        void InvalidateLayout() noexcept { _layoutDirty = true; }
        [[nodiscard]] bool NeedsRender() const noexcept { return _renderDirty || _layoutDirty; }
        void ExecuteLayoutPass();

        // Lay out what changed and draw it if anything did. True when a pass
        // ran into Pixels.
        bool Render();

        // The frame: tightly packed RGBA, top row first, premultiplied.
        [[nodiscard]] const std::uint8_t* Pixels() const noexcept { return _pixels.Pixels(); }
        [[nodiscard]] std::int32_t PixelWidth() const noexcept { return _pixels.Width(); }
        [[nodiscard]] std::int32_t PixelHeight() const noexcept { return _pixels.Height(); }
        [[nodiscard]] std::int32_t Drawn() const noexcept { return _drawn; }
        // Called when a pass has just finished into Pixels.
        std::function<void()> Painted{};

        // ---- input, as the windowing system would deliver it
        void MouseMove(Point point, Input::RawInputModifiers modifiers);
        void MouseDown(Point point, Input::MouseButton button, Input::RawInputModifiers modifiers);
        void MouseUp(Point point, Input::MouseButton button, Input::RawInputModifiers modifiers);
        void MouseWheel(Point point, Vector delta, Input::RawInputModifiers modifiers);
        void TouchBegin(Point point, std::int64_t id);
        void TouchUpdate(Point point, std::int64_t id);
        void TouchEnd(Point point, std::int64_t id);
        void KeyPress(Input::Key key, Input::RawInputModifiers modifiers, std::optional<std::string> keySymbol = std::nullopt);
        void KeyRelease(Input::Key key, Input::RawInputModifiers modifiers, std::optional<std::string> keySymbol = std::nullopt);
        void TextInput(const std::string& text);

        // ---- focus
        [[nodiscard]] Input::InputElement* FocusedElement() const noexcept { return _focused; }
        void SetFocusedElement(Input::InputElement* element, Input::NavigationMethod method,
            Input::KeyModifiers modifiers);
        [[nodiscard]] Input::FocusManager* FocusManager() noexcept { return &_focusManager; }

        // The cursor the pointer is over, for the host to show.
        [[nodiscard]] Input::StandardCursorType CurrentCursor() const noexcept { return _cursor; }
        [[nodiscard]] Input::IPointer& Mouse() noexcept { return *_mouse; }

        // The deepest input element under a point, as InputHitTest finds it.
        [[nodiscard]] Input::InputElement* InputHitTest(Point point) const;

        // Called by the tree.
        void ElementStateChanged(Input::InputElement& element);
        void ElementDetached(Input::InputElement& element);

        // Draw a subtree into a canvas, as RenderTargetBitmap.Render does.
        static void RenderVisual(Visual& visual, Media::DrawingContext& context, bool isRoot);

        std::any TransparencyLevelHint{};
        std::any RequestedThemeVariant{};

    private:
        void UpdatePointerOver(Input::IPointer& pointer, Point point, Input::RawInputModifiers modifiers);
        [[nodiscard]] Input::InputElement* PointerTarget(Input::IPointer& pointer, Point point) const;
        void RaisePointer(Input::IPointer& pointer, const Interactivity::RoutedEvent& routedEvent, Point point,
            Input::RawInputModifiers modifiers, Input::PointerUpdateKind kind);
        void RaiseKey(const Interactivity::RoutedEvent& routedEvent, Input::Key key, Input::RawInputModifiers modifiers,
            std::optional<std::string> keySymbol);
        [[nodiscard]] std::uint64_t Timestamp() const;

        Size _clientSize{1280, 768};
        Skia::Bitmap _pixels;
        std::int32_t _drawn = 0;
        bool _renderDirty = true;
        bool _layoutDirty = true;
        bool _rendering = false;
        Input::InputElement* _focused = nullptr;
        Input::FocusManager _focusManager{*this};
        std::unique_ptr<Input::IPointer> _mouse;
        std::map<std::int64_t, std::unique_ptr<Input::IPointer>> _touches;
        // The chain the pointer is over, innermost first.
        std::vector<Input::InputElement*> _pointerOver;
        Input::StandardCursorType _cursor = Input::StandardCursorType::Arrow;
        // Double clicks: when and where the last press landed.
        std::chrono::steady_clock::time_point _lastPress{};
        Point _lastPressPoint{};
        std::int32_t _clickCount = 0;
        std::chrono::steady_clock::time_point _start = std::chrono::steady_clock::now();
    };

    // Avalonia.Controls.Embedding.EmbeddableControlRoot.
    class EmbeddableControlRoot : public TopLevel
    {
    };

    namespace Media::Imaging
    {
        // RenderTargetBitmap: a visual drawn into a bitmap of its own.
        class RenderTargetBitmap final : public Bitmap
        {
        public:
            RenderTargetBitmap(Avalonia::PixelSize size, Vector dpi = {96, 96});
            void Render(Visual& visual);
        };
    }
}
