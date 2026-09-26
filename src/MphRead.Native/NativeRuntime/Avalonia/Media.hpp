#pragma once

// Avalonia.Media as far as the launcher uses it: colours, brushes, pens, font
// families, typefaces, formatted text, geometry, transforms, box shadows,
// bitmaps and the DrawingContext a control renders into. Drawn by
// NativeRuntime/Skia, which is what Avalonia's Skia backend is on the C# side.

#include "Base.hpp"
#include "../Skia/Skia.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia::Media
{
    struct Color final
    {
        std::uint8_t A = 0;
        std::uint8_t R = 0;
        std::uint8_t G = 0;
        std::uint8_t B = 0;

        [[nodiscard]] static constexpr Color FromRgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
        {
            return {255, r, g, b};
        }
        [[nodiscard]] static constexpr Color FromArgb(std::uint8_t a, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
        {
            return {a, r, g, b};
        }
        [[nodiscard]] static constexpr Color FromUInt32(std::uint32_t argb) noexcept
        {
            return {static_cast<std::uint8_t>(argb >> 24), static_cast<std::uint8_t>(argb >> 16),
                static_cast<std::uint8_t>(argb >> 8), static_cast<std::uint8_t>(argb)};
        }
        [[nodiscard]] constexpr std::uint32_t ToUInt32() const noexcept
        {
            return (static_cast<std::uint32_t>(A) << 24) | (static_cast<std::uint32_t>(R) << 16)
                | (static_cast<std::uint32_t>(G) << 8) | B;
        }
        friend constexpr bool operator==(const Color&, const Color&) noexcept = default;
    };

    namespace Colors
    {
        inline constexpr Color Transparent{0, 255, 255, 255};
        inline constexpr Color Black{255, 0, 0, 0};
        inline constexpr Color White{255, 255, 255, 255};
        inline constexpr Color Red{255, 255, 0, 0};
        inline constexpr Color Gray{255, 128, 128, 128};
    }

    enum class GradientSpreadMethod : std::int32_t { Pad, Reflect, Repeat };

    class Transform;

    // IBrush.
    class IBrush
    {
    public:
        virtual ~IBrush() = default;
        double Opacity = 1.0;
        std::shared_ptr<Transform> Transform{};
        RelativePoint TransformOrigin{0, 0, RelativeUnit::Relative};
    };

    class SolidColorBrush final : public IBrush
    {
    public:
        SolidColorBrush() = default;
        explicit SolidColorBrush(Media::Color color, double opacity = 1.0)
            : Color(color)
        {
            Opacity = opacity;
        }
        Media::Color Color{};
    };

    struct GradientStop final
    {
        Media::Color Color{};
        double Offset = 0.0;

        GradientStop() = default;
        GradientStop(Media::Color color, double offset)
            : Color(color), Offset(offset)
        {
        }
    };

    class GradientBrush : public IBrush
    {
    public:
        std::vector<GradientStop> GradientStops;
        GradientSpreadMethod SpreadMethod = GradientSpreadMethod::Pad;
    };

    class LinearGradientBrush final : public GradientBrush
    {
    public:
        RelativePoint StartPoint = RelativePoint::TopLeft();
        RelativePoint EndPoint = RelativePoint::BottomRight();
    };

    class RadialGradientBrush final : public GradientBrush
    {
    public:
        RelativePoint Center = RelativePoint::Center();
        RelativePoint GradientOrigin = RelativePoint::Center();
        RelativeScalar RadiusX{0.5, RelativeUnit::Relative};
        RelativeScalar RadiusY{0.5, RelativeUnit::Relative};
    };

    using IBrushPtr = std::shared_ptr<IBrush>;

    // Brushes.*: shared, never mutated.
    namespace Brushes
    {
        [[nodiscard]] const IBrushPtr& Transparent();
        [[nodiscard]] const IBrushPtr& Black();
        [[nodiscard]] const IBrushPtr& White();
    }

    enum class PenLineCap : std::int32_t { Flat, Round, Square };
    enum class PenLineJoin : std::int32_t { Miter, Bevel, Round };

    class DashStyle final
    {
    public:
        DashStyle() = default;
        DashStyle(std::vector<double> dashes, double offset)
            : Dashes(std::move(dashes)), Offset(offset)
        {
        }
        std::vector<double> Dashes;
        double Offset = 0.0;
    };

    // IPen.
    class IPen
    {
    public:
        IPen() = default;
        IPen(IBrushPtr brush, double thickness = 1.0, std::shared_ptr<Media::DashStyle> dashStyle = nullptr,
            PenLineCap lineCap = PenLineCap::Flat, PenLineJoin lineJoin = PenLineJoin::Miter, double miterLimit = 10.0)
            : Brush(std::move(brush)), Thickness(thickness), DashStyle(std::move(dashStyle)), LineCap(lineCap),
              LineJoin(lineJoin), MiterLimit(miterLimit)
        {
        }
        virtual ~IPen() = default;

        IBrushPtr Brush{};
        double Thickness = 1.0;
        std::shared_ptr<Media::DashStyle> DashStyle{};
        PenLineCap LineCap = PenLineCap::Flat;
        PenLineJoin LineJoin = PenLineJoin::Miter;
        double MiterLimit = 10.0;
    };

    class Pen final : public IPen
    {
    public:
        using IPen::IPen;
    };

    using IPenPtr = std::shared_ptr<IPen>;

    enum class FontStyle : std::int32_t { Normal, Italic, Oblique };

    enum class FontWeight : std::int32_t
    {
        Thin = 100,
        ExtraLight = 200,
        Light = 300,
        SemiLight = 350,
        Normal = 400,
        Medium = 500,
        SemiBold = 600,
        Bold = 700,
        ExtraBold = 800,
        Black = 900,
        ExtraBlack = 950
    };

    enum class FontStretch : std::int32_t { Normal = 5 };

    // FontFamily: a name, or an avares: URI naming a file and a family in it.
    class FontFamily final
    {
    public:
        explicit FontFamily(std::string name);

        [[nodiscard]] const std::string& Name() const noexcept { return _name; }
        [[nodiscard]] static const std::shared_ptr<FontFamily>& Default();

        // The face to draw a weight in: the file named, or the system's.
        [[nodiscard]] std::shared_ptr<Skia::Typeface> Resolve(FontWeight weight, FontStyle style) const;

        friend bool operator==(const FontFamily& a, const FontFamily& b) noexcept { return a._name == b._name; }

    private:
        std::string _name;
        mutable std::shared_ptr<Skia::Typeface> _file;
        mutable bool _loaded = false;
    };

    using FontFamilyPtr = std::shared_ptr<FontFamily>;

    struct Typeface final
    {
        FontFamilyPtr FontFamily{};
        FontStyle Style = FontStyle::Normal;
        FontWeight Weight = FontWeight::Normal;
        FontStretch Stretch = FontStretch::Normal;

        Typeface() = default;
        Typeface(FontFamilyPtr family, FontStyle style = FontStyle::Normal, FontWeight weight = FontWeight::Normal,
            FontStretch stretch = FontStretch::Normal)
            : FontFamily(std::move(family)), Style(style), Weight(weight), Stretch(stretch)
        {
        }
        friend bool operator==(const Typeface& a, const Typeface& b) noexcept
        {
            const bool families = a.FontFamily == b.FontFamily
                || (a.FontFamily != nullptr && b.FontFamily != nullptr && *a.FontFamily == *b.FontFamily);
            return families && a.Style == b.Style && a.Weight == b.Weight && a.Stretch == b.Stretch;
        }
    };

    enum class FlowDirection : std::int32_t { LeftToRight, RightToLeft };
    enum class TextAlignment : std::int32_t { Left, Center, Right, Start, End, Justify, DetectFromContent };
    enum class TextWrapping : std::int32_t { NoWrap, Wrap, WrapWithOverflow };
    enum class TextTrimming : std::int32_t { None, CharacterEllipsis, WordEllipsis, PrefixCharacterEllipsis, LeadingCharacterEllipsis };

    // One laid-out line: its text, where it starts and how wide it is.
    struct TextLine final
    {
        std::u32string Text;
        double Width = 0.0;
        double WidthIncludingTrailingWhitespace = 0.0;
    };

    // The shared text engine under FormattedText and TextBlock: a run in one
    // face, broken into lines, measured, and drawn.
    class TextLayout final
    {
    public:
        TextLayout(std::u32string text, const Typeface& typeface, double fontSize, IBrushPtr foreground,
            TextAlignment alignment = TextAlignment::Left, TextWrapping wrapping = TextWrapping::NoWrap,
            TextTrimming trimming = TextTrimming::None, double maxWidth = std::numeric_limits<double>::infinity(),
            double maxHeight = std::numeric_limits<double>::infinity(), double lineHeight = std::numeric_limits<double>::quiet_NaN(),
            std::int32_t maxLines = 0, double letterSpacing = 0.0);

        [[nodiscard]] double Width() const noexcept { return _width; }
        [[nodiscard]] double WidthIncludingTrailingWhitespace() const noexcept { return _widthWithWhitespace; }
        [[nodiscard]] double Height() const noexcept { return _height; }
        [[nodiscard]] double Baseline() const noexcept { return _baseline; }
        [[nodiscard]] double LineHeight() const noexcept { return _lineHeight; }
        [[nodiscard]] const std::vector<TextLine>& Lines() const noexcept { return _lines; }
        // x of the caret before each character of line zero, for a text box.
        [[nodiscard]] double CaretX(std::size_t index) const;
        [[nodiscard]] std::size_t HitTest(double x) const;

        void Draw(Skia::Canvas& canvas, Point origin, const IBrushPtr& overrideBrush = nullptr) const;

    private:
        [[nodiscard]] double Measure(std::u32string_view text) const;
        void Layout();

        std::u32string _text;
        Typeface _typeface;
        double _fontSize;
        IBrushPtr _foreground;
        TextAlignment _alignment;
        TextWrapping _wrapping;
        TextTrimming _trimming;
        double _maxWidth;
        double _maxHeight;
        double _requestedLineHeight;
        std::int32_t _maxLines;
        double _letterSpacing;
        std::shared_ptr<Skia::Typeface> _face;
        std::vector<TextLine> _lines;
        double _width = 0.0;
        double _widthWithWhitespace = 0.0;
        double _height = 0.0;
        double _baseline = 0.0;
        double _lineHeight = 0.0;
        double _ascent = 0.0;
    };

    [[nodiscard]] std::u32string ToUtf32(std::string_view text);
    [[nodiscard]] std::string ToUtf8(std::u32string_view text);

    class FormattedText final
    {
    public:
        // new FormattedText(text, culture, flow, typeface, emSize, foreground).
        FormattedText(std::string_view text, const void* culture, FlowDirection flowDirection, Typeface typeface,
            double emSize, IBrushPtr foreground);

        [[nodiscard]] double Width() const;
        [[nodiscard]] double WidthIncludingTrailingWhitespace() const;
        [[nodiscard]] double Height() const;
        [[nodiscard]] double Baseline() const;
        [[nodiscard]] const std::u32string& Text() const noexcept { return _text; }

        [[nodiscard]] double MaxTextWidth() const noexcept { return _maxTextWidth; }
        void MaxTextWidth(double value) { _maxTextWidth = value; _layout.reset(); }
        [[nodiscard]] double MaxTextHeight() const noexcept { return _maxTextHeight; }
        void MaxTextHeight(double value) { _maxTextHeight = value; _layout.reset(); }
        [[nodiscard]] TextTrimming Trimming() const noexcept { return _trimming; }
        void Trimming(TextTrimming value) { _trimming = value; _layout.reset(); }
        [[nodiscard]] double LineHeight() const noexcept { return _lineHeight; }
        void LineHeight(double value) { _lineHeight = value; _layout.reset(); }
        [[nodiscard]] Media::TextAlignment TextAlignment() const noexcept { return _alignment; }
        void TextAlignment(Media::TextAlignment value) { _alignment = value; _layout.reset(); }
        void SetForegroundBrush(IBrushPtr brush) { _foreground = std::move(brush); _layout.reset(); }

        [[nodiscard]] const Media::TextLayout& Layout() const;

    private:
        std::u32string _text;
        Typeface _typeface;
        double _emSize;
        IBrushPtr _foreground;
        double _maxTextWidth = std::numeric_limits<double>::infinity();
        double _maxTextHeight = std::numeric_limits<double>::infinity();
        TextTrimming _trimming = TextTrimming::None;
        double _lineHeight = std::numeric_limits<double>::quiet_NaN();
        Media::TextAlignment _alignment = Media::TextAlignment::Left;
        mutable std::shared_ptr<Media::TextLayout> _layout;
    };

    // CultureInfo.InvariantCulture, as a token: text here is never localised.
    inline constexpr const void* InvariantCulture = nullptr;

    // ----------------------------------------------------------- transforms

    class Transform
    {
    public:
        virtual ~Transform() = default;
        [[nodiscard]] virtual Avalonia::Matrix Value() const = 0;
    };

    using TransformPtr = std::shared_ptr<Transform>;

    class ScaleTransform final : public Transform
    {
    public:
        ScaleTransform() = default;
        ScaleTransform(double x, double y)
            : ScaleX(x), ScaleY(y)
        {
        }
        double ScaleX = 1.0;
        double ScaleY = 1.0;
        [[nodiscard]] Avalonia::Matrix Value() const override { return Avalonia::Matrix::CreateScale(ScaleX, ScaleY); }
    };

    class TranslateTransform final : public Transform
    {
    public:
        TranslateTransform() = default;
        TranslateTransform(double x, double y)
            : X(x), Y(y)
        {
        }
        double X = 0.0;
        double Y = 0.0;
        [[nodiscard]] Avalonia::Matrix Value() const override { return Avalonia::Matrix::CreateTranslation(X, Y); }
    };

    class RotateTransform final : public Transform
    {
    public:
        RotateTransform() = default;
        explicit RotateTransform(double angle)
            : Angle(angle)
        {
        }
        double Angle = 0.0;
        [[nodiscard]] Avalonia::Matrix Value() const override;
    };

    class MatrixTransform final : public Transform
    {
    public:
        MatrixTransform() = default;
        explicit MatrixTransform(Avalonia::Matrix matrix)
            : Matrix(matrix)
        {
        }
        Avalonia::Matrix Matrix{};
        [[nodiscard]] Avalonia::Matrix Value() const override { return Matrix; }
    };

    class TransformGroup final : public Transform
    {
    public:
        std::vector<TransformPtr> Children;
        [[nodiscard]] Avalonia::Matrix Value() const override;
    };

    // ------------------------------------------------------------- geometry

    enum class FillRule : std::int32_t { EvenOdd, NonZero };
    enum class SweepDirection : std::int32_t { CounterClockwise, Clockwise };

    class Geometry
    {
    public:
        virtual ~Geometry() = default;
        [[nodiscard]] virtual Skia::Path ToPath() const = 0;
        [[nodiscard]] Rect Bounds() const;
        TransformPtr Transform{};
    };

    using GeometryPtr = std::shared_ptr<Geometry>;

    class RectangleGeometry final : public Geometry
    {
    public:
        RectangleGeometry() = default;
        explicit RectangleGeometry(Avalonia::Rect rect)
            : Rect(rect)
        {
        }
        Avalonia::Rect Rect{};
        [[nodiscard]] Skia::Path ToPath() const override;
    };

    class EllipseGeometry final : public Geometry
    {
    public:
        EllipseGeometry() = default;
        explicit EllipseGeometry(Avalonia::Rect rect);
        Avalonia::Point Center{};
        double RadiusX = 0.0;
        double RadiusY = 0.0;
        [[nodiscard]] Skia::Path ToPath() const override;
    };

    class StreamGeometryContext final
    {
    public:
        explicit StreamGeometryContext(Skia::Path& path)
            : _path(path)
        {
        }
        void SetFillRule(FillRule rule);
        void BeginFigure(Point startPoint, bool isFilled = true);
        void LineTo(Point point, bool isStroked = true);
        void QuadraticBezierTo(Point control, Point end, bool isStroked = true);
        void CubicBezierTo(Point c1, Point c2, Point end, bool isStroked = true);
        void ArcTo(Point point, Size size, double rotationAngle, bool isLargeArc, SweepDirection sweepDirection,
            bool isStroked = true);
        void EndFigure(bool isClosed);
        // IDisposable: nothing to release.
        void Dispose() noexcept {}

    private:
        Skia::Path& _path;
    };

    class StreamGeometry final : public Geometry
    {
    public:
        [[nodiscard]] StreamGeometryContext Open() { _path = Skia::Path{}; return StreamGeometryContext(_path); }
        [[nodiscard]] Skia::Path ToPath() const override { return _path; }

    private:
        Skia::Path _path;
    };

    // ------------------------------------------------------------- shadows

    struct BoxShadow final
    {
        double OffsetX = 0.0;
        double OffsetY = 0.0;
        double Blur = 0.0;
        double Spread = 0.0;
        Media::Color Color{};
        bool IsInset = false;

        friend bool operator==(const BoxShadow&, const BoxShadow&) = default;
    };

    class BoxShadows final
    {
    public:
        BoxShadows() = default;
        explicit BoxShadows(BoxShadow shadow)
            : _shadows{shadow}
        {
        }
        BoxShadows(BoxShadow first, std::vector<BoxShadow> rest)
            : _shadows{first}
        {
            _shadows.insert(_shadows.end(), rest.begin(), rest.end());
        }
        [[nodiscard]] std::size_t Count() const noexcept { return _shadows.size(); }
        [[nodiscard]] const std::vector<BoxShadow>& Items() const noexcept { return _shadows; }
        friend bool operator==(const BoxShadows&, const BoxShadows&) = default;

    private:
        std::vector<BoxShadow> _shadows;
    };

    // ------------------------------------------------------------- imaging

    enum class BitmapInterpolationMode : std::int32_t { Unspecified, None, LowQuality, MediumQuality, HighQuality };

    struct RenderOptions final
    {
        BitmapInterpolationMode BitmapInterpolationMode = BitmapInterpolationMode::Unspecified;
        std::optional<bool> RequiresFullOpacityHandling{};
    };

    // IImage.
    class IImage
    {
    public:
        virtual ~IImage() = default;
        [[nodiscard]] virtual Avalonia::Size Size() const = 0;
        [[nodiscard]] virtual const Skia::Bitmap* Pixels() const = 0;
    };

    namespace Imaging
    {
        class Bitmap : public IImage
        {
        public:
            // new Bitmap(stream) / new Bitmap(path).
            [[nodiscard]] static std::shared_ptr<Bitmap> FromBytes(const std::vector<std::uint8_t>& bytes);
            [[nodiscard]] static std::shared_ptr<Bitmap> FromFile(const std::string& path);
            explicit Bitmap(std::shared_ptr<Skia::Bitmap> pixels);

            [[nodiscard]] Avalonia::Size Size() const override;
            [[nodiscard]] Avalonia::PixelSize PixelSize() const;
            [[nodiscard]] const Skia::Bitmap* Pixels() const override { return _pixels.get(); }
            [[nodiscard]] Skia::Bitmap* MutablePixels() const { return _pixels.get(); }

        protected:
            std::shared_ptr<Skia::Bitmap> _pixels;
        };

        using BitmapPtr = std::shared_ptr<Bitmap>;
    }

    // -------------------------------------------------------- the context

    class DrawingContext final
    {
    public:
        explicit DrawingContext(Skia::Canvas& canvas);

        void DrawText(const FormattedText& text, Point origin);
        void DrawRectangle(const IBrushPtr& brush, const IPenPtr& pen, const Rect& rect, double radiusX = 0.0,
            double radiusY = 0.0, const BoxShadows& boxShadows = {});
        void DrawRectangle(const IBrushPtr& brush, const IPenPtr& pen, const Rect& rect, const CornerRadius& radius,
            const BoxShadows& boxShadows = {});
        void DrawRectangle(const IPenPtr& pen, const Rect& rect, double cornerRadius = 0.0);
        void FillRectangle(const IBrushPtr& brush, const Rect& rect, double cornerRadius = 0.0);
        void DrawEllipse(const IBrushPtr& brush, const IPenPtr& pen, Point center, double radiusX, double radiusY);
        void DrawEllipse(const IBrushPtr& brush, const IPenPtr& pen, const Rect& rect);
        void DrawLine(const IPenPtr& pen, Point p1, Point p2);
        void DrawGeometry(const IBrushPtr& brush, const IPenPtr& pen, const Geometry& geometry);
        void DrawImage(const IImage& source, const Rect& sourceRect, const Rect& destRect);
        void DrawImage(const IImage& source, const Rect& destRect);

        // Each Push returns a state whose Dispose pops it, as using(...) does.
        class PushedState final
        {
        public:
            PushedState(DrawingContext* context, std::size_t depth) noexcept
                : _context(context), _depth(depth)
            {
            }
            PushedState(PushedState&& other) noexcept
                : _context(std::exchange(other._context, nullptr)), _depth(other._depth)
            {
            }
            PushedState(const PushedState&) = delete;
            PushedState& operator=(const PushedState&) = delete;
            PushedState& operator=(PushedState&&) = delete;
            ~PushedState() { Dispose(); }
            void Dispose() noexcept;

        private:
            DrawingContext* _context;
            std::size_t _depth;
        };

        [[nodiscard]] PushedState PushClip(const Rect& clip);
        [[nodiscard]] PushedState PushClip(const Rect& clip, const CornerRadius& radius);
        [[nodiscard]] PushedState PushGeometryClip(const Geometry& clip);
        [[nodiscard]] PushedState PushOpacity(double opacity);
        [[nodiscard]] PushedState PushTransform(const Avalonia::Matrix& matrix);
        [[nodiscard]] PushedState PushRenderOptions(const RenderOptions& options);

        [[nodiscard]] Skia::Canvas& Canvas() noexcept { return _canvas; }
        [[nodiscard]] const Avalonia::Matrix& CurrentTransform() const noexcept { return _transform.back(); }

    private:
        void Fill(const Skia::Path& path, const IBrushPtr& brush, const Rect& bounds);
        void Stroke(const Skia::Path& path, const IPenPtr& pen, const Rect& bounds);
        [[nodiscard]] std::optional<Skia::Paint> PaintFor(const IBrushPtr& brush, const Rect& bounds) const;
        [[nodiscard]] PushedState Pushed();
        void PopTo(std::size_t depth) noexcept;

        Skia::Canvas& _canvas;
        std::vector<Avalonia::Matrix> _transform{Avalonia::Matrix::Identity()};
        std::vector<RenderOptions> _options{RenderOptions{}};
        std::size_t _baseSaves = 0;
        friend class PushedState;
    };

    [[nodiscard]] Skia::Matrix ToSkia(const Avalonia::Matrix& matrix) noexcept;
    [[nodiscard]] Skia::Color ToSkia(Color color) noexcept;
    [[nodiscard]] Skia::Rect ToSkia(const Rect& rect) noexcept;
}
