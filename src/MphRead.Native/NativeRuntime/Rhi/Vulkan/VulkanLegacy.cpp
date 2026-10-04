#include "VulkanLegacy.hpp"

#if defined(FRUITY_HAS_VULKAN)
#include <algorithm>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace MphRead::NativeRuntime::Rhi::Vulkan::Legacy
{
    namespace
    {
        struct State final
        {
            VkDevice Device = VK_NULL_HANDLE;
            PFN_vkCmdPipelineBarrier CmdPipelineBarrier = nullptr;
            PFN_vkQueueSubmit QueueSubmit = nullptr;
            PFN_vkCmdWriteTimestamp CmdWriteTimestamp = nullptr;
            PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers = nullptr;
            PFN_vkCreateRenderPass CreateRenderPass = nullptr;
            PFN_vkDestroyRenderPass DestroyRenderPass = nullptr;
            PFN_vkCreateFramebuffer CreateFramebuffer = nullptr;
            PFN_vkDestroyFramebuffer DestroyFramebuffer = nullptr;
            PFN_vkCmdBeginRenderPass CmdBeginRenderPass = nullptr;
            PFN_vkCmdEndRenderPass CmdEndRenderPass = nullptr;
            PFN_vkCreateBuffer CreateBuffer = nullptr;
            PFN_vkDestroyBuffer DestroyBuffer = nullptr;
            PFN_vkGetBufferMemoryRequirements2 GetBufferMemoryRequirements2 = nullptr;
            PFN_vkCreateImage CreateImage = nullptr;
            PFN_vkDestroyImage DestroyImage = nullptr;
            PFN_vkGetImageMemoryRequirements2 GetImageMemoryRequirements2 = nullptr;
            PFN_vkCreateImageView CreateImageView = nullptr;
            PFN_vkDestroyImageView DestroyImageView = nullptr;

            // Recording may happen on more than one thread, and views are
            // retired from whichever thread sees their work complete.
            std::mutex Mutex;
            std::unordered_map<VkImageView, VkFormat> ViewFormats;
            // Keys are the attachment descriptions, flattened.
            std::map<std::vector<std::uint64_t>, VkRenderPass> RenderPasses;
            struct Framebuffer final
            {
                VkFramebuffer Handle = VK_NULL_HANDLE;
                std::vector<VkImageView> Views;
            };
            std::map<std::vector<std::uint64_t>, Framebuffer> Framebuffers;
        };

        State& Shared()
        {
            static State state;
            return state;
        }

        void Check(VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string("Vulkan legacy path: ") + operation + " failed ("
                    + std::to_string(static_cast<int>(result)) + ").");
        }

        VkAttachmentLoadOp ToLoad(VkAttachmentLoadOp op)
        { return op == VK_ATTACHMENT_LOAD_OP_NONE_EXT ? VK_ATTACHMENT_LOAD_OP_LOAD : op; }
        VkAttachmentStoreOp ToStore(VkAttachmentStoreOp op)
        { return op == VK_ATTACHMENT_STORE_OP_NONE ? VK_ATTACHMENT_STORE_OP_STORE : op; }

        // One attachment of a render pass, as the cache keys it.
        struct Attachment final
        {
            VkFormat Format = VK_FORMAT_UNDEFINED;
            VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;
            VkAttachmentLoadOp Load = VK_ATTACHMENT_LOAD_OP_LOAD, StencilLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            VkAttachmentStoreOp Store = VK_ATTACHMENT_STORE_OP_STORE, StencilStore = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
        };

        // A single-subpass render pass whose attachments stay in the layout
        // the rendering names: dynamic rendering transitions nothing either,
        // and every barrier around it is recorded outside it, as before.
        // colors[i].Format == VK_FORMAT_UNDEFINED is an unused slot.
        VkRenderPass RenderPassLocked(State& s, std::span<const Attachment> colors, const Attachment* depth)
        {
            std::vector<std::uint64_t> key;
            const auto add = [&](const Attachment& a) {
                key.insert(key.end(), {static_cast<std::uint64_t>(a.Format), static_cast<std::uint64_t>(a.Samples),
                    static_cast<std::uint64_t>(a.Load), static_cast<std::uint64_t>(a.StencilLoad),
                    static_cast<std::uint64_t>(a.Store), static_cast<std::uint64_t>(a.StencilStore),
                    static_cast<std::uint64_t>(a.Layout)});
            };
            key.push_back(colors.size());
            for (const auto& color : colors) add(color);
            key.push_back(depth ? 1U : 0U);
            if (depth) add(*depth);
            if (const auto found = s.RenderPasses.find(key); found != s.RenderPasses.end()) return found->second;

            std::vector<VkAttachmentDescription> attachments;
            std::vector<VkAttachmentReference> colorRefs;
            const auto describe = [&](const Attachment& a) {
                VkAttachmentDescription d{};
                d.format = a.Format; d.samples = a.Samples;
                d.loadOp = a.Load; d.storeOp = a.Store;
                d.stencilLoadOp = a.StencilLoad; d.stencilStoreOp = a.StencilStore;
                d.initialLayout = d.finalLayout = a.Layout;
                attachments.push_back(d);
                return VkAttachmentReference{static_cast<std::uint32_t>(attachments.size() - 1), a.Layout};
            };
            for (const auto& color : colors)
                colorRefs.push_back(color.Format == VK_FORMAT_UNDEFINED
                    ? VkAttachmentReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED} : describe(color));
            VkAttachmentReference depthRef{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
            if (depth) depthRef = describe(*depth);
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = static_cast<std::uint32_t>(colorRefs.size());
            subpass.pColorAttachments = colorRefs.data();
            subpass.pDepthStencilAttachment = depth ? &depthRef : nullptr;
            // The backend barriers an attachment only when its state changes, so
            // a rendering that resumes a target the last one left (load after
            // store) has no barrier between them. A tiler overlaps the two, and
            // the load reads tiles the store has not written: objects drop out
            // for a frame. Every pass waits for the attachment writes before it.
            // (Part of compatibility: the pipelines' passes carry it too.)
            constexpr VkPipelineStageFlags attachmentStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            VkSubpassDependency resume{};
            resume.srcSubpass = VK_SUBPASS_EXTERNAL; resume.dstSubpass = 0;
            resume.srcStageMask = attachmentStages; resume.dstStageMask = attachmentStages;
            resume.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            resume.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            VkRenderPassCreateInfo create{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            create.attachmentCount = static_cast<std::uint32_t>(attachments.size());
            create.pAttachments = attachments.data();
            create.subpassCount = 1; create.pSubpasses = &subpass;
            create.dependencyCount = 1; create.pDependencies = &resume;
            VkRenderPass pass = VK_NULL_HANDLE;
            Check(s.CreateRenderPass(s.Device, &create, nullptr, &pass), "vkCreateRenderPass");
            s.RenderPasses.emplace(std::move(key), pass);
            return pass;
        }

        VkFormat ViewFormatLocked(State& s, VkImageView view)
        {
            const auto found = s.ViewFormats.find(view);
            if (found == s.ViewFormats.end())
                throw std::logic_error("Vulkan legacy path: rendering into an image view it did not see created.");
            return found->second;
        }
    }

    void Install(VkDevice device, PFN_vkGetDeviceProcAddr proc)
    {
        auto& s = Shared();
        std::lock_guard lock(s.Mutex);
        if (s.Device && s.Device != device)
            throw std::logic_error("Vulkan legacy path: another legacy device is still installed.");
        const auto load = [&](auto& slot, const char* name) {
            slot = reinterpret_cast<std::remove_reference_t<decltype(slot)>>(proc(device, name));
            if (!slot) throw std::runtime_error(std::string("Missing Vulkan device entry point: ") + name);
        };
        load(s.CmdPipelineBarrier, "vkCmdPipelineBarrier");
        load(s.QueueSubmit, "vkQueueSubmit");
        load(s.CmdWriteTimestamp, "vkCmdWriteTimestamp");
        load(s.CmdBindVertexBuffers, "vkCmdBindVertexBuffers");
        load(s.CreateRenderPass, "vkCreateRenderPass");
        load(s.DestroyRenderPass, "vkDestroyRenderPass");
        load(s.CreateFramebuffer, "vkCreateFramebuffer");
        load(s.DestroyFramebuffer, "vkDestroyFramebuffer");
        load(s.CmdBeginRenderPass, "vkCmdBeginRenderPass");
        load(s.CmdEndRenderPass, "vkCmdEndRenderPass");
        load(s.CreateBuffer, "vkCreateBuffer");
        load(s.DestroyBuffer, "vkDestroyBuffer");
        load(s.GetBufferMemoryRequirements2, "vkGetBufferMemoryRequirements2");
        load(s.CreateImage, "vkCreateImage");
        load(s.DestroyImage, "vkDestroyImage");
        load(s.GetImageMemoryRequirements2, "vkGetImageMemoryRequirements2");
        load(s.CreateImageView, "vkCreateImageView");
        load(s.DestroyImageView, "vkDestroyImageView");
        s.Device = device;
    }

    void Uninstall(VkDevice device) noexcept
    {
        auto& s = Shared();
        std::lock_guard lock(s.Mutex);
        if (s.Device != device) return;
        for (auto& [key, framebuffer] : s.Framebuffers) s.DestroyFramebuffer(device, framebuffer.Handle, nullptr);
        for (auto& [key, pass] : s.RenderPasses) s.DestroyRenderPass(device, pass, nullptr);
        s.Framebuffers.clear();
        s.RenderPasses.clear();
        s.ViewFormats.clear();
        s.Device = VK_NULL_HANDLE;
    }

    VkRenderPass CompatibleRenderPass(std::span<const VkFormat> colors, VkFormat depth, bool stencil,
        VkSampleCountFlagBits samples)
    {
        auto& s = Shared();
        std::lock_guard lock(s.Mutex);
        std::vector<Attachment> attachments;
        for (const auto format : colors)
        {
            Attachment a{};
            a.Format = format; a.Samples = samples; a.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments.push_back(a);
        }
        Attachment d{};
        d.Format = depth; d.Samples = samples; d.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        if (stencil) { d.StencilLoad = VK_ATTACHMENT_LOAD_OP_LOAD; d.StencilStore = VK_ATTACHMENT_STORE_OP_STORE; }
        return RenderPassLocked(s, attachments, depth != VK_FORMAT_UNDEFINED ? &d : nullptr);
    }

    VkPipelineStageFlags ToStages(VkPipelineStageFlags2 stages, bool source) noexcept
    {
        // TOP_OF_PIPE through ALL_COMMANDS keep their 1.0 values.
        auto result = static_cast<VkPipelineStageFlags>(stages & 0x1FFFFULL);
        if (stages & (VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_RESOLVE_BIT
            | VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT))
            result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        if (stages & (VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT))
            result |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        // The backend has no tessellation or geometry stage to name.
        if (stages & VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT)
            result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (!result) result = source ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        return result;
    }

    VkAccessFlags ToAccess(VkAccessFlags2 access) noexcept
    {
        auto result = static_cast<VkAccessFlags>(access & 0x1FFFFULL);
        if (access & (VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT))
            result |= VK_ACCESS_SHADER_READ_BIT;
        if (access & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT) result |= VK_ACCESS_SHADER_WRITE_BIT;
        return result;
    }

    VkImageLayout ToLayout(VkImageLayout layout, VkImageAspectFlags aspect) noexcept
    {
        const bool depth = (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
        if (layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
            return depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
            return depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return layout;
    }

    VKAPI_ATTR void VKAPI_CALL CmdPipelineBarrier2(VkCommandBuffer commands, const VkDependencyInfo* info)
    {
        // 1.0 has one pair of stage masks per call: their union holds every
        // barrier's own, which is the same dependency or a wider one.
        VkPipelineStageFlags2 source = 0, destination = 0;
        std::vector<VkMemoryBarrier> memory;
        std::vector<VkBufferMemoryBarrier> buffers;
        std::vector<VkImageMemoryBarrier> images;
        for (std::uint32_t i = 0; i < info->memoryBarrierCount; ++i)
        {
            const auto& b = info->pMemoryBarriers[i];
            source |= b.srcStageMask; destination |= b.dstStageMask;
            memory.push_back({VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, ToAccess(b.srcAccessMask), ToAccess(b.dstAccessMask)});
        }
        for (std::uint32_t i = 0; i < info->bufferMemoryBarrierCount; ++i)
        {
            const auto& b = info->pBufferMemoryBarriers[i];
            source |= b.srcStageMask; destination |= b.dstStageMask;
            buffers.push_back({VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, ToAccess(b.srcAccessMask),
                ToAccess(b.dstAccessMask), b.srcQueueFamilyIndex, b.dstQueueFamilyIndex, b.buffer, b.offset, b.size});
        }
        for (std::uint32_t i = 0; i < info->imageMemoryBarrierCount; ++i)
        {
            const auto& b = info->pImageMemoryBarriers[i];
            source |= b.srcStageMask; destination |= b.dstStageMask;
            const auto aspect = b.subresourceRange.aspectMask;
            images.push_back({VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, ToAccess(b.srcAccessMask),
                ToAccess(b.dstAccessMask), ToLayout(b.oldLayout, aspect), ToLayout(b.newLayout, aspect),
                b.srcQueueFamilyIndex, b.dstQueueFamilyIndex, b.image, b.subresourceRange});
        }
        Shared().CmdPipelineBarrier(commands, ToStages(source, true), ToStages(destination, false), info->dependencyFlags,
            static_cast<std::uint32_t>(memory.size()), memory.data(),
            static_cast<std::uint32_t>(buffers.size()), buffers.data(),
            static_cast<std::uint32_t>(images.size()), images.data());
    }

    VKAPI_ATTR VkResult VKAPI_CALL QueueSubmit2(VkQueue queue, std::uint32_t count, const VkSubmitInfo2* submits,
        VkFence fence)
    {
        struct Work final
        {
            std::vector<VkSemaphore> Waits, Signals;
            std::vector<VkPipelineStageFlags> WaitStages;
            std::vector<std::uint64_t> WaitValues, SignalValues;
            std::vector<VkCommandBuffer> Commands;
            VkTimelineSemaphoreSubmitInfo Timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
        };
        std::vector<Work> work(count);
        std::vector<VkSubmitInfo> infos(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto& from = submits[i];
            auto& w = work[i];
            for (std::uint32_t j = 0; j < from.waitSemaphoreInfoCount; ++j)
            {
                const auto& wait = from.pWaitSemaphoreInfos[j];
                w.Waits.push_back(wait.semaphore);
                // 1.0 has no empty wait mask: everything waits.
                w.WaitStages.push_back(wait.stageMask ? ToStages(wait.stageMask, false) : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                w.WaitValues.push_back(wait.value);
            }
            for (std::uint32_t j = 0; j < from.commandBufferInfoCount; ++j)
                w.Commands.push_back(from.pCommandBufferInfos[j].commandBuffer);
            // A 1.0 signal waits for the whole batch, which every mask here is within.
            for (std::uint32_t j = 0; j < from.signalSemaphoreInfoCount; ++j)
            {
                w.Signals.push_back(from.pSignalSemaphoreInfos[j].semaphore);
                w.SignalValues.push_back(from.pSignalSemaphoreInfos[j].value);
            }
            // Values are read for timeline semaphores and ignored for binary ones.
            w.Timeline.waitSemaphoreValueCount = static_cast<std::uint32_t>(w.WaitValues.size());
            w.Timeline.pWaitSemaphoreValues = w.WaitValues.data();
            w.Timeline.signalSemaphoreValueCount = static_cast<std::uint32_t>(w.SignalValues.size());
            w.Timeline.pSignalSemaphoreValues = w.SignalValues.data();
            auto& to = infos[i];
            to = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            to.pNext = &w.Timeline;
            to.waitSemaphoreCount = static_cast<std::uint32_t>(w.Waits.size());
            to.pWaitSemaphores = w.Waits.data();
            to.pWaitDstStageMask = w.WaitStages.data();
            to.commandBufferCount = static_cast<std::uint32_t>(w.Commands.size());
            to.pCommandBuffers = w.Commands.data();
            to.signalSemaphoreCount = static_cast<std::uint32_t>(w.Signals.size());
            to.pSignalSemaphores = w.Signals.data();
        }
        return Shared().QueueSubmit(queue, count, infos.data(), fence);
    }

    VKAPI_ATTR void VKAPI_CALL CmdWriteTimestamp2(VkCommandBuffer commands, VkPipelineStageFlags2 stage,
        VkQueryPool pool, std::uint32_t query)
    {
        // 1.0 takes one stage: the top for the top, the bottom for the rest.
        const bool top = (stage & ~VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT) == 0;
        Shared().CmdWriteTimestamp(commands, top ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            pool, query);
    }

    VKAPI_ATTR void VKAPI_CALL CmdBeginRendering(VkCommandBuffer commands, const VkRenderingInfo* info)
    {
        auto& s = Shared();
        std::vector<Attachment> colors;
        std::vector<VkImageView> views;
        std::vector<VkClearValue> clears;
        Attachment depth{};
        bool hasDepth = false;
        VkRenderPass pass = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        const auto& area = info->renderArea;
        const std::uint32_t width = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::max(0, area.offset.x)) + area.extent.width);
        const std::uint32_t height = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::max(0, area.offset.y)) + area.extent.height);
        {
            std::lock_guard lock(s.Mutex);
            for (std::uint32_t i = 0; i < info->colorAttachmentCount; ++i)
            {
                const auto& from = info->pColorAttachments[i];
                Attachment a{};
                if (from.imageView)
                {
                    a.Format = ViewFormatLocked(s, from.imageView);
                    a.Load = ToLoad(from.loadOp); a.Store = ToStore(from.storeOp);
                    a.Layout = ToLayout(from.imageLayout, VK_IMAGE_ASPECT_COLOR_BIT);
                    views.push_back(from.imageView);
                    clears.push_back(from.clearValue);
                }
                colors.push_back(a);
            }
            // One depth/stencil attachment in a render pass: the depth view,
            // or the stencil one when there is no depth.
            const auto* d = info->pDepthAttachment && info->pDepthAttachment->imageView ? info->pDepthAttachment : nullptr;
            const auto* st = info->pStencilAttachment && info->pStencilAttachment->imageView ? info->pStencilAttachment : nullptr;
            if (d || st)
            {
                const auto& main = d ? *d : *st;
                hasDepth = true;
                depth.Format = ViewFormatLocked(s, main.imageView);
                depth.Layout = ToLayout(main.imageLayout, VK_IMAGE_ASPECT_DEPTH_BIT);
                depth.Load = d ? ToLoad(d->loadOp) : VK_ATTACHMENT_LOAD_OP_LOAD;
                depth.Store = d ? ToStore(d->storeOp) : VK_ATTACHMENT_STORE_OP_STORE;
                // A stencil the rendering does not name is kept, as dynamic rendering leaves it.
                depth.StencilLoad = st ? ToLoad(st->loadOp) : VK_ATTACHMENT_LOAD_OP_LOAD;
                depth.StencilStore = st ? ToStore(st->storeOp) : VK_ATTACHMENT_STORE_OP_STORE;
                if (depth.Format == VK_FORMAT_D16_UNORM || depth.Format == VK_FORMAT_D32_SFLOAT
                    || depth.Format == VK_FORMAT_X8_D24_UNORM_PACK32)
                { depth.StencilLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE; depth.StencilStore = VK_ATTACHMENT_STORE_OP_DONT_CARE; }
                VkClearValue clear{};
                clear.depthStencil.depth = d ? d->clearValue.depthStencil.depth : 1.0F;
                clear.depthStencil.stencil = st ? st->clearValue.depthStencil.stencil : 0U;
                views.push_back(main.imageView);
                clears.push_back(clear);
            }
            pass = RenderPassLocked(s, colors, hasDepth ? &depth : nullptr);

            std::vector<std::uint64_t> key{reinterpret_cast<std::uint64_t>(pass), width, height};
            for (const auto view : views) key.push_back(reinterpret_cast<std::uint64_t>(view));
            auto found = s.Framebuffers.find(key);
            if (found == s.Framebuffers.end())
            {
                VkFramebufferCreateInfo create{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                create.renderPass = pass;
                create.attachmentCount = static_cast<std::uint32_t>(views.size());
                create.pAttachments = views.data();
                create.width = width; create.height = height; create.layers = 1;
                State::Framebuffer made{};
                made.Views = views;
                Check(s.CreateFramebuffer(s.Device, &create, nullptr, &made.Handle), "vkCreateFramebuffer");
                found = s.Framebuffers.emplace(std::move(key), std::move(made)).first;
            }
            framebuffer = found->second.Handle;
        }
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = pass;
        begin.framebuffer = framebuffer;
        begin.renderArea = area;
        begin.clearValueCount = static_cast<std::uint32_t>(clears.size());
        begin.pClearValues = clears.data();
        s.CmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    }

    VKAPI_ATTR void VKAPI_CALL CmdEndRendering(VkCommandBuffer commands)
    {
        Shared().CmdEndRenderPass(commands);
    }

    VKAPI_ATTR void VKAPI_CALL CmdBindVertexBuffers2(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count,
        const VkBuffer* buffers, const VkDeviceSize* offsets, const VkDeviceSize*, const VkDeviceSize*)
    {
        Shared().CmdBindVertexBuffers(commands, first, count, buffers, offsets);
    }

    VKAPI_ATTR void VKAPI_CALL GetDeviceBufferMemoryRequirements(VkDevice device,
        const VkDeviceBufferMemoryRequirements* info, VkMemoryRequirements2* requirements)
    {
        // maintenance4's question, asked of a buffer made for the purpose.
        auto& s = Shared();
        VkBuffer buffer = VK_NULL_HANDLE;
        Check(s.CreateBuffer(device, info->pCreateInfo, nullptr, &buffer), "vkCreateBuffer(memory requirements)");
        VkBufferMemoryRequirementsInfo2 query{VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2};
        query.buffer = buffer;
        s.GetBufferMemoryRequirements2(device, &query, requirements);
        s.DestroyBuffer(device, buffer, nullptr);
    }

    VKAPI_ATTR void VKAPI_CALL GetDeviceImageMemoryRequirements(VkDevice device,
        const VkDeviceImageMemoryRequirements* info, VkMemoryRequirements2* requirements)
    {
        auto& s = Shared();
        VkImage image = VK_NULL_HANDLE;
        Check(s.CreateImage(device, info->pCreateInfo, nullptr, &image), "vkCreateImage(memory requirements)");
        VkImageMemoryRequirementsInfo2 query{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
        query.image = image;
        s.GetImageMemoryRequirements2(device, &query, requirements);
        s.DestroyImage(device, image, nullptr);
    }

    VKAPI_ATTR VkResult VKAPI_CALL CreateImageView(VkDevice device, const VkImageViewCreateInfo* info,
        const VkAllocationCallbacks* allocator, VkImageView* view)
    {
        auto& s = Shared();
        const auto result = s.CreateImageView(device, info, allocator, view);
        if (result == VK_SUCCESS)
        {
            std::lock_guard lock(s.Mutex);
            s.ViewFormats[*view] = info->format;
        }
        return result;
    }

    VKAPI_ATTR void VKAPI_CALL DestroyImageView(VkDevice device, VkImageView view, const VkAllocationCallbacks* allocator)
    {
        auto& s = Shared();
        {
            // A view is retired once its work is complete, and so is every
            // framebuffer that held it; the handle may come back as another view.
            std::lock_guard lock(s.Mutex);
            s.ViewFormats.erase(view);
            for (auto it = s.Framebuffers.begin(); it != s.Framebuffers.end();)
                if (std::find(it->second.Views.begin(), it->second.Views.end(), view) != it->second.Views.end())
                {
                    s.DestroyFramebuffer(device, it->second.Handle, nullptr);
                    it = s.Framebuffers.erase(it);
                }
                else ++it;
        }
        s.DestroyImageView(device, view, allocator);
    }
}
#endif
