#pragma once

#include "Collada.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace MphRead
{
    class Model;
}

namespace MphRead::Export
{
    class Scripting
    {
    public:
        Scripting() = default;

        static std::string GenerateScript(const Model& model,
            const std::vector<std::pair<std::string, std::vector<Collada::Vertex>>>& lists);

    private:
        static void PrintAnimations(const Model& model, std::string& sb);
    };

    class StringBuilderExtensions final
    {
    public:
        static void AppendIndent(std::string& sb);
        static void AppendIndent(std::string& sb, const std::string& text, std::int32_t indent = 1);

        StringBuilderExtensions() = delete;
    };
}
