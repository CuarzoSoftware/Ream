#ifndef RCORE_H
#define RCORE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/RPlatformHandle.h>
#include <memory>
#include <thread>

/**
 * @brief Library entry point and global context.
 *
 * RCore initializes Ream, selecting the platform (DRM, Wayland, Offscreen) and graphics API
 * (Raster, OpenGL, Vulkan). It owns and exposes the available rendering devices (RDevice) and
 * reports information about the active platform and GAPI.
 *
 * A single instance exists per process; create it with RCore::Make() and access it globally via
 * RCore::Get(). The graphics API can be forced with the @c CZ_REAM_GAPI environment variable when
 * RGraphicsAPI::Auto is requested.
 */
class CZ::RCore : public RObject
{
public:
    /**
     * @brief Options used to initialize RCore.
     */
    struct Options
    {
        /**
         * @brief Graphics API to use.
         *
         * When set to RGraphicsAPI::Auto, the API is selected automatically and can be
         * forced through the @c CZ_REAM_GAPI environment variable (@c "GL", @c "VK" or @c "RS").
         */
        RGraphicsAPI graphicsAPI { RGraphicsAPI::Auto };

        /**
         * @brief Handle describing the target platform (DRM, Wayland, Offscreen).
         *
         * Must not be null; RCore::Make() fails otherwise.
         */
        std::shared_ptr<RPlatformHandle> platformHandle;
    };

    /**
     * @brief Creates and initializes the single RCore instance.
     *
     * Selects the graphics API and platform from @p options, enumerates the available devices,
     * and logs library information. An explicitly requested API does not silently fall back;
     * only RGraphicsAPI::Auto may fall back (e.g. to OpenGL).
     *
     * @param options Initialization options.
     * @return A shared pointer to the new RCore, or nullptr on failure or if an instance already exists.
     */
    static std::shared_ptr<RCore> Make(const Options &options) noexcept;

    /**
     * @brief Returns the current global RCore instance.
     *
     * @return The existing RCore instance, or nullptr if none has been created.
     */
    static std::shared_ptr<RCore> Get() noexcept;

    /**
     * @brief Clears resources pending destruction.
     *
     * This is primarily used by the OpenGL graphics API. For example,
     * an `RGLImage` may hold framebuffers, renderbuffers, or other GPU
     * resources that were created in contexts on other threads.
     *
     * Such resources are queued for deletion and will be released either
     * when the owning thread is destroyed or when this function is called.
     */
    virtual void clearGarbage() noexcept = 0;

    /**
     * @brief Returns all rendering devices enumerated by the library.
     *
     * @return A reference to the vector of available devices. Ownership remains with RCore.
     */
    const std::vector<RDevice*> &devices() const noexcept { return m_devices; }

    // The device used when an `RDevice*` parameter is `nullptr`.
    /**
     * @brief Returns the main (default) device.
     *
     * This is the device used whenever an @c RDevice* parameter is left as @c nullptr.
     *
     * @return The main device. Always valid after successful initialization.
     */
    RDevice *mainDevice() const noexcept { return m_mainDevice; }

    // Must not be called after creating images, surfaces, etc
    /**
     * @brief Overrides the main device.
     *
     * @warning Must not be called after creating images, surfaces, or other resources.
     *
     * @param device The device to set as main. Must not be null.
     * @return true if the device was set, false if @p device is null.
     */
    bool overrideMainDevice(RDevice *device) noexcept
    {
        if (!device) return false;
        m_mainDevice = device;
        return true;
    }

    /**
     * @brief Returns the active graphics API.
     *
     * @return The resolved graphics API (never RGraphicsAPI::Auto after initialization).
     */
    RGraphicsAPI graphicsAPI() const noexcept { return options().graphicsAPI; }

    /**
     * @brief Returns the active platform.
     *
     * @return The platform reported by the platform handle (DRM, Wayland, or Offscreen).
     */
    RPlatform platform() const noexcept { return options().platformHandle->platform(); }

    /**
     * @brief Returns the options RCore was created with.
     */
    const Options &options() const noexcept { return m_options; }

    /**
     * @brief Attempts to cast this RCore instance to an RGLCore.
     *
     * @return A shared pointer to RGLCore if this object is an instance of RGLCore,
     *         or `nullptr` otherwise.
     */
    std::shared_ptr<RGLCore> asGL() noexcept;

    /**
     * @brief Attempts to cast this RCore instance to an RRSCore.
     *
     * @return A shared pointer to RRSCore if the active API is Raster, or nullptr otherwise.
     */
    std::shared_ptr<RRSCore> asRS() noexcept;

    /**
     * @brief Attempts to cast this RCore instance to an RVKCore.
     *
     * @return A shared pointer to RVKCore if the active API is Vulkan, or nullptr otherwise.
     */
    std::shared_ptr<RVKCore> asVK() noexcept;

    /**
     * @brief Destructor.
     */
    ~RCore() noexcept;

protected:
    RCore(const Options &options) noexcept;
    virtual bool init() noexcept = 0;
    void logInfo() noexcept;
    Options m_options;
    RDevice *m_mainDevice { nullptr };
    std::vector<RDevice*> m_devices;
    std::thread::id m_mainThread;
};

#endif // RCORE_H
