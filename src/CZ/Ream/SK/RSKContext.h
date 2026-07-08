#ifndef CZ_RSKCONTEXT_H
#define CZ_RSKCONTEXT_H

#include <CZ/skia/gpu/ganesh/GrContextOptions.h>

namespace CZ
{
    /**
     * @brief Returns the global Skia GrContext options used when creating Skia (Ganesh) contexts.
     *
     * The returned reference can be modified before creating an RCore to customize the options.
     */
    GrContextOptions &GetSKContextOptions() noexcept;
};

#endif // CZ_RSKCONTEXT_H
