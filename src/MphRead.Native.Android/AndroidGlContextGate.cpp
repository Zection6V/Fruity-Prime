#include "AndroidGlContextGate.hpp"

#if !defined(__ANDROID__)
#error "AndroidGlContextGate is only valid for the Android native target."
#endif

namespace
{
    std::mutex g_androidGlContextGate;
}

namespace MphRead::Droid
{
    AndroidGlContextLease::AndroidGlContextLease()
        : _lock(g_androidGlContextGate)
    {
    }
}
