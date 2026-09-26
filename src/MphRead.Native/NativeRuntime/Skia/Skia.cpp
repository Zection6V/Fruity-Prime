#include "Skia.hpp"

#include "../Stb/Image.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <utility>

#if !defined(__ANDROID__)
#include <jpeglib.h>
#include <csetjmp>
#endif

// windows.h, pulled in by jpeglib on some toolchains, renames DrawText.
#ifdef DrawText
#undef DrawText
#endif

namespace MphRead::NativeRuntime::Skia
{
    // ------------------------------------------------------------ geometry

    Rect Rect::Intersect(const Rect& other) const noexcept
    {
        return Rect{std::max(Left, other.Left), std::max(Top, other.Top), std::min(Right, other.Right),
            std::min(Bottom, other.Bottom)};
    }

    Rect Rect::Union(const Rect& other) const noexcept
    {
        if (IsEmpty())
        {
            return other;
        }
        if (other.IsEmpty())
        {
            return *this;
        }
        return Rect{std::min(Left, other.Left), std::min(Top, other.Top), std::max(Right, other.Right),
            std::max(Bottom, other.Bottom)};
    }

    Matrix Matrix::Translation(double x, double y) noexcept
    {
        Matrix m;
        m.TransX = x;
        m.TransY = y;
        return m;
    }

    Matrix Matrix::Scale(double x, double y) noexcept
    {
        Matrix m;
        m.ScaleX = x;
        m.ScaleY = y;
        return m;
    }

    Matrix Matrix::Rotation(double radians) noexcept
    {
        Matrix m;
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        m.ScaleX = c;
        m.SkewX = -s;
        m.SkewY = s;
        m.ScaleY = c;
        return m;
    }

    Matrix Matrix::Then(const Matrix& o) const noexcept
    {
        Matrix m;
        m.ScaleX = o.ScaleX * ScaleX + o.SkewX * SkewY;
        m.SkewX = o.ScaleX * SkewX + o.SkewX * ScaleY;
        m.TransX = o.ScaleX * TransX + o.SkewX * TransY + o.TransX;
        m.SkewY = o.SkewY * ScaleX + o.ScaleY * SkewY;
        m.ScaleY = o.SkewY * SkewX + o.ScaleY * ScaleY;
        m.TransY = o.SkewY * TransX + o.ScaleY * TransY + o.TransY;
        return m;
    }

    std::optional<Matrix> Matrix::Invert() const noexcept
    {
        const double det = ScaleX * ScaleY - SkewX * SkewY;
        if (std::abs(det) < 1e-12)
        {
            return std::nullopt;
        }
        Matrix m;
        m.ScaleX = ScaleY / det;
        m.SkewX = -SkewX / det;
        m.SkewY = -SkewY / det;
        m.ScaleY = ScaleX / det;
        m.TransX = -(m.ScaleX * TransX + m.SkewX * TransY);
        m.TransY = -(m.SkewY * TransX + m.ScaleY * TransY);
        return m;
    }

    Point Matrix::Map(Point p) const noexcept
    {
        return Point{ScaleX * p.X + SkewX * p.Y + TransX, SkewY * p.X + ScaleY * p.Y + TransY};
    }

    Rect Matrix::MapRect(const Rect& r) const noexcept
    {
        const Point a = Map({r.Left, r.Top});
        const Point b = Map({r.Right, r.Top});
        const Point c = Map({r.Right, r.Bottom});
        const Point d = Map({r.Left, r.Bottom});
        return Rect{std::min({a.X, b.X, c.X, d.X}), std::min({a.Y, b.Y, c.Y, d.Y}), std::max({a.X, b.X, c.X, d.X}),
            std::max({a.Y, b.Y, c.Y, d.Y})};
    }

    bool Matrix::IsIdentity() const noexcept
    {
        return ScaleX == 1 && SkewX == 0 && TransX == 0 && SkewY == 0 && ScaleY == 1 && TransY == 0;
    }

    bool Matrix::IsScaleTranslate() const noexcept
    {
        return SkewX == 0 && SkewY == 0;
    }

    // ---------------------------------------------------------------- path

    void Path::MoveTo(Point p)
    {
        _verbs.push_back(Verb::Move);
        _points.push_back(p);
        _last = p;
        _start = p;
    }

    void Path::LineTo(Point p)
    {
        if (_verbs.empty())
        {
            MoveTo(_last);
        }
        _verbs.push_back(Verb::Line);
        _points.push_back(p);
        _last = p;
    }

    void Path::QuadTo(Point control, Point end)
    {
        if (_verbs.empty())
        {
            MoveTo(_last);
        }
        _verbs.push_back(Verb::Quad);
        _points.push_back(control);
        _points.push_back(end);
        _last = end;
    }

    void Path::CubicTo(Point c1, Point c2, Point end)
    {
        if (_verbs.empty())
        {
            MoveTo(_last);
        }
        _verbs.push_back(Verb::Cubic);
        _points.push_back(c1);
        _points.push_back(c2);
        _points.push_back(end);
        _last = end;
    }

    void Path::ArcTo(Point end, double rx, double ry, double rotationDegrees, bool largeArc, bool clockwise)
    {
        // SVG implementation notes F.6: endpoint to centre parameterisation,
        // then one cubic per quarter turn at most.
        const Point start = _last;
        rx = std::abs(rx);
        ry = std::abs(ry);
        if (rx < 1e-9 || ry < 1e-9 || (std::abs(start.X - end.X) < 1e-9 && std::abs(start.Y - end.Y) < 1e-9))
        {
            LineTo(end);
            return;
        }
        const double phi = rotationDegrees * std::numbers::pi / 180.0;
        const double cosPhi = std::cos(phi);
        const double sinPhi = std::sin(phi);
        const double dx = (start.X - end.X) / 2;
        const double dy = (start.Y - end.Y) / 2;
        const double x1 = cosPhi * dx + sinPhi * dy;
        const double y1 = -sinPhi * dx + cosPhi * dy;
        const double lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
        if (lambda > 1)
        {
            const double s = std::sqrt(lambda);
            rx *= s;
            ry *= s;
        }
        const double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
        const double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
        double coef = den == 0 ? 0 : std::sqrt(std::max(0.0, num / den));
        if (largeArc == clockwise)
        {
            coef = -coef;
        }
        const double cx1 = coef * rx * y1 / ry;
        const double cy1 = -coef * ry * x1 / rx;
        const double cx = cosPhi * cx1 - sinPhi * cy1 + (start.X + end.X) / 2;
        const double cy = sinPhi * cx1 + cosPhi * cy1 + (start.Y + end.Y) / 2;
        const auto angle = [](double ux, double uy, double vx, double vy)
        {
            const double a = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
            return a;
        };
        const double theta1 = angle(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry);
        double delta = angle((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);
        if (!clockwise && delta > 0)
        {
            delta -= 2 * std::numbers::pi;
        }
        else if (clockwise && delta < 0)
        {
            delta += 2 * std::numbers::pi;
        }
        const int segments = std::max(1, static_cast<int>(std::ceil(std::abs(delta) / (std::numbers::pi / 2))));
        const double step = delta / segments;
        const double k = 4.0 / 3.0 * std::tan(step / 4);
        double t = theta1;
        for (int i = 0; i < segments; i++)
        {
            const double c0 = std::cos(t);
            const double s0 = std::sin(t);
            const double c1 = std::cos(t + step);
            const double s1 = std::sin(t + step);
            const auto map = [&](double ex, double ey)
            {
                return Point{cx + cosPhi * rx * ex - sinPhi * ry * ey, cy + sinPhi * rx * ex + cosPhi * ry * ey};
            };
            const Point p1 = map(c0 - k * s0, s0 + k * c0);
            const Point p2 = map(c1 + k * s1, s1 - k * c1);
            const Point p3 = i == segments - 1 ? end : map(c1, s1);
            CubicTo(p1, p2, p3);
            t += step;
        }
    }

    void Path::Close()
    {
        if (!_verbs.empty() && _verbs.back() != Verb::Close)
        {
            _verbs.push_back(Verb::Close);
            _last = _start;
        }
    }

    void Path::AddRect(const Rect& r)
    {
        MoveTo({r.Left, r.Top});
        LineTo({r.Right, r.Top});
        LineTo({r.Right, r.Bottom});
        LineTo({r.Left, r.Bottom});
        Close();
    }

    void Path::AddRoundRect(const Rect& r, std::array<Point, 4> radii)
    {
        // Radii that do not fit are scaled down together, as Skia's SkRRect does.
        double scale = 1.0;
        const auto fit = [&scale](double sum, double limit)
        {
            if (sum > limit && sum > 0)
            {
                scale = std::min(scale, limit / sum);
            }
        };
        fit(radii[0].X + radii[1].X, r.Width());
        fit(radii[3].X + radii[2].X, r.Width());
        fit(radii[0].Y + radii[3].Y, r.Height());
        fit(radii[1].Y + radii[2].Y, r.Height());
        for (Point& radius : radii)
        {
            radius.X = std::max(0.0, radius.X * scale);
            radius.Y = std::max(0.0, radius.Y * scale);
        }
        if (radii[0].X == 0 && radii[1].X == 0 && radii[2].X == 0 && radii[3].X == 0)
        {
            AddRect(r);
            return;
        }
        constexpr double k = 0.5522847498307936;
        const Point tl = radii[0];
        const Point tr = radii[1];
        const Point br = radii[2];
        const Point bl = radii[3];
        MoveTo({r.Left + tl.X, r.Top});
        LineTo({r.Right - tr.X, r.Top});
        if (tr.X > 0 || tr.Y > 0)
        {
            CubicTo({r.Right - tr.X + tr.X * k, r.Top}, {r.Right, r.Top + tr.Y - tr.Y * k}, {r.Right, r.Top + tr.Y});
        }
        LineTo({r.Right, r.Bottom - br.Y});
        if (br.X > 0 || br.Y > 0)
        {
            CubicTo({r.Right, r.Bottom - br.Y + br.Y * k}, {r.Right - br.X + br.X * k, r.Bottom},
                {r.Right - br.X, r.Bottom});
        }
        LineTo({r.Left + bl.X, r.Bottom});
        if (bl.X > 0 || bl.Y > 0)
        {
            CubicTo({r.Left + bl.X - bl.X * k, r.Bottom}, {r.Left, r.Bottom - bl.Y + bl.Y * k},
                {r.Left, r.Bottom - bl.Y});
        }
        LineTo({r.Left, r.Top + tl.Y});
        if (tl.X > 0 || tl.Y > 0)
        {
            CubicTo({r.Left, r.Top + tl.Y - tl.Y * k}, {r.Left + tl.X - tl.X * k, r.Top}, {r.Left + tl.X, r.Top});
        }
        Close();
    }

    void Path::AddEllipse(Point c, double rx, double ry)
    {
        constexpr double k = 0.5522847498307936;
        MoveTo({c.X + rx, c.Y});
        CubicTo({c.X + rx, c.Y + ry * k}, {c.X + rx * k, c.Y + ry}, {c.X, c.Y + ry});
        CubicTo({c.X - rx * k, c.Y + ry}, {c.X - rx, c.Y + ry * k}, {c.X - rx, c.Y});
        CubicTo({c.X - rx, c.Y - ry * k}, {c.X - rx * k, c.Y - ry}, {c.X, c.Y - ry});
        CubicTo({c.X + rx * k, c.Y - ry}, {c.X + rx, c.Y - ry * k}, {c.X + rx, c.Y});
        Close();
    }

    void Path::AddPath(const Path& other)
    {
        _verbs.insert(_verbs.end(), other._verbs.begin(), other._verbs.end());
        _points.insert(_points.end(), other._points.begin(), other._points.end());
        _last = other._last;
        _start = other._start;
    }

    Rect Path::Bounds() const noexcept
    {
        if (_points.empty())
        {
            return {};
        }
        Rect r{_points[0].X, _points[0].Y, _points[0].X, _points[0].Y};
        for (const Point& p : _points)
        {
            r.Left = std::min(r.Left, p.X);
            r.Top = std::min(r.Top, p.Y);
            r.Right = std::max(r.Right, p.X);
            r.Bottom = std::max(r.Bottom, p.Y);
        }
        return r;
    }

    std::vector<Path::Contour> Path::Flatten(const Matrix& m, double tolerance) const
    {
        std::vector<Contour> contours;
        Contour* current = nullptr;
        Point last{};
        Point contourStart{};
        std::size_t index = 0;
        const auto begin = [&](Point p)
        {
            contours.push_back(Contour{});
            current = &contours.back();
            current->Points.push_back(m.Map(p));
            last = p;
            contourStart = p;
        };
        for (const Verb verb : _verbs)
        {
            switch (verb)
            {
            case Verb::Move:
                begin(_points[index++]);
                break;
            case Verb::Line:
            {
                if (current == nullptr)
                {
                    begin(last);
                }
                const Point p = _points[index++];
                current->Points.push_back(m.Map(p));
                last = p;
                break;
            }
            case Verb::Quad:
            {
                if (current == nullptr)
                {
                    begin(last);
                }
                const Point p0 = m.Map(last);
                const Point p1 = m.Map(_points[index]);
                const Point p2 = m.Map(_points[index + 1]);
                const double dd = std::hypot(p0.X - 2 * p1.X + p2.X, p0.Y - 2 * p1.Y + p2.Y);
                const int n = std::clamp(static_cast<int>(std::ceil(std::sqrt(dd / (4 * tolerance)))), 1, 256);
                for (int i = 1; i <= n; i++)
                {
                    const double t = static_cast<double>(i) / n;
                    const double u = 1 - t;
                    current->Points.push_back(Point{u * u * p0.X + 2 * u * t * p1.X + t * t * p2.X,
                        u * u * p0.Y + 2 * u * t * p1.Y + t * t * p2.Y});
                }
                last = _points[index + 1];
                index += 2;
                break;
            }
            case Verb::Cubic:
            {
                if (current == nullptr)
                {
                    begin(last);
                }
                const Point p0 = m.Map(last);
                const Point p1 = m.Map(_points[index]);
                const Point p2 = m.Map(_points[index + 1]);
                const Point p3 = m.Map(_points[index + 2]);
                const double d1 = std::hypot(p0.X - 2 * p1.X + p2.X, p0.Y - 2 * p1.Y + p2.Y);
                const double d2 = std::hypot(p1.X - 2 * p2.X + p3.X, p1.Y - 2 * p2.Y + p3.Y);
                const double dd = std::max(d1, d2) * 6;
                const int n = std::clamp(static_cast<int>(std::ceil(std::sqrt(dd / (8 * tolerance)))), 1, 256);
                for (int i = 1; i <= n; i++)
                {
                    const double t = static_cast<double>(i) / n;
                    const double u = 1 - t;
                    const double a = u * u * u;
                    const double b = 3 * u * u * t;
                    const double c = 3 * u * t * t;
                    const double d = t * t * t;
                    current->Points.push_back(Point{a * p0.X + b * p1.X + c * p2.X + d * p3.X,
                        a * p0.Y + b * p1.Y + c * p2.Y + d * p3.Y});
                }
                last = _points[index + 2];
                index += 3;
                break;
            }
            case Verb::Close:
                if (current != nullptr)
                {
                    current->Closed = true;
                    last = contourStart;
                    current = nullptr;
                }
                break;
            }
        }
        return contours;
    }

    // -------------------------------------------------------------- bitmap

    Bitmap::Bitmap(std::int32_t width, std::int32_t height)
    {
        Resize(width, height);
    }

    void Bitmap::Resize(std::int32_t width, std::int32_t height)
    {
        _width = std::max(0, width);
        _height = std::max(0, height);
        _pixels.assign(static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height) * 4, 0);
    }

    void Bitmap::Clear(Color color)
    {
        const float a = color.A / 255.0F;
        const std::uint8_t p[4]{static_cast<std::uint8_t>(std::lround(color.R * a)),
            static_cast<std::uint8_t>(std::lround(color.G * a)), static_cast<std::uint8_t>(std::lround(color.B * a)),
            color.A};
        for (std::size_t i = 0; i < _pixels.size(); i += 4)
        {
            std::memcpy(&_pixels[i], p, 4);
        }
    }

    std::shared_ptr<Bitmap> Bitmap::FromStraightRgba(std::int32_t width, std::int32_t height, const std::uint8_t* rgba)
    {
        auto bitmap = std::make_shared<Bitmap>(width, height);
        std::uint8_t* out = bitmap->Pixels();
        const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        for (std::size_t i = 0; i < count; i++)
        {
            const std::uint32_t a = rgba[i * 4 + 3];
            out[i * 4] = static_cast<std::uint8_t>((rgba[i * 4] * a + 127) / 255);
            out[i * 4 + 1] = static_cast<std::uint8_t>((rgba[i * 4 + 1] * a + 127) / 255);
            out[i * 4 + 2] = static_cast<std::uint8_t>((rgba[i * 4 + 2] * a + 127) / 255);
            out[i * 4 + 3] = static_cast<std::uint8_t>(a);
        }
        return bitmap;
    }

#if !defined(__ANDROID__)
    namespace
    {
        struct JpegError final
        {
            jpeg_error_mgr Manager{};
            std::jmp_buf Jump{};
        };

        void JpegExit(j_common_ptr info)
        {
            std::longjmp(reinterpret_cast<JpegError*>(info->err)->Jump, 1);
        }

        [[nodiscard]] std::shared_ptr<Bitmap> DecodeJpeg(const std::uint8_t* data, std::size_t length)
        {
            jpeg_decompress_struct info{};
            JpegError error{};
            info.err = jpeg_std_error(&error.Manager);
            error.Manager.error_exit = JpegExit;
            std::vector<std::uint8_t> rgba;
            std::int32_t width = 0;
            std::int32_t height = 0;
            if (setjmp(error.Jump) != 0)
            {
                jpeg_destroy_decompress(&info);
                return nullptr;
            }
            jpeg_create_decompress(&info);
            jpeg_mem_src(&info, data, static_cast<unsigned long>(length));
            jpeg_read_header(&info, TRUE);
            info.out_color_space = JCS_RGB;
            jpeg_start_decompress(&info);
            width = static_cast<std::int32_t>(info.output_width);
            height = static_cast<std::int32_t>(info.output_height);
            rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
            std::vector<std::uint8_t> row(static_cast<std::size_t>(width) * 3);
            while (info.output_scanline < info.output_height)
            {
                JSAMPROW rows[1]{row.data()};
                const std::size_t y = info.output_scanline;
                jpeg_read_scanlines(&info, rows, 1);
                for (std::int32_t x = 0; x < width; x++)
                {
                    std::uint8_t* p = &rgba[(y * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4];
                    p[0] = row[static_cast<std::size_t>(x) * 3];
                    p[1] = row[static_cast<std::size_t>(x) * 3 + 1];
                    p[2] = row[static_cast<std::size_t>(x) * 3 + 2];
                    p[3] = 255;
                }
            }
            jpeg_finish_decompress(&info);
            jpeg_destroy_decompress(&info);
            return Bitmap::FromStraightRgba(width, height, rgba.data());
        }
    }
#endif

    std::shared_ptr<Bitmap> Bitmap::Decode(const std::uint8_t* data, std::size_t length)
    {
        if (data == nullptr || length < 4)
        {
            return nullptr;
        }
        if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
        {
            const Image image = LoadPng(std::vector<std::uint8_t>(data, data + length), 4);
            if (image.Width <= 0 || image.Height <= 0)
            {
                return nullptr;
            }
            return FromStraightRgba(image.Width, image.Height, image.Pixels.data());
        }
#if !defined(__ANDROID__)
        if (data[0] == 0xFF && data[1] == 0xD8)
        {
            return DecodeJpeg(data, length);
        }
#endif
        return nullptr;
    }

    // ------------------------------------------------------------ gradients

    namespace
    {
        void StraightToPremul(const Color& c, double opacity, float out[4])
        {
            const float a = static_cast<float>(c.A / 255.0 * opacity);
            out[0] = c.R / 255.0F * a;
            out[1] = c.G / 255.0F * a;
            out[2] = c.B / 255.0F * a;
            out[3] = a;
        }

        class GradientBase : public Shader
        {
        public:
            GradientBase(std::vector<GradientStop> stops, SpreadMethod spread, const Matrix& localToDevice)
                : _spread(spread)
            {
                std::stable_sort(stops.begin(), stops.end(),
                    [](const GradientStop& a, const GradientStop& b) { return a.Offset < b.Offset; });
                // A 256-entry ramp, interpolated in premultiplied space as Skia does.
                for (int i = 0; i < 256; i++)
                {
                    const double t = i / 255.0;
                    float c[4]{0, 0, 0, 0};
                    if (stops.empty())
                    {
                    }
                    else if (t <= stops.front().Offset)
                    {
                        StraightToPremul(stops.front().StopColor, 1.0, c);
                    }
                    else if (t >= stops.back().Offset)
                    {
                        StraightToPremul(stops.back().StopColor, 1.0, c);
                    }
                    else
                    {
                        for (std::size_t s = 1; s < stops.size(); s++)
                        {
                            if (t <= stops[s].Offset)
                            {
                                const double span = stops[s].Offset - stops[s - 1].Offset;
                                const double f = span <= 0 ? 1.0 : (t - stops[s - 1].Offset) / span;
                                float a[4];
                                float b[4];
                                StraightToPremul(stops[s - 1].StopColor, 1.0, a);
                                StraightToPremul(stops[s].StopColor, 1.0, b);
                                for (int k = 0; k < 4; k++)
                                {
                                    c[k] = static_cast<float>(a[k] + (b[k] - a[k]) * f);
                                }
                                break;
                            }
                        }
                    }
                    std::memcpy(_ramp[static_cast<std::size_t>(i)].data(), c, sizeof(c));
                }
                _deviceToLocal = localToDevice.Invert().value_or(Matrix{});
            }

        protected:
            void Lookup(double t, float out[4]) const
            {
                switch (_spread)
                {
                case SpreadMethod::Pad:
                    t = std::clamp(t, 0.0, 1.0);
                    break;
                case SpreadMethod::Repeat:
                    t -= std::floor(t);
                    break;
                case SpreadMethod::Reflect:
                {
                    t = std::abs(std::fmod(t, 2.0));
                    if (t > 1)
                    {
                        t = 2 - t;
                    }
                    break;
                }
                }
                const auto& entry = _ramp[static_cast<std::size_t>(std::lround(t * 255))];
                std::memcpy(out, entry.data(), sizeof(float) * 4);
            }

            Matrix _deviceToLocal{};

        private:
            SpreadMethod _spread;
            std::array<std::array<float, 4>, 256> _ramp{};
        };

        class Linear final : public GradientBase
        {
        public:
            Linear(Point start, Point end, std::vector<GradientStop> stops, SpreadMethod spread, const Matrix& m)
                : GradientBase(std::move(stops), spread, m), _start(start)
            {
                _dx = end.X - start.X;
                _dy = end.Y - start.Y;
                const double length = _dx * _dx + _dy * _dy;
                _inverse = length <= 0 ? 0 : 1.0 / length;
            }

            void Shade(double x, double y, float out[4]) const override
            {
                const Point p = _deviceToLocal.Map({x, y});
                Lookup(((p.X - _start.X) * _dx + (p.Y - _start.Y) * _dy) * _inverse, out);
            }

        private:
            Point _start;
            double _dx = 0;
            double _dy = 0;
            double _inverse = 0;
        };

        class Radial final : public GradientBase
        {
        public:
            Radial(Point centre, Point origin, double rx, double ry, std::vector<GradientStop> stops,
                SpreadMethod spread, const Matrix& m)
                : GradientBase(std::move(stops), spread, m), _centre(centre), _origin(origin),
                  _rx(std::max(rx, 1e-9)), _ry(std::max(ry, 1e-9))
            {
            }

            void Shade(double x, double y, float out[4]) const override
            {
                const Point p = _deviceToLocal.Map({x, y});
                // Two-point conical with the focus at the origin, in the
                // circle's normalised space.
                const double px = (p.X - _centre.X) / _rx;
                const double py = (p.Y - _centre.Y) / _ry;
                const double fx = (_origin.X - _centre.X) / _rx;
                const double fy = (_origin.Y - _centre.Y) / _ry;
                if (std::abs(fx) < 1e-9 && std::abs(fy) < 1e-9)
                {
                    Lookup(std::sqrt(px * px + py * py), out);
                    return;
                }
                const double dx = px - fx;
                const double dy = py - fy;
                const double a = dx * dx + dy * dy;
                if (a <= 0)
                {
                    Lookup(0, out);
                    return;
                }
                const double b = 2 * (fx * dx + fy * dy);
                const double c = fx * fx + fy * fy - 1;
                const double disc = std::max(0.0, b * b - 4 * a * c);
                const double s = (-b + std::sqrt(disc)) / (2 * a);
                Lookup(s <= 0 ? 1e9 : 1.0 / s, out);
            }

        private:
            Point _centre;
            Point _origin;
            double _rx;
            double _ry;
        };
    }

    std::shared_ptr<Shader> LinearGradient(Point start, Point end, std::vector<GradientStop> stops,
        SpreadMethod spread, const Matrix& localToDevice)
    {
        return std::make_shared<Linear>(start, end, std::move(stops), spread, localToDevice);
    }

    std::shared_ptr<Shader> RadialGradient(Point centre, Point origin, double radiusX, double radiusY,
        std::vector<GradientStop> stops, SpreadMethod spread, const Matrix& localToDevice)
    {
        return std::make_shared<Radial>(centre, origin, radiusX, radiusY, std::move(stops), spread, localToDevice);
    }

    // -------------------------------------------------------------- stroke

    namespace
    {
        void AppendCircle(std::vector<Path::Contour>& out, Point c, double r)
        {
            const int n = std::clamp(static_cast<int>(std::ceil(r * 2)), 8, 64);
            Path::Contour contour;
            contour.Closed = true;
            for (int i = 0; i < n; i++)
            {
                const double a = 2 * std::numbers::pi * i / n;
                contour.Points.push_back({c.X + std::cos(a) * r, c.Y + std::sin(a) * r});
            }
            out.push_back(std::move(contour));
        }

        [[nodiscard]] double SignedArea(const std::vector<Point>& points)
        {
            double area = 0;
            for (std::size_t i = 0; i < points.size(); i++)
            {
                const Point& a = points[i];
                const Point& b = points[(i + 1) % points.size()];
                area += a.X * b.Y - b.X * a.Y;
            }
            return area / 2;
        }

        // Every polygon handed to the fill wound positively, so overlapping
        // pieces of one stroke add up rather than cancel.
        void Orient(Path::Contour& contour)
        {
            if (SignedArea(contour.Points) < 0)
            {
                std::reverse(contour.Points.begin(), contour.Points.end());
            }
        }

        void AppendQuad(std::vector<Path::Contour>& out, Point a, Point b, Point c, Point d)
        {
            Path::Contour q;
            q.Closed = true;
            q.Points = {a, b, c, d};
            Orient(q);
            out.push_back(std::move(q));
        }

        [[nodiscard]] std::vector<std::vector<Point>> Dash(const Path::Contour& contour, const std::vector<double>& dashes,
            double offset, double scale)
        {
            std::vector<std::vector<Point>> pieces;
            std::vector<Point> points = contour.Points;
            if (contour.Closed && !points.empty())
            {
                points.push_back(points.front());
            }
            double total = 0;
            for (const double dash : dashes)
            {
                total += dash * scale;
            }
            if (total <= 0 || points.size() < 2)
            {
                pieces.push_back(points);
                return pieces;
            }
            std::size_t index = 0;
            double remaining = dashes[0] * scale;
            bool on = true;
            double skip = std::fmod(offset * scale, total);
            while (skip > 0)
            {
                if (skip >= remaining)
                {
                    skip -= remaining;
                    index = (index + 1) % dashes.size();
                    remaining = dashes[index] * scale;
                    on = !on;
                }
                else
                {
                    remaining -= skip;
                    skip = 0;
                }
            }
            std::vector<Point> current;
            if (on)
            {
                current.push_back(points[0]);
            }
            for (std::size_t i = 1; i < points.size(); i++)
            {
                Point a = points[i - 1];
                const Point b = points[i];
                double length = std::hypot(b.X - a.X, b.Y - a.Y);
                while (length > 0)
                {
                    if (length <= remaining)
                    {
                        remaining -= length;
                        if (on)
                        {
                            current.push_back(b);
                        }
                        length = 0;
                    }
                    else
                    {
                        const double f = remaining / length;
                        const Point cut{a.X + (b.X - a.X) * f, a.Y + (b.Y - a.Y) * f};
                        if (on)
                        {
                            current.push_back(cut);
                            pieces.push_back(current);
                            current.clear();
                        }
                        else
                        {
                            current.push_back(cut);
                        }
                        on = !on;
                        length -= remaining;
                        a = cut;
                        index = (index + 1) % dashes.size();
                        remaining = dashes[index] * scale;
                    }
                }
            }
            if (on && current.size() > 1)
            {
                pieces.push_back(current);
            }
            return pieces;
        }
    }

    std::vector<Path::Contour> StrokeContours(const std::vector<Path::Contour>& contours, const StrokeStyle& stroke,
        double scale)
    {
        std::vector<Path::Contour> out;
        const double half = stroke.Width * scale / 2;
        if (half <= 0)
        {
            return out;
        }
        for (const Path::Contour& source : contours)
        {
            std::vector<std::vector<Point>> pieces;
            bool closed = source.Closed;
            if (!stroke.Dashes.empty())
            {
                pieces = Dash(source, stroke.Dashes, stroke.DashOffset, stroke.Width * scale);
                closed = false;
            }
            else
            {
                pieces.push_back(source.Points);
            }
            for (std::vector<Point> points : pieces)
            {
                // Drop repeated points: a zero-length segment has no direction.
                std::vector<Point> clean;
                for (const Point& p : points)
                {
                    if (clean.empty() || std::hypot(p.X - clean.back().X, p.Y - clean.back().Y) > 1e-6)
                    {
                        clean.push_back(p);
                    }
                }
                if (closed && clean.size() > 2
                    && std::hypot(clean.front().X - clean.back().X, clean.front().Y - clean.back().Y) <= 1e-6)
                {
                    clean.pop_back();
                }
                if (clean.size() == 1)
                {
                    if (stroke.Cap == LineCap::Round)
                    {
                        AppendCircle(out, clean[0], half);
                    }
                    else if (stroke.Cap == LineCap::Square)
                    {
                        const Point c = clean[0];
                        AppendQuad(out, {c.X - half, c.Y - half}, {c.X + half, c.Y - half}, {c.X + half, c.Y + half},
                            {c.X - half, c.Y + half});
                    }
                    continue;
                }
                const std::size_t count = clean.size();
                const std::size_t segments = closed ? count : count - 1;
                for (std::size_t i = 0; i < segments; i++)
                {
                    Point a = clean[i];
                    Point b = clean[(i + 1) % count];
                    const double length = std::hypot(b.X - a.X, b.Y - a.Y);
                    const double ux = (b.X - a.X) / length;
                    const double uy = (b.Y - a.Y) / length;
                    if (!closed && stroke.Cap == LineCap::Square)
                    {
                        if (i == 0)
                        {
                            a = {a.X - ux * half, a.Y - uy * half};
                        }
                        if (i == segments - 1)
                        {
                            b = {b.X + ux * half, b.Y + uy * half};
                        }
                    }
                    const double nx = -uy * half;
                    const double ny = ux * half;
                    AppendQuad(out, {a.X + nx, a.Y + ny}, {b.X + nx, b.Y + ny}, {b.X - nx, b.Y - ny},
                        {a.X - nx, a.Y - ny});
                }
                // Joins, at every interior vertex (every vertex when closed).
                const std::size_t first = closed ? 0 : 1;
                const std::size_t last = closed ? count : count - 1;
                for (std::size_t i = first; i < last; i++)
                {
                    const Point p = clean[i];
                    const Point prev = clean[(i + count - 1) % count];
                    const Point next = clean[(i + 1) % count];
                    const double l0 = std::hypot(p.X - prev.X, p.Y - prev.Y);
                    const double l1 = std::hypot(next.X - p.X, next.Y - p.Y);
                    const double u0x = (p.X - prev.X) / l0;
                    const double u0y = (p.Y - prev.Y) / l0;
                    const double u1x = (next.X - p.X) / l1;
                    const double u1y = (next.Y - p.Y) / l1;
                    const double cross = u0x * u1y - u0y * u1x;
                    if (std::abs(cross) < 1e-9 && u0x * u1x + u0y * u1y > 0)
                    {
                        continue;
                    }
                    if (stroke.Join == LineJoin::Round)
                    {
                        AppendCircle(out, p, half);
                        continue;
                    }
                    // The outer side of the turn.
                    const double side = cross > 0 ? -1.0 : 1.0;
                    const Point o0{p.X - u0y * half * side, p.Y + u0x * half * side};
                    const Point o1{p.X - u1y * half * side, p.Y + u1x * half * side};
                    Path::Contour wedge;
                    wedge.Closed = true;
                    wedge.Points = {p, o0};
                    if (stroke.Join == LineJoin::Miter)
                    {
                        const double cosTheta = u0x * u1x + u0y * u1y;
                        const double miterLength = 1.0 / std::sqrt(std::max(1e-12, (1 + cosTheta) / 2));
                        if (miterLength <= stroke.MiterLimit)
                        {
                            const double mx = (o0.X - p.X) + (o1.X - p.X);
                            const double my = (o0.Y - p.Y) + (o1.Y - p.Y);
                            const double ml = std::hypot(mx, my);
                            if (ml > 1e-9)
                            {
                                wedge.Points.push_back({p.X + mx / ml * half * miterLength,
                                    p.Y + my / ml * half * miterLength});
                            }
                        }
                    }
                    wedge.Points.push_back(o1);
                    Orient(wedge);
                    out.push_back(std::move(wedge));
                }
                if (!closed && stroke.Cap == LineCap::Round)
                {
                    AppendCircle(out, clean.front(), half);
                    AppendCircle(out, clean.back(), half);
                }
            }
        }
        return out;
    }

    // -------------------------------------------------------------- canvas

    Canvas::Canvas(Bitmap& target)
        : _base(target)
    {
        State state;
        state.ClipRight = target.Width();
        state.ClipBottom = target.Height();
        _states.push_back(std::move(state));
    }

    Bitmap& Canvas::Target() noexcept
    {
        for (auto it = _states.rbegin(); it != _states.rend(); ++it)
        {
            if (it->Layer != nullptr)
            {
                return *it->Layer;
            }
        }
        return _base;
    }

    void Canvas::Clear(Color color)
    {
        Target().Clear(color);
    }

    void Canvas::Save()
    {
        State copy = Current();
        copy.Layer = nullptr;
        copy.LayerOpacity = 1.0;
        _states.push_back(std::move(copy));
    }

    void Canvas::SaveLayerAlpha(double opacity)
    {
        Save();
        Current().Layer = std::make_shared<Bitmap>(_base.Width(), _base.Height());
        Current().LayerOpacity = std::clamp(opacity, 0.0, 1.0);
    }

    void Canvas::Restore()
    {
        if (_states.size() <= 1)
        {
            return;
        }
        State top = std::move(_states.back());
        _states.pop_back();
        if (top.Layer == nullptr)
        {
            return;
        }
        // Composite the layer, through the clip it was opened under.
        Bitmap& target = Target();
        const float opacity = static_cast<float>(top.LayerOpacity);
        const std::int32_t width = target.Width();
        for (std::int32_t y = top.ClipTop; y < top.ClipBottom; y++)
        {
            for (std::int32_t x = top.ClipLeft; x < top.ClipRight; x++)
            {
                const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x)) * 4;
                const std::uint8_t* s = top.Layer->Pixels() + i;
                if (s[3] == 0)
                {
                    continue;
                }
                std::uint8_t* d = target.Pixels() + i;
                const float sa = s[3] / 255.0F * opacity;
                const float inv = 1 - sa;
                for (int k = 0; k < 4; k++)
                {
                    const float v = s[k] * opacity + d[k] * inv;
                    d[k] = static_cast<std::uint8_t>(std::clamp(v + 0.5F, 0.0F, 255.0F));
                }
            }
        }
    }

    void Canvas::Concat(const Matrix& matrix)
    {
        Current().Transform = matrix.Then(Current().Transform);
    }

    void Canvas::SetMatrix(const Matrix& matrix)
    {
        Current().Transform = matrix;
    }

    const Matrix& Canvas::TotalMatrix() const noexcept
    {
        return Current().Transform;
    }

    Rect Canvas::DeviceClipBounds() const noexcept
    {
        const State& s = Current();
        return Rect{static_cast<double>(s.ClipLeft), static_cast<double>(s.ClipTop), static_cast<double>(s.ClipRight),
            static_cast<double>(s.ClipBottom)};
    }

    void Canvas::ClipRect(const Rect& rect, bool antialias)
    {
        State& s = Current();
        const Matrix m = s.Transform;
        if (m.IsScaleTranslate())
        {
            const Rect d = m.MapRect(rect);
            const double l = std::round(d.Left);
            const double t = std::round(d.Top);
            const double r = std::round(d.Right);
            const double b = std::round(d.Bottom);
            // Pixel aligned: no mask needed, which is nearly every clip.
            if (!antialias || (std::abs(l - d.Left) < 1e-3 && std::abs(t - d.Top) < 1e-3
                && std::abs(r - d.Right) < 1e-3 && std::abs(b - d.Bottom) < 1e-3))
            {
                s.ClipLeft = std::max(s.ClipLeft, static_cast<std::int32_t>(l));
                s.ClipTop = std::max(s.ClipTop, static_cast<std::int32_t>(t));
                s.ClipRight = std::min(s.ClipRight, static_cast<std::int32_t>(r));
                s.ClipBottom = std::min(s.ClipBottom, static_cast<std::int32_t>(b));
                if (s.ClipRight < s.ClipLeft)
                {
                    s.ClipRight = s.ClipLeft;
                }
                if (s.ClipBottom < s.ClipTop)
                {
                    s.ClipBottom = s.ClipTop;
                }
                return;
            }
        }
        Path path;
        path.AddRect(rect);
        ClipPath(path, antialias);
    }

    void Canvas::ClipPath(const Path& path, bool antialias)
    {
        State& s = Current();
        const Rect bounds = s.Transform.MapRect(path.Bounds());
        s.ClipLeft = std::max(s.ClipLeft, static_cast<std::int32_t>(std::floor(bounds.Left)));
        s.ClipTop = std::max(s.ClipTop, static_cast<std::int32_t>(std::floor(bounds.Top)));
        s.ClipRight = std::min(s.ClipRight, static_cast<std::int32_t>(std::ceil(bounds.Right)));
        s.ClipBottom = std::min(s.ClipBottom, static_cast<std::int32_t>(std::ceil(bounds.Bottom)));
        if (s.ClipRight <= s.ClipLeft || s.ClipBottom <= s.ClipTop)
        {
            s.ClipRight = s.ClipLeft;
            s.ClipBottom = s.ClipTop;
            s.ClipMask = nullptr;
            return;
        }
        Mask mask = Rasterize(path.Flatten(s.Transform), path.Rule, antialias);
        if (s.ClipMask != nullptr)
        {
            for (std::int32_t y = 0; y < mask.Height; y++)
            {
                for (std::int32_t x = 0; x < mask.Width; x++)
                {
                    mask.Coverage[static_cast<std::size_t>(y * mask.Width + x)] *= ClipAt(mask.X + x, mask.Y + y);
                }
            }
        }
        s.ClipMask = std::make_shared<const Mask>(std::move(mask));
    }

    float Canvas::ClipAt(std::int32_t x, std::int32_t y) const noexcept
    {
        const State& s = Current();
        if (s.ClipMask == nullptr)
        {
            return 1.0F;
        }
        const Mask& m = *s.ClipMask;
        const std::int32_t mx = x - m.X;
        const std::int32_t my = y - m.Y;
        if (mx < 0 || my < 0 || mx >= m.Width || my >= m.Height)
        {
            return 0.0F;
        }
        return m.Coverage[static_cast<std::size_t>(my * m.Width + mx)];
    }

    // The accumulation rasterizer (font-rs): each edge adds its signed area
    // to the cells it crosses, and a running sum along a row is the winding
    // coverage of every pixel in it.
    Canvas::Mask Canvas::Rasterize(const std::vector<Path::Contour>& contours, FillRule rule, bool antialias) const
    {
        const State& s = Current();
        Rect bounds{};
        bool any = false;
        for (const Path::Contour& contour : contours)
        {
            for (const Point& p : contour.Points)
            {
                if (!any)
                {
                    bounds = Rect{p.X, p.Y, p.X, p.Y};
                    any = true;
                }
                bounds.Left = std::min(bounds.Left, p.X);
                bounds.Top = std::min(bounds.Top, p.Y);
                bounds.Right = std::max(bounds.Right, p.X);
                bounds.Bottom = std::max(bounds.Bottom, p.Y);
            }
        }
        Mask mask;
        if (!any)
        {
            return mask;
        }
        const std::int32_t left = std::max(s.ClipLeft, static_cast<std::int32_t>(std::floor(bounds.Left)));
        const std::int32_t top = std::max(s.ClipTop, static_cast<std::int32_t>(std::floor(bounds.Top)));
        const std::int32_t right = std::min(s.ClipRight, static_cast<std::int32_t>(std::ceil(bounds.Right)) + 1);
        const std::int32_t bottom = std::min(s.ClipBottom, static_cast<std::int32_t>(std::ceil(bounds.Bottom)) + 1);
        if (right <= left || bottom <= top)
        {
            return mask;
        }
        mask.X = left;
        mask.Y = top;
        mask.Width = right - left;
        mask.Height = bottom - top;
        const std::int32_t stride = mask.Width + 2;
        std::vector<float> acc(static_cast<std::size_t>(stride) * static_cast<std::size_t>(mask.Height), 0.0F);
        const double w = mask.Width;
        const double h = mask.Height;
        const auto line = [&](Point p0, Point p1)
        {
            p0.X = std::clamp(p0.X - left, 0.0, w);
            p1.X = std::clamp(p1.X - left, 0.0, w);
            p0.Y -= top;
            p1.Y -= top;
            if (std::abs(p0.Y - p1.Y) <= 1e-9)
            {
                return;
            }
            double dir = 1.0;
            if (p0.Y > p1.Y)
            {
                std::swap(p0, p1);
                dir = -1.0;
            }
            if (p1.Y <= 0 || p0.Y >= h)
            {
                return;
            }
            const double dxdy = (p1.X - p0.X) / (p1.Y - p0.Y);
            double x = p0.X;
            if (p0.Y < 0)
            {
                x -= p0.Y * dxdy;
            }
            const std::int32_t yStart = std::max(0, static_cast<std::int32_t>(std::floor(p0.Y)));
            const std::int32_t yEnd = std::min(mask.Height, static_cast<std::int32_t>(std::ceil(p1.Y)));
            for (std::int32_t y = yStart; y < yEnd; y++)
            {
                float* row = &acc[static_cast<std::size_t>(y) * static_cast<std::size_t>(stride)];
                const double dy = std::min(static_cast<double>(y + 1), p1.Y) - std::max(static_cast<double>(y), p0.Y);
                const double xnext = std::clamp(x + dxdy * dy, 0.0, w);
                const double d = dy * dir;
                const double x0 = std::min(x, xnext);
                const double x1 = std::max(x, xnext);
                const double x0floor = std::floor(x0);
                const auto x0i = static_cast<std::int32_t>(x0floor);
                const double x1ceil = std::ceil(x1);
                const auto x1i = static_cast<std::int32_t>(x1ceil);
                if (x1i <= x0i + 1)
                {
                    const double xmf = 0.5 * (x + xnext) - x0floor;
                    row[x0i] += static_cast<float>(d - d * xmf);
                    row[x0i + 1] += static_cast<float>(d * xmf);
                }
                else
                {
                    const double inv = 1.0 / (x1 - x0);
                    const double x0f = x0 - x0floor;
                    const double a0 = 0.5 * inv * (1.0 - x0f) * (1.0 - x0f);
                    const double x1f = x1 - x1ceil + 1.0;
                    const double am = 0.5 * inv * x1f * x1f;
                    row[x0i] += static_cast<float>(d * a0);
                    if (x1i == x0i + 2)
                    {
                        row[x0i + 1] += static_cast<float>(d * (1.0 - a0 - am));
                    }
                    else
                    {
                        const double a1 = inv * (1.5 - x0f);
                        row[x0i + 1] += static_cast<float>(d * (a1 - a0));
                        for (std::int32_t xi = x0i + 2; xi < x1i - 1; xi++)
                        {
                            row[xi] += static_cast<float>(d * inv);
                        }
                        const double a2 = a1 + (x1i - x0i - 3) * inv;
                        row[x1i - 1] += static_cast<float>(d * (1.0 - a2 - am));
                    }
                    row[x1i] += static_cast<float>(d * am);
                }
                x = xnext;
            }
        };
        for (const Path::Contour& contour : contours)
        {
            const std::size_t n = contour.Points.size();
            if (n < 2)
            {
                continue;
            }
            for (std::size_t i = 0; i + 1 < n; i++)
            {
                line(contour.Points[i], contour.Points[i + 1]);
            }
            // A fill closes every contour, closed or not.
            line(contour.Points[n - 1], contour.Points[0]);
        }
        mask.Coverage.assign(static_cast<std::size_t>(mask.Width) * static_cast<std::size_t>(mask.Height), 0.0F);
        for (std::int32_t y = 0; y < mask.Height; y++)
        {
            float sum = 0;
            const float* row = &acc[static_cast<std::size_t>(y) * static_cast<std::size_t>(stride)];
            float* out = &mask.Coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(mask.Width)];
            for (std::int32_t x = 0; x < mask.Width; x++)
            {
                sum += row[x];
                float coverage;
                if (rule == FillRule::EvenOdd)
                {
                    const float v = std::abs(sum);
                    const float f = v - 2.0F * std::floor(v / 2.0F);
                    coverage = f > 1.0F ? 2.0F - f : f;
                }
                else
                {
                    coverage = std::min(1.0F, std::abs(sum));
                }
                if (!antialias)
                {
                    coverage = coverage >= 0.5F ? 1.0F : 0.0F;
                }
                out[x] = coverage;
            }
        }
        return mask;
    }

    void Canvas::BlendSpan(std::int32_t y, std::int32_t x0, std::int32_t x1, const float* coverage, const Paint& paint,
        float solid[4])
    {
        Bitmap& target = Target();
        const std::int32_t width = target.Width();
        std::uint8_t* row = target.Pixels() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4;
        const float opacity = static_cast<float>(paint.Opacity);
        float c[4];
        for (std::int32_t x = x0; x < x1; x++)
        {
            float cov = coverage[x - x0];
            if (cov <= 0.0F)
            {
                continue;
            }
            cov *= ClipAt(x, y) * opacity;
            if (cov <= 0.0F)
            {
                continue;
            }
            if (paint.Gradient != nullptr)
            {
                paint.Gradient->Shade(x + 0.5, y + 0.5, c);
            }
            else
            {
                std::memcpy(c, solid, sizeof(c));
            }
            const float sa = c[3] * cov;
            if (sa <= 0.0F)
            {
                continue;
            }
            std::uint8_t* d = row + static_cast<std::size_t>(x) * 4;
            const float inv = 1.0F - sa;
            for (int k = 0; k < 3; k++)
            {
                d[k] = static_cast<std::uint8_t>(std::clamp(c[k] * cov * 255.0F + d[k] * inv + 0.5F, 0.0F, 255.0F));
            }
            d[3] = static_cast<std::uint8_t>(std::clamp(sa * 255.0F + d[3] * inv + 0.5F, 0.0F, 255.0F));
        }
    }

    void Canvas::Blend(const Mask& coverage, const Paint& paint)
    {
        float solid[4];
        StraightToPremul(paint.Solid, 1.0, solid);
        for (std::int32_t y = 0; y < coverage.Height; y++)
        {
            BlendSpan(coverage.Y + y, coverage.X, coverage.X + coverage.Width,
                &coverage.Coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(coverage.Width)], paint,
                solid);
        }
    }

    void Canvas::FillPath(const Path& path, const Paint& paint)
    {
        if (path.IsEmpty() || paint.Opacity <= 0)
        {
            return;
        }
        const Mask mask = Rasterize(path.Flatten(Current().Transform), path.Rule, true);
        Blend(mask, paint);
    }

    void Canvas::StrokePath(const Path& path, const StrokeStyle& stroke, const Paint& paint)
    {
        if (path.IsEmpty() || paint.Opacity <= 0 || stroke.Width <= 0)
        {
            return;
        }
        const Matrix m = Current().Transform;
        const double scale = std::sqrt(std::abs(m.ScaleX * m.ScaleY - m.SkewX * m.SkewY));
        const std::vector<Path::Contour> outline = StrokeContours(path.Flatten(m), stroke, scale);
        const Mask mask = Rasterize(outline, FillRule::NonZero, true);
        Blend(mask, paint);
    }

    void Canvas::DrawBitmap(const Bitmap& bitmap, const Rect& source, const Rect& destination, FilterQuality quality,
        double opacity)
    {
        if (bitmap.Width() <= 0 || bitmap.Height() <= 0 || destination.IsEmpty() || source.IsEmpty() || opacity <= 0)
        {
            return;
        }
        const Matrix m = Current().Transform;
        // Destination to source, then device to destination.
        const Matrix local = Matrix::Scale(source.Width() / destination.Width(), source.Height() / destination.Height())
            .Then(Matrix::Translation(0, 0));
        const std::optional<Matrix> inverse = m.Invert();
        if (!inverse.has_value())
        {
            return;
        }
        Path shape;
        shape.AddRect(destination);
        const Mask mask = Rasterize(shape.Flatten(m), FillRule::NonZero, true);
        Bitmap& target = Target();
        const std::int32_t width = target.Width();
        const float alpha = static_cast<float>(opacity);
        const double sx = source.Width() / destination.Width();
        const double sy = source.Height() / destination.Height();
        (void)local;
        const auto sample = [&](double u, double v, float out[4])
        {
            const std::int32_t bw = bitmap.Width();
            const std::int32_t bh = bitmap.Height();
            const std::uint8_t* px = bitmap.Pixels();
            if (quality == FilterQuality::None)
            {
                const std::int32_t ix = std::clamp(static_cast<std::int32_t>(std::floor(u)), 0, bw - 1);
                const std::int32_t iy = std::clamp(static_cast<std::int32_t>(std::floor(v)), 0, bh - 1);
                const std::uint8_t* p = px + (static_cast<std::size_t>(iy) * static_cast<std::size_t>(bw)
                    + static_cast<std::size_t>(ix)) * 4;
                for (int k = 0; k < 4; k++)
                {
                    out[k] = p[k] / 255.0F;
                }
                return;
            }
            const double fu = u - 0.5;
            const double fv = v - 0.5;
            const std::int32_t x0 = static_cast<std::int32_t>(std::floor(fu));
            const std::int32_t y0 = static_cast<std::int32_t>(std::floor(fv));
            const float tx = static_cast<float>(fu - x0);
            const float ty = static_cast<float>(fv - y0);
            const auto at = [&](std::int32_t x, std::int32_t y)
            {
                x = std::clamp(x, 0, bw - 1);
                y = std::clamp(y, 0, bh - 1);
                return px + (static_cast<std::size_t>(y) * static_cast<std::size_t>(bw) + static_cast<std::size_t>(x)) * 4;
            };
            const std::uint8_t* a = at(x0, y0);
            const std::uint8_t* b = at(x0 + 1, y0);
            const std::uint8_t* c = at(x0, y0 + 1);
            const std::uint8_t* d = at(x0 + 1, y0 + 1);
            for (int k = 0; k < 4; k++)
            {
                const float top = a[k] + (b[k] - a[k]) * tx;
                const float bottom = c[k] + (d[k] - c[k]) * tx;
                out[k] = (top + (bottom - top) * ty) / 255.0F;
            }
        };
        // Minification by more than two uses a box average, which is what
        // Skia's mipmapped High quality amounts to for a downscale.
        const double stepU = std::abs(sx * inverse->ScaleX) + std::abs(sx * inverse->SkewX);
        const double stepV = std::abs(sy * inverse->SkewY) + std::abs(sy * inverse->ScaleY);
        const int boxU = quality == FilterQuality::None ? 1 : std::clamp(static_cast<int>(std::floor(stepU)), 1, 16);
        const int boxV = quality == FilterQuality::None ? 1 : std::clamp(static_cast<int>(std::floor(stepV)), 1, 16);
        for (std::int32_t y = 0; y < mask.Height; y++)
        {
            const std::int32_t dy = mask.Y + y;
            std::uint8_t* row = target.Pixels() + static_cast<std::size_t>(dy) * static_cast<std::size_t>(width) * 4;
            for (std::int32_t x = 0; x < mask.Width; x++)
            {
                float cov = mask.Coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(mask.Width)
                    + static_cast<std::size_t>(x)];
                const std::int32_t dx = mask.X + x;
                if (cov <= 0.0F)
                {
                    continue;
                }
                cov *= ClipAt(dx, dy) * alpha;
                if (cov <= 0.0F)
                {
                    continue;
                }
                const Point p = inverse->Map({dx + 0.5, dy + 0.5});
                const double u = source.Left + (p.X - destination.Left) * sx;
                const double v = source.Top + (p.Y - destination.Top) * sy;
                float c[4]{0, 0, 0, 0};
                if (boxU == 1 && boxV == 1)
                {
                    sample(u, v, c);
                }
                else
                {
                    float t[4];
                    for (int j = 0; j < boxV; j++)
                    {
                        for (int i = 0; i < boxU; i++)
                        {
                            sample(u + (i + 0.5) * stepU / boxU - stepU / 2, v + (j + 0.5) * stepV / boxV - stepV / 2, t);
                            for (int k = 0; k < 4; k++)
                            {
                                c[k] += t[k];
                            }
                        }
                    }
                    for (float& value : c)
                    {
                        value /= static_cast<float>(boxU * boxV);
                    }
                }
                const float sa = c[3] * cov;
                if (sa <= 0.0F)
                {
                    continue;
                }
                std::uint8_t* d = row + static_cast<std::size_t>(dx) * 4;
                const float inv = 1.0F - sa;
                for (int k = 0; k < 4; k++)
                {
                    d[k] = static_cast<std::uint8_t>(std::clamp(c[k] * cov * 255.0F + d[k] * inv + 0.5F, 0.0F, 255.0F));
                }
            }
        }
    }

    void Canvas::DrawBoxShadow(const Path& shape, const BoxShadowSpec& shadow, const Rect& bounds,
        const std::array<Point, 4>& radii)
    {
        if (shadow.ShadowColor.A == 0)
        {
            return;
        }
        const Matrix m = Current().Transform;
        const double scale = std::sqrt(std::abs(m.ScaleX * m.ScaleY - m.SkewX * m.SkewY));
        const double sigma = ConvertRadiusToSigma(shadow.Blur) * scale;
        const int reach = static_cast<int>(std::ceil(sigma * 3));
        // The shadow's own shape: the box grown by the spread and moved by
        // the offset (or, inset, the hole it casts inside the box).
        Path spread;
        std::array<Point, 4> grown = radii;
        for (Point& r : grown)
        {
            if (r.X > 0 || r.Y > 0)
            {
                r.X = std::max(0.0, r.X + (shadow.Inset ? -shadow.Spread : shadow.Spread));
                r.Y = std::max(0.0, r.Y + (shadow.Inset ? -shadow.Spread : shadow.Spread));
            }
        }
        const double s = shadow.Inset ? -shadow.Spread : shadow.Spread;
        spread.AddRoundRect(Rect{bounds.Left - s + shadow.OffsetX, bounds.Top - s + shadow.OffsetY,
            bounds.Right + s + shadow.OffsetX, bounds.Bottom + s + shadow.OffsetY}, grown);
        // Rasterise into a mask with room for the blur, independent of the clip.
        const Rect device = m.MapRect(spread.Bounds());
        const std::int32_t left = static_cast<std::int32_t>(std::floor(device.Left)) - reach - 1;
        const std::int32_t top = static_cast<std::int32_t>(std::floor(device.Top)) - reach - 1;
        const std::int32_t right = static_cast<std::int32_t>(std::ceil(device.Right)) + reach + 1;
        const std::int32_t bottom = static_cast<std::int32_t>(std::ceil(device.Bottom)) + reach + 1;
        Save();
        Current().ClipLeft = left;
        Current().ClipTop = top;
        Current().ClipRight = right;
        Current().ClipBottom = bottom;
        Current().ClipMask = nullptr;
        Mask shapeMask = Rasterize(spread.Flatten(m), FillRule::NonZero, true);
        Restore();
        if (shapeMask.Width <= 0)
        {
            return;
        }
        // Room for the blur on every side: the shape's own mask sits inside.
        Mask mask;
        mask.X = left;
        mask.Y = top;
        mask.Width = right - left;
        mask.Height = bottom - top;
        mask.Coverage.assign(static_cast<std::size_t>(mask.Width) * static_cast<std::size_t>(mask.Height), 0.0F);
        for (std::int32_t y = 0; y < shapeMask.Height; y++)
        {
            for (std::int32_t x = 0; x < shapeMask.Width; x++)
            {
                const std::int32_t mx = shapeMask.X + x - mask.X;
                const std::int32_t my = shapeMask.Y + y - mask.Y;
                if (mx >= 0 && my >= 0 && mx < mask.Width && my < mask.Height)
                {
                    mask.Coverage[static_cast<std::size_t>(my * mask.Width + mx)]
                        = shapeMask.Coverage[static_cast<std::size_t>(y * shapeMask.Width + x)];
                }
            }
        }
        if (shadow.Inset)
        {
            for (float& value : mask.Coverage)
            {
                value = 1.0F - value;
            }
        }
        if (sigma > 0.01)
        {
            // Three box blurs each way approximate the Gaussian, as Skia's
            // blur mask filter does for large sigmas.
            const int box = std::max(1, static_cast<int>(std::floor(sigma * 3 * std::sqrt(2 * std::numbers::pi) / 4 + 0.5)));
            std::vector<float> tmp(mask.Coverage.size());
            const auto pass = [&](bool horizontal)
            {
                const std::int32_t lines = horizontal ? mask.Height : mask.Width;
                const std::int32_t length = horizontal ? mask.Width : mask.Height;
                for (std::int32_t l = 0; l < lines; l++)
                {
                    const auto at = [&](std::int32_t i) -> float&
                    {
                        return horizontal ? mask.Coverage[static_cast<std::size_t>(l * mask.Width + i)]
                                          : mask.Coverage[static_cast<std::size_t>(i * mask.Width + l)];
                    };
                    const float edge = shadow.Inset ? 1.0F : 0.0F;
                    const auto value = [&](std::int32_t i) { return i < 0 || i >= length ? edge : at(i); };
                    const int r = box / 2;
                    float sum = 0;
                    for (int i = -r; i <= r; i++)
                    {
                        sum += value(i);
                    }
                    for (std::int32_t i = 0; i < length; i++)
                    {
                        const std::size_t out = horizontal ? static_cast<std::size_t>(l * mask.Width + i)
                                                           : static_cast<std::size_t>(i * mask.Width + l);
                        tmp[out] = sum / static_cast<float>(2 * r + 1);
                        sum += value(i + r + 1) - value(i - r);
                    }
                }
                mask.Coverage.swap(tmp);
            };
            for (int i = 0; i < 3; i++)
            {
                pass(true);
                pass(false);
            }
        }
        // Clipped to outside the box (outset) or inside it (inset).
        Save();
        if (shadow.Inset)
        {
            ClipPath(shape, true);
        }
        Paint paint;
        paint.Solid = shadow.ShadowColor;
        Mask outside;
        if (!shadow.Inset)
        {
            outside = Rasterize(shape.Flatten(m), FillRule::NonZero, true);
        }
        float solid[4];
        StraightToPremul(paint.Solid, 1.0, solid);
        std::vector<float> row(static_cast<std::size_t>(mask.Width));
        for (std::int32_t y = 0; y < mask.Height; y++)
        {
            const std::int32_t dy = mask.Y + y;
            if (dy < Current().ClipTop || dy >= Current().ClipBottom)
            {
                continue;
            }
            std::int32_t x0 = std::max(mask.X, Current().ClipLeft);
            const std::int32_t x1 = std::min(mask.X + mask.Width, Current().ClipRight);
            if (x1 <= x0)
            {
                continue;
            }
            for (std::int32_t x = x0; x < x1; x++)
            {
                float c = mask.Coverage[static_cast<std::size_t>(y * mask.Width + (x - mask.X))];
                if (!shadow.Inset && outside.Width > 0)
                {
                    const std::int32_t ox = x - outside.X;
                    const std::int32_t oy = dy - outside.Y;
                    if (ox >= 0 && oy >= 0 && ox < outside.Width && oy < outside.Height)
                    {
                        c *= 1.0F - outside.Coverage[static_cast<std::size_t>(oy * outside.Width + ox)];
                    }
                }
                row[static_cast<std::size_t>(x - x0)] = c;
            }
            BlendSpan(dy, x0, x1, row.data(), paint, solid);
        }
        Restore();
    }

    void Canvas::DrawText(std::u32string_view text, const Typeface& typeface, double size, Point origin,
        const Paint& paint)
    {
        if (text.empty() || size <= 0 || paint.Opacity <= 0)
        {
            return;
        }
        const Matrix m = Current().Transform;
        // Text is rasterised at its device size: a scale is folded into the
        // glyphs, a translation into where they land.
        const double scale = std::sqrt(std::abs(m.ScaleX * m.ScaleY - m.SkewX * m.SkewY));
        const double deviceSize = size * scale;
        const Point start = m.Map(origin);
        float solid[4];
        StraightToPremul(paint.Solid, 1.0, solid);
        double pen = 0;
        char32_t previous = 0;
        std::vector<float> row;
        for (const char32_t code : text)
        {
            if (previous != 0)
            {
                pen += typeface.Kerning(previous, code, deviceSize);
            }
            const Typeface::GlyphImage* glyph = typeface.Rasterize(code, deviceSize);
            if (glyph != nullptr && glyph->Width > 0)
            {
                const std::int32_t gx = static_cast<std::int32_t>(std::lround(start.X + pen)) + glyph->Left;
                const std::int32_t gy = static_cast<std::int32_t>(std::lround(start.Y)) - glyph->Top;
                row.resize(static_cast<std::size_t>(glyph->Width));
                for (std::int32_t y = 0; y < glyph->Height; y++)
                {
                    const std::int32_t dy = gy + y;
                    if (dy < Current().ClipTop || dy >= Current().ClipBottom)
                    {
                        continue;
                    }
                    const std::int32_t x0 = std::max(gx, Current().ClipLeft);
                    const std::int32_t x1 = std::min(gx + glyph->Width, Current().ClipRight);
                    if (x1 <= x0)
                    {
                        continue;
                    }
                    for (std::int32_t x = x0; x < x1; x++)
                    {
                        row[static_cast<std::size_t>(x - x0)]
                            = glyph->Coverage[static_cast<std::size_t>(y * glyph->Width + (x - gx))] / 255.0F;
                    }
                    BlendSpan(dy, x0, x1, row.data(), paint, solid);
                }
            }
            pen += typeface.Advance(code, deviceSize);
            previous = code;
        }
    }
}
