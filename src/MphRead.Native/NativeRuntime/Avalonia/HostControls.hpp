#pragma once

// The Avalonia Control each of the launcher's own drawn controls sits in: the
// measure and render hooks, the pointer, focus and key events, the cursor and
// the height. The controls themselves are ported already; nothing here decides
// what they look like.
//
// Each host is both the control adapter its ported control was written
// against and the toolkit element that control is. The element owns the host
// (as its Behaviour and its Tag) and the host points back at the element, so
// there is one owner and no cycle.

#include "HostDrawing.hpp"

#include "../Gui/Element.hpp"
#include "../../Mods/Launcher/Gui/MenuEntry.hpp"
#include "../../Mods/Launcher/Gui/ProgressRow.hpp"
#include "../../Mods/Launcher/Gui/SplashView.hpp"
#include "../../Mods/Launcher/Gui/UpdateBadge.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::NativeRuntime::Avalonia
{
    // The element a host control is, with the host reachable from it.
    class HostControl : public Toolkit::CustomBehaviour
    {
    public:
        ~HostControl() override = default;

        [[nodiscard]] Toolkit::Element& Visual() const noexcept { return *_visual; }

        // Binds a freshly created Custom element to this host and returns it.
        [[nodiscard]] static Toolkit::ElementPtr Bind(
            const std::shared_ptr<HostControl>& host);

    protected:
        // Avalonia's Control.MeasureOverride for a leaf: nothing of its own.
        [[nodiscard]] Launcher::GuiRect ControlBounds() const noexcept;
        void Invalidate() const noexcept {}
        void SetFocusHere();
        void CaptureHere();
        void ReleaseCapture();

        Toolkit::Element* _visual = nullptr;
    };

    // Reaches the host a launcher element carries.
    template <typename T>
    [[nodiscard]] T* HostOf(const Toolkit::ElementPtr& element) noexcept
    {
        return element == nullptr ? nullptr : static_cast<T*>(element->Tag.get());
    }

    class MenuEntryHost final : public HostControl,
                                public Launcher::MenuEntryControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            std::optional<std::u16string> title,
            std::optional<std::u16string> subtitle, double titleSize);
        // A control with no entry yet, for a caller that constructs the entry
        // itself and then hands it back with Attach.
        [[nodiscard]] static Toolkit::ElementPtr CreateBare();
        void Attach(Launcher::MenuEntry& entry);

        [[nodiscard]] Launcher::MenuEntry& Entry() const noexcept { return *_entry; }
        void Click(std::function<void()> action);

        // CustomBehaviour
        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        // MenuEntryControlAdapter
        [[nodiscard]] Launcher::MenuEntrySize BaseMeasureOverride(
            Launcher::MenuEntrySize availableSize) override;
        void BaseOnPropertyChanged(Launcher::MenuEntryProperty property) override;
        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetFocusable(bool focusable) override;
        void SetHandCursor() override;
        void Focus() override;
        [[nodiscard]] bool GetIsEnabled() const override;
        void SetIsEnabled(bool isEnabled) override;
        [[nodiscard]] bool IsPointerOver() const override;
        [[nodiscard]] bool IsFocused() const override;
        [[nodiscard]] Launcher::GuiRect Bounds() const override;
        [[nodiscard]] std::u16string ToUpperInvariant(
            std::u16string_view text) override;
        [[nodiscard]] Launcher::TrackedTextFormattedText CreateFormattedText(
            std::u16string_view text, Launcher::TrackedTextCulture culture,
            Launcher::TrackedTextFlowDirection flowDirection,
            Launcher::TrackedTextFace face, double fontSize,
            Launcher::TrackedTextBrush brush) override;
        [[nodiscard]] void* Captured(void* pointer) const override;
        void Capture(void* pointer, void* control) override;
        [[nodiscard]] Launcher::MenuEntryPoint GetPosition(
            const Launcher::MenuEntryPointerEventArgs& e) const override;
        void InvalidateVisual() override;
        void BaseOnPointerEntered(Launcher::MenuEntryPointerEventArgs& e) override;
        void BaseOnPointerExited(Launcher::MenuEntryPointerEventArgs& e) override;
        void BaseOnPointerPressed(Launcher::MenuEntryPointerEventArgs& e) override;
        void BaseOnPointerReleased(Launcher::MenuEntryPointerEventArgs& e) override;
        void BaseOnPointerCaptureLost(
            Launcher::MenuEntryPointerCaptureLostEventArgs& e) override;
        void BaseOnGotFocus(Launcher::MenuEntryGotFocusEventArgs& e) override;
        void BaseOnLostFocus(Launcher::MenuEntryRoutedEventArgs& e) override;
        void BaseOnKeyDown(Launcher::MenuEntryKeyEventArgs& e) override;

    private:
        static void Wire(const Toolkit::ElementPtr& element, MenuEntryHost* host);

        // The entry this control is. It is owned here when the host made it
        // and borrowed when a view made its own over this adapter.
        std::unique_ptr<Launcher::MenuEntry> _owned;
        Launcher::MenuEntry* _entry = nullptr;
        std::function<void()> _click;
        void* _captured = nullptr;
    };

    class UpdateBadgeHost final : public HostControl,
                                  public Launcher::UpdateBadgeControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create();

        [[nodiscard]] Launcher::UpdateBadge& Badge() const noexcept { return *_badge; }
        void Click(std::function<void()> action);

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        void SetFocusable(bool focusable) override;
        void SetHandCursor() override;
        void Focus() override;
        [[nodiscard]] bool GetIsVisible() const override;
        void SetIsVisible(bool isVisible) override;
        [[nodiscard]] bool IsPointerOver() const override;
        [[nodiscard]] bool IsFocused() const override;
        [[nodiscard]] Launcher::GuiRect Bounds() const override;
        [[nodiscard]] Launcher::UpdateBadgeSize DesiredSize() const override;
        [[nodiscard]] std::u16string ToUpperInvariant(
            std::u16string_view text) override;
        [[nodiscard]] Launcher::TrackedTextFormattedText CreateFormattedText(
            std::u16string_view text, Launcher::TrackedTextCulture culture,
            Launcher::TrackedTextFlowDirection flowDirection,
            Launcher::TrackedTextFace face, double fontSize,
            Launcher::TrackedTextBrush brush) override;
        void InvalidateMeasure() override;
        void InvalidateVisual() override;
        void BaseOnPointerEntered(Launcher::UpdateBadgePointerEventArgs& e) override;
        void BaseOnPointerExited(Launcher::UpdateBadgePointerEventArgs& e) override;
        void BaseOnPointerPressed(
            Launcher::UpdateBadgePointerPressedEventArgs& e) override;
        void BaseOnPointerReleased(
            Launcher::UpdateBadgePointerReleasedEventArgs& e) override;
        void BaseOnGotFocus(Launcher::UpdateBadgeGotFocusEventArgs& e) override;
        void BaseOnLostFocus(Launcher::UpdateBadgeRoutedEventArgs& e) override;
        void BaseOnKeyDown(Launcher::UpdateBadgeKeyEventArgs& e) override;

    private:
        std::unique_ptr<Launcher::UpdateBadge> _badge;
        std::function<void()> _click;
        Toolkit::Size _desired{};
    };

    class ProgressRowHost final : public HostControl,
                                  public Launcher::ProgressRowControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create();

        [[nodiscard]] Launcher::ProgressRow& Row() const noexcept { return *_row; }

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        void SetHeight(double height) override;
        void SetIsVisible(bool isVisible) override;
        void InvalidateVisual() override;

    private:
        std::unique_ptr<Launcher::ProgressRow> _row;
    };

    class SplashViewHost final : public HostControl,
                                 public Launcher::SplashViewControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create();

        [[nodiscard]] Launcher::SplashView& Splash() const noexcept { return *_splash; }

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        [[nodiscard]] Launcher::GuiRect Bounds() const override;
        void InvalidateVisual() override;
        [[nodiscard]] std::string AppContextBaseDirectory() const override;
        [[nodiscard]] std::string PathCombine(
            std::string_view left, std::string_view right) const override;
        [[nodiscard]] std::shared_ptr<Launcher::SplashBitmap> CreateBitmapFromMemory(
            std::span<const std::uint8_t> bytes) override;
        [[nodiscard]] std::shared_ptr<Launcher::SplashBitmap> CreateBitmapFromAsset(
            std::string_view uri) override;
        void DisposeBitmap(Launcher::SplashBitmap& bitmap) override;
        [[nodiscard]] std::u16string ToUpperInvariant(
            std::u16string_view text) const override;

    private:
        std::unique_ptr<Launcher::SplashView> _splash;
    };
}
