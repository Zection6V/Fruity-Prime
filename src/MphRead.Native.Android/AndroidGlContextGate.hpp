#pragma once

#if !defined(__ANDROID__)
#error "AndroidGlContextGate is only valid for the Android native target."
#endif

#include "../MphRead.Native/NativeRuntime/Rhi/SceneBackend.hpp"

#include <mutex>
#include <optional>

namespace MphRead::Droid
{
    // GlEs is process-global while Android EGL contexts are deliberately
    // non-shared. Every context that can drive GlEs must hold this lease from
    // before context creation/reset until after its final GL teardown.
    class AndroidGlContextLease final
    {
    public:
        AndroidGlContextLease();
        ~AndroidGlContextLease() = default;

        void Release() noexcept
        {
            if (_lock.owns_lock())
            {
                _lock.unlock();
            }
        }

        AndroidGlContextLease(const AndroidGlContextLease&) = delete;
        AndroidGlContextLease& operator=(const AndroidGlContextLease&) = delete;
        AndroidGlContextLease(AndroidGlContextLease&&) = delete;
        AndroidGlContextLease& operator=(AndroidGlContextLease&&) = delete;

    private:
        std::unique_lock<std::mutex> _lock;
    };

    // The renderer chosen in the settings. Applied as soon as no scene holds
    // a lease (at once if none does), or by the match on its next frame, so
    // switching needs no restart.
    void RequestAndroidRenderer(NativeRuntime::Rhi::SceneBackendRequest request) noexcept;
    // For the match, which holds the lease for its whole life: the renderer
    // chosen since, if any, for the match to apply (no longer pending).
    [[nodiscard]] std::optional<NativeRuntime::Rhi::SceneBackendRequest> TakePendingAndroidRenderer() noexcept;
}
