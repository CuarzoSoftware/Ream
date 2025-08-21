#ifndef RSKPASS_H
#define RSKPASS_H

#include <CZ/Ream/RSurface.h>
#include <CZ/skia/core/SkSurface.h>

/**
 * @brief A Skia render pass.
 *
 * An RSKPass allows issuing rendering commands through an SkCanvas into a target RSurface.
 *
 * It is created via RSurface::beginSKPass().
 *
 * Rendering commands can be recorded via the associated SkCanvas, accessible using the
 * call operator `operator()`. Once all rendering commands are recorded, you must call
 * end() to finalize the pass.
 *
 * @warning If the pass is in an invalid state (see isValid()), it MUST NOT be used.
 *          Issuing rendering commands on an invalid pass may lead to undefined behavior or a segmentation fault.
 *
 * The pass is automatically ended on destruction if it hasn't been explicitly ended.
 */
class CZ::RSKPass
{
public:

    static RSKPass MakeInvalid() noexcept { return RSKPass(); }

    /// Automatically ends the pass if still valid.
    ~RSKPass() noexcept { end(); }

    /**
     * @brief Checks whether the pass is still valid.
     *
     * If false, no further rendering can be performed, and using the SkCanvas is unsafe.
     * The pass becomes invalid after calling end() or if it was never properly initialized.
     *
     * @return true if rendering is allowed, false otherwise.
     */
    bool isValid() const noexcept { return m_image != nullptr; }

    /**
     * @brief Provides access to the underlying SkCanvas.
     */
    SkCanvas *operator()() const noexcept {
        assert(isValid() && "Attempt to use an invalid RSKPass");
        return m_canvas;
    }

    /**
     * @brief Ends the render pass.
     *
     * Finalizes the rendering pass. After calling this method, the pass becomes invalid,
     * and no further rendering should be attempted.
     *
     * This method is automatically called by the destructor if not explicitly invoked.
     *
     * @return true if the pass was valid and has now been ended, false if the pass was already invalid.
     */
    bool end() noexcept;

    RSKPass(const RSKPass&) = delete;
    RSKPass& operator=(const RSKPass&) = delete;

    RSKPass(RSKPass&& other) noexcept
    {
        m_glCurrent = std::move(other.m_glCurrent);
        m_image = std::move(other.m_image);
        m_device = other.m_device;
        m_canvas = other.m_canvas;
        other.m_device = nullptr;
        other.m_canvas = nullptr;
    }

    // Move assignment
    RSKPass& operator=(RSKPass&& other) noexcept
    {
        if (this != &other) {
            end();
            m_glCurrent = std::move(other.m_glCurrent);
            m_image = std::move(other.m_image);
            m_device = other.m_device;
            m_canvas = other.m_canvas;
            other.m_device = nullptr;
            other.m_canvas = nullptr;
        }
        return *this;
    }
private:
    friend class RSurface;
    RSKPass() noexcept = default;
    RSKPass(const SkMatrix &matrix, std::shared_ptr<RImage> image = nullptr, RDevice *device = nullptr) noexcept;
    std::shared_ptr<RGLMakeCurrent> m_glCurrent;
    RDevice *m_device {};
    SkCanvas *m_canvas {};
    std::shared_ptr<RImage> m_image;
};
#endif // RSKPASS_H
