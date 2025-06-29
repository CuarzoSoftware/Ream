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

    const std::vector<RDevice*> &devices() const noexcept { return m_devices; }
    RDevice *mainDevice() const noexcept { return m_mainDevice; }
    RGraphicsAPI graphicsAPI() const noexcept { return options().graphicsAPI; }
    RPlatform platform() const noexcept { return options().platformHandle->platform(); }
    const Options &options() const noexcept { return m_options; }

    virtual void unbindCurrentThread() noexcept = 0;

    /**
     * @brief Attempts to cast this RCore instance to an RGLCore.
     *
     * @return A shared pointer to RGLCore if this object is an instance of RGLCore,
     *         or `nullptr` otherwise.
     */
    std::shared_ptr<RGLCore> asGL() noexcept;
protected:
    RCore(const Options &options) noexcept;
    virtual bool init() noexcept = 0;
    Options m_options;
    RDevice *m_mainDevice { nullptr };
    std::vector<RDevice*> m_devices;
};

#endif // RCORE_H
