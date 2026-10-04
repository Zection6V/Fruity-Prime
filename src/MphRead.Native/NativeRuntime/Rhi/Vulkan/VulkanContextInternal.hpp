#pragma once

#include "VulkanContext.hpp"
#include "../BackendError.hpp"
#include "../GpuDiagnostics.hpp"
#include "../../../Renderer.hpp"
#include "../../../Mods/Branding.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <span>
#include <vector>

#define VK_NO_PROTOTYPES
#if defined(__ANDROID__)
// The same backend on Android: the loader is libvulkan.so, the surface an
// ANativeWindow. Nothing below the instance differs.
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <dlfcn.h>
#else
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#include "VulkanNvidiaReflex.hpp"
#include "VulkanResult.hpp"
#include "VulkanFeatureProbe.hpp"
#include "VulkanLegacy.hpp"
#include "VulkanWindowSystem.hpp"

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    struct Context::Impl
    {
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkPhysicalDevice physical = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphics = VK_NULL_HANDLE;
        VkQueue present = VK_NULL_HANDLE;
        std::uint32_t graphicsFamily = 0, presentFamily = 0;
        Capabilities caps{};
        std::string name;
        std::uint32_t apiVersion = 0;
        std::uint32_t driverVersion = 0;
        std::uint32_t vendorId = 0;
        bool validation = false;
        bool swapchainMaintenance1 = false;
        bool memoryBudget = false;
        // See PhysicalDeviceProbe::Legacy: no dynamic rendering, synchronization2 or 1.3 dynamic state.
        bool legacy = false;
        bool nvLowLatency2 = false;
        std::uint32_t nvLowLatency2Revision = 0;
        std::string reflexUnavailableReason;
        VulkanNvidiaReflex* reflex = nullptr; // Runtime controller owned by presentation swapchain.
        std::uint64_t reflexFrameSequence = 0;
        std::function<void()> establishReflexFrame;
        PFN_vkSetLatencySleepModeNV vkSetLatencySleepModeNV = nullptr;
        PFN_vkLatencySleepNV vkLatencySleepNV = nullptr;
        PFN_vkSetLatencyMarkerNV vkSetLatencyMarkerNV = nullptr;
        PFN_vkGetLatencyTimingsNV vkGetLatencyTimingsNV = nullptr;
        std::atomic<unsigned> errors{0};
        PFN_vkSetDebugUtilsObjectNameEXT setName = nullptr;
        PFN_vkCmdBeginDebugUtilsLabelEXT beginLabel = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT endLabel = nullptr;
        PFN_vkCmdInsertDebugUtilsLabelEXT insertLabel = nullptr;
        TimestampProperties timestampProperties;
        InstanceProbe instanceProbe;
        std::vector<PhysicalDeviceProbe> deviceProbes;
#define VULKAN_INSTANCE_FUNCTIONS(X) \
        X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
        X(vkGetPhysicalDeviceFeatures2) X(vkGetPhysicalDeviceProperties2) X(vkGetPhysicalDeviceMemoryProperties2) X(vkEnumerateDeviceExtensionProperties) \
        X(vkGetPhysicalDeviceFormatProperties) X(vkGetPhysicalDeviceImageFormatProperties) X(vkGetPhysicalDeviceQueueFamilyProperties) \
        X(vkGetPhysicalDeviceMemoryProperties) X(vkGetPhysicalDeviceSurfaceSupportKHR) \
        X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) X(vkGetPhysicalDeviceSurfaceFormatsKHR) \
        X(vkGetPhysicalDeviceSurfacePresentModesKHR) X(vkDestroySurfaceKHR) \
        X(vkCreateDevice) X(vkGetDeviceProcAddr)
#define VULKAN_CACHE_FUNCTIONS(X) \
        X(vkCreatePipelineCache) X(vkDestroyPipelineCache) X(vkGetPipelineCacheData)
// The 1.2 and 1.3 entry points: a legacy device has shims (VulkanLegacy.hpp),
// or the timeline extension's names, in their place.
#define VULKAN_MODERN_DEVICE_FUNCTIONS(X) \
        X(vkCmdWriteTimestamp2) X(vkGetDeviceBufferMemoryRequirements) X(vkGetDeviceImageMemoryRequirements) \
        X(vkCmdPipelineBarrier2) X(vkCmdBeginRendering) X(vkCmdEndRendering) X(vkQueueSubmit2) X(vkCmdBindVertexBuffers2)
#define VULKAN_TIMELINE_DEVICE_FUNCTIONS(X) \
        X(vkWaitSemaphores) X(vkGetSemaphoreCounterValue)
#define VULKAN_DEVICE_FUNCTIONS(X) \
        X(vkCreateQueryPool) X(vkDestroyQueryPool) X(vkGetQueryPoolResults) X(vkCmdResetQueryPool) \
        X(vkCmdPushConstants) X(vkDeviceWaitIdle) X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkCreateCommandPool) \
        X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkFreeCommandBuffers) X(vkResetCommandPool) \
        X(vkBeginCommandBuffer) X(vkEndCommandBuffer) \
        X(vkCmdCopyBuffer) X(vkCmdFillBuffer) \
        X(vkCmdCopyBufferToImage) X(vkCmdCopyImageToBuffer) X(vkCreateImageView) \
        X(vkDestroyImageView) X(vkCreateSampler) X(vkDestroySampler) \
        X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) \
        X(vkGetDescriptorSetLayoutSupport) \
        X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) X(vkResetDescriptorPool) \
        X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCmdBindDescriptorSets) \
        X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) \
        X(vkCreateShaderModule) X(vkDestroyShaderModule) \
        X(vkCreateGraphicsPipelines) X(vkDestroyPipeline) X(vkCmdBindPipeline) \
        X(vkCreateSemaphore) X(vkDestroySemaphore) \
        X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) X(vkResetFences) \
        X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) X(vkGetSwapchainImagesKHR) \
        X(vkAcquireNextImageKHR) X(vkQueuePresentKHR) \
        X(vkCmdSetViewport) X(vkCmdSetScissor) X(vkCmdBindIndexBuffer) \
        X(vkCmdSetStencilReference) X(vkCmdDraw) X(vkCmdDrawIndexed) X(vkCmdCopyImage) X(vkCmdClearColorImage) X(vkCmdBlitImage) X(vkGetFenceStatus)
#define DECLARE_VULKAN_FUNCTION(name) PFN_##name name = nullptr;
        VULKAN_INSTANCE_FUNCTIONS(DECLARE_VULKAN_FUNCTION)
        VULKAN_DEVICE_FUNCTIONS(DECLARE_VULKAN_FUNCTION)
        VULKAN_MODERN_DEVICE_FUNCTIONS(DECLARE_VULKAN_FUNCTION)
        VULKAN_TIMELINE_DEVICE_FUNCTIONS(DECLARE_VULKAN_FUNCTION)
        VULKAN_CACHE_FUNCTIONS(DECLARE_VULKAN_FUNCTION)
        DECLARE_VULKAN_FUNCTION(vkEnumerateInstanceExtensionProperties)
        DECLARE_VULKAN_FUNCTION(vkEnumerateInstanceLayerProperties)
        DECLARE_VULKAN_FUNCTION(vkCreateInstance)
#undef DECLARE_VULKAN_FUNCTION

        // The platform's loader entry point: the window toolkit's on the desktop, the
        // system loader's own on Android.
        static PFN_vkGetInstanceProcAddr InstanceProc()
        {
#if defined(__ANDROID__)
            static PFN_vkGetInstanceProcAddr proc = []
            {
                void* library = ::dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
                if (!library) throw std::runtime_error("This device has no Vulkan loader (libvulkan.so).");
                auto entry = reinterpret_cast<PFN_vkGetInstanceProcAddr>(::dlsym(library, "vkGetInstanceProcAddr"));
                if (!entry) throw std::runtime_error("libvulkan.so has no vkGetInstanceProcAddr.");
                return entry;
            }();
            return proc;
#else
            return WindowSystem::LoaderEntry();
#endif
        }

        template<class T> T Load(const char* function)
        {
            auto pointer = InstanceProc()(instance, function);
            if (!pointer) throw std::runtime_error(std::string("Missing Vulkan entry point: ") + function);
            return reinterpret_cast<T>(pointer);
        }

        static VKAPI_ATTR VkBool32 VKAPI_CALL Debug(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* message, void* userdata)
        {
            auto& self = *static_cast<Impl*>(userdata);
            if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) ++self.errors;
            std::cerr << "[vulkan validation] " << message->pMessage << '\n';
            return VK_FALSE;
        }

        ~Impl() { Shutdown(); }
        void Shutdown() noexcept
        {
            if (device)
            {
                if (!vkDeviceWaitIdle && vkGetDeviceProcAddr)
                    vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(vkGetDeviceProcAddr(device, "vkDeviceWaitIdle"));
                if (!vkDestroyDevice && vkGetDeviceProcAddr)
                    vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(vkGetDeviceProcAddr(device, "vkDestroyDevice"));
                if (vkDeviceWaitIdle) vkDeviceWaitIdle(device);
                if (legacy) Legacy::Uninstall(device);
                if (vkDestroyDevice) vkDestroyDevice(device, nullptr);
                device = VK_NULL_HANDLE;
            }
            if (surface && instance)
            {
                if (vkDestroySurfaceKHR) vkDestroySurfaceKHR(instance, surface, nullptr);
                surface = VK_NULL_HANDLE;
            }
            if (messenger)
            {
                auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    InstanceProc()(instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (destroy) destroy(instance, messenger, nullptr);
                messenger = VK_NULL_HANDLE;
            }
            if (instance)
            {
                if (!vkDestroyInstance)
                    vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(InstanceProc()(instance, "vkDestroyInstance"));
                if (vkDestroyInstance) vkDestroyInstance(instance, nullptr);
                instance = VK_NULL_HANDLE;
            }
        }
        void Name(VkObjectType type, std::uint64_t handle, const char* label)
        {
            if (!setName) return;
            VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            info.objectType = type; info.objectHandle = handle; info.pObjectName = label;
            Check(setName(device, &info), "vkSetDebugUtilsObjectNameEXT");
        }

#if defined(__ANDROID__)
        // A surface for this ANativeWindow, replacing any earlier one (whose
        // swapchain must already be gone). The device stays.
        void CreateAndroidSurface(void* nativeWindow)
        {
            DestroySurface();
            if (!nativeWindow) return;
            auto create = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
                InstanceProc()(instance, "vkCreateAndroidSurfaceKHR"));
            if (!create) throw std::runtime_error("vkCreateAndroidSurfaceKHR is unavailable.");
            VkAndroidSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
            info.window = static_cast<ANativeWindow*>(nativeWindow);
            Check(create(instance, &info, nullptr, &surface), "vkCreateAndroidSurfaceKHR");
        }
#endif
#if !defined(__ANDROID__)
        // A surface for another GLFW window, replacing the last one (whose
        // swapchain must already be gone): the renderer was switched and the
        // window remade, and the device and every resource on it stay.
        void CreateWindowSurface(void* window)
        {
            DestroySurface();
            surface = WindowSystem::CreateSurface(instance, window, InstanceProc());
            VkBool32 supported = VK_FALSE;
            Check(vkGetPhysicalDeviceSurfaceSupportKHR(physical, presentFamily, surface, &supported),
                "vkGetPhysicalDeviceSurfaceSupportKHR");
            if (!supported) throw std::runtime_error("The new window cannot be presented from the device's queue.");
        }
#endif
        void DestroySurface() noexcept
        {
            if (surface && instance && vkDestroySurfaceKHR) vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }

        void Initialize(bool requestedValidation, void* presentationWindow = nullptr,
            bool allowMaintenance = true, bool createLogicalDevice = true)
        {
            vkEnumerateInstanceExtensionProperties = Load<PFN_vkEnumerateInstanceExtensionProperties>("vkEnumerateInstanceExtensionProperties");
            vkEnumerateInstanceLayerProperties = Load<PFN_vkEnumerateInstanceLayerProperties>("vkEnumerateInstanceLayerProperties");
            vkCreateInstance = Load<PFN_vkCreateInstance>("vkCreateInstance");
            const auto enumerateVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
                InstanceProc()(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
#if defined(__ANDROID__)
            const std::array<const char*, 2> windowExtensions{VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
#else
            const std::span<const char* const> windowExtensions = WindowSystem::RequiredInstanceExtensions();
#endif
            instanceProbe = EvaluateInstance(QueryInstanceSnapshot(
                {enumerateVersion, vkEnumerateInstanceExtensionProperties, vkEnumerateInstanceLayerProperties}, windowExtensions),
                requestedValidation, allowMaintenance);
            if (!instanceProbe.Eligible)
                throw BackendError(GraphicsBackend::Vulkan, BackendErrorKind::Unsupported, VK_ERROR_INCOMPATIBLE_DRIVER,
                    "Vulkan instance eligibility: " + RejectionReasons(instanceProbe.Findings));
            std::vector<const char*> extensions;
            for (const auto& extension : instanceProbe.EnabledExtensions) extensions.push_back(extension.c_str());
            const bool debugUtils = instanceProbe.DebugUtils;
            validation = instanceProbe.Validation;
            const VkInstanceCreateFlags flags = instanceProbe.PortabilityEnumeration ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0;
            const char* layer = "VK_LAYER_KHRONOS_validation";
            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app.pApplicationName = Mods::Branding::Name.data(); app.apiVersion = std::min<std::uint32_t>(VK_API_VERSION_1_3, instanceProbe.LoaderVersion);
            VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug.pfnUserCallback = Debug; debug.pUserData = this;
            VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            create.pApplicationInfo = &app; create.flags = flags;
            create.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()); create.ppEnabledExtensionNames = extensions.data();
            create.enabledLayerCount = validation ? 1U : 0U; create.ppEnabledLayerNames = validation ? &layer : nullptr;
            create.pNext = debugUtils ? &debug : nullptr;
            Check(vkCreateInstance(&create, nullptr, &instance), "vkCreateInstance");
#define LOAD_VULKAN_INSTANCE_FUNCTION(name) name = Load<PFN_##name>(#name);
            VULKAN_INSTANCE_FUNCTIONS(LOAD_VULKAN_INSTANCE_FUNCTION)
#undef LOAD_VULKAN_INSTANCE_FUNCTION
            if (debugUtils)
            {
                auto createDebug = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(InstanceProc()(instance, "vkCreateDebugUtilsMessengerEXT"));
                if (!createDebug) throw std::runtime_error("Debug utils entry point unavailable.");
                Check(createDebug(instance, &debug, nullptr, &messenger), "vkCreateDebugUtilsMessengerEXT");
            }
            if (presentationWindow != nullptr)
            {
#if defined(__ANDROID__)
                vkDestroySurfaceKHR = Load<PFN_vkDestroySurfaceKHR>("vkDestroySurfaceKHR");
                CreateAndroidSurface(presentationWindow);
#else
                surface = WindowSystem::CreateSurface(instance, presentationWindow, InstanceProc());
#endif
            }
            PhysicalProbeDispatch query{instance, vkEnumeratePhysicalDevices, vkGetPhysicalDeviceProperties,
                vkEnumerateDeviceExtensionProperties, vkGetPhysicalDeviceFeatures2, vkGetPhysicalDeviceFormatProperties,
                vkGetPhysicalDeviceQueueFamilyProperties, vkGetPhysicalDeviceMemoryProperties,
                vkGetPhysicalDeviceSurfaceSupportKHR, [this](VkPhysicalDevice candidate, std::uint32_t family) {
#if defined(__ANDROID__)
                    (void)candidate; (void)family; return true;
#else
                    return WindowSystem::PresentationSupport(instance, candidate, family, InstanceProc());
#endif
                }};
            std::string rejectedDevices;
            for (auto& snapshot : QueryPhysicalDevices(query, surface, instanceProbe.SurfaceMaintenance1))
            {
                const auto& props = snapshot.Properties;
                std::cout << "[vulkan] GPU " << props.deviceName << " API "
                    << VK_API_VERSION_MAJOR(props.apiVersion) << '.' << VK_API_VERSION_MINOR(props.apiVersion) << '\n';
                auto probe = EvaluatePhysicalDevice(std::move(snapshot), instanceProbe.SurfaceMaintenance1);
                if (!probe.Eligible)
                {
                    const auto reasons = std::string(probe.Snapshot.Properties.deviceName) + ": " + RejectionReasons(probe.Findings);
                    std::cout << "[vulkan] ineligible " << reasons << '\n';
                    if (!rejectedDevices.empty()) rejectedDevices += " | ";
                    rejectedDevices += reasons;
                }
                deviceProbes.push_back(std::move(probe));
            }
            const auto selected = SelectPhysicalDevice(deviceProbes);
            if (!selected)
                throw BackendError(GraphicsBackend::Vulkan, BackendErrorKind::Unsupported, VK_ERROR_FEATURE_NOT_PRESENT,
                    "No eligible Vulkan GPU. " + (rejectedDevices.empty() ? std::string("No physical devices reported.") : rejectedDevices));
            const auto& probe = deviceProbes[*selected];
            const auto& props = probe.Snapshot.Properties;
            physical = probe.Snapshot.Device; graphicsFamily = probe.GraphicsFamily; presentFamily = probe.PresentFamily;
            memoryBudget = probe.MemoryBudget;
            name = props.deviceName; apiVersion = props.apiVersion; driverVersion = props.driverVersion; vendorId = props.vendorID;
            caps = probe.Caps; timestampProperties = probe.Timestamps;
            // Passive eligibility never creates a logical device or queue.
            if (!createLogicalDevice) return;
            CreateLogicalDevice(probe, debugUtils);
        }

        void CreateLogicalDevice(const PhysicalDeviceProbe& probe, bool debugUtils)
        {
            const bool portabilitySubset = probe.PortabilitySubset;
            const bool maintenance1 = probe.SwapchainMaintenance1;
            const auto& selectedFeatures = probe.Snapshot.Features;
            float priority = 1.0F;
            std::vector<VkDeviceQueueCreateInfo> queues;
            for (auto family : {graphicsFamily, presentFamily})
            {
                if (!queues.empty() && queues.front().queueFamilyIndex == family) continue;
                VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                q.queueFamilyIndex = family; q.queueCount = 1; q.pQueuePriorities = &priority; queues.push_back(q);
            }
            legacy = probe.Legacy;
            // Each enabled feature struct goes on the front of the chain. A 1.1
            // device knows none of the VulkanNN structs: its timeline semaphores
            // are the extension's.
            void* featureChain = nullptr;
            const auto chain = [&featureChain](auto& feature) { feature.pNext = featureChain; featureChain = &feature; };
            VkPhysicalDeviceVulkan13Features enabled13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            enabled13.dynamicRendering = VK_TRUE; enabled13.synchronization2 = VK_TRUE;
            VkPhysicalDeviceVulkan12Features enabled12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
            enabled12.timelineSemaphore = VK_TRUE;
            VkPhysicalDeviceTimelineSemaphoreFeatures enabledTimeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
            enabledTimeline.timelineSemaphore = VK_TRUE;
            VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT enabledMaintenance1{
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
            std::vector<const char*> deviceExtensionNames{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
            if (probe.TimelineSemaphoreExtension)
            {
                chain(enabledTimeline);
                deviceExtensionNames.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
            }
            else chain(enabled12);
            // A 1.3 device forced onto the legacy path does not enable what it
            // must not use, so that validation says when it is used.
            if (!legacy) chain(enabled13);
            nvLowLatency2Revision = probe.NvLowLatency2SpecVersion;
            nvLowLatency2 = probe.NvLowLatency2;
            reflexUnavailableReason = probe.ReflexUnavailableReason;
            const auto* injectReflex = std::getenv("FRUITY_REFLEX_TEST_FAILURE");
            if (injectReflex && (std::string_view(injectReflex) == "extension" || std::string_view(injectReflex) == "present-id"))
            {
                nvLowLatency2 = false;
                reflexUnavailableReason = std::string("NVIDIA Reflex: injected unavailable ") + injectReflex + '.';
            }
            VkPhysicalDevicePresentIdFeaturesKHR enabledPresentId{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR};
            if (nvLowLatency2)
            {
                enabledPresentId.presentId = VK_TRUE;
                chain(enabledPresentId);
                deviceExtensionNames.push_back(VK_NV_LOW_LATENCY_2_EXTENSION_NAME);
                deviceExtensionNames.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
            }
            if (memoryBudget) deviceExtensionNames.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
            if (portabilitySubset) deviceExtensionNames.push_back("VK_KHR_portability_subset");
            if (maintenance1)
            {
                enabledMaintenance1.swapchainMaintenance1 = VK_TRUE;
                chain(enabledMaintenance1);
                deviceExtensionNames.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
            }
            VkPhysicalDeviceFeatures enabled{};
            enabled.samplerAnisotropy = selectedFeatures.samplerAnisotropy;
            enabled.fillModeNonSolid = selectedFeatures.fillModeNonSolid;
            enabled.depthClamp = selectedFeatures.depthClamp;
            enabled.wideLines = selectedFeatures.wideLines;
            enabled.independentBlend = selectedFeatures.independentBlend;
            VkDeviceCreateInfo createDevice{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            createDevice.pNext = featureChain; createDevice.pEnabledFeatures = &enabled;
            createDevice.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size()); createDevice.pQueueCreateInfos = queues.data();
            createDevice.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensionNames.size());
            createDevice.ppEnabledExtensionNames = deviceExtensionNames.data();
            Check(vkCreateDevice(physical, &createDevice, nullptr, &device), "vkCreateDevice");
            swapchainMaintenance1 = maintenance1;
#define LOAD_VULKAN_DEVICE_FUNCTION(name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name)); if (!name) throw std::runtime_error("Missing Vulkan device entry point: " #name);
            VULKAN_DEVICE_FUNCTIONS(LOAD_VULKAN_DEVICE_FUNCTION)
            if (!probe.TimelineSemaphoreExtension) { VULKAN_TIMELINE_DEVICE_FUNCTIONS(LOAD_VULKAN_DEVICE_FUNCTION) }
            if (!legacy) { VULKAN_MODERN_DEVICE_FUNCTIONS(LOAD_VULKAN_DEVICE_FUNCTION) }
#undef LOAD_VULKAN_DEVICE_FUNCTION
            if (probe.TimelineSemaphoreExtension)
            {
                vkWaitSemaphores = reinterpret_cast<PFN_vkWaitSemaphores>(vkGetDeviceProcAddr(device, "vkWaitSemaphoresKHR"));
                vkGetSemaphoreCounterValue = reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(
                    vkGetDeviceProcAddr(device, "vkGetSemaphoreCounterValueKHR"));
                if (!vkWaitSemaphores || !vkGetSemaphoreCounterValue)
                    throw std::runtime_error("Missing Vulkan device entry point: VK_KHR_timeline_semaphore.");
            }
            if (legacy)
            {
                Legacy::Install(device, vkGetDeviceProcAddr);
                vkCmdWriteTimestamp2 = Legacy::CmdWriteTimestamp2;
                vkGetDeviceBufferMemoryRequirements = Legacy::GetDeviceBufferMemoryRequirements;
                vkGetDeviceImageMemoryRequirements = Legacy::GetDeviceImageMemoryRequirements;
                vkCmdPipelineBarrier2 = Legacy::CmdPipelineBarrier2;
                vkCmdBeginRendering = Legacy::CmdBeginRendering;
                vkCmdEndRendering = Legacy::CmdEndRendering;
                vkQueueSubmit2 = Legacy::QueueSubmit2;
                vkCmdBindVertexBuffers2 = Legacy::CmdBindVertexBuffers2;
                // Every view goes past the legacy path: its format names a
                // rendering's attachments, and its end retires framebuffers.
                vkCreateImageView = Legacy::CreateImageView;
                vkDestroyImageView = Legacy::DestroyImageView;
            }
#define LOAD_VULKAN_CACHE_FUNCTION(name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));
            VULKAN_CACHE_FUNCTIONS(LOAD_VULKAN_CACHE_FUNCTION)
#undef LOAD_VULKAN_CACHE_FUNCTION
            if (nvLowLatency2)
            {
                vkSetLatencySleepModeNV = reinterpret_cast<PFN_vkSetLatencySleepModeNV>(vkGetDeviceProcAddr(device, "vkSetLatencySleepModeNV"));
                vkLatencySleepNV = reinterpret_cast<PFN_vkLatencySleepNV>(vkGetDeviceProcAddr(device, "vkLatencySleepNV"));
                vkSetLatencyMarkerNV = reinterpret_cast<PFN_vkSetLatencyMarkerNV>(vkGetDeviceProcAddr(device, "vkSetLatencyMarkerNV"));
                vkGetLatencyTimingsNV = reinterpret_cast<PFN_vkGetLatencyTimingsNV>(vkGetDeviceProcAddr(device, "vkGetLatencyTimingsNV"));
                if (!vkSetLatencySleepModeNV || !vkLatencySleepNV || !vkSetLatencyMarkerNV || !vkGetLatencyTimingsNV
                    || (injectReflex && std::string_view(injectReflex) == "entrypoints"))
                { nvLowLatency2 = false; reflexUnavailableReason = "NVIDIA Reflex: required device entry point is missing."; }
            }
            std::cout << "[reflex probe] enabled=" << nvLowLatency2 << " revision=" << nvLowLatency2Revision
                << " presentId=" << probe.PresentId << " reason=" << reflexUnavailableReason << '\n';
            vkGetDeviceQueue(device, graphicsFamily, 0, &graphics); vkGetDeviceQueue(device, presentFamily, 0, &present);
            if (!graphics || !present) throw std::runtime_error("Missing graphics/present queue.");
            setName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
            beginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
            endLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
            insertLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT"));
            caps.supportsDebugLabels = debugUtils && beginLabel && endLabel && insertLabel;
            Name(VK_OBJECT_TYPE_DEVICE, reinterpret_cast<std::uint64_t>(device), "RHI Vulkan device");
            Name(VK_OBJECT_TYPE_QUEUE, reinterpret_cast<std::uint64_t>(graphics), "RHI graphics queue");
            std::cout << "[vulkan] selected " << name << " graphics=" << graphicsFamily
                << " present=" << presentFamily << " validation=" << validation
                << " swapchainMaintenance1=" << maintenance1 << " legacy=" << legacy << '\n';
        }
    };

}
#undef VULKAN_INSTANCE_FUNCTIONS
#undef VULKAN_DEVICE_FUNCTIONS
#undef VULKAN_MODERN_DEVICE_FUNCTIONS
#undef VULKAN_TIMELINE_DEVICE_FUNCTIONS
#undef VULKAN_CACHE_FUNCTIONS

namespace MphRead::NativeRuntime::Rhi
{
    class Swapchain;
}

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // A fence wait that says so when it is stuck: a GPU hang or a fence that
    // was never submitted is otherwise a window thread that silently stops.
    inline VkResult WaitFenceReporting(PFN_vkWaitForFences wait, VkDevice device, const VkFence* fence,
        const char* what)
    {
        for (int seconds = 2;; seconds += 2)
        {
            const VkResult result = wait(device, 1, fence, VK_TRUE, 2'000'000'000ULL);
            if (result != VK_TIMEOUT) return result;
            std::cerr << "[vulkan] still waiting for " << what << " after " << seconds << " s" << std::endl;
        }
    }

    // Acquire the next image unless the window has nothing to draw into.
    // Records the acquired image's frame: the source image blitted upright
    // (or black when there is none), in and back out of the given layout.
    void RecordSwapchainBlit(Swapchain& swapchain, VkImage source, VkImageLayout layout,
        VkPipelineStageFlags2 stages, VkAccessFlags2 access, VkExtent2D extent);
}
