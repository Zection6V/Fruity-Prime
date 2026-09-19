#pragma once

#include "GuiTheme.hpp"
#include "TrackedText.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    class MenuEntryNullReferenceException final : public std::runtime_error
    {
    public:
        MenuEntryNullReferenceException();
    };

    enum class MenuEntryProperty : std::uint8_t
    {
        Title,
        Subtitle,
        Accent,
        SubtitleColor,
        Primary,
        Selected,
        IsEnabled
    };

    template <typename T>
    struct MenuEntryStyledProperty final
    {
        std::string_view Name;
        T DefaultValue;
        bool AffectsRender;
    };

    enum class MenuEntryKey : std::uint8_t
    {
        Other,
        Enter,
        Space
    };

    enum class MenuEntryBrushKind : std::uint8_t
    {
        Transparent,
        SolidColor
    };

    struct MenuEntryBrush final
    {
        MenuEntryBrushKind Kind;
        GuiColor Color{};

        [[nodiscard]] static constexpr MenuEntryBrush Transparent() noexcept
        {
            return MenuEntryBrush{MenuEntryBrushKind::Transparent, {}};
        }

        [[nodiscard]] static constexpr MenuEntryBrush Solid(GuiColor color) noexcept
        {
            return MenuEntryBrush{MenuEntryBrushKind::SolidColor, color};
        }

        friend constexpr bool operator==(
            const MenuEntryBrush&, const MenuEntryBrush&) noexcept = default;
    };

    struct MenuEntryPen final
    {
    };

    struct MenuEntryPoint final
    {
        double X;
        double Y;
    };

    struct MenuEntrySize final
    {
        double Width;
        double Height;
    };

    struct MenuEntryPointerEventArgs final
    {
        void* Native = nullptr;
        void* Pointer = nullptr;
    };

    struct MenuEntryPointerCaptureLostEventArgs final
    {
        void* Native = nullptr;
    };

    struct MenuEntryGotFocusEventArgs final
    {
        void* Native = nullptr;
    };

    struct MenuEntryRoutedEventArgs final
    {
        void* Native = nullptr;
    };

    struct MenuEntryKeyEventArgs final
    {
        void* Native = nullptr;
        MenuEntryKey Key = MenuEntryKey::Other;
        bool Handled = false;
    };

    struct MenuEntryEventArgs final
    {
        static const MenuEntryEventArgs Empty;
    };

    struct MenuEntryEventHandler final
    {
        using Callback = void (*)(void* context, void* sender, const MenuEntryEventArgs& args);

        void* Context = nullptr;
        Callback Function = nullptr;

        friend constexpr bool operator==(
            const MenuEntryEventHandler&, const MenuEntryEventHandler&) noexcept = default;
    };

    class MenuEntryEvent final
    {
    public:
        void Add(MenuEntryEventHandler handler);
        void Remove(MenuEntryEventHandler handler);

    private:
        friend class MenuEntry;
        void Invoke(void* sender, const MenuEntryEventArgs& args) const;
        mutable std::mutex _mutex;
        std::vector<MenuEntryEventHandler> _handlers;
    };

    class MenuEntryControlAdapter
    {
    public:
        virtual ~MenuEntryControlAdapter() = default;

        [[nodiscard]] virtual MenuEntrySize BaseMeasureOverride(MenuEntrySize availableSize) = 0;
        virtual void BaseOnPropertyChanged(MenuEntryProperty property) = 0;

        virtual void SetHeight(double height) = 0;
        [[nodiscard]] virtual double GetHeight() const = 0;

        virtual void SetFocusable(bool focusable) = 0;
        virtual void SetHandCursor() = 0;
        virtual void Focus() = 0;

        [[nodiscard]] virtual bool GetIsEnabled() const = 0;
        virtual void SetIsEnabled(bool isEnabled) = 0;
        [[nodiscard]] virtual bool IsPointerOver() const = 0;
        [[nodiscard]] virtual bool IsFocused() const = 0;
        [[nodiscard]] virtual GuiRect Bounds() const = 0;

        [[nodiscard]] virtual std::u16string ToUpperInvariant(
            std::u16string_view text) = 0;

        [[nodiscard]] virtual TrackedTextFormattedText CreateFormattedText(
            std::u16string_view text, TrackedTextCulture culture,
            TrackedTextFlowDirection flowDirection, TrackedTextFace face,
            double fontSize, TrackedTextBrush brush) = 0;

        [[nodiscard]] virtual void* Captured(void* pointer) const = 0;
        virtual void Capture(void* pointer, void* control) = 0;
        [[nodiscard]] virtual MenuEntryPoint GetPosition(
            const MenuEntryPointerEventArgs& e) const = 0;

        virtual void InvalidateVisual() = 0;

        virtual void BaseOnPointerEntered(MenuEntryPointerEventArgs& e) = 0;
        virtual void BaseOnPointerExited(MenuEntryPointerEventArgs& e) = 0;
        virtual void BaseOnPointerPressed(MenuEntryPointerEventArgs& e) = 0;
        virtual void BaseOnPointerReleased(MenuEntryPointerEventArgs& e) = 0;
        virtual void BaseOnPointerCaptureLost(MenuEntryPointerCaptureLostEventArgs& e) = 0;
        virtual void BaseOnGotFocus(MenuEntryGotFocusEventArgs& e) = 0;
        virtual void BaseOnLostFocus(MenuEntryRoutedEventArgs& e) = 0;
        virtual void BaseOnKeyDown(MenuEntryKeyEventArgs& e) = 0;
    };

    class MenuEntryDrawingContext : public TrackedTextAdapter
    {
    public:
        MenuEntryDrawingContext() noexcept;
        ~MenuEntryDrawingContext() override = default;

        virtual void FillRectangle(MenuEntryBrush brush, GuiRect rect) = 0;
        virtual void DrawRectangle(MenuEntryBrush brush,
            std::optional<MenuEntryPen> pen, GuiRoundedRect rect) = 0;
    };

    class MenuEntry final
    {
    public:
        static const MenuEntryStyledProperty<std::optional<std::u16string>> TitleProperty;
        static const MenuEntryStyledProperty<std::optional<std::u16string>> SubtitleProperty;
        static const MenuEntryStyledProperty<GuiColor> AccentProperty;
        static const MenuEntryStyledProperty<GuiColor> SubtitleColorProperty;
        static const MenuEntryStyledProperty<bool> PrimaryProperty;
        static const MenuEntryStyledProperty<bool> SelectedProperty;

        MenuEntry(MenuEntryControlAdapter& control,
            std::optional<std::u16string> title,
            std::optional<std::u16string> subtitle = std::u16string{},
            double titleSize = 21.0);

        MenuEntry(const MenuEntry&) = delete;
        MenuEntry& operator=(const MenuEntry&) = delete;
        MenuEntry(MenuEntry&&) = delete;
        MenuEntry& operator=(MenuEntry&&) = delete;

        [[nodiscard]] std::optional<std::u16string> Title() const;
        void Title(std::optional<std::u16string> value);

        [[nodiscard]] std::optional<std::u16string> Subtitle() const;
        void Subtitle(std::optional<std::u16string> value);

        [[nodiscard]] GuiColor Accent() const noexcept;
        void Accent(GuiColor value);

        [[nodiscard]] GuiColor SubtitleColor() const noexcept;
        void SubtitleColor(GuiColor value);

        [[nodiscard]] bool Primary() const noexcept;
        void Primary(bool value);

        [[nodiscard]] bool Selected() const noexcept;
        void Selected(bool value);

        [[nodiscard]] double Height() const;
        void Height(double value);

        [[nodiscard]] bool IsEnabled() const;
        void IsEnabled(bool value);

        void AddClick(MenuEntryEventHandler handler);
        void RemoveClick(MenuEntryEventHandler handler);

        [[nodiscard]] MenuEntrySize MeasureOverride(MenuEntrySize availableSize);

        void OnPointerEntered(MenuEntryPointerEventArgs& e);
        void OnPointerExited(MenuEntryPointerEventArgs& e);
        void OnPointerPressed(MenuEntryPointerEventArgs& e);
        void OnPointerReleased(MenuEntryPointerEventArgs& e);
        void OnPointerCaptureLost(MenuEntryPointerCaptureLostEventArgs& e);
        void OnGotFocus(MenuEntryGotFocusEventArgs& e);
        void OnLostFocus(MenuEntryRoutedEventArgs& e);
        void OnKeyDown(MenuEntryKeyEventArgs& e);

        void Render(MenuEntryDrawingContext& context);

    private:
        static constexpr double PlainHeight = 42.0;
        static constexpr double SubtitledHeight = 54.0;

        void OnPropertyChanged(MenuEntryProperty property);
        void RenderPrimary(MenuEntryDrawingContext& context, GuiRect body, bool lit);

        [[nodiscard]] const std::u16string& Require(
            const std::optional<std::u16string>& value) const;
        void InvalidateForProperty(MenuEntryProperty property);

        MenuEntryControlAdapter& _control;
        std::optional<std::u16string> _title;
        std::optional<std::u16string> _subtitle;
        GuiColor _accent;
        GuiColor _subtitleColor;
        bool _primary;
        bool _selected;
        double _titleSize = 0.0;
        bool _pressed = false;
        MenuEntryEvent _click;
    };
}
