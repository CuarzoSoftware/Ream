#include <CZ/Ream/GL/RGLDevice.h>
#include "CZ/Ream/GL/RGLMakeCurrent.h"
#include <CZ/Ream/GL/RGLProgram.h>
#include <CZ/Ream/GL/RGLPainter.h>

using namespace CZ;

std::shared_ptr<RGLProgram> RGLProgram::GetOrMake(RGLPainter *painter, CZBitset<RGLShader::Features> features) noexcept
{
    auto it { painter->m_programs.find(features) };

    if (it != painter->m_programs.end())
        return it->second;

    auto program { std::shared_ptr<RGLProgram>(new RGLProgram(painter, features)) };

    if (program->link())
    {
        painter->m_programs.emplace(features, program);
        return program;
    }

    return {};
}

RGLProgram::~RGLProgram() noexcept
{
    if (m_id == 0)
        return;

    auto current { RGLMakeCurrent::FromDevice(m_painter->device(), false) };
    glUseProgram(0);
    glDeleteProgram(m_id);
}

void RGLProgram::bind() noexcept
{
    glUseProgram(m_id);
}

RGLProgram::RGLProgram(RGLPainter *painter, CZBitset<RGLShader::Features> features) noexcept :
    m_features(features),
    m_painter(painter)
{}

bool RGLProgram::link() noexcept
{
    auto current { RGLMakeCurrent::FromDevice(m_painter->device(), false) };

    m_frag = RGLShader::GetOrMake(m_painter, m_features, GL_FRAGMENT_SHADER);

    if (!m_frag)
        return false;

    m_vert = RGLShader::GetOrMake(m_painter, m_features, GL_VERTEX_SHADER);

    if (!m_vert)
        return false;

    m_id = glCreateProgram();
    glAttachShader(m_id, m_vert->id());
    glAttachShader(m_id, m_frag->id());
    glLinkProgram(m_id);

    GLint linked { 0 };
    glGetProgramiv(m_id, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint logLength { 0 };
        glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &logLength);

        std::string log(logLength, '\0'); // or use a char buffer
        glGetProgramInfoLog(m_id, logLength, nullptr, &log[0]);

        m_painter->device()->log(CZError, CZLN, "Failed to link GL program: {}", log);
        return false;
    }

    glUseProgram(m_id);

    m_loc.pos = glGetAttribLocation(m_id, "pos");
    m_loc.posProj = glGetUniformLocation(m_id, "posProj");

    if (features().has(RGLShader::HasFactorR))
        m_loc.factorR = glGetUniformLocation(m_id, "factorR");

    if (features().has(RGLShader::HasFactorG))
        m_loc.factorG = glGetUniformLocation(m_id, "factorG");

    if (features().has(RGLShader::HasFactorB))
        m_loc.factorB = glGetUniformLocation(m_id, "factorB");

    if (features().has(RGLShader::HasFactorA))
        m_loc.factorA = glGetUniformLocation(m_id, "factorA");

    if (features().has(RGLShader::HasImage))
    {
        m_loc.imageProj = glGetUniformLocation(m_id, "imageProj");
        m_loc.image = glGetUniformLocation(m_id, "image");

        if (features().has(RGLShader::HasMask))
        {
            m_loc.maskProj = glGetUniformLocation(m_id, "maskProj");
            m_loc.mask = glGetUniformLocation(m_id, "mask");
        }
    }

    return true;
}
