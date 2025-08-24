#ifndef CZ_RGLPASS_H
#define CZ_RGLPASS_H

#include <CZ/Ream/RPass.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>

class CZ::RGLPass : public RPass
{
public:
    ~RGLPass() noexcept;
    SkCanvas *getCanvas() const noexcept override;
    RPainter *getPainter() const noexcept override;
    void setGeometry(const RSurfaceGeometry &geometry) noexcept override;
private:
    friend class RPass;
    RGLPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter,
          sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept;
    void updateCurrent(RPassCap cap) const noexcept;
    mutable UInt32 m_lastUsage { 0 };
    mutable std::unique_ptr<RGLMakeCurrent> m_current;
};

#endif // CZ_RGLPASS_H
