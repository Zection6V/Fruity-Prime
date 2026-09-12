#include "Effects.hpp"

#include "Formats.hpp"
#include "Model.hpp"
#include "../Entities/EntityBase.hpp"
#include "../Program.hpp"
#include "../Renderer.hpp"
#include "../Utility/Rng.hpp"

#include <cassert>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    using MphRead::BillboardMode;
    using MphRead::Fixed;
    using MphRead::FxFuncInfo;
    using MphRead::ManagedArray;
    using MphRead::Material;
    using MphRead::Model;
    using MphRead::Node;
    using MphRead::Particle;
    using MphRead::RepeatMode;
    using MphRead::Rng;
    using MphRead::Scene;
    using MphRead::SelectionType;
    using MphRead::TexgenMode;
    using MphRead::Effects::EffectActionDictionary;
    using MphRead::Effects::EffectFuncDictionary;
    using OpenTK::Mathematics::Matrix3;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    template <typename T>
    [[nodiscard]] T& Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] const T& Require(const std::shared_ptr<const T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& Require(T* value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] const std::vector<std::int32_t>& ParametersOf(const FxFuncInfo& info)
    {
        return Require(info.Parameters);
    }

    [[nodiscard]] const FxFuncInfo& FuncAt(
        const std::shared_ptr<const EffectFuncDictionary>& funcs,
        std::uint32_t key)
    {
        const EffectFuncDictionary& dictionary = Require(funcs);
        return Require(dictionary.at(key));
    }

    [[nodiscard]] bool HasFlag(
        MphRead::Effects::EffElemFlags value,
        MphRead::Effects::EffElemFlags flag) noexcept
    {
        return (static_cast<std::uint32_t>(value)
            & static_cast<std::uint32_t>(flag)) != 0U;
    }

    [[nodiscard]] constexpr Vector3 UnitX() noexcept
    {
        return Vector3(1.0F, 0.0F, 0.0F);
    }

    [[nodiscard]] constexpr Vector3 UnitY() noexcept
    {
        return Vector3(0.0F, 1.0F, 0.0F);
    }

    [[nodiscard]] constexpr Vector3 UnitZ() noexcept
    {
        return Vector3(0.0F, 0.0F, 1.0F);
    }

    [[nodiscard]] constexpr Vector3 Negate(Vector3 value) noexcept
    {
        return Vector3(-value.X, -value.Y, -value.Z);
    }

    [[nodiscard]] constexpr Vector3 Multiply(Vector3 value, float scalar) noexcept
    {
        return Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
    }

    [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
    {
        return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
    }

    [[nodiscard]] constexpr Matrix3 IdentityMatrix3() noexcept
    {
        return Matrix3(
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F);
    }

    [[nodiscard]] constexpr Matrix4 IdentityMatrix4() noexcept
    {
        return Matrix4(
            Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] constexpr Matrix4 ClearTranslation(Matrix4 value) noexcept
    {
        value.M41 = 0.0F;
        value.M42 = 0.0F;
        value.M43 = 0.0F;
        return value;
    }

    void SetTranslation(Matrix4& value, Vector3 position) noexcept
    {
        value.M41 = position.X;
        value.M42 = position.Y;
        value.M43 = position.Z;
    }

    [[nodiscard]] constexpr Matrix4 CreateTranslation(Vector3 position) noexcept
    {
        return Matrix4(
            Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(position, 1.0F));
    }

    [[nodiscard]] constexpr Matrix4 CreateTranslation(
        float x, float y, float z) noexcept
    {
        return CreateTranslation(Vector3(x, y, z));
    }

    [[nodiscard]] constexpr Matrix4 CreateScale(
        float x, float y, float z) noexcept
    {
        return Matrix4(
            Vector4(x, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, y, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, z, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] constexpr Matrix4 CreateScale(float value) noexcept
    {
        return CreateScale(value, value, value);
    }

    [[nodiscard]] Matrix4 CreateRotationZ(float angle)
    {
        float sin = std::sin(angle);
        float cos = std::cos(angle);
        return Matrix4(
            Vector4(cos, sin, 0.0F, 0.0F),
            Vector4(-sin, cos, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] constexpr Matrix4 Multiply(Matrix4 left, Matrix4 right) noexcept
    {
        Matrix4 result{};

        result.M11 = left.M11 * right.M11 + left.M12 * right.M21
            + left.M13 * right.M31 + left.M14 * right.M41;
        result.M12 = left.M11 * right.M12 + left.M12 * right.M22
            + left.M13 * right.M32 + left.M14 * right.M42;
        result.M13 = left.M11 * right.M13 + left.M12 * right.M23
            + left.M13 * right.M33 + left.M14 * right.M43;
        result.M14 = left.M11 * right.M14 + left.M12 * right.M24
            + left.M13 * right.M34 + left.M14 * right.M44;

        result.M21 = left.M21 * right.M11 + left.M22 * right.M21
            + left.M23 * right.M31 + left.M24 * right.M41;
        result.M22 = left.M21 * right.M12 + left.M22 * right.M22
            + left.M23 * right.M32 + left.M24 * right.M42;
        result.M23 = left.M21 * right.M13 + left.M22 * right.M23
            + left.M23 * right.M33 + left.M24 * right.M43;
        result.M24 = left.M21 * right.M14 + left.M22 * right.M24
            + left.M23 * right.M34 + left.M24 * right.M44;

        result.M31 = left.M31 * right.M11 + left.M32 * right.M21
            + left.M33 * right.M31 + left.M34 * right.M41;
        result.M32 = left.M31 * right.M12 + left.M32 * right.M22
            + left.M33 * right.M32 + left.M34 * right.M42;
        result.M33 = left.M31 * right.M13 + left.M32 * right.M23
            + left.M33 * right.M33 + left.M34 * right.M43;
        result.M34 = left.M31 * right.M14 + left.M32 * right.M24
            + left.M33 * right.M34 + left.M34 * right.M44;

        result.M41 = left.M41 * right.M11 + left.M42 * right.M21
            + left.M43 * right.M31 + left.M44 * right.M41;
        result.M42 = left.M41 * right.M12 + left.M42 * right.M22
            + left.M43 * right.M32 + left.M44 * right.M42;
        result.M43 = left.M41 * right.M13 + left.M42 * right.M23
            + left.M43 * right.M33 + left.M44 * right.M43;
        result.M44 = left.M41 * right.M14 + left.M42 * right.M24
            + left.M43 * right.M34 + left.M44 * right.M44;

        return result;
    }

    [[nodiscard]] constexpr float DegreesToRadians(float degrees) noexcept
    {
        return degrees * (3.14159265358979323846F / 180.0F);
    }

    [[nodiscard]] constexpr std::int32_t WrapAddInt32(
        std::int32_t value,
        std::uint32_t addend) noexcept
    {
        return std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(value) + addend);
    }

    [[nodiscard]] constexpr std::int32_t WrapSubtractInt32(
        std::int32_t value,
        std::uint32_t subtrahend) noexcept
    {
        return std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(value) - subtrahend);
    }

    [[nodiscard]] std::shared_ptr<ManagedArray<Vector3>> RentEightVector3()
    {
        return std::make_shared<ManagedArray<Vector3>>(8);
    }

    [[noreturn]] void ThrowNotImplemented()
    {
        throw std::logic_error("The method or operation is not implemented.");
    }
}

namespace MphRead::Effects
{
    TimeValues& TimeValues::operator=(const TimeValues& other) noexcept
    {
        if (this != &other)
        {
            this->~TimeValues();
            ::new (static_cast<void*>(this)) TimeValues(other);
        }
        return *this;
    }

    bool SingleParticle::ShouldDraw() const noexcept
    {
        return _shouldDraw;
    }

    Vector2 SingleParticle::Texcoord0() const noexcept
    {
        return _texcoord0;
    }

    Vector3 SingleParticle::Vertex0() const noexcept
    {
        return _vertex0;
    }

    Vector2 SingleParticle::Texcoord1() const noexcept
    {
        return _texcoord1;
    }

    Vector3 SingleParticle::Vertex1() const noexcept
    {
        return _vertex1;
    }

    Vector2 SingleParticle::Texcoord2() const noexcept
    {
        return _texcoord2;
    }

    Vector3 SingleParticle::Vertex2() const noexcept
    {
        return _vertex2;
    }

    Vector2 SingleParticle::Texcoord3() const noexcept
    {
        return _texcoord3;
    }

    Vector3 SingleParticle::Vertex3() const noexcept
    {
        return _vertex3;
    }

    void SingleParticle::Process()
    {
        _shouldDraw = false;
        if (Alpha > 0.0F)
        {
            _shouldDraw = true;
            _vertex0 = Vector3(-Scale, Scale, 0.0F);
            _texcoord0 = Vector2(0.0F, 0.0F);
            _vertex1 = Vector3(Scale, Scale, 0.0F);
            _texcoord1 = Vector2(1.0F, 0.0F);
            _vertex2 = Vector3(Scale, -Scale, 0.0F);
            _texcoord2 = Vector2(1.0F, 1.0F);
            _vertex3 = Vector3(-Scale, -Scale, 0.0F);
            _texcoord3 = Vector2(0.0F, 1.0F);
        }
    }

    void SingleParticle::AddRenderItem(Scene* scene)
    {
        auto uvsAndVerts = RentEightVector3();
        (*uvsAndVerts)[0] = Vector3(_texcoord0.X, _texcoord0.Y, 0.0F);
        (*uvsAndVerts)[1] = _vertex0;
        (*uvsAndVerts)[2] = Vector3(_texcoord1.X, _texcoord1.Y, 0.0F);
        (*uvsAndVerts)[3] = _vertex1;
        (*uvsAndVerts)[4] = Vector3(_texcoord2.X, _texcoord2.Y, 0.0F);
        (*uvsAndVerts)[5] = _vertex2;
        (*uvsAndVerts)[6] = Vector3(_texcoord3.X, _texcoord3.Y, 0.0F);
        (*uvsAndVerts)[7] = _vertex3;

        Particle& particleForModel = Require(ParticleDefinition);
        Model& materialModel = Require(particleForModel.Model);
        Particle& particleForMaterialId = Require(ParticleDefinition);
        Material& material = materialModel.Materials.at(
            static_cast<std::size_t>(particleForMaterialId.MaterialId));

        // C# evaluates the instance expression, then arguments, and only then
        // performs the instance call. Preserve that order without dereferencing
        // scene before the ParticleDefinition.Model argument is re-evaluated.
        Scene* sceneTarget = scene;
        Particle& particleForBinding = Require(ParticleDefinition);
        Model& bindingModel = Require(particleForBinding.Model);
        std::int32_t textureId = material.TextureId;
        std::int32_t paletteId = material.PaletteId;
        Scene& sceneRef = Require(sceneTarget);
        std::int32_t bindingId = sceneRef.BindGetTexture(
            bindingModel, textureId, paletteId, 0);
        RepeatMode xRepeat = material.XRepeat;
        RepeatMode yRepeat = material.YRepeat;
        float scaleS = 1.0F;
        float scaleT = 1.0F;
        if (xRepeat == RepeatMode::Mirror)
        {
            scaleS = material.ScaleS;
        }
        if (yRepeat == RepeatMode::Mirror)
        {
            scaleT = material.ScaleT;
        }
        Matrix4 transform = CreateTranslation(Position);
        sceneRef.AddRenderItem(
            MphRead::RenderItemType::Particle,
            Alpha,
            sceneRef.GetNextPolygonId(),
            Color,
            xRepeat,
            yRepeat,
            scaleS,
            scaleT,
            transform,
            uvsAndVerts,
            bindingId,
            BillboardMode::Sphere);
    }

    std::shared_ptr<const EffectActionDictionary> EffectFuncBase::Actions() const
    {
        return _actions;
    }

    void EffectFuncBase::SetActions(
        std::shared_ptr<const EffectActionDictionary> value)
    {
        _actions = std::move(value);
    }

    std::shared_ptr<const EffectFuncDictionary> EffectFuncBase::Funcs() const
    {
        return _funcs;
    }

    void EffectFuncBase::SetFuncs(
        std::shared_ptr<const EffectFuncDictionary> value)
    {
        _funcs = std::move(value);
    }

    void EffectFuncBase::FxFunc04(
        const std::vector<std::int32_t>& param,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Fixed::ToFloat(param.at(0));
        vec.Y = Fixed::ToFloat(param.at(1));
        vec.Z = Fixed::ToFloat(param.at(2));
    }

    void EffectFuncBase::FxFunc05(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Fixed::ToFloat(Rng::GetRandomInt1(4096));
        vec.Y = Fixed::ToFloat(Rng::GetRandomInt1(4096));
        vec.Z = Fixed::ToFloat(Rng::GetRandomInt1(4096));
    }

    void EffectFuncBase::FxFunc06(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Fixed::ToFloat(Rng::GetRandomInt1(4096));
        vec.Y = 0.0F;
        vec.Z = Fixed::ToFloat(Rng::GetRandomInt1(4096));
    }

    void EffectFuncBase::FxFunc07(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Fixed::ToFloat(Rng::GetRandomInt1(4096));
        vec.Y = 1.0F;
        vec.Z = Fixed::ToFloat(Rng::GetRandomInt1(4096));
    }

    void EffectFuncBase::FxFunc08(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F;
        vec.Y = Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F;
        vec.Z = Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F;
    }

    void EffectFuncBase::FxFunc09(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F;
        vec.Y = 0.0F;
        vec.Z = Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F;
    }

    void EffectFuncBase::FxFunc10(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F;
        vec.Y = 1.0F;
        vec.Z = Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F;
    }

    void EffectFuncBase::FxFunc13(
        const std::vector<std::int32_t>& param,
        TimeValues times,
        Vector3& vec)
    {
        std::uint32_t offset = static_cast<std::uint32_t>(param.at(1));
        while (true)
        {
            std::shared_ptr<const EffectFuncDictionary> funcs = Funcs();
            const EffectFuncDictionary& dictionary = Require(funcs);
            if (dictionary.find(offset) != dictionary.end())
            {
                break;
            }
            offset += 4U;
        }
        const FxFuncInfo& info = FuncAt(Funcs(), offset);
        float value = InvokeFloatFunc(
            static_cast<std::uint32_t>(param.at(0)),
            ParametersOf(info),
            times);
        float percent = times.Elapsed / value;
        if (value < 0.0F)
        {
            percent *= -1.0F;
        }
        float angle = DegreesToRadians(360.0F * percent);
        vec.X = std::sin(angle);
        vec.Y = 0.0F;
        vec.Z = std::cos(angle);
    }

    void EffectFuncBase::FxFunc14(
        const std::vector<std::int32_t>& param,
        TimeValues times,
        Vector3& vec)
    {
        Vector3 temp = Vector3::Zero;
        InvokeVecFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times,
            temp);
        float value = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times);
        float div = times.Elapsed / value;
        if (value < 0.0F)
        {
            div *= -1.0F;
        }
        vec.X = temp.X * div;
        vec.Y = temp.Y * div;
        vec.Z = temp.Z * div;
    }

    void EffectFuncBase::FxFunc15(
        const std::vector<std::int32_t>& param,
        TimeValues times,
        Vector3& vec)
    {
        float value1 = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times);
        float value2 = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times);
        float angle = DegreesToRadians(
            static_cast<float>(Rng::GetRandomInt1(0xFFFF) >> 4)
            * (360.0F / 4096.0F));
        vec.X = std::sin(angle) * value1;
        vec.Y = value2;
        vec.Z = std::cos(angle) * value1;
    }

    void EffectFuncBase::FxFunc16(
        const std::vector<std::int32_t>& param,
        TimeValues times,
        Vector3& vec)
    {
        float value1 = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times);
        float value2 = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times);
        vec.X = (Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F) * value1;
        vec.Y = 0.0F;
        vec.Z = (Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F) * value2;
    }

    void EffectFuncBase::FxFunc17(
        const std::vector<std::int32_t>& param,
        TimeValues times,
        Vector3& vec)
    {
        Vector3 temp1 = Vector3::Zero;
        Vector3 temp2 = Vector3::Zero;
        InvokeVecFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times,
            temp1);
        InvokeVecFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times,
            temp2);
        vec.X = temp1.X + temp2.X;
        vec.Y = temp1.Y + temp2.Y;
        vec.Z = temp1.Z + temp2.Z;
    }

    void EffectFuncBase::FxFunc18(
        const std::vector<std::int32_t>& param,
        TimeValues times,
        Vector3& vec)
    {
        Vector3 temp1 = Vector3::Zero;
        Vector3 temp2 = Vector3::Zero;
        InvokeVecFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times,
            temp1);
        InvokeVecFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times,
            temp2);
        vec.X = temp1.X - temp2.X;
        vec.Y = temp1.Y - temp2.Y;
        vec.Z = temp1.Z - temp2.Z;
    }

    void EffectFuncBase::FxFunc19(
        const std::vector<std::int32_t>& param,
        TimeValues times,
        Vector3& vec)
    {
        Vector3 temp1 = Vector3::Zero;
        Vector3 temp2 = Vector3::Zero;
        InvokeVecFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times,
            temp1);
        InvokeVecFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times,
            temp2);
        vec.X = temp1.X * temp2.X;
        vec.Y = temp1.Y * temp2.Y;
        vec.Z = temp1.Z * temp2.Z;
    }

    void EffectFuncBase::FxFunc20(
        const std::vector<std::int32_t>& param,
        TimeValues times,
        Vector3& vec)
    {
        Vector3 temp = Vector3::Zero;
        float value = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times);
        InvokeVecFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times,
            temp);
        vec.X = temp.X * value;
        vec.Y = temp.Y * value;
        vec.Z = temp.Z * value;
    }

    float EffectFuncBase::FxFunc21(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return 0.0F;
    }

    float EffectFuncBase::FxFunc40(
        const std::vector<std::int32_t>& param,
        TimeValues times)
    {
        if (times.Elapsed / times.Lifespan <= Fixed::ToFloat(param.at(0)))
        {
            return Fixed::ToFloat(param.at(1));
        }
        return Fixed::ToFloat(param.at(2));
    }

    float EffectFuncBase::FxFunc41(
        const std::vector<std::int32_t>& param,
        TimeValues times)
    {
        float percent = times.Elapsed / times.Lifespan;
        if (percent < Fixed::ToFloat(param.at(0)))
        {
            return Fixed::ToFloat(param.at(1));
        }
        bool none = true;
        std::int32_t i = 0;
        do
        {
            if (Fixed::ToFloat(param.at(static_cast<std::size_t>(i))) > percent)
            {
                break;
            }
            none = false;
            i = WrapAddInt32(i, 2U);
        }
        while (param.at(static_cast<std::size_t>(i))
            != std::numeric_limits<std::int32_t>::min());
        if (none)
        {
            return 0.0F;
        }
        i = WrapSubtractInt32(i, 2U);
        if (param.at(static_cast<std::size_t>(WrapAddInt32(i, 2U)))
            == std::numeric_limits<std::int32_t>::min())
        {
            return Fixed::ToFloat(param.at(static_cast<std::size_t>(WrapAddInt32(i, 1U))));
        }
        return Fixed::ToFloat(param.at(static_cast<std::size_t>(WrapAddInt32(i, 1U))))
            + (Fixed::ToFloat(param.at(static_cast<std::size_t>(WrapAddInt32(i, 3U))))
                - Fixed::ToFloat(param.at(static_cast<std::size_t>(WrapAddInt32(i, 1U)))))
            * ((percent
                    - Fixed::ToFloat(param.at(static_cast<std::size_t>(i))))
                / (Fixed::ToFloat(param.at(static_cast<std::size_t>(WrapAddInt32(i, 2U))))
                    - Fixed::ToFloat(param.at(static_cast<std::size_t>(i)))));
    }

    float EffectFuncBase::FxFunc42(
        const std::vector<std::int32_t>& param,
        TimeValues)
    {
        return Fixed::ToFloat(param.at(0));
    }

    float EffectFuncBase::FxFunc43(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Fixed::ToFloat(Rng::GetRandomInt1(4096));
    }

    float EffectFuncBase::FxFunc44(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Fixed::ToFloat(Rng::GetRandomInt1(4096)) - 0.5F;
    }

    float EffectFuncBase::FxFunc45(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Fixed::ToFloat(Rng::GetRandomInt1(0x168000));
    }

    float EffectFuncBase::FxFunc46(
        const std::vector<std::int32_t>& param,
        TimeValues times)
    {
        float left = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times);
        float right = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times);
        return left + right;
    }

    float EffectFuncBase::FxFunc47(
        const std::vector<std::int32_t>& param,
        TimeValues times)
    {
        float left = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times);
        float right = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times);
        return left - right;
    }

    float EffectFuncBase::FxFunc48(
        const std::vector<std::int32_t>& param,
        TimeValues times)
    {
        float left = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times);
        float right = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times);
        return left * right;
    }

    float EffectFuncBase::FxFunc49(
        const std::vector<std::int32_t>& param,
        TimeValues times)
    {
        float left = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(0))),
            times);
        float right = InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(1))),
            times);
        if (left >= right)
        {
            return InvokeFloatFunc(
                FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(2))),
                times);
        }
        return InvokeFloatFunc(
            FuncAt(Funcs(), static_cast<std::uint32_t>(param.at(3))),
            times);
    }

    void EffectFuncBase::InvokeVecFunc(
        const FxFuncInfo& info,
        TimeValues times,
        Vector3& vec)
    {
        const std::vector<std::int32_t>& parameters = ParametersOf(info);
        switch (info.FuncId)
        {
        case 1:
        case 2:
            FxFunc01(parameters, times, vec);
            break;
        case 3:
            FxFunc03(parameters, times, vec);
            break;
        case 4:
            FxFunc04(parameters, times, vec);
            break;
        case 5:
            FxFunc05(parameters, times, vec);
            break;
        case 6:
            FxFunc06(parameters, times, vec);
            break;
        case 7:
            FxFunc07(parameters, times, vec);
            break;
        case 8:
            FxFunc08(parameters, times, vec);
            break;
        case 9:
            FxFunc09(parameters, times, vec);
            break;
        case 10:
            FxFunc10(parameters, times, vec);
            break;
        case 11:
            FxFunc11(parameters, times, vec);
            break;
        case 13:
            FxFunc13(parameters, times, vec);
            break;
        case 14:
            FxFunc14(parameters, times, vec);
            break;
        case 15:
            FxFunc15(parameters, times, vec);
            break;
        case 16:
            FxFunc16(parameters, times, vec);
            break;
        case 17:
            FxFunc17(parameters, times, vec);
            break;
        case 18:
            FxFunc18(parameters, times, vec);
            break;
        case 19:
            FxFunc19(parameters, times, vec);
            break;
        case 20:
            FxFunc20(parameters, times, vec);
            break;
        default:
            throw MphRead::ProgramException("Invalid effect func.");
        }
    }

    float EffectFuncBase::InvokeFloatFunc(
        const FxFuncInfo& info,
        TimeValues times)
    {
        return InvokeFloatFunc(info.FuncId, ParametersOf(info), times);
    }

    float EffectFuncBase::InvokeFloatFunc(
        std::uint32_t funcId,
        const std::vector<std::int32_t>& parameters,
        TimeValues times)
    {
        switch (funcId)
        {
        case 28:
        case 21:
            return FxFunc21(parameters, times);
        case 22:
            return FxFunc22(parameters, times);
        case 23:
            return FxFunc23(parameters, times);
        case 24:
            return FxFunc24(parameters, times);
        case 25:
            return FxFunc25(parameters, times);
        case 26:
            return FxFunc26(parameters, times);
        case 27:
            return FxFunc27(parameters, times);
        case 29:
            return FxFunc29(parameters, times);
        case 30:
            return FxFunc30(parameters, times);
        case 31:
            return FxFunc31(parameters, times);
        case 32:
            return FxFunc32(parameters, times);
        case 33:
            return FxFunc33(parameters, times);
        case 34:
            return FxFunc34(parameters, times);
        case 35:
            return FxFunc35(parameters, times);
        case 36:
            return FxFunc36(parameters, times);
        case 37:
            return FxFunc37(parameters, times);
        case 38:
            return FxFunc38(parameters, times);
        case 39:
            return FxFunc39(parameters, times);
        case 40:
            return FxFunc40(parameters, times);
        case 41:
            return FxFunc41(parameters, times);
        case 42:
            return FxFunc42(parameters, times);
        case 43:
            return FxFunc43(parameters, times);
        case 44:
            return FxFunc44(parameters, times);
        case 45:
            return FxFunc45(parameters, times);
        case 46:
            return FxFunc46(parameters, times);
        case 47:
            return FxFunc47(parameters, times);
        case 48:
            return FxFunc48(parameters, times);
        case 49:
            return FxFunc49(parameters, times);
        default:
            throw MphRead::ProgramException("Invalid effect func.");
        }
    }

    std::pair<std::int32_t, std::int32_t> EffectFuncBase::GetFuncIds(
        EffElemFlags flags,
        std::int32_t drawType)
    {
        std::int32_t drawId;
        std::int32_t setVecsId;
        if (HasFlag(flags, EffElemFlags::UseMesh))
        {
            drawId = 7;
            setVecsId = drawType == 3 ? 4 : 5;
        }
        else
        {
            bool alternate = HasFlag(flags, EffElemFlags::UseTransform);
            switch (drawType)
            {
            case 1:
                setVecsId = 1;
                drawId = alternate ? 1 : 2;
                break;
            case 2:
                setVecsId = 2;
                drawId = alternate ? 1 : 2;
                break;
            case 3:
                setVecsId = 3;
                drawId = 3;
                break;
            case 4:
                setVecsId = 1;
                drawId = alternate ? 4 : 5;
                break;
            case 5:
                setVecsId = 2;
                drawId = alternate ? 4 : 5;
                break;
            case 6:
                setVecsId = 3;
                drawId = 6;
                break;
            default:
                throw MphRead::ProgramException("Invalid draw type.");
            }
        }
        return std::make_pair(setVecsId, drawId);
    }

    bool EffectEntry::IsFinished() const
    {
        for (std::size_t i = 0; i < Elements->size(); ++i)
        {
            EffectElementEntry& element = Require(Elements->at(i));
            if (!element.Expired || !element.Particles->empty())
            {
                return false;
            }
        }
        return true;
    }

    void EffectEntry::Transform(
        Vector3 facing,
        Vector3 up,
        Vector3 position)
    {
        Matrix4 transform
            = MphRead::Entities::EntityBase::GetTransformMatrix(facing, up);
        Transform(position, transform);
    }

    void EffectEntry::Transform(
        Vector3 position,
        Matrix4 transform)
    {
        SetTranslation(transform, position);
        for (std::size_t i = 0; i < Elements->size(); ++i)
        {
            Require(Elements->at(i)).OwnTransform = transform;
        }
    }

    void EffectEntry::SetElementExtension(bool set)
    {
        if (set)
        {
            SetElementExtension();
        }
        else
        {
            ClearElementExtension();
        }
    }

    void EffectEntry::SetDrawEnabled(bool set)
    {
        if (set)
        {
            SetDrawEnabled();
        }
        else
        {
            ClearDrawEnabled();
        }
    }

    void EffectEntry::SetElementExtension()
    {
        for (std::size_t i = 0; i < Elements->size(); ++i)
        {
            Require(Elements->at(i)).Flags |= EffElemFlags::ElementExtension;
        }
    }

    void EffectEntry::ClearElementExtension()
    {
        for (std::size_t i = 0; i < Elements->size(); ++i)
        {
            Require(Elements->at(i)).Flags &= ~EffElemFlags::ElementExtension;
        }
    }

    void EffectEntry::SetDrawEnabled()
    {
        for (std::size_t i = 0; i < Elements->size(); ++i)
        {
            Require(Elements->at(i)).Flags |= EffElemFlags::DrawEnabled;
        }
    }

    void EffectEntry::ClearDrawEnabled()
    {
        for (std::size_t i = 0; i < Elements->size(); ++i)
        {
            Require(Elements->at(i)).Flags &= ~EffElemFlags::DrawEnabled;
        }
    }

    void EffectEntry::SetReadOnlyField(std::int32_t index, float value)
    {
        if (index == 0)
        {
            for (std::size_t i = 0; i < Elements->size(); ++i)
            {
                Require(Elements->at(i)).RoField1 = value;
            }
        }
        else if (index == 1)
        {
            for (std::size_t i = 0; i < Elements->size(); ++i)
            {
                Require(Elements->at(i)).RoField2 = value;
            }
        }
        else if (index == 2)
        {
            for (std::size_t i = 0; i < Elements->size(); ++i)
            {
                Require(Elements->at(i)).RoField3 = value;
            }
        }
        else if (index == 3)
        {
            for (std::size_t i = 0; i < Elements->size(); ++i)
            {
                Require(Elements->at(i)).RoField4 = value;
            }
        }
    }

    void EffectEntry::ResetElements(float elapsedTime)
    {
        for (std::size_t i = 0; i < Elements->size(); ++i)
        {
            EffectElementEntry& element = Require(Elements->at(i));
            element.Expired = false;
            element.Func39Called = false;
            element.CreationTime = elapsedTime;
            element.ExpirationTime = element.CreationTime + element.Lifespan;
        }
    }

    void EffectElementEntry::FxFunc01(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Transform.M41;
        vec.Y = Transform.M42;
        vec.Z = Transform.M43;
    }

    void EffectElementEntry::FxFunc03(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3&)
    {
        ThrowNotImplemented();
    }

    void EffectElementEntry::FxFunc11(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3&)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc22(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Lifespan;
    }

    float EffectElementEntry::FxFunc23(
        const std::vector<std::int32_t>&,
        TimeValues times)
    {
        return times.Global - CreationTime;
    }

    float EffectElementEntry::FxFunc24(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc25(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc26(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc27(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc29(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc30(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc31(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc32(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc33(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc34(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc35(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc36(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc37(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc38(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        ThrowNotImplemented();
    }

    float EffectElementEntry::FxFunc39(
        const std::vector<std::int32_t>& param,
        TimeValues)
    {
        if (Func39Called)
        {
            return 0.0F;
        }
        Func39Called = true;
        return Fixed::ToFloat(param.at(0));
    }

    Vector3 EffectParticle::EffectVec1() const noexcept
    {
        return _effectVec1;
    }

    Vector3 EffectParticle::EffectVec2() const noexcept
    {
        return _effectVec2;
    }

    Vector3 EffectParticle::EffectVec3() const noexcept
    {
        return _effectVec3;
    }

    bool EffectParticle::ShouldDraw() const noexcept
    {
        return _shouldDraw;
    }

    Vector3 EffectParticle::Color() const noexcept
    {
        return _color;
    }

    Vector2 EffectParticle::Texcoord0() const noexcept
    {
        return _texcoord0;
    }

    Vector3 EffectParticle::Vertex0() const noexcept
    {
        return _vertex0;
    }

    Vector2 EffectParticle::Texcoord1() const noexcept
    {
        return _texcoord1;
    }

    Vector3 EffectParticle::Vertex1() const noexcept
    {
        return _vertex1;
    }

    Vector2 EffectParticle::Texcoord2() const noexcept
    {
        return _texcoord2;
    }

    Vector3 EffectParticle::Vertex2() const noexcept
    {
        return _vertex2;
    }

    Vector2 EffectParticle::Texcoord3() const noexcept
    {
        return _texcoord3;
    }

    Vector3 EffectParticle::Vertex3() const noexcept
    {
        return _vertex3;
    }

    bool EffectParticle::DrawNode() const noexcept
    {
        return _drawNode;
    }

    MphRead::BillboardMode EffectParticle::BillboardMode() const noexcept
    {
        return _billboardMode;
    }

    Matrix4 EffectParticle::NodeTransform() const noexcept
    {
        return _nodeTransform;
    }

    std::shared_ptr<const EffectFuncDictionary> EffectParticle::Funcs() const
    {
        return Require(Owner).Funcs();
    }

    void EffectParticle::SetFuncs(
        std::shared_ptr<const EffectFuncDictionary> value)
    {
        Require(Owner).SetFuncs(std::move(value));
    }

    std::shared_ptr<const EffectActionDictionary> EffectParticle::Actions() const
    {
        return Require(Owner).Actions();
    }

    void EffectParticle::SetActions(
        std::shared_ptr<const EffectActionDictionary> value)
    {
        Require(Owner).SetActions(std::move(value));
    }

    void EffectParticle::FxFunc01(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Position.X;
        vec.Y = Position.Y;
        vec.Z = Position.Z;
    }

    void EffectParticle::FxFunc03(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        vec.X = Speed.X;
        vec.Y = Speed.Y;
        vec.Z = Speed.Z;
    }

    void EffectParticle::FxFunc11(
        const std::vector<std::int32_t>&,
        TimeValues,
        Vector3& vec)
    {
        float angle = DegreesToRadians(360.0F * PortionTotal);
        vec.X = std::sin(angle);
        vec.Y = 0.0F;
        vec.Z = std::cos(angle);
    }

    float EffectParticle::FxFunc22(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Require(Owner).Lifespan;
    }

    float EffectParticle::FxFunc23(
        const std::vector<std::int32_t>&,
        TimeValues times)
    {
        return times.Global - Require(Owner).CreationTime;
    }

    float EffectParticle::FxFunc24(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Alpha;
    }

    float EffectParticle::FxFunc25(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Red;
    }

    float EffectParticle::FxFunc26(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Green;
    }

    float EffectParticle::FxFunc27(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Blue;
    }

    float EffectParticle::FxFunc29(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Scale;
    }

    float EffectParticle::FxFunc30(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return Rotation;
    }

    float EffectParticle::FxFunc31(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return RoField1;
    }

    float EffectParticle::FxFunc32(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return RoField2;
    }

    float EffectParticle::FxFunc33(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return RoField3;
    }

    float EffectParticle::FxFunc34(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return RoField4;
    }

    float EffectParticle::FxFunc35(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return RwField1;
    }

    float EffectParticle::FxFunc36(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return RwField2;
    }

    float EffectParticle::FxFunc37(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return RwField3;
    }

    float EffectParticle::FxFunc38(
        const std::vector<std::int32_t>&,
        TimeValues)
    {
        return RwField4;
    }

    float EffectParticle::FxFunc39(
        const std::vector<std::int32_t>& param,
        TimeValues)
    {
        EffectElementEntry& owner = Require(Owner);
        if (owner.Func39Called)
        {
            return 0.0F;
        }
        owner.Func39Called = true;
        return Fixed::ToFloat(param.at(0));
    }

    void EffectParticle::SetVecsB0()
    {
        _effectVec1 = UnitX();
        _effectVec2 = Negate(UnitY());
        _billboardMode = BillboardMode::Sphere;
    }

    void EffectParticle::SetVecsBC()
    {
        _effectVec1 = UnitX();
        _effectVec2 = UnitZ();
    }

    void EffectParticle::SetVecsC0(Matrix4 viewMatrix)
    {
        Matrix3 identity = IdentityMatrix3();
        Vector3 vec1(-identity.M12, -identity.M22, -identity.M32);
        Vector3 vec2(viewMatrix.M13, viewMatrix.M23, viewMatrix.M33);
        _effectVec1 = vec1;
        _effectVec2 = vec2;
    }

    void EffectParticle::SetVecsD4()
    {
        assert(false && "SetVecsD4 was called");
        Matrix4 viewMatrix = IdentityMatrix4();
        Vector3 vec1 = Speed.Normalized();
        Vector3 vec2(viewMatrix.M13, viewMatrix.M23, viewMatrix.M33);
        Vector3 vec3 = Vector3::Cross(vec2, vec1);
        if (LengthSquared(vec3) < Fixed::ToFloat(64))
        {
            vec2 = Vector3(viewMatrix.M11, viewMatrix.M21, viewMatrix.M31);
            vec3 = Vector3::Cross(vec2, vec1);
        }
        vec3 = vec3.Normalized();
        vec1 = Vector3::Cross(vec3, vec2);
        _effectVec1 = vec1;
        _effectVec2 = vec2;
        _effectVec3 = vec3;
    }

    void EffectParticle::SetVecsD8()
    {
        _billboardMode = BillboardMode::Sphere;
    }

    void EffectParticle::InvokeSetVecsFunc(Matrix4 viewMatrix)
    {
        _billboardMode = BillboardMode::None;
        switch (SetVecsId)
        {
        case 1:
            SetVecsB0();
            break;
        case 2:
            SetVecsBC();
            break;
        case 3:
            SetVecsC0(viewMatrix);
            break;
        case 4:
            SetVecsD4();
            break;
        case 5:
            SetVecsD8();
            break;
        default:
            throw MphRead::ProgramException("Invalid set vecs func.");
        }
    }

    void EffectParticle::DrawB8(float scaleFactor)
    {
        if (Alpha > 0.0F)
        {
            _shouldDraw = true;
            _color = Vector3(Red, Green, Blue);
            Vector3 ev1 = Multiply(_effectVec1, Scale);
            Vector3 ev2 = Multiply(_effectVec2, Scale);

            EffectElementEntry& owner = Require(Owner);
            Vector3 position = MphRead::Matrix::Vec3MultMtx4(
                Position, ClearTranslation(owner.Transform));
            float v19 = position.X + (-ev1.X / 2.0F) + (ev2.X / 2.0F);
            float v22 = position.Y + (-ev1.Y / 2.0F) + (ev2.Y / 2.0F);
            float v23 = position.Z + (-ev1.Z / 2.0F) + (ev2.Z / 2.0F);

            float x = v19 / scaleFactor;
            float y = v22 / scaleFactor;
            float z = v23 / scaleFactor;
            _vertex0 = Vector3(x, y, z) - position;
            _texcoord0 = Vector2(0.0F, 1.0F);

            float v24 = v19 + ev1.X;
            float v26 = v22 + ev1.Y;
            float v29 = v23 + ev1.Z;
            x = v24 / scaleFactor;
            y = v26 / scaleFactor;
            z = v29 / scaleFactor;
            _vertex1 = Vector3(x, y, z) - position;
            _texcoord1 = Vector2(1.0F, 1.0F);

            float v25 = v19 + ev1.X - ev2.X;
            float v27 = v22 + ev1.Y - ev2.Y;
            float v30 = v23 + ev1.Z - ev2.Z;
            x = v25 / scaleFactor;
            y = v27 / scaleFactor;
            z = v30 / scaleFactor;
            _vertex2 = Vector3(x, y, z) - position;
            _texcoord2 = Vector2(1.0F, 0.0F);

            float v34 = v25 - ev1.X;
            float v28 = v27 - ev1.Y;
            float v33 = v30 - ev1.Z;
            x = v34 / scaleFactor;
            y = v28 / scaleFactor;
            z = v33 / scaleFactor;
            _vertex3 = Vector3(x, y, z) - position;
            _texcoord3 = Vector2(0.0F, 0.0F);
        }
    }

    void EffectParticle::DrawC4(float scaleFactor)
    {
        if (LengthSquared(Speed) > Fixed::ToFloat(128))
        {
            _effectVec1 = Speed.Normalized();
            DrawShared(scaleFactor, true);
        }
    }

    void EffectParticle::DrawCC(float scaleFactor)
    {
        if (Alpha > 0.0F)
        {
            _shouldDraw = true;
            _color = Vector3(Red, Green, Blue);
            float angle1 = DegreesToRadians(Rotation);
            float angle2 = DegreesToRadians(Rotation + 90.0F);
            float sin1 = std::sin(angle1);
            float cos1 = std::cos(angle1);
            float sin2 = std::sin(angle2);
            float cos2 = std::cos(angle2);

            Vector3 vec1 = _effectVec1;
            Vector3 vec2 = _effectVec2;

            float v20 = (vec1.X * sin2 + vec2.X * cos2) * Scale;
            float v24 = (vec1.Y * sin2 + vec2.Y * cos2) * Scale;
            float v25 = (vec1.Z * sin2 + vec2.Z * cos2) * Scale;

            float v26 = (vec1.X * sin1 + vec2.X * cos1) * Scale;
            float v28 = (vec1.Y * sin1 + vec2.Y * cos1) * Scale;
            float v29 = (vec1.Z * sin1 + vec2.Z * cos1) * Scale;

            EffectElementEntry& owner = Require(Owner);
            Vector3 position = MphRead::Matrix::Vec3MultMtx4(
                Position, ClearTranslation(owner.Transform));
            float v27 = position.X + (-v20 / 2.0F) + (v26 / 2.0F);
            float v30 = position.Y + (-v24 / 2.0F) + (v28 / 2.0F);
            float v31 = position.Z + (-v25 / 2.0F) + (v29 / 2.0F);

            float x = v27 / scaleFactor;
            float y = v30 / scaleFactor;
            float z = v31 / scaleFactor;
            _vertex0 = Vector3(x, y, z) - position;
            _texcoord0 = Vector2(0.0F, 1.0F);

            float v39 = v27 + v20;
            float v38 = v30 + v24;
            float v33 = v31 + v25;
            x = v39 / scaleFactor;
            y = v38 / scaleFactor;
            z = v33 / scaleFactor;
            _vertex1 = Vector3(x, y, z) - position;
            _texcoord1 = Vector2(1.0F, 1.0F);

            float v40 = v27 + v20 - v26;
            float v41 = v38 - v28;
            float v35 = v33 - v29;
            x = v40 / scaleFactor;
            y = v41 / scaleFactor;
            z = v35 / scaleFactor;
            _vertex2 = Vector3(x, y, z) - position;
            _texcoord2 = Vector2(1.0F, 0.0F);

            float v42 = v40 - v20;
            float v43 = v41 - v24;
            float v36 = v33 - v29 - v25;
            x = v42 / scaleFactor;
            y = v43 / scaleFactor;
            z = v36 / scaleFactor;
            _vertex3 = Vector3(x, y, z) - position;
            _texcoord3 = Vector2(0.0F, 0.0F);
        }
    }

    void EffectParticle::DrawD0(float scaleFactor)
    {
        DrawShared(scaleFactor, false);
    }

    void EffectParticle::DrawDC(float)
    {
        if (Alpha > 0.0F)
        {
            _shouldDraw = true;
            _drawNode = true;
            _color = Vector3(Red, Green, Blue);
            EffectElementEntry& owner = Require(Owner);
            Vector4 ev4;
            if (HasFlag(owner.Flags, EffElemFlags::UseTransform))
            {
                ev4 = Vector4(
                    Position + Vector3(
                        owner.Transform.M41,
                        owner.Transform.M42,
                        owner.Transform.M43),
                    1.0F);
            }
            else
            {
                ev4 = Vector4(Position, 1.0F);
            }
            if (_billboardMode == BillboardMode::Sphere)
            {
                _nodeTransform = Multiply(
                    CreateScale(Scale),
                    CreateTranslation(Vector3(ev4.X, ev4.Y, ev4.Z)));
            }
            else
            {
                assert(false && "DrawDC was called with non-billboard");
                Vector4 ev1(Multiply(_effectVec1, Scale));
                Vector4 ev2(Multiply(_effectVec2, Scale));
                Vector4 ev3(Multiply(_effectVec3, Scale));
                _nodeTransform = Matrix4(ev1, ev2, ev3, ev4);
            }
        }
    }

    void EffectParticle::DrawShared(
        float scaleFactor,
        bool skipIfZeroSpeed)
    {
        if (Alpha > 0.0F
            && (!skipIfZeroSpeed || LengthSquared(Speed) > 0.0F))
        {
            _shouldDraw = true;
            _color = Vector3(Red, Green, Blue);
            Vector3 cross = Vector3::Cross(_effectVec1, _effectVec2).Normalized();
            Vector3 ev1 = Multiply(_effectVec1, Scale);
            cross = Multiply(cross, Rotation);
            float v20 = -ev1.X / 2.0F + cross.X / 2.0F;
            float v21 = -ev1.Y / 2.0F + cross.Y / 2.0F;
            float v22 = -ev1.Z / 2.0F + cross.Z / 2.0F;

            float x = v20 / scaleFactor;
            float y = v21 / scaleFactor;
            float z = v22 / scaleFactor;
            _vertex0 = Vector3(x, y, z);
            _texcoord0 = Vector2(0.0F, 1.0F);

            float v26 = v20 + ev1.X;
            float v28 = v21 + ev1.Y;
            float v27 = v22 + ev1.Z;
            x = v26 / scaleFactor;
            y = v28 / scaleFactor;
            z = v27 / scaleFactor;
            _vertex1 = Vector3(x, y, z);
            _texcoord1 = Vector2(1.0F, 1.0F);

            float v30 = v26 - cross.X;
            float v32 = v28 - cross.Y;
            float v34 = v27 - cross.Z;
            x = v30 / scaleFactor;
            y = v32 / scaleFactor;
            z = v34 / scaleFactor;
            _vertex2 = Vector3(x, y, z);
            _texcoord2 = Vector2(1.0F, 0.0F);

            float v35 = v30 - ev1.X;
            float v37 = v32 - ev1.Y;
            float v38 = v34 - ev1.Z;
            x = v35 / scaleFactor;
            y = v37 / scaleFactor;
            z = v38 / scaleFactor;
            _vertex3 = Vector3(x, y, z);
            _texcoord3 = Vector2(0.0F, 0.0F);
        }
    }

    void EffectParticle::InvokeDrawFunc(float scaleFactor)
    {
        _shouldDraw = false;
        _drawNode = false;
        switch (DrawId)
        {
        case 1:
        case 2:
            DrawB8(scaleFactor);
            break;
        case 3:
            DrawC4(scaleFactor);
            break;
        case 4:
        case 5:
            DrawCC(scaleFactor);
            break;
        case 6:
            DrawD0(scaleFactor);
            break;
        case 7:
            DrawDC(scaleFactor);
            break;
        default:
            throw MphRead::ProgramException("Invalid draw func.");
        }
    }

    void EffectParticle::SetFuncIds()
    {
        EffectElementEntry& owner = Require(Owner);
        auto ids = GetFuncIds(owner.Flags, owner.DrawType);
        SetVecsId = ids.first;
        DrawId = ids.second;
    }

    void EffectParticle::AddRenderItem(Scene* scene)
    {
        if (_drawNode)
        {
            EffectElementEntry& owner = Require(Owner);
            std::shared_ptr<Model> modelRef = owner.Model;
            Node& node = Require(owner.Nodes->at(
                static_cast<std::size_t>(ParticleId)));
            Model& meshModel = Require(owner.Model);
            auto& mesh = meshModel.Meshes.at(
                static_cast<std::size_t>(node.MeshId / 2));
            Model& materialModel = Require(owner.Model);
            Material& material = materialModel.Materials.at(
                static_cast<std::size_t>(MaterialId));

            Matrix4 transform = _nodeTransform;
            Matrix4 texcoordMtx = IdentityMatrix4();
            if (material.TexgenMode == TexgenMode::Texcoord)
            {
                texcoordMtx = CreateTranslation(
                    material.ScaleS * material.TranslateS,
                    material.ScaleT * material.TranslateT,
                    0.0F);
                texcoordMtx = Multiply(
                    CreateScale(material.ScaleS, material.ScaleT, 1.0F),
                    texcoordMtx);
                texcoordMtx = Multiply(
                    CreateRotationZ(material.RotateZ),
                    texcoordMtx);
            }
            material.CurrentDiffuse = _color;
            material.CurrentAlpha = Alpha;
            Model& model = Require(modelRef);
            assert(model.NodeMatrixIds.empty());
            Scene& sceneRef = Require(scene);
            sceneRef.UpdateMaterials(model, 0);
            sceneRef.AddRenderItem(
                material,
                sceneRef.GetNextPolygonId(),
                1.0F,
                Vector3::Zero,
                MphRead::LightInfo::Zero,
                texcoordMtx,
                transform,
                mesh.ListId,
                0,
                ManagedArray<float>::Empty(),
                std::nullopt,
                std::nullopt,
                SelectionType::None,
                _billboardMode);
        }
        else
        {
            EffectElementEntry& owner = Require(Owner);
            Model& ownerModel = Require(owner.Model);
            if (MaterialId >= static_cast<std::int32_t>(ownerModel.Materials.size()))
            {
                return;
            }

            auto uvsAndVerts = RentEightVector3();
            (*uvsAndVerts)[0] = Vector3(_texcoord0.X, _texcoord0.Y, 0.0F);
            (*uvsAndVerts)[1] = _vertex0;
            (*uvsAndVerts)[2] = Vector3(_texcoord1.X, _texcoord1.Y, 0.0F);
            (*uvsAndVerts)[3] = _vertex1;
            (*uvsAndVerts)[4] = Vector3(_texcoord2.X, _texcoord2.Y, 0.0F);
            (*uvsAndVerts)[5] = _vertex2;
            (*uvsAndVerts)[6] = Vector3(_texcoord3.X, _texcoord3.Y, 0.0F);
            (*uvsAndVerts)[7] = _vertex3;

            Model& materialModel = Require(owner.Model);
            Material& material = materialModel.Materials.at(
                static_cast<std::size_t>(MaterialId));
            std::int32_t bindingId = owner.TextureBindingIds->at(
                static_cast<std::size_t>(ParticleId));
            RepeatMode xRepeat = material.XRepeat;
            RepeatMode yRepeat = material.YRepeat;
            float scaleS = 1.0F;
            float scaleT = 1.0F;
            if (xRepeat == RepeatMode::Mirror)
            {
                scaleS = material.ScaleS;
            }
            if (yRepeat == RepeatMode::Mirror)
            {
                scaleT = material.ScaleT;
            }

            Matrix4 transform;
            if (HasFlag(owner.Flags, EffElemFlags::UseTransform))
            {
                if (_billboardMode != BillboardMode::None)
                {
                    Vector3 position = MphRead::Matrix::Vec3MultMtx4(
                        Position, ClearTranslation(owner.Transform));
                    transform = CreateTranslation(
                        position + Vector3(
                            owner.Transform.M41,
                            owner.Transform.M42,
                            owner.Transform.M43));
                }
                else
                {
                    transform = Multiply(
                        CreateTranslation(Position),
                        owner.Transform);
                }
            }
            else
            {
                transform = CreateTranslation(Position);
            }

            Scene* sceneTarget = scene;
            std::int32_t polygonId = Require(scene).GetNextPolygonId();
            Scene& sceneRef = Require(sceneTarget);
            sceneRef.AddRenderItem(
                MphRead::RenderItemType::Particle,
                Alpha,
                polygonId,
                _color,
                xRepeat,
                yRepeat,
                scaleS,
                scaleT,
                transform,
                uvsAndVerts,
                bindingId,
                _billboardMode);
        }
    }
}
