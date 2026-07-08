#include <CZ/Ream/VK/RVKPainter.h>
#include <CZ/Ream/VK/RVKPipeline.h>
#include <CZ/Ream/VK/RVKImage.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/RMatrixUtils.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RSync.h>
#include <CZ/Ream/RLog.h>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace CZ;

static constexpr VkDeviceSize VBO_CAPACITY { 1u << 20 }; // 1 MiB

RVKPainter::RVKPainter(std::shared_ptr<RSurface> surface, RVKDevice *device) noexcept :
    RPainter(surface, device)
{
    m_target = surface->image()->asVK().get();
    if (m_target)
        m_format = m_target->vkFormat();
}

RVKPainter::~RVKPainter() noexcept
{
    flush();

    const VkDevice d { dev()->device() };
    if (m_vboMem != VK_NULL_HANDLE) { vkUnmapMemory(d, m_vboMem); vkFreeMemory(d, m_vboMem, nullptr); }
    if (m_vbo != VK_NULL_HANDLE) vkDestroyBuffer(d, m_vbo, nullptr);
    if (m_framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(d, m_framebuffer, nullptr);
    if (m_descPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(d, m_descPool, nullptr);
    if (m_pool != VK_NULL_HANDLE) vkDestroyCommandPool(d, m_pool, nullptr);
}

RVKDevice *RVKPainter::dev() const noexcept { return (RVKDevice*)m_device; }

bool RVKPainter::setGeometry(const RSurfaceGeometry &geometry) noexcept
{
    if (!geometry.isValid())
        return false;
    m_state.geometry = geometry;
    return true;
}

bool RVKPainter::beginRecording() noexcept
{
    if (m_recording)
        return true;

    if (!m_target || m_format == VK_FORMAT_UNDEFINED || m_target->vkImageView() == VK_NULL_HANDLE)
        return false;

    const VkDevice d { dev()->device() };
    auto *pm { dev()->pipelines() };
    if (!pm)
        return false;

    m_rp = pm->renderPass(m_format);
    if (m_rp == VK_NULL_HANDLE)
        return false;

    const UInt32 W { (UInt32)m_target->size().width() };
    const UInt32 H { (UInt32)m_target->size().height() };

    if (m_pool == VK_NULL_HANDLE)
    {
        VkCommandPoolCreateInfo pci {};
        pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pci.queueFamilyIndex = dev()->graphicsQueueFamily();
        if (vkCreateCommandPool(d, &pci, nullptr, &m_pool) != VK_SUCCESS)
            return false;
    }

    VkImageView view { m_target->vkImageView() };
    VkFramebufferCreateInfo fbi {};
    fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbi.renderPass = m_rp;
    fbi.attachmentCount = 1;
    fbi.pAttachments = &view;
    fbi.width = W;
    fbi.height = H;
    fbi.layers = 1;
    if (vkCreateFramebuffer(d, &fbi, nullptr, &m_framebuffer) != VK_SUCCESS)
        return false;

    if (m_vbo == VK_NULL_HANDLE)
    {
        VkBufferCreateInfo bi {};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = VBO_CAPACITY;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(d, &bi, nullptr, &m_vbo) != VK_SUCCESS)
            return false;

        VkMemoryRequirements req {};
        vkGetBufferMemoryRequirements(d, m_vbo, &req);
        const UInt32 mt { dev()->findMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
        if (mt == UINT32_MAX)
            return false;

        VkMemoryAllocateInfo ai {};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = mt;
        if (vkAllocateMemory(d, &ai, nullptr, &m_vboMem) != VK_SUCCESS)
            return false;
        vkBindBufferMemory(d, m_vbo, m_vboMem, 0);
        vkMapMemory(d, m_vboMem, 0, VBO_CAPACITY, 0, &m_vboMapped);
        m_vboCapacity = req.size;
    }

    if (m_descPool == VK_NULL_HANDLE)
    {
        VkDescriptorPoolSize ps {};
        ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ps.descriptorCount = 512;
        VkDescriptorPoolCreateInfo dpi {};
        dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpi.maxSets = 256;
        dpi.poolSizeCount = 1;
        dpi.pPoolSizes = &ps;
        if (vkCreateDescriptorPool(d, &dpi, nullptr, &m_descPool) != VK_SUCCESS)
            return false;
    }
    m_vertexCount = 0;

    VkCommandBufferAllocateInfo cai {};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = m_pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(d, &cai, &m_cmd) != VK_SUCCESS)
        return false;

    VkCommandBufferBeginInfo cbi {};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_cmd, &cbi);

    // Move the target into COLOR_ATTACHMENT_OPTIMAL (loadOp=LOAD preserves its content).
    m_target->transitionLayout(m_cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

    m_recording = true;
    return true;
}

void RVKPainter::ensureRenderPass() noexcept
{
    if (m_renderPassActive)
        return;

    const UInt32 W { (UInt32)m_target->size().width() };
    const UInt32 H { (UInt32)m_target->size().height() };

    VkRenderPassBeginInfo rpb {};
    rpb.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpb.renderPass = m_rp;
    rpb.framebuffer = m_framebuffer;
    rpb.renderArea.extent = { W, H };
    vkCmdBeginRenderPass(m_cmd, &rpb, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp {};
    vp.width = (float)W; vp.height = (float)H;
    vp.minDepth = 0.f; vp.maxDepth = 1.f;
    vkCmdSetViewport(m_cmd, 0, 1, &vp);

    m_renderPassActive = true;
}

void RVKPainter::endRenderPassIfActive() noexcept
{
    if (m_renderPassActive)
    {
        vkCmdEndRenderPass(m_cmd);
        m_renderPassActive = false;
    }
}

void RVKPainter::transitionSource(const std::shared_ptr<RImage> &img) noexcept
{
    auto *vk { img->asVK().get() };
    if (!vk)
        return;

    // When sampling on a device other than the source's allocator (SRM Prime), import the
    // source's dma-buf onto this device first, then transition that per-device image.
    if (dev() != vk->allocatorVK())
        vk->ensureCrossDevice(dev());

    if (vk->layout(dev()) != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        // Layout transitions are illegal inside a render pass.
        endRenderPassIfActive();
        vk->transitionLayout(m_cmd, dev(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, VK_ACCESS_SHADER_READ_BIT);
    }

    // Cross-device write->read is synchronized via the source's write sync (sync_file fence).
    if (img->writeSync())
        img->writeSync()->gpuWait(m_device);

    m_readImages.push_back(img);
}

float *RVKPainter::reserveVertices(UInt32 vertexCount, UInt32 &firstVertex) noexcept
{
    const VkDeviceSize needed { (VkDeviceSize)(m_vertexCount + vertexCount) * 6 * sizeof(float) };
    if (needed > m_vboCapacity)
        return nullptr; // overflow (should not happen for typical regions)

    firstVertex = m_vertexCount;
    float *ptr { static_cast<float*>(m_vboMapped) + (size_t)m_vertexCount * 6 };
    m_vertexCount += vertexCount;
    return ptr;
}

void RVKPainter::flush() noexcept
{
    if (!m_recording)
        return;

    endRenderPassIfActive();

    // The render pass leaves the image in COLOR_ATTACHMENT_OPTIMAL (its finalLayout), which is
    // valid for any render-target image. Do NOT force SHADER_READ_ONLY here: render-target-only
    // images (e.g. swapchain images without VK_IMAGE_USAGE_SAMPLED_BIT) cannot use that layout.
    // Later consumers transition from COLOR_ATTACHMENT_OPTIMAL as needed (sampling ->
    // SHADER_READ_ONLY, readback -> TRANSFER_SRC, present -> PRESENT_SRC).
    m_target->setTrackedLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkEndCommandBuffer(m_cmd);

    const VkDevice d { dev()->device() };

    // Submit WITHOUT blocking: the CPU keeps recording other passes while the GPU works. A fence
    // tracks completion; the per-pass resources are reclaimed later by clearGarbage() (called each
    // frame by the compositor). This CPU/GPU pipelining is the key difference vs a blocking submit.
    // CZ_REAM_VK_SYNC_SUBMIT=1 forces the legacy blocking submit (for A/B comparison / debugging).
    static const bool forceSync { [] { const char *e { std::getenv("CZ_REAM_VK_SYNC_SUBMIT") }; return e && atoi(e) != 0; }() };

    VkFence fence { VK_NULL_HANDLE };
    VkFenceCreateInfo fi {};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    if (forceSync || vkCreateFence(d, &fi, nullptr, &fence) != VK_SUCCESS)
    {
        if (fence != VK_NULL_HANDLE) { vkDestroyFence(d, fence, nullptr); fence = VK_NULL_HANDLE; }
        // Blocking submit + immediate cleanup (legacy path).
        dev()->submitCommand(m_cmd);
        vkFreeCommandBuffers(d, m_pool, 1, &m_cmd);
        if (m_framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(d, m_framebuffer, nullptr);
        if (m_descPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(d, m_descPool, nullptr);
        if (m_vboMem != VK_NULL_HANDLE) { vkUnmapMemory(d, m_vboMem); vkFreeMemory(d, m_vboMem, nullptr); }
        if (m_vbo != VK_NULL_HANDLE) vkDestroyBuffer(d, m_vbo, nullptr);
        if (m_pool != VK_NULL_HANDLE) vkDestroyCommandPool(d, m_pool, nullptr);
    }
    else
    {
        std::vector<VkSemaphore> ownedWaits;
        dev()->submitCommandAsync(m_cmd, fence, ownedWaits);

        // Hand every per-pass resource to the fence-tracked GC (freed once the GPU is done).
        const VkCommandPool pool { m_pool };
        const VkCommandBuffer cmd { m_cmd };
        const VkBuffer vbo { m_vbo };
        const VkDeviceMemory vboMem { m_vboMem };
        const VkDescriptorPool descPool { m_descPool };
        const VkFramebuffer framebuffer { m_framebuffer };
        RVKDevice *device { dev() };

        device->deferDestroy(fence, [device, pool, cmd, vbo, vboMem, descPool, framebuffer,
                                     ow = std::move(ownedWaits)]()
        {
            const VkDevice dd { device->device() };
            if (cmd != VK_NULL_HANDLE) vkFreeCommandBuffers(dd, pool, 1, &cmd);
            if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(dd, framebuffer, nullptr);
            if (descPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(dd, descPool, nullptr);
            if (vboMem != VK_NULL_HANDLE) { vkUnmapMemory(dd, vboMem); vkFreeMemory(dd, vboMem, nullptr); }
            if (vbo != VK_NULL_HANDLE) vkDestroyBuffer(dd, vbo, nullptr);
            if (pool != VK_NULL_HANDLE) vkDestroyCommandPool(dd, pool, nullptr);
            for (VkSemaphore s : ow) vkDestroySemaphore(dd, s, nullptr);
        });
    }

    // These resources now belong to the GC (or were freed on the fallback path).
    m_cmd = VK_NULL_HANDLE;
    m_framebuffer = VK_NULL_HANDLE;
    m_descPool = VK_NULL_HANDLE;
    m_vbo = VK_NULL_HANDLE;
    m_vboMem = VK_NULL_HANDLE;
    m_vboMapped = nullptr;
    m_vboCapacity = 0;
    m_pool = VK_NULL_HANDLE;

    // Publish a read sync for source images (ordered after this submit on the same queue).
    if (!m_readImages.empty())
    {
        auto sync { RSync::Make(m_device) };
        for (auto &img : m_readImages)
            img->setReadSync(sync);
        m_readImages.clear();
    }

    m_recording = false;
}

// Premultiplied color (mirrors RGLPainter::calcDrawColorColor).
static SkColor4f DrawColorColor(const RPainter::State &s) noexcept
{
    SkColor4f c { SkColor4f::FromColor(s.color) };
    if (s.options.has(RPainter::ColorIsPremult))
    {
        const float a { s.opacity * s.factor.fA };
        c.fR *= a * s.factor.fR; c.fG *= a * s.factor.fG; c.fB *= a * s.factor.fB; c.fA *= a;
    }
    else
    {
        c.fA *= s.opacity * s.factor.fA;
        c.fR *= c.fA * s.factor.fR; c.fG *= c.fA * s.factor.fG; c.fB *= c.fA * s.factor.fB;
    }
    return c;
}

static RVKBlend ColorBlend(RBlendMode mode) noexcept
{
    switch (mode)
    {
    case RBlendMode::Src:
        return { false, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO };
    case RBlendMode::DstIn:
        return { true, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC_ALPHA };
    default: // SrcOver
        return { true, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA };
    }
}

bool RVKPainter::drawColor(const SkRegion &userRegion) noexcept
{
    if (blendMode() == RBlendMode::SrcOver && (SkColorGetA(color()) == 0 || factor().fA <= 0.f || opacity() <= 0.f))
        return true;

    if (!m_target)
        return false;

    const SkIRect clipRect { geometry().viewport.roundOut() };
    SkRegion region;
    if (!region.op(clipRect, userRegion, SkRegion::kIntersect_Op))
        return true;

    const SkColor4f colorF { DrawColorColor(m_state) };
    if (blendMode() == RBlendMode::DstIn && colorF.fA >= 1.f)
        return true; // multiplying dst by 1 is a no-op

    if (!beginRecording())
        return false;
    ensureRenderPass();

    const int W { m_target->size().width() };
    const int H { m_target->size().height() };
    const SkMatrix vi { RMatrixUtils::VirtualToImage(geometry().transform, geometry().viewport, geometry().dst) };

    const UInt32 quadCount { (UInt32)region.computeRegionComplexity() };
    UInt32 firstVertex { 0 };
    float *v { reserveVertices(quadCount * 6, firstVertex) };
    if (!v)
        return false;

    const auto emit = [&](float ix, float iy)
    {
        *v++ = 2.f * ix / (float)W - 1.f; // NDC x
        *v++ = 2.f * iy / (float)H - 1.f; // NDC y (Vulkan y-down, image top-left)
        *v++ = 0.f; *v++ = 0.f;           // imageUV (unused)
        *v++ = 0.f; *v++ = 0.f;           // maskUV (unused)
    };

    for (SkRegion::Iterator it(region); !it.done(); it.next())
    {
        const SkRect r { SkRect::Make(it.rect()) };
        SkPoint c[4] {
            { r.fLeft, r.fTop }, { r.fRight, r.fTop }, { r.fRight, r.fBottom }, { r.fLeft, r.fBottom } };
        vi.mapPoints(c, 4);
        // Two triangles: (0,1,2) (0,2,3)
        emit(c[0].fX, c[0].fY); emit(c[1].fX, c[1].fY); emit(c[2].fX, c[2].fY);
        emit(c[0].fX, c[0].fY); emit(c[2].fX, c[2].fY); emit(c[3].fX, c[3].fY);
    }

    auto *pm { dev()->pipelines() };
    VkPipeline pipe { pm->colorPipeline(m_rp, m_format, ColorBlend(blendMode())) };
    if (pipe == VK_NULL_HANDLE)
        return false;

    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

    VkDeviceSize vboOffset { 0 };
    vkCmdBindVertexBuffers(m_cmd, 0, 1, &m_vbo, &vboOffset);

    // Scissor = region bounds mapped to image coords.
    SkRect bounds; vi.mapRect(&bounds, SkRect::Make(region.getBounds()));
    VkRect2D scissor {};
    const int x0 { std::clamp((int)std::floor(bounds.left()), 0, W) };
    const int y0 { std::clamp((int)std::floor(bounds.top()), 0, H) };
    const int x1 { std::clamp((int)std::ceil(bounds.right()), 0, W) };
    const int y1 { std::clamp((int)std::ceil(bounds.bottom()), 0, H) };
    scissor.offset = { x0, y0 };
    scissor.extent = { (UInt32)std::max(0, x1 - x0), (UInt32)std::max(0, y1 - y0) };
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);

    RVKPushConstants pc {};
    pc.color[0] = colorF.fR; pc.color[1] = colorF.fG; pc.color[2] = colorF.fB; pc.color[3] = colorF.fA;
    vkCmdPushConstants(m_cmd, pm->colorLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    vkCmdDraw(m_cmd, quadCount * 6, 1, firstVertex, 0);
    return true;
}

// Blend selection mirroring RGLPainter::drawImage's glBlendFunc chain.
static RVKBlend ImageBlend(RBlendMode mode, bool replaceColor, SkAlphaType at, float colorA, bool hasMask) noexcept
{
    const RVKBlend disabled { false, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO };
    const RVKBlend srcAlphaZero { true, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO };
    const RVKBlend srcAlphaOver { true, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA };
    const RVKBlend premultOver { true, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA };
    const RVKBlend dstIn { true, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC_ALPHA };

    if (mode == RBlendMode::Src)
    {
        if (replaceColor) return srcAlphaZero;
        return (at == kUnpremul_SkAlphaType) ? srcAlphaZero : disabled;
    }
    if (mode == RBlendMode::SrcOver)
    {
        if (replaceColor) return srcAlphaOver;
        if (at == kOpaque_SkAlphaType)
            return (colorA >= 1.f && !hasMask) ? disabled : srcAlphaOver;
        if (at == kUnpremul_SkAlphaType) return srcAlphaOver;
        return premultOver; // premultiplied
    }
    return dstIn;
}

bool RVKPainter::drawImage(const RDrawImageInfo &imageInfo, const SkRegion *clip, const RDrawImageInfo *maskInfo) noexcept
{
    if (blendMode() == RBlendMode::SrcOver && (factor().fA <= 0.f || opacity() <= 0.f))
        return true;
    if (!m_target)
        return false;

    auto image { imageInfo.image };
    if (!image)
        return false;

    SkRegion region { geometry().viewport.roundOut() };
    region.op(imageInfo.dst, SkRegion::kIntersect_Op);
    if (maskInfo)
        region.op(maskInfo->dst, SkRegion::kIntersect_Op);
    if (clip)
        region.op(*clip, SkRegion::kIntersect_Op);
    if (region.isEmpty())
        return true;

    std::shared_ptr<RImage> mask;
    if (maskInfo)
    {
        mask = maskInfo->image;
        if (!mask)
            return false;
        if (mask->alphaType() == kOpaque_SkAlphaType)
            mask.reset(); // an opaque mask has no effect
    }

    const bool replaceColor { m_state.options.has(ReplaceImageColor) && blendMode() != RBlendMode::DstIn };

    SkColor4f colorF { m_state.factor };
    if (replaceColor)
    {
        const SkColor4f rc { SkColor4f::FromColor(color()) };
        colorF.fA *= opacity();
        colorF.fR *= rc.fR; colorF.fG *= rc.fG; colorF.fB *= rc.fB;
    }
    else
        colorF.fA *= opacity();

    // Fast-paths for opaque source + no mask (reduce to a solid color fill).
    if (image->alphaType() == kOpaque_SkAlphaType && !mask)
    {
        if (blendMode() == RBlendMode::DstIn)
        {
            if (colorF.fA == 1.f)
                return true;
            save(); reset();
            setBlendMode(RBlendMode::DstIn); setOpacity(colorF.fA);
            setColor(SK_ColorWHITE); setOptions(ColorIsPremult);
            const bool r { drawColor(region) };
            restore();
            return r;
        }
        else if (replaceColor)
        {
            save();
            setColor(SkColorSetA(color(), 255)); setOptions(ColorIsPremult);
            const bool r { drawColor(region) };
            restore();
            return r;
        }
    }

    auto *srcVk { image->asVK().get() };
    if (!srcVk || srcVk->vkImageView() == VK_NULL_HANDLE)
        return false;
    RVKImage *maskVk { mask ? mask->asVK().get() : nullptr };
    if (mask && (!maskVk || maskVk->vkImageView() == VK_NULL_HANDLE))
        return false;

    // Cross-device sampling (Prime) requires the source(s) to be dma-shareable onto this device.
    if (dev() != srcVk->allocatorVK() && !srcVk->ensureCrossDevice(dev()))
        return false;
    if (maskVk && dev() != maskVk->allocatorVK() && !maskVk->ensureCrossDevice(dev()))
        return false;

    if (!beginRecording())
        return false;

    transitionSource(image);
    if (mask)
        transitionSource(mask);

    ensureRenderPass();

    RVKImageSpec spec {};
    spec.hasMask = mask ? 1 : 0;
    spec.replaceImageColor = replaceColor ? 1 : 0;
    spec.premultSrc = (!replaceColor && blendMode() != RBlendMode::DstIn && image->alphaType() == kPremul_SkAlphaType) ? 1 : 0;
    spec.blendDstIn = (blendMode() == RBlendMode::DstIn) ? 1 : 0;

    const RVKBlend blend { ImageBlend(blendMode(), replaceColor, image->alphaType(), colorF.fA, mask != nullptr) };

    auto *pm { dev()->pipelines() };
    VkPipeline pipe { pm->imagePipeline(m_rp, m_format, blend, spec) };
    if (pipe == VK_NULL_HANDLE)
        return false;

    // Allocate + update a descriptor set (image + mask combined image samplers).
    VkDescriptorSetLayout setLayout { pm->imageSetLayout() };
    VkDescriptorSetAllocateInfo dsa {};
    dsa.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsa.descriptorPool = m_descPool;
    dsa.descriptorSetCount = 1;
    dsa.pSetLayouts = &setLayout;
    VkDescriptorSet set { VK_NULL_HANDLE };
    if (vkAllocateDescriptorSets(dev()->device(), &dsa, &set) != VK_SUCCESS)
        return false;

    VkSampler imgSampler { pm->sampler(imageInfo.minFilter, imageInfo.magFilter, imageInfo.wrapS, imageInfo.wrapT) };
    VkSampler maskSampler { mask ? pm->sampler(maskInfo->minFilter, maskInfo->magFilter, maskInfo->wrapS, maskInfo->wrapT) : imgSampler };

    VkDescriptorImageInfo dii[2] {};
    dii[0].sampler = imgSampler;
    dii[0].imageView = srcVk->vkImageView(dev()); // per-device view (cross-device aware)
    dii[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dii[1].sampler = maskSampler;
    dii[1].imageView = (maskVk ? maskVk : srcVk)->vkImageView(dev()); // dummy = source when no mask
    dii[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[2] {};
    for (UInt32 i = 0; i < 2; i++)
    {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &dii[i];
    }
    vkUpdateDescriptorSets(dev()->device(), 2, writes, 0, nullptr);

    // Vertices: NDC positions + normalized image/mask UVs (all derived on the CPU).
    const int W { m_target->size().width() };
    const int H { m_target->size().height() };
    const SkMatrix vi { RMatrixUtils::VirtualToImage(geometry().transform, geometry().viewport, geometry().dst) };
    const SkMatrix imageProj { RMatrixUtils::VirtualToUV(SkRect::Make(imageInfo.dst), imageInfo.srcTransform, imageInfo.srcScale, imageInfo.src, image->size()) };
    SkMatrix maskProj;
    maskProj.setIdentity();
    if (mask)
        maskProj = RMatrixUtils::VirtualToUV(SkRect::Make(maskInfo->dst), maskInfo->srcTransform, maskInfo->srcScale, maskInfo->src, mask->size());

    const UInt32 quadCount { (UInt32)region.computeRegionComplexity() };
    UInt32 firstVertex { 0 };
    float *v { reserveVertices(quadCount * 6, firstVertex) };
    if (!v)
        return false;

    const auto emit = [&](const SkPoint &imgPt, const SkPoint &vPt)
    {
        *v++ = 2.f * imgPt.fX / (float)W - 1.f;
        *v++ = 2.f * imgPt.fY / (float)H - 1.f;
        const SkPoint uv { imageProj.mapXY(vPt.fX, vPt.fY) };
        *v++ = uv.fX; *v++ = uv.fY;
        const SkPoint muv { maskProj.mapXY(vPt.fX, vPt.fY) };
        *v++ = muv.fX; *v++ = muv.fY;
    };

    for (SkRegion::Iterator it(region); !it.done(); it.next())
    {
        const SkRect r { SkRect::Make(it.rect()) };
        SkPoint vpts[4] { { r.fLeft, r.fTop }, { r.fRight, r.fTop }, { r.fRight, r.fBottom }, { r.fLeft, r.fBottom } };
        SkPoint ipts[4];
        vi.mapPoints(ipts, vpts, 4);
        emit(ipts[0], vpts[0]); emit(ipts[1], vpts[1]); emit(ipts[2], vpts[2]);
        emit(ipts[0], vpts[0]); emit(ipts[2], vpts[2]); emit(ipts[3], vpts[3]);
    }

    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    VkDeviceSize vboOffset { 0 };
    vkCmdBindVertexBuffers(m_cmd, 0, 1, &m_vbo, &vboOffset);
    vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pm->imageLayout(), 0, 1, &set, 0, nullptr);

    SkRect bounds; vi.mapRect(&bounds, SkRect::Make(region.getBounds()));
    VkRect2D scissor {};
    const int x0 { std::clamp((int)std::floor(bounds.left()), 0, W) };
    const int y0 { std::clamp((int)std::floor(bounds.top()), 0, H) };
    const int x1 { std::clamp((int)std::ceil(bounds.right()), 0, W) };
    const int y1 { std::clamp((int)std::ceil(bounds.bottom()), 0, H) };
    scissor.offset = { x0, y0 };
    scissor.extent = { (UInt32)std::max(0, x1 - x0), (UInt32)std::max(0, y1 - y0) };
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);

    RVKPushConstants pc {};
    pc.factor[0] = colorF.fR; pc.factor[1] = colorF.fG; pc.factor[2] = colorF.fB; pc.factor[3] = colorF.fA;
    vkCmdPushConstants(m_cmd, pm->imageLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    vkCmdDraw(m_cmd, quadCount * 6, 1, firstVertex, 0);
    return true;
}

bool RVKPainter::drawImageEffect(const RDrawImageInfo &imageInfo, ImageEffect effect, const SkRegion *clip) noexcept
{
    if (!m_target)
        return false;

    auto image { imageInfo.image };
    if (!image)
        return false;

    SkRegion region { geometry().viewport.roundOut() };
    region.op(imageInfo.dst, SkRegion::kIntersect_Op);
    if (clip)
        region.op(*clip, SkRegion::kIntersect_Op);
    if (region.isEmpty())
        return true;

    auto *srcVk { image->asVK().get() };
    if (!srcVk || srcVk->vkImageView() == VK_NULL_HANDLE)
        return false;
    if (dev() != srcVk->allocatorVK() && !srcVk->ensureCrossDevice(dev()))
        return false;

    if (!beginRecording())
        return false;

    transitionSource(image);
    ensureRenderPass();

    auto *pm { dev()->pipelines() };
    VkPipeline pipe { pm->effectPipeline(m_rp, m_format, (UInt32)effect) };
    if (pipe == VK_NULL_HANDLE)
        return false;

    VkDescriptorSetLayout setLayout { pm->imageSetLayout() };
    VkDescriptorSetAllocateInfo dsa {};
    dsa.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsa.descriptorPool = m_descPool;
    dsa.descriptorSetCount = 1;
    dsa.pSetLayouts = &setLayout;
    VkDescriptorSet set { VK_NULL_HANDLE };
    if (vkAllocateDescriptorSets(dev()->device(), &dsa, &set) != VK_SUCCESS)
        return false;

    VkSampler s { pm->sampler(imageInfo.minFilter, imageInfo.magFilter, imageInfo.wrapS, imageInfo.wrapT) };
    VkDescriptorImageInfo dii[2] {};
    for (UInt32 i = 0; i < 2; i++)
    {
        dii[i].sampler = s;
        dii[i].imageView = srcVk->vkImageView(dev());
        dii[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    VkWriteDescriptorSet writes[2] {};
    for (UInt32 i = 0; i < 2; i++)
    {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &dii[i];
    }
    vkUpdateDescriptorSets(dev()->device(), 2, writes, 0, nullptr);

    const int W { m_target->size().width() };
    const int H { m_target->size().height() };
    const SkMatrix vi { RMatrixUtils::VirtualToImage(geometry().transform, geometry().viewport, geometry().dst) };
    const SkMatrix imageProj { RMatrixUtils::VirtualToUV(SkRect::Make(imageInfo.dst), imageInfo.srcTransform, imageInfo.srcScale, imageInfo.src, image->size()) };

    const UInt32 quadCount { (UInt32)region.computeRegionComplexity() };
    UInt32 firstVertex { 0 };
    float *v { reserveVertices(quadCount * 6, firstVertex) };
    if (!v)
        return false;

    const auto emit = [&](const SkPoint &imgPt, const SkPoint &vPt)
    {
        *v++ = 2.f * imgPt.fX / (float)W - 1.f;
        *v++ = 2.f * imgPt.fY / (float)H - 1.f;
        const SkPoint uv { imageProj.mapXY(vPt.fX, vPt.fY) };
        *v++ = uv.fX; *v++ = uv.fY;
        *v++ = 0.f; *v++ = 0.f;
    };

    for (SkRegion::Iterator it(region); !it.done(); it.next())
    {
        const SkRect r { SkRect::Make(it.rect()) };
        SkPoint vpts[4] { { r.fLeft, r.fTop }, { r.fRight, r.fTop }, { r.fRight, r.fBottom }, { r.fLeft, r.fBottom } };
        SkPoint ipts[4];
        vi.mapPoints(ipts, vpts, 4);
        emit(ipts[0], vpts[0]); emit(ipts[1], vpts[1]); emit(ipts[2], vpts[2]);
        emit(ipts[0], vpts[0]); emit(ipts[2], vpts[2]); emit(ipts[3], vpts[3]);
    }

    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    VkDeviceSize vboOffset { 0 };
    vkCmdBindVertexBuffers(m_cmd, 0, 1, &m_vbo, &vboOffset);
    vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pm->imageLayout(), 0, 1, &set, 0, nullptr);

    SkRect bounds; vi.mapRect(&bounds, SkRect::Make(region.getBounds()));
    VkRect2D scissor {};
    const int x0 { std::clamp((int)std::floor(bounds.left()), 0, W) };
    const int y0 { std::clamp((int)std::floor(bounds.top()), 0, H) };
    const int x1 { std::clamp((int)std::ceil(bounds.right()), 0, W) };
    const int y1 { std::clamp((int)std::ceil(bounds.bottom()), 0, H) };
    scissor.offset = { x0, y0 };
    scissor.extent = { (UInt32)std::max(0, x1 - x0), (UInt32)std::max(0, y1 - y0) };
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);

    RVKPushConstants pc {};
    pc.factor[0] = (effect == VibrancyH)
        ? imageInfo.srcScale / (float)imageInfo.src.width()
        : imageInfo.srcScale / (float)imageInfo.src.height(); // pixelSize
    vkCmdPushConstants(m_cmd, pm->imageLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    vkCmdDraw(m_cmd, quadCount * 6, 1, firstVertex, 0);
    return true;
}
