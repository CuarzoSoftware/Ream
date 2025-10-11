#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

#include <CZ/Ream/RResourceTracker.h>

#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <CZ/Ream/RS/RRSCore.h>

#include <CZReamVersion.h>

#include <cstring>

using namespace CZ;

static std::weak_ptr<RCore> s_core;

RCore::RCore(const Options &options) noexcept : m_options(options)
{
    RResourceTrackerAdd(RCoreRes);
}

void RCore::logInfo() noexcept
{
    if (RLog.level() >= CZInfo)
    {
        printf("\n");
        RLog(CZInfo, "--------------- Ream ---------------");
        RLog(CZInfo, "Version: {}.{}.{}", CZ_REAM_VERSION_MAJOR, CZ_REAM_VERSION_MINOR, CZ_REAM_VERSION_PATCH);
        RLog(CZInfo, "Graphics API: {}", RGraphicsAPIString(graphicsAPI()));
        RLog(CZInfo, "Platform: {}", RPlatformString(platform()));
        RLog(CZInfo, "Main Device: {}", mainDevice()->drmNode());
        RLog(CZInfo, "Devices:");
        for (auto *dev : devices())
        {
            RLog(CZInfo, "    {}:", dev->drmNode());
            RLog(CZInfo, "        Driver: {}", dev->drmDriverName());
            RLog(CZInfo, "        GBM: {}", dev->gbmDevice() != nullptr);
            RLog(CZInfo, "        AddFb2Modifiers: {}", dev->caps().AddFb2Modifiers);
            RLog(CZInfo, "        DumbBuffer: {}", dev->caps().DumbBuffer);
            RLog(CZInfo, "        Sync GPU: {}", dev->caps().SyncGPU);
            RLog(CZInfo, "        Sync CPU: {}", dev->caps().SyncCPU);
            RLog(CZInfo, "        Sync Import: {}", dev->caps().SyncImport);
            RLog(CZInfo, "        Sync Export: {}", dev->caps().SyncExport);
            RLog(CZInfo, "        Timeline: {}", dev->caps().Timeline);
        }
        RLog(CZInfo, "------------------------------------\n");
    }
}

std::shared_ptr<RCore> RCore::Make(const Options &options) noexcept
{
    auto instance { s_core.lock() };

    if (instance)
    {
        RLog(CZWarning, CZLN, "Trying to create an RCore twice. Use RCore::Get() to retreive the current instance instead");
        return nullptr;
    }

    auto gAPI { options.graphicsAPI };

    if (!options.platformHandle)
    {
        RLog(CZFatal, CZLN, "Missing Options::platformHandle");
        goto fail;
    }

    if (gAPI == RGraphicsAPI::Auto)
    {
        const char *gAPIEnv { getenv("CZ_REAM_GAPI") };

        if (gAPIEnv)
        {
            if (strcmp(gAPIEnv, "GL") == 0)
                gAPI = RGraphicsAPI::GL;
            else if (strcmp(gAPIEnv, "VK") == 0)
                gAPI = RGraphicsAPI::VK;
            else if (strcmp(gAPIEnv, "RS") == 0)
                gAPI = RGraphicsAPI::RS;
        }
    }

    if (gAPI == RGraphicsAPI::RS)
    {
        auto core { std::shared_ptr<RCore>(new RRSCore(options)) };
        s_core = core;

        if (core->init())
        {
            core->logInfo();
            return core;
        }

        if (gAPI != RGraphicsAPI::Auto)
            goto fail;
    }

    if (options.platformHandle->platform() == RPlatform::Offscreen)
    {
        RLog(CZFatal, CZLN, "The offscreen platform supports only the Raster GAPI");
        goto fail;
    }

    if (gAPI == RGraphicsAPI::VK)
    {
        RLog(CZWarning, CZLN, "The Vulkan GAPI is still incomplete. Falling back to OpenGL...");

        /*
        auto core { std::shared_ptr<RCore>(new RVKCore(options)) };
        s_core = core;

        if (core->init())
        {
            core->logInfo();
            return core;
        }

        if (gAPI != RGraphicsAPI::Auto)
            goto fail;*/
    }

    {
        auto core { std::shared_ptr<RCore>(new RGLCore(options)) };
        s_core = core;

        if (core->init())
        {
            core->logInfo();
            return core;
        }
    }

fail:
    RLog(CZFatal, CZLN, "Failed to create RCore");
    return nullptr;
}

std::shared_ptr<RGLCore> RCore::asGL() noexcept
{
    if (graphicsAPI() == RGraphicsAPI::GL)
        return std::static_pointer_cast<RGLCore>(s_core.lock());
    return {};
}

std::shared_ptr<RRSCore> RCore::asRS() noexcept
{
    if (graphicsAPI() == RGraphicsAPI::RS)
        return std::static_pointer_cast<RRSCore>(s_core.lock());
    return {};
}

RCore::~RCore() noexcept
{
    RLog(CZTrace, "RCore destroyed");
    RResourceTrackerSub(RCoreRes);
}

std::shared_ptr<RCore> RCore::Get() noexcept
{
    return s_core.lock();
}
