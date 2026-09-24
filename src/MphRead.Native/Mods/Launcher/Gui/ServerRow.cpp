#include "ServerRow.hpp"
#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iterator>
#include <limits>
#include <utility>

using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::MathMin;
using ::MphRead::NativeRuntime::Utf8ToUtf16;

namespace
{
    using namespace MphRead::Mods::Launcher::Gui;

    [[nodiscard]] std::u16string InvariantInt32(std::int32_t value)
    {
        std::array<char, 16> buffer{};
        const auto result = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value);
        std::u16string text;
        text.reserve(static_cast<std::size_t>(result.ptr - buffer.data()));
        for (const char* current = buffer.data(); current != result.ptr; ++current)
        {
            text.push_back(static_cast<char16_t>(
                static_cast<unsigned char>(*current)));
        }
        return text;
    }

    [[nodiscard]] std::optional<std::u16string_view> ViewOf(
        const ServerRowStringRef& text) noexcept
    {
        if (!text)
        {
            return std::nullopt;
        }
        return std::u16string_view(*text);
    }

    [[nodiscard]] const std::u16string_view& RequireText(
        const std::optional<std::u16string_view>& text)
    {
        if (!text.has_value())
        {
            throw ServerRowNullReferenceException();
        }
        return *text;
    }

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

        // A C# using statement lowers to try/finally. If Dispose throws while
        // another exception is pending, the Dispose exception replaces it.
        finalizer();

        if (bodyException != nullptr)
        {
            std::rethrow_exception(bodyException);
        }
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    ServerRowNullReferenceException::ServerRowNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    const ServerRowEventArgs ServerRowEventArgs::Empty{};

    ServerRowEventHandler::ServerRowEventHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    ServerRowEventHandler::ServerRowEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    ServerRowEventHandler ServerRowEventHandler::Combine(
        const ServerRowEventHandler& left, const ServerRowEventHandler& right)
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
        return ServerRowEventHandler(std::move(list));
    }

    bool ServerRowEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const ServerRowEventHandler& left, const ServerRowEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void ServerRowEvent::Add(const ServerRowEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            std::shared_ptr<const InvocationList> desired;
            if (!current)
            {
                desired = handler._invocations;
            }
            else
            {
                auto next = std::make_shared<InvocationList>();
                next->reserve(current->size() + handler._invocations->size());
                next->insert(next->end(), current->begin(), current->end());
                next->insert(next->end(),
                    handler._invocations->begin(), handler._invocations->end());
                desired = std::move(next);
            }
            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void ServerRowEvent::Remove(const ServerRowEventHandler& handler)
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

    void ServerRowEvent::Invoke(void* sender, const ServerRowEventArgs& args) const
    {
        const std::shared_ptr<const InvocationList> handlers = _handlers.load();
        if (!handlers)
        {
            return;
        }
        for (const Invocation& handler : *handlers)
        {
            handler.Function(handler.Target.get(), sender, args);
        }
    }

    ServerRowDrawingContext::ServerRowDrawingContext() noexcept
        : TrackedTextAdapter(TrackedTextBrush{&GuiTheme::TextBrush})
    {
    }

    ServerRow::Columns::Columns() noexcept
        : NameX(0.0), NameWidth(0.0),
          MapX(0.0), MapWidth(0.0),
          ModeX(0.0), ModeWidth(0.0),
          PlayersRight(0.0), PlayersWidth(0.0),
          PingRight(0.0), PingWidth(0.0)
    {
    }

    ServerRow::Columns::Columns(const Values& values) noexcept
        : NameX(values[0]), NameWidth(values[1]),
          MapX(values[2]), MapWidth(values[3]),
          ModeX(values[4]), ModeWidth(values[5]),
          PlayersRight(values[6]), PlayersWidth(values[7]),
          PingRight(values[8]), PingWidth(values[9])
    {
    }

    ServerRow::Columns::Values ServerRow::Columns::Compute(double width) noexcept
    {
        const double pingRight = width - Margin;
        const double pingWidth = MaxPing;
        const double playersRight = pingRight - MaxPing - Gutter;
        const double playersWidth = MaxPlayers;
        const double modeWidth = MathMin(
            MaxMode, MathMax(0.0, (width - 200.0) * 0.4));
        const double modeX = playersRight - MaxPlayers - Gutter - modeWidth;
        const double nameX = Margin;
        const double rest = MathMax(0.0, modeX - Gutter - Margin);
        const double nameWidth = rest * NameShare;
        const double mapX = nameX + nameWidth + Gutter;
        const double mapWidth = MathMax(0.0, rest - nameWidth - Gutter);
        return Values{
            nameX, nameWidth,
            mapX, mapWidth,
            modeX, modeWidth,
            playersRight, playersWidth,
            pingRight, pingWidth
        };
    }

    ServerRow::Columns::Columns(double width) noexcept
        : Columns(Compute(width))
    {
    }

    ServerRow::Columns& ServerRow::Columns::operator=(
        const Columns& other) noexcept
    {
        if (this != &other)
        {
            std::destroy_at(this);
            std::construct_at(this, other);
        }
        return *this;
    }

    ServerRow::Columns& ServerRow::Columns::operator=(
        Columns&& other) noexcept
    {
        return *this = other;
    }

    ServerRow::ServerRow(ServerRowControlAdapter& control,
        ServerRowStringRef name,
        ServerRowStringRef endpoint)
        : _control(control)
    {
        _name = std::move(name);
        _endpoint = std::move(endpoint);
        _map = u"asking...";
        Height(30.0);
        _control.SetFocusable(true);
        _control.SetHandCursor();
    }

    double ServerRow::Height() const
    {
        return _control.GetHeight();
    }

    void ServerRow::Height(double value)
    {
        _control.SetHeight(value);
    }

    void ServerRow::AddClicked(const ServerRowEventHandler& handler)
    {
        _clicked.Add(handler);
    }

    void ServerRow::RemoveClicked(const ServerRowEventHandler& handler)
    {
        _clicked.Remove(handler);
    }

    void ServerRow::SetStatus(::MphRead::Mods::Network::ServerStatus status)
    {
        _answered = status.Online;
        if (!status.Online)
        {
            _map = u"did not answer";
            _mode = u"";
            _players = u"";
            _ping = u"--";
            _pingBrush = &GuiTheme::BadBrush;
            _control.InvalidateVisual();
            return;
        }

        _map = Utf8ToUtf16(status.RoomKey);
        _mode = Utf8ToUtf16(::MphRead::Mods::Network::NetStatus::ModeName(status.Mode));
        if (status.MaxPlayers > 0)
        {
            std::u16string players = _control.FormatCurrentInt32(status.Players);
            players.push_back(u'/');
            players += _control.FormatCurrentInt32(status.MaxPlayers);
            _players = std::move(players);
        }
        else
        {
            _players = InvariantInt32(status.Players);
        }

        if (status.Latency >= 0)
        {
            _ping = InvariantInt32(status.Latency);
            _pingBrush = status.Latency < 80
                ? &GuiTheme::GoodBrush
                : status.Latency < 160
                    ? &GuiTheme::WarmBrush
                    : &GuiTheme::BadBrush;
        }
        else
        {
            _ping = u"--";
            _pingBrush = &GuiTheme::TextDimBrush;
        }
        _control.InvalidateVisual();
    }

    void ServerRow::OnPointerEntered(ServerRowPointerEventArgs& e)
    {
        _hot = true;
        _control.InvalidateVisual();
        _control.BaseOnPointerEntered(e);
    }

    void ServerRow::OnPointerExited(ServerRowPointerEventArgs& e)
    {
        _hot = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerExited(e);
    }

    void ServerRow::OnPointerPressed(ServerRowPointerPressedEventArgs& e)
    {
        _control.Focus();
        _clicked.Invoke(this, ServerRowEventArgs::Empty);
        _control.BaseOnPointerPressed(e);
    }

    void ServerRow::OnKeyDown(ServerRowKeyEventArgs& e)
    {
        if (e.Key == ServerRowKey::Enter || e.Key == ServerRowKey::Space)
        {
            _clicked.Invoke(this, ServerRowEventArgs::Empty);
            e.Handled = true;
            return;
        }
        _control.BaseOnKeyDown(e);
    }

    void ServerRow::Render(ServerRowDrawingContext& context)
    {
        const double fullWidth = _control.Bounds().Width;
        const double fullHeight = _control.Bounds().Height;
        const GuiRect full{0.0, 0.0, fullWidth, fullHeight};
        context.FillRectangle(ServerRowBrush::Transparent(), full);

        bool highlighted = _hot;
        if (!highlighted)
        {
            highlighted = _control.IsFocused();
        }
        if (highlighted)
        {
            context.FillRectangle(
                ServerRowBrush::Reference(GuiTheme::PanelLightBrush), full, 4.0);
        }

        const Columns columns(_control.Bounds().Width);
        GuiBrush* nameBrush = _answered
            ? &GuiTheme::TextBrush
            : &GuiTheme::TextDimBrush;
        Draw(context, ViewOf(_name), columns.NameX, columns.NameWidth,
            nameBrush, true, false);
        Draw(context, std::u16string_view(_map), columns.MapX, columns.MapWidth,
            &GuiTheme::TextDimBrush, false, false);
        Draw(context, std::u16string_view(_mode), columns.ModeX, columns.ModeWidth,
            &GuiTheme::TextDimBrush, false, false);
        Draw(context, std::u16string_view(_players), columns.PlayersRight,
            columns.PlayersWidth, &GuiTheme::TextBrush, false, true);
        Draw(context, std::u16string_view(_ping), columns.PingRight,
            columns.PingWidth, _pingBrush, false, true);
    }

    void ServerRow::Draw(ServerRowDrawingContext& context,
        std::optional<std::u16string_view> text, double x, double width,
        GuiBrush* brush, bool bold, bool rightAlign, double size)
    {
        const std::u16string_view& required = RequireText(text);
        if (required.empty() || width <= 4.0)
        {
            return;
        }

        TrackedTextFormattedText formatted = TrackedText::Make(
            context, required, size, bold, TrackedTextBrush{brush});
        context.SetFormattedTextMaxTextWidth(formatted, width);
        context.SetFormattedTextMaxTextHeight(formatted, size * 1.6);
        context.SetFormattedTextTrimming(
            formatted, ServerRowTextTrimming::CharacterEllipsis);

        const double left = rightAlign
            ? x - MathMin(formatted.Width, width)
            : x;
        const GuiRect clip{
            rightAlign ? x - width : x,
            0.0,
            width,
            size * 1.6 + 8.0
        };
        const ServerRowClipHandle clipHandle = context.PushClip(clip);
        CSharpTryFinally(
            [&]()
            {
                context.DrawText(formatted, TrackedTextPoint{left, 8.0});
            },
            [&]()
            {
                context.DisposeClip(clipHandle);
            });
    }

    ServerRowStringRef ServerRow::Endpoint() const noexcept
    {
        return _endpoint;
    }

    ServerHeader::ServerHeader(ServerHeaderControlAdapter& control)
        : _control(control)
    {
        Height(22.0);
        _control.SetIsHitTestVisible(false);
    }

    double ServerHeader::Height() const
    {
        return _control.GetHeight();
    }

    void ServerHeader::Height(double value)
    {
        _control.SetHeight(value);
    }

    void ServerHeader::Render(ServerRowDrawingContext& context)
    {
        const ServerRow::Columns columns(_control.Bounds().Width);
        ServerRow::Draw(context, std::u16string_view(u"SERVER"),
            columns.NameX, columns.NameWidth,
            &GuiTheme::TextDimBrush, true, false, 11.0);
        ServerRow::Draw(context, std::u16string_view(u"MAP"),
            columns.MapX, columns.MapWidth,
            &GuiTheme::TextDimBrush, true, false, 11.0);
        ServerRow::Draw(context, std::u16string_view(u"TYPE"),
            columns.ModeX, columns.ModeWidth,
            &GuiTheme::TextDimBrush, true, false, 11.0);
        ServerRow::Draw(context, std::u16string_view(u"PLAYERS"),
            columns.PlayersRight, columns.PlayersWidth,
            &GuiTheme::TextDimBrush, true, true, 11.0);
        ServerRow::Draw(context, std::u16string_view(u"PING"),
            columns.PingRight, columns.PingWidth,
            &GuiTheme::TextDimBrush, true, true, 11.0);

        const double lineY = _control.Bounds().Height - 1.0;
        const double lineWidth = _control.Bounds().Width;
        context.FillRectangle(ServerRowBrush::Reference(GuiTheme::EdgeBrush),
            GuiRect{0.0, lineY, lineWidth, 1.0});
    }
}
