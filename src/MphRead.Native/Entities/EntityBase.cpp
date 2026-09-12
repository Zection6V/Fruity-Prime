#include "EntityBase.hpp"

#include "../Formats/Collision.hpp"
#include "../Formats/Entity.hpp"
#include "../Mods/Network/DemoPlayback.hpp"
#include "../Read.hpp"
#include "../Renderer.hpp"
#include "../Selection.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace
{
    using OpenTK::Mathematics::Matrix3;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    [[nodiscard]] constexpr Matrix4 IdentityMatrix() noexcept
    {
        return Matrix4(
            Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] constexpr bool Equal(Vector3 left, Vector3 right) noexcept
    {
        return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
    }

    [[nodiscard]] constexpr bool Equal(Matrix4 left, Matrix4 right) noexcept
    {
        return left.M11 == right.M11 && left.M12 == right.M12
            && left.M13 == right.M13 && left.M14 == right.M14
            && left.M21 == right.M21 && left.M22 == right.M22
            && left.M23 == right.M23 && left.M24 == right.M24
            && left.M31 == right.M31 && left.M32 == right.M32
            && left.M33 == right.M33 && left.M34 == right.M34
            && left.M41 == right.M41 && left.M42 == right.M42
            && left.M43 == right.M43 && left.M44 == right.M44;
    }

    [[nodiscard]] constexpr Vector3 Add(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
    }

    [[nodiscard]] constexpr Vector3 Multiply(Vector3 value, float scalar) noexcept
    {
        return Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
    }

    [[nodiscard]] constexpr Vector3 Divide(Vector3 value, float scalar) noexcept
    {
        return Vector3(value.X / scalar, value.Y / scalar, value.Z / scalar);
    }

    [[nodiscard]] float Length(Vector3 value) noexcept
    {
        return std::sqrt(value.X * value.X + value.Y * value.Y + value.Z * value.Z);
    }

    [[nodiscard]] Matrix4 Multiply(Matrix4 left, Matrix4 right) noexcept
    {
        Matrix4 result{};
        result.M11 = left.M11 * right.M11 + left.M12 * right.M21 + left.M13 * right.M31 + left.M14 * right.M41;
        result.M12 = left.M11 * right.M12 + left.M12 * right.M22 + left.M13 * right.M32 + left.M14 * right.M42;
        result.M13 = left.M11 * right.M13 + left.M12 * right.M23 + left.M13 * right.M33 + left.M14 * right.M43;
        result.M14 = left.M11 * right.M14 + left.M12 * right.M24 + left.M13 * right.M34 + left.M14 * right.M44;
        result.M21 = left.M21 * right.M11 + left.M22 * right.M21 + left.M23 * right.M31 + left.M24 * right.M41;
        result.M22 = left.M21 * right.M12 + left.M22 * right.M22 + left.M23 * right.M32 + left.M24 * right.M42;
        result.M23 = left.M21 * right.M13 + left.M22 * right.M23 + left.M23 * right.M33 + left.M24 * right.M43;
        result.M24 = left.M21 * right.M14 + left.M22 * right.M24 + left.M23 * right.M34 + left.M24 * right.M44;
        result.M31 = left.M31 * right.M11 + left.M32 * right.M21 + left.M33 * right.M31 + left.M34 * right.M41;
        result.M32 = left.M31 * right.M12 + left.M32 * right.M22 + left.M33 * right.M32 + left.M34 * right.M42;
        result.M33 = left.M31 * right.M13 + left.M32 * right.M23 + left.M33 * right.M33 + left.M34 * right.M43;
        result.M34 = left.M31 * right.M14 + left.M32 * right.M24 + left.M33 * right.M34 + left.M34 * right.M44;
        result.M41 = left.M41 * right.M11 + left.M42 * right.M21 + left.M43 * right.M31 + left.M44 * right.M41;
        result.M42 = left.M41 * right.M12 + left.M42 * right.M22 + left.M43 * right.M32 + left.M44 * right.M42;
        result.M43 = left.M41 * right.M13 + left.M42 * right.M23 + left.M43 * right.M33 + left.M44 * right.M43;
        result.M44 = left.M41 * right.M14 + left.M42 * right.M24 + left.M43 * right.M34 + left.M44 * right.M44;
        return result;
    }

    [[nodiscard]] constexpr Matrix4 CreateScale(Vector3 scale) noexcept
    {
        return Matrix4(
            Vector4(scale.X, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, scale.Y, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, scale.Z, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] constexpr Matrix4 CreateScale(float x, float y, float z) noexcept
    {
        return CreateScale(Vector3(x, y, z));
    }

    [[nodiscard]] Matrix4 CreateRotationX(float angle) noexcept
    {
        const float sin = std::sin(angle);
        const float cos = std::cos(angle);
        return Matrix4(
            Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, cos, sin, 0.0F),
            Vector4(0.0F, -sin, cos, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] Matrix4 CreateRotationY(float angle) noexcept
    {
        const float sin = std::sin(angle);
        const float cos = std::cos(angle);
        return Matrix4(
            Vector4(cos, 0.0F, -sin, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(sin, 0.0F, cos, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] Matrix4 CreateRotationZ(float angle) noexcept
    {
        const float sin = std::sin(angle);
        const float cos = std::cos(angle);
        return Matrix4(
            Vector4(cos, sin, 0.0F, 0.0F),
            Vector4(-sin, cos, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] constexpr Matrix4 CreateTranslation(float x, float y, float z) noexcept
    {
        return Matrix4(
            Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(x, y, z, 1.0F));
    }

    [[nodiscard]] Vector3 ExtractScale(Matrix4 value) noexcept
    {
        return Vector3(
            Length(Vector3(value.M11, value.M12, value.M13)),
            Length(Vector3(value.M21, value.M22, value.M23)),
            Length(Vector3(value.M31, value.M32, value.M33)));
    }

    struct Quaternion
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;
        float W = 0.0F;
    };

    [[nodiscard]] Vector3 ExtractEulerAngles(Matrix4 value) noexcept
    {
        Vector3 row0(value.M11, value.M12, value.M13);
        Vector3 row1(value.M21, value.M22, value.M23);
        Vector3 row2(value.M31, value.M32, value.M33);
        row0 = row0.Normalized();
        row1 = row1.Normalized();
        row2 = row2.Normalized();

        Quaternion q{};
        const double trace = 0.25 * (row0.X + row1.Y + row2.Z + 1.0);
        if (trace > 0.0)
        {
            double sq = std::sqrt(trace);
            q.W = static_cast<float>(sq);
            sq = 1.0 / (4.0 * sq);
            q.X = static_cast<float>((row1.Z - row2.Y) * sq);
            q.Y = static_cast<float>((row2.X - row0.Z) * sq);
            q.Z = static_cast<float>((row0.Y - row1.X) * sq);
        }
        else if (row0.X > row1.Y && row0.X > row2.Z)
        {
            double sq = 2.0 * std::sqrt(1.0 + row0.X - row1.Y - row2.Z);
            q.X = static_cast<float>(0.25 * sq);
            sq = 1.0 / sq;
            q.W = static_cast<float>((row2.Y - row1.Z) * sq);
            q.Y = static_cast<float>((row1.X + row0.Y) * sq);
            q.Z = static_cast<float>((row2.X + row0.Z) * sq);
        }
        else if (row1.Y > row2.Z)
        {
            double sq = 2.0 * std::sqrt(1.0 + row1.Y - row0.X - row2.Z);
            q.Y = static_cast<float>(0.25 * sq);
            sq = 1.0 / sq;
            q.W = static_cast<float>((row2.X - row0.Z) * sq);
            q.X = static_cast<float>((row1.X + row0.Y) * sq);
            q.Z = static_cast<float>((row2.Y + row1.Z) * sq);
        }
        else
        {
            double sq = 2.0 * std::sqrt(1.0 + row2.Z - row0.X - row1.Y);
            q.Z = static_cast<float>(0.25 * sq);
            sq = 1.0 / sq;
            q.W = static_cast<float>((row1.X - row0.Y) * sq);
            q.X = static_cast<float>((row2.X + row0.Z) * sq);
            q.Y = static_cast<float>((row2.Y + row1.Z) * sq);
        }

        const float inverseLength = 1.0F / std::sqrt(
            q.W * q.W + q.X * q.X + q.Y * q.Y + q.Z * q.Z);
        q.X *= inverseLength;
        q.Y *= inverseLength;
        q.Z *= inverseLength;
        q.W *= inverseLength;

        constexpr float singularityThreshold = 0.4999995F;
        constexpr float piOver2 = 1.57079632679489661923F;
        const float sqw = q.W * q.W;
        const float sqx = q.X * q.X;
        const float sqy = q.Y * q.Y;
        const float sqz = q.Z * q.Z;
        const float unit = sqx + sqy + sqz + sqw;
        const float singularityTest = q.X * q.Z + q.W * q.Y;

        Vector3 angles{};
        if (singularityTest > singularityThreshold * unit)
        {
            angles.Z = 2.0F * std::atan2(q.X, q.W);
            angles.Y = piOver2;
            angles.X = 0.0F;
        }
        else if (singularityTest < -singularityThreshold * unit)
        {
            angles.Z = -2.0F * std::atan2(q.X, q.W);
            angles.Y = -piOver2;
            angles.X = 0.0F;
        }
        else
        {
            angles.Z = std::atan2(2.0F * (q.W * q.Z - q.X * q.Y),
                sqw + sqx - sqy - sqz);
            angles.Y = std::asin(2.0F * singularityTest / unit);
            angles.X = std::atan2(2.0F * (q.W * q.X - q.Y * q.Z),
                sqw - sqx - sqy + sqz);
        }
        return angles;
    }

    [[nodiscard]] Matrix4 ClearScale(Matrix4 value) noexcept
    {
        const Vector3 scale = ExtractScale(value);
        value.M11 /= scale.X;
        value.M12 /= scale.X;
        value.M13 /= scale.X;
        value.M21 /= scale.Y;
        value.M22 /= scale.Y;
        value.M23 /= scale.Y;
        value.M31 /= scale.Z;
        value.M32 /= scale.Z;
        value.M33 /= scale.Z;
        return value;
    }

    [[nodiscard]] Matrix4 Invert(Matrix4 value)
    {
        const float a = value.M11;
        const float b = value.M21;
        const float c = value.M31;
        const float d = value.M41;
        const float e = value.M12;
        const float f = value.M22;
        const float g = value.M32;
        const float h = value.M42;
        const float i = value.M13;
        const float j = value.M23;
        const float k = value.M33;
        const float l = value.M43;
        const float m = value.M14;
        const float n = value.M24;
        const float o = value.M34;
        const float p = value.M44;

        const float kpLo = k * p - l * o;
        const float jpLn = j * p - l * n;
        const float joKn = j * o - k * n;
        const float ipLm = i * p - l * m;
        const float ioKm = i * o - k * m;
        const float inJm = i * n - j * m;

        const float a11 = +(f * kpLo - g * jpLn + h * joKn);
        const float a12 = -(e * kpLo - g * ipLm + h * ioKm);
        const float a13 = +(e * jpLn - f * ipLm + h * inJm);
        const float a14 = -(e * joKn - f * ioKm + g * inJm);

        const float det = a * a11 + b * a12 + c * a13 + d * a14;
        if (std::abs(det) < std::numeric_limits<float>::denorm_min())
        {
            throw std::runtime_error("Matrix is singular and cannot be inverted.");
        }

        const float invDet = 1.0F / det;
        Matrix4 result{};
        result.M11 = a11 * invDet;
        result.M12 = a12 * invDet;
        result.M13 = a13 * invDet;
        result.M14 = a14 * invDet;

        result.M21 = -(b * kpLo - c * jpLn + d * joKn) * invDet;
        result.M22 = +(a * kpLo - c * ipLm + d * ioKm) * invDet;
        result.M23 = -(a * jpLn - b * ipLm + d * inJm) * invDet;
        result.M24 = +(a * joKn - b * ioKm + c * inJm) * invDet;

        const float gpHo = g * p - h * o;
        const float fpHn = f * p - h * n;
        const float foGn = f * o - g * n;
        const float epHm = e * p - h * m;
        const float eoGm = e * o - g * m;
        const float enFm = e * n - f * m;

        result.M31 = +(b * gpHo - c * fpHn + d * foGn) * invDet;
        result.M32 = -(a * gpHo - c * epHm + d * eoGm) * invDet;
        result.M33 = +(a * fpHn - b * epHm + d * enFm) * invDet;
        result.M34 = -(a * foGn - b * eoGm + c * enFm) * invDet;

        const float glHk = g * l - h * k;
        const float flHj = f * l - h * j;
        const float fkGj = f * k - g * j;
        const float elHi = e * l - h * i;
        const float ekGi = e * k - g * i;
        const float ejFi = e * j - f * i;

        result.M41 = -(b * glHk - c * flHj + d * fkGj) * invDet;
        result.M42 = +(a * glHk - c * elHi + d * ekGi) * invDet;
        result.M43 = -(a * flHj - b * elHi + d * ejFi) * invDet;
        result.M44 = +(a * fkGj - b * ekGi + c * ejFi) * invDet;
        return result;
    }

    [[nodiscard]] constexpr Vector3 Multiply(Vector3 value, Matrix3 matrix) noexcept
    {
        return Vector3(
            value.X * matrix.M11 + value.Y * matrix.M21 + value.Z * matrix.M31,
            value.X * matrix.M12 + value.Y * matrix.M22 + value.Z * matrix.M32,
            value.X * matrix.M13 + value.Y * matrix.M23 + value.Z * matrix.M33);
    }

    [[nodiscard]] Matrix4 Multiply(Matrix4 value, float scalar) noexcept
    {
        value.M11 *= scalar;
        value.M12 *= scalar;
        value.M13 *= scalar;
        value.M14 *= scalar;
        value.M21 *= scalar;
        value.M22 *= scalar;
        value.M23 *= scalar;
        value.M24 *= scalar;
        value.M31 *= scalar;
        value.M32 *= scalar;
        value.M33 *= scalar;
        value.M34 *= scalar;
        value.M41 *= scalar;
        value.M42 *= scalar;
        value.M43 *= scalar;
        value.M44 *= scalar;
        return value;
    }

    [[nodiscard]] float Clamp(float value, float minimum, float maximum)
    {
        if (minimum > maximum)
        {
            throw std::invalid_argument("'min' cannot be greater than max.");
        }
        if (value < minimum)
        {
            return minimum;
        }
        if (value > maximum)
        {
            return maximum;
        }
        return value;
    }
}

namespace MphRead::Entities
{
    EntityBase::MatrixProperty::MatrixProperty(EntityBase* owner) noexcept
        : M11(owner->_transform.M11), M12(owner->_transform.M12),
          M13(owner->_transform.M13), M14(owner->_transform.M14),
          M21(owner->_transform.M21), M22(owner->_transform.M22),
          M23(owner->_transform.M23), M24(owner->_transform.M24),
          M31(owner->_transform.M31), M32(owner->_transform.M32),
          M33(owner->_transform.M33), M34(owner->_transform.M34),
          M41(owner->_transform.M41), M42(owner->_transform.M42),
          M43(owner->_transform.M43), M44(owner->_transform.M44),
          _owner(owner)
    {
    }

    EntityBase::MatrixProperty& EntityBase::MatrixProperty::operator=(Matrix4 value)
    {
        _owner->SetTransformProperty(value);
        return *this;
    }

    EntityBase::MatrixProperty::operator Matrix4() const noexcept
    {
        return _owner->_transform;
    }

    Vector4 EntityBase::MatrixProperty::Row0() const noexcept
    {
        return Vector4(M11, M12, M13, M14);
    }

    Vector4 EntityBase::MatrixProperty::Row1() const noexcept
    {
        return Vector4(M21, M22, M23, M24);
    }

    Vector4 EntityBase::MatrixProperty::Row2() const noexcept
    {
        return Vector4(M31, M32, M33, M34);
    }

    Vector4 EntityBase::MatrixProperty::Row3() const noexcept
    {
        return Vector4(M41, M42, M43, M44);
    }

    EntityBase::VectorProperty::VectorProperty(
        EntityBase* owner, VectorPropertyKind kind) noexcept
        : X(kind == VectorPropertyKind::Scale ? owner->_scale.X
            : kind == VectorPropertyKind::Rotation ? owner->_rotation.X : owner->_position.X),
          Y(kind == VectorPropertyKind::Scale ? owner->_scale.Y
            : kind == VectorPropertyKind::Rotation ? owner->_rotation.Y : owner->_position.Y),
          Z(kind == VectorPropertyKind::Scale ? owner->_scale.Z
            : kind == VectorPropertyKind::Rotation ? owner->_rotation.Z : owner->_position.Z),
          _owner(owner), _kind(kind)
    {
    }

    EntityBase::VectorProperty& EntityBase::VectorProperty::operator=(Vector3 value)
    {
        if (_kind == VectorPropertyKind::Scale)
        {
            _owner->SetScaleProperty(value);
        }
        else if (_kind == VectorPropertyKind::Rotation)
        {
            _owner->SetRotationProperty(value);
        }
        else
        {
            _owner->SetPositionProperty(value);
        }
        return *this;
    }

    EntityBase::VectorProperty::operator Vector3() const noexcept
    {
        if (_kind == VectorPropertyKind::Scale)
        {
            return _owner->_scale;
        }
        if (_kind == VectorPropertyKind::Rotation)
        {
            return _owner->_rotation;
        }
        return _owner->_position;
    }

    Vector3 EntityBase::VectorProperty::Normalized() const
    {
        return static_cast<Vector3>(*this).Normalized();
    }

    std::size_t EntityBase::ModelList::Size() const noexcept
    {
        return _items.size();
    }

    ModelInstance& EntityBase::ModelList::operator[](std::size_t index)
    {
        return *_items.at(index);
    }

    const ModelInstance& EntityBase::ModelList::operator[](std::size_t index) const
    {
        return *_items.at(index);
    }

    void EntityBase::ModelList::Add(std::shared_ptr<ModelInstance> value)
    {
        _items.push_back(std::move(value));
    }

    const std::vector<std::shared_ptr<ModelInstance>>& EntityBase::ModelList::Items() const noexcept
    {
        return _items;
    }

    EntityBase::EntityBase(EntityType type, Scene* scene)
        : Type(type),
          _scene(scene),
          Transform(this),
          Scale(this, VectorPropertyKind::Scale),
          Rotation(this, VectorPropertyKind::Rotation),
          Position(this, VectorPropertyKind::Position)
    {
    }

    EntityBase::EntityBase(EntityType type, std::string nodeName, Scene* scene)
        : Type(type),
          _scene(scene),
          _nodeName(std::move(nodeName)),
          Transform(this),
          Scale(this, VectorPropertyKind::Scale),
          Rotation(this, VectorPropertyKind::Rotation),
          Position(this, VectorPropertyKind::Position)
    {
    }

    EntityBase::EntityBase(EntityType type, MphRead::Formats::Culling::NodeRef nodeRef, Scene* scene)
        : Type(type),
          NodeRef(std::move(nodeRef)),
          _scene(scene),
          Transform(this),
          Scale(this, VectorPropertyKind::Scale),
          Rotation(this, VectorPropertyKind::Rotation),
          Position(this, VectorPropertyKind::Position)
    {
    }

    EntityBase::~EntityBase() = default;

    std::int32_t EntityBase::Recolor() const
    {
        return _recolor;
    }

    void EntityBase::SetRecolor(std::int32_t value)
    {
        _recolor = value;
    }

    void EntityBase::SetTransformProperty(Matrix4 value)
    {
        if (!Equal(_transform, value))
        {
            _scale = ExtractScale(value);
            _rotation = ExtractEulerAngles(value);
            _position = Vector3(value.M41, value.M42, value.M43);
            _transform = value;
            _drawColUpdated = false;
        }
    }

    void EntityBase::SetScaleProperty(Vector3 value)
    {
        if (!Equal(_scale, value))
        {
            Matrix4 transform = Multiply(CreateScale(value), CreateRotationZ(Rotation.Z));
            transform = Multiply(transform, CreateRotationY(Rotation.Y));
            transform = Multiply(transform, CreateRotationX(Rotation.X));
            transform.M41 = Position.X;
            transform.M42 = Position.Y;
            transform.M43 = Position.Z;
            _transform = transform;
            _scale = value;
            _drawColUpdated = false;
        }
    }

    void EntityBase::SetRotationProperty(Vector3 value)
    {
        if (!Equal(_rotation, value))
        {
            Matrix4 transform = Multiply(CreateScale(Scale), CreateRotationZ(value.Z));
            transform = Multiply(transform, CreateRotationY(value.Y));
            transform = Multiply(transform, CreateRotationX(value.X));
            transform.M41 = Position.X;
            transform.M42 = Position.Y;
            transform.M43 = Position.Z;
            _transform = transform;
            _rotation = value;
            _drawColUpdated = false;
        }
    }

    void EntityBase::SetPositionProperty(Vector3 value)
    {
        if (!Equal(_position, value))
        {
            Matrix4 transform = _transform;
            transform.M41 = value.X;
            transform.M42 = value.Y;
            transform.M43 = value.Z;
            _transform = transform;
            _position = value;
            _drawColUpdated = false;
        }
    }

    Matrix4 EntityBase::CollisionTransform() const
    {
        return _colAttachNode == nullptr ? _transform : _colAttachNode->Animation;
    }

    Vector3 EntityBase::RightVector() const
    {
        return Vector3(Transform.M11, Transform.M12, Transform.M13).Normalized();
    }

    Vector3 EntityBase::UpVector() const
    {
        return Vector3(Transform.M21, Transform.M22, Transform.M23).Normalized();
    }

    Vector3 EntityBase::FacingVector() const
    {
        return Vector3(Transform.M31, Transform.M32, Transform.M33).Normalized();
    }

    bool EntityBase::UseNodeTransform() const
    {
        return true;
    }

    std::optional<Vector4> EntityBase::OverrideColor() const
    {
        return std::nullopt;
    }

    std::optional<Vector4> EntityBase::PaletteOverride() const
    {
        return _paletteOverride;
    }

    void EntityBase::SetPaletteOverride(std::optional<Vector4> value)
    {
        _paletteOverride = value;
    }

    void EntityBase::Initialize()
    {
        bool anyLighting = false;
        for (const std::shared_ptr<ModelInstance>& inst : _models.Items())
        {
            for (const Material& material : inst->Model->Materials)
            {
                if (material.Lighting != 0)
                {
                    anyLighting = true;
                    break;
                }
            }
            if (anyLighting)
            {
                break;
            }
        }
        _anyLighting = _anyLighting | anyLighting;
        if (_nodeName.has_value())
        {
            NodeRef = _scene->GetNodeRefByName(*_nodeName);
        }
    }

    ModelInstance& EntityBase::SetUpModel(
        std::string name, std::int32_t animIndex, AnimFlags animFlags, bool firstHunt)
    {
        std::shared_ptr<ModelInstance> inst = Read::GetModelInstance(name, firstHunt);
        if (inst == nullptr)
        {
            throw System::NullReferenceException();
        }
        inst->SetAnimation(animIndex, animFlags);
        _models.Add(inst);
        return *inst;
    }

    void EntityBase::SetCollision(
        MphRead::Formats::Collision::CollisionInstance* collision,
        std::int32_t slot, ModelInstance* attach)
    {
        auto entCol = std::make_shared<MphRead::Formats::Collision::EntityCollision>(collision, this);
        SetCollisionMaxAvg(*entCol);
        EntityCollision.at(static_cast<std::size_t>(slot)) = entCol;
        _drawColUpdated = false;
        UpdateCollisionTransform(slot, ClearScale(Transform));
        UpdateLinkedInverse(slot);
        if (entCol->Collision != nullptr)
        {
            entCol->DrawPoints.insert(entCol->DrawPoints.end(),
                entCol->Collision->Info->Points.begin(), entCol->Collision->Info->Points.end());
        }
        if (attach != nullptr)
        {
            _colAttachNode = attach->Model->GetNodeByName("attach");
        }
    }

    void EntityBase::SetCollisionMaxAvg(MphRead::Formats::Collision::EntityCollision& entCol)
    {
        if (entCol.Collision == nullptr)
        {
            return;
        }
        const std::size_t count = entCol.Collision->Info->Points.size();
        Vector3 average = Vector3::Zero;
        for (Vector3 point : entCol.Collision->Info->Points)
        {
            average.X += point.X;
            average.Y += point.Y;
            average.Z += point.Z;
        }
        average = Divide(average, static_cast<float>(count));
        entCol.InitialCenter = average;
        float maxDistance = 0.0F;
        for (Vector3 point : entCol.Collision->Info->Points)
        {
            const float distance = Vector3::Distance(point, average);
            if (distance > maxDistance)
            {
                maxDistance = distance;
            }
        }
        entCol.MaxDistance = maxDistance;
    }

    void EntityBase::UpdateCollisionTransform(std::int32_t slot, Matrix4 transform)
    {
        std::shared_ptr<MphRead::Formats::Collision::EntityCollision>& entCol
            = EntityCollision.at(static_cast<std::size_t>(slot));
        if (entCol != nullptr)
        {
            entCol->Transform = transform;
            entCol->Inverse1 = Invert(transform);
            entCol->CurrentCenter = Matrix::Vec3MultMtx4(entCol->InitialCenter, transform);
        }
    }

    void EntityBase::UpdateLinkedInverse(std::int32_t slot)
    {
        std::shared_ptr<MphRead::Formats::Collision::EntityCollision>& entCol
            = EntityCollision.at(static_cast<std::size_t>(slot));
        if (entCol != nullptr)
        {
            entCol->Inverse2 = Invert(entCol->Transform);
        }
    }

    void EntityBase::UpdateDrawCollision()
    {
        if (!_drawColUpdated || _colAttachNode != nullptr)
        {
            const Matrix4 transform = CollisionTransform();
            for (std::size_t i = 0; i < 2; i++)
            {
                const std::shared_ptr<MphRead::Formats::Collision::EntityCollision>& entCol = EntityCollision[i];
                if (entCol != nullptr && entCol->Collision != nullptr)
                {
                    const std::vector<Vector3>& points = entCol->Collision->Info->Points;
                    for (std::size_t j = 0; j < points.size(); j++)
                    {
                        entCol->DrawPoints.at(j) = Matrix::Vec3MultMtx4(points[j], transform);
                    }
                }
            }
            _drawColUpdated = true;
        }
    }

    void EntityBase::Destroy()
    {
    }

    Matrix4 EntityBase::GetModelTransform(ModelInstance& inst, std::int32_t index)
    {
        (void)index;
        return Multiply(CreateScale(inst.Model->Scale), _transform);
    }

    void EntityBase::GetPosition(Vector3& position)
    {
        position = Position;
    }

    void EntityBase::GetVectors(Vector3& position, Vector3& up, Vector3& facing)
    {
        position = Position;
        up = UpVector();
        facing = FacingVector();
    }

    bool EntityBase::GetTargetable()
    {
        return true;
    }

    std::int32_t EntityBase::GetScanId(bool alternate)
    {
        (void)alternate;
        return _scanId;
    }

    void EntityBase::OnScanned()
    {
    }

    bool EntityBase::Process()
    {
        if (Active)
        {
            for (std::size_t i = 0; i < _models.Size(); i++)
            {
                UpdateAnimFrames(_models[i]);
            }
        }
        return true;
    }

    void EntityBase::UpdateAnimFrames(ModelInstance& inst)
    {
        if (_scene->FrameCount != 0 && _scene->FrameCount % 2 == 0)
        {
            inst.UpdateAnimFrames();
        }
    }

    std::int32_t EntityBase::GetModelRecolor(ModelInstance& inst, std::int32_t index)
    {
        (void)inst;
        (void)index;
        return Recolor();
    }

    const std::vector<std::shared_ptr<ModelInstance>>& EntityBase::GetModels() const
    {
        return _models.Items();
    }

    void EntityBase::AddPlaceholderModel()
    {
        std::shared_ptr<ModelInstance> inst = Read::GetModelInstance("pick_wpn_missile", false);
        if (inst == nullptr)
        {
            throw System::NullReferenceException();
        }
        inst->IsPlaceholder = true;
        _models.Add(std::move(inst));
    }

    std::optional<Vector4> EntityBase::GetOverrideColor(ModelInstance& inst, std::int32_t index)
    {
        (void)inst;
        (void)index;
        return OverrideColor();
    }

    LightInfo EntityBase::GetLightInfo()
    {
        return LightInfo(_scene->Light1Vector, _scene->Light1Color,
            _scene->Light2Vector, _scene->Light2Color);
    }

    std::optional<std::int32_t> EntityBase::GetBindingOverride(
        ModelInstance& inst, Material& material, std::int32_t index)
    {
        (void)inst;
        (void)material;
        (void)index;
        return std::nullopt;
    }

    void EntityBase::UpdateTransforms(ModelInstance& inst, std::int32_t index)
    {
        Model& model = *inst.Model;
        model.AnimateMaterials(inst.AnimInfo);
        model.AnimateTextures(inst.AnimInfo);
        model.ComputeNodeMatrices(0);
        const Matrix4 transform = GetModelTransform(inst, index);
        model.AnimateNodes(0, UseNodeTransform() || _scene->TransformRoomNodes,
            transform, model.Scale, inst.AnimInfo);
        model.UpdateMatrixStack();
        _scene->UpdateMaterials(model, GetModelRecolor(inst, index));
        if (_scene->ShowCollision)
        {
            UpdateDrawCollision();
        }
    }

    void EntityBase::UpdateTransforms(ModelInstance& inst, Matrix4 transform, std::int32_t recolor)
    {
        Model& model = *inst.Model;
        model.AnimateMaterials(inst.AnimInfo);
        model.AnimateTextures(inst.AnimInfo);
        model.ComputeNodeMatrices(0);
        model.AnimateNodes(0, UseNodeTransform(), transform, model.Scale, inst.AnimInfo);
        model.UpdateMatrixStack();
        _scene->UpdateMaterials(model, recolor);
    }

    void EntityBase::UpdateMaterials(ModelInstance& inst, std::int32_t recolor)
    {
        Model& model = *inst.Model;
        model.AnimateMaterials(inst.AnimInfo);
        model.AnimateTextures(inst.AnimInfo);
        _scene->UpdateMaterials(model, recolor);
    }

    void EntityBase::GetDrawItems(ModelInstance& inst, std::int32_t i, std::optional<LightInfo> lightInfo)
    {
        const std::int32_t polygonId = _scene->GetNextPolygonId();
        Model& model = *inst.Model;
        auto getItems = [&](auto&& self, Node& node) -> void
        {
            if (node.Enabled)
            {
                const std::int32_t start = node.MeshId / 2;
                for (std::int32_t k = 0; k < node.MeshCount; k++)
                {
                    Mesh& mesh = model.Meshes.at(static_cast<std::size_t>(start + k));
                    if (!mesh.Visible)
                    {
                        continue;
                    }
                    Material& material = model.Materials.at(static_cast<std::size_t>(mesh.MaterialId));
                    const Vector3 emission = GetEmission(inst, material, mesh.MaterialId);
                    const Matrix4 texcoordMatrix = GetTexcoordMatrix(inst, material,
                        mesh.MaterialId, node);
                    const std::optional<Vector4> color = inst.IsPlaceholder
                        ? GetOverrideColor(inst, i) : std::nullopt;
                    const SelectionType selectionType = Selection::CheckSelection(this, inst, node, mesh);
                    const std::optional<std::int32_t> bindingOverride
                        = GetBindingOverride(inst, material, mesh.MaterialId);
                    _scene->AddRenderItem(material, polygonId, Alpha, emission,
                        lightInfo.has_value() ? *lightInfo : GetLightInfo(), texcoordMatrix,
                        node.Animation, mesh.ListId, static_cast<std::int32_t>(model.NodeMatrixIds.size()),
                        model.MatrixStackValues, color, PaletteOverride(), selectionType,
                        node.BillboardMode, _drawScale, bindingOverride);
                }
                if (node.ChildIndex != -1)
                {
                    self(self, model.Nodes.at(static_cast<std::size_t>(node.ChildIndex)));
                }
            }
            if (node.NextIndex != -1)
            {
                self(self, model.Nodes.at(static_cast<std::size_t>(node.NextIndex)));
            }
        };
        getItems(getItems, model.Nodes.at(0));
    }

    void EntityBase::UpdateNodeRefVolume()
    {
        _soundSource.Volume = IsAudible(NodeRef) ? 1.0F : 0.0F;
    }

    bool EntityBase::IsAudible(MphRead::Formats::Culling::NodeRef nodeRef)
    {
        if (nodeRef == MphRead::Formats::Culling::NodeRef::None || _scene->CameraMode != CameraMode::Player)
        {
            return true;
        }
        return _scene->IsNodeRefAudible(nodeRef);
    }

    bool EntityBase::IsVisible(MphRead::Formats::Culling::NodeRef nodeRef)
    {
        if (nodeRef == MphRead::Formats::Culling::NodeRef::None
            || _scene->CameraMode != CameraMode::Player || _scene->ShowInvisibleEntities
            || MphRead::Mods::Network::DemoPlayback::IsActive)
        {
            return true;
        }
        return _scene->IsNodeRefVisible(nodeRef);
    }

    bool EntityBase::ScanVisible()
    {
        return IsVisible(NodeRef);
    }

    void EntityBase::GetDrawInfo()
    {
        for (std::size_t i = 0; i < _models.Size(); i++)
        {
            ModelInstance& inst = _models[i];
            if ((!inst.Active && !_scene->ShowAllEntities)
                || (inst.IsPlaceholder && !_scene->ShowInvisibleEntities && !_scene->ShowAllEntities))
            {
                continue;
            }
            UpdateTransforms(inst, static_cast<std::int32_t>(i));
            if (!Hidden)
            {
                GetDrawItems(inst, static_cast<std::int32_t>(i));
            }
        }
        if (_scene->ShowCollision
            && (_scene->ColEntDisplay == EntityType::All || _scene->ColEntDisplay == Type))
        {
            GetCollisionDrawInfo();
        }
    }

    void EntityBase::GetCollisionDrawInfo()
    {
        for (std::size_t i = 0; i < 2; i++)
        {
            const std::shared_ptr<MphRead::Formats::Collision::EntityCollision>& entCol = EntityCollision[i];
            if (entCol != nullptr && entCol->Collision != nullptr && entCol->Collision->Active)
            {
                entCol->Collision->Info->GetDrawInfo(entCol->DrawPoints,
                    Vector3::Zero, Type, _scene);
            }
        }
    }

    Vector3 EntityBase::GetEmission(ModelInstance& inst, Material& material, std::int32_t index)
    {
        (void)inst;
        (void)material;
        (void)index;
        return Vector3::Zero;
    }

    Matrix4 EntityBase::GetTexcoordMatrix(
        ModelInstance& inst, Material& material, std::int32_t materialId, Node& node,
        std::int32_t recolor)
    {
        (void)materialId;
        (void)node;
        Model& model = *inst.Model;
        Matrix4 texcoordMatrix = IdentityMatrix();
        TexcoordAnimationGroup* group = inst.AnimInfo.Texcoord.Group;
        const TexcoordAnimation* animation = nullptr;
        if (group != nullptr)
        {
            const auto found = group->Animations.find(material.Name);
            if (found != group->Animations.end())
            {
                animation = &found->second;
            }
        }
        if (group != nullptr && animation != nullptr
            && (!inst.Model->FirstHunt || material.TexgenMode != TexgenMode::None))
        {
            texcoordMatrix = model.AnimateTexcoords(*group, *animation, inst.AnimInfo.TexcoordFrame());
        }
        if (material.TexgenMode != TexgenMode::None)
        {
            Matrix4 materialMatrix;
            if (!model.TextureMatrices.empty())
            {
                materialMatrix = model.TextureMatrices.at(static_cast<std::size_t>(material.MatrixId));
            }
            else
            {
                materialMatrix = CreateTranslation(material.ScaleS * material.TranslateS,
                    material.ScaleT * material.TranslateT, 0.0F);
                materialMatrix = Multiply(CreateScale(material.ScaleS, material.ScaleT, 1.0F), materialMatrix);
                materialMatrix = Multiply(CreateRotationZ(material.RotateZ), materialMatrix);
            }
            if (group == nullptr || animation == nullptr)
            {
                texcoordMatrix = materialMatrix;
            }
            if (material.TexgenMode == TexgenMode::Normal)
            {
                const Texture& texture = model.Recolors
                    .at(static_cast<std::size_t>(recolor == -1 ? Recolor() : recolor))
                    .Textures.at(static_cast<std::size_t>(material.TextureId));
                Matrix4 texgenMatrix = IdentityMatrix();
                if (model.Scale.X != 1.0F || model.Scale.Y != 1.0F || model.Scale.Z != 1.0F)
                {
                    texgenMatrix = CreateScale(model.Scale);
                }
                Matrix4 product = texgenMatrix;
                product.M12 *= -1.0F;
                product.M13 *= -1.0F;
                product.M22 *= -1.0F;
                product.M23 *= -1.0F;
                product.M32 *= -1.0F;
                product.M33 *= -1.0F;
                product = Multiply(product, materialMatrix);
                product = Multiply(product, texcoordMatrix);
                product = Multiply(product, 1.0F / static_cast<float>(texture.Width / 2));
                texcoordMatrix = Matrix4(
                    Vector4(product.M11 * 16.0F, product.M12 * 16.0F, product.M13 * 16.0F, product.M14 * 16.0F),
                    Vector4(product.M21 * 16.0F, product.M22 * 16.0F, product.M23 * 16.0F, product.M24 * 16.0F),
                    Vector4(product.M31 * 16.0F, product.M32 * 16.0F, product.M33 * 16.0F, product.M34 * 16.0F),
                    product.Row3());
            }
        }
        return texcoordMatrix;
    }

    void EntityBase::SetTransform(Vector3Fx facing, Vector3Fx up, Vector3Fx position)
    {
        SetTransform(facing.ToFloatVector(), up.ToFloatVector(), position.ToFloatVector());
    }

    void EntityBase::SetTransform(Vector3 facing, Vector3 up, Vector3 position)
    {
        Matrix4 transform = GetTransformMatrix(facing, up);
        transform = Multiply(CreateScale(_scale), transform);
        transform.M41 = position.X;
        transform.M42 = position.Y;
        transform.M43 = position.Z;
        Transform = transform;
    }

    Matrix4 EntityBase::GetTransformMatrix(Vector3 facing, Vector3 up)
    {
        const Vector3 right = Vector3::Cross(up, facing).Normalized();
        up = Vector3::Cross(facing, right);
        Matrix4 transform{};
        transform.M11 = right.X;
        transform.M12 = right.Y;
        transform.M13 = right.Z;
        transform.M14 = 0.0F;
        transform.M21 = up.X;
        transform.M22 = up.Y;
        transform.M23 = up.Z;
        transform.M24 = 0.0F;
        transform.M31 = facing.X;
        transform.M32 = facing.Y;
        transform.M33 = facing.Z;
        transform.M34 = 0.0F;
        transform.M41 = 0.0F;
        transform.M42 = 0.0F;
        transform.M43 = 0.0F;
        transform.M44 = 1.0F;
        return transform;
    }

    Matrix4 EntityBase::GetTransformMatrix(Vector3 facing, Vector3 up, Vector3 position)
    {
        Matrix4 transform = GetTransformMatrix(facing, up);
        transform.M41 = position.X;
        transform.M42 = position.Y;
        transform.M43 = position.Z;
        return transform;
    }

    void EntityBase::AddDotItem(Vector3 position, Vector3 color)
    {
        AddVolumeItem(CollisionVolume(position, 0.1F), color, 1.0F);
    }

    void EntityBase::AddVolumeItem(CollisionVolume volume, Vector3 color, float alpha)
    {
        if (!Selection::CheckVolume(this))
        {
            return;
        }
        std::shared_ptr<ManagedArray<Vector3>> verts = ManagedArray<Vector3>::Empty();
        if (volume.Type == VolumeType::Box)
        {
            verts = std::make_shared<ManagedArray<Vector3>>(8);
            const Vector3 point0 = volume.BoxPosition;
            const Vector3 sideX = Multiply(volume.BoxVector1, volume.BoxDot1);
            const Vector3 sideY = Multiply(volume.BoxVector2, volume.BoxDot2);
            const Vector3 sideZ = Multiply(volume.BoxVector3, volume.BoxDot3);
            (*verts)[0] = point0;
            (*verts)[1] = Add(point0, sideZ);
            (*verts)[2] = Add(point0, sideX);
            (*verts)[3] = Add(Add(point0, sideX), sideZ);
            (*verts)[4] = Add(point0, sideY);
            (*verts)[5] = Add(Add(point0, sideY), sideZ);
            (*verts)[6] = Add(Add(point0, sideX), sideY);
            (*verts)[7] = Add(Add(Add(point0, sideX), sideY), sideZ);
        }
        else if (volume.Type == VolumeType::Cylinder)
        {
            verts = std::make_shared<ManagedArray<Vector3>>(34);
            const Vector3 vector = volume.CylinderVector.Normalized();
            const float radius = volume.CylinderRadius;
            const Matrix3 rotation = Matrix::RotateAlign(Vector3(0.0F, 1.0F, 0.0F), vector);
            Vector3 start;
            Vector3 end;
            if (Equal(vector, Vector3(1.0F, 0.0F, 0.0F))
                || Equal(vector, Vector3(0.0F, 1.0F, 0.0F))
                || Equal(vector, Vector3(0.0F, 0.0F, 1.0F)))
            {
                start = volume.CylinderPosition;
                end = Add(volume.CylinderPosition, Multiply(vector, volume.CylinderDot));
            }
            else
            {
                start = Add(volume.CylinderPosition, Multiply(vector, volume.CylinderDot));
                end = volume.CylinderPosition;
            }
            for (std::size_t i = 0; i < 16; i++)
            {
                (*verts)[i] = Add(Multiply(GetDiscVertices(radius, static_cast<std::int32_t>(i)), rotation), start);
            }
            for (std::size_t i = 0; i < 16; i++)
            {
                (*verts)[i + 16] = Add(Multiply(GetDiscVertices(radius, static_cast<std::int32_t>(i)), rotation), end);
            }
            (*verts)[32] = start;
            (*verts)[33] = end;
        }
        else if (volume.Type == VolumeType::Sphere)
        {
            constexpr std::int32_t stackCount = Scene::DisplaySphereStacks;
            constexpr std::int32_t sectorCount = Scene::DisplaySphereSectors;
            verts = std::make_shared<ManagedArray<Vector3>>(
                static_cast<std::size_t>((stackCount + 1) * (sectorCount + 1)));
            const float radius = volume.SphereRadius;
            const float pi = std::acos(-1.0F);
            const float sectorStep = 2.0F * pi / static_cast<float>(sectorCount);
            const float stackStep = pi / static_cast<float>(stackCount);
            for (std::int32_t i = 0; i <= stackCount; i++)
            {
                const float stackAngle = pi / 2.0F - static_cast<float>(i) * stackStep;
                const float xy = radius * std::cos(stackAngle);
                const float z = radius * std::sin(stackAngle);
                for (std::int32_t j = 0; j <= sectorCount; j++)
                {
                    const float sectorAngle = static_cast<float>(j) * sectorStep;
                    const float x = xy * std::cos(sectorAngle);
                    const float y = xy * std::sin(sectorAngle);
                    (*verts)[static_cast<std::size_t>(i * (sectorCount + 1) + j)]
                        = Add(Vector3(x, z, y), volume.SpherePosition);
                }
            }
        }
        const CullingMode cullingMode = volume.TestPoint(_scene->CameraPosition)
            ? CullingMode::Front : CullingMode::Back;
        _scene->AddRenderItem(cullingMode, _scene->GetNextPolygonId(),
            Vector4(color, alpha), static_cast<RenderItemType>(static_cast<std::int32_t>(volume.Type) + 1),
            verts);
    }

    Vector3 EntityBase::GetDiscVertices(float radius, std::int32_t index)
    {
        const float pi = std::acos(-1.0F);
        return Vector3(
            radius * std::cos(2.0F * pi * static_cast<float>(index) / 16.0F),
            0.0F,
            radius * std::sin(2.0F * pi * static_cast<float>(index) / 16.0F));
    }

    void EntityBase::AddVectorItem(Vector3 point, Vector3 vector, Vector3 color)
    {
        CollisionVolume volume;
        if (Equal(vector, Vector3::Zero))
        {
            volume = CollisionVolume(point, 0.1F);
        }
        else
        {
            volume = CollisionVolume(vector.Normalized(), point, 0.05F, Length(vector));
        }
        AddVolumeItem(volume, color);
    }

    void EntityBase::GetDisplayVolumes()
    {
    }

    void EntityBase::SetActive(bool active)
    {
        Active = active;
    }

    void EntityBase::SetScanId(std::int32_t scanId)
    {
        _scanId = scanId;
    }

    EntityBase* EntityBase::GetParent()
    {
        return nullptr;
    }

    EntityBase* EntityBase::GetChild()
    {
        return nullptr;
    }

    void EntityBase::HandleMessage(MessageInfo info)
    {
        (void)info;
    }

    void EntityBase::CheckContactDamage(DamageResult& result)
    {
        (void)result;
    }

    void EntityBase::CheckBeamReflection(bool& result)
    {
        (void)result;
    }

    std::tuple<float, float> EntityBase::ConstantAcceleration(
        float step, float velocity, float minVelocity, float maxVelocity)
    {
        float newVelocity = velocity + step * 30.0F * 31.0F * _scene->FrameTime;
        newVelocity = Clamp(newVelocity, minVelocity, maxVelocity);
        const float displacement = velocity * _scene->FrameTime
            + (newVelocity - velocity) / 2.0F * _scene->FrameTime;
        return {newVelocity, displacement};
    }

    std::tuple<float, float> EntityBase::Drag(float step, float velocity)
    {
        const float decay = std::pow(step, 30.0F);
        const float newVelocity = velocity * std::pow(decay, _scene->FrameTime);
        const float displacement = (newVelocity - velocity) / std::log(decay);
        return {newVelocity, displacement};
    }

    float EntityBase::ExponentialDecay(float step, float value)
    {
        const float decay = std::pow(step, 30.0F);
        return value * std::pow(decay, _scene->FrameTime);
    }

    ModelEntity::ModelEntity(std::shared_ptr<ModelInstance> model, Scene* scene, std::int32_t recolor)
        : EntityBase(EntityType::Model, scene)
    {
        SetRecolor(recolor);
        _models.Add(model);
        if (model == nullptr)
        {
            throw System::NullReferenceException();
        }
        model->SetAnimation(0);
    }
}
