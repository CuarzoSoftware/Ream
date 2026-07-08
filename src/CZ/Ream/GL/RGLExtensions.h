#ifndef RGLEXTENSIONS_H
#define RGLEXTENSIONS_H

namespace CZ
{
    /**
     * @brief Availability flags for the OpenGL ES extensions used by the GL backend.
     *
     * Each field is `true` if the corresponding GLES extension is exposed by the device's
     * context. Populated per RGLDevice while querying the GL context (see RGLDevice::glExtensions()).
     */
    struct RGLExtensions
    {
        bool EXT_read_format_bgra;          ///< GL_EXT_read_format_bgra: allows reading pixels back in BGRA layout.
        bool EXT_texture_format_BGRA8888;   ///< GL_EXT_texture_format_BGRA8888: BGRA8888 texture upload/format support.
        bool OES_EGL_image;                 ///< GL_OES_EGL_image: bind an EGLImage as a texture/renderbuffer.
        bool OES_EGL_image_base;            ///< GL_OES_EGL_image_base: base functionality for binding EGLImages.
        bool OES_EGL_image_external;        ///< GL_OES_EGL_image_external: sample EGLImages via GL_TEXTURE_EXTERNAL_OES.
        bool OES_EGL_sync;                  ///< GL_OES_EGL_sync: EGL fence sync objects.
        bool OES_surfaceless_context;       ///< GL_OES_surfaceless_context: make a context current without a surface.
    };
};

#endif // RGLEXTENSIONS_H
