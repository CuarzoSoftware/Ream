#ifndef RGLPAINTER_H
#define RGLPAINTER_H

#include <CZ/Ream/RPainter.h>
#include <GLES2/gl2.h>

class CZ::RGLPainter : public RPainter
{
public:
    bool drawImage(const SkRegion &region) noexcept override;
    RGLDevice *device() const noexcept { return (RGLDevice*)m_device; }

private:
    friend class RGLDevice;
    static std::shared_ptr<RGLPainter> Make(RGLDevice *device) noexcept;
    bool init() noexcept;
    RGLPainter(RGLDevice *device) noexcept : RPainter((RDevice*)device) {};
    bool setSurface(std::shared_ptr<RSurface> surface) noexcept;
    GLint u_screenMatrix;
    GLint u_texMatrix;
    GLint u_tex;
    GLint a_pos;
    GLuint vert, frag, prog;
};

#endif // RGLPAINTER_H
