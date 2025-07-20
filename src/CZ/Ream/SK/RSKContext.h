#ifndef CZ_RSKCONTEXT_H
#define CZ_RSKCONTEXT_H

#include <CZ/skia/gpu/ganesh/GrContextOptions.h>

namespace CZ
{
    /* Skia GrContext options.
     * Can be modified before creating an RCore */
    GrContextOptions &GetSKContextOptions() noexcept;
};

#endif // CZ_RSKCONTEXT_H
