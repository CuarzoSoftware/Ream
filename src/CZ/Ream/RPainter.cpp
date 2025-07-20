#include "skia/gpu/ganesh/GrDirectContext.h"
#include "skia/gpu/ganesh/GrRecordingContext.h"
#include <CZ/skia/core/SkRegion.h>
#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RSync.h>

using namespace CZ;

void RPainter::save() noexcept
{
    m_history.emplace_back(m_state);
}

void RPainter::restore() noexcept
{
    if (m_history.empty())
        m_state = {};
    else
    {
        m_state = m_history.back();
        m_history.pop_back();
    }
}

void RPainter::reset() noexcept
{
    m_state = {};
}

bool RPainter::clearSurface() noexcept
{
    auto surface { m_surface.lock() };

    if (!surface)
        return false;

    auto image { surface->image() };

    if (!image)
        return false;

    save();
    setOptions(ColorIsPremult);
    setBlendMode(RBlendMode::Src);
    setFactor();
    setOpacity(1.f);
    const bool ret { drawColor(SkRegion(SkIRect::MakePtSize(surface->pos(), surface->size()))) };
    restore();
    return ret;
}

bool RPainter::endPass() noexcept
{
    auto surface { m_surface.lock() };

    if (!surface)
    {
        device()->log(CZError, CZLN, "endPass() failed, missing RSurface");
        return false;
    }

    auto image { surface->image() };

    if (!image)
    {
        device()->log(CZError, CZLN, "endPass() failed, missing RSurface image");
        return false;
    }

    if (m_canvas)
        m_canvas->getSurface()->recordingContext()->asDirectContext()->flush();

    image->setWriteSync(RSync::Make(device()));
    return true;
}
