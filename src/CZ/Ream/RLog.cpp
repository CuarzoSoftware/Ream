#include <CZ/Ream/RLog.h>

using namespace CZ;

const CZ::CZLogger &RLogger() noexcept
{
    static CZLogger logger { "Ream", "CZ_REAM_LOG_LEVEL" };
    return logger;
}
