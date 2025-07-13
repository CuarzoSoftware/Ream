#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/VK/RVKCore.h>

using namespace CZ;

static std::weak_ptr<RCore> s_core;

RCore::RCore(const Options &options) noexcept : m_options(options)
{
    RLog(CZTrace, "+ (1) RCore");
}

std::shared_ptr<RCore> RCore::Make(const Options &options) noexcept
{
    auto instance { s_core.lock() };

    if (instance)
    {
        RLog(CZWarning, CZLN, "Trying to create an RCore twice. Use RCore::Get() to retreive the current instance instead");
        return nullptr;
    }

    if (!options.platformHandle)
    {
        RLog(CZFatal, CZLN, "Missing Options::platformHandle");
        goto fail;
    }

    if (options.graphicsAPI == RGraphicsAPI::VK || options.graphicsAPI == RGraphicsAPI::Auto)
    {
        auto core { std::shared_ptr<RCore>(new RVKCore(options)) };
        s_core = core;

        if (core->init())
            return core;

        if (options.graphicsAPI != RGraphicsAPI::Auto)
            goto fail;
    }

    {
        auto core { std::shared_ptr<RCore>(new RGLCore(options)) };
        s_core = core;

        if (core->init())
            return core;
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

RCore::~RCore() noexcept
{
    RLog(CZTrace, "- (0) RCore");
}

std::shared_ptr<CZ::RCore> RCore::Get() noexcept
{
    return s_core.lock();
}
