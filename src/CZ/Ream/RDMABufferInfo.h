#ifndef CZ_RDMABUFFERINFO_H
#define CZ_RDMABUFFERINFO_H

#include <CZ/Ream/Ream.h>
#include <CZ/Ream/RLog.h>

#include <cstring>
#include <fcntl.h>
#include <xf86drm.h>
#include <drm_fourcc.h>

struct CZ::RDMABufferInfo
{
    Int32 width, height;
    RFormat format;
    RModifier modifier;
    int planeCount;
    UInt32 offset[4] {};
    UInt32 stride[4] {};
    int fd[4] { -1, -1, -1, -1 };

    bool isValid() const noexcept
    {
        if (width <= 0 || height <= 0)
        {
            RLog(CZError, CZLN, "Invalid dimensions {}x{}", width, height);
            return false;
        }

        if (format == DRM_FORMAT_INVALID)
        {
            RLog(CZError, CZLN, "Invalid format {}", drmGetFormatName(format));
            return false;
        }

        if (planeCount <= 0 || planeCount > 4)
        {
            RLog(CZError, CZLN, "Invalid number of planes: {}. The accepted range is [1, 4]", planeCount);
            return false;
        }

        for (int i = 0; i < planeCount; i++)
        {
            if (fd[i] < 0)
            {
                RLog(CZError, CZLN, "Invalid fd {} for plane {}", fd[i], i);
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Creates a copy of this DMA info, duplicating all file descriptors.
     *
     * @return A new `RDMABufferInfo` with duplicated file descriptors, or `std::nullopt` on failure or if the buffer is invalid.
     */
    std::optional<RDMABufferInfo> dup() const noexcept
    {
        if (!isValid())
            return {};

        RDMABufferInfo dmaCopy { *this };
        std::memset(dmaCopy.fd, 0, sizeof(*dmaCopy.fd) * 4);

        for (int i = 0; i < dmaCopy.planeCount; i++)
        {
            dmaCopy.fd[i] = fcntl(fd[i], F_DUPFD_CLOEXEC, 0);

            if (dmaCopy.fd[i] < 0)
            {
                RLog(CZError, CZLN, "Failed to duplicate DRM fd");

                for (int j = 0; j < i; j++)
                    close(dmaCopy.fd[i]);

                return {};
            }
        }

        return dmaCopy;
    }

    void closeFds() noexcept
    {
        for (auto i = 0; i < planeCount; i++)
            close(fd[i]);
    }
};

#endif // CZ_RDMABUFFERINFO_H


