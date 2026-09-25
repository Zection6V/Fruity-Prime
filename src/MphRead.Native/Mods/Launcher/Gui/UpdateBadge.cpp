#include "UpdateBadge.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::MathMin;

namespace
{
    class UpdateBadgeMetricsAdapter final
        : public MphRead::Mods::Launcher::Gui::TrackedTextAdapter
    {
    public:
        explicit UpdateBadgeMetricsAdapter(
            MphRead::Mods::Launcher::Gui::UpdateBadgeControlAdapter& control) noexcept
            : TrackedTextAdapter(
                MphRead::Mods::Launcher::Gui::TrackedTextBrush{
                    &MphRead::Mods::Launcher::Gui::GuiTheme::TextBrush}),
              _control(control)
        {
        }

        MphRead::Mods::Launcher::Gui::TrackedTextFormattedText CreateFormattedText(
            std::u16string_view text,
            MphRead::Mods::Launcher::Gui::TrackedTextCulture culture,
            MphRead::Mods::Launcher::Gui::TrackedTextFlowDirection flowDirection,
            MphRead::Mods::Launcher::Gui::TrackedTextFace face,
            double fontSize,
            MphRead::Mods::Launcher::Gui::TrackedTextBrush brush) override
        {
            return _control.CreateFormattedText(
                text, culture, flowDirection, face, fontSize, brush);
        }

        void DrawText(
            const MphRead::Mods::Launcher::Gui::TrackedTextFormattedText&,
            MphRead::Mods::Launcher::Gui::TrackedTextPoint) override
        {
        }

    private:
        MphRead::Mods::Launcher::Gui::UpdateBadgeControlAdapter& _control;
    };
}

namespace MphRead::Mods::Launcher::Gui
{
    const UpdateBadgeEventArgs UpdateBadgeEventArgs::Empty{};

    UpdateBadgeEventHandler::UpdateBadgeEventHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    UpdateBadgeEventHandler::UpdateBadgeEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    UpdateBadgeEventHandler UpdateBadgeEventHandler::Combine(
        const UpdateBadgeEventHandler& left,
        const UpdateBadgeEventHandler& right)
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
        return UpdateBadgeEventHandler(std::move(list));
    }

    bool UpdateBadgeEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const UpdateBadgeEventHandler& left,
        const UpdateBadgeEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void UpdateBadgeEvent::Add(const UpdateBadgeEventHandler& handler)
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
                next->insert(next->end(), handler._invocations->begin(),
                    handler._invocations->end());
                desired = std::move(next);
            }
            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void UpdateBadgeEvent::Remove(const UpdateBadgeEventHandler& handler)
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

    void UpdateBadgeEvent::Invoke(void* sender, const UpdateBadgeEventArgs& args) const
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

    UpdateBadgeDrawingContext::UpdateBadgeDrawingContext() noexcept
        : TrackedTextAdapter(TrackedTextBrush{&GuiTheme::TextBrush})
    {
    }

    UpdateBadge::UpdateBadge(UpdateBadgeControlAdapter& control)
        : _control(control)
    {
        _control.SetFocusable(true);
        _control.SetHandCursor();
        _control.SetIsVisible(false);
    }

    bool UpdateBadge::IsVisible() const
    {
        return _control.GetIsVisible();
    }

    void UpdateBadge::IsVisible(bool value)
    {
        _control.SetIsVisible(value);
    }

    UpdateBadgeSize UpdateBadge::DesiredSize() const
    {
        return _control.DesiredSize();
    }

    void UpdateBadge::AddClick(const UpdateBadgeEventHandler& handler)
    {
        _click.Add(handler);
    }

    void UpdateBadge::RemoveClick(const UpdateBadgeEventHandler& handler)
    {
        _click.Remove(handler);
    }

    void UpdateBadge::Show(std::optional<std::u16string> subtitle)
    {
        _subtitle = std::move(subtitle);
        _control.SetIsVisible(true);
        _control.InvalidateMeasure();
        _control.InvalidateVisual();
    }

    void UpdateBadge::Say(std::optional<std::u16string> subtitle)
    {
        _subtitle = std::move(subtitle);
        _control.InvalidateMeasure();
        _control.InvalidateVisual();
    }

    const std::u16string& UpdateBadge::RequireSubtitle() const
    {
        if (!_subtitle.has_value())
        {
            throw UpdateBadgeNullReferenceException();
        }
        return *_subtitle;
    }

    UpdateBadgeSize UpdateBadge::MeasureOverride(UpdateBadgeSize available)
    {
        UpdateBadgeMetricsAdapter metrics(_control);
        const std::u16string upper = _control.ToUpperInvariant(_title);
        const double title = TrackedText::Measure(metrics, upper, TitleSize, Tracking);

        double subtitle = 0.0;
        if (!RequireSubtitle().empty())
        {
            const std::u16string& subtitleArgument = RequireSubtitle();
            subtitle = TrackedText::Make(metrics, subtitleArgument, SubtitleSize, false,
                TrackedTextBrush{&GuiTheme::TextBrush}).Width;
        }
        const double width = MathMax(title, subtitle) + PadX * 2.0;
        const double lineHeight = TrackedText::LineHeight(metrics, TitleSize);
        const bool hasSubtitleForHeight = !RequireSubtitle().empty();
        const double height = lineHeight + PadY * 2.0
            + (hasSubtitleForHeight ? SubtitleSize + 5.0 : 0.0);
        return UpdateBadgeSize{MathMin(width, available.Width), height};
    }

    void UpdateBadge::OnPointerEntered(UpdateBadgePointerEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnPointerEntered(e);
    }

    void UpdateBadge::OnPointerExited(UpdateBadgePointerEventArgs& e)
    {
        _pressed = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerExited(e);
    }

    void UpdateBadge::OnPointerPressed(UpdateBadgePointerPressedEventArgs& e)
    {
        _pressed = true;
        _control.Focus();
        _control.InvalidateVisual();
        _control.BaseOnPointerPressed(e);
    }

    void UpdateBadge::OnPointerReleased(UpdateBadgePointerReleasedEventArgs& e)
    {
        const bool was = _pressed;
        _pressed = false;
        _control.InvalidateVisual();
        _control.BaseOnPointerReleased(e);
        if (was && _control.IsPointerOver())
        {
            _click.Invoke(this, UpdateBadgeEventArgs::Empty);
        }
    }

    void UpdateBadge::OnGotFocus(UpdateBadgeGotFocusEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnGotFocus(e);
    }

    void UpdateBadge::OnLostFocus(UpdateBadgeRoutedEventArgs& e)
    {
        _control.InvalidateVisual();
        _control.BaseOnLostFocus(e);
    }

    void UpdateBadge::OnKeyDown(UpdateBadgeKeyEventArgs& e)
    {
        if (e.Key == UpdateBadgeKey::Enter || e.Key == UpdateBadgeKey::Space)
        {
            e.Handled = true;
            _click.Invoke(this, UpdateBadgeEventArgs::Empty);
            return;
        }
        _control.BaseOnKeyDown(e);
    }

    void UpdateBadge::Render(UpdateBadgeDrawingContext& context)
    {
        const bool pointerOver = _control.IsPointerOver();
        const bool lit = pointerOver || _control.IsFocused();
        const double bodyWidth = _control.Bounds().Width;
        const double bodyHeight = _control.Bounds().Height;
        const GuiRect body{0.0, 0.0, bodyWidth, bodyHeight};
        const GuiColor fill = _pressed
            ? GuiTheme::Shade(GuiTheme::Warm, -0.25)
            : lit ? GuiTheme::Shade(GuiTheme::Warm, 0.15) : GuiTheme::Warm;
        const GuiBrush fillBrush{fill};
        const GuiVector radius{5.0, 5.0};
        context.DrawRectangle(fillBrush, std::nullopt, GuiRoundedRect{
            body, radius, radius, radius, radius
        });

        const GuiBrush ink{GuiColor::FromRgb(26, 18, 4)};
        const std::u16string upper = _control.ToUpperInvariant(_title);
        TrackedText::Draw(context, upper, TitleSize, TrackedTextBrush{&ink},
            PadX, PadY, Tracking);

        if (!RequireSubtitle().empty())
        {
            const std::u16string& subtitleArgument = RequireSubtitle();
            const GuiBrush subtitleInk{GuiColor::FromArgb(200, 26, 18, 4)};
            const TrackedTextFormattedText sub = TrackedText::Make(
                context, subtitleArgument, SubtitleSize, false,
                TrackedTextBrush{&subtitleInk});
            context.DrawText(sub, TrackedTextPoint{
                PadX,
                PadY + TrackedText::LineHeight(context, TitleSize) + 3.0
            });
        }
    }
}
