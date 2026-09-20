#pragma once

#include "../Formats/Types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MphRead
{
    class Material;
    class Model;
}

namespace System
{
    class Type;
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
