#pragma once

#include <string>
#include <string_view>

namespace fruityprime::mods::render {

// Native counterpart of MphRead/Mods/Render/EsBindings.cs.  The Android head
// uses the same loader contract as the managed OpenTK binding: first ask the
// GLES shared object for a core symbol, then fall back to EGL for extension
// entry points.  The actual GL function-pointer casts belong to the frontend
// that owns its ES ABI; this class deliberately exposes an opaque address so
// the portable core has no OpenGL or EGL header dependency.
class EsBindings final {
public:
    EsBindings() noexcept = default;
    EsBindings(const EsBindings&) = delete;
    EsBindings& operator=(const EsBindings&) = delete;
    ~EsBindings();

    // Load libGLESv2 and libEGL once.  A process may have only one of the two
    // libraries available, as long as at least one can resolve the requested
    // symbol.  The error is retained for a launcher or Android log to show.
    [[nodiscard]] bool load();
    void reset() noexcept;

    [[nodiscard]] void* get_proc_address(std::string_view name) const;
    [[nodiscard]] bool loaded() const noexcept { return loaded_; }
    [[nodiscard]] std::string_view last_error() const noexcept {
        return last_error_;
    }

    [[nodiscard]] static EsBindings& instance() noexcept;

private:
    void* gles_ = nullptr;
    void* egl_ = nullptr;
    void* egl_get_proc_address_ = nullptr;
    bool loaded_ = false;
    std::string last_error_;
};

} // namespace fruityprime::mods::render
