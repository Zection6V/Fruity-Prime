#include "WebLink.hpp"

#include <utility>

namespace MphRead::Mods::Platform
{
    ::MphRead::NativeRuntime::AtomicSharedPtr<IWebLink> WebLink::_current{};

    std::shared_ptr<IWebLink> WebLink::Current()
    {
        return _current.load();
    }

    void WebLink::Current(std::shared_ptr<IWebLink> value)
    {
        _current.store(std::move(value));
    }
}
