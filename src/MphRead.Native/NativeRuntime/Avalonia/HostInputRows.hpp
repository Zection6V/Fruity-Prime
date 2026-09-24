#pragma once

// The three settings rows that read a device rather than a value: the slider,
// the keyboard binding row and the gamepad binding row.

#include "HostControls.hpp"

#include "../../Mods/Launcher/Gui/KeyRow.hpp"
#include "../../Mods/Launcher/Gui/PadRow.hpp"
#include "../../Mods/Launcher/Gui/SliderRow.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::NativeRuntime::Avalonia
{
    class SliderRowHost final : public HostControl,
                                public Launcher::SliderRowControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            std::optional<std::u16string> label, std::int32_t value,
            Launcher::SliderRow::FormatHandler format, double labelWidth,
            std::int32_t min, std::int32_t max, std::int32_t keyStep);

        [[nodiscard]] Launcher::SliderRow& Row() const noexcept { return *_row; }

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetFocusable(bool focusable) override;
        void SetHandCursor() override;
        void Focus() override;
        [[nodiscard]] bool GetIsEnabled() const override;
        void SetIsEnabled(bool isEnabled) override;
        [[nodiscard]] bool IsFocused() const override;
        [[nodiscard]] Launcher::GuiRect Bounds() const override;
        [[nodiscard]] std::u16string ToUpperInvariant(
            std::u16string_view text) override;
        [[nodiscard]] Launcher::SliderRowPoint GetPosition(
            const Launcher::SliderRowPointerEventArgs& e) const override;
        void Capture(void* pointer, void* control) override;
        void InvalidateVisual() override;
        void BaseOnPointerPressed(Launcher::SliderRowPointerEventArgs& e) override;
        void BaseOnPointerMoved(Launcher::SliderRowPointerEventArgs& e) override;
        void BaseOnPointerReleased(Launcher::SliderRowPointerEventArgs& e) override;
        void BaseOnPointerExited(Launcher::SliderRowPointerEventArgs& e) override;
        void BaseOnKeyDown(Launcher::SliderRowKeyEventArgs& e) override;
        void BaseOnGotFocus(Launcher::SliderRowGotFocusEventArgs& e) override;
        void BaseOnLostFocus(Launcher::SliderRowRoutedEventArgs& e) override;

    private:
        std::unique_ptr<Launcher::SliderRow> _row;
    };

    class KeyRowHost final : public HostControl,
                             public Launcher::KeyRowControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            const MphRead::Mods::InputBindingProperty* property, double labelWidth);
        [[nodiscard]] static Toolkit::ElementPtr Create(
            std::optional<std::u16string> label, Launcher::KeyRowGetHandler get,
            Launcher::KeyRowSetHandler set, double labelWidth);

        [[nodiscard]] Launcher::KeyRow& Row() const noexcept { return *_row; }

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetFocusable(bool focusable) override;
        void SetHandCursor() override;
        void Focus() override;
        [[nodiscard]] bool IsFocused() const override;
        [[nodiscard]] Launcher::GuiRect Bounds() const override;
        [[nodiscard]] Launcher::KeyRowPointerUpdateKind GetPointerUpdateKind(
            const Launcher::KeyRowPointerPressedEventArgs& e) const override;
        [[nodiscard]] Launcher::KeyRowPoint GetPosition(
            const Launcher::KeyRowPointerPressedEventArgs& e) const override;
        void InvalidateVisual() override;
        void BaseOnPointerPressed(
            Launcher::KeyRowPointerPressedEventArgs& e) override;
        void BaseOnPointerWheelChanged(
            Launcher::KeyRowPointerWheelEventArgs& e) override;
        void BaseOnPointerEntered(Launcher::KeyRowPointerEventArgs& e) override;
        void BaseOnPointerExited(Launcher::KeyRowPointerEventArgs& e) override;
        void BaseOnKeyDown(Launcher::KeyRowKeyEventArgs& e) override;
        void BaseOnLostFocus(Launcher::KeyRowRoutedEventArgs& e) override;
        void BaseOnGotFocus(Launcher::KeyRowGotFocusEventArgs& e) override;

    private:
        static void Wire(const Toolkit::ElementPtr& element, KeyRowHost* host);

        std::unique_ptr<Launcher::KeyRow> _row;
    };

    class PadRowHost final : public HostControl,
                             public Launcher::PadRowControlAdapter
    {
    public:
        [[nodiscard]] static Toolkit::ElementPtr Create(
            MphRead::Mods::Input::PadAction action, double labelWidth);

        [[nodiscard]] Launcher::PadRow& Row() const noexcept { return *_row; }

        [[nodiscard]] Toolkit::Size Measure(
            Toolkit::Element& element, Toolkit::Size available) override;
        void Render(Toolkit::Element& element, Toolkit::Renderer& renderer) override;

        void SetHeight(double height) override;
        [[nodiscard]] double GetHeight() const override;
        void SetFocusable(bool focusable) override;
        void SetHandCursor() override;
        void Focus() override;
        [[nodiscard]] bool IsFocused() const override;
        [[nodiscard]] Launcher::GuiRect Bounds() const override;
        [[nodiscard]] Launcher::PadRowPoint GetPosition(
            const Launcher::PadRowPointerPressedEventArgs& e) const override;
        [[nodiscard]] bool RectContains(
            Launcher::GuiRect rect, Launcher::PadRowPoint point) const override;
        void InvalidateVisual() override;
        [[nodiscard]] std::shared_ptr<Launcher::PadRowDispatcherTimer>
            CreateDispatcherTimer(std::chrono::milliseconds interval,
                Launcher::PadRowDispatcherPriority priority, Tick tick) override;
        void BaseOnPointerPressed(
            Launcher::PadRowPointerPressedEventArgs& e) override;
        void BaseOnKeyDown(Launcher::PadRowKeyEventArgs& e) override;
        void BaseOnPointerEntered(Launcher::PadRowPointerEventArgs& e) override;
        void BaseOnPointerExited(Launcher::PadRowPointerEventArgs& e) override;
        void BaseOnLostFocus(Launcher::PadRowRoutedEventArgs& e) override;
        void BaseOnGotFocus(Launcher::PadRowGotFocusEventArgs& e) override;
        void BaseOnDetachedFromVisualTree(
            Launcher::PadRowVisualTreeAttachmentEventArgs& e) override;

    private:
        std::unique_ptr<Launcher::PadRow> _row;
    };
}
