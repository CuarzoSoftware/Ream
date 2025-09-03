#include <CZ/Ream/WL/RWLSwapchain.h>
#include <CZ/Ream/GL/RGLSwapchainWL.h>
#include <CZ/Ream/RS/RRSSwapchainWL.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

using namespace CZ;

std::shared_ptr<RWLSwapchain> RWLSwapchain::Make(wl_surface *surface, SkISize size) noexcept
{
    if (!surface)
    {
        RLog(CZError, CZLN, "Invalid wl_surface (nullptr)");
        return {};
    }

    if (size.isEmpty())
    {
        RLog(CZError, CZLN, "Invalid size");
        return {};
    }

    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    std::shared_ptr<RWLSwapchain> ss;

    if (core->asGL())
        ss = RGLSwapchainWL::Make(surface, size);
    else if (core->asRS())
        ss = RRSSwapchainWL::Make(surface, size);

    if (!ss)
        RLog(CZError, CZLN, "Failed to create RWLSwapchain");

    return ss;
}
