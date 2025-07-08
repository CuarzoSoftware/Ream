#include <CZ/Ream/RLog.h>
#include <CZ/Ream/GL/RGLPainter.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/skia/core/SkMatrix.h>

using namespace CZ;

const char *v =
R"(
uniform mat3 proj;
uniform mat3 tex_proj;
attribute vec2 pos;
varying vec2 v_texcoord;

void main() {
    vec3 pos3 = vec3(pos, 1.0);
    gl_Position = vec4(pos3 * proj, 1.0);
    v_texcoord = (pos3 * tex_proj).xy;
}
)";

const char *f =
R"(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

varying vec2 v_texcoord;
uniform sampler2D tex;

void main() {
    gl_FragColor = texture2D(tex, v_texcoord);
}
)";

static GLuint compileShader(GLenum type, const char *shaderString)
{
    GLuint shader;
    GLint compiled;
    shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderString, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        GLchar errorLog[infoLen];
        glGetShaderInfoLog(shader, infoLen, &infoLen, errorLog);
        RError("%s", errorLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool RGLPainter::drawImage(const SkRegion &region) noexcept
{
    if (region.isEmpty() || imageDstRect().isEmpty())
        return true;

    auto image { m_image.lock() };
    auto surface { m_surface.lock() };

    if (!image || !surface || !surface->image())
        return false;

    auto fb { m_surface.lock()->image()->asGL()->glFb(device()) };

    if (!fb.has_value())
    {
        RError(CZLN, "Invalid framebuffer");
        return false;
    }

    auto tex { image->asGL()->texture(device()) };

    if (tex.id == 0)
    {
        return false;
    }

    auto current { RGLMakeCurrent::FromDevice(device(), true) };
    glBindFramebuffer(GL_FRAMEBUFFER, fb.value());
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex.target, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glUniform1i(u_tex, 0);

{
    // Screen
    SkScalar matVals[9];
    SkMatrix mat = SkMatrix::I();
    mat.preScale(
        2.f / SkScalar(surface->size().width()),
        2.f / SkScalar(surface->size().height()));
    mat.postTranslate(-1.0f, -1.f);
    if (fb.value() == 0)
        mat.postScale(1.f, -1.f);
    mat.get9(matVals);
    glUniformMatrix3fv(u_screenMatrix, 1, GL_FALSE, matVals);

    // Texture
    mat.setIdentity();
    mat.postTranslate(
        -imageDstRect().x(),
        -imageDstRect().y());

    Float32 sx = imageSrcRect().width() / SkScalar(imageDstRect().width());
    Float32 sy = imageSrcRect().height() / SkScalar(imageDstRect().height());

    mat.postScale(sx, sy);
    mat.postTranslate(
        imageSrcRect().x(),
        imageSrcRect().y());

    mat.postScale(
        imageScale() / SkScalar(image->size().width()),
        imageScale() / SkScalar(image->size().height()));

    //mat.postTranslate(-1.0f, -1.f);
    mat.get9(matVals);
    glUniformMatrix3fv(u_texMatrix, 1, GL_FALSE, matVals);
}

    std::vector<GLfloat> vbo;
    vbo.resize(region.computeRegionComplexity() * 12);

    SkRegion::Iterator it { region };
    size_t i { 0 };
    while (!it.done())
    {
        // Triangle 1

        // Bottom left
        vbo[i++] = it.rect().fLeft;
        vbo[i++] = it.rect().fBottom;

        // Bottom right
        vbo[i++] = it.rect().fRight;
        vbo[i++] = it.rect().fBottom;

        // Top left
        vbo[i++] = it.rect().fLeft;
        vbo[i++] = it.rect().fTop;

        // Triangle 2

        // Top left
        vbo[i++] = it.rect().fLeft;
        vbo[i++] = it.rect().fTop;

        // Bottom right
        vbo[i++] = it.rect().fRight;
        vbo[i++] = it.rect().fBottom;

        // Top Right
        vbo[i++] = it.rect().fRight;
        vbo[i++] = it.rect().fTop;

        it.next();
    }

    glEnableVertexAttribArray(a_pos);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vbo.data());
    const auto size { surface->image()->size() };
    glViewport(0, 0, size.width(), size.height());
    glDisable(GL_SCISSOR_TEST);
    glDrawArrays(GL_TRIANGLES, 0, region.computeRegionComplexity() * 6);
    glDisableVertexAttribArray(a_pos);
    return true;
}

std::shared_ptr<RGLPainter> RGLPainter::Make(RGLDevice *device) noexcept
{
    auto painter { std::shared_ptr<RGLPainter>(new RGLPainter(device)) };

    if (painter->init())
        return painter;

    return {};
}

bool RGLPainter::init() noexcept
{
    auto current { RGLMakeCurrent::FromDevice(device(), false) };
    vert = compileShader(GL_VERTEX_SHADER, v);
    frag = compileShader(GL_FRAGMENT_SHADER, f);
    prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLen);
        glDeleteProgram(prog);
        return false;
    }

    glUseProgram(prog);
    u_screenMatrix = glGetUniformLocation(prog, "proj");
    u_texMatrix    = glGetUniformLocation(prog, "tex_proj");
    u_tex    = glGetUniformLocation(prog, "tex");
    a_pos = glGetAttribLocation(prog, "pos");
    return true;
}

bool RGLPainter::setSurface(std::shared_ptr<RSurface> surface) noexcept
{
    m_surface = surface;
    return true;
}
