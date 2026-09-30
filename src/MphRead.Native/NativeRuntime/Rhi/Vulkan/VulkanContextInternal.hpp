#pragma once

#include "VulkanContext.hpp"
#include "../../../Renderer.hpp"
#include "../../../Mods/Branding.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    namespace
    {
        void Check(VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(result));
        }
        template<class T> bool Contains(const std::vector<T>& list, const char* name)
        {
            return std::any_of(list.begin(), list.end(), [name](const T& value) {
                if constexpr (requires { value.extensionName; })
                    return std::strcmp(value.extensionName, name) == 0;
                else return std::strcmp(value.layerName, name) == 0;
            });
        }
    }

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
        bool validation = false;
        bool swapchainMaintenance1 = false;
        std::atomic<unsigned> errors{0};
        PFN_vkSetDebugUtilsObjectNameEXT setName = nullptr;
#define VULKAN_INSTANCE_FUNCTIONS(X) \
        X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
        X(vkGetPhysicalDeviceFeatures2) X(vkEnumerateDeviceExtensionProperties) \
        X(vkGetPhysicalDeviceFormatProperties) X(vkGetPhysicalDeviceQueueFamilyProperties) \
        X(vkGetPhysicalDeviceMemoryProperties) X(vkGetPhysicalDeviceSurfaceSupportKHR) \
        X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) X(vkGetPhysicalDeviceSurfaceFormatsKHR) \
        X(vkGetPhysicalDeviceSurfacePresentModesKHR) X(vkDestroySurfaceKHR) \
        X(vkCreateDevice) X(vkGetDeviceProcAddr)
#define VULKAN_DEVICE_FUNCTIONS(X) \
        X(vkDeviceWaitIdle) X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkCreateCommandPool) \
        X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkResetCommandPool) \
        X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCmdPipelineBarrier2) \
        X(vkCmdBeginRendering) X(vkCmdEndRendering) X(vkCreateImageView) \
        X(vkDestroyImageView) X(vkCreateSemaphore) X(vkDestroySemaphore) \
        X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) X(vkResetFences) \
        X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) X(vkGetSwapchainImagesKHR) \
        X(vkAcquireNextImageKHR) X(vkQueueSubmit2) X(vkQueuePresentKHR)
#define DECLARE_VULKAN_FUNCTION(name) PFN_##name name = nullptr;
        VULKAN_INSTANCE_FUNCTIONS(DECLARE_VULKAN_FUNCTION)
        VULKAN_DEVICE_FUNCTIONS(DECLARE_VULKAN_FUNCTION)
        DECLARE_VULKAN_FUNCTION(vkEnumerateInstanceExtensionProperties)
        DECLARE_VULKAN_FUNCTION(vkEnumerateInstanceLayerProperties)
        DECLARE_VULKAN_FUNCTION(vkCreateInstance)
#undef DECLARE_VULKAN_FUNCTION

        template<class T> T Load(const char* function)
        {
            auto pointer = glfwGetInstanceProcAddress(instance, function);
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
            if (device) { vkDeviceWaitIdle(device); vkDestroyDevice(device, nullptr); device = VK_NULL_HANDLE; }
            if (surface && instance)
            {
                if (vkDestroySurfaceKHR) vkDestroySurfaceKHR(instance, surface, nullptr);
                surface = VK_NULL_HANDLE;
            }
            if (messenger)
            {
                auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    glfwGetInstanceProcAddress(instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (destroy) destroy(instance, messenger, nullptr);
                messenger = VK_NULL_HANDLE;
            }
            if (instance) { vkDestroyInstance(instance, nullptr); instance = VK_NULL_HANDLE; }
        }
        void Name(VkObjectType type, std::uint64_t handle, const char* label)
        {
            if (!setName) return;
            VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            info.objectType = type; info.objectHandle = handle; info.pObjectName = label;
            Check(setName(device, &info), "vkSetDebugUtilsObjectNameEXT");
        }

        void Initialize(bool requestedValidation, GLFWwindow* presentationWindow = nullptr)
        {
            std::uint32_t version = VK_API_VERSION_1_0;
            auto enumerateVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
                glfwGetInstanceProcAddress(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
            if (enumerateVersion) Check(enumerateVersion(&version), "vkEnumerateInstanceVersion");
            if (version < VK_API_VERSION_1_3) throw std::runtime_error("Vulkan 1.3 loader required.");
            vkEnumerateInstanceExtensionProperties = Load<PFN_vkEnumerateInstanceExtensionProperties>("vkEnumerateInstanceExtensionProperties");
            vkEnumerateInstanceLayerProperties = Load<PFN_vkEnumerateInstanceLayerProperties>("vkEnumerateInstanceLayerProperties");
            vkCreateInstance = Load<PFN_vkCreateInstance>("vkCreateInstance");

            std::uint32_t count = 0;
            Check(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr), "instance extension count");
            std::vector<VkExtensionProperties> available(count);
            Check(vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data()), "instance extensions");
            std::uint32_t glfwCount = 0;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);
            if (!glfwExtensions || !glfwCount) throw std::runtime_error("No Vulkan window-system extensions.");
            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwCount);
            for (const char* extension : extensions)
                if (!Contains(available, extension)) throw std::runtime_error(std::string("Missing instance extension: ") + extension);
            bool debugUtils = Contains(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            if (debugUtils) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            const bool surfaceCapabilities2 = Contains(
                available, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            const bool surfaceMaintenance1 = surfaceCapabilities2
                && Contains(available, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            if (surfaceCapabilities2 && surfaceMaintenance1)
            {
                extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
                extensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            }
            VkInstanceCreateFlags flags = 0;
            if (Contains(available, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
            {
                extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            }
            Check(vkEnumerateInstanceLayerProperties(&count, nullptr), "instance layer count");
            std::vector<VkLayerProperties> layers(count);
            Check(vkEnumerateInstanceLayerProperties(&count, layers.data()), "instance layers");
            validation = requestedValidation && Contains(layers, "VK_LAYER_KHRONOS_validation");
            if (validation && !debugUtils) throw std::runtime_error("Validation requires debug utils reporting.");
            const char* layer = "VK_LAYER_KHRONOS_validation";
            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app.pApplicationName = Mods::Branding::Name.data(); app.apiVersion = VK_API_VERSION_1_3;
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
                auto createDebug = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(glfwGetInstanceProcAddress(instance, "vkCreateDebugUtilsMessengerEXT"));
                if (!createDebug) throw std::runtime_error("Debug utils entry point unavailable.");
                Check(createDebug(instance, &debug, nullptr, &messenger), "vkCreateDebugUtilsMessengerEXT");
            }
            if (presentationWindow != nullptr)
            {
                Check(glfwCreateWindowSurface(instance, presentationWindow, nullptr, &surface),
                    "glfwCreateWindowSurface");
            }
            Check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "physical device count");
            std::vector<VkPhysicalDevice> devices(count);
            Check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "physical devices");
            std::uint64_t bestScore = 0;
            bool portabilitySubset = false;
            bool maintenance1 = false;
            VkPhysicalDeviceFeatures selectedFeatures{};
            for (auto candidate : devices)
            {
                VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(candidate, &props);
                std::cout << "[vulkan] GPU " << props.deviceName << " API " << VK_API_VERSION_MAJOR(props.apiVersion) << '.' << VK_API_VERSION_MINOR(props.apiVersion) << '\n';
                if (props.apiVersion < VK_API_VERSION_1_3) continue;
                Check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr), "device extension count");
                std::vector<VkExtensionProperties> deviceExtensions(count);
                Check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, deviceExtensions.data()), "device extensions");
                if (!Contains(deviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;
                const bool hasMaintenance1 = surfaceMaintenance1 && Contains(
                    deviceExtensions, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
                VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenanceFeatures{
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
                VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
                if (hasMaintenance1) features13.pNext = &maintenanceFeatures;
                VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &features13;
                vkGetPhysicalDeviceFeatures2(candidate, &features);
                if (!features13.dynamicRendering || !features13.synchronization2) continue;
                const bool candidateMaintenance1 = hasMaintenance1 && maintenanceFeatures.swapchainMaintenance1;
                VkFormatProperties color{}, depth{};
                vkGetPhysicalDeviceFormatProperties(candidate, VK_FORMAT_R8G8B8A8_UNORM, &color);
                vkGetPhysicalDeviceFormatProperties(candidate, VK_FORMAT_D32_SFLOAT, &depth);
                const auto colorRequired = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
                if ((color.optimalTilingFeatures & colorRequired) != colorRequired || !(depth.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) continue;
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
                std::vector<VkQueueFamilyProperties> queues(count); vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, queues.data());
                std::uint32_t g = UINT32_MAX, p = UINT32_MAX;
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    if (!queues[i].queueCount) continue;
                    VkBool32 surfaceSupported = VK_FALSE;
                    bool canPresent = false;
                    if (surface)
                    {
                        Check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &surfaceSupported),
                            "vkGetPhysicalDeviceSurfaceSupportKHR");
                        canPresent = surfaceSupported == VK_TRUE;
                    }
                    else
                    {
                        canPresent = glfwGetPhysicalDevicePresentationSupport(instance, candidate, i) == GLFW_TRUE;
                    }
                    if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent) { g = p = i; break; }
                    if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && g == UINT32_MAX) g = i;
                    if (canPresent && p == UINT32_MAX) p = i;
                }
                if (g == UINT32_MAX || p == UINT32_MAX) continue;
                VkPhysicalDeviceMemoryProperties memory{}; vkGetPhysicalDeviceMemoryProperties(candidate, &memory);
                std::uint64_t bytes = 0;
                for (std::uint32_t i = 0; i < memory.memoryHeapCount; ++i)
                    if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) bytes += memory.memoryHeaps[i].size;
                std::uint64_t score = bytes / (1024 * 1024) + 1;
                if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000000;
                if (score <= bestScore) continue;
                bestScore = score; physical = candidate; graphicsFamily = g; presentFamily = p;
                portabilitySubset = Contains(deviceExtensions, "VK_KHR_portability_subset");
                maintenance1 = candidateMaintenance1;
                selectedFeatures = features.features; name = props.deviceName;
                caps.backend = GraphicsBackend::Vulkan;
                caps.maxTexture2DDimension = props.limits.maxImageDimension2D;
                caps.maxTextureArrayLayers = props.limits.maxImageArrayLayers;
                caps.maxColorAttachments = props.limits.maxColorAttachments;
                caps.maxVertexBuffers = props.limits.maxVertexInputBindings;
                caps.maxBindingSets = props.limits.maxBoundDescriptorSets;
                caps.supportsAnisotropy = features.features.samplerAnisotropy != 0;
                caps.maxSamplerAnisotropy = caps.supportsAnisotropy ? props.limits.maxSamplerAnisotropy : 1.0F;
                caps.supportsWireframe = features.features.fillModeNonSolid != 0;
                caps.supportsDepthClamp = features.features.depthClamp != 0;
                caps.supportsCompute = (queues[g].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
                caps.supportsTimestampQueries = queues[g].timestampValidBits != 0;
            }
            if (!physical) throw std::runtime_error("No Vulkan 1.3 GPU with graphics/present, dynamic rendering, synchronization2 and required formats.");
            float priority = 1.0F;
            std::vector<VkDeviceQueueCreateInfo> queues;
            for (auto family : {graphicsFamily, presentFamily})
            {
                if (!queues.empty() && queues.front().queueFamilyIndex == family) continue;
                VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                q.queueFamilyIndex = family; q.queueCount = 1; q.pQueuePriorities = &priority; queues.push_back(q);
            }
            VkPhysicalDeviceVulkan13Features enabled13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            enabled13.dynamicRendering = VK_TRUE; enabled13.synchronization2 = VK_TRUE;
            VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT enabledMaintenance1{
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
            std::vector<const char*> deviceExtensionNames{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
            if (portabilitySubset) deviceExtensionNames.push_back("VK_KHR_portability_subset");
            if (maintenance1)
            {
                enabledMaintenance1.swapchainMaintenance1 = VK_TRUE;
                enabled13.pNext = &enabledMaintenance1;
                deviceExtensionNames.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
            }
            VkPhysicalDeviceFeatures enabled{};
            enabled.samplerAnisotropy = selectedFeatures.samplerAnisotropy;
            enabled.fillModeNonSolid = selectedFeatures.fillModeNonSolid;
            enabled.depthClamp = selectedFeatures.depthClamp;
            VkDeviceCreateInfo createDevice{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            createDevice.pNext = &enabled13; createDevice.pEnabledFeatures = &enabled;
            createDevice.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size()); createDevice.pQueueCreateInfos = queues.data();
            createDevice.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensionNames.size());
            createDevice.ppEnabledExtensionNames = deviceExtensionNames.data();
            Check(vkCreateDevice(physical, &createDevice, nullptr, &device), "vkCreateDevice");
#define LOAD_VULKAN_DEVICE_FUNCTION(name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name)); if (!name) throw std::runtime_error("Missing Vulkan device entry point: " #name);
            VULKAN_DEVICE_FUNCTIONS(LOAD_VULKAN_DEVICE_FUNCTION)
#undef LOAD_VULKAN_DEVICE_FUNCTION
            vkGetDeviceQueue(device, graphicsFamily, 0, &graphics); vkGetDeviceQueue(device, presentFamily, 0, &present);
            if (!graphics || !present) throw std::runtime_error("Missing graphics/present queue.");
            setName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
            Name(VK_OBJECT_TYPE_DEVICE, reinterpret_cast<std::uint64_t>(device), "RHI Vulkan device");
            Name(VK_OBJECT_TYPE_QUEUE, reinterpret_cast<std::uint64_t>(graphics), "RHI graphics queue");
            std::cout << "[vulkan] selected " << name << " graphics=" << graphicsFamily
                << " present=" << presentFamily << " validation=" << validation
                << " swapchainMaintenance1=" << maintenance1 << '\n';
        }
    };

}
#undef VULKAN_INSTANCE_FUNCTIONS
#undef VULKAN_DEVICE_FUNCTIONS