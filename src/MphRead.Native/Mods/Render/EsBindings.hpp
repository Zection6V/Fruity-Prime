#pragma once

#if defined(__ANDROID__)
#include <string>

namespace MphRead::Mods::Render
{
    class EsBindings final
    {
    public:
        void* GetProcAddress(const std::string& procName);
        static void Load();

    private:
        EsBindings(void* gles, void* egl);

        static void* EglGetProcAddress(const std::string& procName);
        static void* TryLoad(const std::string& name);

        void* const _gles;
        void* const _egl;
        static bool _loaded;
    };
}
#endif
