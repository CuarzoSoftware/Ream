#ifndef RCORE_H
#define RCORE_H

#include <CZ/Ream/RObject.h>
#include <memory>

class CZ::RCore : public RObject
{
public:
    struct Options
    {
        RGraphicsAPI graphicsAPI { RGraphicsAPI::Auto };
    };

    static std::shared_ptr<RCore> Make(const Options &options) noexcept;
    static std::shared_ptr<RCore> Get() noexcept;

protected:
    RCore(const Options &options) noexcept;
    virtual bool init() noexcept = 0;
    Options m_options;
};

#endif // RCORE_H
