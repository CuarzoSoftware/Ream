#ifndef RPAINTER_H
#define RPAINTER_H

#include <CZ/skia/core/SkRRect.h>
#include <CZ/skia/core/SkColor.h>
#include <CZ/Ream/RObject.h>
#include <CZ/Ream/RImageFilter.h>
#include <CZ/Ream/RImageWrap.h>
#include <CZ/Ream/RBlendMode.h>
#include <CZ/CZTransform.h>
#include <CZ/CZBitset.h>
#include <memory>

#include <CZ/skia/core/SkBlendMode.h>

namespace CZ
{
    /**
     * @brief Describes how to draw an image (or a portion of it) onto an RSurface's viewport.
     *
     * This struct is used by RPainter::drawImage() to define source and destination rects,
     * transformations, and sampling options for drawing an RImage.
     */
    struct RDrawImageInfo
    {
        /**
         * @brief The image to draw.
         *
         * If nullptr, drawImage() will fail or return `false`.
         */
        std::shared_ptr<RImage> image;

        /**
         * @brief Transformation applied to the source rect before scaling.
         *
         * Defaults to the identity transformation (no change).
         */
        CZTransform srcTransform { CZTransform::Normal };

        /**
         * @brief Scale factor applied to `src`.
         */
        SkScalar srcScale { 1.f };

        /**
         * @brief Portion of the image to draw.
         *
         * This rect can extend beyond the image bounds. It is affected by both
         * `srcTransform` and `srcScale`.
         *
         * @note To use pixel coordinates set `srcScale` to 1.f.
         *
         * If width or height are <= 0.f, they will be clamped to a small positive delta.
         */
        SkRect src {};

        /**
         * @brief Target rect within the RSurface viewport where the image will be drawn.
         *
         * For example, if the RSurface is positioned at (100, 100) and you want to draw
         * the image in its top-left corner with a size of (width, height), then `dst`
         * should be set to XYWH(100, 100, width, height).
         */
        SkIRect dst {};

        /**
         * @brief Filter to use when the image is minified (i.e., scaled down).
         */
        RImageFilter minFilter { RImageFilter::Linear };

        /**
         * @brief Filter to use when the image is magnified (i.e., scaled up).
         */
        RImageFilter magFilter { RImageFilter::Linear };

        /**
         * @brief Wrapping mode for horizontal (S-axis) texture coordinates.
         */
        RImageWrap wrapS { RImageWrap::ClampToEdge };

        /**
         * @brief Wrapping mode for vertical (T-axis) texture coordinates.
         */
        RImageWrap wrapT { RImageWrap::ClampToEdge };
    };
};

class CZ::RPainter : public RObject
{
public:

    /**
     * @brief Rendering options.
     */
    enum Option : UInt32
    {
        /**
         * @brief Replaces the image's RGB values with the value from color(), while preserving the image's alpha.
         *
         * This effectively tints the image with a solid color. Disabled by default.
         */
        ReplaceImageColor = 1u << 0,

        /**
         * @brief Indicates that color() is premultiplied alpha.
         *
         * Disabled by default.
         */
        ColorIsPremult = 1u << 1
    };

    struct State
    {
        CZBitset<Option> options { 0 };
        RBlendMode blendMode { RBlendMode::SrcOver };
        SkScalar opacity { 1.f };
        SkColor color { SK_ColorBLACK };
        SkColor4f factor { 1.f, 1.f, 1.f, 1.f };
    };

    const State &state() const noexcept { return m_state; }
    void setState(const State &state) noexcept
    {
        m_state = state;
        setOpacity(m_state.opacity);
    }

    /**
     * @brief Saves the current RPainter state.
     */
    void save() noexcept;

    /**
     * @brief Restores the most recently saved RPainter state.
     *
     * If no saved state exists, the painter is reset to its default values.
     */
    void restore() noexcept;

    /**
     * @brief Resets the current RPainter state to its default configuration.
     *
     * Does not affect the saved state history.
     */
    void reset() noexcept;

    /**
     * @brief Enables a rendering option.
     *
     * @param option The option to enable.
     */
    void enable(Option option) noexcept { m_state.options.add(option); }

    /**
     * @brief Disables a rendering option.
     *
     * @param option The option to disable.
     */
    void disable(Option option) noexcept { m_state.options.remove(option); }

    /**
     * @brief Sets the current rendering options, replacing any previously set flags.
     *
     * @param options The new set of options to apply. Defaults to none.
     */
    void setOptions(CZBitset<Option> options = 0) { m_state.options = options; }

    /**
     * @brief Sets the current blend mode used for drawing operations.
     *
     * This affects both drawColor() and drawImage().
     * The default blend mode is SrcOver.
     *
     * @param mode The blend mode to set.
     */
    void setBlendMode(RBlendMode mode = RBlendMode::SrcOver) noexcept { m_state.blendMode = mode; }

    /**
     * @brief Returns the current blend mode.
     */
    RBlendMode blendMode() const noexcept { return m_state.blendMode; }

    /**
     * @brief Sets the global opacity for drawing operations.
     *
     * This affects both drawImage() and drawColor().
     * The opacity value is clamped to the range [0, 1].
     *
     * @param opacity The opacity to set, where 0 is fully transparent and 1 is fully opaque.
     *                Defaults to 1.0 (fully opaque).
     */
    void setOpacity(SkScalar opacity = 1.f) noexcept { m_state.opacity = std::clamp(opacity, 0.f, 1.f); }

    /**
     * @brief Returns the current global opacity.
     *
     * @return The opacity value in the range [0, 1].
     */
    SkScalar opacity() const noexcept { return m_state.opacity; }

    /**
     * @brief Specifies the color used by drawColor() and clearSurface().
     *
     * The default color is black.
     *
     * When Option::ReplaceImageColor is enabled, this color replaces the RGB components
     * of the RImage in drawImage().
     *
     * Use Option::ColorIsPremult to indicate if this color uses premultiplied alpha.
     *
     * @param color The color to set.
     */
    void setColor(SkColor color = SK_ColorBLACK) noexcept { m_state.color = color; }

    /**
     * @brief Returns the current drawing color.
     *
     * @return The color currently set.
     */
    SkColor color() const noexcept { return m_state.color; }

    /**
     * @brief Sets per-channel multiplication factors applied in drawColor() and drawImage().
     *
     * Each RGBA component of the drawn color/image is multiplied by the corresponding factor.
     * Factors can exceed the [0, 1] range.
     *
     * A factor value of 1 disables multiplication for that channel (the shader skips it for performance).
     *
     * Defaults to {1.f, 1.f, 1.f, 1.f}.
     *
     * @param factor Multiplication factors for red, green, blue, and alpha channels.
     */
    void setFactor(const SkColor4f &factor = {1.f, 1.f, 1.f, 1.f}) noexcept { m_state.factor = factor; }

    /**
     * @brief Returns the current per-channel multiplication factors.
     *
     * @return The RGBA factors currently set.
     */
    const SkColor4f &factor() const noexcept { return m_state.factor; }

    /**
     * @brief Draws an image onto the surface.
     *
     * This function renders an image onto the target RSurface, clipped to a specified region.
     *
     * - If no mask is provided, the effective rendering area is the intersection between `region` and `image.dst`.
     * - If a mask is provided, the region is further intersected with `mask.dst`.
     *
     * @param image  Information about the image to draw.
     * @param region Clipping region within the RSurface viewport.
     * @param mask   Optional mask image. If provided, applies alpha masking using `mask.a`.
     *
     * @return true if the image was successfully drawn, false otherwise.
     */
    virtual bool drawImage(const RDrawImageInfo& image, const SkRegion *region = nullptr, const RDrawImageInfo* mask = nullptr) noexcept = 0;

    /**
     * @brief Fills the specified region with the current color().
     *
     * @param region The region within the surface to fill.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool drawColor(const SkRegion& region) noexcept = 0;

    /**
     * @brief Clears the entire surface using the current color.
     *
     * This is equivalent to filling the entire surface with the color set by color().
     *
     * @return true if the surface was successfully cleared; false otherwise.
     */
    bool clearSurface() noexcept;

    /**
     * @brief Returns the device used for rendering by this painter.
     *
     * @return Pointer to the associated RDevice.
     */
    RDevice* device() const noexcept { return m_device; }

    bool endPass() noexcept;
protected:
    virtual void beginPass() noexcept = 0;
    State m_state {};
    std::vector<State> m_history;
    friend class RSurface;
    RPainter(RDevice *device) noexcept : m_device(device) {}
    std::weak_ptr<RSurface> m_surface;
    RDevice *m_device;
};

#endif // RPAINTER_H
