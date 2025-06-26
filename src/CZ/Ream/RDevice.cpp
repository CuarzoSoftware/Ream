#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RCore.h>

using namespace CZ;

RGLDevice *RDevice::asGL() noexcept
{
    if (core().graphicsAPI() == RGraphicsAPI::GL)
        return (RGLDevice*)this;
    return nullptr;
}
