#include "VulkanGraphicsDevice.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
#include "VulkanContextInternal.hpp"
#include <vk_mem_alloc.h>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    namespace
    {
        [[noreturn]] void Unsupported(const char* operation)
        {
            throw std::logic_error(std::string("Vulkan RHI operation is scheduled for a later phase: ") + operation);
        }

        [[nodiscard]] bool Has(BufferUsage value, BufferUsage flag) noexcept
        {
            return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
        }

        [[nodiscard]] bool Has(TextureUsage value, TextureUsage flag) noexcept
        {
            return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
        }

        [[nodiscard]] VkFormat ToVkFormat(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::R8Unorm: return VK_FORMAT_R8_UNORM;
            case TextureFormat::RG8Unorm: return VK_FORMAT_R8G8_UNORM;
            case TextureFormat::RGB8Unorm: return VK_FORMAT_R8G8B8_UNORM;
            case TextureFormat::RGBA8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
            case TextureFormat::RGBA8Srgb: return VK_FORMAT_R8G8B8A8_SRGB;
            case TextureFormat::BGRA8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
            case TextureFormat::BGRA8Srgb: return VK_FORMAT_B8G8R8A8_SRGB;
            case TextureFormat::R16Float: return VK_FORMAT_R16_SFLOAT;
            case TextureFormat::RG16Float: return VK_FORMAT_R16G16_SFLOAT;
            case TextureFormat::RGBA16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case TextureFormat::R32Float: return VK_FORMAT_R32_SFLOAT;
            case TextureFormat::RG32Float: return VK_FORMAT_R32G32_SFLOAT;
            case TextureFormat::RGB32Float: return VK_FORMAT_R32G32B32_SFLOAT;
            case TextureFormat::RGBA32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
            case TextureFormat::D16Unorm: return VK_FORMAT_D16_UNORM;
            case TextureFormat::D24UnormS8Uint: return VK_FORMAT_D24_UNORM_S8_UINT;
            case TextureFormat::D32Float: return VK_FORMAT_D32_SFLOAT;
            case TextureFormat::D32FloatS8Uint: return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case TextureFormat::Undefined: break;
            }
            throw std::invalid_argument("Vulkan RHI: undefined or unsupported texture format.");
        }

        [[nodiscard]] std::uint32_t BytesPerPixel(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::R8Unorm: return 1;
            case TextureFormat::RG8Unorm: return 2;
            case TextureFormat::RGB8Unorm: return 3;
            case TextureFormat::RGBA8Unorm:
            case TextureFormat::RGBA8Srgb:
            case TextureFormat::BGRA8Unorm:
            case TextureFormat::BGRA8Srgb:
            case TextureFormat::R32Float:
            case TextureFormat::D24UnormS8Uint:
            case TextureFormat::D32Float: return 4;
            case TextureFormat::R16Float: return 2;
            case TextureFormat::RG16Float: return 4;
            case TextureFormat::RG32Float:
            case TextureFormat::D32FloatS8Uint: return 8;
            case TextureFormat::RGBA16Float: return 8;
            case TextureFormat::RGB32Float: return 12;
            case TextureFormat::RGBA32Float: return 16;
            case TextureFormat::D16Unorm: return 2;
            case TextureFormat::Undefined: break;
            }
            throw std::invalid_argument("Vulkan RHI: unknown bytes-per-pixel value.");
        }

        [[nodiscard]] VkImageAspectFlags Aspect(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::D16Unorm:
            case TextureFormat::D32Float:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case TextureFormat::D24UnormS8Uint:
            case TextureFormat::D32FloatS8Uint:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            case TextureFormat::Undefined:
                throw std::invalid_argument("Vulkan RHI: an image view needs a defined format.");
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            }
        }

        [[nodiscard]] VkSampleCountFlagBits ToVkSamples(std::uint32_t count)
        {
            switch (count)
            {
            case 1: return VK_SAMPLE_COUNT_1_BIT;
            case 2: return VK_SAMPLE_COUNT_2_BIT;
            case 4: return VK_SAMPLE_COUNT_4_BIT;
            case 8: return VK_SAMPLE_COUNT_8_BIT;
            case 16: return VK_SAMPLE_COUNT_16_BIT;
            case 32: return VK_SAMPLE_COUNT_32_BIT;
            case 64: return VK_SAMPLE_COUNT_64_BIT;
            default: throw std::invalid_argument("Vulkan RHI: invalid texture sample count.");
            }
        }

        [[nodiscard]] VkBufferUsageFlags ToVkBufferUsage(BufferUsage usage)
        {
            VkBufferUsageFlags result = 0;
            if (Has(usage, BufferUsage::Vertex)) result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            if (Has(usage, BufferUsage::Index)) result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            if (Has(usage, BufferUsage::Uniform)) result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            if (Has(usage, BufferUsage::Storage)) result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            if (Has(usage, BufferUsage::TransferSrc)) result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            if (Has(usage, BufferUsage::TransferDst)) result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            return result;
        }

        [[nodiscard]] VkImageUsageFlags ToVkImageUsage(TextureUsage usage)
        {
            VkImageUsageFlags result = 0;
            if (Has(usage, TextureUsage::Sampled)) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
            if (Has(usage, TextureUsage::Storage)) result |= VK_IMAGE_USAGE_STORAGE_BIT;
            if (Has(usage, TextureUsage::ColorAttachment)) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (Has(usage, TextureUsage::DepthStencilAttachment))
                result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            if (Has(usage, TextureUsage::TransferSrc)) result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            if (Has(usage, TextureUsage::TransferDst)) result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            return result;
        }

        struct StateMapping final
        {
            VkImageLayout Layout = VK_IMAGE_LAYOUT_GENERAL;
            VkPipelineStageFlags2 Stages = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2 Access = VK_ACCESS_2_NONE;
        };

        // All Vulkan resource-state to synchronization2 mappings live here.
        [[nodiscard]] StateMapping ToVkState(ResourceState state, bool image)
        {
            if (!IsValidResourceState(state))
                throw std::invalid_argument("Vulkan RHI: invalid resource-state combination.");
            if (state == ResourceState::Undefined)
                return {VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE};
            if (state == ResourceState::Present)
            {
                if (!image) throw std::invalid_argument("Vulkan RHI: Present is valid only for images.");
                return {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE};
            }

            StateMapping result{};
            const auto has = [state](ResourceState flag) { return HasAny(state, flag); };
            constexpr ResourceState bufferOnly = ResourceState::VertexBuffer
                | ResourceState::IndexBuffer | ResourceState::ConstantBuffer;
            constexpr ResourceState imageOnly = ResourceState::ColorAttachment
                | ResourceState::DepthStencilRead | ResourceState::DepthStencilWrite;
            if ((image && HasAny(state, bufferOnly)) || (!image && HasAny(state, imageOnly)))
                throw std::invalid_argument("Vulkan RHI: resource state does not apply to this resource type.");
            if (has(ResourceState::Common))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                result.Access |= VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            }
            if (has(ResourceState::VertexBuffer))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
                result.Access |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
            }
            if (has(ResourceState::IndexBuffer))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
                result.Access |= VK_ACCESS_2_INDEX_READ_BIT;
            }
            if (has(ResourceState::ConstantBuffer))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                result.Access |= VK_ACCESS_2_UNIFORM_READ_BIT;
            }
            if (has(ResourceState::ShaderRead))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                result.Access |= image ? (VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
                    | VK_ACCESS_2_SHADER_STORAGE_READ_BIT) : VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
            if (has(ResourceState::ShaderWrite))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                result.Access |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            }
            if (has(ResourceState::ColorAttachment))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                result.Access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                result.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }
            if (has(ResourceState::DepthStencilRead))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                result.Access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                result.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
            if (has(ResourceState::DepthStencilWrite))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                result.Access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                result.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            if (has(ResourceState::CopySrc))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                result.Access |= VK_ACCESS_2_TRANSFER_READ_BIT;
            }
            if (has(ResourceState::CopyDst))
            {
                result.Stages |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                result.Access |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
            }

            // A single read-only state gets the specialized layout. Combined
            // read states use GENERAL so all access masks remain valid.
            if (image && state == ResourceState::ShaderRead)
                result.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            else if (image && state == ResourceState::CopySrc)
                result.Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            else if (image && state == ResourceState::CopyDst)
                result.Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            else if (image && state != ResourceState::ColorAttachment
                && state != ResourceState::DepthStencilRead
                && state != ResourceState::DepthStencilWrite
                && state != ResourceState::Common)
            {
                result.Layout = VK_IMAGE_LAYOUT_GENERAL;
            }
            if (result.Stages == VK_PIPELINE_STAGE_2_NONE)
                throw std::invalid_argument("Vulkan RHI: resource state has no pipeline stage mapping.");
            return result;
        }

        [[nodiscard]] VmaAllocationCreateInfo ToVmaAllocation(MemoryUsage usage)
        {
            VmaAllocationCreateInfo result{};
            result.usage = VMA_MEMORY_USAGE_AUTO;
            if (usage == MemoryUsage::CpuToGpu)
            {
                result.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }
            else if (usage == MemoryUsage::GpuToCpu)
            {
                result.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            }
            return result;
        }

        [[nodiscard]] VkBufferImageCopy ToVkBufferImageCopy(
            const BufferDesc& buffer, const TextureDesc& texture, const BufferTextureCopy& region)
        {
            if (texture.sampleCount != 1 || region.mipLevel >= texture.mipLevels
                || region.arrayLayer >= texture.arrayLayers)
                throw std::invalid_argument("Vulkan RHI: invalid image-copy subresource or buffer offset.");
            if (texture.depth > 1 && region.arrayLayer != 0)
                throw std::invalid_argument("Vulkan RHI: a 3D image has no array layers.");
            if ((texture.depth == 1 && (region.z != 0 || region.depth != 1))
                || (texture.depth > 1 && region.depth == 0))
                throw std::invalid_argument("Vulkan RHI: image-copy depth does not match the image type.");

            const auto mipDimension = [level = region.mipLevel](std::uint32_t size)
            {
                return level >= 32 ? 1U : std::max(1U, size >> level);
            };
            const std::uint32_t mipWidth = mipDimension(texture.width);
            const std::uint32_t mipHeight = mipDimension(texture.height);
            const std::uint32_t mipDepth = texture.depth > 1 ? mipDimension(texture.depth) : 1U;
            if (region.width == 0 || region.height == 0 || region.depth == 0
                || region.x > mipWidth || region.width > mipWidth - region.x
                || region.y > mipHeight || region.height > mipHeight - region.y
                || region.z > mipDepth || region.depth > mipDepth - region.z)
                throw std::out_of_range("Vulkan RHI: image-copy region is outside the selected mip.");

            const VkImageAspectFlags available = Aspect(texture.format);
            VkImageAspectFlags selected = available == VK_IMAGE_ASPECT_COLOR_BIT
                ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
            switch (region.aspect)
            {
            case TextureAspect::Automatic: break;
            case TextureAspect::Color: selected = VK_IMAGE_ASPECT_COLOR_BIT; break;
            case TextureAspect::Depth: selected = VK_IMAGE_ASPECT_DEPTH_BIT; break;
            case TextureAspect::Stencil: selected = VK_IMAGE_ASPECT_STENCIL_BIT; break;
            default: throw std::invalid_argument("Vulkan RHI: invalid copy aspect.");
            }
            if ((available & selected) == 0)
                throw std::invalid_argument("Vulkan RHI: copy aspect does not exist in the image.");
            const std::uint64_t pixelSize = selected == VK_IMAGE_ASPECT_STENCIL_BIT ? 1
                : (texture.format == TextureFormat::D32FloatS8Uint ? 4 : BytesPerPixel(texture.format));
            const std::uint64_t offsetAlignment = selected == VK_IMAGE_ASPECT_COLOR_BIT ? pixelSize : 4;
            if (region.bufferOffset % offsetAlignment != 0)
                throw std::invalid_argument("Vulkan RHI: image-copy buffer offset has invalid texel/aspect alignment.");
            const std::uint64_t tightRowBytes = static_cast<std::uint64_t>(region.width) * pixelSize;
            const std::uint64_t rowBytes = region.bytesPerRow == 0 ? tightRowBytes : region.bytesPerRow;
            const std::uint64_t rowsPerImage = region.rowsPerImage == 0
                ? region.height : region.rowsPerImage;
            if (rowBytes < tightRowBytes || rowBytes % pixelSize != 0
                || rowsPerImage < region.height)
                throw std::invalid_argument("Vulkan RHI: image-copy row pitch or height is too small.");

            constexpr std::uint64_t maxSize = std::numeric_limits<std::uint64_t>::max();
            if (rowBytes > maxSize / rowsPerImage)
                throw std::out_of_range("Vulkan RHI: image-copy row pitch overflows.");
            const std::uint64_t imageStride = rowBytes * rowsPerImage;
            if (region.depth - 1 > maxSize / imageStride)
                throw std::out_of_range("Vulkan RHI: image-copy depth stride overflows.");
            std::uint64_t required = static_cast<std::uint64_t>(region.depth - 1) * imageStride;
            const std::uint64_t lastRow = static_cast<std::uint64_t>(region.height - 1) * rowBytes;
            if (required > maxSize - lastRow || required + lastRow > maxSize - tightRowBytes)
                throw std::out_of_range("Vulkan RHI: image-copy byte range overflows.");
            required += lastRow + tightRowBytes;
            if (region.bufferOffset > buffer.size || required > buffer.size - region.bufferOffset)
                throw std::out_of_range("Vulkan RHI: image-copy byte range is outside the buffer.");

            VkBufferImageCopy copy{};
            copy.bufferOffset = region.bufferOffset;
            copy.bufferRowLength = region.bytesPerRow == 0 ? 0
                : static_cast<std::uint32_t>(rowBytes / pixelSize);
            copy.bufferImageHeight = region.rowsPerImage;
            copy.imageSubresource.aspectMask = selected;
            copy.imageSubresource.mipLevel = region.mipLevel;
            copy.imageSubresource.baseArrayLayer = region.arrayLayer;
            copy.imageSubresource.layerCount = 1;
            copy.imageOffset = {static_cast<std::int32_t>(region.x),
                static_cast<std::int32_t>(region.y), static_cast<std::int32_t>(region.z)};
            copy.imageExtent = {region.width, region.height, region.depth};
            return copy;
        }
    }

        class VulkanDeviceState;

        class VulkanBuffer final : public Buffer
        {
        public:
            VulkanBuffer(std::shared_ptr<VulkanDeviceState> state, const BufferDesc& desc);
            ~VulkanBuffer() override;
            [[nodiscard]] const BufferDesc& Desc() const noexcept override { return _desc; }

            [[nodiscard]] VkBuffer Native() const noexcept { return _buffer; }
            [[nodiscard]] VmaAllocation Allocation() const noexcept { return _allocation; }
            [[nodiscard]] ResourceState State() const noexcept { return _state; }
            void State(ResourceState value) noexcept { _state = value; }
            [[nodiscard]] const std::shared_ptr<VulkanDeviceState>& DeviceState() const noexcept
            {
                return _device;
            }

        private:
            std::shared_ptr<VulkanDeviceState> _device;
            BufferDesc _desc{};
            VkBuffer _buffer = VK_NULL_HANDLE;
            VmaAllocation _allocation = VK_NULL_HANDLE;
            ResourceState _state = ResourceState::Undefined;
        };

        class VulkanTexture;
        class VulkanTextureView;

        class VulkanSampler final : public Sampler
        {
        public:
            VulkanSampler(std::shared_ptr<VulkanDeviceState> state, const SamplerDesc& desc);
            ~VulkanSampler() override;
            [[nodiscard]] const SamplerDesc& Desc() const noexcept override { return _desc; }
            [[nodiscard]] VkSampler Native() const noexcept { return _sampler; }

        private:
            std::shared_ptr<VulkanDeviceState> _device;
            SamplerDesc _desc{};
            VkSampler _sampler = VK_NULL_HANDLE;
        };

        class VulkanTexture final : public Texture
        {
        public:
            VulkanTexture(std::shared_ptr<VulkanDeviceState> state,
                const TextureDesc& desc, TextureHandle handle);
            ~VulkanTexture() override;
            [[nodiscard]] const TextureDesc& Desc() const noexcept override { return _desc; }
            [[nodiscard]] TextureHandle Handle() const noexcept override { return _handle; }
            [[nodiscard]] VkImage Native() const noexcept { return _image; }
            [[nodiscard]] ResourceState State() const noexcept { return _state; }
            void State(ResourceState value) noexcept { _state = value; }
            [[nodiscard]] const std::shared_ptr<VulkanDeviceState>& DeviceState() const noexcept
            {
                return _device;
            }
            void Resize(std::uint32_t width, std::uint32_t height);
            void Register(VulkanTextureView& view);
            void Unregister(VulkanTextureView& view) noexcept;
            void RecreateViews();

        private:
            void CreateImage();
            void DestroyImage() noexcept;
            std::shared_ptr<VulkanDeviceState> _device;
            TextureDesc _desc{};
            TextureHandle _handle{};
            VkImage _image = VK_NULL_HANDLE;
            VmaAllocation _allocation = VK_NULL_HANDLE;
            ResourceState _state = ResourceState::Undefined;
            std::vector<VulkanTextureView*> _views{};
        };

        class VulkanTextureView final : public TextureView
        {
        public:
            VulkanTextureView(VulkanTexture& texture, const TextureViewDesc& desc);
            ~VulkanTextureView() override;
            [[nodiscard]] const TextureViewDesc& Desc() const noexcept override { return _desc; }
            [[nodiscard]] const Texture& TextureResource() const noexcept override { return _texture; }
            [[nodiscard]] VkImageView Native() const noexcept { return _view; }
            void CreateNative();
            void DestroyNative() noexcept;

        private:
            VulkanTexture& _texture;
            TextureViewDesc _desc{};
            VkImageView _view = VK_NULL_HANDLE;
        };

        class VulkanDeviceState final
        {
        public:
            explicit VulkanDeviceState(Context& context) : ContextPointer(&context)
            {
                auto& vk = *context._impl;
                VmaVulkanFunctions functions{};
                functions.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                    glfwGetInstanceProcAddress);
                functions.vkGetDeviceProcAddr = vk.vkGetDeviceProcAddr;
                VmaAllocatorCreateInfo create{};
                create.instance = vk.instance;
                create.physicalDevice = vk.physical;
                create.device = vk.device;
                create.vulkanApiVersion = VK_API_VERSION_1_3;
                create.pVulkanFunctions = &functions;
                Check(vmaCreateAllocator(&create, &Allocator), "vmaCreateAllocator");
            }

            ~VulkanDeviceState()
            {
                if (Allocator != VK_NULL_HANDLE)
                {
                    ContextPointer->WaitIdle();
                    vmaDestroyAllocator(Allocator);
                    Allocator = VK_NULL_HANDLE;
                }
            }

            Context* ContextPointer = nullptr;
            VmaAllocator Allocator = VK_NULL_HANDLE;
            std::atomic<std::uint32_t> Buffers{0};
            std::atomic<std::uint32_t> Textures{0};
            std::atomic<std::uint64_t> CurrentFrame{0};
            std::atomic<std::uint64_t> CompletedFrame{0};
            std::mutex TextureMutex{};
            std::unordered_map<std::int32_t, VulkanTexture*> TexturesByHandle{};
            std::int32_t NextTextureHandle = 1;
        };

        VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanDeviceState> state, const BufferDesc& desc)
            : _device(std::move(state)), _desc(desc), _state(ResourceState::Undefined)
        {
            if (_desc.size == 0 || _desc.usage == BufferUsage::None)
                throw std::invalid_argument("Vulkan RHI: buffers need a nonzero size and usage.");
            VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            create.size = _desc.size;
            create.usage = ToVkBufferUsage(_desc.usage);
            create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            const VmaAllocationCreateInfo allocation = ToVmaAllocation(_desc.memoryUsage);
            Check(vmaCreateBuffer(_device->Allocator, &create, &allocation,
                &_buffer, &_allocation, nullptr), "vmaCreateBuffer");
            ++_device->Buffers;
        }

        VulkanBuffer::~VulkanBuffer()
        {
            if (_buffer != VK_NULL_HANDLE)
            {
                vmaDestroyBuffer(_device->Allocator, _buffer, _allocation);
                _buffer = VK_NULL_HANDLE;
                _allocation = VK_NULL_HANDLE;
                --_device->Buffers;
            }
        }

        VulkanSampler::VulkanSampler(std::shared_ptr<VulkanDeviceState> state, const SamplerDesc& desc)
            : _device(std::move(state)), _desc(desc)
        {
            auto& vk = *_device->ContextPointer->_impl;
            const auto filter = [](Filter value)
            {
                return value == Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
            };
            const auto address = [](SamplerAddressMode value)
            {
                switch (value)
                {
                case SamplerAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
                case SamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                case SamplerAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                case SamplerAddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                }
                throw std::invalid_argument("Vulkan RHI: invalid sampler address mode.");
            };
            VkSamplerCreateInfo create{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            create.magFilter = filter(desc.magFilter);
            create.minFilter = filter(desc.minFilter);
            create.mipmapMode = desc.mipFilter == Filter::Nearest
                ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
            create.addressModeU = address(desc.addressU);
            create.addressModeV = address(desc.addressV);
            create.addressModeW = address(desc.addressW);
            create.minLod = desc.minLod;
            create.maxLod = desc.maxLod;
            create.maxAnisotropy = 1.0F;
            if (desc.maxAnisotropy > 1.0F)
            {
                if (!_device->ContextPointer->Caps().supportsAnisotropy)
                    throw std::runtime_error("Vulkan RHI: anisotropic filtering is unavailable.");
                create.anisotropyEnable = VK_TRUE;
                create.maxAnisotropy = std::min(desc.maxAnisotropy,
                    _device->ContextPointer->Caps().maxSamplerAnisotropy);
            }
            switch (desc.borderColor)
            {
            case BorderColor::TransparentBlack:
                create.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK; break;
            case BorderColor::OpaqueBlack:
                create.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK; break;
            case BorderColor::OpaqueWhite:
                create.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; break;
            }
            Check(vk.vkCreateSampler(vk.device, &create, nullptr, &_sampler), "vkCreateSampler");
            vk.Name(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<std::uint64_t>(_sampler), "RHI sampler");
        }

        VulkanSampler::~VulkanSampler()
        {
            if (_sampler != VK_NULL_HANDLE)
                _device->ContextPointer->_impl->vkDestroySampler(
                    _device->ContextPointer->_impl->device, _sampler, nullptr);
        }

        VulkanTexture::VulkanTexture(std::shared_ptr<VulkanDeviceState> state,
            const TextureDesc& desc, TextureHandle handle)
            : _device(std::move(state)), _desc(desc), _handle(handle), _state(ResourceState::Undefined)
        {
            if (_desc.width == 0 || _desc.height == 0 || _desc.depth == 0
                || _desc.mipLevels == 0 || _desc.arrayLayers == 0 || _desc.usage == TextureUsage::None)
                throw std::invalid_argument("Vulkan RHI: textures need nonzero extents, subresources and usage.");
            if (_desc.depth > 1 && _desc.arrayLayers != 1)
                throw std::invalid_argument("Vulkan RHI: 3D texture arrays are not supported.");
            if (_desc.memoryUsage != MemoryUsage::GpuOnly)
                throw std::invalid_argument("Vulkan RHI: images must use GPU-only memory; use a buffer for host access.");
            CreateImage();
            ++_device->Textures;
        }

        void VulkanTexture::CreateImage()
        {
            auto& vk = *_device->ContextPointer->_impl;
            const Capabilities& caps = _device->ContextPointer->Caps();
            if (_desc.width > caps.maxTexture2DDimension || _desc.height > caps.maxTexture2DDimension
                || _desc.arrayLayers > caps.maxTextureArrayLayers)
                throw std::out_of_range("Vulkan RHI: texture extent or layer count exceeds device limits.");
            std::uint32_t maxExtent = std::max({_desc.width, _desc.height, _desc.depth});
            std::uint32_t maxMipLevels = 1;
            while (maxExtent > 1)
            {
                maxExtent >>= 1;
                ++maxMipLevels;
            }
            if (_desc.mipLevels > maxMipLevels)
                throw std::out_of_range("Vulkan RHI: texture mip count exceeds its extent.");
            if (_desc.sampleCount != 1
                && (Has(_desc.usage, TextureUsage::TransferSrc)
                    || Has(_desc.usage, TextureUsage::TransferDst)))
                throw std::invalid_argument("Vulkan RHI: multisampled images cannot be transfer resources.");
            const VkFormat format = ToVkFormat(_desc.format);
            VkImageFormatProperties imageProperties{};
            Check(vk.vkGetPhysicalDeviceImageFormatProperties(vk.physical, format,
                _desc.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL, ToVkImageUsage(_desc.usage), 0, &imageProperties),
                "vkGetPhysicalDeviceImageFormatProperties");
            if (_desc.width > imageProperties.maxExtent.width
                || _desc.height > imageProperties.maxExtent.height
                || _desc.depth > imageProperties.maxExtent.depth
                || _desc.arrayLayers > imageProperties.maxArrayLayers
                || _desc.mipLevels > imageProperties.maxMipLevels
                || (imageProperties.sampleCounts & ToVkSamples(_desc.sampleCount)) == 0)
                throw std::out_of_range("Vulkan RHI: image description exceeds format/type limits.");
            VkFormatProperties properties{};
            vk.vkGetPhysicalDeviceFormatProperties(vk.physical, format, &properties);
            VkFormatFeatureFlags required = 0;
            if (Has(_desc.usage, TextureUsage::Sampled)) required |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            if (Has(_desc.usage, TextureUsage::Storage)) required |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
            if (Has(_desc.usage, TextureUsage::ColorAttachment)) required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
            if (Has(_desc.usage, TextureUsage::DepthStencilAttachment))
                required |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            if ((properties.optimalTilingFeatures & required) != required)
                throw std::runtime_error("Vulkan RHI: the selected texture format lacks a requested image usage.");

            VkImageCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            create.imageType = _desc.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
            create.format = format;
            create.extent = {_desc.width, _desc.height, _desc.depth};
            create.mipLevels = _desc.mipLevels;
            create.arrayLayers = _desc.arrayLayers;
            create.samples = ToVkSamples(_desc.sampleCount);
            create.tiling = VK_IMAGE_TILING_OPTIMAL;
            create.usage = ToVkImageUsage(_desc.usage);
            create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            create.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            const VmaAllocationCreateInfo allocation = ToVmaAllocation(_desc.memoryUsage);
            Check(vmaCreateImage(_device->Allocator, &create, &allocation,
                &_image, &_allocation, nullptr), "vmaCreateImage");
            vk.Name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(_image), "RHI texture");
        }

        void VulkanTexture::DestroyImage() noexcept
        {
            if (_image != VK_NULL_HANDLE)
            {
                vmaDestroyImage(_device->Allocator, _image, _allocation);
                _image = VK_NULL_HANDLE;
                _allocation = VK_NULL_HANDLE;
            }
        }

        VulkanTexture::~VulkanTexture()
        {
            auto& vk = *_device->ContextPointer->_impl;
            vk.vkDeviceWaitIdle(vk.device);
            for (VulkanTextureView* view : _views)
                view->DestroyNative();
            _views.clear();
            DestroyImage();
            {
                std::lock_guard lock(_device->TextureMutex);
                const auto found = _device->TexturesByHandle.find(_handle.value);
                if (found != _device->TexturesByHandle.end() && found->second == this)
                    _device->TexturesByHandle.erase(found);
            }
            --_device->Textures;
        }

        void VulkanTexture::Resize(std::uint32_t width, std::uint32_t height)
        {
            if (width == 0 || height == 0)
                throw std::invalid_argument("Vulkan RHI: a texture extent cannot be zero.");
            auto& vk = *_device->ContextPointer->_impl;
            Check(vk.vkDeviceWaitIdle(vk.device), "vkDeviceWaitIdle before image resize");
            for (VulkanTextureView* view : _views)
                view->DestroyNative();
            DestroyImage();
            _desc.width = width;
            _desc.height = height;
            _state = ResourceState::Undefined;
            try
            {
                CreateImage();
                RecreateViews();
            }
            catch (...)
            {
                for (VulkanTextureView* view : _views)
                    view->DestroyNative();
                DestroyImage();
                throw;
            }
        }

        void VulkanTexture::Register(VulkanTextureView& view)
        {
            _views.push_back(&view);
            try { view.CreateNative(); }
            catch (...)
            {
                _views.pop_back();
                throw;
            }
        }

        void VulkanTexture::Unregister(VulkanTextureView& view) noexcept
        {
            const auto found = std::find(_views.begin(), _views.end(), &view);
            if (found != _views.end()) _views.erase(found);
        }

        void VulkanTexture::RecreateViews()
        {
            for (VulkanTextureView* view : _views)
                view->CreateNative();
        }

        VulkanTextureView::VulkanTextureView(VulkanTexture& texture, const TextureViewDesc& desc)
            : _texture(texture), _desc(desc)
        {
            if (_desc.format == TextureFormat::Undefined)
                _desc.format = texture.Desc().format;
            if (_desc.format != texture.Desc().format)
                throw std::invalid_argument("Vulkan RHI: texture view format reinterpretation is unsupported.");
            if (_desc.mipLevelCount == 0 || _desc.baseMipLevel >= texture.Desc().mipLevels
                || _desc.mipLevelCount > texture.Desc().mipLevels - _desc.baseMipLevel
                || _desc.arrayLayerCount == 0 || _desc.baseArrayLayer >= texture.Desc().arrayLayers
                || _desc.arrayLayerCount > texture.Desc().arrayLayers - _desc.baseArrayLayer)
                throw std::invalid_argument("Vulkan RHI: texture view range is outside the image.");
            texture.Register(*this);
        }

        VulkanTextureView::~VulkanTextureView()
        {
            DestroyNative();
            _texture.Unregister(*this);
        }

        void VulkanTextureView::CreateNative()
        {
            if (_view != VK_NULL_HANDLE) return;
            auto& vk = *_texture.DeviceState()->ContextPointer->_impl;
            VkImageViewCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            create.image = _texture.Native();
            create.viewType = _texture.Desc().depth > 1 ? VK_IMAGE_VIEW_TYPE_3D
                : (_texture.Desc().arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
            create.format = ToVkFormat(_desc.format);
            create.subresourceRange.aspectMask = Aspect(_desc.format);
            create.subresourceRange.baseMipLevel = _desc.baseMipLevel;
            create.subresourceRange.levelCount = _desc.mipLevelCount;
            create.subresourceRange.baseArrayLayer = _desc.baseArrayLayer;
            create.subresourceRange.layerCount = _desc.arrayLayerCount;
            Check(vk.vkCreateImageView(vk.device, &create, nullptr, &_view), "vkCreateImageView");
            vk.Name(VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<std::uint64_t>(_view), "RHI texture view");
        }

        void VulkanTextureView::DestroyNative() noexcept
        {
            if (_view != VK_NULL_HANDLE)
            {
                auto& vk = *_texture.DeviceState()->ContextPointer->_impl;
                vk.vkDestroyImageView(vk.device, _view, nullptr);
                _view = VK_NULL_HANDLE;
            }
        }

        class VulkanCommandList final : public CommandList
        {
        public:
            explicit VulkanCommandList(std::shared_ptr<VulkanDeviceState> state);
            ~VulkanCommandList() override;

            void Begin() override;
            void End() override;
            void BeginRendering(const RenderingInfo&) override { Unsupported("BeginRendering"); }
            void EndRendering() override { Unsupported("EndRendering"); }
            void SetPipeline(const GraphicsPipeline&) override { Unsupported("SetPipeline"); }
            void SetViewport(const Viewport&) override { Unsupported("SetViewport"); }
            void SetScissor(const Scissor&) override { Unsupported("SetScissor"); }
            void SetVertexBuffer(std::uint32_t, const Buffer&, std::uint64_t) override
            {
                Unsupported("SetVertexBuffer");
            }
            void SetIndexBuffer(const Buffer&, IndexType, std::uint64_t) override
            {
                Unsupported("SetIndexBuffer");
            }
            void SetBindingSet(std::uint32_t, const BindingSet&) override { Unsupported("SetBindingSet"); }
            void SetStencilReference(std::uint32_t) override { Unsupported("SetStencilReference"); }
            void Draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) override
            {
                Unsupported("Draw");
            }
            void DrawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) override
            {
                Unsupported("DrawIndexed");
            }
            void CopyBuffer(const Buffer& source, std::uint64_t sourceOffset,
                Buffer& destination, std::uint64_t destinationOffset, std::uint64_t size) override;
            void CopyBufferToTexture(
                const Buffer& source, Texture& destination, const BufferTextureCopy& region) override;
            void CopyTextureToBuffer(
                const Texture& source, Buffer& destination, const BufferTextureCopy& region) override;
            void Transition(Buffer& resource, ResourceState before, ResourceState after) override;
            void Transition(Texture& resource, ResourceState before, ResourceState after) override;
            void BindSampledTexture(std::uint32_t, const Texture*, const Sampler*) override
            {
                Unsupported("BindSampledTexture");
            }
            void ReadColor(const RenderingInfo&, std::uint32_t, std::uint32_t,
                std::uint32_t, std::uint32_t, TextureFormat, void*) override
            {
                Unsupported("ReadColor");
            }
            void CopyColorAttachmentToTexture(Texture&, std::uint32_t, std::uint32_t) override
            {
                Unsupported("CopyColorAttachmentToTexture");
            }

        private:
            void RequireRecording() const;
            std::shared_ptr<VulkanDeviceState> _device;
            VkCommandPool _pool = VK_NULL_HANDLE;
            VkCommandBuffer _commandBuffer = VK_NULL_HANDLE;
            VkFence _fence = VK_NULL_HANDLE;
            bool _recording = false;
        };

        VulkanCommandList::VulkanCommandList(std::shared_ptr<VulkanDeviceState> state)
            : _device(std::move(state))
        {
            auto& vk = *_device->ContextPointer->_impl;
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool.queueFamilyIndex = vk.graphicsFamily;
            Check(vk.vkCreateCommandPool(vk.device, &pool, nullptr, &_pool), "vkCreateCommandPool");
            try
            {
                VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocate.commandPool = _pool;
                allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocate.commandBufferCount = 1;
                Check(vk.vkAllocateCommandBuffers(vk.device, &allocate, &_commandBuffer),
                    "vkAllocateCommandBuffers");
                VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                Check(vk.vkCreateFence(vk.device, &fence, nullptr, &_fence), "vkCreateFence");
                vk.Name(VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<std::uint64_t>(_commandBuffer),
                    "RHI resource command buffer");
            }
            catch (...)
            {
                if (_commandBuffer) vk.vkFreeCommandBuffers(vk.device, _pool, 1, &_commandBuffer);
                vk.vkDestroyCommandPool(vk.device, _pool, nullptr);
                _pool = VK_NULL_HANDLE;
                throw;
            }
        }

        VulkanCommandList::~VulkanCommandList()
        {
            if (_device->ContextPointer == nullptr) return;
            auto& vk = *_device->ContextPointer->_impl;
            if (vk.device == VK_NULL_HANDLE) return;
            if (_fence) vk.vkDestroyFence(vk.device, _fence, nullptr);
            if (_pool) vk.vkDestroyCommandPool(vk.device, _pool, nullptr);
        }

        void VulkanCommandList::Begin()
        {
            if (_recording) throw std::logic_error("Vulkan RHI: command list is already recording.");
            auto& vk = *_device->ContextPointer->_impl;
            Check(vk.vkResetCommandPool(vk.device, _pool, 0), "vkResetCommandPool");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Check(vk.vkBeginCommandBuffer(_commandBuffer, &begin), "vkBeginCommandBuffer");
            _recording = true;
        }

        void VulkanCommandList::End()
        {
            RequireRecording();
            auto& vk = *_device->ContextPointer->_impl;
            Check(vk.vkEndCommandBuffer(_commandBuffer), "vkEndCommandBuffer");
            VkCommandBufferSubmitInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
            command.commandBuffer = _commandBuffer;
            VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
            submit.commandBufferInfoCount = 1;
            submit.pCommandBufferInfos = &command;
            Check(vk.vkQueueSubmit2(vk.graphics, 1, &submit, _fence), "vkQueueSubmit2");
            Check(vk.vkWaitForFences(vk.device, 1, &_fence, VK_TRUE,
                std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences");
            Check(vk.vkResetFences(vk.device, 1, &_fence), "vkResetFences");
            _recording = false;
            _device->CompletedFrame.store(_device->CurrentFrame.load());
        }

        void VulkanCommandList::RequireRecording() const
        {
            if (!_recording) throw std::logic_error("Vulkan RHI: command list is not recording.");
        }

        void VulkanCommandList::CopyBuffer(const Buffer& source, std::uint64_t sourceOffset,
            Buffer& destination, std::uint64_t destinationOffset, std::uint64_t size)
        {
            RequireRecording();
            auto& src = static_cast<const VulkanBuffer&>(source);
            auto& dst = static_cast<VulkanBuffer&>(destination);
            if (src.DeviceState() != _device || dst.DeviceState() != _device)
                throw std::invalid_argument("Vulkan RHI: buffers belong to another device.");
            if (!Has(src.Desc().usage, BufferUsage::TransferSrc)
                || !Has(dst.Desc().usage, BufferUsage::TransferDst))
                throw std::invalid_argument("Vulkan RHI: buffer copies need TransferSrc and TransferDst usage.");
            if (src.State() != ResourceState::CopySrc || dst.State() != ResourceState::CopyDst)
                throw std::invalid_argument("Vulkan RHI: buffer copies require CopySrc and CopyDst states.");
            if (size == 0 || sourceOffset > src.Desc().size || size > src.Desc().size - sourceOffset
                || destinationOffset > dst.Desc().size || size > dst.Desc().size - destinationOffset)
                throw std::out_of_range("Vulkan RHI: buffer copy range is outside the resource.");
            if (&source == &destination && sourceOffset < destinationOffset + size
                && destinationOffset < sourceOffset + size)
                throw std::invalid_argument("Vulkan RHI: source and destination buffer copy ranges overlap.");
            VkBufferCopy region{};
            region.srcOffset = sourceOffset;
            region.dstOffset = destinationOffset;
            region.size = size;
            _device->ContextPointer->_impl->vkCmdCopyBuffer(
                _commandBuffer, src.Native(), dst.Native(), 1, &region);
        }

        void VulkanCommandList::CopyBufferToTexture(
            const Buffer& source, Texture& destination, const BufferTextureCopy& region)
        {
            RequireRecording();
            auto& src = static_cast<const VulkanBuffer&>(source);
            auto& dst = static_cast<VulkanTexture&>(destination);
            if (src.DeviceState() != _device || dst.DeviceState() != _device)
                throw std::invalid_argument("Vulkan RHI: copy resources belong to another device.");
            if (!Has(src.Desc().usage, BufferUsage::TransferSrc)
                || !Has(dst.Desc().usage, TextureUsage::TransferDst))
                throw std::invalid_argument("Vulkan RHI: image upload needs TransferSrc and TransferDst usage.");
            if (src.State() != ResourceState::CopySrc || dst.State() != ResourceState::CopyDst)
                throw std::invalid_argument("Vulkan RHI: image uploads require CopySrc and CopyDst states.");
            const VkBufferImageCopy copy = ToVkBufferImageCopy(src.Desc(), dst.Desc(), region);
            _device->ContextPointer->_impl->vkCmdCopyBufferToImage(_commandBuffer,
                src.Native(), dst.Native(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        }

        void VulkanCommandList::CopyTextureToBuffer(
            const Texture& source, Buffer& destination, const BufferTextureCopy& region)
        {
            RequireRecording();
            auto& src = static_cast<const VulkanTexture&>(source);
            auto& dst = static_cast<VulkanBuffer&>(destination);
            if (src.DeviceState() != _device || dst.DeviceState() != _device)
                throw std::invalid_argument("Vulkan RHI: copy resources belong to another device.");
            if (!Has(src.Desc().usage, TextureUsage::TransferSrc)
                || !Has(dst.Desc().usage, BufferUsage::TransferDst))
                throw std::invalid_argument("Vulkan RHI: image readback needs TransferSrc and TransferDst usage.");
            if (src.State() != ResourceState::CopySrc || dst.State() != ResourceState::CopyDst)
                throw std::invalid_argument("Vulkan RHI: image readback requires CopySrc and CopyDst states.");
            const VkBufferImageCopy copy = ToVkBufferImageCopy(dst.Desc(), src.Desc(), region);
            _device->ContextPointer->_impl->vkCmdCopyImageToBuffer(_commandBuffer,
                src.Native(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.Native(), 1, &copy);
        }

        void VulkanCommandList::Transition(Buffer& resource, ResourceState before, ResourceState after)
        {
            RequireRecording();
            auto& buffer = static_cast<VulkanBuffer&>(resource);
            if (buffer.DeviceState() != _device)
                throw std::invalid_argument("Vulkan RHI: buffer belongs to another device.");
            if (buffer.State() != before || !IsValidTransition(before, after))
                throw std::invalid_argument("Vulkan RHI: buffer transition does not match its tracked state.");
            constexpr ResourceState imageOnly = ResourceState::ColorAttachment
                | ResourceState::DepthStencilRead | ResourceState::DepthStencilWrite
                | ResourceState::Present;
            if (HasAny(before | after, imageOnly))
                throw std::invalid_argument("Vulkan RHI: image-only state used for a buffer.");
            const StateMapping src = ToVkState(before, false);
            const StateMapping dst = ToVkState(after, false);
            VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
            barrier.srcStageMask = src.Stages;
            barrier.srcAccessMask = src.Access;
            barrier.dstStageMask = dst.Stages;
            barrier.dstAccessMask = dst.Access;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = buffer.Native();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dependency.bufferMemoryBarrierCount = 1;
            dependency.pBufferMemoryBarriers = &barrier;
            _device->ContextPointer->_impl->vkCmdPipelineBarrier2(_commandBuffer, &dependency);
            buffer.State(after);
        }

        void VulkanCommandList::Transition(Texture& resource, ResourceState before, ResourceState after)
        {
            RequireRecording();
            auto& texture = static_cast<VulkanTexture&>(resource);
            if (texture.DeviceState() != _device)
                throw std::invalid_argument("Vulkan RHI: texture belongs to another device.");
            if (texture.State() != before || !IsValidTransition(before, after))
                throw std::invalid_argument("Vulkan RHI: image transition does not match its tracked state.");
            constexpr ResourceState bufferOnly = ResourceState::VertexBuffer
                | ResourceState::IndexBuffer | ResourceState::ConstantBuffer;
            if (HasAny(before | after, bufferOnly))
                throw std::invalid_argument("Vulkan RHI: buffer-only state used for an image.");
            const StateMapping src = ToVkState(before, true);
            const StateMapping dst = ToVkState(after, true);
            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            barrier.srcStageMask = src.Stages;
            barrier.srcAccessMask = src.Access;
            barrier.dstStageMask = dst.Stages;
            barrier.dstAccessMask = dst.Access;
            barrier.oldLayout = src.Layout;
            barrier.newLayout = dst.Layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = texture.Native();
            barrier.subresourceRange.aspectMask = Aspect(texture.Desc().format);
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = texture.Desc().mipLevels;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = texture.Desc().arrayLayers;
            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dependency.imageMemoryBarrierCount = 1;
            dependency.pImageMemoryBarriers = &barrier;
            _device->ContextPointer->_impl->vkCmdPipelineBarrier2(_commandBuffer, &dependency);
            texture.State(after);
        }

        class VulkanGraphicsDevice final : public GraphicsDevice
        {
        public:
            explicit VulkanGraphicsDevice(Context& context) : _state(std::make_shared<VulkanDeviceState>(context)) {}
            ~VulkanGraphicsDevice() override
            {
                WaitIdle();
                _retained.clear();
            }

            [[nodiscard]] GraphicsBackend GetBackend() const noexcept override { return GraphicsBackend::Vulkan; }
            [[nodiscard]] const Capabilities& GetCapabilities() const noexcept override
            {
                return _state->ContextPointer->Caps();
            }

            [[nodiscard]] std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc) override
            {
                auto buffer = std::make_unique<VulkanBuffer>(_state, desc);
                if (desc.initialState != ResourceState::Undefined)
                {
                    auto commands = CreateCommandList();
                    commands->Begin();
                    commands->Transition(*buffer, ResourceState::Undefined, desc.initialState);
                    commands->End();
                }
                return buffer;
            }

            [[nodiscard]] std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc) override
            {
                std::int32_t handle;
                {
                    std::lock_guard lock(_state->TextureMutex);
                    handle = _state->NextTextureHandle;
                    while (_state->TexturesByHandle.contains(handle))
                        handle = handle == std::numeric_limits<std::int32_t>::max() ? 1 : handle + 1;
                    _state->NextTextureHandle = handle == std::numeric_limits<std::int32_t>::max() ? 1 : handle + 1;
                }
                auto texture = std::make_unique<VulkanTexture>(_state, desc, TextureHandle{handle});
                InitializeTextureState(*texture, desc.initialState);
                bool inserted;
                {
                    std::lock_guard lock(_state->TextureMutex);
                    inserted = _state->TexturesByHandle.emplace(handle, texture.get()).second;
                }
                if (!inserted) throw std::invalid_argument("Vulkan RHI: texture handle already live.");
                return texture;
            }

            [[nodiscard]] std::unique_ptr<Texture> CreateTexture(
                const TextureDesc& desc, TextureHandle handle) override
            {
                if (!handle) throw std::invalid_argument("Vulkan RHI: a chosen texture handle must be nonzero.");
                auto texture = std::make_unique<VulkanTexture>(_state, desc, handle);
                InitializeTextureState(*texture, desc.initialState);
                bool inserted;
                {
                    std::lock_guard lock(_state->TextureMutex);
                    inserted = _state->TexturesByHandle.emplace(handle.value, texture.get()).second;
                }
                if (!inserted) throw std::invalid_argument("Vulkan RHI: texture handle already live.");
                return texture;
            }

            [[nodiscard]] Texture* FindTexture(TextureHandle handle) noexcept override
            {
                std::lock_guard lock(_state->TextureMutex);
                const auto found = _state->TexturesByHandle.find(handle.value);
                return found == _state->TexturesByHandle.end() ? nullptr : found->second;
            }

            Texture& RetainTexture(std::unique_ptr<Texture> texture) override
            {
                if (!texture || static_cast<VulkanTexture&>(*texture).DeviceState() != _state)
                    throw std::invalid_argument("Vulkan RHI: retained texture belongs to another device.");
                Texture& retained = *texture;
                _retained.push_back(std::move(texture));
                return retained;
            }

            [[nodiscard]] std::unique_ptr<TextureView> CreateTextureView(
                Texture& texture, const TextureViewDesc& desc) override
            {
                auto& native = static_cast<VulkanTexture&>(texture);
                if (native.DeviceState() != _state)
                    throw std::invalid_argument("Vulkan RHI: texture belongs to another device.");
                return std::make_unique<VulkanTextureView>(native, desc);
            }

            [[nodiscard]] std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) override
            {
                return std::make_unique<VulkanSampler>(_state, desc);
            }

            [[nodiscard]] std::unique_ptr<Shader> CreateShader(const ShaderDesc&) override
            {
                Unsupported("CreateShader");
            }
            [[nodiscard]] std::unique_ptr<BindingLayout> CreateBindingLayout(const BindingLayoutDesc&) override
            {
                Unsupported("CreateBindingLayout");
            }
            [[nodiscard]] std::unique_ptr<BindingSet> CreateBindingSet(const BindingSetDesc&) override
            {
                Unsupported("CreateBindingSet");
            }
            [[nodiscard]] std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(
                const GraphicsPipelineDesc&) override
            {
                Unsupported("CreateGraphicsPipeline");
            }
            [[nodiscard]] std::unique_ptr<CommandList> CreateCommandList() override
            {
                return std::make_unique<VulkanCommandList>(_state);
            }

            void WriteTexture(Texture& texture, const TextureWrite& write) override
            {
                auto& image = static_cast<VulkanTexture&>(texture);
                if (image.DeviceState() != _state)
                    throw std::invalid_argument("Vulkan RHI: texture belongs to another device.");
                if (write.data == nullptr || write.width == 0 || write.height == 0
                    || image.Desc().depth != 1 || image.Desc().arrayLayers != 1
                    || image.Desc().sampleCount != 1 || write.format != image.Desc().format)
                    throw std::invalid_argument("Vulkan RHI: texture upload format or extent is invalid.");
                if (write.width != image.Desc().width || write.height != image.Desc().height)
                    image.Resize(write.width, write.height);
                if (!Has(image.Desc().usage, TextureUsage::TransferDst))
                    throw std::invalid_argument("Vulkan RHI: texture upload requires TransferDst usage.");
                const std::size_t dataSize = static_cast<std::size_t>(write.width)
                    * write.height * BytesPerPixel(write.format);
                BufferDesc stagingDesc{};
                stagingDesc.size = dataSize;
                stagingDesc.usage = BufferUsage::TransferSrc;
                stagingDesc.memoryUsage = MemoryUsage::CpuToGpu;
                auto staging = CreateBuffer(stagingDesc);
                WriteBuffer(*staging, 0, std::span(
                    static_cast<const std::byte*>(write.data), dataSize));
                auto commands = CreateCommandList();
                commands->Begin();
                const ResourceState previous = image.State();
                if (previous != ResourceState::CopyDst)
                    commands->Transition(image, previous, ResourceState::CopyDst);
                auto& stagingNative = static_cast<VulkanBuffer&>(*staging);
                commands->Transition(stagingNative, stagingNative.State(), ResourceState::CopySrc);
                BufferTextureCopy region{};
                region.width = write.width;
                region.height = write.height;
                commands->CopyBufferToTexture(*staging, image, region);
                const ResourceState finalState = previous != ResourceState::Undefined
                    ? previous
                    : (Has(image.Desc().usage, TextureUsage::Sampled) ? ResourceState::ShaderRead
                        : (Has(image.Desc().usage, TextureUsage::ColorAttachment)
                            ? ResourceState::ColorAttachment : ResourceState::Common));
                if (finalState != ResourceState::CopyDst)
                    commands->Transition(image, ResourceState::CopyDst, finalState);
                commands->End();
            }

            void WriteBuffer(Buffer& buffer, std::uint64_t offset,
                std::span<const std::byte> data) override
            {
                auto& destination = static_cast<VulkanBuffer&>(buffer);
                if (destination.DeviceState() != _state)
                    throw std::invalid_argument("Vulkan RHI: buffer belongs to another device.");
                if (data.empty() || offset > destination.Desc().size
                    || data.size() > destination.Desc().size - offset)
                    throw std::out_of_range("Vulkan RHI: buffer write range is empty or outside the resource.");
                if (destination.Desc().memoryUsage == MemoryUsage::GpuToCpu)
                    throw std::invalid_argument("Vulkan RHI: a readback buffer cannot be used as an upload destination.");

                if (destination.Desc().memoryUsage == MemoryUsage::CpuToGpu)
                {
                    void* mapped = nullptr;
                    Check(vmaMapMemory(_state->Allocator, destination.Allocation(), &mapped), "vmaMapMemory");
                    std::memcpy(static_cast<std::byte*>(mapped) + offset, data.data(), data.size());
                    Check(vmaFlushAllocation(_state->Allocator, destination.Allocation(),
                        offset, data.size()), "vmaFlushAllocation");
                    vmaUnmapMemory(_state->Allocator, destination.Allocation());
                    return;
                }
                if (!Has(destination.Desc().usage, BufferUsage::TransferDst))
                    throw std::invalid_argument("Vulkan RHI: GPU-only buffer upload requires TransferDst usage.");

                BufferDesc stagingDesc{};
                stagingDesc.size = data.size();
                stagingDesc.usage = BufferUsage::TransferSrc;
                stagingDesc.memoryUsage = MemoryUsage::CpuToGpu;
                auto staging = CreateBuffer(stagingDesc);
                auto& stagingNative = static_cast<VulkanBuffer&>(*staging);
                void* mapped = nullptr;
                Check(vmaMapMemory(_state->Allocator, stagingNative.Allocation(), &mapped), "vmaMapMemory");
                std::memcpy(mapped, data.data(), data.size());
                Check(vmaFlushAllocation(_state->Allocator, stagingNative.Allocation(),
                    0, data.size()), "vmaFlushAllocation");
                vmaUnmapMemory(_state->Allocator, stagingNative.Allocation());

                auto commands = CreateCommandList();
                commands->Begin();
                const ResourceState previous = destination.State();
                if (previous != ResourceState::CopyDst)
                    commands->Transition(destination, previous, ResourceState::CopyDst);
                commands->Transition(stagingNative, stagingNative.State(), ResourceState::CopySrc);
                commands->CopyBuffer(*staging, 0, destination, offset, data.size());
                if (previous != ResourceState::CopyDst && previous != ResourceState::Undefined)
                    commands->Transition(destination, ResourceState::CopyDst, previous);
                commands->End();
            }

            void ReadBuffer(Buffer& buffer, std::uint64_t offset, std::span<std::byte> data) override
            {
                auto& source = static_cast<VulkanBuffer&>(buffer);
                if (source.DeviceState() != _state)
                    throw std::invalid_argument("Vulkan RHI: buffer belongs to another device.");
                if (data.empty() || offset > source.Desc().size
                    || data.size() > source.Desc().size - offset)
                    throw std::out_of_range("Vulkan RHI: buffer read range is empty or outside the resource.");
                if (source.Desc().memoryUsage != MemoryUsage::GpuToCpu)
                    throw std::invalid_argument("Vulkan RHI: CPU readback requires MemoryUsage::GpuToCpu.");
                WaitIdle();
                void* mapped = nullptr;
                Check(vmaMapMemory(_state->Allocator, source.Allocation(), &mapped), "vmaMapMemory");
                Check(vmaInvalidateAllocation(_state->Allocator, source.Allocation(),
                    offset, data.size()), "vmaInvalidateAllocation");
                std::memcpy(data.data(), static_cast<const std::byte*>(mapped) + offset, data.size());
                vmaUnmapMemory(_state->Allocator, source.Allocation());
            }

            void ResizeTexture(Texture& texture, std::uint32_t width, std::uint32_t height) override
            {
                auto& image = static_cast<VulkanTexture&>(texture);
                if (image.DeviceState() != _state)
                    throw std::invalid_argument("Vulkan RHI: texture belongs to another device.");
                image.Resize(width, height);
            }

            [[nodiscard]] FrameContext BeginFrame() override
            {
                const std::uint64_t frame = ++_state->CurrentFrame;
                return FrameContext{frame, static_cast<std::uint32_t>((frame - 1) % FramesInFlight)};
            }

            void EndFrame() override
            {
                // Phase 14 resource transfers submit synchronously. Later
                // graphics phases replace this with the frame's submit fence.
                _state->CompletedFrame.store(_state->CurrentFrame.load());
            }

            void WaitIdle() override
            {
                _state->ContextPointer->WaitIdle();
                _state->CompletedFrame.store(_state->CurrentFrame.load());
            }

            [[nodiscard]] GpuResourceStatistics Statistics() const override
            {
                GpuResourceStatistics result{};
                result.Textures = _state->Textures.load();
                result.Buffers = _state->Buffers.load();
                result.CompletedFrame = _state->CompletedFrame.load();
                return result;
            }

            [[nodiscard]] std::string AdapterDescription() override
            {
                return _state->ContextPointer->DeviceName();
            }

            [[nodiscard]] std::int32_t DrainErrors() override
            {
                const unsigned now = _state->ContextPointer->ValidationErrors();
                const unsigned old = _reportedValidationErrors.exchange(now);
                return now == old ? 0 : static_cast<std::int32_t>(now - old);
            }

            [[nodiscard]] bool CanRender(const RenderingInfo& info) override
            {
                if (info.width == 0 || info.height == 0) return false;
                for (const RenderingColorAttachment& attachment : info.colorAttachments)
                {
                    if (!attachment.view) continue;
                    const auto& image = static_cast<const VulkanTexture&>(attachment.view->TextureResource());
                    if (!Has(image.Desc().usage, TextureUsage::ColorAttachment)) return false;
                }
                if (info.depthStencilAttachment && info.depthStencilAttachment->view)
                {
                    const auto& image = static_cast<const VulkanTexture&>(
                        info.depthStencilAttachment->view->TextureResource());
                    if (!Has(image.Desc().usage, TextureUsage::DepthStencilAttachment)) return false;
                }
                return true;
            }

            [[nodiscard]] std::uint32_t DepthBits(const RenderingInfo& info) override
            {
                if (!info.depthStencilAttachment || !info.depthStencilAttachment->view) return 0;
                const auto& image = static_cast<const VulkanTexture&>(
                    info.depthStencilAttachment->view->TextureResource());
                switch (image.Desc().format)
                {
                case TextureFormat::D16Unorm: return 16;
                case TextureFormat::D24UnormS8Uint: return 24;
                case TextureFormat::D32Float:
                case TextureFormat::D32FloatS8Uint: return 32;
                default: return 0;
                }
            }

        private:
            void InitializeTextureState(VulkanTexture& texture, ResourceState initialState)
            {
                if (initialState == ResourceState::Undefined) return;
                auto commands = CreateCommandList();
                commands->Begin();
                commands->Transition(texture, ResourceState::Undefined, initialState);
                commands->End();
            }

            std::shared_ptr<VulkanDeviceState> _state;
            std::vector<std::unique_ptr<Texture>> _retained{};
            std::atomic<unsigned> _reportedValidationErrors{0};
        };
    std::unique_ptr<GraphicsDevice> CreateGraphicsDevice(Context& context)
    {
        return std::make_unique<VulkanGraphicsDevice>(context);
    }
}
#else
namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    std::unique_ptr<GraphicsDevice> CreateGraphicsDevice(Context&)
    {
        throw std::runtime_error("Desktop Vulkan development support was not built.");
    }
}
#endif
