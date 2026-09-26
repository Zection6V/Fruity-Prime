#pragma once

// The part of SkiaSharp that Avalonia's Skia backend draws the launcher's
// screens with, reproduced on the CPU: a premultiplied RGBA surface, paths
// filled with anti-aliased coverage, strokes, solid and gradient paint, clips,
// transforms, opacity layers, images, blurred shadows and FreeType text.
//
// This is what the C# build gets from libSkiaSharp. The launcher never asks it
// for anything a 2D canvas does not do, so this is a canvas and nothing more.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::NativeRuntime::Skia
{
    struct Point final
    {
        double X = 0.0;
        double Y = 0.0;
    };

    struct Rect final
    {
        double Left = 0.0;
        double Top = 0.0;
        double Right = 0.0;
        double Bottom = 0.0;

        [[nodiscard]] static constexpr Rect FromXYWH(double x, double y, double width, double height) noexcept
        {
            return Rect{x, y, x + width, y + height};
        }
        [[nodiscard]] constexpr double Width() const noexcept { return Right - Left; }
        [[nodiscard]] constexpr double Height() const noexcept { return Bottom - Top; }
        [[nodiscard]] constexpr bool IsEmpty() const noexcept { return !(Right > Left && Bottom > Top); }
        [[nodiscard]] Rect Intersect(const Rect& other) const noexcept;
        [[nodiscard]] Rect Union(const Rect& other) const noexcept;
    };

    // Premultiplied is what is stored; this is straight colour, as a brush has it.
    struct Color final
    {
        std::uint8_t R = 0;
        std::uint8_t G = 0;
        std::uint8_t B = 0;
        std::uint8_t A = 0;
    };

    // An affine transform: x' = ScaleX*x + SkewX*y + TransX, y' = SkewY*x + ScaleY*y + TransY.
    struct Matrix final
    {
        double ScaleX = 1.0;
        double SkewX = 0.0;
        double TransX = 0.0;
        double SkewY = 0.0;
        double ScaleY = 1.0;
        double TransY = 0.0;

        [[nodiscard]] static Matrix Translation(double x, double y) noexcept;
        [[nodiscard]] static Matrix Scale(double x, double y) noexcept;
        [[nodiscard]] static Matrix Rotation(double radians) noexcept;
        // this, then other: a point goes through this first.
        [[nodiscard]] Matrix Then(const Matrix& other) const noexcept;
        [[nodiscard]] std::optional<Matrix> Invert() const noexcept;
        [[nodiscard]] Point Map(Point p) const noexcept;
        [[nodiscard]] Rect MapRect(const Rect& rect) const noexcept;
        [[nodiscard]] bool IsIdentity() const noexcept;
        [[nodiscard]] bool IsScaleTranslate() const noexcept;
    };

    enum class FillRule : std::uint8_t { NonZero, EvenOdd };

    class Path final
    {
    public:
        void MoveTo(Point p);
        void LineTo(Point p);
        void QuadTo(Point control, Point end);
        void CubicTo(Point c1, Point c2, Point end);
        // SVG's elliptical arc, which is what a StreamGeometry's ArcTo is.
        void ArcTo(Point end, double radiusX, double radiusY, double rotationDegrees, bool largeArc, bool clockwise);
        void Close();

        void AddRect(const Rect& rect);
        // Radii per corner, clockwise from the top left, each (x, y).
        void AddRoundRect(const Rect& rect, std::array<Point, 4> radii);
        void AddEllipse(Point centre, double radiusX, double radiusY);
        void AddPath(const Path& other);

        [[nodiscard]] bool IsEmpty() const noexcept { return _verbs.empty(); }
        [[nodiscard]] Rect Bounds() const noexcept;

        FillRule Rule = FillRule::NonZero;

        // The outline as polylines in device space, each marked closed or open.
        struct Contour final
        {
            std::vector<Point> Points;
            bool Closed = false;
        };
        [[nodiscard]] std::vector<Contour> Flatten(const Matrix& matrix, double tolerance = 0.2) const;

    private:
        enum class Verb : std::uint8_t { Move, Line, Quad, Cubic, Close };
        std::vector<Verb> _verbs;
        std::vector<Point> _points;
        Point _last{};
        Point _start{};
    };

    enum class LineCap : std::uint8_t { Butt, Round, Square };
    enum class LineJoin : std::uint8_t { Miter, Round, Bevel };

    struct GradientStop final
    {
        Color StopColor{};
        double Offset = 0.0;
    };

    enum class SpreadMethod : std::uint8_t { Pad, Reflect, Repeat };

    // What colour a pixel gets, before coverage.
    class Shader
    {
    public:
        virtual ~Shader() = default;
        // Premultiplied, 0..1, at a device pixel centre.
        virtual void Shade(double x, double y, float out[4]) const = 0;
    };

    [[nodiscard]] std::shared_ptr<Shader> LinearGradient(Point start, Point end, std::vector<GradientStop> stops,
        SpreadMethod spread, const Matrix& localToDevice);
    [[nodiscard]] std::shared_ptr<Shader> RadialGradient(Point centre, Point origin, double radiusX, double radiusY,
        std::vector<GradientStop> stops, SpreadMethod spread, const Matrix& localToDevice);

    struct Paint final
    {
        Color Solid{0, 0, 0, 255};
        std::shared_ptr<Shader> Gradient{};
        double Opacity = 1.0;
    };

    struct StrokeStyle final
    {
        double Width = 1.0;
        LineCap Cap = LineCap::Butt;
        LineJoin Join = LineJoin::Miter;
        double MiterLimit = 10.0;
        std::vector<double> Dashes{};
        double DashOffset = 0.0;
    };

    // Premultiplied RGBA, rows top first, tightly packed.
    class Bitmap final
    {
    public:
        Bitmap() = default;
        Bitmap(std::int32_t width, std::int32_t height);

        [[nodiscard]] std::int32_t Width() const noexcept { return _width; }
        [[nodiscard]] std::int32_t Height() const noexcept { return _height; }
        [[nodiscard]] std::uint8_t* Pixels() noexcept { return _pixels.data(); }
        [[nodiscard]] const std::uint8_t* Pixels() const noexcept { return _pixels.data(); }
        void Clear(Color color = {});
        void Resize(std::int32_t width, std::int32_t height);

        // Straight RGBA in, as a decoded file is.
        [[nodiscard]] static std::shared_ptr<Bitmap> FromStraightRgba(
            std::int32_t width, std::int32_t height, const std::uint8_t* rgba);
        // PNG, JPEG, BMP: whatever stb_image reads. Null when it cannot.
        [[nodiscard]] static std::shared_ptr<Bitmap> Decode(const std::uint8_t* data, std::size_t length);

    private:
        std::int32_t _width = 0;
        std::int32_t _height = 0;
        std::vector<std::uint8_t> _pixels;
    };

    enum class FilterQuality : std::uint8_t { None, Low, Medium, High };

    // A font file loaded once and kept for the process.
    class Typeface final
    {
    public:
        [[nodiscard]] static std::shared_ptr<Typeface> FromFile(const std::string& path);
        [[nodiscard]] static std::shared_ptr<Typeface> FromData(std::vector<std::uint8_t> data);
        // The platform's default sans serif, at a weight.
        [[nodiscard]] static std::shared_ptr<Typeface> Default(std::int32_t weight);
        ~Typeface();

        struct Metrics final
        {
            double Ascent = 0.0;
            double Descent = 0.0;
            double LineGap = 0.0;
        };
        // In pixels at this size, ascent and descent both positive.
        [[nodiscard]] Metrics MetricsAt(double size) const;
        [[nodiscard]] double Advance(char32_t code, double size) const;
        [[nodiscard]] double Kerning(char32_t left, char32_t right, double size) const;
        [[nodiscard]] bool HasGlyph(char32_t code) const;

        struct GlyphImage final
        {
            std::int32_t Width = 0;
            std::int32_t Height = 0;
            std::int32_t Left = 0;
            std::int32_t Top = 0;
            std::vector<std::uint8_t> Coverage;
        };
        [[nodiscard]] const GlyphImage* Rasterize(char32_t code, double size) const;

    private:
        Typeface() = default;
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };

    struct BoxShadowSpec final
    {
        double OffsetX = 0.0;
        double OffsetY = 0.0;
        double Blur = 0.0;
        double Spread = 0.0;
        Color ShadowColor{};
        bool Inset = false;
    };

    class Canvas final
    {
    public:
        explicit Canvas(Bitmap& target);

        [[nodiscard]] std::int32_t Width() const noexcept { return _base.Width(); }
        [[nodiscard]] std::int32_t Height() const noexcept { return _base.Height(); }

        void Clear(Color color);

        void Save();
        void Restore();
        [[nodiscard]] std::size_t SaveCount() const noexcept { return _states.size(); }

        void Concat(const Matrix& matrix);
        void SetMatrix(const Matrix& matrix);
        [[nodiscard]] const Matrix& TotalMatrix() const noexcept;

        void ClipRect(const Rect& rect, bool antialias = true);
        void ClipPath(const Path& path, bool antialias = true);
        // The device rectangle drawing is limited to, for a caller that can skip work.
        [[nodiscard]] Rect DeviceClipBounds() const noexcept;

        // Everything until the matching Restore is drawn into a layer and
        // composited with this opacity.
        void SaveLayerAlpha(double opacity);

        void FillPath(const Path& path, const Paint& paint);
        void StrokePath(const Path& path, const StrokeStyle& stroke, const Paint& paint);
        void DrawBitmap(const Bitmap& bitmap, const Rect& source, const Rect& destination, FilterQuality quality,
            double opacity);
        void DrawBoxShadow(const Path& shape, const BoxShadowSpec& shadow, const Rect& shapeBounds,
            const std::array<Point, 4>& radii);
        // One run of text with its baseline starting at the origin, in local
        // coordinates. Kerning applied; no shaping beyond that.
        void DrawText(std::u32string_view text, const Typeface& typeface, double size, Point origin,
            const Paint& paint);

    private:
        struct Mask final
        {
            // Device rectangle the mask covers; outside it coverage is zero.
            std::int32_t X = 0;
            std::int32_t Y = 0;
            std::int32_t Width = 0;
            std::int32_t Height = 0;
            std::vector<float> Coverage;
        };

        struct State final
        {
            Matrix Transform{};
            // Integer device rectangle every draw is limited to.
            std::int32_t ClipLeft = 0;
            std::int32_t ClipTop = 0;
            std::int32_t ClipRight = 0;
            std::int32_t ClipBottom = 0;
            std::shared_ptr<const Mask> ClipMask{};
            // Set when this save opened a layer.
            std::shared_ptr<Bitmap> Layer{};
            double LayerOpacity = 1.0;
        };

        [[nodiscard]] Bitmap& Target() noexcept;
        [[nodiscard]] State& Current() noexcept { return _states.back(); }
        [[nodiscard]] const State& Current() const noexcept { return _states.back(); }

        // Coverage of a set of device-space contours within the clip bounds.
        [[nodiscard]] Mask Rasterize(const std::vector<Path::Contour>& contours, FillRule rule, bool antialias) const;
        void Blend(const Mask& coverage, const Paint& paint);
        void BlendSpan(std::int32_t y, std::int32_t x0, std::int32_t x1, const float* coverage, const Paint& paint,
            float solid[4]);
        [[nodiscard]] float ClipAt(std::int32_t x, std::int32_t y) const noexcept;

        Bitmap& _base;
        std::vector<State> _states;
    };

    // The outline of a stroke as closed polygons, all wound the same way, so
    // a non-zero fill of the result is the union.
    [[nodiscard]] std::vector<Path::Contour> StrokeContours(
        const std::vector<Path::Contour>& contours, const StrokeStyle& stroke, double scale);

    // SkiaSharpExtensions.ConvertRadiusToSigma, which is how Avalonia turns a
    // BoxShadow's blur radius into a Gaussian.
    [[nodiscard]] constexpr double ConvertRadiusToSigma(double radius) noexcept
    {
        return radius > 0 ? 0.288675 * radius + 0.5 : 0.0;
    }
}
