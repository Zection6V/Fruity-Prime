#pragma once

#if defined(__ANDROID__)
#include <string>

namespace MphRead::Mods::Render
{
    class EsShaders final
    {
    public:
        static const std::string VertexShader;
        static const std::string FragmentShader;
        static const std::string RttVertexShader;
        static const std::string RttFragmentShader;
        static const std::string CelFragmentShader;
        static const std::string ShiftFragmentShader;

        static const std::string* Translate(const std::string* desktopSource);
        static void CheckInSync();

    private:
        EsShaders() = delete;
        static bool _checked;
        static void Check(const std::string& name, const std::string& source, const std::string& expected);
    };
}
#endif
