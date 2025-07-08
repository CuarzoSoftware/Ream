#ifndef RDRMFRAMEBUFFER_H
#define RDRMFRAMEBUFFER_H

#include <memory>
#include <CZ/Ream/RObject.h>

class CZ::RDRMFramebuffer final : public RObject
{
public:
    static std::shared_ptr<RDRMFramebuffer> MakeFromGBMBo(std::shared_ptr<RGBMBo> bo) noexcept;
    ~RDRMFramebuffer() noexcept;

private:
    RDRMFramebuffer(std::shared_ptr<RCore> core, RDevice *device, UInt32 id, Int32 width, Int32 height, bool hasModifier, const std::vector<UInt32> &handles, CZOwnership handlesOwnership) noexcept;
    UInt32 m_id;
    Int32 m_width, m_height;
    RDevice *m_device;
    std::shared_ptr<RGBMBo> m_gbmBo;
    std::shared_ptr<RCore> m_core;
    std::vector<UInt32> m_handles;
    CZOwnership m_handlesOwn;
    bool m_hasModifier;
};

#endif // RDRMFRAMEBUFFER_H
