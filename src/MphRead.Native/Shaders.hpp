#pragma once

#include <cstdint>
#include <string>

namespace MphRead
{
    class Shaders final
    {
    public:
        Shaders() = delete;
        Shaders(const Shaders&) = delete;
        Shaders(Shaders&&) = delete;
        Shaders& operator=(const Shaders&) = delete;
        Shaders& operator=(Shaders&&) = delete;

        static const std::string VertexShader;
        static const std::string FragmentShader;
        static const std::string BackdropVertexShader;
        static const std::string BackdropFragmentShader;
        static const std::string RttVertexShader;
        static const std::string RttFragmentShader;
        static const std::string CelFragmentShader;
        static const std::string ShiftFragmentShader;
    };
}
