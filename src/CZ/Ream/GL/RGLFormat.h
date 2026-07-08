#ifndef CZ_RGLFORMAT_H
#define CZ_RGLFORMAT_H

#include <CZ/Ream/Ream.h>
#include <GLES2/gl2.h>

namespace CZ
{
    /**
     * @brief OpenGL ES pixel format descriptor mapped from a DRM format.
     *
     * Bundles the three arguments GLES needs to describe a pixel layout in calls such as
     * `glTexImage2D()`/`glReadPixels()`: the client-side format, the sized internal format,
     * and the component type.
     */
    struct RGLFormat
    {
        GLint format;           ///< Client pixel format (e.g. `GL_RGBA`, `GL_BGRA_EXT`, `GL_ALPHA`).
        GLint internalFormat;   ///< Sized internal format (e.g. `GL_RGBA8_OES`, `GL_BGRA8_EXT`).
        GLint type;             ///< Component data type (e.g. `GL_UNSIGNED_BYTE`, `GL_HALF_FLOAT_OES`).

        /**
         * @brief Returns the GL format descriptor for a given DRM fourcc format.
         *
         * @param format A DRM fourcc format code.
         * @return A pointer to a static RGLFormat describing the GL layout, or `nullptr`
         *         if the DRM format has no known GL mapping. Some mappings are only available
         *         on little-endian hosts.
         */
        static const RGLFormat *FromDRM(RFormat format) noexcept;
    };
}

#endif // CZ_RGLFORMAT_H
