#pragma once

// Avalonia's base types -- Point, Size, Rect, Vector, Thickness, CornerRadius,
// Matrix, RelativePoint -- and its property system: AvaloniaProperty,
// StyledProperty, attached properties and AvaloniaObject.
//
// This is the part of Avalonia.Base the launcher's screens are written
// against, reproduced so the screens port one to one. Values are held as
// std::any because a C# AvaloniaObject holds them as object.

#include <algorithm>
#include <any>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    struct Vector final
    {
        double X = 0.0;
        double Y = 0.0;

        [[nodiscard]] double Length() const noexcept { return std::sqrt(X * X + Y * Y); }
        friend constexpr bool operator==(const Vector&, const Vector&) noexcept = default;
        friend constexpr Vector operator+(Vector a, Vector b) noexcept { return {a.X + b.X, a.Y + b.Y}; }
        friend constexpr Vector operator-(Vector a, Vector b) noexcept { return {a.X - b.X, a.Y - b.Y}; }
        friend constexpr Vector operator*(Vector a, double s) noexcept { return {a.X * s, a.Y * s}; }
        friend constexpr Vector operator-(Vector a) noexcept { return {-a.X, -a.Y}; }
    };

    struct Point final
    {
        double X = 0.0;
        double Y = 0.0;

        friend constexpr bool operator==(const Point&, const Point&) noexcept = default;
        friend constexpr Vector operator-(Point a, Point b) noexcept { return {a.X - b.X, a.Y - b.Y}; }
        friend constexpr Point operator+(Point a, Vector b) noexcept { return {a.X + b.X, a.Y + b.Y}; }
        friend constexpr Point operator-(Point a, Vector b) noexcept { return {a.X - b.X, a.Y - b.Y}; }
        friend constexpr Point operator*(Point a, double s) noexcept { return {a.X * s, a.Y * s}; }
        [[nodiscard]] explicit constexpr operator Vector() const noexcept { return {X, Y}; }
    };

    struct Size final
    {
        double Width = 0.0;
        double Height = 0.0;

        [[nodiscard]] static constexpr Size Infinity() noexcept
        {
            return {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
        }
        friend constexpr bool operator==(const Size&, const Size&) noexcept = default;
        friend constexpr Size operator*(Size a, double s) noexcept { return {a.Width * s, a.Height * s}; }
        friend constexpr Size operator/(Size a, double s) noexcept { return {a.Width / s, a.Height / s}; }
        [[nodiscard]] Size Deflate(const struct Thickness& t) const noexcept;
        [[nodiscard]] Size Inflate(const struct Thickness& t) const noexcept;
    };

    struct Thickness final
    {
        double Left = 0.0;
        double Top = 0.0;
        double Right = 0.0;
        double Bottom = 0.0;

        constexpr Thickness() noexcept = default;
        constexpr explicit Thickness(double uniform) noexcept
            : Left(uniform), Top(uniform), Right(uniform), Bottom(uniform)
        {
        }
        constexpr Thickness(double horizontal, double vertical) noexcept
            : Left(horizontal), Top(vertical), Right(horizontal), Bottom(vertical)
        {
        }
        constexpr Thickness(double left, double top, double right, double bottom) noexcept
            : Left(left), Top(top), Right(right), Bottom(bottom)
        {
        }
        [[nodiscard]] constexpr bool IsUniform() const noexcept
        {
            return Left == Top && Left == Right && Left == Bottom;
        }
        friend constexpr bool operator==(const Thickness&, const Thickness&) noexcept = default;
    };

    inline Size Size::Deflate(const Thickness& t) const noexcept
    {
        return {std::max(0.0, Width - t.Left - t.Right), std::max(0.0, Height - t.Top - t.Bottom)};
    }

    inline Size Size::Inflate(const Thickness& t) const noexcept
    {
        return {Width + t.Left + t.Right, Height + t.Top + t.Bottom};
    }

    struct Rect final
    {
        double X = 0.0;
        double Y = 0.0;
        double Width = 0.0;
        double Height = 0.0;

        constexpr Rect() noexcept = default;
        constexpr Rect(double x, double y, double width, double height) noexcept
            : X(x), Y(y), Width(width), Height(height)
        {
        }
        constexpr Rect(Point position, Size size) noexcept
            : X(position.X), Y(position.Y), Width(size.Width), Height(size.Height)
        {
        }
        constexpr explicit Rect(Size size) noexcept
            : Width(size.Width), Height(size.Height)
        {
        }
        constexpr Rect(Point a, Point b) noexcept
            : X(std::min(a.X, b.X)), Y(std::min(a.Y, b.Y)), Width(std::abs(b.X - a.X)), Height(std::abs(b.Y - a.Y))
        {
        }

        [[nodiscard]] constexpr double Left() const noexcept { return X; }
        [[nodiscard]] constexpr double Top() const noexcept { return Y; }
        [[nodiscard]] constexpr double Right() const noexcept { return X + Width; }
        [[nodiscard]] constexpr double Bottom() const noexcept { return Y + Height; }
        [[nodiscard]] constexpr Point Position() const noexcept { return {X, Y}; }
        [[nodiscard]] constexpr Point TopLeft() const noexcept { return {X, Y}; }
        [[nodiscard]] constexpr Point TopRight() const noexcept { return {X + Width, Y}; }
        [[nodiscard]] constexpr Point BottomLeft() const noexcept { return {X, Y + Height}; }
        [[nodiscard]] constexpr Point BottomRight() const noexcept { return {X + Width, Y + Height}; }
        [[nodiscard]] constexpr Point Center() const noexcept { return {X + Width / 2, Y + Height / 2}; }
        [[nodiscard]] constexpr Size GetSize() const noexcept { return {Width, Height}; }
        [[nodiscard]] constexpr bool Contains(Point p) const noexcept
        {
            return p.X >= X && p.X <= X + Width && p.Y >= Y && p.Y <= Y + Height;
        }
        [[nodiscard]] Rect Intersect(const Rect& other) const noexcept;
        [[nodiscard]] Rect Union(const Rect& other) const noexcept;
        [[nodiscard]] constexpr bool Intersects(const Rect& o) const noexcept
        {
            return o.X < X + Width && X < o.X + o.Width && o.Y < Y + Height && Y < o.Y + o.Height;
        }
        [[nodiscard]] Rect Deflate(const Thickness& t) const noexcept;
        [[nodiscard]] Rect Deflate(double amount) const noexcept { return Deflate(Thickness(amount)); }
        [[nodiscard]] Rect Inflate(const Thickness& t) const noexcept;
        [[nodiscard]] Rect Inflate(double amount) const noexcept { return Inflate(Thickness(amount)); }
        [[nodiscard]] constexpr Rect Translate(Vector v) const noexcept { return {X + v.X, Y + v.Y, Width, Height}; }
        [[nodiscard]] constexpr bool IsEmpty() const noexcept { return Width <= 0 || Height <= 0; }
        friend constexpr bool operator==(const Rect&, const Rect&) noexcept = default;
    };

    struct CornerRadius final
    {
        double TopLeft = 0.0;
        double TopRight = 0.0;
        double BottomRight = 0.0;
        double BottomLeft = 0.0;

        constexpr CornerRadius() noexcept = default;
        constexpr explicit CornerRadius(double uniform) noexcept
            : TopLeft(uniform), TopRight(uniform), BottomRight(uniform), BottomLeft(uniform)
        {
        }
        constexpr CornerRadius(double top, double bottom) noexcept
            : TopLeft(top), TopRight(top), BottomRight(bottom), BottomLeft(bottom)
        {
        }
        constexpr CornerRadius(double topLeft, double topRight, double bottomRight, double bottomLeft) noexcept
            : TopLeft(topLeft), TopRight(topRight), BottomRight(bottomRight), BottomLeft(bottomLeft)
        {
        }
        [[nodiscard]] constexpr bool IsUniform() const noexcept
        {
            return TopLeft == TopRight && TopLeft == BottomRight && TopLeft == BottomLeft;
        }
        [[nodiscard]] constexpr bool IsZero() const noexcept
        {
            return TopLeft == 0 && TopRight == 0 && BottomRight == 0 && BottomLeft == 0;
        }
        friend constexpr bool operator==(const CornerRadius&, const CornerRadius&) noexcept = default;
    };

    // Avalonia.Matrix: M11 M12 / M21 M22 / M31 M32, row vectors.
    struct Matrix final
    {
        double M11 = 1.0;
        double M12 = 0.0;
        double M21 = 0.0;
        double M22 = 1.0;
        double M31 = 0.0;
        double M32 = 0.0;

        [[nodiscard]] static constexpr Matrix Identity() noexcept { return {}; }
        [[nodiscard]] static constexpr Matrix CreateTranslation(double x, double y) noexcept
        {
            return {1, 0, 0, 1, x, y};
        }
        [[nodiscard]] static constexpr Matrix CreateTranslation(Vector v) noexcept { return CreateTranslation(v.X, v.Y); }
        [[nodiscard]] static constexpr Matrix CreateScale(double x, double y) noexcept { return {x, 0, 0, y, 0, 0}; }
        [[nodiscard]] static Matrix CreateRotation(double radians) noexcept;
        [[nodiscard]] constexpr bool IsIdentity() const noexcept
        {
            return M11 == 1 && M12 == 0 && M21 == 0 && M22 == 1 && M31 == 0 && M32 == 0;
        }
        [[nodiscard]] std::optional<Matrix> TryInvert() const noexcept;
        [[nodiscard]] Matrix Invert() const;
        [[nodiscard]] constexpr Point Transform(Point p) const noexcept
        {
            return {p.X * M11 + p.Y * M21 + M31, p.X * M12 + p.Y * M22 + M32};
        }
        // a * b applies a first, then b, as Avalonia's operator* does.
        friend constexpr Matrix operator*(const Matrix& a, const Matrix& b) noexcept
        {
            return {a.M11 * b.M11 + a.M12 * b.M21, a.M11 * b.M12 + a.M12 * b.M22, a.M21 * b.M11 + a.M22 * b.M21,
                a.M21 * b.M12 + a.M22 * b.M22, a.M31 * b.M11 + a.M32 * b.M21 + b.M31,
                a.M31 * b.M12 + a.M32 * b.M22 + b.M32};
        }
        friend constexpr bool operator==(const Matrix&, const Matrix&) noexcept = default;
    };

    [[nodiscard]] Rect TransformToAABB(const Rect& rect, const Matrix& matrix) noexcept;

    enum class RelativeUnit : std::int32_t { Relative, Absolute };

    struct RelativePoint final
    {
        Avalonia::Point Point{};
        RelativeUnit Unit = RelativeUnit::Relative;

        constexpr RelativePoint() noexcept = default;
        constexpr RelativePoint(double x, double y, RelativeUnit unit) noexcept
            : Point{x, y}, Unit(unit)
        {
        }
        [[nodiscard]] static constexpr RelativePoint TopLeft() noexcept { return {0, 0, RelativeUnit::Relative}; }
        [[nodiscard]] static constexpr RelativePoint Center() noexcept { return {0.5, 0.5, RelativeUnit::Relative}; }
        [[nodiscard]] static constexpr RelativePoint BottomRight() noexcept
        {
            return {1, 1, RelativeUnit::Relative};
        }
        [[nodiscard]] constexpr Avalonia::Point ToPixels(Size size) const noexcept
        {
            return Unit == RelativeUnit::Absolute ? Point : Avalonia::Point{Point.X * size.Width, Point.Y * size.Height};
        }
        [[nodiscard]] constexpr Avalonia::Point ToPixels(const Rect& rect) const noexcept
        {
            return Unit == RelativeUnit::Absolute ? Point
                                                  : Avalonia::Point{rect.X + Point.X * rect.Width,
                                                        rect.Y + Point.Y * rect.Height};
        }
        friend constexpr bool operator==(const RelativePoint&, const RelativePoint&) noexcept = default;
    };

    struct RelativeScalar final
    {
        double Scalar = 0.0;
        RelativeUnit Unit = RelativeUnit::Relative;

        constexpr RelativeScalar() noexcept = default;
        constexpr RelativeScalar(double scalar, RelativeUnit unit) noexcept
            : Scalar(scalar), Unit(unit)
        {
        }
        [[nodiscard]] constexpr double ToValue(double size) const noexcept
        {
            return Unit == RelativeUnit::Absolute ? Scalar : Scalar * size;
        }
        friend constexpr bool operator==(const RelativeScalar&, const RelativeScalar&) noexcept = default;
    };

    struct PixelSize final
    {
        std::int32_t Width = 0;
        std::int32_t Height = 0;

        [[nodiscard]] static PixelSize FromSize(Size size, double scale) noexcept
        {
            return {static_cast<std::int32_t>(std::ceil(size.Width * scale)),
                static_cast<std::int32_t>(std::ceil(size.Height * scale))};
        }
        friend constexpr bool operator==(const PixelSize&, const PixelSize&) noexcept = default;
    };

    struct PixelPoint final
    {
        std::int32_t X = 0;
        std::int32_t Y = 0;
        friend constexpr bool operator==(const PixelPoint&, const PixelPoint&) noexcept = default;
    };

    // ------------------------------------------------------ property system

    class AvaloniaObject;

    class AvaloniaProperty
    {
    public:
        AvaloniaProperty(std::string name, const std::type_info& ownerType, std::any defaultValue, bool inherits);
        virtual ~AvaloniaProperty() = default;

        AvaloniaProperty(const AvaloniaProperty&) = delete;
        AvaloniaProperty& operator=(const AvaloniaProperty&) = delete;

        [[nodiscard]] const std::string& Name() const noexcept { return _name; }
        [[nodiscard]] const std::type_info& OwnerType() const noexcept { return *_owner; }
        [[nodiscard]] const std::any& DefaultValue() const noexcept { return _default; }
        [[nodiscard]] bool Inherits() const noexcept { return _inherits; }

        // Whether setting two values should raise a change: std::any cannot
        // compare, so each typed property supplies it.
        [[nodiscard]] virtual bool ValuesEqual(const std::any& a, const std::any& b) const = 0;

        enum class Affects : std::uint8_t { Render = 1, Measure = 2, Arrange = 4 };
        // Visual.AffectsRender<T> / Layoutable.AffectsMeasure<T>: the effect
        // applies to objects of a given class and its subclasses.
        void AddAffects(std::function<bool(const AvaloniaObject&)> isOwner, Affects affects);
        [[nodiscard]] std::uint8_t AffectsFor(const AvaloniaObject& target) const;

    private:
        std::string _name;
        const std::type_info* _owner;
        std::any _default;
        bool _inherits;
        std::vector<std::pair<std::function<bool(const AvaloniaObject&)>, Affects>> _affects;
    };

    template <typename T>
    class StyledProperty : public AvaloniaProperty
    {
    public:
        StyledProperty(std::string name, const std::type_info& ownerType, T defaultValue, bool inherits = false)
            : AvaloniaProperty(std::move(name), ownerType, std::any(std::move(defaultValue)), inherits)
        {
        }

        [[nodiscard]] bool ValuesEqual(const std::any& a, const std::any& b) const override
        {
            if (!a.has_value() || !b.has_value())
            {
                return a.has_value() == b.has_value();
            }
            if constexpr (requires(const T& x, const T& y) { { x == y } -> std::convertible_to<bool>; })
            {
                return std::any_cast<const T&>(a) == std::any_cast<const T&>(b);
            }
            else
            {
                return false;
            }
        }
    };

    // Attached properties are styled properties an unrelated class declares.
    template <typename T>
    using AttachedProperty = StyledProperty<T>;

    // Direct properties hold their value in a field; for the launcher's
    // purposes a styled property with the same notification behaves the same.
    template <typename T>
    using DirectProperty = StyledProperty<T>;

    // AvaloniaProperty.Register<TOwner, TValue>(name, default).
    template <typename TOwner, typename T>
    [[nodiscard]] StyledProperty<T>& Register(std::string name, T defaultValue = T{}, bool inherits = false)
    {
        // Properties live for the process, as their static fields do in C#.
        auto* property = new StyledProperty<T>(std::move(name), typeid(TOwner), std::move(defaultValue), inherits);
        return *property;
    }

    template <typename TOwner, typename T>
    [[nodiscard]] AttachedProperty<T>& RegisterAttached(std::string name, T defaultValue = T{}, bool inherits = false)
    {
        return Register<TOwner, T>(std::move(name), std::move(defaultValue), inherits);
    }

    class AvaloniaPropertyChangedEventArgs final
    {
    public:
        AvaloniaPropertyChangedEventArgs(AvaloniaObject& sender, const AvaloniaProperty& property, std::any oldValue,
            std::any newValue)
            : Sender(sender), Property(property), OldValue(std::move(oldValue)), NewValue(std::move(newValue))
        {
        }

        AvaloniaObject& Sender;
        const AvaloniaProperty& Property;
        std::any OldValue;
        std::any NewValue;

        template <typename T>
        [[nodiscard]] T GetOldValue() const
        {
            return OldValue.has_value() ? std::any_cast<T>(OldValue) : T{};
        }
        template <typename T>
        [[nodiscard]] T GetNewValue() const
        {
            return NewValue.has_value() ? std::any_cast<T>(NewValue) : T{};
        }
    };

    enum class BindingPriority : std::int32_t
    {
        Animation = -1,
        LocalValue = 0,
        StyleTrigger = 1,
        Template = 2,
        Style = 3,
        Inherited = 4,
        Unset = 2147483647
    };

    class AvaloniaObject : public std::enable_shared_from_this<AvaloniaObject>
    {
    public:
        AvaloniaObject() = default;
        virtual ~AvaloniaObject() = default;

        AvaloniaObject(const AvaloniaObject&) = delete;
        AvaloniaObject& operator=(const AvaloniaObject&) = delete;

        template <typename T>
        [[nodiscard]] T GetValue(const StyledProperty<T>& property) const
        {
            const std::any* value = Find(property);
            return value == nullptr ? std::any_cast<T>(property.DefaultValue()) : std::any_cast<T>(*value);
        }

        template <typename T, typename U>
        void SetValue(const StyledProperty<T>& property, U&& value,
            BindingPriority priority = BindingPriority::LocalValue)
        {
            SetAny(property, std::any(T(std::forward<U>(value))), priority);
        }

        // Changes the value without changing where it came from.
        template <typename T, typename U>
        void SetCurrentValue(const StyledProperty<T>& property, U&& value)
        {
            SetAny(property, std::any(T(std::forward<U>(value))), BindingPriority::LocalValue);
        }

        void ClearValue(const AvaloniaProperty& property);
        [[nodiscard]] bool IsSet(const AvaloniaProperty& property) const noexcept;

        // The object inherited values are read from: the logical parent.
        [[nodiscard]] AvaloniaObject* InheritanceParent() const noexcept { return _inheritanceParent; }

        // AvaloniaObject.PropertyChanged.
        std::function<void(const AvaloniaPropertyChangedEventArgs&)> PropertyChanged{};

    protected:
        virtual void OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change);
        void SetInheritanceParent(AvaloniaObject* parent);
        // Every inheriting property this object reads from its parent, when
        // the parent changed underneath it.
        virtual void InheritedValuesChanged(const AvaloniaProperty& property, const std::any& oldValue);
        virtual void ForEachInheritanceChild(const std::function<void(AvaloniaObject&)>& visit);

    private:
        [[nodiscard]] const std::any* Find(const AvaloniaProperty& property) const;
        void SetAny(const AvaloniaProperty& property, std::any value, BindingPriority priority);
        void Raise(const AvaloniaProperty& property, const std::any& oldValue, const std::any& newValue);

        struct Entry final
        {
            std::any Value;
            BindingPriority Priority = BindingPriority::LocalValue;
        };
        std::unordered_map<const AvaloniaProperty*, Entry> _values;
        AvaloniaObject* _inheritanceParent = nullptr;
    };

    // Avalonia.Interactivity's plain event: a list of handlers, raised in order.
    template <typename... Args>
    class Event final
    {
    public:
        using Handler = std::function<void(Args...)>;

        std::size_t operator+=(Handler handler)
        {
            _handlers.emplace_back(++_next, std::move(handler));
            return _next;
        }
        void Remove(std::size_t token)
        {
            std::erase_if(_handlers, [token](const auto& entry) { return entry.first == token; });
        }
        void operator()(Args... args) const
        {
            // A handler may remove itself; iterate a copy.
            const auto handlers = _handlers;
            for (const auto& [token, handler] : handlers)
            {
                (void)token;
                handler(args...);
            }
        }
        [[nodiscard]] bool Empty() const noexcept { return _handlers.empty(); }
        void Clear() noexcept { _handlers.clear(); }

    private:
        std::vector<std::pair<std::size_t, Handler>> _handlers;
        std::size_t _next = 0;
    };
}
