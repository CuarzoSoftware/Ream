#ifndef RSWAPCHAIN_H
#define RSWAPCHAIN_H

#include <CZ/skia/core/SkSize.h>
#include <CZ/Ream/RObject.h>
#include <memory>
#include <optional>

namespace CZ
{
    struct RSwapchainImage
    {
        UInt32 index;
        UInt32 age;
        std::shared_ptr<RImage> image;
    };
};

/**
 * @brief Abstract base class representing a rendering swapchain.
 *
 * A swapchain manages a set of images that can be acquired, rendered into,
 * and then presented to the display. This class provides an interface for
 * acquiring images, presenting rendered content, and handling resize events.
 */
class CZ::RSwapchain : public RObject
{
public:
    /**
     * @brief Construct a swapchain with the given image size.
     */
    RSwapchain(SkISize size) noexcept : m_size(size) {}

    /**
     * @brief The size of swapchain images in pixels.
     */
    SkISize size() const noexcept { return m_size; }

    /**
     * @brief Acquire the next available swapchain image.
     *
     * @return A new swapchain image on success, or nullopt on failure.
     */
    virtual std::optional<const RSwapchainImage> acquire() noexcept = 0;

    /**
     * @brief Present a previously acquired image to the display.
     *
     * @param image   The image to be presented.
     * @param damage  Region of the image that has changed since the last frame.
     *                - Specified in buffer coordinates, relative to the top-left corner.
     *                - May extend beyond the image bounds.
     *                - Passing @c nullptr indicates full damage.
     *                - Passing an empty region indicates no damage.
     *
     * @return @c true if the presentation succeeded, otherwise @c false.
     */
    virtual bool present(const RSwapchainImage &image, SkRegion *damage = nullptr) noexcept = 0;

    /**
     * @brief Request a resize of future swapchain images.
     *
     * This affects only newly acquired images; currently acquired images
     * remain at the previous size until released.
     *
     * @param size The new desired image size in pixels.
     * @return @c true if the resize request was accepted, otherwise @c false.
     */
    virtual bool resize(SkISize size) noexcept = 0;
protected:
    SkISize m_size;
};

#endif // RSWAPCHAIN_H
