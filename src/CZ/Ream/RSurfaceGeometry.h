#ifndef CZ_RSURFACEGEOMETRY_H
#define CZ_RSURFACEGEOMETRY_H

#include <CZ/Core/CZTransform.h>
#include <CZ/skia/core/SkRect.h>

namespace CZ
{
    struct RSurfaceGeometry
    {
        SkRect viewport;
        SkRect dst;
        CZTransform transform { CZTransform::Normal };

        bool isValid() const noexcept
        {
            return viewport.isSorted() && !viewport.isEmpty() && viewport.isFinite() &&
                   dst.isSorted() && !dst.isEmpty() && dst.isFinite();
        }
    };
};

#endif // CZ_RSURFACEGEOMETRY_H
