#pragma once

#include "../Formats/Types.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace System
{
    class ArgumentException final : public std::invalid_argument
    {
    public:
        explicit ArgumentException(std::string_view message)
            : std::invalid_argument(std::string(message))
        {
        }
    };
}

namespace MphRead::Testing
{
    class TestParse final
    {
    public:
        using StringArray = std::vector<std::optional<std::string>>;

        static void TestMatrix();
        static void TestMatrices();

        [[nodiscard]] static OpenTK::Mathematics::Matrix3 TestVectors(
            OpenTK::Mathematics::Vector3 field58,
            OpenTK::Mathematics::Vector3 field64,
            OpenTK::Mathematics::Vector3 field70);

        [[nodiscard]] static OpenTK::Mathematics::Vector3 ParseVector3(
            const std::optional<std::string>& values);

        [[nodiscard]] static OpenTK::Mathematics::Matrix4x3 ParseMatrix12(
            const std::shared_ptr<StringArray>& values);

        [[nodiscard]] static OpenTK::Mathematics::Matrix4 ParseMatrix16(
            const std::shared_ptr<StringArray>& values);

        [[nodiscard]] static OpenTK::Mathematics::Matrix4 ParseMatrix16(
            const std::optional<std::string>& value);

        [[nodiscard]] static OpenTK::Mathematics::Matrix4x3 ParseMatrix48(
            const std::optional<std::string>& value);

        [[nodiscard]] static OpenTK::Mathematics::Matrix4 ParseMatrix64(
            const std::optional<std::string>& value);

        TestParse() = delete;
        TestParse(const TestParse&) = delete;
        TestParse& operator=(const TestParse&) = delete;
        TestParse(TestParse&&) = delete;
        TestParse& operator=(TestParse&&) = delete;

    private:
        static void Nop() noexcept;
    };
}
