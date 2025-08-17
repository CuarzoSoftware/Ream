#ifndef CZ_RRSPAINTER_H
#define CZ_RRSPAINTER_H

#include <CZ/Ream/RPainter.h>

class CZ::RRSPainter final : public RPainter
{
public:
    bool drawImage(const RDrawImageInfo &image, const SkRegion *region = nullptr, const RDrawImageInfo *mask = nullptr) noexcept override;
    bool drawColor(const SkRegion &region) noexcept override;
    RRSDevice *device() const noexcept { return (RRSDevice*)m_device; }
private:
    friend class RRSDevice;

    enum class ValRes
    {
        Ok,
        Noop,
        Error
    };

    static std::shared_ptr<RRSPainter> Make(RRSDevice *device) noexcept;
    RRSPainter(RRSDevice *device) noexcept : RPainter((RDevice*)device) {};
    void beginPass() noexcept override;

    ValRes validateDrawImage(const RDrawImageInfo &image, const SkRegion *region, const RDrawImageInfo *mask, std::shared_ptr<RSurface> surface, SkPath &outClip) noexcept;
};

#endif // CZ_RRSPAINTER_H
