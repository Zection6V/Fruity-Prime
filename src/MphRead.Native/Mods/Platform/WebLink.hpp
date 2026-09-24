#pragma once

#include "../../NativeRuntime/System/AtomicSharedPtr.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace MphRead::Mods::Platform
{
    // A platform that opens an address in whatever it uses for a browser.
    //
    // The seam exists for the same reason Mods::ILogShare's does: the shared
    // code must not know what an Android intent is. Update::Updater asks this
    // first and falls back to the desktop's own answers -- ShellExecute,
    // open, xdg-open -- when nothing is installed.
    //
    // Android is the one head that needs it, and it needed it badly: there is
    // no DISPLAY and no WAYLAND_DISPLAY on a phone, so the Linux branch
    // answered "there is no browser here" and the support mark on the front
    // screen did nothing at all.
    class IWebLink
    {
    public:
        virtual ~IWebLink() = default;

        // Open it. False when the platform refused, so the caller can put the
        // address on screen instead of appearing to do nothing.
        [[nodiscard]] virtual bool Open(std::string_view url) = 0;
    };

    // The opener this build has, or null. Set by the platform head.
    class WebLink final
    {
    public:
        WebLink() = delete;
        ~WebLink() = delete;
        WebLink(const WebLink&) = delete;
        WebLink& operator=(const WebLink&) = delete;
        WebLink(WebLink&&) = delete;
        WebLink& operator=(WebLink&&) = delete;

        [[nodiscard]] static std::shared_ptr<IWebLink> Current();
        static void Current(std::shared_ptr<IWebLink> value);

    private:
        static ::MphRead::NativeRuntime::AtomicSharedPtr<IWebLink> _current;
    };
}
