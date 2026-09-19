#pragma once

#include "GuiTheme.hpp"
#include "TrackedText.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    class MenuEntry;
    class MenuEntryControlAdapter;

    using MapPickerStringRef = std::shared_ptr<const std::string>;

    class MapPickerNullReferenceException final : public std::runtime_error
    {
    public:
        MapPickerNullReferenceException();
    };

    class MapPickerArgumentNullException final : public std::invalid_argument
    {
    public:
        explicit MapPickerArgumentNullException(std::string parameterName);
        [[nodiscard]] const std::string& ParameterName() const noexcept;

    private:
        std::string _parameterName;
    };

    enum class MapPickerKey : std::uint8_t
    {
        Other,
        Escape,
        Enter,
        Space
    };

    enum class MapPickerOrientation : std::uint8_t
    {
        Horizontal
    };

    enum class MapPickerHorizontalAlignment : std::uint8_t
    {
        Left,
        Center,
        Right,
        Stretch
    };

    enum class MapPickerVerticalAlignment : std::uint8_t
    {
        Top,
        Center,
        Bottom,
        Stretch
    };

    enum class MapPickerScrollBarVisibility : std::uint8_t
    {
        Disabled
    };

    enum class MapPickerDock : std::uint8_t
    {
        Top
    };

    enum class MapPickerDispatcherPriority : std::uint8_t
    {
        Background
    };

    enum class MapPickerThemeVariant : std::uint8_t
    {
        Dark
    };

    enum class MapPickerWindowStartupLocation : std::uint8_t
    {
        CenterOwner
    };

    enum class MapPickerTextTrimming : std::uint8_t
    {
        CharacterEllipsis
    };

    struct MapPickerThickness final
    {
        double Left;
        double Top;
        double Right;
        double Bottom;

        [[nodiscard]] static constexpr MapPickerThickness Uniform(double value) noexcept
        {
            return MapPickerThickness{value, value, value, value};
        }
    };

    struct MapPickerPoint final
    {
        double X;
        double Y;
    };

    struct MapPickerPointerEventArgs final
    {
        void* Native = nullptr;
    };

    struct MapPickerPointerReleasedEventArgs final
    {
        void* Native = nullptr;
    };

    struct MapPickerGotFocusEventArgs final
    {
        void* Native = nullptr;
    };

    struct MapPickerRoutedEventArgs final
    {
        void* Native = nullptr;
    };

    struct MapPickerVisualTreeAttachmentEventArgs final
    {
        void* Native = nullptr;
    };

    struct MapPickerKeyEventArgs final
    {
        void* Native = nullptr;
        MapPickerKey Key = MapPickerKey::Other;
        bool Handled = false;
    };

    struct MapPickerEventArgs final
    {
        static const MapPickerEventArgs Empty;
    };

    class MapPickerEventHandler final
    {
    public:
        using Callback = void (*)(void* context, void* sender, const MapPickerEventArgs& args);

        MapPickerEventHandler() = default;
        MapPickerEventHandler(void* context, Callback function,
            std::shared_ptr<void> keepAlive = {});

        [[nodiscard]] static MapPickerEventHandler Static(Callback function);
        [[nodiscard]] static MapPickerEventHandler Instance(
            std::shared_ptr<void> target, Callback function);
        [[nodiscard]] static MapPickerEventHandler Combine(
            const MapPickerEventHandler& left,
            const MapPickerEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const MapPickerEventHandler& left,
            const MapPickerEventHandler& right) noexcept;

    private:
        struct Invocation final
        {
            void* Context = nullptr;
            Callback Function = nullptr;
            std::shared_ptr<void> KeepAlive;

            friend bool operator==(
                const Invocation& left, const Invocation& right) noexcept
            {
                return left.Context == right.Context
                    && left.Function == right.Function;
            }
        };

        explicit MapPickerEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;
        friend class MapPickerEvent;
    };

    class MapPickerEvent final
    {
    public:
        void Add(const MapPickerEventHandler& handler);
        void Remove(const MapPickerEventHandler& handler);
        void Invoke(void* sender, const MapPickerEventArgs& args) const;

    private:
        using Invocation = MapPickerEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        std::atomic<std::shared_ptr<const InvocationList>> _handlers{};
    };

    class MapPickerRoomEnumerator
    {
    public:
        virtual ~MapPickerRoomEnumerator() = default;
        [[nodiscard]] virtual bool MoveNext() = 0;
        [[nodiscard]] virtual MapPickerStringRef Current() const = 0;
        virtual void Dispose() = 0;
    };

    class MapPickerRoomList
    {
    public:
        virtual ~MapPickerRoomList() = default;
        [[nodiscard]] virtual std::shared_ptr<MapPickerRoomEnumerator> GetEnumerator() const = 0;
    };

    using MapPickerRoomListRef = std::shared_ptr<const MapPickerRoomList>;

    struct MapPickerBitmap final
    {
        std::shared_ptr<void> Native;
        double Width = 0.0;
        double Height = 0.0;
    };

    struct MapPickerPen final
    {
        const GuiBrush* Brush = nullptr;
        double Thickness = 0.0;
    };

    class MapPickerDrawingContext : public TrackedTextAdapter
    {
    public:
        MapPickerDrawingContext() noexcept;
        ~MapPickerDrawingContext() override = default;

        virtual void DrawImage(const MapPickerBitmap& image,
            GuiRect source, GuiRect destination) = 0;
        virtual void FillRectangle(const GuiBrush& brush, GuiRect rect) = 0;
        virtual void SetFormattedTextMaxTextWidth(
            TrackedTextFormattedText& text, double maxTextWidth) = 0;
        virtual void SetFormattedTextMaxTextHeight(
            TrackedTextFormattedText& text, double maxTextHeight) = 0;
        virtual void SetFormattedTextTrimming(
            TrackedTextFormattedText& text, MapPickerTextTrimming trimming) = 0;
        virtual void DrawRectangle(const GuiBrush* brush,
            const MapPickerPen& pen, GuiRect rect) = 0;
    };

    class MapTileControlAdapter
    {
    public:
        virtual ~MapTileControlAdapter() = default;

        virtual void SetWidth(double width) = 0;
        virtual void SetHeight(double height) = 0;
        virtual void SetMargin(MapPickerThickness margin) = 0;
        virtual void SetFocusable(bool focusable) = 0;
        virtual void SetHandCursor() = 0;
        virtual void Focus() = 0;

        [[nodiscard]] virtual GuiRect Bounds() const = 0;
        [[nodiscard]] virtual MapPickerPoint GetPosition(
            const MapPickerPointerReleasedEventArgs& e) const = 0;

        [[nodiscard]] virtual std::shared_ptr<MapPickerBitmap> CreateBitmapFromMemory(
            std::span<const std::uint8_t> bytes) = 0;

        virtual void InvalidateVisual() = 0;
        virtual void BaseOnPointerEntered(MapPickerPointerEventArgs& e) = 0;
        virtual void BaseOnPointerExited(MapPickerPointerEventArgs& e) = 0;
        virtual void BaseOnPointerReleased(MapPickerPointerReleasedEventArgs& e) = 0;
        virtual void BaseOnKeyDown(MapPickerKeyEventArgs& e) = 0;
        virtual void BaseOnGotFocus(MapPickerGotFocusEventArgs& e) = 0;
        virtual void BaseOnLostFocus(MapPickerRoutedEventArgs& e) = 0;
    };

    class MapTile final
    {
    public:
        MapTile(MapTileControlAdapter& control, MapPickerStringRef roomKey,
            double width = 248.0, double height = 168.0);

        MapTile(const MapTile&) = delete;
        MapTile& operator=(const MapTile&) = delete;
        MapTile(MapTile&&) = delete;
        MapTile& operator=(MapTile&&) = delete;

        [[nodiscard]] MapPickerStringRef RoomKey() const noexcept;
        [[nodiscard]] bool Selected() const noexcept;

        void AddClicked(const MapPickerEventHandler& handler);
        void RemoveClicked(const MapPickerEventHandler& handler);

        void Focus();
        void OnPointerEntered(MapPickerPointerEventArgs& e);
        void OnPointerExited(MapPickerPointerEventArgs& e);
        void OnPointerReleased(MapPickerPointerReleasedEventArgs& e);
        void OnKeyDown(MapPickerKeyEventArgs& e);
        void OnGotFocus(MapPickerGotFocusEventArgs& e);
        void OnLostFocus(MapPickerRoutedEventArgs& e);
        void Render(MapPickerDrawingContext& context);

    private:
        friend class MapPickerView;

        static constexpr double CaptionHeight = 26.0;

        void InitializeSelected(bool selected) noexcept;
        [[nodiscard]] std::shared_ptr<MapPickerBitmap> LoadPreview(
            const MapPickerStringRef& roomKey);

        MapTileControlAdapter& _control;
        MapPickerStringRef _roomKey;
        std::shared_ptr<MapPickerBitmap> _image;
        std::u16string _caption;
        bool _hover = false;
        bool _selected = false;
        MapPickerEvent _clicked;
    };

    class MapPickerViewAdapter
    {
    public:
        using ElementHandle = void*;
        using PostedCallback = void (*)(void* context);

        virtual ~MapPickerViewAdapter() = default;

        virtual void SetBackground(const GuiBrush& brush) = 0;
        virtual void SetFocusable(bool focusable) = 0;

        [[nodiscard]] virtual ElementHandle CreateWrapPanel() = 0;
        virtual void SetWrapPanelOrientation(
            ElementHandle panel, MapPickerOrientation orientation) = 0;

        [[nodiscard]] virtual std::shared_ptr<MapTileControlAdapter>
            CreateMapTileControl() = 0;
        [[nodiscard]] virtual ElementHandle ElementOfMapTile(
            MapTileControlAdapter& tile) = 0;

        [[nodiscard]] virtual std::shared_ptr<MenuEntryControlAdapter>
            CreateMenuEntryControl() = 0;
        [[nodiscard]] virtual ElementHandle ElementOfMenuEntry(
            MenuEntryControlAdapter& entry) = 0;

        [[nodiscard]] virtual ElementHandle CreateTextBlock() = 0;
        virtual void SetTextBlockText(
            ElementHandle textBlock, std::optional<std::u16string_view> text) = 0;
        virtual void SetTextBlockFontFamily(
            ElementHandle textBlock, GuiFontFamily family) = 0;
        virtual void SetTextBlockFontSize(ElementHandle textBlock, double fontSize) = 0;
        virtual void SetTextBlockForeground(
            ElementHandle textBlock, const GuiBrush& brush) = 0;

        [[nodiscard]] virtual ElementHandle CreateGrid() = 0;
        virtual void SetGridColumnDefinitions(
            ElementHandle grid, std::u16string_view definitions) = 0;
        virtual void SetGridColumn(ElementHandle child, std::int32_t column) = 0;

        [[nodiscard]] virtual ElementHandle CreateBorder() = 0;
        virtual void SetBorderBackground(
            ElementHandle border, const GuiBrush& brush) = 0;
        virtual void SetBorderPadding(ElementHandle border, MapPickerThickness padding) = 0;
        virtual void SetBorderChild(ElementHandle border, ElementHandle child) = 0;

        [[nodiscard]] virtual ElementHandle CreateScrollViewer() = 0;
        virtual void SetScrollViewerContent(
            ElementHandle scrollViewer, ElementHandle content) = 0;
        virtual void SetScrollViewerPadding(
            ElementHandle scrollViewer, MapPickerThickness padding) = 0;
        virtual void SetHorizontalScrollBarVisibility(ElementHandle scrollViewer,
            MapPickerScrollBarVisibility visibility) = 0;

        [[nodiscard]] virtual ElementHandle CreateDockPanel() = 0;
        virtual void SetDockPanelLastChildFill(ElementHandle panel, bool value) = 0;
        virtual void SetDock(ElementHandle child, MapPickerDock dock) = 0;

        virtual void SetWidth(ElementHandle element, double width) = 0;
        virtual void SetHorizontalAlignment(
            ElementHandle element, MapPickerHorizontalAlignment alignment) = 0;
        virtual void SetVerticalAlignment(
            ElementHandle element, MapPickerVerticalAlignment alignment) = 0;
        virtual void AddChild(ElementHandle parent, ElementHandle child) = 0;
        virtual void SetContent(ElementHandle content) = 0;

        virtual void Post(void* context, PostedCallback callback,
            std::shared_ptr<void> keepAlive,
            MapPickerDispatcherPriority priority) = 0;

        virtual void BaseOnAttachedToVisualTree(
            MapPickerVisualTreeAttachmentEventArgs& e) = 0;
        virtual void BaseOnKeyDown(MapPickerKeyEventArgs& e) = 0;
    };

    class MapPickerView final
    {
    public:
        MapPickerView(MapPickerViewAdapter& adapter,
            MapPickerRoomListRef rooms, MapPickerStringRef current);
        ~MapPickerView();

        MapPickerView(const MapPickerView&) = delete;
        MapPickerView& operator=(const MapPickerView&) = delete;
        MapPickerView(MapPickerView&&) = delete;
        MapPickerView& operator=(MapPickerView&&) = delete;

        [[nodiscard]] MapPickerStringRef RoomKey() const noexcept;
        void AddClosed(const MapPickerEventHandler& handler);
        void RemoveClosed(const MapPickerEventHandler& handler);

        void OnAttachedToVisualTree(MapPickerVisualTreeAttachmentEventArgs& e);
        void OnKeyDown(MapPickerKeyEventArgs& e);

    private:
        static void OnTileClicked(
            void* context, void* sender, const MapPickerEventArgs& args);
        static void OnBackClicked(
            void* context, void* sender, const MapPickerEventArgs& args);
        static void FocusPostedTile(void* context);

        MapPickerViewAdapter& _adapter;
        MapPickerStringRef _roomKey;
        std::shared_ptr<MapTile> _first;
        std::shared_ptr<MapTile> _selected;
        std::vector<std::shared_ptr<MapTileControlAdapter>> _tileControls;
        std::vector<std::shared_ptr<MapTile>> _tiles;
        std::shared_ptr<MenuEntryControlAdapter> _backControl;
        std::unique_ptr<MenuEntry> _back;
        MapPickerEvent _closed;
    };

    class MapPickerWindowAdapter
    {
    public:
        virtual ~MapPickerWindowAdapter() = default;

        virtual void Close() = 0;
        virtual void SetTitle(std::u16string_view title) = 0;
        virtual void SetIcon(const std::optional<GuiWindowIcon>& icon) = 0;
        virtual void SetWidth(double width) = 0;
        virtual void SetHeight(double height) = 0;
        virtual void SetMinWidth(double minWidth) = 0;
        virtual void SetMinHeight(double minHeight) = 0;
        virtual void SetBackground(const GuiBrush& brush) = 0;
        virtual void SetRequestedThemeVariant(MapPickerThemeVariant variant) = 0;
        virtual void SetWindowStartupLocation(
            MapPickerWindowStartupLocation location) = 0;
        virtual void SetContent(MapPickerView& view) = 0;
    };

    class MapPickerWindow final
    {
    public:
        MapPickerWindow(MapPickerWindowAdapter& adapter, MapPickerView* view);

        MapPickerWindow(const MapPickerWindow&) = delete;
        MapPickerWindow& operator=(const MapPickerWindow&) = delete;
        MapPickerWindow(MapPickerWindow&&) = delete;
        MapPickerWindow& operator=(MapPickerWindow&&) = delete;

    private:
        static void OnViewClosed(
            void* context, void* sender, const MapPickerEventArgs& args);

        MapPickerWindowAdapter& _adapter;
    };
}
