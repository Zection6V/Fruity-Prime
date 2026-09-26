#include "Panels.hpp"

#include "Text.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace MphRead::NativeRuntime::Avalonia::Controls
{
    namespace
    {
        constexpr double Infinity = std::numeric_limits<double>::infinity();

        [[nodiscard]] Layout::Layoutable* AsLayoutable(const std::shared_ptr<Visual>& v)
        {
            return dynamic_cast<Layout::Layoutable*>(v.get());
        }
    }

    // ----------------------------------------------------------- children

    void PanelChildren::Add(const ControlPtr& control)
    {
        Insert(_items.size(), control);
    }

    void PanelChildren::Insert(std::size_t index, const ControlPtr& control)
    {
        if (control == nullptr)
        {
            return;
        }
        index = std::min(index, _items.size());
        _items.insert(_items.begin() + static_cast<std::ptrdiff_t>(index), control);
        _owner.AttachChild(index, control);
        _owner.ChildrenChanged();
    }

    bool PanelChildren::Remove(const ControlPtr& control)
    {
        const auto found = std::find(_items.begin(), _items.end(), control);
        if (found == _items.end())
        {
            return false;
        }
        ControlPtr keep = *found;
        _items.erase(found);
        _owner.DetachChild(keep);
        _owner.ChildrenChanged();
        return true;
    }

    void PanelChildren::RemoveAt(std::size_t index)
    {
        if (index < _items.size())
        {
            Remove(_items[index]);
        }
    }

    void PanelChildren::Clear()
    {
        const std::vector<ControlPtr> items = std::move(_items);
        _items.clear();
        for (const ControlPtr& item : items)
        {
            _owner.DetachChild(item);
        }
        _owner.ChildrenChanged();
    }

    void PanelChildren::AddRange(const std::vector<ControlPtr>& controls)
    {
        for (const ControlPtr& control : controls)
        {
            Add(control);
        }
    }

    std::ptrdiff_t PanelChildren::IndexOf(const Control* control) const
    {
        for (std::size_t i = 0; i < _items.size(); i++)
        {
            if (_items[i].get() == control)
            {
                return static_cast<std::ptrdiff_t>(i);
            }
        }
        return -1;
    }

    // -------------------------------------------------------------- Panel

    StyledProperty<Media::IBrushPtr>& Panel::BackgroundProperty = Register<Panel, Media::IBrushPtr>("Background", nullptr);

    Panel::Panel()
    {
        static const bool registered = []
        {
            AffectsRender<Panel>(BackgroundProperty);
            return true;
        }();
        (void)registered;
    }

    void Panel::AttachChild(std::size_t index, const ControlPtr& child)
    {
        InsertVisualChild(index, child);
    }

    void Panel::DetachChild(const ControlPtr& child)
    {
        RemoveVisualChild(child);
    }

    void Panel::Render(Media::DrawingContext& context)
    {
        if (const Media::IBrushPtr background = Background())
        {
            context.FillRectangle(background, Rect(Bounds().GetSize()));
        }
    }

    // --------------------------------------------------------- StackPanel

    StyledProperty<Layout::Orientation>& StackPanel::OrientationProperty
        = Register<StackPanel, Layout::Orientation>("Orientation", Layout::Orientation::Vertical);
    StyledProperty<double>& StackPanel::SpacingProperty = Register<StackPanel, double>("Spacing", 0.0);

    StackPanel::StackPanel()
    {
        static const bool registered = []
        {
            AffectsMeasure<StackPanel>(OrientationProperty, SpacingProperty);
            return true;
        }();
        (void)registered;
    }

    Size StackPanel::MeasureOverride(Size availableSize)
    {
        const bool horizontal = Orientation() == Layout::Orientation::Horizontal;
        const double spacing = Spacing();
        const Size childAvailable = horizontal ? Size{Infinity, availableSize.Height} : Size{availableSize.Width, Infinity};
        double along = 0;
        double across = 0;
        bool anyVisible = false;
        for (const ControlPtr& child : Children)
        {
            child->Measure(childAvailable);
            if (!child->IsVisible())
            {
                continue;
            }
            const Size desired = child->DesiredSize();
            along += (horizontal ? desired.Width : desired.Height) + spacing;
            across = std::max(across, horizontal ? desired.Height : desired.Width);
            anyVisible = true;
        }
        if (anyVisible)
        {
            along -= spacing;
        }
        return horizontal ? Size{along, across} : Size{across, along};
    }

    Size StackPanel::ArrangeOverride(Size finalSize)
    {
        const bool horizontal = Orientation() == Layout::Orientation::Horizontal;
        const double spacing = Spacing();
        double offset = 0;
        for (const ControlPtr& child : Children)
        {
            if (!child->IsVisible())
            {
                continue;
            }
            const Size desired = child->DesiredSize();
            if (horizontal)
            {
                child->Arrange(Rect{offset, 0, desired.Width, std::max(finalSize.Height, desired.Height)});
                offset += desired.Width + spacing;
            }
            else
            {
                child->Arrange(Rect{0, offset, std::max(finalSize.Width, desired.Width), desired.Height});
                offset += desired.Height + spacing;
            }
        }
        return finalSize;
    }

    // --------------------------------------------------------------- Grid

    std::vector<GridLength> ParseGridLengths(std::string_view text)
    {
        std::vector<GridLength> lengths;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find_first_of(", ", start);
            if (end == std::string_view::npos)
            {
                end = text.size();
            }
            std::string_view token = text.substr(start, end - start);
            start = end + 1;
            while (!token.empty() && token.front() == ' ')
            {
                token.remove_prefix(1);
            }
            while (!token.empty() && token.back() == ' ')
            {
                token.remove_suffix(1);
            }
            if (token.empty())
            {
                if (end >= text.size())
                {
                    break;
                }
                continue;
            }
            if (token == "Auto" || token == "auto")
            {
                lengths.push_back(GridLength::Auto());
            }
            else if (token.back() == '*')
            {
                const std::string_view number = token.substr(0, token.size() - 1);
                lengths.emplace_back(number.empty() ? 1.0 : std::stod(std::string(number)), GridUnitType::Star);
            }
            else
            {
                lengths.emplace_back(std::stod(std::string(token)), GridUnitType::Pixel);
            }
            if (end >= text.size())
            {
                break;
            }
        }
        return lengths;
    }

    ColumnDefinitions::ColumnDefinitions(std::string_view text)
    {
        for (const GridLength& length : ParseGridLengths(text))
        {
            Items.emplace_back(length);
        }
    }

    RowDefinitions::RowDefinitions(std::string_view text)
    {
        for (const GridLength& length : ParseGridLengths(text))
        {
            Items.emplace_back(length);
        }
    }

    AttachedProperty<std::int32_t>& Grid::RowProperty = RegisterAttached<Grid, std::int32_t>("Row", 0);
    AttachedProperty<std::int32_t>& Grid::ColumnProperty = RegisterAttached<Grid, std::int32_t>("Column", 0);
    AttachedProperty<std::int32_t>& Grid::RowSpanProperty = RegisterAttached<Grid, std::int32_t>("RowSpan", 1);
    AttachedProperty<std::int32_t>& Grid::ColumnSpanProperty = RegisterAttached<Grid, std::int32_t>("ColumnSpan", 1);
    StyledProperty<double>& Grid::RowSpacingProperty = Register<Grid, double>("RowSpacing", 0.0);
    StyledProperty<double>& Grid::ColumnSpacingProperty = Register<Grid, double>("ColumnSpacing", 0.0);

    Grid::Grid()
    {
        static const bool registered = []
        {
            AffectsMeasure<Grid>(RowSpacingProperty, ColumnSpacingProperty);
            return true;
        }();
        (void)registered;
    }

    namespace
    {
        struct Cell final
        {
            Control* Child;
            std::size_t Column;
            std::size_t Row;
            std::size_t ColumnSpan;
            std::size_t RowSpan;
        };

        // One axis of a grid, resolved: what each definition comes to.
        [[nodiscard]] std::vector<double> ResolveTrack(const std::vector<GridLength>& lengths,
            const std::vector<double>& minimums, const std::vector<double>& maximums, const std::vector<double>& autoSizes,
            const std::vector<double>& starDesired, double available, double spacing, bool forArrange)
        {
            const std::size_t n = lengths.size();
            std::vector<double> sizes(n, 0.0);
            double fixedTotal = spacing * static_cast<double>(n > 0 ? n - 1 : 0);
            double starWeight = 0;
            for (std::size_t i = 0; i < n; i++)
            {
                if (lengths[i].IsAbsolute())
                {
                    sizes[i] = std::clamp(lengths[i].Value, minimums[i], maximums[i]);
                    fixedTotal += sizes[i];
                }
                else if (lengths[i].IsAuto() || (!forArrange && !std::isfinite(available)))
                {
                    sizes[i] = std::clamp(lengths[i].IsAuto() ? autoSizes[i] : starDesired[i], minimums[i], maximums[i]);
                    fixedTotal += sizes[i];
                }
                else
                {
                    starWeight += lengths[i].Value;
                }
            }
            if (starWeight > 0)
            {
                if (std::isfinite(available))
                {
                    const double remaining = std::max(0.0, available - fixedTotal);
                    for (std::size_t i = 0; i < n; i++)
                    {
                        if (lengths[i].IsStar())
                        {
                            sizes[i] = std::clamp(remaining * lengths[i].Value / starWeight, minimums[i], maximums[i]);
                        }
                    }
                }
                else
                {
                    // Unconstrained: stars keep their proportions at the
                    // size the largest of them needs, as WPF's grid does.
                    double ratio = 0;
                    for (std::size_t i = 0; i < n; i++)
                    {
                        if (lengths[i].IsStar() && lengths[i].Value > 0)
                        {
                            ratio = std::max(ratio, starDesired[i] / lengths[i].Value);
                        }
                    }
                    for (std::size_t i = 0; i < n; i++)
                    {
                        if (lengths[i].IsStar())
                        {
                            sizes[i] = std::clamp(ratio * lengths[i].Value, minimums[i], maximums[i]);
                        }
                    }
                }
            }
            return sizes;
        }
    }

    Size Grid::MeasureOverride(Size availableSize)
    {
        std::vector<GridLength> columns;
        std::vector<double> columnMin;
        std::vector<double> columnMax;
        for (const ColumnDefinition& c : _columns.Items)
        {
            columns.push_back(c.Length);
            columnMin.push_back(c.MinLength);
            columnMax.push_back(c.MaxLength);
        }
        if (columns.empty())
        {
            columns.push_back(GridLength::Star());
            columnMin.push_back(0);
            columnMax.push_back(Infinity);
        }
        std::vector<GridLength> rows;
        std::vector<double> rowMin;
        std::vector<double> rowMax;
        for (const RowDefinition& r : _rows.Items)
        {
            rows.push_back(r.Length);
            rowMin.push_back(r.MinLength);
            rowMax.push_back(r.MaxLength);
        }
        if (rows.empty())
        {
            rows.push_back(GridLength::Star());
            rowMin.push_back(0);
            rowMax.push_back(Infinity);
        }
        const double columnSpacing = ColumnSpacing();
        const double rowSpacing = RowSpacing();
        std::vector<Cell> cells;
        for (const ControlPtr& child : Children)
        {
            const std::size_t column = static_cast<std::size_t>(std::clamp(GetColumn(*child), 0,
                static_cast<std::int32_t>(columns.size()) - 1));
            const std::size_t row = static_cast<std::size_t>(std::clamp(GetRow(*child), 0,
                static_cast<std::int32_t>(rows.size()) - 1));
            const std::size_t columnSpan = std::min<std::size_t>(
                static_cast<std::size_t>(std::max(1, child->GetValue(ColumnSpanProperty))), columns.size() - column);
            const std::size_t rowSpan = std::min<std::size_t>(
                static_cast<std::size_t>(std::max(1, child->GetValue(RowSpanProperty))), rows.size() - row);
            cells.push_back(Cell{child.get(), column, row, columnSpan, rowSpan});
        }
        const auto spansOnly = [](const std::vector<GridLength>& lengths, std::size_t start, std::size_t span,
                                   GridUnitType type)
        {
            for (std::size_t i = start; i < start + span; i++)
            {
                if (lengths[i].GridUnitType != type)
                {
                    return false;
                }
            }
            return true;
        };
        const auto hasStar = [](const std::vector<GridLength>& lengths, std::size_t start, std::size_t span)
        {
            for (std::size_t i = start; i < start + span; i++)
            {
                if (lengths[i].IsStar())
                {
                    return true;
                }
            }
            return false;
        };
        // Columns: auto and (unconstrained) star columns from the children
        // measured at an unlimited width.
        std::vector<double> columnAuto(columns.size(), 0.0);
        std::vector<double> columnStar(columns.size(), 0.0);
        std::vector<double> rowAuto(rows.size(), 0.0);
        std::vector<double> rowStar(rows.size(), 0.0);
        const bool widthFinite = std::isfinite(availableSize.Width);
        for (const Cell& cell : cells)
        {
            if (hasStar(columns, cell.Column, cell.ColumnSpan) && widthFinite)
            {
                continue;
            }
            // Measured with the pixel width if that is all it spans.
            double width = Infinity;
            if (spansOnly(columns, cell.Column, cell.ColumnSpan, GridUnitType::Pixel))
            {
                width = columnSpacing * static_cast<double>(cell.ColumnSpan - 1);
                for (std::size_t i = cell.Column; i < cell.Column + cell.ColumnSpan; i++)
                {
                    width += columns[i].Value;
                }
            }
            cell.Child->Measure({width, Infinity});
            const double desired = cell.Child->DesiredSize().Width;
            if (cell.ColumnSpan == 1)
            {
                if (columns[cell.Column].IsAuto())
                {
                    columnAuto[cell.Column] = std::max(columnAuto[cell.Column], desired);
                }
                else if (columns[cell.Column].IsStar())
                {
                    columnStar[cell.Column] = std::max(columnStar[cell.Column], desired);
                }
            }
        }
        // Spanning children widen the last auto column they span if they do
        // not fit in what the rest came to.
        for (const Cell& cell : cells)
        {
            if (cell.ColumnSpan <= 1 || (hasStar(columns, cell.Column, cell.ColumnSpan) && widthFinite))
            {
                continue;
            }
            double current = columnSpacing * static_cast<double>(cell.ColumnSpan - 1);
            std::ptrdiff_t lastAuto = -1;
            for (std::size_t i = cell.Column; i < cell.Column + cell.ColumnSpan; i++)
            {
                current += columns[i].IsAbsolute() ? columns[i].Value
                    : columns[i].IsAuto() ? columnAuto[i] : columnStar[i];
                if (!columns[i].IsAbsolute())
                {
                    lastAuto = static_cast<std::ptrdiff_t>(i);
                }
            }
            const double desired = cell.Child->DesiredSize().Width;
            if (lastAuto >= 0 && desired > current)
            {
                auto& target = columns[static_cast<std::size_t>(lastAuto)].IsAuto()
                    ? columnAuto[static_cast<std::size_t>(lastAuto)]
                    : columnStar[static_cast<std::size_t>(lastAuto)];
                target += desired - current;
            }
        }
        _columnSizes = ResolveTrack(columns, columnMin, columnMax, columnAuto, columnStar, availableSize.Width,
            columnSpacing, false);
        // Rows: every child measured at the width its columns came to.
        const auto spanWidth = [&](const Cell& cell)
        {
            double width = columnSpacing * static_cast<double>(cell.ColumnSpan - 1);
            for (std::size_t i = cell.Column; i < cell.Column + cell.ColumnSpan; i++)
            {
                width += _columnSizes[i];
            }
            return width;
        };
        const bool heightFinite = std::isfinite(availableSize.Height);
        for (const Cell& cell : cells)
        {
            double height = Infinity;
            if (spansOnly(rows, cell.Row, cell.RowSpan, GridUnitType::Pixel))
            {
                height = rowSpacing * static_cast<double>(cell.RowSpan - 1);
                for (std::size_t i = cell.Row; i < cell.Row + cell.RowSpan; i++)
                {
                    height += rows[i].Value;
                }
            }
            cell.Child->Measure({spanWidth(cell), height});
            const double desired = cell.Child->DesiredSize().Height;
            if (cell.RowSpan == 1)
            {
                if (rows[cell.Row].IsAuto())
                {
                    rowAuto[cell.Row] = std::max(rowAuto[cell.Row], desired);
                }
                else if (rows[cell.Row].IsStar())
                {
                    rowStar[cell.Row] = std::max(rowStar[cell.Row], desired);
                }
            }
        }
        for (const Cell& cell : cells)
        {
            if (cell.RowSpan <= 1 || (hasStar(rows, cell.Row, cell.RowSpan) && heightFinite))
            {
                continue;
            }
            double current = rowSpacing * static_cast<double>(cell.RowSpan - 1);
            std::ptrdiff_t lastAuto = -1;
            for (std::size_t i = cell.Row; i < cell.Row + cell.RowSpan; i++)
            {
                current += rows[i].IsAbsolute() ? rows[i].Value : rows[i].IsAuto() ? rowAuto[i] : rowStar[i];
                if (!rows[i].IsAbsolute())
                {
                    lastAuto = static_cast<std::ptrdiff_t>(i);
                }
            }
            const double desired = cell.Child->DesiredSize().Height;
            if (lastAuto >= 0 && desired > current)
            {
                auto& target = rows[static_cast<std::size_t>(lastAuto)].IsAuto() ? rowAuto[static_cast<std::size_t>(lastAuto)]
                                                                                 : rowStar[static_cast<std::size_t>(lastAuto)];
                target += desired - current;
            }
        }
        _rowSizes = ResolveTrack(rows, rowMin, rowMax, rowAuto, rowStar, availableSize.Height, rowSpacing, false);
        // What the grid wants: the tracks at their natural sizes, stars at
        // what their children need.
        const std::vector<double> desiredColumns = ResolveTrack(columns, columnMin, columnMax, columnAuto, columnStar,
            Infinity, columnSpacing, false);
        const std::vector<double> desiredRows = ResolveTrack(rows, rowMin, rowMax, rowAuto, rowStar, Infinity, rowSpacing,
            false);
        double width = std::accumulate(desiredColumns.begin(), desiredColumns.end(), 0.0)
            + columnSpacing * static_cast<double>(columns.size() - 1);
        double height = std::accumulate(desiredRows.begin(), desiredRows.end(), 0.0)
            + rowSpacing * static_cast<double>(rows.size() - 1);
        return {width, height};
    }

    Size Grid::ArrangeOverride(Size finalSize)
    {
        std::vector<GridLength> columns;
        std::vector<double> columnMin;
        std::vector<double> columnMax;
        for (const ColumnDefinition& c : _columns.Items)
        {
            columns.push_back(c.Length);
            columnMin.push_back(c.MinLength);
            columnMax.push_back(c.MaxLength);
        }
        if (columns.empty())
        {
            columns.push_back(GridLength::Star());
            columnMin.push_back(0);
            columnMax.push_back(Infinity);
        }
        std::vector<GridLength> rows;
        std::vector<double> rowMin;
        std::vector<double> rowMax;
        for (const RowDefinition& r : _rows.Items)
        {
            rows.push_back(r.Length);
            rowMin.push_back(r.MinLength);
            rowMax.push_back(r.MaxLength);
        }
        if (rows.empty())
        {
            rows.push_back(GridLength::Star());
            rowMin.push_back(0);
            rowMax.push_back(Infinity);
        }
        // Auto tracks keep what measure found; stars share what is left.
        std::vector<double> columnAuto(columns.size(), 0.0);
        std::vector<double> rowAuto(rows.size(), 0.0);
        for (std::size_t i = 0; i < columns.size() && i < _columnSizes.size(); i++)
        {
            columnAuto[i] = _columnSizes[i];
        }
        for (std::size_t i = 0; i < rows.size() && i < _rowSizes.size(); i++)
        {
            rowAuto[i] = _rowSizes[i];
        }
        const std::vector<double> columnSizes = ResolveTrack(columns, columnMin, columnMax, columnAuto, columnAuto,
            finalSize.Width, ColumnSpacing(), true);
        const std::vector<double> rowSizes = ResolveTrack(rows, rowMin, rowMax, rowAuto, rowAuto, finalSize.Height,
            RowSpacing(), true);
        for (std::size_t i = 0; i < _columns.Items.size() && i < columnSizes.size(); i++)
        {
            _columns.Items[i].ActualLength = columnSizes[i];
        }
        for (std::size_t i = 0; i < _rows.Items.size() && i < rowSizes.size(); i++)
        {
            _rows.Items[i].ActualLength = rowSizes[i];
        }
        const double columnSpacing = ColumnSpacing();
        const double rowSpacing = RowSpacing();
        for (const ControlPtr& child : Children)
        {
            const std::size_t column = static_cast<std::size_t>(std::clamp(GetColumn(*child), 0,
                static_cast<std::int32_t>(columns.size()) - 1));
            const std::size_t row = static_cast<std::size_t>(std::clamp(GetRow(*child), 0,
                static_cast<std::int32_t>(rows.size()) - 1));
            const std::size_t columnSpan = std::min<std::size_t>(
                static_cast<std::size_t>(std::max(1, child->GetValue(ColumnSpanProperty))), columns.size() - column);
            const std::size_t rowSpan = std::min<std::size_t>(
                static_cast<std::size_t>(std::max(1, child->GetValue(RowSpanProperty))), rows.size() - row);
            double x = 0;
            for (std::size_t i = 0; i < column; i++)
            {
                x += columnSizes[i] + columnSpacing;
            }
            double y = 0;
            for (std::size_t i = 0; i < row; i++)
            {
                y += rowSizes[i] + rowSpacing;
            }
            double width = columnSpacing * static_cast<double>(columnSpan - 1);
            for (std::size_t i = column; i < column + columnSpan; i++)
            {
                width += columnSizes[i];
            }
            double height = rowSpacing * static_cast<double>(rowSpan - 1);
            for (std::size_t i = row; i < row + rowSpan; i++)
            {
                height += rowSizes[i];
            }
            child->Arrange(Rect{x, y, width, height});
        }
        return finalSize;
    }

    // ---------------------------------------------------------- DockPanel

    AttachedProperty<Dock>& DockPanel::DockProperty = RegisterAttached<DockPanel, Dock>("Dock", Dock::Left);
    StyledProperty<bool>& DockPanel::LastChildFillProperty = Register<DockPanel, bool>("LastChildFill", true);

    DockPanel::DockPanel()
    {
        static const bool registered = []
        {
            AffectsMeasure<DockPanel>(LastChildFillProperty);
            return true;
        }();
        (void)registered;
    }

    Size DockPanel::MeasureOverride(Size availableSize)
    {
        double usedWidth = 0;
        double usedHeight = 0;
        double maxWidth = 0;
        double maxHeight = 0;
        for (const ControlPtr& child : Children)
        {
            child->Measure({std::max(0.0, availableSize.Width - usedWidth), std::max(0.0, availableSize.Height - usedHeight)});
            const Size desired = child->DesiredSize();
            switch (GetDock(*child))
            {
            case Dock::Left:
            case Dock::Right:
                maxHeight = std::max(maxHeight, usedHeight + desired.Height);
                usedWidth += desired.Width;
                break;
            case Dock::Top:
            case Dock::Bottom:
                maxWidth = std::max(maxWidth, usedWidth + desired.Width);
                usedHeight += desired.Height;
                break;
            }
        }
        return {std::max(maxWidth, usedWidth), std::max(maxHeight, usedHeight)};
    }

    Size DockPanel::ArrangeOverride(Size finalSize)
    {
        double left = 0;
        double top = 0;
        double right = 0;
        double bottom = 0;
        const std::size_t count = Children.Count();
        const std::size_t fillIndex = LastChildFill() && count > 0 ? count - 1 : count;
        for (std::size_t i = 0; i < count; i++)
        {
            const ControlPtr& child = Children[i];
            const Size desired = child->DesiredSize();
            Rect rect{left, top, std::max(0.0, finalSize.Width - left - right),
                std::max(0.0, finalSize.Height - top - bottom)};
            if (i < fillIndex)
            {
                switch (GetDock(*child))
                {
                case Dock::Left:
                    left += desired.Width;
                    rect.Width = desired.Width;
                    break;
                case Dock::Right:
                    right += desired.Width;
                    rect.X = std::max(0.0, finalSize.Width - right);
                    rect.Width = desired.Width;
                    break;
                case Dock::Top:
                    top += desired.Height;
                    rect.Height = desired.Height;
                    break;
                case Dock::Bottom:
                    bottom += desired.Height;
                    rect.Y = std::max(0.0, finalSize.Height - bottom);
                    rect.Height = desired.Height;
                    break;
                }
            }
            child->Arrange(rect);
        }
        return finalSize;
    }

    // ------------------------------------------------------------- Canvas

    AttachedProperty<double>& Canvas::LeftProperty
        = RegisterAttached<Canvas, double>("Left", std::numeric_limits<double>::quiet_NaN());
    AttachedProperty<double>& Canvas::TopProperty
        = RegisterAttached<Canvas, double>("Top", std::numeric_limits<double>::quiet_NaN());
    AttachedProperty<double>& Canvas::RightProperty
        = RegisterAttached<Canvas, double>("Right", std::numeric_limits<double>::quiet_NaN());
    AttachedProperty<double>& Canvas::BottomProperty
        = RegisterAttached<Canvas, double>("Bottom", std::numeric_limits<double>::quiet_NaN());

    Canvas::Canvas() = default;

    Size Canvas::MeasureOverride(Size availableSize)
    {
        (void)availableSize;
        for (const ControlPtr& child : Children)
        {
            child->Measure(Size::Infinity());
        }
        return {};
    }

    Size Canvas::ArrangeOverride(Size finalSize)
    {
        for (const ControlPtr& child : Children)
        {
            const Size desired = child->DesiredSize();
            double x = 0;
            double y = 0;
            if (const double left = GetLeft(*child); !std::isnan(left))
            {
                x = left;
            }
            else if (const double right = child->GetValue(RightProperty); !std::isnan(right))
            {
                x = finalSize.Width - desired.Width - right;
            }
            if (const double top = GetTop(*child); !std::isnan(top))
            {
                y = top;
            }
            else if (const double bottom = child->GetValue(BottomProperty); !std::isnan(bottom))
            {
                y = finalSize.Height - desired.Height - bottom;
            }
            child->Arrange(Rect{x, y, desired.Width, desired.Height});
        }
        return finalSize;
    }

    // ---------------------------------------------------------- Decorator

    StyledProperty<ControlPtr>& Decorator::ChildProperty = Register<Decorator, ControlPtr>("Child", nullptr);
    StyledProperty<Thickness>& Decorator::PaddingProperty = Register<Decorator, Thickness>("Padding", Thickness{});

    Decorator::Decorator()
    {
        static const bool registered = []
        {
            AffectsMeasure<Decorator>(ChildProperty, PaddingProperty);
            return true;
        }();
        (void)registered;
    }

    void Decorator::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        if (&change.Property == &ChildProperty)
        {
            if (const ControlPtr old = change.GetOldValue<ControlPtr>())
            {
                RemoveVisualChild(old);
            }
            if (const ControlPtr now = change.GetNewValue<ControlPtr>())
            {
                AddVisualChild(now);
            }
        }
        Control::OnPropertyChanged(change);
    }

    Size Decorator::MeasureOverride(Size availableSize)
    {
        const Thickness padding = Padding();
        const ControlPtr child = Child();
        if (child == nullptr)
        {
            return Size{}.Inflate(padding);
        }
        child->Measure(availableSize.Deflate(padding));
        return child->DesiredSize().Inflate(padding);
    }

    Size Decorator::ArrangeOverride(Size finalSize)
    {
        if (const ControlPtr child = Child())
        {
            child->Arrange(Rect(finalSize).Deflate(Padding()));
        }
        return finalSize;
    }

    // ------------------------------------------------------------- Border

    StyledProperty<Media::IBrushPtr>& Border::BackgroundProperty = Register<Border, Media::IBrushPtr>("Background", nullptr);
    StyledProperty<Media::IBrushPtr>& Border::BorderBrushProperty
        = Register<Border, Media::IBrushPtr>("BorderBrush", nullptr);
    StyledProperty<Thickness>& Border::BorderThicknessProperty = Register<Border, Thickness>("BorderThickness", Thickness{});
    StyledProperty<Avalonia::CornerRadius>& Border::CornerRadiusProperty
        = Register<Border, Avalonia::CornerRadius>("CornerRadius", Avalonia::CornerRadius{});
    StyledProperty<Media::BoxShadows>& Border::BoxShadowProperty
        = Register<Border, Media::BoxShadows>("BoxShadow", Media::BoxShadows{});

    Border::Border()
    {
        static const bool registered = []
        {
            AffectsRender<Border>(BackgroundProperty, BorderBrushProperty, BorderThicknessProperty, CornerRadiusProperty,
                BoxShadowProperty);
            AffectsMeasure<Border>(BorderThicknessProperty);
            return true;
        }();
        (void)registered;
    }

    Size Border::MeasureOverride(Size availableSize)
    {
        const Thickness t = BorderThickness();
        const Thickness padding = Padding();
        const Thickness total{t.Left + padding.Left, t.Top + padding.Top, t.Right + padding.Right, t.Bottom + padding.Bottom};
        const ControlPtr child = Child();
        if (child == nullptr)
        {
            return Size{}.Inflate(total);
        }
        child->Measure(availableSize.Deflate(total));
        return child->DesiredSize().Inflate(total);
    }

    Size Border::ArrangeOverride(Size finalSize)
    {
        const Thickness t = BorderThickness();
        const Thickness padding = Padding();
        if (const ControlPtr child = Child())
        {
            child->Arrange(Rect(finalSize).Deflate(Thickness{t.Left + padding.Left, t.Top + padding.Top,
                t.Right + padding.Right, t.Bottom + padding.Bottom}));
        }
        return finalSize;
    }

    void RenderBorder(Media::DrawingContext& context, const Rect& bounds, const Media::IBrushPtr& background,
        const Media::IBrushPtr& borderBrush, const Thickness& thickness, const Avalonia::CornerRadius& radius,
        const Media::BoxShadows& shadows)
    {
        const bool hasBorder = borderBrush != nullptr
            && (thickness.Left > 0 || thickness.Top > 0 || thickness.Right > 0 || thickness.Bottom > 0);
        if (!hasBorder || thickness.IsUniform())
        {
            const double t = hasBorder ? thickness.Left : 0.0;
            // The stroke is centred on an edge inset by half its width.
            const Rect rect = bounds.Deflate(t / 2);
            const Avalonia::CornerRadius inner{std::max(0.0, radius.TopLeft - t / 2),
                std::max(0.0, radius.TopRight - t / 2), std::max(0.0, radius.BottomRight - t / 2),
                std::max(0.0, radius.BottomLeft - t / 2)};
            Media::IPenPtr pen = hasBorder ? std::make_shared<Media::Pen>(borderBrush, t) : nullptr;
            if (shadows.Count() > 0)
            {
                // Shadows are cast by the outer edge.
                context.DrawRectangle(nullptr, nullptr, bounds, radius, shadows);
            }
            context.DrawRectangle(background, pen, rect, inner);
            return;
        }
        // Uneven edges: the fill inside them, then each edge as a band.
        if (shadows.Count() > 0)
        {
            context.DrawRectangle(nullptr, nullptr, bounds, radius, shadows);
        }
        context.DrawRectangle(background, nullptr, bounds.Deflate(thickness), Avalonia::CornerRadius{});
        const double w = bounds.Width;
        const double h = bounds.Height;
        if (thickness.Top > 0)
        {
            context.FillRectangle(borderBrush, Rect{bounds.X, bounds.Y, w, thickness.Top});
        }
        if (thickness.Bottom > 0)
        {
            context.FillRectangle(borderBrush, Rect{bounds.X, bounds.Y + h - thickness.Bottom, w, thickness.Bottom});
        }
        if (thickness.Left > 0)
        {
            context.FillRectangle(borderBrush, Rect{bounds.X, bounds.Y + thickness.Top, thickness.Left,
                std::max(0.0, h - thickness.Top - thickness.Bottom)});
        }
        if (thickness.Right > 0)
        {
            context.FillRectangle(borderBrush, Rect{bounds.X + w - thickness.Right, bounds.Y + thickness.Top,
                thickness.Right, std::max(0.0, h - thickness.Top - thickness.Bottom)});
        }
    }

    void Border::Render(Media::DrawingContext& context)
    {
        RenderBorder(context, Rect(Bounds().GetSize()), Background(), BorderBrush(), BorderThickness(), CornerRadius(),
            BoxShadow());
    }

    // ----------------------------------------------------- ContentControl

    StyledProperty<ControlPtr>& ContentControl::ContentProperty = Register<ContentControl, ControlPtr>("Content", nullptr);
    StyledProperty<Layout::HorizontalAlignment>& ContentControl::HorizontalContentAlignmentProperty
        = Register<ContentControl, Layout::HorizontalAlignment>("HorizontalContentAlignment",
            Layout::HorizontalAlignment::Stretch);
    StyledProperty<Layout::VerticalAlignment>& ContentControl::VerticalContentAlignmentProperty
        = Register<ContentControl, Layout::VerticalAlignment>("VerticalContentAlignment", Layout::VerticalAlignment::Stretch);

    ContentControl::ContentControl()
    {
        static const bool registered = []
        {
            AffectsMeasure<ContentControl>(ContentProperty, PaddingProperty, BorderThicknessProperty);
            AffectsArrange<ContentControl>(HorizontalContentAlignmentProperty, VerticalContentAlignmentProperty);
            AffectsRender<ContentControl>(BackgroundProperty, BorderBrushProperty, CornerRadiusProperty);
            return true;
        }();
        (void)registered;
    }

    void ContentControl::Content(std::string_view text)
    {
        auto block = std::make_shared<TextBlock>();
        block->Text(std::string(text));
        Content(std::static_pointer_cast<Control>(block));
    }

    void ContentControl::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        if (&change.Property == &ContentProperty)
        {
            if (const ControlPtr old = change.GetOldValue<ControlPtr>())
            {
                RemoveVisualChild(old);
            }
            if (const ControlPtr now = change.GetNewValue<ControlPtr>())
            {
                AddVisualChild(now);
            }
        }
        TemplatedControl::OnPropertyChanged(change);
    }

    Size ContentControl::MeasureOverride(Size availableSize)
    {
        const Thickness t = BorderThickness();
        const Thickness p = Padding();
        const Thickness total{t.Left + p.Left, t.Top + p.Top, t.Right + p.Right, t.Bottom + p.Bottom};
        const ControlPtr content = Content();
        if (content == nullptr)
        {
            return Size{}.Inflate(total);
        }
        content->Measure(availableSize.Deflate(total));
        return content->DesiredSize().Inflate(total);
    }

    Size ContentControl::ArrangeOverride(Size finalSize)
    {
        const Thickness t = BorderThickness();
        const Thickness p = Padding();
        const Rect inner = Rect(finalSize).Deflate(Thickness{t.Left + p.Left, t.Top + p.Top, t.Right + p.Right,
            t.Bottom + p.Bottom});
        if (const ControlPtr content = Content())
        {
            // The presenter aligns its content within the inner box.
            const Size desired = content->DesiredSize();
            Rect rect = inner;
            switch (HorizontalContentAlignment())
            {
            case Layout::HorizontalAlignment::Left:
                rect.Width = std::min(desired.Width, inner.Width);
                break;
            case Layout::HorizontalAlignment::Center:
                rect.Width = std::min(desired.Width, inner.Width);
                rect.X += (inner.Width - rect.Width) / 2;
                break;
            case Layout::HorizontalAlignment::Right:
                rect.Width = std::min(desired.Width, inner.Width);
                rect.X += inner.Width - rect.Width;
                break;
            default:
                break;
            }
            switch (VerticalContentAlignment())
            {
            case Layout::VerticalAlignment::Top:
                rect.Height = std::min(desired.Height, inner.Height);
                break;
            case Layout::VerticalAlignment::Center:
                rect.Height = std::min(desired.Height, inner.Height);
                rect.Y += (inner.Height - rect.Height) / 2;
                break;
            case Layout::VerticalAlignment::Bottom:
                rect.Height = std::min(desired.Height, inner.Height);
                rect.Y += inner.Height - rect.Height;
                break;
            default:
                break;
            }
            content->Arrange(rect);
        }
        return finalSize;
    }

    void ContentControl::Render(Media::DrawingContext& context)
    {
        RenderBorder(context, Rect(Bounds().GetSize()), Background(), BorderBrush(), BorderThickness(), CornerRadius(), {});
    }

    // --------------------------------------------- LayoutTransformControl

    StyledProperty<Media::TransformPtr>& LayoutTransformControl::LayoutTransformProperty
        = Register<LayoutTransformControl, Media::TransformPtr>("LayoutTransform", nullptr);

    LayoutTransformControl::LayoutTransformControl()
    {
        static const bool registered = []
        {
            AffectsMeasure<LayoutTransformControl>(LayoutTransformProperty);
            return true;
        }();
        (void)registered;
    }

    void LayoutTransformControl::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        Decorator::OnPropertyChanged(change);
        if (&change.Property == &ChildProperty && change.GetNewValue<ControlPtr>() == nullptr)
        {
            // Avalonia 11.3 clears the transform with the child; UiSurface
            // puts it back, and depends on this happening.
            SetValue(LayoutTransformProperty, Media::TransformPtr{});
        }
    }

    Matrix LayoutTransformControl::VisualChildTransform() const
    {
        const Media::TransformPtr transform = LayoutTransform();
        return transform == nullptr ? Matrix::Identity() : transform->Value();
    }

    Size LayoutTransformControl::MeasureOverride(Size availableSize)
    {
        const ControlPtr child = Child();
        if (child == nullptr)
        {
            return {};
        }
        const Matrix m = VisualChildTransform();
        // Scale and translate only: measure through the inverse.
        const double sx = std::abs(m.M11) > 1e-9 ? std::abs(m.M11) : 1.0;
        const double sy = std::abs(m.M22) > 1e-9 ? std::abs(m.M22) : 1.0;
        child->Measure({availableSize.Width / sx, availableSize.Height / sy});
        const Rect transformed = TransformToAABB(Rect(child->DesiredSize()), m);
        return transformed.GetSize();
    }

    Size LayoutTransformControl::ArrangeOverride(Size finalSize)
    {
        if (const ControlPtr child = Child())
        {
            const Matrix m = VisualChildTransform();
            const double sx = std::abs(m.M11) > 1e-9 ? std::abs(m.M11) : 1.0;
            const double sy = std::abs(m.M22) > 1e-9 ? std::abs(m.M22) : 1.0;
            child->Arrange(Rect{0, 0, finalSize.Width / sx, finalSize.Height / sy});
        }
        return finalSize;
    }

    // -------------------------------------------------------------- Image

    StyledProperty<std::shared_ptr<Media::IImage>>& Image::SourceProperty
        = Register<Image, std::shared_ptr<Media::IImage>>("Source", nullptr);
    StyledProperty<Media::Stretch>& Image::StretchProperty = Register<Image, Media::Stretch>("Stretch", Media::Stretch::Uniform);
    StyledProperty<Media::StretchDirection>& Image::StretchDirectionProperty
        = Register<Image, Media::StretchDirection>("StretchDirection", Media::StretchDirection::Both);

    AttachedProperty<Media::BitmapInterpolationMode>& RenderOptions::BitmapInterpolationModeProperty
        = RegisterAttached<RenderOptions, Media::BitmapInterpolationMode>("BitmapInterpolationMode",
            Media::BitmapInterpolationMode::Unspecified);

    Image::Image()
    {
        static const bool registered = []
        {
            AffectsMeasure<Image>(SourceProperty, StretchProperty, StretchDirectionProperty);
            AffectsRender<Image>(SourceProperty, StretchProperty, StretchDirectionProperty);
            return true;
        }();
        (void)registered;
    }

    Size Image::MeasureOverride(Size availableSize)
    {
        const std::shared_ptr<Media::IImage> source = Source();
        if (source == nullptr)
        {
            return {};
        }
        const Size natural = source->Size();
        const Vector scale = Media::CalculateScaling(Stretch(), availableSize, natural, StretchDirection());
        return {natural.Width * scale.X, natural.Height * scale.Y};
    }

    Size Image::ArrangeOverride(Size finalSize)
    {
        const std::shared_ptr<Media::IImage> source = Source();
        if (source == nullptr)
        {
            return {};
        }
        const Size natural = source->Size();
        const Vector scale = Media::CalculateScaling(Stretch(), finalSize, natural, StretchDirection());
        return {natural.Width * scale.X, natural.Height * scale.Y};
    }

    void Image::Render(Media::DrawingContext& context)
    {
        const std::shared_ptr<Media::IImage> source = Source();
        const Rect bounds(Bounds().GetSize());
        if (source == nullptr || bounds.Width <= 0 || bounds.Height <= 0)
        {
            return;
        }
        const Size natural = source->Size();
        const Vector scale = Media::CalculateScaling(Stretch(), bounds.GetSize(), natural, StretchDirection());
        const Size scaled{natural.Width * scale.X, natural.Height * scale.Y};
        // Centred, and what falls outside the bounds cut from the source.
        const Rect dest{(bounds.Width - scaled.Width) / 2, (bounds.Height - scaled.Height) / 2, scaled.Width, scaled.Height};
        const Rect visible = dest.Intersect(bounds);
        const Rect sourceRect{(visible.X - dest.X) / scale.X, (visible.Y - dest.Y) / scale.Y, visible.Width / scale.X,
            visible.Height / scale.Y};
        Media::RenderOptions options;
        options.BitmapInterpolationMode = RenderOptions::GetBitmapInterpolationMode(*this);
        auto state = context.PushRenderOptions(options);
        context.DrawImage(*source, sourceRect, visible);
    }
}

namespace MphRead::NativeRuntime::Avalonia::Media
{
    Vector CalculateScaling(Stretch stretch, Size destinationSize, Size sourceSize, StretchDirection direction)
    {
        double scaleX = 1;
        double scaleY = 1;
        const bool widthInfinite = !std::isfinite(destinationSize.Width);
        const bool heightInfinite = !std::isfinite(destinationSize.Height);
        if (stretch != Stretch::None && (!widthInfinite || !heightInfinite) && sourceSize.Width > 0 && sourceSize.Height > 0)
        {
            scaleX = destinationSize.Width / sourceSize.Width;
            scaleY = destinationSize.Height / sourceSize.Height;
            if (widthInfinite)
            {
                scaleX = scaleY;
            }
            else if (heightInfinite)
            {
                scaleY = scaleX;
            }
            else
            {
                switch (stretch)
                {
                case Stretch::Uniform:
                    scaleX = scaleY = std::min(scaleX, scaleY);
                    break;
                case Stretch::UniformToFill:
                    scaleX = scaleY = std::max(scaleX, scaleY);
                    break;
                default:
                    break;
                }
            }
            switch (direction)
            {
            case StretchDirection::UpOnly:
                scaleX = std::max(1.0, scaleX);
                scaleY = std::max(1.0, scaleY);
                break;
            case StretchDirection::DownOnly:
                scaleX = std::min(1.0, scaleX);
                scaleY = std::min(1.0, scaleY);
                break;
            default:
                break;
            }
        }
        return {scaleX, scaleY};
    }
}
