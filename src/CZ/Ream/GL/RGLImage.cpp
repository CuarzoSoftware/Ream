#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RCore.h>

#include <xf86drm.h>

using namespace CZ;

std::shared_ptr<RGLImage> RGLImage::MakeFromPixels(const MakeFromPixelsParams &params, RGLDevice *allocator) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RError(RLINE, "Cannot create an RImage without an RCore.");
        return {};
    }

    if (core->graphicsAPI() != RGraphicsAPI::GL)
    {
        RError(RLINE, "The current RGraphicsAPI is not GL.");
        return {};
    }

    if (!allocator)
    {
        assert(core->mainDevice());
        allocator = core->mainDevice()->asGL();
    }

    if (params.size.isEmpty())
    {
        RError(RLINE, "Invalid image dimensions (%dx%d).", params.size.width(), params.size.height());
        return {};
    }

    const RFormatInfo *formatInfo { RDRMFormat::GetInfo(params.format) };

    if (!formatInfo)
    {
        RError(RLINE, "Unsupported image format %s", drmGetFormatName(params.format));
        return {};
    }

    const RGLFormat *glFormat { RGLFormat::FromDRM(params.format) };

    if (!glFormat)
    {
        RError(RLINE, "Failed to find GL equivalent for format %s", drmGetFormatName(params.format));
        return {};
    }


}
