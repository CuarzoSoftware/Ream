#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

using namespace CZ;

std::shared_ptr<RImage> RImage::MakeFromPixels(const MakeFromPixelsParams &params, RDevice *allocator) noexcept
{
    auto core { RCore::Get() };

    assert(core);

    if (core->graphicsAPI() == RGraphicsAPI::GL)
        return RGLImage::MakeFromPixels(params, (RGLDevice*)allocator);

    return {};
}

RImage::RImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RDRMFormat &format) noexcept :
    m_size(size),
    m_format(format),
    m_allocator(device),
    m_core(core)
{
    assert(!size.isEmpty());
    assert(device);
    assert(core);
}
