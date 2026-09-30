#include "VulkanWindowUi.hpp"

#include "../../NativeRuntime/Rhi/SceneBackend.hpp"

#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
#include "../../NativeRuntime/Rhi/Vulkan/VulkanScene.hpp"
#endif

#include <memory>

namespace MphRead::Mods::Render
{
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
    namespace
    {
        std::unique_ptr<NativeRuntime::Rhi::Vulkan::WindowUi>& Instance()
        {
            static std::unique_ptr<NativeRuntime::Rhi::Vulkan::WindowUi> ui;
            return ui;
        }
    }

    bool VulkanWindowUi::Active() noexcept
    {
        return NativeRuntime::Rhi::ScenePresentsWindow();
    }

    NativeRuntime::Rhi::Vulkan::WindowUi* VulkanWindowUi::Get()
    {
        if (!Active()) return nullptr;
        auto& ui = Instance();
        if (!ui) ui = std::make_unique<NativeRuntime::Rhi::Vulkan::WindowUi>(NativeRuntime::Rhi::SceneDevice());
        return ui.get();
    }

    void VulkanWindowUi::Release() noexcept
    {
        Instance().reset();
    }
#else
    bool VulkanWindowUi::Active() noexcept { return false; }
    NativeRuntime::Rhi::Vulkan::WindowUi* VulkanWindowUi::Get() { return nullptr; }
    void VulkanWindowUi::Release() noexcept {}
#endif
}
