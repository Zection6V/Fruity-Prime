#pragma once

#if !defined(__ANDROID__)
#error "AndroidGlContextGate is only valid for the Android native target."
#endif

#include <mutex>

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

        AndroidGlContextLease(const AndroidGlContextLease&) = delete;
        AndroidGlContextLease& operator=(const AndroidGlContextLease&) = delete;
        AndroidGlContextLease(AndroidGlContextLease&&) = delete;
        AndroidGlContextLease& operator=(AndroidGlContextLease&&) = delete;

    private:
        std::unique_lock<std::mutex> _lock;
    };
}
