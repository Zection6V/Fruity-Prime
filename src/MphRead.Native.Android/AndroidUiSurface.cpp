#include "AndroidUiSurface.hpp"
#include "AndroidQuick.hpp"
namespace MphRead::Droid
{
    std::shared_ptr<AndroidUiSurface> AndroidUiSurface::Current() noexcept { return Ensure(); }
    std::shared_ptr<AndroidUiSurface> AndroidUiSurface::Ensure()
    { static auto surface = std::make_shared<AndroidUiSurface>(); return surface; }
    void AndroidUiSurface::Show(const std::shared_ptr<AndroidLauncherPage>& page) { _visible = true; page->Reset(); }
    void AndroidUiSurface::ShowEnd() { _visible = true; QuickPage("end"); }
    void AndroidUiSurface::Hide() { _visible = false; QuickPage(""); }
}
