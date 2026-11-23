#ifndef CZ_RVKPAINTER_H
#define CZ_RVKPAINTER_H

#include <CZ/Ream/RPainter.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

/**
 * @brief Vulkan implementation of RPainter (native SPIR-V shaders).
 *
 * Records drawing commands into a per-pass command buffer inside a VkRenderPass targeting the
 * surface image, and submits at flush() (called by ~RVKPass). Positions/UVs are pre-transformed
 * to NDC/normalized image coords on the CPU; blend modes map to fixed-function blend state.
 */
class CZ::RVKPainter final : public RPainter
{
public:
    bool drawImage(const RDrawImageInfo &image, const SkRegion *region = nullptr, const RDrawImageInfo *mask = nullptr) noexcept override;
    bool drawColor(const SkRegion &region) noexcept override;
    bool drawImageEffect(const RDrawImageInfo &image, ImageEffect effect, const SkRegion *region = nullptr,
                         const RImageEffectParams &params = {}) noexcept override;
    bool setGeometry(const RSurfaceGeometry &geometry) noexcept override;

    // Ends the render pass and submits recorded work (blocking). Called by ~RVKPass.
    void flush() noexcept;

    /**
     * @brief Submits the work of every painter of this thread still recording (but @p except).
     *
     * Layouts are tracked as commands are recorded, so they must be submitted in that order too:
     * a painter starting to record, or Skia about to draw (see RVKPass::getCanvas()), submits the
     * ones recorded before. Passes nest (e.g. baking within a frame), so several may be recording.
     */
    static void FlushRecording(RVKPainter *except = nullptr) noexcept;

    ~RVKPainter() noexcept;
private:
    friend class RVKDevice;
    RVKPainter(std::shared_ptr<RSurface> surface, RVKDevice *device) noexcept;
    RVKDevice *dev() const noexcept;

    bool beginRecording() noexcept;              // begin cmd buffer + transition target
    void ensureRenderPass() noexcept;            // begin render pass lazily (after source transitions)
    void endRenderPassIfActive() noexcept;
    void transitionSource(const std::shared_ptr<RImage> &img) noexcept; // -> SHADER_READ_ONLY, outside render pass
    float *reserveVertices(UInt32 vertexCount, UInt32 &firstVertex) noexcept; // returns mapped write ptr

    // Per-pass resources, chained when full (a pass may draw any number of images/vertices):
    // the full ones are kept until the pass is done with them (freed with it).
    bool createDescPool() noexcept;
    bool createVbo() noexcept;
    VkDescriptorSet allocDescriptorSet(VkDescriptorSetLayout layout) noexcept;
    struct Vbo { VkBuffer buffer; VkDeviceMemory memory; };
    std::vector<VkDescriptorPool> m_fullDescPools;
    std::vector<Vbo> m_fullVbos;

    RVKImage *m_target { nullptr };
    VkFormat m_format { VK_FORMAT_UNDEFINED };
    VkRenderPass m_rp { VK_NULL_HANDLE };
    VkFramebuffer m_framebuffer { VK_NULL_HANDLE };
    VkCommandPool m_pool { VK_NULL_HANDLE };
    VkCommandBuffer m_cmd { VK_NULL_HANDLE };
    VkDescriptorPool m_descPool { VK_NULL_HANDLE };
    bool m_recording { false };
    bool m_renderPassActive { false };

    // Source images read during this pass; assigned a read sync at flush.
    std::vector<std::shared_ptr<RImage>> m_readImages;

    VkBuffer m_vbo { VK_NULL_HANDLE };
    VkDeviceMemory m_vboMem { VK_NULL_HANDLE };
    void *m_vboMapped { nullptr };
    VkDeviceSize m_vboCapacity { 0 };
    UInt32 m_vertexCount { 0 }; // vertices written so far (6 floats each)
};

#endif // CZ_RVKPAINTER_H
