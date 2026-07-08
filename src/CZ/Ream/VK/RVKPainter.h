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
    bool drawImageEffect(const RDrawImageInfo &image, ImageEffect effect, const SkRegion *region = nullptr) noexcept override;
    bool setGeometry(const RSurfaceGeometry &geometry) noexcept override;

    // Ends the render pass and submits recorded work (blocking). Called by ~RVKPass.
    void flush() noexcept;

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
