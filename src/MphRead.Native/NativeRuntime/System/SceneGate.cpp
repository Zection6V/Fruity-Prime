#include "SceneGate.hpp"

namespace MphRead::NativeRuntime
{
    std::recursive_mutex& SceneGate() noexcept
    {
        static std::recursive_mutex gate;
        return gate;
    }
}
