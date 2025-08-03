#include <CZ/skia/core/SkCanvas.h>
#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/skia/gpu/ganesh/GrRecordingContext.h>
#include <CZ/Ream/SK/RSKPass.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RSync.h>
#include <CZ/Ream/RImage.h>

using namespace CZ;

bool RSKPass::end() noexcept
{
    if (!isValid())
        return false;

    m_canvas->restore();
    m_canvas->getSurface()->recordingContext()->asDirectContext()->flush();
    m_image->setWriteSync(RSync::Make(m_device));
    m_image.reset();
    m_glCurrent.reset();
    return true;
}

RSKPass::RSKPass(const SkMatrix &matrix, std::shared_ptr<RImage> image, RDevice *device) noexcept :
    m_device(device),
    m_image(image)
{
    if (!image || !device)
    {
        m_image.reset();
        return;
    }

    auto skSurface { image->skSurface(device) };

    if (!skSurface)
    {
        m_image.reset();
        return;
    }

    if (image->readSync())
        image->readSync()->gpuWait(device);

    if (image->writeSync())
        image->writeSync()->gpuWait(device);

    if (device->asGL())
        m_glCurrent.reset(new RGLMakeCurrent(RGLMakeCurrent::FromDevice(device->asGL(), true)));

    skSurface->recordingContext()->asDirectContext()->resetContext();
    m_canvas = skSurface->getCanvas();
    m_canvas->save();
    m_canvas->setMatrix(matrix);
}
