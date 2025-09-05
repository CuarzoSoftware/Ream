#ifndef ROFPLATFORMHANDLE_H
#define ROFPLATFORMHANDLE_H

#include <CZ/Ream/RPlatformHandle.h>
#include <memory>

class CZ::ROFPlatformHandle : public RPlatformHandle
{
public:
    static std::shared_ptr<ROFPlatformHandle> Make() noexcept { return std::shared_ptr<ROFPlatformHandle>(new ROFPlatformHandle()); };
private:
    ROFPlatformHandle() noexcept : RPlatformHandle(RPlatform::Offscreen) {}
};

#endif // ROFPLATFORMHANDLE_H
