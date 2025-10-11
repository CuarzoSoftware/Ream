#include <CZ/Ream/SK/RSKFormat.h>
#include <CZ/Ream/RS/RRSDevice.h>
#include <CZ/Ream/RS/RRSCore.h>
#include <CZ/Ream/RS/RRSPainter.h>
#include <CZ/Core/Utils/CZStringUtils.h>
#include <CZ/Core/Utils/CZSetUtils.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <xf86drm.h>
#include <gbm.h>

using namespace CZ;

RRSDevice *RRSDevice::Make(RRSCore &core, int drmFd, void *userData) noexcept
{
    auto dev { new RRSDevice(core, drmFd, userData) };

    if (dev->init())
        return dev;

    delete dev;
    return nullptr;
}

RRSDevice::RRSDevice(RRSCore &core, int drmFd, void *userData) noexcept : RDevice(core)
{
    m_drmFd = drmFd;
    m_drmUserData = userData;
}

RRSDevice::~RRSDevice() noexcept
{
    if (gbmDevice())
    {
        gbm_device_destroy(m_gbmDevice);
        m_gbmDevice = nullptr;
    }

    if (core().platform() == RPlatform::Wayland)
    {
        if (drmFd() >= 0)
        {
            close(drmFd());
            m_drmFd = -1;
        }

        if (drmFdReadOnly() >= 0)
        {
            close(drmFdReadOnly());
            m_drmFdReadOnly = -1;
        }
    }
}

bool RRSDevice::init() noexcept
{
    if (core().platform() == RPlatform::Wayland)
        return initWL();
    else if (core().platform() == RPlatform::DRM)
        return initDRM();
    else
        return initOF();

    return false;
}

bool RRSDevice::initWL() noexcept
{
    m_drmNode = "Unknown Device";
    log = RLog.newWithContext(m_drmNode);
    return initFormats();
}

bool RRSDevice::initDRM() noexcept
{
    drmDevicePtr device;

    struct stat stat;
    if (fstat(m_drmFd, &stat) == 0)
        m_id = stat.st_rdev;

    if (drmGetDevice(m_drmFd, &device) == 0)
    {
        if (device->available_nodes & (1 << DRM_NODE_PRIMARY) && device->nodes[DRM_NODE_PRIMARY])
            m_drmNode = device->nodes[DRM_NODE_PRIMARY];
        else if (device->available_nodes & (1 << DRM_NODE_RENDER) && device->nodes[DRM_NODE_RENDER])
            m_drmNode = device->nodes[DRM_NODE_RENDER];

        drmFreeDevice(&device);
    }

    if (m_drmNode.empty())
        m_drmNode = "Unknown Device";
    else
    {
        m_drmFdReadOnly = open(m_drmNode.c_str(), O_RDONLY | O_CLOEXEC);
        log = RLog.newWithContext(CZStringUtils::SubStrAfterLastOf(m_drmNode, "/"));
    }

    setDRMDriverName(m_drmFd);

    m_gbmDevice = gbm_create_device(m_drmFd);

    if (!m_gbmDevice)
        log(CZError, CZLN, "Failed to create gbm_device");

    UInt64 value {};
    drmGetCap(m_drmFd, DRM_CAP_ADDFB2_MODIFIERS, &value);
    m_caps.AddFb2Modifiers = value == 1;
    value = {};
    drmGetCap(m_drmFd, DRM_CAP_DUMB_BUFFER, &value);
    m_caps.DumbBuffer = value == 1;

    if (!m_caps.DumbBuffer)
    {
        log(CZError, CZLN, "Missing DumbBuffer cap");
        return false;
    }

    return initFormats();
}

bool RRSDevice::initOF() noexcept
{
    m_drmNode = "Raster Device";
    log = RLog.newWithContext(m_drmNode);
    return initFormats();
}

bool RRSDevice::initFormats() noexcept
{
    auto set { CZSetUtils::Intersect(RSKFormat::SupportedFormats(), RDRMFormat::SupportedFormats()) };

    for (auto fmt : set)
    {
        m_textureFormats.add(fmt, DRM_FORMAT_MOD_LINEAR);
        m_textureFormats.add(fmt, DRM_FORMAT_MOD_INVALID);
    }

    m_renderFormats = m_textureFormats;
    return true;
}

std::shared_ptr<RPainter> RRSDevice::makePainter(std::shared_ptr<RSurface> surface) noexcept
{
    assert(surface);
    return std::shared_ptr<RPainter>(new RRSPainter(surface, this));
}
