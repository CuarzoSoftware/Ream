#ifndef CZ_RVKPASS_H
#define CZ_RVKPASS_H

#include <CZ/Ream/RPass.h>

/**
 * @brief Vulkan render pass.
 *
 * Multiplexes an RVKPainter and a Skia SkCanvas over the same VkImage. On destruction it
 * flushes and submits any deferred Skia work so the rendering is realized on the queue before
 * the base RPass publishes the image's write RSync (an empty submit ordered after it on the
 * same queue), then reconciles the image's tracked VkImageLayout with Skia's.
 */
class CZ::RVKPass final : public RPass
{
public:
    ~RVKPass() noexcept;
    SkCanvas *getCanvas(bool sync = true) const noexcept override;
    RPainter *getPainter(bool sync = true) const noexcept override;
    void setGeometry(const RSurfaceGeometry &geometry) noexcept override;
private:
    friend class RPass;
    RVKPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter,
            sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept;

    // 0 / RPassCap_Painter / RPassCap_SkCanvas — which API last recorded work.
    mutable UInt32 m_lastUsage { 0 };
};

#endif // CZ_RVKPASS_H
