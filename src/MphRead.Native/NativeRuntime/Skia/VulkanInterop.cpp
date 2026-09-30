#include "VulkanInterop.hpp"

#include "../Rhi/SceneBackend.hpp"

#if defined(FRUITY_SKIA_VULKAN) && defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
#include "../Rhi/Vulkan/VulkanGraphicsDevice.hpp"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/GpuTypes.h>
#include <include/gpu/MutableTextureState.h>
#include <include/gpu/ganesh/GrBackendSurface.h>
#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/SkSurfaceGanesh.h>
#include <include/gpu/ganesh/vk/GrVkBackendSurface.h>
#include <include/gpu/ganesh/vk/GrVkDirectContext.h>
#include <include/gpu/ganesh/vk/GrVkTypes.h>
#include <include/gpu/vk/VulkanBackendContext.h>
#include <include/gpu/vk/VulkanExtensions.h>
#include <include/gpu/vk/VulkanMutableTextureState.h>
#endif

#include <stdexcept>

namespace MphRead::NativeRuntime::Skia::VulkanInterop
{
    Target::Target() = default;
    Target::~Target() = default;
    Target::Target(Target&&) noexcept = default;
    Target& Target::operator=(Target&&) noexcept = default;

#if defined(FRUITY_SKIA_VULKAN) && defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
    namespace Rhi = ::MphRead::NativeRuntime::Rhi;

    bool Available() noexcept { return true; }

    bool Active() noexcept
    {
        return Rhi::ScenePresentsWindow();
    }

    sk_sp<GrDirectContext> MakeContext()
    {
        const Rhi::Vulkan::InteropDevice device = Rhi::Vulkan::DescribeDevice(Rhi::SceneDevice());
        const auto getInstance = reinterpret_cast<PFN_vkGetInstanceProcAddr>(device.GetInstanceProcAddr);
        const auto getDevice = reinterpret_cast<PFN_vkGetDeviceProcAddr>(device.GetDeviceProcAddr);
        skgpu::VulkanBackendContext backend{};
        backend.fInstance = reinterpret_cast<VkInstance>(device.Instance);
        backend.fPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(device.PhysicalDevice);
        backend.fDevice = reinterpret_cast<VkDevice>(device.Device);
        backend.fQueue = reinterpret_cast<VkQueue>(device.Queue);
        backend.fGraphicsQueueIndex = device.QueueFamily;
        backend.fMaxAPIVersion = device.ApiVersion;
        // The RHI enables its own feature set; Skia is told of none and asks
        // for none, so it needs nothing the device does not have.
        static const skgpu::VulkanExtensions extensions;
        backend.fVkExtensions = &extensions;
        backend.fGetProc = [getInstance, getDevice](const char* name, VkInstance instance, VkDevice vkDevice)
        {
            if (vkDevice != VK_NULL_HANDLE)
            {
                if (PFN_vkVoidFunction function = getDevice(vkDevice, name)) return function;
            }
            return getInstance(instance, name);
        };
        sk_sp<GrDirectContext> context = GrDirectContexts::MakeVulkan(backend);
        if (!context) throw std::runtime_error("Skia could not create a Ganesh Vulkan context on the RHI device.");
        return context;
    }

    sk_sp<SkSurface> MakeSurface(GrDirectContext& context, Target& target, std::int32_t width, std::int32_t height)
    {
        auto& gpu = Rhi::SceneDevice();
        target.Texture = gpu.CreateTexture(Rhi::TextureDesc{static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height), 1, 1, 1, 1, Rhi::TextureFormat::RGBA8Unorm,
            Rhi::TextureUsage::Sampled | Rhi::TextureUsage::ColorAttachment
                | Rhi::TextureUsage::TransferSrc | Rhi::TextureUsage::TransferDst});
        target.Width = width;
        target.Height = height;
        const Rhi::Vulkan::InteropImage image
            = Rhi::Vulkan::PrepareForExternal(gpu, *target.Texture, Rhi::ResourceState::Common);
        GrVkImageInfo info{};
        info.fImage = reinterpret_cast<VkImage>(image.Image);
        info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
        info.fImageLayout = static_cast<VkImageLayout>(image.Layout);
        info.fFormat = static_cast<VkFormat>(image.Format);
        info.fImageUsageFlags = image.Usage;
        info.fSampleCount = 1;
        info.fLevelCount = 1;
        info.fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED;
        info.fSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        const GrBackendTexture backend = GrBackendTextures::MakeVk(width, height, info);
        // Top row first, as every texture this renderer uploads is.
        sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(&context, backend, kTopLeft_GrSurfaceOrigin,
            1, kRGBA_8888_SkColorType, nullptr, nullptr);
        if (!surface) throw std::runtime_error("Skia could not wrap the RHI Vulkan image as a surface.");
        return surface;
    }

    void BeginFrame(Target& target)
    {
        if (!target.Texture)
        {
            Rhi::Vulkan::FlushDevice(Rhi::SceneDevice());
            return;
        }
        // Back to GENERAL (the composite left it ShaderRead), with every
        // earlier RHI submission ahead of Skia's.
        (void)Rhi::Vulkan::PrepareForExternal(Rhi::SceneDevice(), *target.Texture, Rhi::ResourceState::Common);
    }

    void EndFrame(GrDirectContext& context, SkSurface& surface, Target& target)
    {
        const skgpu::MutableTextureState general
            = skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED);
        context.flush(&surface, SkSurfaces::BackendSurfaceAccess::kNoAccess, &general);
        context.submit(GrSyncCpu::kYes);
        if (target.Texture) Rhi::Vulkan::AdoptExternalState(*target.Texture, Rhi::ResourceState::Common);
    }
#else
    bool Available() noexcept { return false; }
    bool Active() noexcept { return false; }
#endif
}
