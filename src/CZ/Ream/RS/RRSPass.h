#ifndef CZ_RRSPASS_H
#define CZ_RRSPASS_H

#include <CZ/Ream/RPass.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>

class CZ::RRSPass : public RPass
{
public:
    ~RRSPass() noexcept;
    SkCanvas *getCanvas() const noexcept override;
    RPainter *getPainter() const noexcept override;
    void setGeometry(const RSurfaceGeometry &geometry) noexcept override;
protected:
    friend class RPass;
    RRSPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter,
            sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept;
};
#endif // CZ_RRSPASS_H
