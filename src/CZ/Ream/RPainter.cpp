#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/skia/gpu/ganesh/GrRecordingContext.h>
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
        reset();
    else
    {
        m_state = m_history.back();
        m_history.pop_back();
    }
}

void RPainter::clearHistory() noexcept
{
    m_history.clear();
    reset();
}

void RPainter::reset() noexcept
{
    m_state = {};
    m_state.geometry = m_surface->geometry();
}

void RPainter::clear() noexcept
{
    save();
    setOptions(ColorIsPremult);
    setBlendMode(RBlendMode::Src);
    setFactor();
    setOpacity(1.f);
    drawColor(SkRegion(geometry().viewport.roundOut()));
    restore();
}
