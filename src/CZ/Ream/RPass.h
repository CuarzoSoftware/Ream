#ifndef CZ_RPASS_H
#define CZ_RPASS_H

#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/RLockGuard.h>

/**
 * @brief A Ream render pass.
 *
 * An RPass allows issuing rendering commands through an RPainter into a target RSurface.
 *
 * It is created via RSurface::beginPass().
 *
 * Rendering commands can be recorded via the associated RPainter, accessible using the
 * call operator `operator()`. Once all rendering commands are recorded, you must call
 * end() to finalize the pass.
 *
 * @warning If the pass is in an invalid state (see isValid()), it MUST NOT be used.
 *          Issuing rendering commands on an invalid pass may lead to undefined behavior or a segmentation fault.
 *
 * The pass is automatically ended on destruction if it hasn't been explicitly ended.
 */
class CZ::RPass
{
public:

    /// Automatically ends the pass if still valid.
    ~RPass() noexcept { end(); }

    /**
     * @brief Checks whether the pass is still valid.
     *
     * If false, no further rendering can be performed, and using the RPainter is unsafe.
     * The pass becomes invalid after calling end() or if it was never properly initialized.
     *
     * @return true if rendering is allowed, false otherwise.
     */
    bool isValid() const noexcept { return m_surface != nullptr; }

    /**
     * @brief Provides access to the underlying RPainter.
     */
    RPainter* operator()() const noexcept { return m_painter; }

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

    RPass(const RPass&) = delete;
    RPass& operator=(const RPass&) = delete;

    RPass(RPass&&) noexcept = default;
    RPass& operator=(RPass&&) noexcept = default;
private:
    friend class RSurface;
    RPass(std::shared_ptr<RSurface> surface = nullptr, RPainter *painter = nullptr) noexcept;
    RPainter *m_painter {};
    std::shared_ptr<RSurface> m_surface;
    RLockGuard m_lock;
};

#endif // CZ_RPASS_H
