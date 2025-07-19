#ifndef RGBMBO_H
#define RGBMBO_H

#include <CZ/CZ.h>
#include <CZ/Ream/RObject.h>
#include <memory>
#include <gbm.h>

struct gbm_bo;

class CZ::RGBMBo : public RObject
{
public:
    static std::shared_ptr<RGBMBo> Make(SkISize size, const RDRMFormat &format, RDevice *allocator = nullptr) noexcept;
    const RDMABufferInfo &dmaInfo() const noexcept { return m_dmaInfo; }
    RDevice *allocator() const noexcept { return m_allocator; }
    bool hasModifier() const noexcept { return m_hasModifier; }
    RModifier modifier() const noexcept { return m_dmaInfo.modifier; }
    int planeCount() const noexcept { return m_dmaInfo.planeCount; }
    union gbm_bo_handle planeHandle(int planeIndex) const noexcept;
    bool supportsMapRead() const noexcept;
    bool supportsMapWrite() const noexcept;
    gbm_bo *bo() const noexcept { return m_bo; }
    ~RGBMBo() noexcept;
private:
    RGBMBo(std::shared_ptr<RCore> core, RDevice *allocator, gbm_bo *bo, CZOwnership ownership, bool hasModifier, const RDMABufferInfo &dmaInfo) noexcept;
    gbm_bo *m_bo;
    RDMABufferInfo m_dmaInfo;
    CZOwnership m_ownership;
    RDevice *m_allocator;
    std::shared_ptr<RCore> m_core;
    // 2: unchecked, 1: supported, 0: unsupported
    mutable UInt8 m_supportsMapRead { 2 };
    mutable UInt8 m_supportsMapWrite { 2 };
    bool m_hasModifier;
};

#endif // RGBMBO_H
