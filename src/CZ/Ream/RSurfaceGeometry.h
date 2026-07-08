#ifndef CZ_RSURFACEGEOMETRY_H
#define CZ_RSURFACEGEOMETRY_H

#include <CZ/Core/CZTransform.h>
#include <CZ/skia/core/SkRect.h>

namespace CZ
{
    /**
     * @brief Describes the coordinate mapping of an RSurface.
     *
     * Defines how the surface's virtual viewport is mapped onto its destination rect
     * within the backing image, along with the transform applied during that mapping.
     */
    struct RSurfaceGeometry
    {
        /**
         * @brief Virtual coordinate space of the surface (source rect).
         */
        SkRect viewport;

        /**
         * @brief Destination rect the viewport is mapped to within the backing image.
         */
        SkRect dst;

        /**
         * @brief Transform applied when mapping the viewport onto the destination.
         */
        CZTransform transform { CZTransform::Normal };

        /**
         * @brief Checks whether the geometry is usable.
         *
         * @return `true` if both `viewport` and `dst` are sorted, non-empty and finite.
         */
        bool isValid() const noexcept
        {
            return viewport.isSorted() && !viewport.isEmpty() && viewport.isFinite() &&
                   dst.isSorted() && !dst.isEmpty() && dst.isFinite();
        }
    };
};

#endif // CZ_RSURFACEGEOMETRY_H
