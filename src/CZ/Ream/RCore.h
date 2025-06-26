#ifndef RCORE_H
#define RCORE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/RPlatformHandle.h>
#include <memory>

class CZ::RCore : public RObject
{
public:
    struct Options
    {
        RGraphicsAPI graphicsAPI { RGraphicsAPI::Auto };
        std::shared_ptr<RPlatformHandle> platformHandle;
    };

    static std::shared_ptr<RCore> Make(const Options &options) noexcept;
    static std::shared_ptr<RCore> Get() noexcept;

    virtual void bindDevice(RDevice *device = nullptr) = 0;

    const std::vector<RDevice*> &devices() const noexcept { return m_devices; }
    RDevice *mainDevice() const noexcept { return m_mainDevice; }
    RDevice *boundDevice() const noexcept { return m_boundDevice; }
    RGraphicsAPI graphicsAPI() const noexcept { return options().graphicsAPI; }
    RPlatform platform() const noexcept { return options().platformHandle->platform(); }
    const Options &options() const noexcept { return m_options; }

    RGLCore *asGL() noexcept
    {
        if (graphicsAPI() == RGraphicsAPI::GL)
            return (RGLCore*)this;
        return nullptr;
    }

protected:
    RCore(const Options &options) noexcept;
    virtual bool init() noexcept = 0;
    Options m_options;
    RDevice *m_mainDevice { nullptr };
    RDevice *m_boundDevice { nullptr };
    std::vector<RDevice*> m_devices;
};

#endif // RCORE_H
