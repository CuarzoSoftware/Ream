#include <CZ/Ream/RS/RRSPass.h>
#include <CZ/Ream/RMatrixUtils.h>
#include <CZ/skia/core/SkSurface.h>

using namespace CZ;

RRSPass::~RRSPass() noexcept {}

SkCanvas *RRSPass::getCanvas(bool sync) const noexcept
{
    CZ_UNUSED(sync)
    if (m_skSurface)
        return m_skSurface->getCanvas();
    return nullptr;
}

RPainter *RRSPass::getPainter(bool sync) const noexcept
{
    CZ_UNUSED(sync)
    return m_painter.get();
}

void RRSPass::setGeometry(const RSurfaceGeometry &geometry) noexcept
{
    if (!geometry.isValid())
        return;

    m_geometry = geometry;

    if (m_painter)
        m_painter->setGeometry(geometry);

    if (m_skSurface)
        m_skSurface->getCanvas()->setMatrix(RMatrixUtils::VirtualToImage(geometry.transform, geometry.viewport, geometry.dst));
}

RRSPass::RRSPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter, sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept :
    RPass(surface, image, painter, skSurface, device, caps)
{
    resetGeometry();
}
