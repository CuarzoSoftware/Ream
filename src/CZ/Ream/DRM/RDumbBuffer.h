#ifndef RDUMBBUFFER_H
#define RDUMBBUFFER_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/skia/core/SkSize.h>
#include <memory>

class CZ::RDumbBuffer final : public RObject
{
public:
    static std::shared_ptr<RDumbBuffer> Make(SkISize size, const RDRMFormat &format, RDevice *allocator = nullptr) noexcept;

    ~RDumbBuffer() noexcept;
    std::shared_ptr<RCore> core() const noexcept { return m_core; }
    std::shared_ptr<RDRMFramebuffer> fb() const noexcept;
    SkISize size() const noexcept { return m_size; }
    const RFormatInfo &formatInfo() const noexcept { return *m_formatInfo; }
    RDevice *allocator() const noexcept { return m_allocator; }
    UInt32 handle() const noexcept { return m_handle; }
    UInt32 stride() const noexcept { return m_stride; }
    UInt8 *pixels() const noexcept { return m_data; }
private:
    RDumbBuffer(std::shared_ptr<RCore> core, std::shared_ptr<RDRMFramebuffer> fb, SkISize size, const RFormatInfo *formatInfo, RDevice *allocator, UInt32 handle,
                UInt32 stride, UInt64 mapSize, UInt8 *data) noexcept;
    std::shared_ptr<RCore> m_core;
    std::shared_ptr<RDRMFramebuffer> m_fb;
    SkISize m_size;
    const RFormatInfo *m_formatInfo;
    RDevice *m_allocator;
    UInt32 m_handle;
    UInt32 m_stride;
    UInt64 m_mapSize;
    UInt8 *m_data;
};

#endif // RDUMBBUFFER_H
