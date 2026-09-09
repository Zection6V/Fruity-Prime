/*
 * Native counterpart of MphRead/Mods/Render/EsBindings.cs.
 *
 * Keep this loader separate from the renderer.  Android owns the GLES
 * context/thread, while the shared native sources only need a safe way to
 * resolve the entry points once that context exists.
 */
#include "Mods/Render/es_bindings.hpp"

#include <array>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fruityprime::mods::render {
namespace {

struct SharedLibraryNames {
    std::array<const char*, 2> gles;
    std::array<const char*, 2> egl;
};

[[nodiscard]] constexpr SharedLibraryNames library_names() noexcept {
#ifdef _WIN32
    return {{{"libGLESv2.dll", "GLESv2.dll"}},
            {{"libEGL.dll", "EGL.dll"}}};
#else
    return {{{"libGLESv2.so", "libGLESv2.so.3"}},
            {{"libEGL.so", "libEGL.so.1"}}};
#endif
}

[[nodiscard]] void* open_library(const char* name) noexcept {
#ifdef _WIN32
    return reinterpret_cast<void*>(LoadLibraryA(name));
#else
    return dlopen(name, RTLD_NOW | RTLD_LOCAL);
#endif
}

void close_library(void* handle) noexcept {
    if (handle == nullptr) {
        return;
    }
#ifdef _WIN32
    FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

[[nodiscard]] void* exported_symbol(void* library,
                                     std::string_view name) {
    if (library == nullptr || name.empty()) {
        return nullptr;
    }
    const std::string symbol(name);
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(
        reinterpret_cast<HMODULE>(library), symbol.c_str()));
#else
    return dlsym(library, symbol.c_str());
#endif
}

[[nodiscard]] std::string load_error(std::string_view library) {
#ifdef _WIN32
    return "could not load " + std::string(library)
        + " (Windows error " + std::to_string(GetLastError()) + ")";
#else
    const char* error = dlerror();
    return "could not load " + std::string(library)
        + (error == nullptr ? std::string{} : " (" + std::string(error) + ")");
#endif
}

[[nodiscard]] void* try_library(const std::array<const char*, 2>& names,
                                std::string& error) noexcept {
    for (const char* name : names) {
        if (void* handle = open_library(name); handle != nullptr) {
            return handle;
        }
    }
    error = load_error(names[0]);
    return nullptr;
}

} // namespace

EsBindings::~EsBindings() {
    reset();
}

EsBindings& EsBindings::instance() noexcept {
    static EsBindings bindings;
    return bindings;
}

bool EsBindings::load() {
    if (loaded_) {
        return true;
    }

    reset();
    const SharedLibraryNames names = library_names();
    std::string gles_error;
    std::string egl_error;
    gles_ = try_library(names.gles, gles_error);
    egl_ = try_library(names.egl, egl_error);
    if (gles_ == nullptr && egl_ == nullptr) {
        last_error_ = gles_error.empty() ? egl_error
                                         : std::move(gles_error);
        return false;
    }

    // Store the opaque address rather than a platform-specific EGL typedef.
    // get_proc_address() casts it only at the call site on the same platform
    // that provided it.
    egl_get_proc_address_ = exported_symbol(egl_, "eglGetProcAddress");
    loaded_ = true;
    last_error_.clear();
    return true;
}

void EsBindings::reset() noexcept {
    close_library(egl_);
    close_library(gles_);
    egl_ = nullptr;
    gles_ = nullptr;
    egl_get_proc_address_ = nullptr;
    loaded_ = false;
    last_error_.clear();
}

void* EsBindings::get_proc_address(std::string_view name) const {
    if (!loaded_ || name.empty()) {
        return nullptr;
    }
    if (void* address = exported_symbol(gles_, name); address != nullptr) {
        return address;
    }
    if (egl_get_proc_address_ == nullptr) {
        return nullptr;
    }

    using EglGetProcAddress = void* (*)(const char*);
    const auto get_egl_proc = reinterpret_cast<EglGetProcAddress>(
        egl_get_proc_address_);
    const std::string symbol(name);
    return get_egl_proc(symbol.c_str());
}

} // namespace fruityprime::mods::render
