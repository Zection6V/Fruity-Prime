#include "MapPickerView.hpp"

#include "MenuEntry.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../../Metadata/Metadata.hpp"
#include "../../../Metadata/Rooms.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>

namespace
{
    using namespace MphRead::Mods::Launcher::Gui;

    [[nodiscard]] const std::string& RequireString(const MapPickerStringRef& value)
    {
        if (!value)
        {
            throw MapPickerArgumentNullException("key");
        }
        return *value;
    }

    [[nodiscard]] bool StringEquals(
        const MapPickerStringRef& left, const MapPickerStringRef& right) noexcept
    {
        if (!left || !right)
        {
            return !left && !right;
        }
        return *left == *right;
    }

    [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view text)
    {
        std::u16string result;
        result.reserve(text.size());

        std::size_t index = 0;
        while (index < text.size())
        {
            const auto first = static_cast<unsigned char>(text[index]);
            std::uint32_t scalar = 0;
            std::size_t consumed = 1;
            bool valid = true;

            if (first < 0x80U)
            {
                scalar = first;
            }
            else if (first >= 0xC2U && first <= 0xDFU
                && index + 1 < text.size())
            {
                const auto b1 = static_cast<unsigned char>(text[index + 1]);
                if ((b1 & 0xC0U) != 0x80U)
                {
                    valid = false;
                }
                else
                {
                    scalar = (static_cast<std::uint32_t>(first & 0x1FU) << 6)
                        | static_cast<std::uint32_t>(b1 & 0x3FU);
                    consumed = 2;
                }
            }
            else if (first >= 0xE0U && first <= 0xEFU
                && index + 2 < text.size())
            {
                const auto b1 = static_cast<unsigned char>(text[index + 1]);
                const auto b2 = static_cast<unsigned char>(text[index + 2]);
                const bool continuation = (b1 & 0xC0U) == 0x80U
                    && (b2 & 0xC0U) == 0x80U;
                const bool notOverlong = first != 0xE0U || b1 >= 0xA0U;
                const bool notSurrogate = first != 0xEDU || b1 <= 0x9FU;
                if (!continuation || !notOverlong || !notSurrogate)
                {
                    valid = false;
                }
                else
                {
                    scalar = (static_cast<std::uint32_t>(first & 0x0FU) << 12)
                        | (static_cast<std::uint32_t>(b1 & 0x3FU) << 6)
                        | static_cast<std::uint32_t>(b2 & 0x3FU);
                    consumed = 3;
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U
                && index + 3 < text.size())
            {
                const auto b1 = static_cast<unsigned char>(text[index + 1]);
                const auto b2 = static_cast<unsigned char>(text[index + 2]);
                const auto b3 = static_cast<unsigned char>(text[index + 3]);
                const bool continuation = (b1 & 0xC0U) == 0x80U
                    && (b2 & 0xC0U) == 0x80U
                    && (b3 & 0xC0U) == 0x80U;
                const bool notOverlong = first != 0xF0U || b1 >= 0x90U;
                const bool inRange = first != 0xF4U || b1 <= 0x8FU;
                if (!continuation || !notOverlong || !inRange)
                {
                    valid = false;
                }
                else
                {
                    scalar = (static_cast<std::uint32_t>(first & 0x07U) << 18)
                        | (static_cast<std::uint32_t>(b1 & 0x3FU) << 12)
                        | (static_cast<std::uint32_t>(b2 & 0x3FU) << 6)
                        | static_cast<std::uint32_t>(b3 & 0x3FU);
                    consumed = 4;
                }
            }
            else
            {
                valid = false;
            }

            if (!valid)
            {
                result.push_back(static_cast<char16_t>(0xFFFDU));
                ++index;
                continue;
            }

            if (scalar <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(scalar));
            }
            else
            {
                scalar -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3FFU)));
            }
            index += consumed;
        }
        return result;
    }

    [[nodiscard]] std::filesystem::path Utf8Path(std::string_view path)
    {
        std::u8string encoded;
        encoded.reserve(path.size());
        for (const unsigned char byte : path)
        {
            encoded.push_back(static_cast<char8_t>(byte));
        }
        return std::filesystem::path(encoded);
    }

    [[nodiscard]] bool FileExists(std::string_view path)
    {
        std::error_code error;
        const std::filesystem::path native = Utf8Path(path);
        const std::filesystem::file_status status = std::filesystem::status(native, error);
        return !error && std::filesystem::exists(status)
            && !std::filesystem::is_directory(status);
    }

    [[nodiscard]] std::vector<std::uint8_t> ReadAllBytes(std::string_view path)
    {
        const std::filesystem::path native = Utf8Path(path);
        std::ifstream stream(native, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Could not open file.");
        }

        stream.seekg(0, std::ios::end);
        const std::streamoff end = stream.tellg();
        if (end < 0)
        {
            throw std::runtime_error("Could not determine file length.");
        }
        if (static_cast<std::uintmax_t>(end)
            > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
        {
            throw std::length_error("File is too large.");
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            if (!stream || stream.gcount() != static_cast<std::streamsize>(bytes.size()))
            {
                throw std::runtime_error("Could not read file.");
            }
        }
        return bytes;
    }

    struct PostedFocusState final
    {
        std::shared_ptr<MapTile> Tile;
        std::shared_ptr<MapTileControlAdapter> Control;
    };

    template <typename TBody, typename TFinally>
    void CSharpTryFinally(TBody&& body, TFinally&& finalizer)
    {
        std::exception_ptr bodyException;
        try
        {
            body();
        }
        catch (...)
        {
            bodyException = std::current_exception();
        }

        finalizer();

        if (bodyException != nullptr)
        {
            std::rethrow_exception(bodyException);
        }
    }

    [[nodiscard]] std::shared_ptr<MapPickerRoomEnumerator> RequireEnumerator(
        const MapPickerRoomListRef& rooms)
    {
        if (!rooms)
        {
            throw MapPickerNullReferenceException();
        }
        std::shared_ptr<MapPickerRoomEnumerator> enumerator = rooms->GetEnumerator();
        if (!enumerator)
        {
            throw MapPickerNullReferenceException();
        }
        return enumerator;
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    MapPickerNullReferenceException::MapPickerNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    MapPickerArgumentNullException::MapPickerArgumentNullException(
        std::string parameterName)
        : std::invalid_argument("Value cannot be null. (Parameter '" + parameterName + "')"),
          _parameterName(std::move(parameterName))
    {
    }

    const std::string& MapPickerArgumentNullException::ParameterName() const noexcept
    {
        return _parameterName;
    }

    const MapPickerEventArgs MapPickerEventArgs::Empty{};

    MapPickerEventHandler::MapPickerEventHandler(
        void* context, Callback function, std::shared_ptr<void> keepAlive)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{context, function, std::move(keepAlive)});
            _invocations = std::move(list);
        }
    }

    MapPickerEventHandler::MapPickerEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    MapPickerEventHandler MapPickerEventHandler::Static(Callback function)
    {
        return MapPickerEventHandler(nullptr, function);
    }

    MapPickerEventHandler MapPickerEventHandler::Instance(
        std::shared_ptr<void> target, Callback function)
    {
        void* context = target.get();
        return MapPickerEventHandler(context, function, std::move(target));
    }

    MapPickerEventHandler MapPickerEventHandler::Combine(
        const MapPickerEventHandler& left,
        const MapPickerEventHandler& right)
    {
        if (left.IsNull())
        {
            return right;
        }
        if (right.IsNull())
        {
            return left;
        }

        auto list = std::make_shared<std::vector<Invocation>>();
        list->reserve(left._invocations->size() + right._invocations->size());
        list->insert(list->end(), left._invocations->begin(), left._invocations->end());
        list->insert(list->end(), right._invocations->begin(), right._invocations->end());
        return MapPickerEventHandler(std::move(list));
    }

    bool MapPickerEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const MapPickerEventHandler& left,
        const MapPickerEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void MapPickerEvent::Add(const MapPickerEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            auto next = std::make_shared<InvocationList>();
            next->reserve((current ? current->size() : 0) + handler._invocations->size());
            if (current)
            {
                next->insert(next->end(), current->begin(), current->end());
            }
            next->insert(next->end(),
                handler._invocations->begin(), handler._invocations->end());
            std::shared_ptr<const InvocationList> desired = std::move(next);
            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void MapPickerEvent::Remove(const MapPickerEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            if (!current || current->size() < handler._invocations->size())
            {
                return;
            }

            const std::size_t removeCount = handler._invocations->size();
            std::optional<std::size_t> match;
            for (std::size_t start = current->size() - removeCount + 1; start-- > 0;)
            {
                if (std::equal(handler._invocations->begin(), handler._invocations->end(),
                    current->begin() + static_cast<std::ptrdiff_t>(start)))
                {
                    match = start;
                    break;
                }
            }
            if (!match.has_value())
            {
                return;
            }

            std::shared_ptr<const InvocationList> desired;
            if (removeCount != current->size())
            {
                auto next = std::make_shared<InvocationList>();
                next->reserve(current->size() - removeCount);
                next->insert(next->end(), current->begin(),
                    current->begin() + static_cast<std::ptrdiff_t>(*match));
                next->insert(next->end(),
                    current->begin() + static_cast<std::ptrdiff_t>(*match + removeCount),
                    current->end());
                desired = std::move(next);
            }

            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void MapPickerEvent::Invoke(void* sender, const MapPickerEventArgs& args) const
    {
        const std::shared_ptr<const InvocationList> handlers = _handlers.load();
        if (!handlers)
        {
            return;
        }
        for (const Invocation& handler : *handlers)
        {
            handler.Function(handler.Context, sender, args);
        }
    }

    MapPickerDrawingContext::MapPickerDrawingContext() noexcept
        : TrackedTextAdapter(TrackedTextBrush{&GuiTheme::TextBrush})
    {
    }

    MapTile::MapTile(MapTileControlAdapter& control, MapPickerStringRef roomKey,
        double width, double height)
        : _control(control), _roomKey(std::move(roomKey))
    {
        const std::string& key = RequireString(_roomKey);
        const auto [meta, ignored] = ::MphRead::Metadata::GetRoomByName(key);
        static_cast<void>(ignored);
        _caption = Utf8ToUtf16(meta != nullptr && meta->InGameName.has_value()
            ? *meta->InGameName
            : key);
        _image = LoadPreview(_roomKey);
        _control.SetWidth(width);
        _control.SetHeight(height);
        _control.SetMargin(MapPickerThickness::Uniform(8.0));
        _control.SetFocusable(true);
        _control.SetHandCursor();
    }

    MapPickerStringRef MapTile::RoomKey() const noexcept
    {
        return _roomKey;
    }

    bool MapTile::Selected() const noexcept
    {
        return _selected;
    }

    void MapTile::Selected(bool value) noexcept
    {
        _selected = value;
    }

    void MapTile::AddClicked(const MapPickerEventHandler& handler)
    {
        _clicked.Add(handler);
    }

    void MapTile::RemoveClicked(const MapPickerEventHandler& handler)
    {
        _clicked.Remove(handler);
    }

    void MapTile::Focus()
    {
        _control.Focus();
    }

    std::shared_ptr<MapPickerBitmap> MapTile::LoadPreview(
        const MapPickerStringRef& roomKey)
    {
        const std::string path = ::MphRead::Mods::ThumbnailGenerator::PathFor(
            RequireString(roomKey));
        try
        {
            if (!FileExists(path))
            {
                return {};
            }
            const std::vector<std::uint8_t> bytes = ReadAllBytes(path);
            return _control.CreateBitmapFromMemory(bytes);
        }
        catch (...)
        {
            return {};
        }
    }

    void MapTile::OnPointerEntered(MapPickerPointerEventArgs& e)
    {
        _hover = true;
        _control.InvalidateVisual();
        _control.BaseOnPointerEntered(e);
    }

    void MapTile::OnPointerExited(MapPickerPointerEventArgs& e)
    {
        _hover = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerExited(e);
    }

    void MapTile::OnPointerReleased(MapPickerPointerReleasedEventArgs& e)
    {
        const MapPickerPoint p = _control.GetPosition(e);
        if (p.X >= 0.0 && p.Y >= 0.0
            && p.X <= _control.Bounds().Width
            && p.Y <= _control.Bounds().Height)
        {
            _clicked.Invoke(this, MapPickerEventArgs::Empty);
        }
        _control.BaseOnPointerReleased(e);
    }

    void MapTile::OnKeyDown(MapPickerKeyEventArgs& e)
    {
        if (e.Key == MapPickerKey::Enter || e.Key == MapPickerKey::Space)
        {
            _clicked.Invoke(this, MapPickerEventArgs::Empty);
            e.Handled = true;
            return;
        }
        _control.BaseOnKeyDown(e);
    }

    void MapTile::OnGotFocus(MapPickerGotFocusEventArgs& e)
    {
        _hover = true;
        _control.InvalidateVisual();
        _control.BaseOnGotFocus(e);
    }

    void MapTile::OnLostFocus(MapPickerRoutedEventArgs& e)
    {
        _hover = false;
        _control.InvalidateVisual();
        _control.BaseOnLostFocus(e);
    }

    void MapTile::Render(MapPickerDrawingContext& context)
    {
        const double pictureWidth = _control.Bounds().Width;
        const double pictureHeight = _control.Bounds().Height - CaptionHeight;
        const GuiRect picture{0.0, 0.0, pictureWidth, pictureHeight};
        if (_image)
        {
            context.DrawImage(*_image,
                GuiRect{0.0, 0.0, _image->Width, _image->Height}, picture);
        }
        else
        {
            context.FillRectangle(GuiTheme::PanelBrush, picture);
            const TrackedTextFormattedText none = TrackedText::Make(
                context, u"no preview", 12.0, false,
                TrackedTextBrush{&GuiTheme::TextDimBrush});
            context.DrawText(none, TrackedTextPoint{
                (picture.Width - none.Width) / 2.0,
                (picture.Height - none.Height) / 2.0
            });
        }

        const double captionY = _control.Bounds().Height - CaptionHeight;
        const double captionWidth = _control.Bounds().Width;
        const GuiRect caption{0.0, captionY, captionWidth, CaptionHeight};
        context.FillRectangle(GuiTheme::PanelBrush, caption);

        GuiBrush captionBrush(Selected() || _hover
            ? GuiTheme::Accent
            : GuiTheme::Text);
        TrackedTextFormattedText text = context.CreateFormattedText(
            _caption, TrackedTextCulture::Invariant,
            TrackedTextFlowDirection::LeftToRight,
            TrackedTextFace::FaceTrue, 12.0,
            TrackedTextBrush{&captionBrush});
        context.SetFormattedTextMaxTextWidth(
            text, _control.Bounds().Width - 8.0);
        context.SetFormattedTextMaxTextHeight(text, CaptionHeight);
        context.SetFormattedTextTrimming(
            text, MapPickerTextTrimming::CharacterEllipsis);
        context.DrawText(text, TrackedTextPoint{
            (_control.Bounds().Width - text.Width) / 2.0,
            caption.Y + (caption.Height - text.Height) / 2.0
        });
        if (Selected() || _hover)
        {
            context.DrawRectangle(nullptr,
                MapPickerPen{&GuiTheme::AccentBrush, Selected() ? 3.0 : 2.0},
                GuiRect{
                    1.0,
                    1.0,
                    _control.Bounds().Width - 2.0,
                    _control.Bounds().Height - 2.0
                });
        }
    }

    MapPickerView::MapPickerView(MapPickerViewAdapter& adapter,
        MapPickerRoomListRef rooms, MapPickerStringRef current)
        : _adapter(adapter)
    {
        _adapter.SetBackground(GuiTheme::InkBrush);
        _adapter.SetFocusable(true);

        const MapPickerViewAdapter::ElementHandle grid = _adapter.CreateWrapPanel();
        _adapter.SetWrapPanelOrientation(grid, MapPickerOrientation::Horizontal);

        std::shared_ptr<MapPickerRoomEnumerator> enumerator = RequireEnumerator(rooms);
        CSharpTryFinally(
            [&]
            {
                while (enumerator->MoveNext())
                {
                    MapPickerStringRef room = enumerator->Current();
                    std::shared_ptr<MapTileControlAdapter> tileControl
                        = _adapter.CreateMapTileControl();
                    if (!tileControl)
                    {
                        throw MapPickerNullReferenceException();
                    }

                    auto tile = std::make_shared<MapTile>(*tileControl, room);
                    _adapter.AttachMapTile(*tileControl, tile);
                    tile->Selected(StringEquals(room, current));
                    if (!_first)
                    {
                        _first = tile;
                    }
                    if (tile->Selected())
                    {
                        _selected = tile;
                    }
                    tile->AddClicked(MapPickerEventHandler(
                        this, &MapPickerView::OnTileClicked));

                    const MapPickerViewAdapter::ElementHandle tileElement
                        = _adapter.ElementOfMapTile(*tileControl);
                    _adapter.AddChild(grid, tileElement);
                    _tileControls.push_back(std::move(tileControl));
                    _tiles.push_back(std::move(tile));
                }
            },
            [&]
            {
                enumerator->Dispose();
            });

        _backControl = _adapter.CreateMenuEntryControl();
        if (!_backControl)
        {
            throw MapPickerNullReferenceException();
        }
        _back = std::make_unique<MenuEntry>(
            *_backControl, std::u16string(u"Back"), std::u16string{}, 13.0);
        _adapter.AttachMenuEntry(*_backControl, *_back);
        _back->Accent(GuiTheme::TextDim);
        const MapPickerViewAdapter::ElementHandle backElement
            = _adapter.ElementOfMenuEntry(*_backControl);
        _adapter.SetWidth(backElement, 120.0);
        _adapter.SetHorizontalAlignment(
            backElement, MapPickerHorizontalAlignment::Right);
        _adapter.SetVerticalAlignment(
            backElement, MapPickerVerticalAlignment::Center);
        _back->AddClick(MenuEntryEventHandler{
            this,
            [](void* context, void*, const MenuEntryEventArgs&)
            {
                MapPickerView::OnBackClicked(
                    context, nullptr, MapPickerEventArgs::Empty);
            }});

        const MapPickerViewAdapter::ElementHandle title = _adapter.CreateTextBlock();
        _adapter.SetTextBlockText(title, std::u16string_view(u"Choose a map"));
        _adapter.SetTextBlockFontFamily(title, GuiTheme::Display);
        _adapter.SetTextBlockFontSize(title, 18.0);
        _adapter.SetTextBlockForeground(title, GuiTheme::TextBrush);
        _adapter.SetVerticalAlignment(title, MapPickerVerticalAlignment::Center);

        const MapPickerViewAdapter::ElementHandle bar = _adapter.CreateGrid();
        _adapter.SetGridColumnDefinitions(bar, u"*,Auto");
        _adapter.SetGridColumn(title, 0);
        _adapter.SetGridColumn(backElement, 1);
        _adapter.AddChild(bar, title);
        _adapter.AddChild(bar, backElement);

        const MapPickerViewAdapter::ElementHandle header = _adapter.CreateBorder();
        _adapter.SetBorderBackground(header, GuiTheme::PanelBrush);
        _adapter.SetBorderPadding(header, MapPickerThickness{18.0, 12.0, 12.0, 12.0});
        _adapter.SetBorderChild(header, bar);

        const MapPickerViewAdapter::ElementHandle body = _adapter.CreateScrollViewer();
        _adapter.SetScrollViewerContent(body, grid);
        _adapter.SetScrollViewerPadding(body, MapPickerThickness::Uniform(14.0));
        _adapter.SetHorizontalScrollBarVisibility(
            body, MapPickerScrollBarVisibility::Disabled);

        const MapPickerViewAdapter::ElementHandle dock = _adapter.CreateDockPanel();
        _adapter.SetDockPanelLastChildFill(dock, true);
        _adapter.SetDock(header, MapPickerDock::Top);
        _adapter.AddChild(dock, header);
        _adapter.AddChild(dock, body);
        _adapter.SetContent(dock);
    }

    MapPickerView::~MapPickerView() = default;

    MapPickerStringRef MapPickerView::RoomKey() const noexcept
    {
        return _roomKey;
    }

    void MapPickerView::AddClosed(const MapPickerEventHandler& handler)
    {
        _closed.Add(handler);
    }

    void MapPickerView::RemoveClosed(const MapPickerEventHandler& handler)
    {
        _closed.Remove(handler);
    }

    void MapPickerView::OnTileClicked(
        void* context, void* sender, const MapPickerEventArgs&)
    {
        auto& self = *static_cast<MapPickerView*>(context);
        auto& tile = *static_cast<MapTile*>(sender);
        self._roomKey = tile.RoomKey();
        self._closed.Invoke(&self, MapPickerEventArgs::Empty);
    }

    void MapPickerView::OnBackClicked(
        void* context, void*, const MapPickerEventArgs&)
    {
        auto& self = *static_cast<MapPickerView*>(context);
        self._closed.Invoke(&self, MapPickerEventArgs::Empty);
    }

    void MapPickerView::FocusPostedTile(void* context)
    {
        if (context != nullptr)
        {
            auto& state = *static_cast<PostedFocusState*>(context);
            state.Tile->Focus();
        }
    }

    void MapPickerView::OnAttachedToVisualTree(
        MapPickerVisualTreeAttachmentEventArgs& e)
    {
        _adapter.BaseOnAttachedToVisualTree(e);

        const std::shared_ptr<MapTile> target = _selected ? _selected : _first;
        std::shared_ptr<PostedFocusState> state;
        if (target)
        {
            for (std::size_t index = 0; index < _tiles.size(); ++index)
            {
                if (_tiles[index].get() == target.get())
                {
                    state = std::make_shared<PostedFocusState>(PostedFocusState{
                        target, _tileControls[index]});
                    break;
                }
            }
            if (!state)
            {
                throw MapPickerNullReferenceException();
            }
        }
        _adapter.Post(state.get(), &MapPickerView::FocusPostedTile,
            state, MapPickerDispatcherPriority::Background);
    }

    void MapPickerView::OnKeyDown(MapPickerKeyEventArgs& e)
    {
        if (e.Key == MapPickerKey::Escape)
        {
            _closed.Invoke(this, MapPickerEventArgs::Empty);
            e.Handled = true;
            return;
        }
        _adapter.BaseOnKeyDown(e);
    }

    MapPickerWindow::MapPickerWindow(
        std::shared_ptr<MapPickerWindowAdapter> adapter,
        std::shared_ptr<MapPickerView> view)
        : _adapter(std::move(adapter)), _view(std::move(view))
    {
        if (!_adapter || !_view)
        {
            throw MapPickerNullReferenceException();
        }

        _view->AddClosed(MapPickerEventHandler::Instance(
            _adapter, &MapPickerWindow::OnViewClosed));
        _adapter->SetTitle(u"Choose a map");
        _adapter->SetIcon(GuiTheme::AppIcon.Value());
        _adapter->SetWidth(1120.0);
        _adapter->SetHeight(720.0);
        _adapter->SetMinWidth(560.0);
        _adapter->SetMinHeight(420.0);
        _adapter->SetBackground(GuiTheme::InkBrush);
        _adapter->SetRequestedThemeVariant(MapPickerThemeVariant::Dark);
        _adapter->SetWindowStartupLocation(MapPickerWindowStartupLocation::CenterOwner);
        _adapter->SetContent(*_view);
    }

    void MapPickerWindow::OnViewClosed(
        void* context, void*, const MapPickerEventArgs&)
    {
        static_cast<MapPickerWindowAdapter*>(context)->Close();
    }
}
