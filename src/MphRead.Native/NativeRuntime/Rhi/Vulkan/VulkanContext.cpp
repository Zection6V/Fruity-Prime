#include "VulkanContext.hpp"
#include "../../../Renderer.hpp"
#include "../../../Mods/Branding.hpp"
#include <iostream>
#include <stdexcept>

#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
#include "VulkanContextInternal.hpp"

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    Context::Context(bool validation) : _impl(std::make_unique<Impl>())
    {
        _impl->Initialize(validation);
    }
    Context::Context(bool validation, ::MphRead::RendererPlatform::Window& window)
        : _impl(std::make_unique<Impl>())
    {
        auto* native = static_cast<GLFWwindow*>(window.NativeHandle());
        if (!native || window.GraphicsMode() != ::MphRead::RendererPlatform::GraphicsWindowMode::NoApi)
            throw std::invalid_argument("A Vulkan context requires a GLFW NoApi window.");
        _impl->Initialize(validation, native);
    }
    Context::~Context() = default;
    const Capabilities& Context::Caps() const noexcept { return _impl->caps; }
    const std::string& Context::DeviceName() const noexcept { return _impl->name; }
    unsigned Context::ValidationErrors() const noexcept { return _impl->errors.load(); }
    bool Context::ValidationEnabled() const noexcept { return _impl->validation; }
    void Context::WaitIdle() { if (_impl->device) Check(_impl->vkDeviceWaitIdle(_impl->device), "vkDeviceWaitIdle"); }
    void Context::Shutdown() { WaitIdle(); _impl->Shutdown(); }

    void Context::CheckCommandBufferDebugName()
    {
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; info.queueFamilyIndex = _impl->graphicsFamily;
        VkCommandPool pool = VK_NULL_HANDLE; Check(_impl->vkCreateCommandPool(_impl->device, &info, nullptr, &pool), "vkCreateCommandPool");
        try
        {
            VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocate.commandPool = pool; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount = 1;
            VkCommandBuffer buffer = VK_NULL_HANDLE;
            Check(_impl->vkAllocateCommandBuffers(_impl->device, &allocate, &buffer), "vkAllocateCommandBuffers");
            _impl->Name(VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<std::uint64_t>(buffer), "RHI foundation command buffer");
        }
        catch (...) { _impl->vkDestroyCommandPool(_impl->device, pool, nullptr); throw; }
        _impl->vkDestroyCommandPool(_impl->device, pool, nullptr);
    }

    int RunFoundationCheck()
    {
        try
        {
            RendererPlatform::WindowSettings settings{};
            settings.GraphicsMode = RendererPlatform::GraphicsWindowMode::NoApi;
            settings.Title = std::string(Mods::Branding::Name) + " Vulkan foundation check";
            settings.StartVisible = false;
            auto window = RendererPlatform::CreateWindow(settings);
            Context context(true);
            context.CheckCommandBufferDebugName(); context.WaitIdle();
            // Destroy while the messenger still exists, so teardown errors count.
            context.Shutdown();
            if (context.ValidationErrors()) throw std::runtime_error("Vulkan validation errors.");
            std::cout << "[vulkan] foundation PASS; clean shutdown; validation=" << context.ValidationEnabled() << '\n';
            return 0;
        }
        catch (const std::exception& e) { std::cerr << "[vulkan] foundation FAIL: " << e.what() << '\n'; return 1; }
    }
}
#else
namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    struct Context::Impl { Capabilities caps; std::string name; };
    Context::Context(bool) { throw std::runtime_error("Desktop Vulkan development support was not built."); }
    Context::Context(bool, ::MphRead::RendererPlatform::Window&)
    {
        throw std::runtime_error("Desktop Vulkan development support was not built.");
    }
    Context::~Context() = default;
    const Capabilities& Context::Caps() const noexcept { return _impl->caps; }
    const std::string& Context::DeviceName() const noexcept { return _impl->name; }
    unsigned Context::ValidationErrors() const noexcept { return 0; }
    bool Context::ValidationEnabled() const noexcept { return false; }
    void Context::WaitIdle() { throw std::runtime_error("Vulkan unavailable."); }
    void Context::Shutdown() { throw std::runtime_error("Vulkan unavailable."); }
    void Context::CheckCommandBufferDebugName() { throw std::runtime_error("Vulkan unavailable."); }
    int RunFoundationCheck()
    {
        std::cerr << "[vulkan] foundation unavailable: desktop Vulkan SDK support was not built.\n";
        return 1;
    }
}
#endif
