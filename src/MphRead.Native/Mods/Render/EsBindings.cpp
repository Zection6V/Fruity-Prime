#include "EsBindings.hpp"

#if defined(__ANDROID__)
#include "../../Program.hpp"

#include <dlfcn.h>

namespace MphRead::Mods::Render
{
    namespace
    {
        using EglProcAddress = void (*)();
        using EglGetProcAddressFunction = EglProcAddress (*)(const char*);
    }

    bool EsBindings::_loaded = false;

    EsBindings::EsBindings(void* gles, void* egl)
        : _gles(gles), _egl(egl)
    {
    }

    void* EsBindings::EglGetProcAddress(const std::string& procName)
    {
        static void* egl = nullptr;
        if (egl == nullptr)
        {
            egl = dlopen("libEGL.so", RTLD_LAZY | RTLD_LOCAL);
            if (egl == nullptr)
            {
                return nullptr;
            }
        }

        dlerror();
        void* entryPoint = dlsym(egl, "eglGetProcAddress");
        if (dlerror() != nullptr)
        {
            return nullptr;
        }

        const auto eglGetProcAddress
            = reinterpret_cast<EglGetProcAddressFunction>(entryPoint);
        EglProcAddress address = eglGetProcAddress(procName.c_str());
        return reinterpret_cast<void*>(address);
    }

    void* EsBindings::GetProcAddress(const std::string& procName)
    {
        if (_gles != nullptr)
        {
            dlerror();
            void* address = dlsym(_gles, procName.c_str());
            if (dlerror() == nullptr)
            {
                return address;
            }
        }
        return EglGetProcAddress(procName);
    }

    void EsBindings::Load()
    {
        if (_loaded)
        {
            return;
        }
        void* gles = TryLoad("libGLESv2.so");
        void* egl = TryLoad("libEGL.so");
        if (gles == nullptr && egl == nullptr)
        {
            throw MphRead::ProgramException(
                "Neither libGLESv2.so nor libEGL.so could be loaded.");
        }
        EsBindings bindings(gles, egl);
        _loaded = true;
    }

    void* EsBindings::TryLoad(const std::string& name)
    {
        return dlopen(name.c_str(), RTLD_LAZY | RTLD_LOCAL);
    }
}
#endif
