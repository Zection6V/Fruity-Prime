#pragma once

#include "../Formats/Types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MphRead
{
    class CollisionVolume;
    class Material;
    class Model;
}

namespace System::Reflection
{
    class PropertyInfo
    {
    public:
        const std::string Name;
        const void* const PropertyType;

        PropertyInfo(std::string name, const void* propertyType)
            : Name(std::move(name)), PropertyType(propertyType)
        {
        }

        PropertyInfo(const PropertyInfo&) = delete;
        PropertyInfo& operator=(const PropertyInfo&) = delete;
        PropertyInfo(PropertyInfo&&) = delete;
        PropertyInfo& operator=(PropertyInfo&&) = delete;
    };
}

namespace System
{
    class IndexOutOfRangeException final : public std::out_of_range
    {
    public:
        IndexOutOfRangeException()
            : std::out_of_range("Index was outside the bounds of the array.")
        {
        }
    };

    class Type
    {
    public:
        using PropertyList = std::vector<std::shared_ptr<Reflection::PropertyInfo>>;

        const std::string Name;
        const std::shared_ptr<const PropertyList> Properties;

        Type(std::string name, std::shared_ptr<const PropertyList> properties)
            : Name(std::move(name)), Properties(std::move(properties))
        {
        }

        Type(const Type&) = delete;
        Type& operator=(const Type&) = delete;
        Type(Type&&) = delete;
        Type& operator=(Type&&) = delete;

        [[nodiscard]] const PropertyList& GetProperties() const;

        template <typename T>
        [[nodiscard]] static const void* Of() noexcept
        {
            static const unsigned char token = 0;
            return &token;
        }
    };
}

namespace MphRead::Testing
{
    class TestPrint
    {
    public:
        TestPrint() = default;
        TestPrint(const TestPrint&) = delete;
        TestPrint& operator=(const TestPrint&) = delete;
        TestPrint(TestPrint&&) = delete;
        TestPrint& operator=(TestPrint&&) = delete;

        static void PrintStruct(const std::optional<std::string>& name, std::int32_t size);

        static void GetPolygonAttrs(
            const std::shared_ptr<MphRead::Model>& model,
            std::int32_t polygonId);

        static void GetPolygonAttrs(
            const std::shared_ptr<MphRead::Model>& model,
            const std::shared_ptr<MphRead::Material>& material,
            std::int32_t polygonId);

        static void DumpPolygonAttr(std::uint32_t attr);

        [[nodiscard]] static OpenTK::Mathematics::Vector3 LightCalc(
            OpenTK::Mathematics::Vector3 light_vec,
            OpenTK::Mathematics::Vector3 light_col,
            OpenTK::Mathematics::Vector3 normal_vec,
            OpenTK::Mathematics::Vector3 dif_col,
            OpenTK::Mathematics::Vector3 amb_col,
            OpenTK::Mathematics::Vector3 spe_col);

        static void PrintEntityEditor(const std::shared_ptr<System::Type>& type);

        static void ParseStruct(
            const std::optional<std::string>& className,
            const std::optional<std::string>& baseClass,
            const std::optional<std::string>& data);

    private:
        static void Nop() noexcept;
    };
}
