#pragma once

#include "GuiTheme.hpp"
#include "TrackedText.hpp"
#include "../../Network/NetStatus.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using ServerRowStringRef = std::shared_ptr<const std::u16string>;

    class ServerRowNullReferenceException final : public std::runtime_error
    {
    public:
        ServerRowNullReferenceException();
    };

    // Keep the surrogate numerically identical to Avalonia 11.3.11 Key so
    // adapters can pass raw Key values through without remapping or collision.
    enum class ServerRowKey : std::int32_t
    {
        Other = 0,
        Enter = 6,
        Space = 18
    };

    enum class ServerRowTextTrimming : std::uint8_t
    {
        CharacterEllipsis
    };

    enum class ServerRowBrushKind : std::uint8_t
    {
        Transparent,
        Reference
    };

    struct ServerRowBrush final
    {
        ServerRowBrushKind Kind;
        GuiBrush* Brush;

        [[nodiscard]] static constexpr ServerRowBrush Transparent() noexcept
        {
            return ServerRowBrush{ServerRowBrushKind::Transparent, nullptr};
        }

        [[nodiscard]] static constexpr ServerRowBrush Reference(GuiBrush& brush) noexcept
        {
            return ServerRowBrush{ServerRowBrushKind::Reference, &brush};
        }
    };

    struct ServerRowPointerEventArgs final
    {
        void* Native = nullptr;
    };

    struct ServerRowPointerPressedEventArgs final
    {
        void* Native = nullptr;
    };

    struct ServerRowKeyEventArgs final
    {
        void* Native = nullptr;
        ServerRowKey Key = ServerRowKey::Other;
        bool Handled = false;
    };

    struct ServerRowEventArgs final
    {
        static const ServerRowEventArgs Empty;
    };

    class ServerRowEventHandler final
    {
    public:
        using Callback = void (*)(void* target, void* sender, const ServerRowEventArgs& args);

        ServerRowEventHandler() = default;
        ServerRowEventHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static ServerRowEventHandler Combine(
            const ServerRowEventHandler& left, const ServerRowEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const ServerRowEventHandler& left, const ServerRowEventHandler& right) noexcept;

    private:
        struct Invocation final
        {
            std::shared_ptr<void> Target;
            Callback Function = nullptr;

            friend bool operator==(
                const Invocation& left, const Invocation& right) noexcept
            {
                return left.Target.get() == right.Target.get()
                    && left.Function == right.Function;
            }
        };

        explicit ServerRowEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;

        friend class ServerRowEvent;
    };

    class ServerRowEvent final
    {
    public:
        void Add(const ServerRowEventHandler& handler);
        void Remove(const ServerRowEventHandler& handler);

    private:
        friend class ServerRow;
        void Invoke(void* sender, const ServerRowEventArgs& args) const;

        using Invocation = ServerRowEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        std::atomic<std::shared_ptr<const InvocationList>> _handlers{};
    };

    class ServerRowControlAdapter
    {
    public:
        virtual ~ServerRowControlAdapter() = default;

        virtual void SetHeight(double height) = 0;
        [[nodiscard]] virtual double GetHeight() const = 0;
        virtual void SetFocusable(bool focusable) = 0;
        virtual void SetHandCursor() = 0;
        virtual void Focus() = 0;
        [[nodiscard]] virtual bool IsFocused() const = 0;
        [[nodiscard]] virtual GuiRect Bounds() const = 0;

        // DefaultInterpolatedStringHandler uses CurrentCulture for the
        // $"{players}/{maxPlayers}" branch in the C# oracle.
        [[nodiscard]] virtual std::u16string FormatCurrentInt32(std::int32_t value) const = 0;

        virtual void InvalidateVisual() = 0;

        virtual void BaseOnPointerEntered(ServerRowPointerEventArgs& e) = 0;
        virtual void BaseOnPointerExited(ServerRowPointerEventArgs& e) = 0;
        virtual void BaseOnPointerPressed(ServerRowPointerPressedEventArgs& e) = 0;
        virtual void BaseOnKeyDown(ServerRowKeyEventArgs& e) = 0;
    };

    class ServerHeaderControlAdapter
    {
    public:
        virtual ~ServerHeaderControlAdapter() = default;

        virtual void SetHeight(double height) = 0;
        [[nodiscard]] virtual double GetHeight() const = 0;
        virtual void SetIsHitTestVisible(bool isHitTestVisible) = 0;
        [[nodiscard]] virtual GuiRect Bounds() const = 0;
    };

    struct ServerRowClipHandle final
    {
        std::uintptr_t Native = 0;
    };

    class ServerRowDrawingContext : public TrackedTextAdapter
    {
    public:
        ServerRowDrawingContext() noexcept;
        ~ServerRowDrawingContext() override = default;

        virtual void FillRectangle(
            ServerRowBrush brush, GuiRect rect, double radius = 0.0) = 0;
        virtual void SetFormattedTextMaxTextWidth(
            TrackedTextFormattedText& text, double maxTextWidth) = 0;
        virtual void SetFormattedTextMaxTextHeight(
            TrackedTextFormattedText& text, double maxTextHeight) = 0;
        virtual void SetFormattedTextTrimming(
            TrackedTextFormattedText& text, ServerRowTextTrimming trimming) = 0;
        [[nodiscard]] virtual ServerRowClipHandle PushClip(GuiRect rect) = 0;
        virtual void DisposeClip(ServerRowClipHandle handle) = 0;
    };

    class ServerRow final
    {
    public:
        struct Columns final
        {
            const double NameX;
            const double NameWidth;
            const double MapX;
            const double MapWidth;
            const double ModeX;
            const double ModeWidth;
            const double PlayersRight;
            const double PlayersWidth;
            const double PingRight;
            const double PingWidth;

            Columns() noexcept;
            explicit Columns(double width) noexcept;
            Columns(const Columns&) noexcept = default;
            Columns(Columns&&) noexcept = default;
            Columns& operator=(const Columns& other) noexcept;
            Columns& operator=(Columns&& other) noexcept;

        private:
            static constexpr double Margin = 8.0;
            static constexpr double Gutter = 10.0;
            static constexpr double MaxPing = 34.0;
            static constexpr double MaxPlayers = 52.0;
            static constexpr double MaxMode = 66.0;
            static constexpr double NameShare = 0.44;

            using Values = std::array<double, 10>;
            explicit Columns(const Values& values) noexcept;
            [[nodiscard]] static Values Compute(double width) noexcept;
        };

        ServerRow(ServerRowControlAdapter& control,
            ServerRowStringRef name,
            ServerRowStringRef endpoint);

        ServerRow(const ServerRow&) = delete;
        ServerRow& operator=(const ServerRow&) = delete;
        ServerRow(ServerRow&&) = delete;
        ServerRow& operator=(ServerRow&&) = delete;

        [[nodiscard]] double Height() const;
        void Height(double value);

        void AddClicked(const ServerRowEventHandler& handler);
        void RemoveClicked(const ServerRowEventHandler& handler);

        void SetStatus(::MphRead::Mods::Network::ServerStatus status);

        void OnPointerEntered(ServerRowPointerEventArgs& e);
        void OnPointerExited(ServerRowPointerEventArgs& e);
        void OnPointerPressed(ServerRowPointerPressedEventArgs& e);
        void OnKeyDown(ServerRowKeyEventArgs& e);

        void Render(ServerRowDrawingContext& context);

        static void Draw(ServerRowDrawingContext& context,
            std::optional<std::u16string_view> text, double x, double width,
            GuiBrush* brush, bool bold, bool rightAlign, double size = 13.0);

        [[nodiscard]] ServerRowStringRef Endpoint() const noexcept;

    private:
        ServerRowControlAdapter& _control;
        ServerRowStringRef _name;
        ServerRowStringRef _endpoint;
        std::u16string _map;
        std::u16string _mode;
        std::u16string _players;
        std::u16string _ping;
        GuiBrush* _pingBrush = &GuiTheme::TextDimBrush;
        bool _answered = false;
        bool _hot = false;
        ServerRowEvent _clicked;
    };

    class ServerHeader final
    {
    public:
        explicit ServerHeader(ServerHeaderControlAdapter& control);

        ServerHeader(const ServerHeader&) = delete;
        ServerHeader& operator=(const ServerHeader&) = delete;
        ServerHeader(ServerHeader&&) = delete;
        ServerHeader& operator=(ServerHeader&&) = delete;

        [[nodiscard]] double Height() const;
        void Height(double value);

        void Render(ServerRowDrawingContext& context);

    private:
        ServerHeaderControlAdapter& _control;
    };
}
