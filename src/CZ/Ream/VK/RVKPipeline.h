#ifndef CZ_RVKPIPELINE_H
#define CZ_RVKPIPELINE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/RImageFilter.h>
#include <CZ/Ream/RImageWrap.h>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>

namespace CZ
{
    // Push constants shared by the painter shaders (fragment stage). 32 bytes.
    struct RVKPushConstants
    {
        float color[4];  // premultiplied (drawColor)
        float factor[4]; // rgb: per-channel factor, a: final alpha (drawImage)
    };

    // Fixed-function blend configuration (matches the GL painter's glBlendFunc choices).
    struct RVKBlend
    {
        bool enable;                                   ///< Whether color blending is enabled.
        VkBlendFactor srcColor, dstColor, srcAlpha, dstAlpha; ///< Source/destination color and alpha blend factors (blend op is ADD).
    };

    // Fragment specialization constants for image.frag.
    struct RVKImageSpec
    {
        UInt32 hasMask;           ///< 1 if a mask sampler is bound.
        UInt32 replaceImageColor; ///< 1 to replace the image RGB with the push-constant color.
        UInt32 premultSrc;        ///< 1 if the source is premultiplied alpha.
        UInt32 blendDstIn;        ///< 1 for the DstIn blend path.

        /** @brief Packs the four flags into a single bitfield (used as a pipeline cache key). */
        UInt32 pack() const noexcept { return hasMask | (replaceImageColor << 1) | (premultSrc << 2) | (blendDstIn << 3); }
    };
}

/**
 * @brief Device-owned cache of Vulkan render passes, pipelines, layouts, and samplers used by
 *        RVKPainter. Shared across all passes on the device.
 */
class CZ::RVKPipeline final : public RObject
{
public:
    /**
     * @brief Creates the pipeline cache for @p device (loads shader modules, layouts). Returns
     *        nullptr on failure.
     */
    static std::unique_ptr<RVKPipeline> Make(RVKDevice *device) noexcept;

    /** @brief Destroys all cached pipelines, render passes, samplers, layouts and shader modules. */
    ~RVKPipeline() noexcept;

    // A single-color-attachment render pass with loadOp=LOAD for the given format.
    VkRenderPass renderPass(VkFormat format) noexcept;

    /** @brief Returns (creating and caching if needed) the drawColor pipeline for the given
     *         render pass/format and blend state. */
    VkPipeline colorPipeline(VkRenderPass rp, VkFormat format, const RVKBlend &blend) noexcept;

    /** @brief Returns (creating and caching if needed) the drawImage pipeline for the given render
     *         pass/format, blend state, and fragment specialization (@p spec). */
    VkPipeline imagePipeline(VkRenderPass rp, VkFormat format, const RVKBlend &blend, const RVKImageSpec &spec) noexcept;
    // Vibrancy effect pipeline; fx = 1 (H), 2 (V light), 3 (V dark). Blending disabled.
    VkPipeline effectPipeline(VkRenderPass rp, VkFormat format, UInt32 fx) noexcept;

    /** @brief Returns (creating and caching if needed) a sampler for the given filter/wrap modes. */
    VkSampler sampler(RImageFilter min, RImageFilter mag, RImageWrap wrapS, RImageWrap wrapT) noexcept;

    /** @brief Pipeline layout for the color pipeline (push constants only). */
    VkPipelineLayout colorLayout() const noexcept { return m_colorLayout; }

    /** @brief Pipeline layout for the image/effect pipelines (image+mask sampler set + push constants). */
    VkPipelineLayout imageLayout() const noexcept { return m_imageLayout; }

    /** @brief Descriptor set layout of the image+mask combined image samplers. */
    VkDescriptorSetLayout imageSetLayout() const noexcept { return m_imageSetLayout; }
private:
    RVKPipeline(RVKDevice *device) noexcept : m_dev(device) {}
    bool init() noexcept;
    VkShaderModule loadModule(const UInt32 *code, size_t bytes) noexcept;
    // frag: 0 = color, 1 = image, 2 = effect
    VkPipeline buildPipeline(VkRenderPass rp, int frag, const RVKBlend &blend, const void *specData, UInt32 specCount) noexcept;

    RVKDevice *m_dev;
    VkShaderModule m_vert { VK_NULL_HANDLE };
    VkShaderModule m_colorFrag { VK_NULL_HANDLE };
    VkShaderModule m_imageFrag { VK_NULL_HANDLE };
    VkShaderModule m_effectFrag { VK_NULL_HANDLE };
    VkPipelineCache m_cache { VK_NULL_HANDLE };
    VkDescriptorSetLayout m_imageSetLayout { VK_NULL_HANDLE };
    VkPipelineLayout m_colorLayout { VK_NULL_HANDLE };
    VkPipelineLayout m_imageLayout { VK_NULL_HANDLE };

    std::unordered_map<UInt32, VkRenderPass> m_renderPasses;   // key: VkFormat
    std::unordered_map<UInt64, VkPipeline> m_pipelines;        // key: composed
    std::unordered_map<UInt32, VkSampler> m_samplers;          // key: filter/wrap bits
};

#endif // CZ_RVKPIPELINE_H
