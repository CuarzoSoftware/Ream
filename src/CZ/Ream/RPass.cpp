#include <CZ/Ream/RPass.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RSync.h>

using namespace CZ;

bool RPass::end() noexcept
{
    if (!isValid())
        return false;

    m_surface->image()->setWriteSync(RSync::Make(m_painter->device()));
    m_surface.reset();
    return true;
}

RPass::RPass(std::shared_ptr<RSurface> surface, RPainter *painter) noexcept :
    m_painter(painter), m_surface(surface)
{
    if (!m_painter || !m_surface)
    {
        m_painter = {};
        m_surface.reset();
        return;
    }

    m_painter->m_surface = m_surface;

    if (m_surface->image()->readSync())
        m_surface->image()->readSync()->gpuWait(painter->device());

    if (m_surface->image()->writeSync())
        m_surface->image()->writeSync()->gpuWait(painter->device());

    m_painter->beginPass();
}
