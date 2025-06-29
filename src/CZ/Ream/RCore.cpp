#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

#include <CZ/Ream/GL/RGLCore.h>

using namespace CZ;

static std::weak_ptr<RCore> s_core;

RCore::RCore(const Options &options) noexcept : m_options(options) {}

std::shared_ptr<RCore> RCore::Make(const Options &options) noexcept
{
    RLogInit();

    auto instance { s_core.lock() };

    if (instance)
    {
        RWarning(RLINE, "Trying to create an RCore twice. Use RCore::Get() to retreive the current instance instead.");
        return nullptr;
    }

    if (!options.platformHandle)
    {
        RError(RLINE, "Missing Options::platformHandle.");
        goto fail;
    }

    if (options.graphicsAPI == RGraphicsAPI::Vk)
    {
        RError(RLINE, "Vulkan API not supported.");
        goto fail;
    }

    {
        auto core { std::shared_ptr<RCore>(new RGLCore(options)) };

        if (core->init())
        {
            s_core = core;
            return core;
        }
    }

fail:
    RFatal(RLINE, "Failed to create RCore.");
    return nullptr;
}

std::shared_ptr<RGLCore> RCore::asGL() noexcept
{
    if (graphicsAPI() == RGraphicsAPI::GL)
        return std::static_pointer_cast<RGLCore>(s_core.lock());
    return {};
}

std::shared_ptr<CZ::RCore> RCore::Get() noexcept
{
    return s_core.lock();
}
