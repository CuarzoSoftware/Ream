#ifndef RGLCORE_H
#define RGLCORE_H

#include <CZ/Ream/RCore.h>
#include <CZ/Ream/EGL/REGLExtensions.h>
#include <CZ/Ream/GL/RGLExtensions.h>

/**
 * @brief OpenGL backend implementation of RCore.
 *
 * Entry point for the OpenGL ES / EGL graphics API. It binds the EGL client API, queries the
 * client-level EGL extensions, and enumerates the rendering devices (RGLDevice), one per DRM node
 * (or a single Wayland device). Obtain it from an RCore via RCore::asGL().
 *
 * Because the GL backend keeps per-thread EGL contexts and defers destruction of resources created
 * on other threads, it also drives garbage collection through clearGarbage() and freeThread().
 */
class CZ::RGLCore : public RCore
{
public:
    ~RGLCore();

    /**
     * @brief Returns the main rendering device as an RGLDevice.
     */
    RGLDevice *mainDevice() const noexcept { return (RGLDevice*)m_mainDevice; }

    /**
     * @brief Returns the client-level EGL extensions available without a display.
     */
    const REGLClientExtensions &clientEGLExtensions() const noexcept { return m_clientEGLExtensions; }

    /**
     * @brief Returns the client-level EGL function pointers loaded at initialization.
     */
    const REGLClientProcs &clientEGLProcs() const noexcept { return m_clientEGLProcs; }

    /**
     * @brief Releases resources on the calling thread that were queued for destruction.
     *
     * Frees GL resources (contexts, RGLContextData, etc.) that were destroyed from other threads
     * and could not be freed there because their context was not current.
     */
    void clearGarbage() noexcept override;

    /**
     * @brief Tears down all GL state owned by the calling thread.
     *
     * Destroys the per-thread EGL/Skia contexts and context data for this thread. Called
     * automatically when a thread terminates; may be invoked explicitly to release early.
     */
    void freeThread() noexcept;
private:
    friend class RCore;
    friend struct RGLThreadDataManager;
    friend class RGLContextDataManager;
    bool init() noexcept override;
    bool initClientEGLExtensions() noexcept;
    bool initDevices() noexcept;

    void unitDevices() noexcept;
    RGLCore(const Options &options) noexcept;
    REGLClientExtensions m_clientEGLExtensions {};
    REGLClientProcs m_clientEGLProcs {};
};

#endif // RGLCORE_H
