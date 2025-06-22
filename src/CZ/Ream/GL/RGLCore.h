#ifndef RGLCORE_H
#define RGLCORE_H

#include <CZ/Ream/RCore.h>
#include <CZ/Ream/GL/EGL/REGLExtensions.h>
#include <CZ/Ream/GL/EGL/REGLProcs.h>

class CZ::RGLCore : public RCore
{
public:
    const REGLInstanceExtensions &instanceEGLExtensions() const noexcept { return m_instanceEGLExtensions; }
    const REGLInstanceProcs &instanceEGLProcs() const noexcept { return m_instanceEGLProcs; }
private:
    friend class RCore;
    bool init() noexcept override;
    bool initInstanceEGLExtensions() noexcept;
    bool initInstanceEGLProcs() noexcept;
    RGLCore(const Options &options) noexcept;

    REGLInstanceExtensions m_instanceEGLExtensions {};
    REGLInstanceProcs m_instanceEGLProcs {};
};

#endif // RGLCORE_H
