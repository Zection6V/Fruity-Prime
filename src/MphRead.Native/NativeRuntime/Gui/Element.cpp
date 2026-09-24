#include "Element.hpp"

#include "Text.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MphRead::NativeRuntime::Gui
{
    namespace
    {
        [[nodiscard]] double Clamp(
            double value, std::optional<double> low, std::optional<double> high) noexcept
        {
            if (low.has_value() && value < *low)
            {
                value = *low;
            }
            if (high.has_value() && value > *high)
            {
                value = *high;
            }
            return value < 0.0 ? 0.0 : value;
        }

        [[nodiscard]] bool IsInfinite(double value) noexcept
        {
            return value >= std::numeric_limits<double>::max() / 2.0;
        }
    }

    ElementPtr Element::Create(ElementKind kind)
    {
        return std::make_shared<Element>(kind);
    }

    void Element::AddChild(const ElementPtr& child)
    {
        InsertChild(static_cast<std::int32_t>(_children.size()), child);
    }

    void Element::InsertChild(std::int32_t index, const ElementPtr& child)
    {
        if (child == nullptr)
        {
            return;
        }
        const std::size_t at = std::min(
            static_cast<std::size_t>(index < 0 ? 0 : index), _children.size());
        child->_parent = this;
        _children.insert(_children.begin() + static_cast<std::ptrdiff_t>(at), child);
    }

    void Element::ClearChildren()
    {
        for (const ElementPtr& child : _children)
        {
            child->_parent = nullptr;
        }
        _children.clear();
    }

    Size Element::ClampToConstraints(Size value) const noexcept
    {
        Size result = value;
        if (Width.has_value())
        {
            result.Width = *Width;
        }
        if (Height.has_value())
        {
            result.Height = *Height;
        }
        result.Width = Clamp(result.Width, MinWidth, MaxWidth);
        result.Height = Clamp(result.Height, MinHeight, MaxHeight);
        return result;
    }

    Size Element::Measure(Size available)
    {
        if (!Visible)
        {
            _desired = Size{};
            return _desired;
        }

        // The margin is outside the control, so it comes off what the child
        // may use and goes back on to what it asks for.
        Size inner = available;
        inner.Width = std::max(0.0, inner.Width - Margin.Horizontal());
        inner.Height = std::max(0.0, inner.Height - Margin.Vertical());
        if (Width.has_value())
        {
            inner.Width = *Width;
        }
        if (Height.has_value())
        {
            inner.Height = *Height;
        }

        // A layout transform lays the child out at full size and reports the
        // scaled result, which is what LayoutTransformControl does.
        const double scale = LayoutScale > 0.0 ? LayoutScale : 1.0;
        if (scale != 1.0)
        {
            inner.Width /= scale;
            inner.Height /= scale;
        }
        Size content = MeasureCore(inner);
        if (scale != 1.0)
        {
            content.Width *= scale;
            content.Height *= scale;
        }
        content = ClampToConstraints(content);
        _desired = Size{content.Width + Margin.Horizontal(),
            content.Height + Margin.Vertical()};
        return _desired;
    }

    Size Element::MeasureCore(Size available)
    {
        const double padWidth = Padding.Horizontal() + BorderThickness.Horizontal();
        const double padHeight = Padding.Vertical() + BorderThickness.Vertical();
        Size inner{std::max(0.0, available.Width - padWidth),
            std::max(0.0, available.Height - padHeight)};

        switch (_kind)
        {
        case ElementKind::TextBlock:
        {
            if (!Text.has_value() || Text->empty())
            {
                return Size{0.0, 0.0};
            }
            const Size measured = MeasureText(
                *Text, FontSize, Weight, WrapText ? inner.Width : 0.0);
            return Size{measured.Width + padWidth, measured.Height + padHeight};
        }

        case ElementKind::Custom:
        {
            if (Behaviour == nullptr)
            {
                return Size{0.0, 0.0};
            }
            const Size measured = Behaviour->Measure(*this, inner);
            return Size{measured.Width + padWidth, measured.Height + padHeight};
        }

        case ElementKind::DockPanel:
        {
            // Each docked child takes its edge out of what is left; the last
            // one fills the remainder when the panel says so.
            Size remaining = inner;
            Size used{};
            const std::size_t count = _children.size();
            for (std::size_t i = 0; i < count; ++i)
            {
                const ElementPtr& child = _children[i];
                if (!child->Visible)
                {
                    child->Measure(Size{});
                    continue;
                }
                const bool fills = LastChildFill && i + 1 == count;
                // A docked child is measured with no bound along the edge it
                // takes, as Avalonia's DockPanel measures one; otherwise a
                // child that stretches would claim the whole panel.
                const bool sideways = child->DockSide == Dock::Left
                    || child->DockSide == Dock::Right;
                const Size available = fills
                    ? remaining
                    : Size{sideways ? std::numeric_limits<double>::max()
                                    : remaining.Width,
                          sideways ? remaining.Height
                                   : std::numeric_limits<double>::max()};
                const Size desired = child->Measure(available);
                if (fills)
                {
                    used.Width += desired.Width;
                    used.Height = std::max(used.Height, desired.Height);
                    break;
                }
                if (child->DockSide == Dock::Left || child->DockSide == Dock::Right)
                {
                    remaining.Width = std::max(0.0, remaining.Width - desired.Width);
                    used.Width += desired.Width;
                    used.Height = std::max(used.Height, desired.Height);
                    continue;
                }
                remaining.Height = std::max(0.0, remaining.Height - desired.Height);
                used.Height += desired.Height;
                used.Width = std::max(used.Width, desired.Width);
            }
            return Size{used.Width + padWidth, used.Height + padHeight};
        }

        case ElementKind::WrapPanel:
        {
            double lineWidth = 0.0;
            double lineHeight = 0.0;
            double width = 0.0;
            double height = 0.0;
            for (const ElementPtr& child : _children)
            {
                const Size desired = child->Measure(inner);
                if (!child->Visible)
                {
                    continue;
                }
                if (lineWidth > 0.0 && lineWidth + desired.Width > inner.Width)
                {
                    width = std::max(width, lineWidth);
                    height += lineHeight;
                    lineWidth = 0.0;
                    lineHeight = 0.0;
                }
                lineWidth += desired.Width;
                lineHeight = std::max(lineHeight, desired.Height);
            }
            width = std::max(width, lineWidth);
            height += lineHeight;
            return Size{width + padWidth, height + padHeight};
        }

        case ElementKind::StackPanel:
        {
            Size total{};
            std::int32_t counted = 0;
            for (const ElementPtr& child : _children)
            {
                if (!child->Visible)
                {
                    continue;
                }
                Size childAvailable = inner;
                if (StackOrientation == Orientation::Vertical)
                {
                    childAvailable.Height = std::numeric_limits<double>::max();
                }
                else
                {
                    childAvailable.Width = std::numeric_limits<double>::max();
                }
                const Size childSize = child->Measure(childAvailable);
                if (StackOrientation == Orientation::Vertical)
                {
                    total.Width = std::max(total.Width, childSize.Width);
                    total.Height += childSize.Height;
                }
                else
                {
                    total.Height = std::max(total.Height, childSize.Height);
                    total.Width += childSize.Width;
                }
                ++counted;
            }
            if (counted > 1)
            {
                const double gaps = Spacing * (counted - 1);
                if (StackOrientation == Orientation::Vertical)
                {
                    total.Height += gaps;
                }
                else
                {
                    total.Width += gaps;
                }
            }
            return Size{total.Width + padWidth, total.Height + padHeight};
        }

        case ElementKind::Grid:
        {
            const std::vector<double> columns = MeasureGridTrack(
                ColumnDefinitions, inner.Width, true);
            const std::vector<double> rows = MeasureGridTrack(
                RowDefinitions, inner.Height, false);
            // Sizing the tracks measures only the children in an auto one, so
            // every child is measured here against the cell it landed in --
            // without which a child of a star track is never measured at all.
            for (const ElementPtr& child : _children)
            {
                const std::size_t column = std::min(
                    static_cast<std::size_t>(
                        child->GridColumn < 0 ? 0 : child->GridColumn),
                    columns.empty() ? 0 : columns.size() - 1);
                const std::size_t row = std::min(
                    static_cast<std::size_t>(child->GridRow < 0 ? 0 : child->GridRow),
                    rows.empty() ? 0 : rows.size() - 1);
                double height = rows.empty() ? inner.Height : rows[row];
                for (std::size_t i = row + 1;
                    i < rows.size()
                    && i < row + static_cast<std::size_t>(
                        child->GridRowSpan < 1 ? 1 : child->GridRowSpan);
                    ++i)
                {
                    height += rows[i];
                }
                child->Measure(Size{
                    columns.empty() ? inner.Width : columns[column], height});
            }
            double width = 0.0;
            double height = 0.0;
            for (const double value : columns) { width += value; }
            for (const double value : rows) { height += value; }
            return Size{width + padWidth, height + padHeight};
        }

        case ElementKind::ScrollViewer:
        {
            Size total{};
            for (const ElementPtr& child : _children)
            {
                Size childAvailable = inner;
                childAvailable.Height = std::numeric_limits<double>::max();
                const Size childSize = child->Measure(childAvailable);
                total.Width = std::max(total.Width, childSize.Width);
                total.Height = std::max(total.Height, childSize.Height);
            }
            // A scroll viewer asks for no more than it was offered; what does
            // not fit is what it scrolls.
            if (!IsInfinite(inner.Height))
            {
                total.Height = std::min(total.Height, inner.Height);
            }
            return Size{total.Width + padWidth, total.Height + padHeight};
        }

        case ElementKind::Panel:
        case ElementKind::Border:
        default:
        {
            Size total{};
            for (const ElementPtr& child : _children)
            {
                const Size childSize = child->Measure(inner);
                total.Width = std::max(total.Width, childSize.Width);
                total.Height = std::max(total.Height, childSize.Height);
            }
            return Size{total.Width + padWidth, total.Height + padHeight};
        }
        }
    }

    std::vector<double> Element::MeasureGridTrack(
        const std::vector<GridLength>& definitions, double available, bool horizontal)
    {
        std::vector<GridLength> defs = definitions;
        if (defs.empty())
        {
            defs.push_back(GridLength{GridLength::Unit::Star, 1.0});
        }
        std::vector<double> sizes(defs.size(), 0.0);

        // Fixed and auto tracks first; the stars share what is left.
        double used = 0.0;
        double stars = 0.0;
        for (std::size_t i = 0; i < defs.size(); ++i)
        {
            if (defs[i].Kind == GridLength::Unit::Pixel)
            {
                sizes[i] = defs[i].Value;
                used += sizes[i];
            }
            else if (defs[i].Kind == GridLength::Unit::Star)
            {
                stars += defs[i].Value;
            }
        }
        for (std::size_t i = 0; i < defs.size(); ++i)
        {
            if (defs[i].Kind != GridLength::Unit::Auto)
            {
                continue;
            }
            double largest = 0.0;
            for (const ElementPtr& child : _children)
            {
                if (!child->Visible)
                {
                    continue;
                }
                const std::int32_t index = horizontal ? child->GridColumn : child->GridRow;
                if (static_cast<std::size_t>(index < 0 ? 0 : index) != i)
                {
                    continue;
                }
                Size childAvailable{
                    horizontal ? std::numeric_limits<double>::max() : available,
                    horizontal ? available : std::numeric_limits<double>::max()};
                const Size childSize = child->Measure(childAvailable);
                largest = std::max(largest, horizontal ? childSize.Width : childSize.Height);
            }
            sizes[i] = largest;
            used += largest;
        }
        if (stars > 0.0 && !IsInfinite(available))
        {
            const double remaining = std::max(0.0, available - used);
            for (std::size_t i = 0; i < defs.size(); ++i)
            {
                if (defs[i].Kind == GridLength::Unit::Star)
                {
                    sizes[i] = remaining * (defs[i].Value / stars);
                }
            }
        }
        return sizes;
    }

    void Element::Arrange(Rect finalRect)
    {
        if (!Visible)
        {
            _bounds = Rect{};
            return;
        }

        Rect inner{finalRect.X + Margin.Left, finalRect.Y + Margin.Top,
            std::max(0.0, finalRect.Width - Margin.Horizontal()),
            std::max(0.0, finalRect.Height - Margin.Vertical())};

        // Alignment: a stretched control fills the slot, any other takes the
        // size it asked for and sits where it was told.
        const double wanted = std::max(0.0, _desired.Width - Margin.Horizontal());
        const double wantedHeight = std::max(0.0, _desired.Height - Margin.Vertical());

        if (Horizontal != HorizontalAlignment::Stretch || Width.has_value())
        {
            const double width = std::min(inner.Width, Width.value_or(wanted));
            switch (Horizontal)
            {
            case HorizontalAlignment::Center:
                inner.X += (inner.Width - width) / 2.0;
                break;
            case HorizontalAlignment::Right:
                inner.X += inner.Width - width;
                break;
            default:
                break;
            }
            inner.Width = width;
        }
        if (Vertical != VerticalAlignment::Stretch || Height.has_value())
        {
            const double height = std::min(inner.Height, Height.value_or(wantedHeight));
            switch (Vertical)
            {
            case VerticalAlignment::Center:
                inner.Y += (inner.Height - height) / 2.0;
                break;
            case VerticalAlignment::Bottom:
                inner.Y += inner.Height - height;
                break;
            default:
                break;
            }
            inner.Height = height;
        }

        _bounds = inner;
        const double scale = LayoutScale > 0.0 ? LayoutScale : 1.0;
        if (scale != 1.0)
        {
            // The child is arranged at full size about this corner; the draw
            // scales it back down.
            ArrangeCore(Rect{inner.X, inner.Y, inner.Width / scale,
                inner.Height / scale});
            return;
        }
        ArrangeCore(inner);
    }

    void Element::ArrangeCore(Rect bounds)
    {
        const double left = Padding.Left + BorderThickness.Left;
        const double top = Padding.Top + BorderThickness.Top;
        Rect content{bounds.X + left, bounds.Y + top,
            std::max(0.0, bounds.Width - Padding.Horizontal() - BorderThickness.Horizontal()),
            std::max(0.0, bounds.Height - Padding.Vertical() - BorderThickness.Vertical())};

        switch (_kind)
        {
        case ElementKind::TextBlock:
            break;

        case ElementKind::Custom:
            if (Behaviour != nullptr)
            {
                Behaviour->Arrange(*this, content);
            }
            break;

        case ElementKind::DockPanel:
        {
            Rect remaining = content;
            const std::size_t count = _children.size();
            for (std::size_t i = 0; i < count; ++i)
            {
                const ElementPtr& child = _children[i];
                if (!child->Visible)
                {
                    child->Arrange(Rect{});
                    continue;
                }
                if (LastChildFill && i + 1 == count)
                {
                    child->Arrange(remaining);
                    break;
                }
                const Size desired = child->Desired();
                switch (child->DockSide)
                {
                case Dock::Left:
                    child->Arrange(Rect{remaining.X, remaining.Y,
                        std::min(desired.Width, remaining.Width), remaining.Height});
                    remaining.X += std::min(desired.Width, remaining.Width);
                    remaining.Width
                        = std::max(0.0, remaining.Width - desired.Width);
                    break;
                case Dock::Right:
                    child->Arrange(Rect{
                        remaining.X + remaining.Width
                            - std::min(desired.Width, remaining.Width),
                        remaining.Y, std::min(desired.Width, remaining.Width),
                        remaining.Height});
                    remaining.Width
                        = std::max(0.0, remaining.Width - desired.Width);
                    break;
                case Dock::Top:
                    child->Arrange(Rect{remaining.X, remaining.Y, remaining.Width,
                        std::min(desired.Height, remaining.Height)});
                    remaining.Y += std::min(desired.Height, remaining.Height);
                    remaining.Height
                        = std::max(0.0, remaining.Height - desired.Height);
                    break;
                case Dock::Bottom:
                    child->Arrange(Rect{remaining.X,
                        remaining.Y + remaining.Height
                            - std::min(desired.Height, remaining.Height),
                        remaining.Width, std::min(desired.Height, remaining.Height)});
                    remaining.Height
                        = std::max(0.0, remaining.Height - desired.Height);
                    break;
                }
            }
            break;
        }

        case ElementKind::WrapPanel:
        {
            double x = 0.0;
            double y = 0.0;
            double lineHeight = 0.0;
            for (const ElementPtr& child : _children)
            {
                if (!child->Visible)
                {
                    child->Arrange(Rect{});
                    continue;
                }
                const Size desired = child->Desired();
                if (x > 0.0 && x + desired.Width > content.Width)
                {
                    x = 0.0;
                    y += lineHeight;
                    lineHeight = 0.0;
                }
                child->Arrange(Rect{content.X + x, content.Y + y, desired.Width,
                    desired.Height});
                x += desired.Width;
                lineHeight = std::max(lineHeight, desired.Height);
            }
            break;
        }

        case ElementKind::StackPanel:
        {
            double offset = 0.0;
            bool first = true;
            for (const ElementPtr& child : _children)
            {
                if (!child->Visible)
                {
                    child->Arrange(Rect{});
                    continue;
                }
                if (!first)
                {
                    offset += Spacing;
                }
                first = false;
                if (StackOrientation == Orientation::Vertical)
                {
                    child->Arrange(Rect{content.X, content.Y + offset,
                        content.Width, child->Desired().Height});
                    offset += child->Desired().Height;
                }
                else
                {
                    child->Arrange(Rect{content.X + offset, content.Y,
                        child->Desired().Width, content.Height});
                    offset += child->Desired().Width;
                }
            }
            break;
        }

        case ElementKind::Grid:
        {
            const std::vector<double> columns
                = MeasureGridTrack(ColumnDefinitions, content.Width, true);
            const std::vector<double> rows
                = MeasureGridTrack(RowDefinitions, content.Height, false);
            for (const ElementPtr& child : _children)
            {
                if (!child->Visible)
                {
                    child->Arrange(Rect{});
                    continue;
                }
                const std::size_t column = std::min(
                    static_cast<std::size_t>(child->GridColumn < 0 ? 0 : child->GridColumn),
                    columns.empty() ? 0 : columns.size() - 1);
                const std::size_t row = std::min(
                    static_cast<std::size_t>(child->GridRow < 0 ? 0 : child->GridRow),
                    rows.empty() ? 0 : rows.size() - 1);
                double x = content.X;
                for (std::size_t i = 0; i < column; ++i) { x += columns[i]; }
                double y = content.Y;
                for (std::size_t i = 0; i < row; ++i) { y += rows[i]; }
                double height = rows.empty() ? content.Height : rows[row];
                for (std::size_t i = row + 1;
                    i < rows.size()
                    && i < row + static_cast<std::size_t>(
                        child->GridRowSpan < 1 ? 1 : child->GridRowSpan);
                    ++i)
                {
                    height += rows[i];
                }
                child->Arrange(Rect{x, y,
                    columns.empty() ? content.Width : columns[column], height});
            }
            break;
        }

        case ElementKind::ScrollViewer:
        {
            for (const ElementPtr& child : _children)
            {
                child->Arrange(Rect{content.X, content.Y - ScrollOffset,
                    content.Width, child->Desired().Height});
            }
            break;
        }

        case ElementKind::Panel:
        case ElementKind::Border:
        default:
            for (const ElementPtr& child : _children)
            {
                child->Arrange(content);
            }
            break;
        }
    }

    Element* Element::HitTest(double x, double y)
    {
        if (!Visible || !_bounds.Contains(x, y))
        {
            return nullptr;
        }
        // A scaled subtree is laid out at full size, so the point has to be
        // put back into the child's own coordinates.
        double childX = x;
        double childY = y;
        if (LayoutScale > 0.0 && LayoutScale != 1.0)
        {
            childX = _bounds.X + (x - _bounds.X) / LayoutScale;
            childY = _bounds.Y + (y - _bounds.Y) / LayoutScale;
        }
        // Topmost first, as the later child draws over the earlier one.
        for (std::size_t i = _children.size(); i-- > 0;)
        {
            if (Element* const hit = _children[i]->HitTest(childX, childY))
            {
                return hit;
            }
        }
        // A control the pointer can reach at all: one that reacts to it, one
        // that takes focus, or one drawing its own hover state.
        const bool interactive = PointerPressed || PointerReleased || PointerEntered
            || PointerExited || PointerMoved || Focusable
            || Kind() == ElementKind::Custom;
        return interactive ? this : nullptr;
    }

    std::vector<GridLength> ParseGridDefinitions(std::string_view text)
    {
        std::vector<GridLength> result;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find(',', start);
            if (end == std::string_view::npos)
            {
                end = text.size();
            }
            std::string_view item = text.substr(start, end - start);
            while (!item.empty() && (item.front() == ' ' || item.front() == '\t'))
            {
                item.remove_prefix(1);
            }
            while (!item.empty() && (item.back() == ' ' || item.back() == '\t'))
            {
                item.remove_suffix(1);
            }
            if (!item.empty())
            {
                GridLength length;
                if (item == "Auto" || item == "auto")
                {
                    length.Kind = GridLength::Unit::Auto;
                }
                else if (item.back() == '*')
                {
                    length.Kind = GridLength::Unit::Star;
                    item.remove_suffix(1);
                    length.Value = item.empty() ? 1.0 : std::strtod(std::string(item).c_str(), nullptr);
                }
                else
                {
                    length.Kind = GridLength::Unit::Pixel;
                    length.Value = std::strtod(std::string(item).c_str(), nullptr);
                }
                result.push_back(length);
            }
            if (end == text.size())
            {
                break;
            }
            start = end + 1;
        }
        return result;
    }
}
