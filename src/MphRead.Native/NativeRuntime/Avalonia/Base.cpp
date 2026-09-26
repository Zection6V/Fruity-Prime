#include "Base.hpp"

#include <stdexcept>

namespace MphRead::NativeRuntime::Avalonia
{
    Rect Rect::Intersect(const Rect& o) const noexcept
    {
        const double left = std::max(X, o.X);
        const double top = std::max(Y, o.Y);
        const double right = std::min(Right(), o.Right());
        const double bottom = std::min(Bottom(), o.Bottom());
        if (right < left || bottom < top)
        {
            return Rect{};
        }
        return Rect{left, top, right - left, bottom - top};
    }

    Rect Rect::Union(const Rect& o) const noexcept
    {
        if (Width == 0 && Height == 0)
        {
            return o;
        }
        if (o.Width == 0 && o.Height == 0)
        {
            return *this;
        }
        const double left = std::min(X, o.X);
        const double top = std::min(Y, o.Y);
        return Rect{left, top, std::max(Right(), o.Right()) - left, std::max(Bottom(), o.Bottom()) - top};
    }

    Rect Rect::Deflate(const Thickness& t) const noexcept
    {
        return Rect{X + t.Left, Y + t.Top, std::max(0.0, Width - t.Left - t.Right),
            std::max(0.0, Height - t.Top - t.Bottom)};
    }

    Rect Rect::Inflate(const Thickness& t) const noexcept
    {
        return Rect{X - t.Left, Y - t.Top, Width + t.Left + t.Right, Height + t.Top + t.Bottom};
    }

    Matrix Matrix::CreateRotation(double radians) noexcept
    {
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        return {c, s, -s, c, 0, 0};
    }

    std::optional<Matrix> Matrix::TryInvert() const noexcept
    {
        const double det = M11 * M22 - M12 * M21;
        if (std::abs(det) < 1e-14)
        {
            return std::nullopt;
        }
        Matrix m;
        m.M11 = M22 / det;
        m.M12 = -M12 / det;
        m.M21 = -M21 / det;
        m.M22 = M11 / det;
        m.M31 = (M21 * M32 - M22 * M31) / det;
        m.M32 = (M12 * M31 - M11 * M32) / det;
        return m;
    }

    Matrix Matrix::Invert() const
    {
        const std::optional<Matrix> inverse = TryInvert();
        if (!inverse.has_value())
        {
            throw std::invalid_argument("Transform is not invertible.");
        }
        return *inverse;
    }

    Rect TransformToAABB(const Rect& rect, const Matrix& m) noexcept
    {
        const Point a = m.Transform(rect.TopLeft());
        const Point b = m.Transform(rect.TopRight());
        const Point c = m.Transform(rect.BottomRight());
        const Point d = m.Transform(rect.BottomLeft());
        const double left = std::min({a.X, b.X, c.X, d.X});
        const double top = std::min({a.Y, b.Y, c.Y, d.Y});
        return Rect{left, top, std::max({a.X, b.X, c.X, d.X}) - left, std::max({a.Y, b.Y, c.Y, d.Y}) - top};
    }

    // ------------------------------------------------------ property system

    AvaloniaProperty::AvaloniaProperty(std::string name, const std::type_info& ownerType, std::any defaultValue,
        bool inherits)
        : _name(std::move(name)), _owner(&ownerType), _default(std::move(defaultValue)), _inherits(inherits)
    {
    }

    void AvaloniaProperty::AddAffects(std::function<bool(const AvaloniaObject&)> isOwner, Affects affects)
    {
        _affects.emplace_back(std::move(isOwner), affects);
    }

    std::uint8_t AvaloniaProperty::AffectsFor(const AvaloniaObject& target) const
    {
        std::uint8_t result = 0;
        for (const auto& [isOwner, affects] : _affects)
        {
            if (isOwner(target))
            {
                result |= static_cast<std::uint8_t>(affects);
            }
        }
        return result;
    }

    const std::any* AvaloniaObject::Find(const AvaloniaProperty& property) const
    {
        const auto found = _values.find(&property);
        if (found != _values.end())
        {
            return &found->second.Value;
        }
        if (property.Inherits())
        {
            for (const AvaloniaObject* parent = _inheritanceParent; parent != nullptr;
                parent = parent->_inheritanceParent)
            {
                const auto inherited = parent->_values.find(&property);
                if (inherited != parent->_values.end())
                {
                    return &inherited->second.Value;
                }
            }
        }
        return nullptr;
    }

    bool AvaloniaObject::IsSet(const AvaloniaProperty& property) const noexcept
    {
        return _values.contains(&property);
    }

    void AvaloniaObject::SetAny(const AvaloniaProperty& property, std::any value, BindingPriority priority)
    {
        const std::any* current = Find(property);
        std::any oldValue = current != nullptr ? *current : property.DefaultValue();
        auto found = _values.find(&property);
        if (found != _values.end() && static_cast<std::int32_t>(priority) > static_cast<std::int32_t>(found->second.Priority))
        {
            // A style cannot override a local value.
            return;
        }
        const bool changed = !property.ValuesEqual(oldValue, value);
        _values[&property] = Entry{value, priority};
        if (changed)
        {
            Raise(property, oldValue, value);
        }
    }

    void AvaloniaObject::ClearValue(const AvaloniaProperty& property)
    {
        const auto found = _values.find(&property);
        if (found == _values.end())
        {
            return;
        }
        std::any oldValue = found->second.Value;
        _values.erase(found);
        const std::any* now = Find(property);
        std::any newValue = now != nullptr ? *now : property.DefaultValue();
        if (!property.ValuesEqual(oldValue, newValue))
        {
            Raise(property, oldValue, newValue);
        }
    }

    void AvaloniaObject::Raise(const AvaloniaProperty& property, const std::any& oldValue, const std::any& newValue)
    {
        const AvaloniaPropertyChangedEventArgs change(*this, property, oldValue, newValue);
        OnPropertyChanged(change);
        if (PropertyChanged)
        {
            PropertyChanged(change);
        }
        if (property.Inherits())
        {
            ForEachInheritanceChild([&](AvaloniaObject& child) { child.InheritedValuesChanged(property, oldValue); });
        }
    }

    void AvaloniaObject::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        (void)change;
    }

    void AvaloniaObject::SetInheritanceParent(AvaloniaObject* parent)
    {
        _inheritanceParent = parent;
    }

    void AvaloniaObject::InheritedValuesChanged(const AvaloniaProperty& property, const std::any& oldValue)
    {
        if (_values.contains(&property))
        {
            return;
        }
        const std::any* now = Find(property);
        const std::any newValue = now != nullptr ? *now : property.DefaultValue();
        if (!property.ValuesEqual(oldValue, newValue))
        {
            Raise(property, oldValue, newValue);
        }
    }

    void AvaloniaObject::ForEachInheritanceChild(const std::function<void(AvaloniaObject&)>& visit)
    {
        (void)visit;
    }
}
