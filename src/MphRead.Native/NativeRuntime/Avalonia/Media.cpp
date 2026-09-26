#include "Media.hpp"

#include "Platform.hpp"
#include "../System/Encoding.hpp"
#include "../System/IO.hpp"

#include <cmath>
#include <map>
#include <mutex>
#include <numbers>
#include <utility>

namespace MphRead::NativeRuntime::Avalonia::Media
{
    namespace Brushes
    {
        const IBrushPtr& Transparent()
        {
            static const IBrushPtr brush = std::make_shared<SolidColorBrush>(Colors::Transparent);
            return brush;
        }

        const IBrushPtr& Black()
        {
            static const IBrushPtr brush = std::make_shared<SolidColorBrush>(Colors::Black);
            return brush;
        }

        const IBrushPtr& White()
        {
            static const IBrushPtr brush = std::make_shared<SolidColorBrush>(Colors::White);
            return brush;
        }
    }

    std::u32string ToUtf32(std::string_view text)
    {
        return Utf8ToUtf32(text);
    }

    std::string ToUtf8(std::u32string_view text)
    {
        return Utf32ToUtf8(text);
    }

    // ------------------------------------------------------------ fonts

    FontFamily::FontFamily(std::string name)
        : _name(std::move(name))
    {
    }

    const std::shared_ptr<FontFamily>& FontFamily::Default()
    {
        static const std::shared_ptr<FontFamily> family = std::make_shared<FontFamily>("$Default");
        return family;
    }

    std::shared_ptr<Skia::Typeface> FontFamily::Resolve(FontWeight weight, FontStyle style) const
    {
        (void)style;
        if (!_loaded)
        {
            _loaded = true;
            if (_name.rfind("avares://", 0) == 0)
            {
                // Every face opened once for the process, as Avalonia's font
                // manager caches its typefaces.
                static std::map<std::string, std::shared_ptr<Skia::Typeface>> files;
                static std::mutex mutex;
                std::lock_guard lock(mutex);
                const std::string key = _name.substr(0, _name.find('#'));
                auto found = files.find(key);
                if (found == files.end())
                {
                    const std::optional<std::string> path = Platform::AssetLoader::Resolve(key);
                    found = files.emplace(key, path.has_value() ? Skia::Typeface::FromFile(*path) : nullptr).first;
                }
                _file = found->second;
            }
        }
        if (_file != nullptr)
        {
            return _file;
        }
        return Skia::Typeface::Default(static_cast<std::int32_t>(weight));
    }

    // ------------------------------------------------------------- text

    namespace
    {
        // The face a character is drawn in: the family's own if it has the
        // glyph, the system's otherwise -- which is what Avalonia's font
        // fallback does for the odd symbol a pixel face lacks.
        [[nodiscard]] const Skia::Typeface* FaceFor(const std::shared_ptr<Skia::Typeface>& primary, char32_t code,
            FontWeight weight)
        {
            if (primary != nullptr && (code < 0x20 || primary->HasGlyph(code)))
            {
                return primary.get();
            }
            const std::shared_ptr<Skia::Typeface> fallback = Skia::Typeface::Default(static_cast<std::int32_t>(weight));
            if (fallback != nullptr && fallback->HasGlyph(code))
            {
                return fallback.get();
            }
#if defined(_WIN32)
            static const std::shared_ptr<Skia::Typeface> symbols = Skia::Typeface::FromFile("C:\\Windows\\Fonts\\seguisym.ttf");
            if (symbols != nullptr && symbols->HasGlyph(code))
            {
                return symbols.get();
            }
#endif
            return primary != nullptr ? primary.get() : fallback.get();
        }

        [[nodiscard]] bool IsSpace(char32_t c) noexcept
        {
            return c == U' ' || c == U'\t' || c == 0x00A0;
        }
    }

    TextLayout::TextLayout(std::u32string text, const Typeface& typeface, double fontSize, IBrushPtr foreground,
        TextAlignment alignment, TextWrapping wrapping, TextTrimming trimming, double maxWidth, double maxHeight,
        double lineHeight, std::int32_t maxLines, double letterSpacing)
        : _text(std::move(text)), _typeface(typeface), _fontSize(fontSize), _foreground(std::move(foreground)),
          _alignment(alignment), _wrapping(wrapping), _trimming(trimming), _maxWidth(maxWidth), _maxHeight(maxHeight),
          _requestedLineHeight(lineHeight), _maxLines(maxLines), _letterSpacing(letterSpacing)
    {
        const FontFamilyPtr family = _typeface.FontFamily != nullptr ? _typeface.FontFamily : FontFamily::Default();
        _face = family->Resolve(_typeface.Weight, _typeface.Style);
        Layout();
    }

    double TextLayout::Measure(std::u32string_view text) const
    {
        double width = 0;
        const Skia::Typeface* previousFace = nullptr;
        char32_t previous = 0;
        for (const char32_t c : text)
        {
            const Skia::Typeface* face = FaceFor(_face, c, _typeface.Weight);
            if (face == nullptr)
            {
                continue;
            }
            if (face == previousFace && previous != 0)
            {
                width += face->Kerning(previous, c, _fontSize);
            }
            width += face->Advance(c, _fontSize) + _letterSpacing;
            previousFace = face;
            previous = c;
        }
        return width;
    }

    void TextLayout::Layout()
    {
        Skia::Typeface::Metrics metrics{};
        if (_face != nullptr)
        {
            metrics = _face->MetricsAt(_fontSize);
        }
        const double natural = metrics.Ascent + metrics.Descent + metrics.LineGap;
        _lineHeight = std::isnan(_requestedLineHeight) || _requestedLineHeight <= 0 ? natural : _requestedLineHeight;
        _ascent = metrics.Ascent + (_lineHeight - natural) / 2;
        _baseline = _ascent;

        // Paragraphs first, then each broken to the width.
        std::vector<std::u32string> paragraphs;
        {
            std::u32string current;
            for (std::size_t i = 0; i < _text.size(); i++)
            {
                const char32_t c = _text[i];
                if (c == U'\r' && i + 1 < _text.size() && _text[i + 1] == U'\n')
                {
                    continue;
                }
                if (c == U'\n' || c == U'\r' || c == 0x2028)
                {
                    paragraphs.push_back(current);
                    current.clear();
                    continue;
                }
                current.push_back(c);
            }
            paragraphs.push_back(current);
        }
        const bool wrap = _wrapping != TextWrapping::NoWrap && std::isfinite(_maxWidth);
        std::vector<std::u32string> lines;
        for (const std::u32string& paragraph : paragraphs)
        {
            if (!wrap || Measure(paragraph) <= _maxWidth)
            {
                lines.push_back(paragraph);
                continue;
            }
            std::size_t start = 0;
            while (start < paragraph.size())
            {
                // The longest run from start that fits, broken after a space.
                std::size_t end = start;
                std::size_t lastBreak = std::u32string::npos;
                double width = 0;
                while (end < paragraph.size())
                {
                    const double next = Measure(std::u32string_view(paragraph).substr(start, end - start + 1));
                    if (next > _maxWidth && !IsSpace(paragraph[end]))
                    {
                        break;
                    }
                    width = next;
                    if (IsSpace(paragraph[end]))
                    {
                        lastBreak = end;
                    }
                    end++;
                }
                (void)width;
                if (end >= paragraph.size())
                {
                    lines.push_back(paragraph.substr(start));
                    break;
                }
                std::size_t cut;
                if (lastBreak != std::u32string::npos && lastBreak >= start)
                {
                    cut = lastBreak + 1;
                }
                else if (_wrapping == TextWrapping::WrapWithOverflow)
                {
                    // The word overflows rather than breaks.
                    cut = end;
                    while (cut < paragraph.size() && !IsSpace(paragraph[cut]))
                    {
                        cut++;
                    }
                    if (cut < paragraph.size())
                    {
                        cut++;
                    }
                }
                else
                {
                    cut = std::max(end, start + 1);
                }
                lines.push_back(paragraph.substr(start, cut - start));
                start = cut;
            }
        }
        // MaxLines and MaxTextHeight drop what does not fit; trimming marks
        // the last line that stays.
        std::size_t keep = lines.size();
        if (_maxLines > 0)
        {
            keep = std::min(keep, static_cast<std::size_t>(_maxLines));
        }
        if (std::isfinite(_maxHeight) && _lineHeight > 0)
        {
            keep = std::min(keep, std::max<std::size_t>(1, static_cast<std::size_t>(std::floor(_maxHeight / _lineHeight + 1e-6))));
        }
        const bool truncated = keep < lines.size();
        lines.resize(keep);
        const std::u32string ellipsis = U"\u2026";
        for (std::size_t i = 0; i < lines.size(); i++)
        {
            std::u32string& line = lines[i];
            const bool last = i + 1 == lines.size();
            if (_trimming == TextTrimming::None || !std::isfinite(_maxWidth))
            {
                continue;
            }
            if (Measure(line) <= _maxWidth && !(last && truncated))
            {
                continue;
            }
            std::u32string cut = line;
            while (!cut.empty() && Measure(cut + ellipsis) > _maxWidth)
            {
                if (_trimming == TextTrimming::WordEllipsis)
                {
                    const std::size_t space = cut.find_last_of(U' ');
                    cut = space == std::u32string::npos ? cut.substr(0, cut.size() - 1) : cut.substr(0, space);
                }
                else
                {
                    cut.pop_back();
                }
            }
            while (!cut.empty() && IsSpace(cut.back()))
            {
                cut.pop_back();
            }
            line = cut + ellipsis;
        }
        _lines.clear();
        _width = 0;
        _widthWithWhitespace = 0;
        for (std::u32string& line : lines)
        {
            TextLine laid;
            laid.WidthIncludingTrailingWhitespace = Measure(line);
            std::size_t trimmed = line.size();
            while (trimmed > 0 && IsSpace(line[trimmed - 1]))
            {
                trimmed--;
            }
            laid.Width = Measure(std::u32string_view(line).substr(0, trimmed));
            laid.Text = std::move(line);
            _width = std::max(_width, laid.Width);
            _widthWithWhitespace = std::max(_widthWithWhitespace, laid.WidthIncludingTrailingWhitespace);
            _lines.push_back(std::move(laid));
        }
        _height = _lineHeight * static_cast<double>(std::max<std::size_t>(1, _lines.size()));
    }

    double TextLayout::CaretX(std::size_t index) const
    {
        if (_lines.empty())
        {
            return 0;
        }
        const std::u32string& text = _lines.front().Text;
        return Measure(std::u32string_view(text).substr(0, std::min(index, text.size())));
    }

    std::size_t TextLayout::HitTest(double x) const
    {
        if (_lines.empty())
        {
            return 0;
        }
        const std::u32string& text = _lines.front().Text;
        for (std::size_t i = 0; i < text.size(); i++)
        {
            const double left = CaretX(i);
            const double right = CaretX(i + 1);
            if (x < (left + right) / 2)
            {
                return i;
            }
        }
        return text.size();
    }

    void TextLayout::Draw(Skia::Canvas& canvas, Point origin, const IBrushPtr& overrideBrush) const
    {
        const IBrushPtr& brush = overrideBrush != nullptr ? overrideBrush : _foreground;
        if (brush == nullptr || _lines.empty())
        {
            return;
        }
        Skia::Paint paint;
        paint.Opacity = brush->Opacity;
        if (const auto* solid = dynamic_cast<const SolidColorBrush*>(brush.get()))
        {
            paint.Solid = ToSkia(solid->Color);
        }
        else if (const auto* gradient = dynamic_cast<const GradientBrush*>(brush.get()))
        {
            // A gradient over the text's own box.
            const Rect bounds{origin.X, origin.Y, std::max(_width, 1.0), std::max(_height, 1.0)};
            std::vector<Skia::GradientStop> stops;
            for (const GradientStop& stop : gradient->GradientStops)
            {
                stops.push_back({ToSkia(stop.Color), stop.Offset});
            }
            if (const auto* linear = dynamic_cast<const LinearGradientBrush*>(gradient))
            {
                const Point a = linear->StartPoint.ToPixels(bounds);
                const Point b = linear->EndPoint.ToPixels(bounds);
                paint.Gradient = Skia::LinearGradient({a.X, a.Y}, {b.X, b.Y}, std::move(stops),
                    static_cast<Skia::SpreadMethod>(gradient->SpreadMethod), canvas.TotalMatrix());
            }
        }
        const double boxWidth = std::isfinite(_maxWidth) ? _maxWidth : _width;
        for (std::size_t i = 0; i < _lines.size(); i++)
        {
            const TextLine& line = _lines[i];
            double x = origin.X;
            switch (_alignment)
            {
            case TextAlignment::Center:
                x += (boxWidth - line.Width) / 2;
                break;
            case TextAlignment::Right:
            case TextAlignment::End:
                x += boxWidth - line.Width;
                break;
            default:
                break;
            }
            const double baseline = origin.Y + _lineHeight * static_cast<double>(i) + _ascent;
            // Runs of one face at a time.
            std::size_t start = 0;
            double pen = x;
            while (start < line.Text.size())
            {
                const Skia::Typeface* face = FaceFor(_face, line.Text[start], _typeface.Weight);
                std::size_t end = start + 1;
                while (end < line.Text.size() && FaceFor(_face, line.Text[end], _typeface.Weight) == face)
                {
                    end++;
                }
                const std::u32string_view run = std::u32string_view(line.Text).substr(start, end - start);
                if (face != nullptr)
                {
                    if (_letterSpacing == 0)
                    {
                        canvas.DrawText(run, *face, _fontSize, {pen, baseline}, paint);
                        pen += Measure(run);
                    }
                    else
                    {
                        for (const char32_t c : run)
                        {
                            canvas.DrawText(std::u32string_view(&c, 1), *face, _fontSize, {pen, baseline}, paint);
                            pen += face->Advance(c, _fontSize) + _letterSpacing;
                        }
                    }
                }
                start = end;
            }
        }
    }

    FormattedText::FormattedText(std::string_view text, const void* culture, FlowDirection flowDirection,
        Typeface typeface, double emSize, IBrushPtr foreground)
        : _text(ToUtf32(text)), _typeface(std::move(typeface)), _emSize(emSize), _foreground(std::move(foreground))
    {
        (void)culture;
        (void)flowDirection;
    }

    const TextLayout& FormattedText::Layout() const
    {
        if (_layout == nullptr)
        {
            // MaxTextWidth wraps, as FormattedText does; trimming only
            // applies when asked for.
            _layout = std::make_shared<TextLayout>(_text, _typeface, _emSize, _foreground, _alignment,
                std::isfinite(_maxTextWidth) ? TextWrapping::Wrap : TextWrapping::NoWrap, _trimming, _maxTextWidth,
                _maxTextHeight, _lineHeight);
        }
        return *_layout;
    }

    double FormattedText::Width() const
    {
        return Layout().Width();
    }

    double FormattedText::WidthIncludingTrailingWhitespace() const
    {
        return Layout().WidthIncludingTrailingWhitespace();
    }

    double FormattedText::Height() const
    {
        return Layout().Height();
    }

    double FormattedText::Baseline() const
    {
        return Layout().Baseline();
    }

    // ------------------------------------------------------- transforms

    Avalonia::Matrix RotateTransform::Value() const
    {
        return Avalonia::Matrix::CreateRotation(Angle * std::numbers::pi / 180.0);
    }

    Avalonia::Matrix TransformGroup::Value() const
    {
        Avalonia::Matrix result = Avalonia::Matrix::Identity();
        for (const TransformPtr& child : Children)
        {
            if (child != nullptr)
            {
                result = result * child->Value();
            }
        }
        return result;
    }

    // --------------------------------------------------------- geometry

    Rect Geometry::Bounds() const
    {
        const Skia::Rect r = ToPath().Bounds();
        Rect bounds{r.Left, r.Top, r.Width(), r.Height()};
        if (Transform != nullptr)
        {
            bounds = TransformToAABB(bounds, Transform->Value());
        }
        return bounds;
    }

    Skia::Path RectangleGeometry::ToPath() const
    {
        Skia::Path path;
        path.AddRect(ToSkia(Rect));
        return path;
    }

    EllipseGeometry::EllipseGeometry(Avalonia::Rect rect)
        : Center(rect.Center()), RadiusX(rect.Width / 2), RadiusY(rect.Height / 2)
    {
    }

    Skia::Path EllipseGeometry::ToPath() const
    {
        Skia::Path path;
        path.AddEllipse({Center.X, Center.Y}, RadiusX, RadiusY);
        return path;
    }

    void StreamGeometryContext::SetFillRule(FillRule rule)
    {
        _path.Rule = rule == FillRule::EvenOdd ? Skia::FillRule::EvenOdd : Skia::FillRule::NonZero;
    }

    void StreamGeometryContext::BeginFigure(Point startPoint, bool isFilled)
    {
        (void)isFilled;
        _path.MoveTo({startPoint.X, startPoint.Y});
    }

    void StreamGeometryContext::LineTo(Point point, bool isStroked)
    {
        (void)isStroked;
        _path.LineTo({point.X, point.Y});
    }

    void StreamGeometryContext::QuadraticBezierTo(Point control, Point end, bool isStroked)
    {
        (void)isStroked;
        _path.QuadTo({control.X, control.Y}, {end.X, end.Y});
    }

    void StreamGeometryContext::CubicBezierTo(Point c1, Point c2, Point end, bool isStroked)
    {
        (void)isStroked;
        _path.CubicTo({c1.X, c1.Y}, {c2.X, c2.Y}, {end.X, end.Y});
    }

    void StreamGeometryContext::ArcTo(Point point, Size size, double rotationAngle, bool isLargeArc,
        SweepDirection sweepDirection, bool isStroked)
    {
        (void)isStroked;
        _path.ArcTo({point.X, point.Y}, size.Width, size.Height, rotationAngle, isLargeArc,
            sweepDirection == SweepDirection::Clockwise);
    }

    void StreamGeometryContext::EndFigure(bool isClosed)
    {
        if (isClosed)
        {
            _path.Close();
        }
    }

    // ---------------------------------------------------------- imaging

    namespace Imaging
    {
        Bitmap::Bitmap(std::shared_ptr<Skia::Bitmap> pixels)
            : _pixels(std::move(pixels))
        {
        }

        std::shared_ptr<Bitmap> Bitmap::FromBytes(const std::vector<std::uint8_t>& bytes)
        {
            std::shared_ptr<Skia::Bitmap> pixels = Skia::Bitmap::Decode(bytes.data(), bytes.size());
            if (pixels == nullptr)
            {
                throw std::invalid_argument("Unable to load bitmap from provided data");
            }
            return std::make_shared<Bitmap>(std::move(pixels));
        }

        std::shared_ptr<Bitmap> Bitmap::FromFile(const std::string& path)
        {
            return FromBytes(FileReadAllBytes(path));
        }

        Avalonia::Size Bitmap::Size() const
        {
            return _pixels == nullptr ? Avalonia::Size{}
                                      : Avalonia::Size{static_cast<double>(_pixels->Width()),
                                            static_cast<double>(_pixels->Height())};
        }

        Avalonia::PixelSize Bitmap::PixelSize() const
        {
            return _pixels == nullptr ? Avalonia::PixelSize{} : Avalonia::PixelSize{_pixels->Width(), _pixels->Height()};
        }
    }

    // ---------------------------------------------------------- context

    Skia::Matrix ToSkia(const Avalonia::Matrix& m) noexcept
    {
        Skia::Matrix s;
        s.ScaleX = m.M11;
        s.SkewX = m.M21;
        s.TransX = m.M31;
        s.SkewY = m.M12;
        s.ScaleY = m.M22;
        s.TransY = m.M32;
        return s;
    }

    Skia::Color ToSkia(Color color) noexcept
    {
        return Skia::Color{color.R, color.G, color.B, color.A};
    }

    Skia::Rect ToSkia(const Rect& rect) noexcept
    {
        return Skia::Rect::FromXYWH(rect.X, rect.Y, rect.Width, rect.Height);
    }

    DrawingContext::DrawingContext(Skia::Canvas& canvas)
        : _canvas(canvas)
    {
        _baseSaves = canvas.SaveCount();
        const Skia::Matrix& m = canvas.TotalMatrix();
        _transform.back() = Avalonia::Matrix{m.ScaleX, m.SkewY, m.SkewX, m.ScaleY, m.TransX, m.TransY};
    }

    std::optional<Skia::Paint> DrawingContext::PaintFor(const IBrushPtr& brush, const Rect& bounds) const
    {
        if (brush == nullptr)
        {
            return std::nullopt;
        }
        Skia::Paint paint;
        paint.Opacity = brush->Opacity;
        Avalonia::Matrix local = Avalonia::Matrix::Identity();
        if (brush->Transform != nullptr)
        {
            const Point origin = brush->TransformOrigin.ToPixels(bounds);
            local = Avalonia::Matrix::CreateTranslation(-origin.X, -origin.Y) * brush->Transform->Value()
                * Avalonia::Matrix::CreateTranslation(origin.X, origin.Y);
        }
        const Skia::Matrix device = ToSkia(local * _transform.back());
        if (const auto* solid = dynamic_cast<const SolidColorBrush*>(brush.get()))
        {
            if (solid->Color.A == 0)
            {
                return std::nullopt;
            }
            paint.Solid = ToSkia(solid->Color);
            return paint;
        }
        const auto* gradient = dynamic_cast<const GradientBrush*>(brush.get());
        if (gradient == nullptr)
        {
            return std::nullopt;
        }
        std::vector<Skia::GradientStop> stops;
        for (const GradientStop& stop : gradient->GradientStops)
        {
            stops.push_back({ToSkia(stop.Color), stop.Offset});
        }
        const auto spread = static_cast<Skia::SpreadMethod>(gradient->SpreadMethod);
        if (const auto* linear = dynamic_cast<const LinearGradientBrush*>(gradient))
        {
            const Point a = linear->StartPoint.ToPixels(bounds);
            const Point b = linear->EndPoint.ToPixels(bounds);
            paint.Gradient = Skia::LinearGradient({a.X, a.Y}, {b.X, b.Y}, std::move(stops), spread, device);
            return paint;
        }
        if (const auto* radial = dynamic_cast<const RadialGradientBrush*>(gradient))
        {
            const Point c = radial->Center.ToPixels(bounds);
            const Point o = radial->GradientOrigin.ToPixels(bounds);
            paint.Gradient = Skia::RadialGradient({c.X, c.Y}, {o.X, o.Y}, radial->RadiusX.ToValue(bounds.Width),
                radial->RadiusY.ToValue(bounds.Height), std::move(stops), spread, device);
            return paint;
        }
        return std::nullopt;
    }

    void DrawingContext::Fill(const Skia::Path& path, const IBrushPtr& brush, const Rect& bounds)
    {
        const std::optional<Skia::Paint> paint = PaintFor(brush, bounds);
        if (paint.has_value())
        {
            _canvas.FillPath(path, *paint);
        }
    }

    void DrawingContext::Stroke(const Skia::Path& path, const IPenPtr& pen, const Rect& bounds)
    {
        if (pen == nullptr || pen->Thickness <= 0)
        {
            return;
        }
        const std::optional<Skia::Paint> paint = PaintFor(pen->Brush, bounds);
        if (!paint.has_value())
        {
            return;
        }
        Skia::StrokeStyle style;
        style.Width = pen->Thickness;
        style.Cap = pen->LineCap == PenLineCap::Round ? Skia::LineCap::Round
            : pen->LineCap == PenLineCap::Square ? Skia::LineCap::Square : Skia::LineCap::Butt;
        style.Join = pen->LineJoin == PenLineJoin::Round ? Skia::LineJoin::Round
            : pen->LineJoin == PenLineJoin::Bevel ? Skia::LineJoin::Bevel : Skia::LineJoin::Miter;
        style.MiterLimit = pen->MiterLimit;
        if (pen->DashStyle != nullptr)
        {
            style.Dashes = pen->DashStyle->Dashes;
            style.DashOffset = pen->DashStyle->Offset;
        }
        _canvas.StrokePath(path, style, *paint);
    }

    void DrawingContext::DrawText(const FormattedText& text, Point origin)
    {
        DrawCount++;
        text.Layout().Draw(_canvas, origin);
    }

    void DrawingContext::DrawRectangle(const IBrushPtr& brush, const IPenPtr& pen, const Rect& rect, double radiusX,
        double radiusY, const BoxShadows& boxShadows)
    {
        DrawCount++;
        std::array<Skia::Point, 4> radii{};
        for (Skia::Point& r : radii)
        {
            r = {radiusX, radiusY};
        }
        Skia::Path path;
        path.AddRoundRect(ToSkia(rect), radii);
        for (const BoxShadow& shadow : boxShadows.Items())
        {
            if (!shadow.IsInset)
            {
                _canvas.DrawBoxShadow(path, {shadow.OffsetX, shadow.OffsetY, shadow.Blur, shadow.Spread,
                    ToSkia(shadow.Color), false}, ToSkia(rect), radii);
            }
        }
        Fill(path, brush, rect);
        for (const BoxShadow& shadow : boxShadows.Items())
        {
            if (shadow.IsInset)
            {
                _canvas.DrawBoxShadow(path, {shadow.OffsetX, shadow.OffsetY, shadow.Blur, shadow.Spread,
                    ToSkia(shadow.Color), true}, ToSkia(rect), radii);
            }
        }
        Stroke(path, pen, rect);
    }

    void DrawingContext::DrawRectangle(const IBrushPtr& brush, const IPenPtr& pen, const Rect& rect,
        const CornerRadius& radius, const BoxShadows& boxShadows)
    {
        DrawCount++;
        const std::array<Skia::Point, 4> radii{Skia::Point{radius.TopLeft, radius.TopLeft},
            Skia::Point{radius.TopRight, radius.TopRight}, Skia::Point{radius.BottomRight, radius.BottomRight},
            Skia::Point{radius.BottomLeft, radius.BottomLeft}};
        Skia::Path path;
        path.AddRoundRect(ToSkia(rect), radii);
        for (const BoxShadow& shadow : boxShadows.Items())
        {
            if (!shadow.IsInset)
            {
                _canvas.DrawBoxShadow(path, {shadow.OffsetX, shadow.OffsetY, shadow.Blur, shadow.Spread,
                    ToSkia(shadow.Color), false}, ToSkia(rect), radii);
            }
        }
        Fill(path, brush, rect);
        for (const BoxShadow& shadow : boxShadows.Items())
        {
            if (shadow.IsInset)
            {
                _canvas.DrawBoxShadow(path, {shadow.OffsetX, shadow.OffsetY, shadow.Blur, shadow.Spread,
                    ToSkia(shadow.Color), true}, ToSkia(rect), radii);
            }
        }
        Stroke(path, pen, rect);
    }

    void DrawingContext::DrawRectangle(const IPenPtr& pen, const Rect& rect, double cornerRadius)
    {
        DrawRectangle(nullptr, pen, rect, cornerRadius, cornerRadius);
    }

    void DrawingContext::FillRectangle(const IBrushPtr& brush, const Rect& rect, double cornerRadius)
    {
        DrawRectangle(brush, nullptr, rect, cornerRadius, cornerRadius);
    }

    void DrawingContext::DrawEllipse(const IBrushPtr& brush, const IPenPtr& pen, Point center, double radiusX,
        double radiusY)
    {
        DrawCount++;
        Skia::Path path;
        path.AddEllipse({center.X, center.Y}, radiusX, radiusY);
        const Rect bounds{center.X - radiusX, center.Y - radiusY, radiusX * 2, radiusY * 2};
        Fill(path, brush, bounds);
        Stroke(path, pen, bounds);
    }

    void DrawingContext::DrawEllipse(const IBrushPtr& brush, const IPenPtr& pen, const Rect& rect)
    {
        DrawEllipse(brush, pen, rect.Center(), rect.Width / 2, rect.Height / 2);
    }

    void DrawingContext::DrawLine(const IPenPtr& pen, Point p1, Point p2)
    {
        DrawCount++;
        Skia::Path path;
        path.MoveTo({p1.X, p1.Y});
        path.LineTo({p2.X, p2.Y});
        Stroke(path, pen, Rect(p1, p2));
    }

    void DrawingContext::DrawGeometry(const IBrushPtr& brush, const IPenPtr& pen, const Geometry& geometry)
    {
        DrawCount++;
        const Skia::Path path = geometry.ToPath();
        const Rect bounds = geometry.Bounds();
        if (geometry.Transform != nullptr)
        {
            auto state = PushTransform(geometry.Transform->Value());
            Fill(path, brush, bounds);
            Stroke(path, pen, bounds);
            return;
        }
        Fill(path, brush, bounds);
        Stroke(path, pen, bounds);
    }

    void DrawingContext::DrawImage(const IImage& source, const Rect& sourceRect, const Rect& destRect)
    {
        DrawCount++;
        const Skia::Bitmap* pixels = source.Pixels();
        if (pixels == nullptr)
        {
            return;
        }
        Skia::FilterQuality quality = Skia::FilterQuality::Low;
        switch (_options.back().BitmapInterpolationMode)
        {
        case BitmapInterpolationMode::None:
            quality = Skia::FilterQuality::None;
            break;
        case BitmapInterpolationMode::MediumQuality:
        case BitmapInterpolationMode::HighQuality:
            quality = Skia::FilterQuality::High;
            break;
        default:
            break;
        }
        _canvas.DrawBitmap(*pixels, ToSkia(sourceRect), ToSkia(destRect), quality, 1.0);
    }

    void DrawingContext::DrawImage(const IImage& source, const Rect& destRect)
    {
        const Avalonia::Size size = source.Size();
        DrawImage(source, Rect{0, 0, size.Width, size.Height}, destRect);
    }

    DrawingContext::PushedState DrawingContext::Pushed()
    {
        const std::size_t depth = _canvas.SaveCount();
        _canvas.Save();
        _transform.push_back(_transform.back());
        _options.push_back(_options.back());
        return PushedState(this, depth);
    }

    void DrawingContext::PopTo(std::size_t depth) noexcept
    {
        while (_canvas.SaveCount() > depth && _canvas.SaveCount() > _baseSaves)
        {
            _canvas.Restore();
            if (_transform.size() > 1)
            {
                _transform.pop_back();
            }
            if (_options.size() > 1)
            {
                _options.pop_back();
            }
        }
    }

    void DrawingContext::PushedState::Dispose() noexcept
    {
        if (_context != nullptr)
        {
            _context->PopTo(_depth);
            _context = nullptr;
        }
    }

    DrawingContext::PushedState DrawingContext::PushClip(const Rect& clip)
    {
        PushedState state = Pushed();
        _canvas.ClipRect(ToSkia(clip));
        return state;
    }

    DrawingContext::PushedState DrawingContext::PushClip(const Rect& clip, const CornerRadius& radius)
    {
        PushedState state = Pushed();
        Skia::Path path;
        path.AddRoundRect(ToSkia(clip), {Skia::Point{radius.TopLeft, radius.TopLeft},
            Skia::Point{radius.TopRight, radius.TopRight}, Skia::Point{radius.BottomRight, radius.BottomRight},
            Skia::Point{radius.BottomLeft, radius.BottomLeft}});
        _canvas.ClipPath(path);
        return state;
    }

    DrawingContext::PushedState DrawingContext::PushGeometryClip(const Geometry& clip)
    {
        PushedState state = Pushed();
        Skia::Path path = clip.ToPath();
        if (clip.Transform != nullptr)
        {
            _canvas.Concat(ToSkia(clip.Transform->Value()));
            _canvas.ClipPath(path);
            _canvas.SetMatrix(ToSkia(_transform.back()));
        }
        else
        {
            _canvas.ClipPath(path);
        }
        return state;
    }

    DrawingContext::PushedState DrawingContext::PushOpacity(double opacity)
    {
        const std::size_t depth = _canvas.SaveCount();
        _canvas.SaveLayerAlpha(opacity);
        _transform.push_back(_transform.back());
        _options.push_back(_options.back());
        return PushedState(this, depth);
    }

    DrawingContext::PushedState DrawingContext::PushTransform(const Avalonia::Matrix& matrix)
    {
        PushedState state = Pushed();
        _transform.back() = matrix * _transform.back();
        _canvas.SetMatrix(ToSkia(_transform.back()));
        return state;
    }

    DrawingContext::PushedState DrawingContext::PushRenderOptions(const RenderOptions& options)
    {
        PushedState state = Pushed();
        if (options.BitmapInterpolationMode != BitmapInterpolationMode::Unspecified)
        {
            _options.back().BitmapInterpolationMode = options.BitmapInterpolationMode;
        }
        return state;
    }
}
